#define _POSIX_SOURCE

#include <common/Protocol.h>
#include <common/Debug.h>
#include <common/Time.h>
#include <common/Field.h>
#include <common/BMP.h>

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#ifndef WIN32
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#else
#include <winsock2.h>
typedef int socklen_t;
#define close closesocket
#define ioctl ioctlsocket
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_CLIENTS           (64)
#define SERVER_FPS            (30)
#define MOVE_BACKLOG          (60)
#define MAX_PACKET_LEN      (4094)
#define MAX_NAME_LEN          (20)
#define PLAYERS_PER_CLIENT     (4)
#define TURN_RATE             (40)
#define MOVE_RATE              (7)
#define SCORE_HISTORY         (10)
#define HOLE_PROBABILITY      (60)
#define HOLE_LENGTH_MIN        (2)
#define HOLE_LENGTH_MAX        (6)
#define HOLE_COOLDOWN         (10)

#define VICTORY_TIME        (3*SERVER_FPS)
#define WARMUP              (3*SERVER_FPS)
#define MAX_PLAYERS         (PLAYERS_PER_CLIENT*MAX_CLIENTS)

#define REPLAYDIR           "replay"

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

    /* Scores */
    int             score_total;
    int             score_moving_sum;
    int             score_history[SCORE_HISTORY];

    /* Hole creation */
    int             hole, prev_hole;
    int             solid_since;

    /* Multiply-with-carry RNG for this player
       (used to determine random state transitions) */
    unsigned        rng_base;       /* base (current) value */
    unsigned        rng_carry;      /* last carry value */
} Player;


typedef struct Client
{
    /* Set to true only if this client is currently in use */
    bool            in_use;
    bool            hailed;        /* have we received a HELO? */
    int             protocol;      /* protocol version used by the client */

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
static double g_time_start;     /* Time since at last game restart */
static int g_deadline;          /* Game ends at this time */
static int g_num_clients;       /* Number of connected clients */
static int g_num_players;       /* Number of people in the current game */
static int g_num_alive;         /* Number of people still alive */
static unsigned g_gameid;
static Client g_clients[MAX_CLIENTS];
static Player *g_players[MAX_PLAYERS];
unsigned char g_field[1000][1000];

FILE *fp_replay;                /* Record replay to this file */

/*
    Function prototypes
*/

/* Convert a hue (0 <= hue < 1) to RGB */
static struct RGB rgb_from_hue(double hue);

/* Disconnect a client (sending the given reason, if possible) */
static void client_disconnect(Client *cl, const char *reason);

/* Send a message to all connected clients */
static void client_broadcast(const void *buf, size_t len, bool reliable);

/* Send a message to a client */
static void client_send(Client *cl, const void *buf, size_t len, bool reliable);

/* Kill a player */
static void player_kill(Player *p);

/* Network handling */
static void handle_packet( Client *cl,
                           unsigned char *buf, size_t len, bool reliable );
static void handle_HELO(Client *cl, unsigned char *buf, size_t len);
static void handle_MOVE(Client *cl, unsigned char *buf, size_t len);
static void handle_CHAT(Client *cl, unsigned char *buf, size_t len);

/* Restart the game (called when all players have died) */
static void restart_game();

/* Send scores to all client */
static void send_scores();

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

static void client_broadcast(const void *buf, size_t len, bool reliable)
{
    for (int n = 0; n < MAX_CLIENTS; ++n)
    {
        Client *cl = &g_clients[n];
        if (!cl->in_use) continue;
        if (!reliable && !cl->hailed) continue;
        client_send(cl, buf, len, reliable);
    }
}

static void client_send(Client *cl, const void *buf, size_t len, bool reliable)
{
    assert(len < (size_t)MAX_PACKET_LEN);
    if (reliable)
    {
        unsigned char header[2];
        header[0] = len>>8;
        header[1] = len&255;
        if ( send(cl->fd_stream, header, 2, 0) != 2 ||
             send(cl->fd_stream, buf, len, 0) != (ssize_t)len )
        {
            warn("reliable send() failed");
            client_disconnect(cl, NULL);
        }
    }
    else
    {
        if (sendto( g_fd_packet, buf, len, 0,
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

    /* Set player dead */
    pl->dead_since = g_timestamp;
    --g_num_alive;
    if (g_num_alive <= 1 && g_deadline == -1)
    {
        /* End the game after a fixed number of seconds */
        g_deadline = g_timestamp + VICTORY_TIME;
    }

    /* Give remaining players a point.
       NB. if two players die in the same turn, they both get a point from
           each other's death. */
    for (int n = 0; n < g_num_players; ++n)
    {
        if (g_players[n] == pl) continue;
        if ( g_players[n]->dead_since == -1 ||
             g_players[n]->dead_since >= g_timestamp )
        {
            g_players[n]->score_total += 1;
            g_players[n]->score_moving_sum += 1;
            g_players[n]->score_history[0] += 1;
        }
    }

    send_scores();
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
    case MUCS_MOVE: return handle_MOVE(cl, buf, len);
    case MRCS_CHAT: return handle_CHAT(cl, buf, len);
    case MRCS_QUIT: return client_disconnect(cl, "client quit");
    default: client_disconnect(cl, "invalid packet type");
    }
}

static void handle_HELO(Client *cl, unsigned char *buf, size_t len)
{
    /* Check if players have already been registered */
    if (cl->hailed) return;
    cl->hailed = true;

    if (len < 3)
    {
        client_disconnect(cl, "(HELO) truncated packet");
        return;
    }
    cl->protocol = buf[1];
    if (cl->protocol != 1)
    {
        client_disconnect(cl, "(HELO) unsupported protocol (expected 1)");
        return;
    }

    int P = buf[2];
    if (P > PLAYERS_PER_CLIENT)
    {
        client_disconnect(cl, "(HELO) invalid number of players");
        return;
    }

    size_t pos = 3;
    for (int p = 0; p < P; ++p)
    {
        Player *pl = &cl->players[p];

        /* Parse name length */
        if (pos >= len)
        {
            client_disconnect(cl, "(HELO) truncated packet");
            return;
        }
        size_t L = buf[pos++];
        if (L > MAX_NAME_LEN)
        {
            client_disconnect(cl, "(HELO) player name too long");
            return;
        }
        if (L < 1 || L > len - pos)
        {
            client_disconnect(cl, "(HELO) invalid name length");
            return;
        }

        /* Assign player name */
        for (size_t n = 0; n < L; ++n)
        {
            pl->name[n] = buf[pos++];
            if (pl->name[n] < 32 || pl->name[n] > 126)
            {
                client_disconnect(cl, "(HELO) invalid characters in player name");
                return;
            }
        }
        if (pl->name[0] == ' ' || pl->name[L-1] == ' ')
        {
            client_disconnect(cl, "(HELO) player name may not start or "
                                  "end with space");
        }
        pl->name[L] = '\0';

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
                    char reason[128];
                    sprintf(reason, "(HELO) player name \"%s\" already in use "
                                    "on this server", cl->players[p].name);
                    client_disconnect(cl, reason);
                    return;
                }
            }
        }

        /* Enable player */
        pl->in_use = true;
    }
}

static void handle_MOVE(Client *cl, unsigned char *buf, size_t len)
{
    size_t P = 0;

    while (P < PLAYERS_PER_CLIENT && cl->players[P].in_use) ++P;

    if (len != 9 + P*MOVE_BACKLOG)
    {
        error( "(MOVE) invalid length packet received from client %d "
               "(received %d; expected %d)", cl - g_clients,
               len, 9 + P*MOVE_BACKLOG );
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
        error( "(MOVE) timestamp too great (received: %d; expected %d)",
                timestamp, g_timestamp + 1 );
        return;
    }

    for (size_t p = 0; p < P; ++p)
    {
        Player *pl = &cl->players[p];

        int added = timestamp - pl->timestamp;
        if (added < 0)
        {
            warn("(MOVE) discarded out-of-order move packet");
            return;
        }
        if (added > MOVE_BACKLOG)
        {
            warn("(MOVE) discarded packet from out-of-sync player");
            return;
        }

        memmove(pl->moves, pl->moves + added, MOVE_BACKLOG - added);
        memcpy( pl->moves + MOVE_BACKLOG - added,
                buf + 9 + (p + 1)*MOVE_BACKLOG - added, added );

        for (int n = MOVE_BACKLOG - added; n < MOVE_BACKLOG; ++n)
        {
            if (pl->dead_since != -1) pl->moves[n] = 4;

            int m = pl->moves[n];
            if (m < 1 || m > 4)
            {
                error("received invalid move %d\n", m);
                player_kill(pl);
            }
            else
            if (pl->dead_since == -1)
            {
                if ( pl->hole == 0 &&
                     pl->timestamp >= WARMUP +  HOLE_COOLDOWN &&
                     pl->timestamp - pl->solid_since >= HOLE_COOLDOWN &&
                     pl->rng_base%HOLE_PROBABILITY == 0 )
                {
                    pl->hole = HOLE_LENGTH_MIN +
                               (pl->rng_base/HOLE_PROBABILITY)
                               % (HOLE_LENGTH_MAX - HOLE_LENGTH_MIN + 1);
                }

                int a = (m == 2) ? +1 : (m == 3) ? -1 : 0;
                int v = (pl->timestamp < WARMUP ? 0 : 1);

                if (fp_replay != NULL)
                {
                    /* Write to trace: time, player, turn, move*/
                    fprintf( fp_replay, "%d %d %d %d\n",
                             pl->timestamp, pl->index, a, (pl->hole ? 2 : v) );
                }

                /* Calculate new angle/position */
                pl->a += a*2.0*M_PI/TURN_RATE;
                double nx = pl->x + v*1e-3*MOVE_RATE*cos(pl->a);
                double ny = pl->y + v*1e-3*MOVE_RATE*sin(pl->a);

                /* First, blank out previous dot */
                if (pl->prev_hole == 0)
                {
                    field_plot(&g_field, pl->x, pl->y, 0);
                }

                /* Check for out-of-bounds or overlapping dots */
                if ( nx < 0 || nx >= 1 || ny < 0 || ny >= 1 ||
                     field_plot(&g_field,nx, ny, pl->index + 1) != 0 )
                {
                    player_kill(pl);
                }

                /* Redraw previous dot */
                if (pl->prev_hole == 0)
                {
                    field_plot(&g_field, pl->x, pl->y, pl->index + 1);
                }

                if (pl->hole > 0) field_plot(&g_field,nx, ny, 0);

                pl->x = nx;
                pl->y = ny;

                if (pl->prev_hole != 0)
                {
                    pl->solid_since = pl->timestamp;
                }

                pl->prev_hole = pl->hole;
                if (pl->hole > 0) --pl->hole;
            }

            ++pl->timestamp;

            unsigned long long rng_next = pl->rng_base*1967773755ull
                                        + pl->rng_carry;
            pl->rng_base  = rng_next&0xffffffff;
            pl->rng_carry = rng_next>>32;
        }
        assert(pl->timestamp == timestamp);
    }
}

static void handle_CHAT(Client *cl, unsigned char *buf, size_t len)
{
    (void)cl;

    unsigned char *msg = &buf[1];
    size_t msg_len = len - 1;
    if (msg_len < 1) return;
    if (msg_len > 255) msg_len = 255;   /* limit chat message length */

    Player *pl = NULL;
    for (int n = 0; n < PLAYERS_PER_CLIENT; ++n)
    {
        if (cl->players[n].in_use)
        {
            pl = &cl->players[n];
            break;
        }
    }
    if (!pl)
    {
        warn("Chat message from unregistered player ignored.");
        return;
    }

    msg[msg_len] = '\0'; /* hack */
    info("(CHAT) %s: %s", pl->name, msg);

    /* Build message packet */
    unsigned char packet[MAX_PACKET_LEN];
    size_t pos = 0, name_len = strlen(pl->name);
    packet[pos++] = MRSC_MESG;
    packet[pos++] = name_len;
    memcpy(&packet[pos], pl->name, name_len);
    pos += name_len;
    for (size_t n = 0; n < msg_len; ++n)
    {
        packet[pos++] = (msg[n] >= 32 && msg[n] <= 126) ? msg[n] : ' ';
    }
    client_broadcast(packet, pos, true);
}

static void restart_game()
{
    g_time_start = time_now();
    g_timestamp = 0;
    g_deadline = -1;

    if (g_num_clients == 0) return;
    if (g_num_players > 0)

#ifdef DEBUG
    if (g_gameid != 0)
    {
        char path[32];

        /* Dump field to BMP image */
        sprintf(path, "bmp/field-%08x.bmp", g_gameid);
        if (bmp_write(path, &g_field[0][0], 1000, 1000))
        {
            info("Field dumped to file \"%s\"", path);
        }
        else
        {
            warn("Couldn't write BMP file \"%s\"", path);
        }
    }
#endif

    /* Update scores */
    for (int n = 0; n < g_num_players; ++n)
    {
        Player *pl = g_players[n];
        pl->score_moving_sum -= pl->score_history[SCORE_HISTORY - 1];
        for (int s = SCORE_HISTORY - 1; s > 0; --s)
        {
            pl->score_history[s] = pl->score_history[s - 1];
        }
        pl->score_history[0] = 0;
    }

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

    g_num_alive = g_num_players;

    g_gameid = ((rand()&255) << 24) |
               ((rand()&255) << 16) |
               ((rand()&255) <<  8) |
               ((rand()&255) <<  0);

    /* Initialize players */
    for (int n = 0; n < g_num_players; ++n)
    {
        Player *pl = g_players[n];

        /* Reset player state to starting position */
        pl->dead_since = -1;
        pl->color = rgb_from_hue((double)pl->index/g_num_players);
        pl->x = 0.02 + 0.96*rand()/RAND_MAX;
        pl->y = 0.02 + 0.96*rand()/RAND_MAX;
        pl->a = 2.0*M_PI*rand()/RAND_MAX;
        pl->timestamp = 0;
        memset(pl->moves, 0, sizeof(pl->moves));
        pl->hole        = 0;
        pl->prev_hole   = 0;
        pl->solid_since = 0;
        pl->rng_base    = g_gameid ^ n;
        pl->rng_carry   = 0;
    }

    {
        /* Open replay file for new game */
        char path[32];
        if (fp_replay != NULL) fclose(fp_replay);
        sprintf(path, REPLAYDIR "/game-%08x.txt", g_gameid);
        fp_replay = fopen(path, "wt");
        if (fp_replay != NULL)
        {
            info("Opened replay file \"%s\"", path);

            /* Write header */
            fprintf( fp_replay, "%d %u %d\n",
                     1, g_gameid, g_num_players );
            fprintf( fp_replay, "%d %d %d %d %d %d %d %d\n",
                     SERVER_FPS, TURN_RATE, MOVE_RATE, WARMUP, HOLE_PROBABILITY,
                     HOLE_LENGTH_MIN, HOLE_LENGTH_MAX, HOLE_COOLDOWN );

            for (int n = 0; n < g_num_players; ++n)
            {
                fprintf(fp_replay, "%s\n", g_players[n]->name);
            }
            for (int n = 0; n < g_num_players; ++n)
            {
                fprintf( fp_replay, "%.6f %.6f %.6f\n",
                         g_players[n]->x, g_players[n]->y, g_players[n]->a );
            }
        }
        else
        {
            warn("Couldn't open file \"%s\" for writing", path);
        }
    }

    /* Build start game packet */
    unsigned char packet[MAX_PACKET_LEN];
    size_t pos = 0;
    packet[pos++] = MRSC_STRT;
    packet[pos++] = SERVER_FPS;
    packet[pos++] = TURN_RATE;
    packet[pos++] = MOVE_RATE;
    packet[pos++] = WARMUP;
    packet[pos++] = SCORE_HISTORY;
    packet[pos++] = (HOLE_PROBABILITY >> 8)&255;
    packet[pos++] = (HOLE_PROBABILITY >> 0)&255;
    packet[pos++] = HOLE_LENGTH_MIN;
    packet[pos++] = HOLE_LENGTH_MAX - HOLE_LENGTH_MIN;
    packet[pos++] = HOLE_COOLDOWN;
    packet[pos++] = MOVE_BACKLOG;
    packet[pos++] = g_num_players;
    packet[pos++] = (g_gameid>>24)&255;
    packet[pos++] = (g_gameid>>16)&255;
    packet[pos++] = (g_gameid>> 8)&255;
    packet[pos++] = (g_gameid>> 0)&255;
    assert(pos == 17);
    for (int i = 0; i < g_num_players; ++i)
    {
        Player *pl = g_players[i];
        assert(pos + 10 < MAX_PACKET_LEN);
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
        assert(pos + name_len < MAX_PACKET_LEN);
        memcpy(&packet[pos], pl->name, name_len);
        pos += name_len;
    }

    /* Send start packet to all clients */
    client_broadcast(packet, pos, true);
    send_scores();

    /* Reset timer (important!) */
    time_reset();
}

static void send_scores()
{
    unsigned char packet[MAX_PACKET_LEN];
    size_t pos = 0;

    packet[pos++] = MRSC_SCOR;
    for (int n = 0; n < g_num_players; ++n)
    {
        packet[pos++] = g_players[n]->score_total >> 8;
        packet[pos++] = g_players[n]->score_total&255;
        packet[pos++] = g_players[n]->score_history[0] >> 8;
        packet[pos++] = g_players[n]->score_history[0]&255;
        packet[pos++] = g_players[n]->score_moving_sum >> 8;
        packet[pos++] = g_players[n]->score_moving_sum&255;
        packet[pos++] = 0;
        packet[pos++] = 0;
    }
    client_broadcast(packet, pos, true);

}

static void do_frame()
{
    if (g_num_clients == 0) return;

    if ( (g_num_alive == 0 && g_deadline == -1) ||
         (g_deadline != -1 && g_timestamp >= g_deadline) )
    {
        restart_game();
    }

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
    assert(pos + g_num_players < MAX_PACKET_LEN);
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
        assert(pos + MOVE_BACKLOG < MAX_PACKET_LEN);
        int backlog = g_timestamp - g_players[n]->timestamp;
        if (backlog > MOVE_BACKLOG) backlog = MOVE_BACKLOG;
        memcpy(packet + pos, g_players[n]->moves + backlog,
                             MOVE_BACKLOG - backlog);
        memset(packet + pos + MOVE_BACKLOG - backlog, 0, backlog);
        pos += MOVE_BACKLOG;
    }

    client_broadcast(packet, pos, false);
}

/* Processes frames until the server is up to date, and returns the
   number of seconds until the next frame must be processed. */
static double process_frames()
{
    do {
        /* Compute time to next tick */
        double delay = (double)(g_timestamp + 1)/SERVER_FPS - time_now();
        if (delay > 0) return delay;
        g_timestamp += 1;
        do_frame();
    } while (g_num_players > 0);
    return 1.0;
}

static bool make_non_blocking(int fd)
{
    unsigned v = 1;
    return ioctl(fd, FIONBIO, &v) == 0;
}

static int run()
{
    for (;;)
    {
        /* Calculcate select time-out */
        struct timeval timeout;
        double delay = process_frames();
        timeout.tv_sec  = (int)floor(delay);
        timeout.tv_usec = (int)(1e6*(delay - floor(delay)));

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
            if (!make_non_blocking(fd))
            {
                error("could not put TCP socket non-blocking mode");
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
                    info( "accepted client from %s:%d in slot #%d",
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

        /* Ensure server is still up to date */
        process_frames();

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
    srand(time(NULL));
    time_reset();

    (void)argc;
    (void)argv;

#ifdef WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0)
    {
        fatal("WSAStartup failed!");
    }
#endif

#ifndef WIN32
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
#endif

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
    if (!make_non_blocking(g_fd_packet))
    {
        fatal("could not put UDP socket in non-blocking mode");
    }

    /* Main loop */
    return run();
}
