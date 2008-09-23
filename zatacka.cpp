#include "common.h"
#include "Debug.h"
#include "GameView.h"
#include "ScoreView.h"
#include "ClientSocket.h"
#include "Protocol.h"
#include <algorithm>
#include <vector>
#include <string>
#include <math.h>
#include <string.h>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#endif

#define HZ 60

struct Player
{
    Fl_Color    col;
    double      x, y, a;
    bool        dead;
    std::string name;
    int         timestamp;
};

/* Global variables*/
Fl_Window *g_window;    /* main window */
GameView *g_gv;         /* graphical game view */
ScoreView *g_sv;        /* score view */
ClientSocket *g_cs;     /* client connection to the server */
unsigned g_gameid;      /* current game id */
int g_last_timestamp;   /* last timestamp received */
int g_data_rate;        /* moves per second (currently also display FPS) */
int g_turn_rate;        /* turn rate (2pi/g_turn_rate radians per turn) */
int g_move_rate;        /* move rate (g_move_rate*screen_size/1000 per turn) */
int g_warmup;           /* number of turns to wait before moving forward */
int g_move_backlog;     /* number of moves to cache and send/receive */
int g_num_players;      /* number of players in the game == g_players.size() */

std::vector<std::string> g_my_names;   /* my player names */
std::vector<std::string> g_my_moves;   /* my recent moves (each g_move_backlog long) */

std::vector<Player> g_players;  /* all players in the game */


static void plot_player(int n)
{
    g_gv->plot(
        (int)(g_gv->w() * g_players[n].x),
        g_gv->h() - 1 - (int)(g_gv->h() * g_players[n].y),
        g_players[n].col );
}

static void handle_MESG(unsigned char *buf, size_t len)
{
    /* TODO */
    (void)buf;
    (void)len;
    info("MESG received (TODO!)");
}

static void handle_DISC(unsigned char *buf, size_t len)
{
    info("Disconnected by server; reason:");
    fwrite(buf + 1, 1, len - 1, stdout);
    fputc('\n', stdout);
    exit(0);
}

static void handle_STRT(unsigned char *buf, size_t len)
{
    if (len < 11)
    {
        fatal("(STRT) packet too short");
        return;
    }

    g_last_timestamp = -1;
    size_t pos = 1;
    g_data_rate     = buf[pos++];
    g_turn_rate     = buf[pos++];
    g_move_rate     = buf[pos++];
    g_warmup        = buf[pos++];
    g_move_backlog  = buf[pos++];
    g_num_players   = buf[pos++];
    g_gameid = (buf[pos] << 24) | (buf[pos + 1] << 16) | (buf[pos + 2] << 8) | (buf[pos + 3] << 0);
    pos += 4;

    assert(pos == 11);

    info("Restarting game with %d players", g_num_players);
    g_players = std::vector<Player>(g_num_players);

    g_my_moves = std::vector<std::string> (
        g_my_names.size(), std::string((size_t)g_move_backlog, 0) );

    /* FIXME: read outside of buffer here, if the packet is not correctly formatted! */
    for (int n = 0; n < g_num_players; ++n)
    {
        g_players[n].col = fl_rgb_color(buf[pos], buf[pos + 1], buf[pos + 2]);
        pos += 3;
        g_players[n].x = (256*buf[pos] + buf[pos + 1])/65536.0;
        pos += 2;
        g_players[n].y = (256*buf[pos] + buf[pos + 1])/65536.0;
        pos += 2;
        g_players[n].a = (256*buf[pos] + buf[pos + 1])/65536.0*(2*M_PI);
        pos += 2;
        int name_len = buf[pos++];
        g_players[n].name.assign((char*)(buf + pos), name_len);
        pos += name_len;
        info( "Player %d: name=%s x=%.3f y=%.3f a=%.3f",
              n, g_players[n].name.c_str(),
              g_players[n].x, g_players[n].y, g_players[n].a );
        g_players[n].timestamp = 0;

        plot_player(n);
    }
    assert((size_t)pos == len);

    /* Recreate game widget */
    g_window->remove(g_gv);
    delete g_gv;
    g_gv = new GameView(g_num_players, 0, 0, 600, 600);
    g_window->add(g_gv);
    g_gv->redraw();

    for (int n = 0; n < g_num_players; ++n)
    {
        g_gv->setSprite(n, (int)(g_gv->w()*g_players[n].x),
                           (int)(g_gv->h()*g_players[n].y),
                           g_players[n].a, g_players[n].col);
        g_gv->showSprite(n);
    }

    /* TEMP: update scores */
    std::vector<std::string> names;
    std::vector<int> scores;
    std::vector<Fl_Color> colors;
    for (int n = 0; n < g_num_players; ++n)
    {
        names.push_back(g_players[n].name);
        scores.push_back(0);
        colors.push_back(g_players[n].col);
    }
    g_sv->update(names, scores, colors);
}

static void player_turn(int n, int dir)
{
    g_players[n].a += dir*2.0*M_PI/g_turn_rate;
}

static double velocity(int t)
{
    return t < g_warmup ? 0.0 : 1.0;
}

static void player_advance(int n)
{
    double v = velocity(g_players[n].timestamp);
    g_players[n].x += v*1e-3*g_move_rate*cos(g_players[n].a);
    g_players[n].y += v*1e-3*g_move_rate*sin(g_players[n].a);
}

static void forward_to(int timestamp)
{
    static const int keys[4][2] = {
        { FL_Left, FL_Right }, { 'A', 'D'}, { 'M', '.'}, { '4', '6'} };

    assert(timestamp > g_last_timestamp);
    int delay = timestamp - g_last_timestamp;
    for (size_t n = 0; n < g_my_moves.size(); ++n)
    {
        std::string &moves = g_my_moves[n];
        std::rotate(moves.begin(), moves.begin() + delay, moves.end());
        unsigned char m = 1;
        if (Fl::event_key(keys[n][0]) && !Fl::event_key(keys[n][1])) m = 2;
        if (Fl::event_key(keys[n][1]) && !Fl::event_key(keys[n][0])) m = 3;
        for (int pos = g_move_backlog - delay; pos < g_move_backlog; ++pos)
        {
            moves[pos] = m;
        }
    }
    g_last_timestamp = timestamp;
}

static void handle_MOVE(unsigned char *buf, size_t len)
{
    if (len < (size_t)9 + g_num_players)
    {
        error("(MOVE) packet too small");
        return;
    }

    unsigned gameid = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
    if (gameid != g_gameid)
    {
        warn("(MOVE) packet with invalid game id (received %d; expected %d)",
            gameid, g_gameid);
        return;
    }

    int timestamp = (buf[5] << 24) | (buf[6] << 16) | (buf[7] << 8) | buf[8];
    if (timestamp < 0) error("negative timestamp received");
    if (timestamp < g_last_timestamp)
    {
        warn("(MOVE) packet with old timestamp (%d < %d)",
             timestamp, g_last_timestamp);
        return;
    }

    if (timestamp - g_last_timestamp >= g_move_backlog)
    {
        fatal("(MOVE) out of sync! (%d >> %d)\n", timestamp, g_last_timestamp);
        return;
    }

    /* Update alive/dead status */
    size_t pos = 9;
    for (int n = 0; n < g_num_players; ++n)
    {
        g_players[n].dead = !buf[pos++];
    }

    /* Update moves */
    for (int n = 0; n < g_num_players; ++n)
    {
        if (g_players[n].dead) continue;

        for (int t = timestamp - g_move_backlog; t < timestamp; ++t)
        {
            unsigned char m = buf[pos++];
            if (t < g_players[n].timestamp) continue;

            /* If move not yet known; skip. All other moves must be 0 too! */
            if (m == 0) continue;

            switch (m)
            {
            default:
                error("invalid move (%d) interpreted as 1 %d", (int)m, t);
                /* falls through */
            case 1: /* ahead */
                player_advance(n);
                break;

            case 2: /* left */
                player_turn(n, +1);
                player_advance(n);
                break;

            case 3: /* right */
                player_turn(n, -1);
                player_advance(n);
                break;

            case 4: /* dead */
                g_gv->hideSprite(n);
                break;
            }

            plot_player(n);
            g_players[n].timestamp++;
        }

        if (timestamp < g_warmup)
        {
            g_gv->setSprite( n,
                            (int)(g_gv->w() * g_players[n].x),
                            g_gv->h() - 1 - (int)(g_gv->h() * g_players[n].y),
                            g_players[n].a, g_players[n].col );
        }
        else
        {
            g_gv->hideSprite(n);
        }
    }

    forward_to(timestamp + 1);

    {
        char packet[4096];
        size_t pos = 0;
        packet[pos++] = MUCS_MOVE;
        packet[pos++] = (g_gameid >> 24)&255;
        packet[pos++] = (g_gameid >> 16)&255;
        packet[pos++] = (g_gameid >>  8)&255;
        packet[pos++] = (g_gameid >>  0)&255;
        packet[pos++] = (g_last_timestamp >> 24)&255;
        packet[pos++] = (g_last_timestamp >> 16)&255;
        packet[pos++] = (g_last_timestamp >>  8)&255;
        packet[pos++] = (g_last_timestamp >>  0)&255;
        for (size_t n = 0; n < g_my_moves.size(); ++n)
        {
            memcpy(packet + pos, g_my_moves[n].data(), g_move_backlog);
            pos += g_move_backlog;
        }
        g_cs->write(packet, pos, false);
    }
}

static void handle_packet(unsigned char *buf, size_t len)
{
    /*
    info("packet type %d of length %d received", (int)buf[0], len);
    hex_dump(buf, len);
    */
    switch ((int)buf[0])
    {
    case MRSC_MESG: return handle_MESG(buf, len);
    case MRSC_DISC: return handle_DISC(buf, len);
    case MRSC_STRT: return handle_STRT(buf, len);
    case MUSC_MOVE: return handle_MOVE(buf, len);
    default: error("invalid message type");
    }
}

void callback(void *arg)
{
    /* Read network input */
    unsigned char buf[4096];
    ssize_t len;
    while ((len = g_cs->read(buf, sizeof(buf) - 1)) > 0)
    {
        handle_packet(buf, (size_t)len);
    }
    if (len < 0) error("socket read failed");

    Fl::repeat_timeout(1.0/HZ, callback, arg);
}

static void disconnect()
{
    unsigned char msg[1] = { MRCS_QUIT };
    if (g_cs != NULL) g_cs->write(msg, sizeof(msg), true);
}

int main(int argc, char *argv[])
{
    time_reset();
    srand(time(NULL));

    /* Create main window */
    g_window = new Fl_Window(800, 600);
    g_window->label("Zatagain!");
    g_window->color(FL_WHITE);
    g_gv = new GameView(0, 0, 0, 600, 600);
    g_sv = new ScoreView(600, 0, 200, 600);
    g_window->end();
    g_window->show();

    /* Connect to the server */
    g_cs = new ClientSocket(argc > 1 ? argv[1] : "localhost", argc > 2 ? atoi(argv[2]) : 12321);
    if (!g_cs->connected())
    {
        error("Couldn't connect to server.");
        return 1;
    }

    /* Set-up names */
    g_my_names.push_back("Maks");
    while (g_my_names.size() < 4 && 3 + (int)g_my_names.size() < argc)
    {
        g_my_names.push_back(argv[3 + g_my_names.size()]);
        if (g_my_names.back().size() > 20)
        {
            g_my_names.back().erase(
                g_my_names.back().begin() + 20, g_my_names.back().end() );
        }
    }
    if (g_my_names.empty())
    {
        char name[16];
        sprintf(name, "player-%04x", rand()&0xffff);
        g_my_names.push_back(name);
    }

    /* Send hello message */
    {
        char packet[4096];
        size_t pos = 0;

        packet[pos++] = MRCS_HELO;
        packet[pos++] = PROTOCOL_VERSION;
        packet[pos++] = g_my_names.size();
        for (size_t n = 0; n < g_my_names.size(); ++n)
        {
            packet[pos++] = g_my_names[n].size();
            memcpy(packet + pos, g_my_names[n].data(), g_my_names[n].size());
            pos += g_my_names[n].size();
        }
        g_cs->write(packet, pos, true);
    }

    /* FIXME: should wait for window to be visible */
    Fl::add_timeout(0.25, callback, NULL);
    int res = Fl::run();
    disconnect();
    return res;
}
