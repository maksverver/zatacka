#define _POSIX_SOURCE

#include <common/BMP.h>
#include <common/Colors.h>
#include <common/Debug.h>
#include <common/Field.h>
#include <common/Movement.h>
#include <common/Protocol.h>
#include <common/Time.h>

#include <assert.h>
#include <math.h>
#include <stdarg.h>
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
#include <netinet/tcp.h>
typedef int SOCKET;
const SOCKET INVALID_SOCKET = -1;
#else
#include <winsock2.h>
#define send(s,b,l,f) send(s,(char*)b,l,f)
#define recv(s,b,l,f) recv(s,(char*)b,l,f)
#define close(s) closesocket(s)
#define ioctl(s,c,a) ioctlsocket(s,c,a)
typedef int socklen_t;
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_CLIENTS           (64)
#define SERVER_FPS            (30)
#define MOVE_BACKLOG          (60)
#define MAX_PACKET_LEN     (16384)
#define MAX_NAME_LEN          (20)
#define PLAYERS_PER_CLIENT     (4)
#define TURN_RATE             (48)
#define MOVE_RATE              (6)
#define LINE_WIDTH             (7)
#define SCORE_HISTORY         (10)
#define HOLE_PROBABILITY      (60)
#define HOLE_LENGTH_MIN        (3)
#define HOLE_LENGTH_MAX        (8)
#define HOLE_COOLDOWN         (10)

#define VICTORY_TIME        (3*SERVER_FPS)
#define WARMUP              (3*SERVER_FPS)
#define MAX_PLAYERS         (PLAYERS_PER_CLIENT*MAX_CLIENTS)

#ifdef REPLAY
#ifndef REPLAYDIR
#define REPLAYDIR           "replay"
#endif /* ndef REPLAYDIR */
#endif /* def REPLAY */


typedef struct Player
{
    /* Set to false if the player is in use. If true, name must be valid. */
    bool            in_use;

    /* Move buffer */
    int             timestamp;
    char            moves_queue[MOVE_BACKLOG];
    int             moves_queue_len;
    bool            has_moved;              /* did player move during warmup? */

    /* Player info */
    int             index;
    int             dead_since;
    int             flags;
    char            name[MAX_NAME_LEN + 1];
    struct RGB      color;
    Position        pos;

    /* Scores */
    int             score_total;
    int             score_moving_sum;
    int             score_history[SCORE_HISTORY];
    int             score_holes;

    /* Hole creation */
    int             hole;           /* remaining length of hole (in frames) */
    int             solid_since;    /* timestamp after last hole */
    int             my_holeid;      /* id of current hole being created */
    int             cross_holeid;   /* id of hole being crossed */

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
    bool            started;       /* game start acknowledged? */
    int             protocol;      /* protocol version used by the client */
    int             flags;         /* connection flags */

    /* Remote address (TCP only) */
    struct sockaddr_in sa_remote;

    /* Streaming data */
    SOCKET          fd_stream;
    unsigned char   buf[MAX_PACKET_LEN + 2];
    int             buf_pos;

    /* Players controlled by the client */
    Player          players[PLAYERS_PER_CLIENT];
} Client;


/*
    Global variables
*/
static SOCKET g_fd_listen;      /* Stream data listening socket */
static SOCKET g_fd_packet;      /* Packet data socket */
static int g_timestamp;         /* Time counter */
static double g_time_start;     /* Time since at last game restart */
static int g_deadline;          /* Game ends at this time */
static int g_num_clients;       /* Number of connected clients */
static int g_num_players;       /* Number of people in the current game */
static int g_num_alive;         /* Number of people still alive */
static unsigned g_gameid;       /* Game identifier (also used as random seed) */
static unsigned g_num_holes;    /* Number of holes created */
static Client g_clients[MAX_CLIENTS];
static Player *g_players[MAX_PLAYERS];
static unsigned char g_field[FIELD_SIZE][FIELD_SIZE];
static unsigned char g_holes[FIELD_SIZE][FIELD_SIZE];

static char packet_data_[MAX_PACKET_LEN + 2];     /* packet data buffer */
static char *packet_buf = packet_data_ + 2;       /* packet payload */
static size_t packet_len;                         /* size of packet payload */

#ifdef REPLAY
static FILE *fp_replay;         /* Record replay to this file */
#endif

/*
    Function prototypes
*/

/* Return a uniformly random integer in the range [lo,hi] (inclusive!) */
static int rand_int(int lo, int hi);

/* Three-way comparison of two RGB colors */
static int rgb_cmp(const struct RGB *c, const struct RGB *d);

/* Determine if the given RGB color is black (default initialized) */
static bool rgb_black(const struct RGB *c);

/* Disconnect a client (sending the given reason, if possible) */
static void client_disconnect(Client *cl, const char *reason);

/* Start a new packet with the given type */
static void packet_begin(int type);

/* Add a byte to the packet */
static size_t packet_write_byte(int value);

/* Add data to the packet */
static size_t packet_write(const char *buf, size_t len);

/* Add formatted data to the packet */
static int packet_vprintf(const char *fmt, va_list ap);

/* Finish the packet */
static void packet_end();

/* Send a packet to a client */
static void packet_send(Client *cl, bool reliable);

/* Broadcast a packet to all connected clients */
static void packet_broadcast(bool reliable);

/* Kill a player */
static void player_kill(Player *p);

/* Send a formatted server message */
static void message(const char *fmt, ...);

/* Network handling */
static void handle_packet( Client *cl,
                           unsigned char *buf, size_t len, bool reliable );
static void handle_HELO(Client *cl, unsigned char *buf, size_t len);
static void handle_MOVE(Client *cl, unsigned char *buf, size_t len);
static void handle_CHAT(Client *cl, unsigned char *buf, size_t len);

/* Restart the game (called when all players have died) */
static void restart_game();

/* Send scores to all clients */
static void send_scores();

/* Process a server frame. */
static void do_frame();

/* Server main loop */
static int run();

/* Application entry point */
int main(int argc, char *argv[]);


/* Function definitions */

#ifdef WIN32
#define srandom(seed) srand(seed)

static int random()
{
    /* On Windows, rand() returns a 15 bits number.
       We combine the result from three calls to obtain a 31 number. */
    assert(RAND_MAX == 32767);
    return (rand() >> 14) | (rand() << 1) | (rand() << 16); 
}
#endif /* def WIN32 */

/* NOTE: caller must ensure hi - lo + 1 does not overflow an int! */
static int rand_int(int lo, int hi)
{
    const int random_max = 2147483647;
    assert(lo <= hi);
    assert(hi - lo <= random_max);
    int range = hi - lo + 1;
    int lim = (random_max/range)*range;
    int i;
    do { i = random(); } while (i >= lim);
    return lo + i%range;
}

static int rgb_cmp(const struct RGB *c, const struct RGB *d)
{
    if (c->r != d->r) return c->r - d->r;
    if (c->g != d->g) return c->g - d->g;
    if (c->b != d->b) return c->b - d->b;
    return 0;
}

static bool rgb_black(const struct RGB *c)
{
    return c->r + c->g + c->b == 0;
}

static void packet_begin(int type)
{
    packet_len = 0;
    packet_buf[packet_len++] = type;
}

static size_t packet_write_byte(int value)
{
    if (packet_len < MAX_PACKET_LEN)
    {
        packet_buf[packet_len++] = value;
        return 1;
    }
    else
    {
        return 0;
    }
}

static size_t packet_write(const char *buf, size_t len)
{
    size_t bytes_left = MAX_PACKET_LEN - packet_len;
    if (bytes_left < len) len = bytes_left;
    memcpy(packet_buf + packet_len, buf, len);
    packet_len += len;
    return len;
}

static int packet_vprintf(const char *fmt, va_list ap)
{
    int bytes_left = MAX_PACKET_LEN - packet_len;
    int written = vsnprintf(packet_buf + packet_len, bytes_left, fmt, ap);
    if (written < 0) return 0;
    if (written > bytes_left) written = bytes_left;
    packet_len += written;
    return written;
}

static void packet_end()
{
    /* Add 16-bit packet length in front (in network order) */
    packet_buf[-2] = (packet_len >> 8)&0xff;
    packet_buf[-1] = (packet_len >> 0)&0xff;
}

static void packet_broadcast(bool reliable)
{
    for (int n = 0; n < MAX_CLIENTS; ++n)
    {
        Client *cl = &g_clients[n];
        if (!cl->in_use) continue;
        if (!reliable && !cl->hailed) continue;
        packet_send(cl, reliable);
    }
}

static void packet_send(Client *cl, bool reliable)
{
    /* Verify that packet_end() has been called: */
    assert(packet_buf[-1] || packet_buf[-2]);

    if (reliable || (cl->flags & CLFL_RELIABLE_ONLY))
    {
        char *buf = packet_buf - 2;
        ssize_t len = packet_len + 2;
        if (send(cl->fd_stream, buf, len, 0) != (ssize_t)len)
        {
            info("reliable send() failed");

            /* we must not provide a reason, to prevent an infinite send loop */
            client_disconnect(cl, NULL);
        }
    }
    else
    {
        if (sendto( g_fd_packet, packet_buf, packet_len, 0,
                    (struct sockaddr*)&cl->sa_remote,
                    sizeof(cl->sa_remote) ) != (ssize_t)packet_len)
        {
            info("unreliable send() failed");
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
        packet_begin(MRSC_DISC);
        packet_write(reason, strlen(reason));
        packet_end();
        packet_send(cl, true);
    }
    close(cl->fd_stream);
}

static void message(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    packet_begin(MRSC_MESG);
    packet_write_byte(0);
    packet_vprintf(fmt, ap);
    packet_end();
    packet_broadcast(true);

    va_end(ap);
}

static void player_kill(Player *pl)
{
    if (!pl->in_use)
    {
        /* this occasionally happens if a client is disconnected while its MOVE
           packet is being processed (or maybe in other situations). */
        warn("player_kill() called on player slot that is not in use\n");
        return;
    }

    if (pl->dead_since != -1) return;

    info("player %d died.", pl->index);

    /* Set player dead */
    pl->dead_since = pl->timestamp;
    --g_num_alive;

    if (pl->timestamp >= WARMUP)
    {
        if (g_num_alive <= 1 && g_deadline == -1)
        {
            /* End the game after a fixed number of seconds */
            g_deadline = pl->timestamp + VICTORY_TIME;
        }

        /* Give remaining players a point.
           NB. if two players die in the same turn, they both get a point from
           each other's death. */
        for (int n = 0; n < g_num_players; ++n)
        {
            if (g_players[n] == pl) continue;
            if ( g_players[n]->dead_since == -1 ||
                 g_players[n]->dead_since >= g_timestamp - (g_players[n] > pl) )
            {
                g_players[n]->score_total += 1;
                g_players[n]->score_moving_sum += 1;
                g_players[n]->score_history[0] += 1;
            }
        }

        send_scores();
    }
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
    case MRCS_STRT: cl->started = true; return;
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
    if (cl->protocol != 2)
    {
        client_disconnect(cl, "(HELO) unsupported protocol (expected 2)");
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

        if (len - pos < 2)
        {
            client_disconnect(cl, "(HELO) truncated packet");
            return;
        }

        /* Get player flags */
        pl->flags = buf[pos++]&PLFL_ALL;

        /* Get name length */
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

    if (pos < len)
    {
        cl->flags = buf[pos++]&CLFL_ALL;
    }
}

static void handle_MOVE(Client *cl, unsigned char *buf, size_t len)
{
    if (!cl->started) return;

    size_t P = 0;

    while (P < PLAYERS_PER_CLIENT && cl->players[P].in_use) ++P;

    if (len != 1 + P)
    {
        error( "(MOVE) invalid length packet received from client %d "
               "(received %d; expected %d)", cl - g_clients, len, 1 + P );
        return;
    }

    for (size_t p = 0; p < P; ++p)
    {
        Player *pl = &cl->players[p];
        int m = buf[1 + p];
        if (m < MOVE_FORWARD || m > MOVE_DEAD)
        {
            error("(MOVE) received invalid move %d", m);
            player_kill(pl);
        }

        if (pl->dead_since == -1)
        {
            if (pl->moves_queue_len == MOVE_BACKLOG)
            {
                error("(MOVE) player move queue is full");
                player_kill(pl);
            }
            else
            {
                pl->moves_queue[pl->moves_queue_len++] = m;
            }
        }
    }
}

static void handle_CHAT(Client *cl, unsigned char *buf, size_t len)
{
    size_t pos = 1;

    /* Get player name */
    char name[MAX_NAME_LEN + 1];
    if (len - pos < 1) goto malformed;
    size_t L = buf[pos++];
    if (L > len - pos || L > MAX_NAME_LEN) goto malformed;
    memcpy(name, &buf[pos], L);
    name[L] = '\0';
    pos += L;
    hex_dump(buf, len);
    printf("%zd %s\n", L, name);

    /* Get message text */
    unsigned char *msg = &buf[pos];
    size_t msg_len = len - pos;
    if (msg_len < 1) return;
    if (msg_len > 255) msg_len = 255;   /* limit chat message length */

    /* Ensure player name is valid */
    Player *pl = NULL;
    for (int n = 0; n < PLAYERS_PER_CLIENT; ++n)
    {
        if (cl->players[n].in_use && strcmp(cl->players[n].name, name) == 0)
        {
            pl = &cl->players[n];
            break;
        }
    }

    if (pl == NULL)
    {
        warn( "Chat message from player %s ignored (not connected to client).\n",
              name );
        return;
    }

    msg[msg_len] = '\0'; /* hack */
    info("(CHAT) %s: %s", pl->name, msg);

    /* Write to replay file */
    if (fp_replay != NULL)
    {
        fprintf(fp_replay, "CHAT %s: %s\n", pl->name, msg);
    }

    /* Build message packet */
    {
        packet_begin(MRSC_MESG);
        size_t name_len = strlen(pl->name);
        packet_write_byte(name_len);
        packet_write(pl->name, name_len);
        for (size_t n = 0; n < msg_len; ++n)
        {
            packet_write_byte((msg[n] >= 32 && msg[n] <= 126) ? msg[n] : ' ');
        }
        packet_end();
        packet_broadcast(true);
    }
    return;

malformed:
    warn("Malformed CHAT message ignored.\n");
    return;
}

static void restart_game()
{
    g_time_start = time_now();
    g_timestamp = 0;
    g_deadline = -1;

    if (g_num_clients == 0) return;
    if (g_num_players > 0)

#ifdef BMP
    if (g_gameid != 0)
    {
        char path[32];

        /* Dump field to BMP image */
        sprintf(path, "bmp/field-%08x.bmp", g_gameid);
        if (bmp_write(path, &g_field[0][0], FIELD_SIZE, FIELD_SIZE))
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
        pl->score_holes = 0;
    }

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

    g_num_alive = g_num_players;

    g_gameid = ((rand()&255) << 24) |
               ((rand()&255) << 16) |
               ((rand()&255) <<  8) |
               ((rand()&255) <<  0);

    info("Starting game %08x with %d players", g_gameid, g_num_alive);

    memset(g_field, 0, sizeof(g_field));
    memset(g_holes, 0, sizeof(g_holes));
    g_num_holes = 0;

    /* Initialize players */
    for (int n = 0; n < g_num_players; ++n)
    {
        Player *pl = g_players[n];

        /* Reset player state to starting position */
        pl->timestamp       = 0;
        pl->moves_queue_len = 0;
        pl->has_moved       = false;
        pl->dead_since      = -1;
        pl->pos.x           = rand_int(2048, 65536 - 2048);
        pl->pos.y           = rand_int(2048, 65536 - 2048);
        pl->pos.a           = rand_int(0, 65535);
        pl->hole            = 0;
        pl->solid_since     = 0;
        pl->my_holeid       = 0;
        pl->cross_holeid    = 0;
        pl->rng_base        = g_gameid ^ n;
        pl->rng_carry       = 0;
    }

    /* Intialize clients */
    for (int n = 0; n < MAX_CLIENTS; ++n)
    {
        if (g_clients[n].in_use)
        {
            g_clients[n].started = false;
        }
    }

    /* Assign player colors */
    {
        int users[NUM_COLORS] = { };

        for (int c = 0; c < NUM_COLORS; ++c)
        {
            for (int p = 0; p < g_num_players; ++p)
            {
                users[c] += rgb_cmp(&g_players[p]->color, &g_colors[c]) == 0;
            }
        }

        for (int p = 0; p < g_num_players; ++p)
        {
            struct RGB *col = &g_players[p]->color;
            if (rgb_black(col))
            {
                int best_c = 0;
                for (int c = 1; c < NUM_COLORS; ++c)
                {
                    if (users[c] < users[best_c]) best_c = c;
                }
                *col = g_colors[best_c];
                users[best_c] += 1;
            }
        }
    }

#ifdef REPLAY
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
            fprintf( fp_replay, "%d %d %d %d %d %d %d %d %d\n",
                     SERVER_FPS, TURN_RATE, MOVE_RATE, LINE_WIDTH, WARMUP,
                     HOLE_PROBABILITY, HOLE_LENGTH_MIN, HOLE_LENGTH_MAX,
                     HOLE_COOLDOWN );

            for (int n = 0; n < g_num_players; ++n)
            {
                fprintf(fp_replay, "%s\n", g_players[n]->name);
            }
            for (int n = 0; n < g_num_players; ++n)
            {
                fprintf( fp_replay, "%d %d %d %d %d %d\n",
                         (int)g_players[n]->pos.x,
                         (int)g_players[n]->pos.y,
                         (int)g_players[n]->pos.a,
                         (int)g_players[n]->color.r,
                         (int)g_players[n]->color.g,
                         (int)g_players[n]->color.b );
            }
        }
        else
        {
            warn("Couldn't open file \"%s\" for writing", path);
        }
    }
#endif

    /* Build start game packet */
    packet_begin(MRSC_STRT);
    packet_write_byte(SERVER_FPS);
    packet_write_byte(TURN_RATE);
    packet_write_byte(MOVE_RATE);
    packet_write_byte(LINE_WIDTH);
    packet_write_byte(WARMUP);
    packet_write_byte(SCORE_HISTORY);
    packet_write_byte((HOLE_PROBABILITY >> 8)&255);
    packet_write_byte((HOLE_PROBABILITY >> 0)&255);
    packet_write_byte(HOLE_LENGTH_MIN);
    packet_write_byte(HOLE_LENGTH_MAX - HOLE_LENGTH_MIN);
    packet_write_byte(HOLE_COOLDOWN);
    packet_write_byte(MOVE_BACKLOG);
    packet_write_byte(g_num_players);
    packet_write_byte((g_gameid>>24)&255);
    packet_write_byte((g_gameid>>16)&255);
    packet_write_byte((g_gameid>> 8)&255);
    packet_write_byte((g_gameid>> 0)&255);

    for (int i = 0; i < g_num_players; ++i)
    {
        Player *pl = g_players[i];
        packet_write_byte(pl->color.r);
        packet_write_byte(pl->color.g);
        packet_write_byte(pl->color.b);
        int x = (int)pl->pos.x;
        int y = (int)pl->pos.y;
        int a = (int)pl->pos.a;
        packet_write_byte((x>>8)&255);
        packet_write_byte((x>>0)&255);
        packet_write_byte((y>>8)&255);
        packet_write_byte((y>>0)&255);
        packet_write_byte((a>>8)&255);
        packet_write_byte((a>>0)&255);
        size_t name_len = strlen(pl->name);
        packet_write_byte(name_len);
        packet_write(pl->name, name_len);
    }
    packet_end();

    /* Convert positions to more practical format */
    for (int i = 0; i < g_num_players; ++i)
    {
        Player *pl = g_players[i];
        pl->pos.x *= 1.0/65536;
        pl->pos.y *= 1.0/65536;
        pl->pos.a *= 2*M_PI/65536;
    }

    /* Send start packet to all clients */
    packet_broadcast(true);
    send_scores();
}

static void send_scores()
{
    packet_begin(MRSC_SCOR);
    for (int n = 0; n < g_num_players; ++n)
    {
        int tot, cur, avg;

        tot = g_players[n]->score_total;
        cur = g_players[n]->score_history[0];
        avg = g_players[n]->score_moving_sum;

        packet_write_byte(tot >> 8);
        packet_write_byte(tot & 255);
        packet_write_byte(cur >> 8);
        packet_write_byte(cur & 255);
        packet_write_byte(avg >> 8);
        packet_write_byte(avg & 255);
        packet_write_byte(g_players[n]->score_holes);
        packet_write_byte(0);
    }
    packet_end();
    packet_broadcast(true);
}

/* Executes one move for the given player and updates his timestamp: */
static void do_player_move(Player *pl, Move m)
{
    assert(pl->dead_since == -1);

    if ( pl->hole == 0 &&
            pl->timestamp >= WARMUP +  HOLE_COOLDOWN &&
            pl->timestamp - pl->solid_since >= HOLE_COOLDOWN &&
            pl->rng_base%HOLE_PROBABILITY == 0 )
    {
        pl->hole = HOLE_LENGTH_MIN +
                    (pl->rng_base/HOLE_PROBABILITY)
                    % (HOLE_LENGTH_MAX - HOLE_LENGTH_MIN + 1);
        pl->my_holeid = 1 + (g_num_holes++)%255;
    }

    /* Calculate movement */
    int v = (pl->timestamp < WARMUP ? 0 : 1);
    int a = (m == MOVE_TURN_LEFT)  ? +1 :
            (m == MOVE_TURN_RIGHT) ? -1 : 0;

#ifdef REPLAY
    if (fp_replay != NULL)
    {
        /* Write to replay: player, turn, move*/
        fprintf( fp_replay, "MOVE %d %d %d\n",
                    pl->index, a, (pl->hole ? 2 : v) );
    }
#endif

    /* Register movement during warmup */
    if (!v && a) pl->has_moved = true;

    /* Calculate new position */
    Position npos = pl->pos;
    position_update(&npos, (Move)m, v*1e-3*MOVE_RATE, 2.0*M_PI/TURN_RATE);

    if (v > 0)
    {
        int color = pl->hole > 0 ? -1 : pl->index + 1;

        /* Fill and test new line segment */
        if ( field_line_th( &g_field, &pl->pos, &npos,
                            FIELD_SIZE*1e-3*LINE_WIDTH, color, NULL ) != 0 )
        {
            /* Player bumped into something! */
            player_kill(pl);
        }

        /* Detect hole crossing */
        int holeid = field_line_th( &g_holes, &pl->pos, &npos,
            4.0, (pl->hole > 0 ? pl->my_holeid : -1), NULL );
        if (holeid < 256 && holeid != pl->cross_holeid)
        {
            if (pl->cross_holeid != 0)
            {
                pl->score_holes += 1;
                send_scores();
            }
            pl->cross_holeid = holeid;
        }
    }

    pl->pos = npos;

    if (pl->hole > 0)
    {
        --pl->hole;
        pl->solid_since = pl->timestamp + 1;
    }

    /* Kill players that do not move during the warmup period */
    if (pl->timestamp + 1 == WARMUP && !pl->has_moved) player_kill(pl);

    /* Update player timestamp and RNG: */
    ++pl->timestamp;
    unsigned long long rng_next = pl->rng_base*1967773755ull
                                + pl->rng_carry;
    pl->rng_base  = rng_next&0xffffffff;
    pl->rng_carry = rng_next>>32;
}

static void do_frame()
{
    char data[MAX_PLAYERS*(MOVE_BACKLOG + 1)], *ptr = data;

    if (g_num_clients == 0) return;

    if ( (g_num_alive == 0 && g_deadline == -1) ||
         (g_deadline != -1 && g_timestamp >= g_deadline) )
    {
        restart_game();
    }

    if (g_num_players == 0) return;

    /* Process player moves */
    for (int n = 0; n < g_num_players; ++n)
    {
        Player *pl = g_players[n];
        bool just_died = pl->dead_since == pl->timestamp;

        if (pl->dead_since == -1)  /* player alive? */
        {
            int pos;
            for ( pos = 0; pl->timestamp < g_timestamp &&
                           pos < pl->moves_queue_len; ++pos )
            {
                Move m = pl->moves_queue[pos];
                *ptr++ = (char)pl->index;
                *ptr++ = (char)m;
                do_player_move(pl, m);
                if (pl->dead_since >= 0)
                {
                    just_died = true;
                    break;
                }
            }

            if (pos > 0)
            {
                /* Move remaining moves to the front of the queue: */
                pl->moves_queue_len -= pos;
                if (pl->moves_queue_len > 0)
                {
                    memmove( pl->moves_queue, pl->moves_queue + pos,
                            sizeof(*pl->moves_queue)*pl->moves_queue_len );
                }
            }
            else
            if (g_timestamp - g_players[n]->timestamp > MOVE_BACKLOG)
            {
                /* Check if player is out of sync: */
                message("Killed %s: client out-of-sync!", g_players[n]->name);
                player_kill(pl);
                just_died = true;
            }
        }

        if (just_died)
        {
            *ptr++ = (char)pl->index;
            *ptr++ = (char)MOVE_DEAD;
        }

        if (pl->dead_since >= 0)
        {
            pl->timestamp = g_timestamp;  /* implicitely up-to-date */
            pl->moves_queue_len = 0;      /* discard queued moves */
        }
    }

    /* Broadcast moves packet */
    packet_begin(MUSC_MOVE);
    packet_write(data, ptr - data);
    packet_end();
    packet_broadcast(false);
}

/* Processes frames until the server is up to date, and returns the
   number of seconds until the next frame must be processed. */
static double process_frames()
{
    do {
        /* Compute time to next tick */
        double delay = g_time_start + (double)(g_timestamp + 1)/SERVER_FPS -
            time_now();
        if (delay > 0) return delay;
        ++g_timestamp;
        do_frame();
    } while (g_num_players > 0);
    return 1.0;
}

static bool socket_set_blocking(SOCKET fd, bool val)
{
    /* Note that we set FIONBIO (NON-blocking IO!) iff val is false! */
#ifdef WIN32
    unsigned long v = !val;
#else
    int v = !val;
#endif
    return ioctl(fd, FIONBIO, &v) == 0;
}

#if 0
static bool socket_set_delayed(SOCKET fd, bool val)
{
    /* Note that we set the NODELAY option iff val is false! */
#ifdef WIN32
    BOOL v = !val;
#else
    int v = !val;
#endif
    return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v)) == 0;
}
#endif

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
            if ((int)g_clients[n].fd_stream > max_fd)
            {
                max_fd = g_clients[n].fd_stream;
            }
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
            SOCKET fd = accept(g_fd_listen, (struct sockaddr*)&sa, &sa_len);
            if (fd == INVALID_SOCKET)
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
            if (!socket_set_blocking(fd, 0))
            {
                error("could not put TCP socket non-blocking mode");
                close(fd);
            }
            else
            {
#if 0
                if (!socket_set_delayed(fd, 0))
                {
                    warn("could not put TCP socket in undelayed mode");
                }
#endif

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

            buf_len = recvfrom( g_fd_packet, (void*)buf, sizeof(buf), 0,
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
                while (cl->buf_pos >= 2)
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
    srandom(time(NULL));
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
    if (g_fd_listen == INVALID_SOCKET)
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
    if (g_fd_packet == INVALID_SOCKET)
    {
        fatal("could not create UDP socket: socket() failed");
    }
    if (bind(g_fd_packet, (struct sockaddr*)&sa_local, sizeof(sa_local)) != 0)
    {
        fatal("could not create UDP socket: bind() failed");
    }
    if (!socket_set_blocking(g_fd_packet, 0))
    {
        fatal("could not put UDP socket in non-blocking mode");
    }

    /* Main loop */
    return run();
}
