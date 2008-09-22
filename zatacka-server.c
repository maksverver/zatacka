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
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define max_clients       (64)
#define server_fps        (40)
#define move_backlog      (20)
#define max_packet_len  (4096)
#define max_name_len      (20)

const int data_rate      = server_fps;
const int turn_rate      =   72;
const int move_rate      =    5;

unsigned char g_field[1000][1000];

struct RGB
{
    unsigned char r, g, b;
};

typedef struct Client
{
    /* Set to true only if this client is currently in use */
    bool            in_use;

    /* Remote address (TCP only) */
    struct sockaddr_in sa_remote;

    /* Streaming data */
    int             fd_stream;
    unsigned char   buf[max_packet_len + 2];
    int             buf_pos;

    /* Move buffer */
    int             timestamp;
    char            moves[move_backlog];

    /* Player info */
    bool            player;
    int             dead_since;
    char            name[max_name_len + 1];
    struct RGB      color;
    double          x, y, a;    /* 0 <= x,y < 1; 0 < a <= 2pi */
} Client;

/* Globals */
static int g_fd_listen;         /* Stream data listening socket */
static int g_fd_packet;         /* Packet data socket */
static int g_timestamp;         /* Time counter */
static int g_num_clients;       /* Number of connected clients */
static int g_num_players;       /* Number of people ready to play */
static int g_num_alive;         /* Number of people still alive */
static unsigned g_gameid;
static Client g_clients[max_clients];
static Client *g_players[max_clients];

/* Hue assumed to be in range [0,1) */
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
        }
    }
    return res;
}


static void disconnect(int c, const char *reason);

static void client_send(int c, const void *buf, size_t len, bool reliable)
{
    assert(len < (size_t)max_packet_len);
    if (reliable)
    {
        unsigned char packet[max_packet_len + 2];
        packet[0] = len>>8;
        packet[1] = len&255;
        memcpy(packet + 2, buf, len);
        if ( send(g_clients[c].fd_stream, packet, len + 2, MSG_DONTWAIT)
             != (ssize_t)len + 2 )
        {
            error("reliable send() failed");
            disconnect(c, NULL);
        }
    }
    else
    {
        if (sendto( g_fd_packet, buf, len, MSG_DONTWAIT,
                    (struct sockaddr*)&g_clients[c].sa_remote,
                    sizeof(g_clients[c].sa_remote) ) != (ssize_t)len)
        {
            warn("unreliable send() failed");
        }
    }
}

static void kill_player(int c);

static void disconnect(int c, const char *reason)
{
    if (!g_clients[c].in_use) return;

    info( "disconnecting client %d at %s:%d (reason: %s)",
          c, inet_ntoa(g_clients[c].sa_remote.sin_addr),
          ntohs(g_clients[c].sa_remote.sin_port), reason );

    kill_player(c);

    g_clients[c].in_use = false;
    if (reason != NULL)
    {
        unsigned char packet[max_packet_len];
        size_t len = strlen(reason);
        if (len > max_packet_len - 1) len = max_packet_len - 1;
        packet[0] = MRSC_DISC;
        memcpy(packet + 1, reason, len);
        client_send(c, packet, len + 1, true);
    }
    close(g_clients[c].fd_stream);
    --g_num_clients;
    if (g_clients[c].player) --g_num_players;
    g_clients[c].player = false;
}

static void kill_player(int c)
{
    if (g_clients[c].dead_since != -1) return;
    info("client %d died.", c);
    g_clients[c].dead_since = g_timestamp;
    --g_num_alive;
}

static void handle_HELO(int c, unsigned char *buf, size_t len)
{
    Client *cl = &g_clients[c];

    /* Can only send this once! */
    if (cl->player) return;

    /* Verify packet data */
    int L = len > 1 ? buf[1] : 0;
    if (L < 1 || L > max_name_len || L + 2 > (int)len)
    {
        disconnect(c, "(HELO) invalid name length");
        return;
    }
    for (int n = 0; n < L; ++n)
    {
        if (buf[2 + n] < 32 || buf[2 + n] > 126)
        {
            disconnect(c, "(HELO) invalid name character");
            return;
        }
    }

    /* Assign player name */
    memcpy(cl->name, buf + 2, L);
    cl->name[L] = '\0';

    /* Check availability of name */
    for (int n = 0; n < max_clients; ++n)
    {
        if (n == c || !g_clients[n].in_use) continue;
        if (strcmp(cl->name, g_clients[n].name) == 0)
        {
            disconnect(c, "(HELO) name unavailable");
            return;
        }
    }
}

static void handle_MOVE(int c, unsigned char *buf, size_t len)
{
    Client *cl = g_players[c];

    if (len != 9 + move_backlog)
    {
        error( "(MOVE) invalid length packet received from client %d "
               "(received %d; expected %d)", c, len, 9 + move_backlog);
        return;
    }
    unsigned gameid = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
    if (gameid != g_gameid)
    {
        warn( "(MOVE) packet with invalid game id from client %d "
              "(received %d; expected %d)", c, gameid, g_gameid);
        return;
    }

    int timestamp = (buf[5] << 24) | (buf[6] << 16) | (buf[7] << 8) | buf[8];
    if (timestamp > g_timestamp + 1)
    {
        error("(MOVE) timestamp too great");
        return;
    }

    if (timestamp <= cl->timestamp)
    {
        warn("(MOVE) discarded out-of-order move packet %d,%d,%d", timestamp, cl->timestamp,0);
        return;
    }

    int added = timestamp - cl->timestamp;
    assert(added >= 0 && added <= move_backlog);
    memmove(cl->moves, cl->moves + added, move_backlog - added);
    memcpy( cl->moves + move_backlog - added,
            buf + 9 + move_backlog - added, added );

    for (int n = move_backlog - added; n < move_backlog; ++n)
    {
        if (cl->dead_since != -1)
        {
            cl->moves[n] = 4;
            continue;
        }

        int m = cl->moves[n];
        if (m < 1 || m > 3)
        {
            error("Invalid move %d received from client %d\n", m, c);
            kill_player(c);
        }
        else
        if (cl->dead_since == -1)
        {
            if (m == 2) cl->a += 2.0*M_PI/turn_rate;
            if (m == 3) cl->a -= 2.0*M_PI/turn_rate;
            double nx = cl->x + 1e-3*move_rate*cos(cl->a);
            double ny = cl->y + 1e-3*move_rate*sin(cl->a);

            /* First, blank out previous dot */
            plot(cl->x, cl->y, 0);

            /* Check for out-of-bounds or overlapping dots */
            if ( nx < 0 || nx >= 1 || ny < 0 || ny >= 1 ||
                 plot(nx, ny, c + 1) != 0 )
            {
                kill_player(c);
            }

            /* Redraw previous dot */
            plot(cl->x, cl->y, c + 1);

            cl->x = nx;
            cl->y = ny;
        }
    }
    cl->timestamp += added;
    assert(cl->timestamp == timestamp);
}

static void handle_packet( int c, unsigned char *buf, size_t len,
                           bool reliable )
{
/*
    info( "%s packet type %d of length %d received from client #%d",
          reliable ? "reliable" : "unreliable", (int)buf[0], len, c );
    hex_dump(buf, len);
*/

    switch ((int)buf[0])
    {
    case MRCS_HELO: return handle_HELO(c, buf, len);
    case MRCS_QUIT: return disconnect(c, "client quit");
    case MUCS_MOVE: return handle_MOVE(c, buf, len);
    default: disconnect(c, "invalid packet type");
    }
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

    sprintf(path, "field-%d.bmp", (int)time(NULL));
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

    int i = 0;
    for (int n = 0; n < max_clients; ++n)
    {
        if (!g_clients[n].in_use || !g_clients[n].name[0]) continue;

        if (!g_clients[n].player)
        {
            /* Enable player */
            g_clients[n].player = true;
            g_num_players += 1;
        }
        g_clients[n].dead_since = -1;
        g_clients[n].color = rgb_from_hue((double)i/g_num_players);
        g_clients[n].x = 0.1 + 0.8*rand()/RAND_MAX;
        g_clients[n].y = 0.1 + 0.8*rand()/RAND_MAX;
        g_clients[n].a = 2.0*M_PI*rand()/RAND_MAX;
        g_clients[n].timestamp = 0;
        memset(g_clients[n].moves, 0, sizeof(g_clients[n].moves));
        g_players[i] = &g_clients[n];
        ++i;
    }
    assert(i == g_num_players);
    g_num_alive = g_num_players;

    if (g_num_players == 0) return;

    info("Starting game with %d players", g_num_players);

    g_gameid = ((rand()&255) << 24) |
               ((rand()&255) << 16) |
               ((rand()&255) <<  8) |
               ((rand()&255) <<  0);

    /* Build start game packet */
    unsigned char packet[max_packet_len];
    size_t pos = 0;
    packet[pos++] = MRSC_STRT;
    packet[pos++] = data_rate;
    packet[pos++] = turn_rate;
    packet[pos++] = move_rate;
    packet[pos++] = move_backlog;
    packet[pos++] = g_num_players;
    packet[pos++] = 0;
    packet[pos++] = (g_gameid>>24)&255;
    packet[pos++] = (g_gameid>>16)&255;
    packet[pos++] = (g_gameid>> 8)&255;
    packet[pos++] = (g_gameid>> 0)&255;
    /* FIXME: possible buffer overflow here, depending on server parameters */
    for (int i = 0; i < g_num_players; ++i)
    {
        Client *cl = g_players[i];
        packet[pos++] = cl->color.r;
        packet[pos++] = cl->color.g;
        packet[pos++] = cl->color.b;
        int x = (int)(cl->x*65536);
        int y = (int)(cl->y*65536);
        int a = (int)(cl->a/(2.0*M_PI)*65536);
        packet[pos++] = (x>>8)&255;
        packet[pos++] = (x>>0)&255;
        packet[pos++] = (y>>8)&255;
        packet[pos++] = (y>>0)&255;
        packet[pos++] = (a>>8)&255;
        packet[pos++] = (a>>0)&255;
        size_t name_len = strlen(cl->name);
        packet[pos++] = name_len;
        memcpy(&packet[pos], cl->name, name_len);
        pos += name_len;
    }

    /* Send start packet to all players */
    for (int i = 0; i < g_num_players; ++i)
    {
        Client *cl = g_players[i];
        packet[6] = i;
        client_send(cl - g_clients, packet, pos, true);
    }
}

static void do_frame()
{
    if (g_num_clients == 0) return;
    if (g_num_alive == 0) restart_game();
    if (g_num_players == 0) return;

    /* Disconnect out-of-sync clients */
    for (int n = 0; n < g_num_players; ++n)
    {
        int backlog = g_timestamp - g_players[n]->timestamp;
        assert(backlog >= 0);
        if (backlog >= move_backlog)
        {
            disconnect(g_players[n] - g_clients, "out of sync");
        }
    }

    /* Send moves packet to all players */
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
            g_timestamp - g_players[n]->dead_since >= move_backlog)
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
            g_timestamp - g_players[n]->dead_since >= move_backlog)
        {
            continue;
        }
        int backlog = g_timestamp - g_players[n]->timestamp;
        memcpy(packet + pos, g_players[n]->moves + backlog,
                             move_backlog - backlog);
	memset(packet + pos + move_backlog - backlog, 0, backlog);
        pos += move_backlog;
    }
    for (int n = 0; n < g_num_players; ++n)
    {
        client_send(g_players[n] - g_clients, packet, pos, false);
    }
}

static int run()
{
    for (;;)
    {
        /* Compute time to next tick */
        double delay = (double)g_timestamp/server_fps - time_now();
        if (delay <= 0)
        {
            do_frame();
            delay += (double)1/server_fps;
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
        for (int n = 0; n < max_clients; ++n)
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
                while (n < max_clients && g_clients[n].in_use) ++n;
                if (n == max_clients)
                {
                    warn( "No client slot free; rejecting connection from %s:%d",
                          inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
                    close(fd);
                }
                else
                {
                    /* Initialize new client slot */
                    memset(&g_clients[n], 0, sizeof(g_clients[n]));
                    g_clients[n].in_use    = true;
                    g_clients[n].sa_remote = sa;
                    g_clients[n].fd_stream = fd;
                    info( "Accepted client from %s:%d in slot #%d",
                          inet_ntoa(sa.sin_addr), ntohs(sa.sin_port), n );
                    g_num_clients += 1;
                }
            }
        }

        /* Accept incoming packets */
        if (FD_ISSET(g_fd_packet, &readfds))
        {
            struct sockaddr_in sa;
            socklen_t sa_len = sizeof(sa);
            unsigned char buf[max_packet_len];
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
                int c = -1;
                for (int n = 0; n < max_clients; ++n)
                {
                    if ( g_clients[n].in_use &&
                         g_clients[n].sa_remote.sin_addr.s_addr ==
                            sa.sin_addr.s_addr &&
                         g_clients[n].sa_remote.sin_port == sa.sin_port )
                    {
                        c = n;
                        break;
                    }
                }
                if (c == -1)
                {
                    warn ( "packet from %s:%d ignored (not registered)",
                        inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
                }
                else
                {
                    handle_packet(c, buf, buf_len, false);
                }
            }
        }

        /* Accept incoming stream packets */
        for (int n = 0; n < max_clients; ++n)
        {
            if (!g_clients[n].in_use) continue;
            if (!FD_ISSET(g_clients[n].fd_stream, &readfds)) continue;

            ssize_t read = recv(
                g_clients[n].fd_stream, g_clients[n].buf + g_clients[n].buf_pos,
                sizeof(g_clients[n].buf) - g_clients[n].buf_pos, 0 );
            if (read <= 0)
            {
                disconnect(n, read == 0 ? "EOF reached" : "recv() failed");
            }
            else
            {
                g_clients[n].buf_pos += read;
                if (g_clients[n].buf_pos >= 2)
                {
                    int len = 256*g_clients[n].buf[0] + g_clients[n].buf[1];
                    if (len > max_packet_len)
                    {
                        disconnect(n, "packet too large");
                    }
                    else
                    if (len < 1)
                    {
                        disconnect(n, "packet too small");
                    }
                    else
                    if (g_clients[n].buf_pos >= len + 2)
                    {
                        handle_packet(n, g_clients[n].buf + 2, len, true);
                        memmove( g_clients[n].buf, g_clients[n].buf + len + 2,
                                 g_clients[n].buf_pos - len - 2);
                        g_clients[n].buf_pos -= len + 2;
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

    struct sockaddr_in sa_local;
    sa_local.sin_family = AF_INET;
    sa_local.sin_port   = htons(12321);
    sa_local.sin_addr.s_addr = INADDR_ANY;

    /* Create TCP listening socket */
    g_fd_listen = socket(PF_INET, SOCK_STREAM, 0);
    if (g_fd_listen < 0)
    {
        fatal("Could not create TCP socket: socket() failed");
    }
    if (bind(g_fd_listen, (struct sockaddr*)&sa_local, sizeof(sa_local)) != 0)
    {
        fatal("Could not create TCP socket: bind() failed");
    }
    if (listen(g_fd_listen, 4) != 0)
    {
        fatal("Could not create TCP socket: listen() failed");
    }

    /* Create UDP datagram socket */
    g_fd_packet = socket(PF_INET, SOCK_DGRAM, 0);
    if (g_fd_packet < 0)
    {
        fatal("Could not create UDP socket: socket() failed");
    }
    if (bind(g_fd_packet, (struct sockaddr*)&sa_local, sizeof(sa_local)) != 0)
    {
        fatal("Could not create UDP socket: bind() failed");
    }

    /* Main loop */
    return run();
}