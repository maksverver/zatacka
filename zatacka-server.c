#define _POSIX_SOURCE
#include "Debug.h"
#include "Protocol.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h> 
#include <sys/resource.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_CLIENTS           (64)
#define SERVER_FPS            (40)
#define MOVE_BACKLOG          (20)
#define MAX_PACKET_LEN      (4094)
#define MAX_NAME_LEN          (20)
#define PLAYERS_PER_CLIENT     (4)
#define TURN_RATE             (72)
#define MOVE_RATE              (5)

#define MAX_PLAYERS         (PLAYERS_PER_CLIENT*MAX_CLIENTS)

struct RGB
{
    unsigned char r, g, b;
};


typedef struct Player
{
    /* Set to false if the player is in use. If true, name must be valid. */
    bool            in_use;

    /* Move buffer */
    int             timestamp;
    char            moves[MOVE_BACKLOG];

    /* Player info */
    int             index;
    int             dead_since;
    char            name[MAX_NAME_LEN + 1];
    struct RGB      color;
    double          x, y, a;    /* 0 <= x,y < 1; 0 < a <= 2pi */

} Player;


typedef struct Client
{
    /* Set to true only if this client is currently in use */
    bool            in_use;

    /* Remote address (TCP only) */
    struct sockaddr_in sa_remote;

    /* Streaming data */
    int             fd_stream;
    unsigned char   buf[MAX_PACKET_LEN + 2];
    int             buf_pos;

    /* Players controlled by the client */
    Player          players[PLAYERS_PER_CLIENT];
} Client;

/*
    Global variables
*/
static int g_fd_listen;         /* Stream data listening socket */
static int g_fd_packet;         /* Packet data socket */
static int g_timestamp;         /* Time counter */
static int g_num_clients;       /* Number of connected clients */
static int g_num_players;       /* Number of people in the current game */
static int g_num_alive;         /* Number of people still alive */
static unsigned g_gameid;
static Client g_clients[MAX_CLIENTS];
static Player *g_players[MAX_PLAYERS];
unsigned char g_field[1000][1000];

/*
    Function prototypes
*/

/* Convert a hue (0 <= hue < 1) to RGB */
static struct RGB rgb_from_hue(double hue);

/* Plot a dot at the given position in the given color.
   Returns the maximum value of the colors of the overlapping pixels,
   or 256 if the dot falls(partially) outside the field. */
static int plot(double x, double y, int col);

/* Disconnect a client (sending the given reason, if possible) */
static void client_disconnect(Client *cl, const char *reason);

/* Send a message to a client */
static void client_send(Client *cl, const void *buf, size_t len, bool reliable);

/* Kill a player */
static void player_kill(Player *p);

/* Network handling */
static void handle_packet( Client *cl,
                           unsigned char *buf, size_t len, bool reliable );
static void handle_HELO(Client *cl, unsigned char *buf, size_t len);
static void handle_MOVE(Client *cl, unsigned char *buf, size_t len);

/* Dump an image of the field in BMP format to file "bmp/field-GAMEID.bmp" */
static void debug_dump_image();

/* Restart the game (called when all players have died) */
static void restart_game();

/* Process a server frame. */
static void do_frame();

/* Server main loop */
static int run();

/* Application entry point */
int main(int argc, char *argv[]);


/* Function definitions */

static struct RGB rgb_from_hue(double hue)
{
    struct RGB res;

    if (hue < 0 || hue >= 1)
    {
        res.r = res.g = res.b = 128;
    }
    else
    if (hue < 1/3.0)
    {
        res.r = (int)(255*3.0*(1/3.0 - hue));
        res.g = (int)(255*3.0*(hue));
        res.b = 0;
    }
    else
    if (hue < 2/3.0)
    {
        res.r = 0;
        res.g = (int)(255*3.0*(2/3.0 - hue));
        res.b = (int)(255*3.0*(hue - 1/3.0));
    }
    else
    {
        res.r = (int)(255*3.0*(hue - 2/3.0));
        res.g = 0;
        res.b = (int)(255*3.0*(1.0 - hue));
    }

    return res;
}

static int plot(double x, double y, int col)
{
    static const bool templ[7][7] = {
        { 0,0,1,1,1,0,0 },
        { 0,1,1,1,1,1,0 },
        { 1,1,1,1,1,1,1 },
        { 1,1,1,1,1,1,1 },
        { 1,1,1,1,1,1,1 },
        { 0,1,1,1,1,1,0 },
        { 0,0,1,1,1,0,0 } };

    int cx = (int)round(1000*x);
    int cy = (int)round(1000*y);

    int res = 0;
    for (int dx = -3; dx <= 3; ++dx)
    {
        for (int dy = -3; dy <= 3; ++dy)
        {
            if (!templ[dx + 3][dy + 3]) continue;
            int x = cx + dx, y = cy + dy;
            if (x >= 0 && x < 1000 && y >= 0 && y < 1000)
            {
                if (g_field[y][x] > res) res = g_field[y][x];
                g_field[y][x] = col;
            }
            else
            {
                res = 256;
            }
        }
    }
    return res;
}

static void client_send(Client *cl, const void *buf, size_t len, bool reliable)
{
    assert(len < (size_t)MAX_PACKET_LEN);
    if (reliable)
    {
        unsigned char packet[MAX_PACKET_LEN + 2];
        packet[0] = len>>8;
        packet[1] = len&255;
        memcpy(packet + 2, buf, len);
        if ( send(cl->fd_stream, packet, len + 2, MSG_DONTWAIT)
             != (ssize_t)len + 2 )
        {
            error("reliable send() failed");
            client_disconnect(cl, NULL);
        }
    }
    else
    {
        if (sendto( g_fd_packet, buf, len, MSG_DONTWAIT,
                    (struct sockaddr*)&cl->sa_remote,
                    sizeof(cl->sa_remote) ) != (ssize_t)len)
        {
            warn("unreliable send() failed");
        }
    }
}

static void client_disconnect(Client *cl, const char *reason)
{
    if (!cl->in_use) return;

    info( "disconnecting client %d at %s:%d (reason: %s)",
          cl - g_clients, inet_ntoa(cl->sa_remote.sin_addr),
          ntohs(cl->sa_remote.sin_port), reason );

    /* Remove client -- it's important to do this first, because functions
       call below may call disconnect again (i.e. if client_send() fails) */
    cl->in_use = false;
    --g_num_clients;

    /* Kill and deactivate all associated players */
    for (int n = 0; n < PLAYERS_PER_CLIENT; ++n)
    {
        if (!cl->players[n].in_use) continue;
        player_kill(&cl->players[n]);
        cl->players[n].in_use = false;
    }

    /* Send quit packet containing reason to client */
    if (reason != NULL)
    {
        unsigned char packet[MAX_PACKET_LEN];
        size_t len = strlen(reason);
        if (len > MAX_PACKET_LEN - 1) len = MAX_PACKET_LEN - 1;
        packet[0] = MRSC_DISC;
        memcpy(packet + 1, reason, len);
        client_send(cl, packet, len + 1, true);
    }
    close(cl->fd_stream);
}

static void player_kill(Player *pl)
{
    assert(pl->in_use);

    if (pl->dead_since != -1) return;

    info("player %d died.", pl->index);

    pl->dead_since = g_timestamp;
    --g_num_alive;
}

static void handle_packet( Client *cl, unsigned char *buf, size_t len,
                           bool reliable )
{
    (void)reliable; /* unused */

/*
    info( "%s packet type %d of length %d received from client #%d",
          reliable ? "reliable" : "unreliable", (int)buf[0], len, c );
    hex_dump(buf, len);
*/

    switch ((int)buf[0])
    {
    case MRCS_HELO: return handle_HELO(cl, buf, len);
    case MRCS_QUIT: return client_disconnect(cl, "client quit");
    case MUCS_MOVE: return handle_MOVE(cl, buf, len);
    default: client_disconnect(cl, "invalid packet type");
    }
}

static void handle_HELO(Client *cl, unsigned char *buf, size_t len)
{
    /* Check if players have already been registered */
    if (cl->players[0].in_use) return;

    for (int p = 0; p < 1; ++p)
    {
        Player *pl = &cl->players[p];

        /* Verify packet data */
        int L = len > 1 ? buf[1] : 0;
        if (L < 1 || L > MAX_NAME_LEN || L + 2 > (int)len)
        {
            client_disconnect(cl, "(HELO) invalid name length");
            return;
        }
        for (int n = 0; n < L; ++n)
        {
            if (buf[2 + n] < 32 || buf[2 + n] > 126)
            {
                client_disconnect(cl, "(HELO) invalid name character");
                return;
            }
        }

        /* Assign player name */
        memcpy(pl->name, buf + 2, L);
        pl->name[L] = '\0';
        pl->in_use = true;

        /* Check availability of name */
        for (int n = 0; n < MAX_CLIENTS; ++n)
        {
            if (!g_clients[n].in_use) continue;
            for (int m = 0; m < PLAYERS_PER_CLIENT; ++m)
            {
                if (!g_clients[n].players[m].in_use) continue;

                if (&g_clients[n] == cl && m == p) continue;  /* skip self */

                if (strcmp(cl->players[p].name, g_clients[n].players[m].name) == 0)
                {
                    client_disconnect(cl, "(HELO) name unavailable");
                    return;
                }
            }
        }
    }
}

static void handle_MOVE(Client *cl, unsigned char *buf, size_t len)
{
    if (len != 9 + MOVE_BACKLOG)
    {
        error( "(MOVE) invalid length packet received from client %d "
               "(received %d; expected %d)", cl - g_clients,
               len, 9 + MOVE_BACKLOG );
        return;
    }
    unsigned gameid = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
    if (gameid != g_gameid)
    {
        warn( "(MOVE) packet with invalid game id from client %d "
              "(received %d; expected %d)", cl - g_clients, gameid, g_gameid);
        return;
    }

    int timestamp = (buf[5] << 24) | (buf[6] << 16) | (buf[7] << 8) | buf[8];
    if (timestamp > g_timestamp + 1)
    {
        error("(MOVE) timestamp too great");
        return;
    }

    Player *pl = &cl->players[0];

    if (timestamp <= pl->timestamp)
    {
        warn("(MOVE) discarded out-of-order move packet");
        return;
    }

    int added = timestamp - pl->timestamp;
    if (added > MOVE_BACKLOG)
    {
        warn("(MOVE) discarded packet from long dead player");
        return;
    }

    memmove(pl->moves, pl->moves + added, MOVE_BACKLOG - added);
    memcpy( pl->moves + MOVE_BACKLOG - added,
            buf + 9 + MOVE_BACKLOG - added, added );

    for (int n = MOVE_BACKLOG - added; n < MOVE_BACKLOG; ++n)
    {
        if (pl->dead_since != -1)
        {
            pl->moves[n] = 4;
            continue;
        }

        int m = pl->moves[n];
        if (m < 1 || m > 3)
        {
            error("received invalid move %d\n", m);
            player_kill(pl);
        }
        else
        if (pl->dead_since == -1)
        {
            if (m == 2) pl->a += 2.0*M_PI/TURN_RATE;
            if (m == 3) pl->a -= 2.0*M_PI/TURN_RATE;
            double nx = pl->x + 1e-3*MOVE_RATE*cos(pl->a);
            double ny = pl->y + 1e-3*MOVE_RATE*sin(pl->a);

            /* First, blank out previous dot */
            plot(pl->x, pl->y, 0);

            /* Check for out-of-bounds or overlapping dots */
            if ( nx < 0 || nx >= 1 || ny < 0 || ny >= 1 ||
                 plot(nx, ny, pl->index + 1) != 0 )
            {
                player_kill(pl);
            }
            /* Redraw previous dot */
            plot(pl->x, pl->y, pl->index + 1);

            pl->x = nx;
            pl->y = ny;
        }
    }
    pl->timestamp += added;
    assert(pl->timestamp == timestamp);
}

static void debug_dump_image()
{
    char path[64];
    FILE *fp;

    struct BMP_Header
    {
        char   bfType[2];
        int    bfSize;
        int    bfReserved;
        int    bfOffBits;

        int    biSize;
        int    biWidth;
        int    biHeight;
        short  biPlanes;
        short  biBitCount;
        int    biCompression;
        int    biSizeImage;
        int    biXPelsPerMeter;
        int    biYPelsPerMeter;
        int    biClrUsed;
        int    biClrImportant;
    } __attribute__((__packed__)) bmp_header = {
        .bfType = { 'B', 'M' },
        .bfSize = 54 + 1000*1000 + 256*4,
        .bfOffBits = 54 + 1024,
        .biSize = 40,
        .biWidth = 1000,
        .biHeight = 1000,
        .biPlanes = 1,
        .biBitCount = 8,
    };
    struct BMP_RGB {
        unsigned char b, g, r, a;
    } bmp_palette[256] = {
        {    0,   0,   0,   0 },
        {    0,   0, 255,   0 },
        {    0, 255,   0,   0 },
        {  255,   0,   0,   0 },
        {    0, 255, 255,   0 },
        {  255,   0, 255,   0 },
        {  255, 255,   0,   0 },
        {  255, 255, 255,   0 } };

    assert(sizeof(bmp_header) == 54);
    assert(sizeof(bmp_palette) == 1024);

    sprintf(path, "bmp/field-%08x.bmp", g_gameid);
    fp = fopen(path, "wb");
    if (fp == NULL) fatal("can't open %s for writing.", path);
    if (fwrite(&bmp_header, sizeof(bmp_header), 1, fp) != 1)
    {
        fatal("couldn't write BMP header");
    }
    if (fwrite(&bmp_palette, sizeof(bmp_palette), 1, fp) != 1)
    {
        fatal("couldn't write BMP palette data");
    }
    if (fwrite(g_field, 1000, 1000, fp) != 1000)
    {
        fatal("couldn't write BMP pixel data");
    }
    fclose(fp);
}

static void restart_game()
{
    g_timestamp = 0;
    time_reset();

    if (g_num_clients == 0) return;
    if (g_num_players > 0) debug_dump_image();

    memset(g_field, 0, sizeof(g_field));

    /* Find players for the next game */
    g_num_players = 0;
    for (int n = 0; n < MAX_CLIENTS; ++n)
    {
        if (!g_clients[n].in_use) continue;

        for (int m = 0; m < PLAYERS_PER_CLIENT; ++m)
        {
            Player *pl = &g_clients[n].players[m];
            if (pl->in_use)
            {
                g_players[g_num_players] = pl;
                pl->index = g_num_players++;
            }
        }
    }

    if (g_num_players == 0) return; /* none ready yet */

    info("Starting game with %d players", g_num_players);

    /* Initialize players */
    for (int n = 0; n < g_num_players; ++n)
    {
        Player *pl = g_players[n];

        /* Reset player state to starting position */
        pl->dead_since = -1;
        pl->color = rgb_from_hue((double)pl->index/g_num_players);
        pl->x = 0.1 + 0.8*rand()/RAND_MAX;
        pl->y = 0.1 + 0.8*rand()/RAND_MAX;
        pl->a = 2.0*M_PI*rand()/RAND_MAX;
        pl->timestamp = 0;
        memset(pl->moves, 0, sizeof(pl->moves));
    }

    g_num_alive = g_num_players;

    g_gameid = ((rand()&255) << 24) |
               ((rand()&255) << 16) |
               ((rand()&255) <<  8) |
               ((rand()&255) <<  0);

    /* Build start game packet */
    unsigned char packet[MAX_PACKET_LEN];
    size_t pos = 0;
    packet[pos++] = MRSC_STRT;
    packet[pos++] = SERVER_FPS;
    packet[pos++] = TURN_RATE;
    packet[pos++] = MOVE_RATE;
    packet[pos++] = MOVE_BACKLOG;
    packet[pos++] = g_num_players;
    packet[pos++] = 0;
    packet[pos++] = (g_gameid>>24)&255;
    packet[pos++] = (g_gameid>>16)&255;
    packet[pos++] = (g_gameid>> 8)&255;
    packet[pos++] = (g_gameid>> 0)&255;
    /* FIXME: possible buffer overflow here, depending on server parameters */
    for (int i = 0; i < g_num_players; ++i)
    {
        Player *pl = g_players[i];
        packet[pos++] = pl->color.r;
        packet[pos++] = pl->color.g;
        packet[pos++] = pl->color.b;
        int x = (int)(pl->x*65536);
        int y = (int)(pl->y*65536);
        int a = (int)(pl->a/(2.0*M_PI)*65536);
        packet[pos++] = (x>>8)&255;
        packet[pos++] = (x>>0)&255;
        packet[pos++] = (y>>8)&255;
        packet[pos++] = (y>>0)&255;
        packet[pos++] = (a>>8)&255;
        packet[pos++] = (a>>0)&255;
        size_t name_len = strlen(pl->name);
        packet[pos++] = name_len;
        memcpy(&packet[pos], pl->name, name_len);
        pos += name_len;
    }

    /* Send start packet to all clients */
    for (int n = 0; n < MAX_CLIENTS; ++n)
    {
        Client *cl = &g_clients[n];
        if (!cl->in_use) continue;
        /* FIXME: this doesn't work if player[0] is not in_use yet */
        packet[6] = cl->players[0].index;
        client_send(cl, packet, pos, true);
    }
}

static void do_frame()
{
    if (g_num_clients == 0) return;
    if (g_num_alive == 0) restart_game();
    if (g_num_players == 0) return;

    /* Kill out-of-sync players */
    for (int n = 0; n < g_num_players; ++n)
    {
        Player *pl = g_players[n];
        if (pl->dead_since != -1) continue;

        int backlog = g_timestamp - g_players[n]->timestamp;
        assert(backlog >= 0);
        if (backlog >= MOVE_BACKLOG)
        {
            player_kill(pl);
        }
    }

    /* Send moves packet to all clients  */
    unsigned char packet[4096];
    /* FIXME: possible buffer overflow here, depending on server parameters */
    size_t pos = 0;
    packet[pos++] = MUSC_MOVE;
    packet[pos++] = (g_gameid >> 24)&255;
    packet[pos++] = (g_gameid >> 16)&255;
    packet[pos++] = (g_gameid >>  8)&255;
    packet[pos++] = (g_gameid >>  0)&255;
    packet[pos++] = (g_timestamp >> 24)&255;
    packet[pos++] = (g_timestamp >> 16)&255;
    packet[pos++] = (g_timestamp >>  8)&255;
    packet[pos++] = (g_timestamp >>  0)&255;
    for (int n = 0; n < g_num_players; ++n)
    {
        if (g_players[n]->dead_since != -1 &&
            g_timestamp - g_players[n]->dead_since >= MOVE_BACKLOG)
        {
            packet[pos++] = 0;
        }
        else
        {
            packet[pos++] = 1;
        }
    }
    for (int n = 0; n < g_num_players; ++n)
    {
        if (g_players[n]->dead_since != -1 &&
            g_timestamp - g_players[n]->dead_since >= MOVE_BACKLOG)
        {
            continue;
        }
        int backlog = g_timestamp - g_players[n]->timestamp;
        if (backlog > MOVE_BACKLOG) backlog = MOVE_BACKLOG;
        memcpy(packet + pos, g_players[n]->moves + backlog,
                             MOVE_BACKLOG - backlog);
        memset(packet + pos + MOVE_BACKLOG - backlog, 0, backlog);
        pos += MOVE_BACKLOG;
    }

    for (int n = 0; n < MAX_CLIENTS; ++n)
    {
        Client *cl = &g_clients[n];
        if (!cl->in_use) continue;
        client_send(cl, packet, pos, false);
    }
}

static int run()
{
    for (;;)
    {
        /* Compute time to next tick */
        double delay = (double)g_timestamp/SERVER_FPS - time_now();
        if (delay <= 0)
        {
            do_frame();
            delay += (double)1/SERVER_FPS;
            g_timestamp += 1;
        }

        /* Calculcate select time-out */
        struct timeval timeout;
        if (delay <= 0)
        {
            timeout.tv_sec = timeout.tv_usec = 0;
        }
        else
        {
            timeout.tv_sec  = (int)delay;
            timeout.tv_usec = (int)(1e6*(delay - timeout.tv_sec));
        }

        /* Prepare file selectors to select on */
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(g_fd_listen, &readfds);
        FD_SET(g_fd_packet, &readfds);
        int max_fd = g_fd_listen > g_fd_packet ? g_fd_listen : g_fd_packet;
        for (int n = 0; n < MAX_CLIENTS; ++n)
        {
            if (!g_clients[n].in_use) continue;
            FD_SET(g_clients[n].fd_stream, &readfds);
            if (g_clients[n].fd_stream > max_fd) max_fd = g_clients[n].fd_stream;
        }

        /* Select readable file descriptors (or wait until next tick) */
        int ready = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (ready < 0)
        {
            fatal("select() failed");
        }

        if (ready == 0) continue;

        /* Accept new connections */
        if (FD_ISSET(g_fd_listen, &readfds))
        {
            struct sockaddr_in sa;
            socklen_t sa_len = sizeof(sa);
            int fd = accept(g_fd_listen, (struct sockaddr*)&sa, &sa_len);
            if (fd < 0)
            {
                error("accept() failed");
            }
            else
            if (sa_len != sizeof(sa) || sa.sin_family != AF_INET)
            {
                error("accepted connection from unsupported remote address");
                close(fd);
            }
            else
            {
                int n = 0;
                while (n < MAX_CLIENTS && g_clients[n].in_use) ++n;
                if (n == MAX_CLIENTS)
                {
                    warn( "No client slot free; rejecting connection from %s:%d",
                          inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
                    close(fd);
                }
                else
                {
                    info( "Accepted client from %s:%d in slot #%d",
                          inet_ntoa(sa.sin_addr), ntohs(sa.sin_port), n );

                    /* Initialize new client slot */
                    memset(&g_clients[n], 0, sizeof(g_clients[n]));
                    g_clients[n].sa_remote = sa;
                    g_clients[n].fd_stream = fd;
                    g_clients[n].in_use    = true;
                    g_num_clients += 1;
                }
            }
        }

        /* Accept incoming packets */
        if (FD_ISSET(g_fd_packet, &readfds))
        {
            struct sockaddr_in sa;
            socklen_t sa_len = sizeof(sa);
            unsigned char buf[MAX_PACKET_LEN];
            ssize_t buf_len;

            buf_len = recvfrom( g_fd_packet, buf, sizeof(buf), 0,
                                (struct sockaddr*)&sa, &sa_len );
            if (buf_len < 0)
            {
                error("recvfrom() failed");
            }
            else
            if (sa_len != sizeof(sa) || sa.sin_family != AF_INET)
            {
                error("received packet from unsupported remote address");
            }
            else
            if (buf_len < 1)
            {
                error( "received invalid packet from %s:%d",
                       inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
            }
            else
            {
                Client *cl = NULL;
                for (int n = 0; n < MAX_CLIENTS; ++n)
                {
                    if ( g_clients[n].in_use &&
                         g_clients[n].sa_remote.sin_addr.s_addr ==
                            sa.sin_addr.s_addr &&
                         g_clients[n].sa_remote.sin_port == sa.sin_port )
                    {
                        cl = &g_clients[n];
                        break;
                    }
                }
                if (cl == NULL)
                {
                    warn ( "packet from %s:%d ignored (not registered)",
                        inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
                }
                else
                {
                    handle_packet(cl, buf, buf_len, false);
                }
            }
        }

        /* Accept incoming stream packets */
        for (int n = 0; n < MAX_CLIENTS; ++n)
        {
            Client *cl = &g_clients[n];

            if (!cl->in_use) continue;

            if (!FD_ISSET(cl->fd_stream, &readfds)) continue;

            ssize_t read = recv( cl->fd_stream, cl->buf + cl->buf_pos,
                                 sizeof(cl->buf) - cl->buf_pos, 0 );
            if (read <= 0)
            {
                client_disconnect(cl, read == 0 ? "EOF reached" : "recv() failed");
            }
            else
            {
                cl->buf_pos += read;
                if (cl->buf_pos >= 2)
                {
                    int len = 256*cl->buf[0] + cl->buf[1];
                    if (len > MAX_PACKET_LEN)
                    {
                        client_disconnect(cl, "packet too large");
                    }
                    else
                    if (len < 1)
                    {
                        client_disconnect(cl, "packet too small");
                    }
                    else
                    if (cl->buf_pos >= len + 2)
                    {
                        handle_packet(cl, cl->buf + 2, len, true);
                        memmove( cl->buf, cl->buf + len + 2,
                                 cl->buf_pos - (len + 2) );
                        cl->buf_pos -= len + 2;
                    }
                }
            }
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    time_reset();

    (void)argc;
    (void)argv;

    /* Mask SIGPIPE, so failed writes do not kill the server */
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);

    /* for debugging: enable core dumps */
    struct rlimit rlim = { RLIM_INFINITY, RLIM_INFINITY };
    if (setrlimit(RLIMIT_CORE, &rlim) != 0)
    {
        error("could not change core dump size!");
    }

    struct sockaddr_in sa_local;
    sa_local.sin_family = AF_INET;
    sa_local.sin_port   = htons(12321);
    sa_local.sin_addr.s_addr = INADDR_ANY;

    /* Create TCP listening socket */
    g_fd_listen = socket(PF_INET, SOCK_STREAM, 0);
    if (g_fd_listen < 0)
    {
        fatal("could not create TCP socket: socket() failed");
    }
    if (bind(g_fd_listen, (struct sockaddr*)&sa_local, sizeof(sa_local)) != 0)
    {
        fatal("could not create TCP socket: bind() failed");
    }
    if (listen(g_fd_listen, 4) != 0)
    {
        fatal("could not create TCP socket: listen() failed");
    }

    /* Create UDP datagram socket */
    g_fd_packet = socket(PF_INET, SOCK_DGRAM, 0);
    if (g_fd_packet < 0)
    {
        fatal("could not create UDP socket: socket() failed");
    }
    if (bind(g_fd_packet, (struct sockaddr*)&sa_local, sizeof(sa_local)) != 0)
    {
        fatal("could not create UDP socket: bind() failed");
    }

    /* Main loop */
    return run();
}
