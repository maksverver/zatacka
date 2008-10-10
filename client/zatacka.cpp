#include "common.h"
#include "ClientSocket.h"
#include "Config.h"
#include "GameModel.h"
#include "MainWindow.h"
#include "ScoreView.h"
#include <algorithm>
#include <vector>
#include <string>
#include <math.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#endif

#ifdef WIN32
#include <shlobj.h>
#endif

#define CLIENT_FPS 100

class KeyBindings
{
public:
    KeyBindings(int a, int b) { keys[0] = a; keys[1] = b; };
    int operator[] (int i) const { return i >= 0 && i < 2 ? keys[i] : 0; };
    int left()  { return keys[0]; }
    int right() { return keys[1]; }
    bool contains(int i) { return keys[0] == i || keys[1] == i; }
private:
    int keys[2];
};

/* Global variables*/
MainWindow *g_window;    /* main window */
ClientSocket *g_cs;     /* client connection to the server */

unsigned g_gameid;      /* current game id */
int g_last_timestamp;   /* last timestamp received */
int g_local_timestamp;  /* local estimated timestamp */
double g_server_time;   /* estimated server time at timestamp 0 */
int g_data_rate;        /* moves per second */
int g_turn_rate;        /* turn rate (2pi/g_turn_rate radians per turn) */
int g_move_rate;        /* move rate (g_move_rate*screen_size/1000 per turn) */
int g_warmup;           /* number of turns to wait before moving forward */
int g_score_rounds;     /* number of rounds for the moving average score */
int g_hole_probability; /* probability of a hole (1/g_hole_probability) */
int g_hole_length_min;  /* minimum length of a hole (in turns) */
int g_hole_length_max;  /* maximum length of a hole (in turns) */
int g_hole_cooldown;    /* minimum number of turns between holes */
int g_move_backlog;     /* number of moves to cache and send/receive */
int g_num_players;      /* number of players in the game == g_players.size() */

std::vector<std::string> g_my_names;    /* my player names */
std::vector<KeyBindings> g_my_keys;     /* my player keys */
std::vector<std::string> g_my_moves;    /* my recent moves (each g_move_backlog long) */
std::vector<int>         g_my_players;  /* indices of my players in g_players */
std::vector<int>         g_my_indices;

std::vector<Player> g_players;  /* all players in the game */

#ifdef DEBUG
unsigned char g_field[1000][1000];
FILE *fp_trace;
#endif

static void player_reset_prediction(int n)
{
    Player &pl = g_players[n];
    pl.px = g_players[n].x;
    pl.py = g_players[n].y;
    pl.pa = g_players[n].a;
    pl.pt = g_players[n].timestamp;
}

static bool player_update_prediction(int n, int move)
{
    Player &pl = g_players[n];
    if (pl.dead) return false;

    int turn_dir = (move == 2) ? +1 : (move == 3) ? -1 : 0;
    pl.pa += turn_dir*2.0*M_PI/g_turn_rate;
    if (pl.pt >= g_warmup)
    {
        pl.px += 1e-3*g_move_rate*cos(pl.pa);
        pl.py += 1e-3*g_move_rate*sin(pl.pa);
    }
    ++pl.pt;
    return true;
}

static void update_sprites()
{
    for (int n = 0; n < g_num_players; ++n)
    {
        Player &pl = g_players[n];
        while (pl.pt < g_local_timestamp)
        {
            if (!player_update_prediction(n, pl.last_move)) break;
        }
        g_window->gameView()->setSprite(n, pl.px, pl.py, pl.pa);
    }
}

static void handle_MESG(unsigned char *buf, size_t len)
{
    if (len < 2) return;

    size_t name_len = buf[1];
    if (name_len > len - 2)
    {
        error("(MESG) invalid name length");
        return;
    }

    std::string name((char*)buf + 2, name_len);
    std::string text((char*)buf + 2 + name_len, len - 2 - name_len);

    if (name.empty())
    {
        /* Show server message */
        g_window->gameView()->appendMessage("(server) " + text);
    }
    else
    {
        /* Show player chat message */
        Fl_Color col = fl_gray_ramp(FL_NUM_GRAY/2);
        for (int n = 0; n < g_num_players; ++n)
        {
            if (g_players[n].name == name)
            {
                col = g_players[n].col;
                break;
            }
        }
        g_window->gameView()->appendMessage(name + ": " + text, col);
    }
}

static void handle_DISC(unsigned char *buf, size_t len)
{
    std::string msg = "Disconnected by server!";
    if (len > 1) msg += "\nReason: " + std::string((char*)buf + 1, len - 1);
    fatal("%s", msg.c_str());
}

static void handle_STRT(unsigned char *buf, size_t len)
{
    if (len < 16)
    {
        fatal("(STRT) packet too short");
        return;
    }

#ifdef DEBUG
    if (g_gameid != 0)
    {
        char path[32];
        sprintf(path, "bmp/field-%08x.bmp", g_gameid);
        if (bmp_write(path, &g_field[0][0], 1000, 1000))
        {
            info("Field dumped to file \"%s\"", path);
        }
    }
#endif

    /* Read game parameters */
    size_t pos = 1;
    g_data_rate         = buf[pos++];
    g_turn_rate         = buf[pos++];
    g_move_rate         = buf[pos++];
    g_warmup            = buf[pos++];
    g_score_rounds      = buf[pos++];
    g_hole_probability  = buf[pos++]*256;
    g_hole_probability += buf[pos++];
    g_hole_length_min   = buf[pos++];
    g_hole_length_max   = buf[pos++] + g_hole_length_min;
    g_hole_cooldown     = buf[pos++];
    g_move_backlog      = buf[pos++];
    g_num_players       = buf[pos++];
    g_gameid            = buf[pos++] << 24;
    g_gameid           += buf[pos++] << 16;
    g_gameid           += buf[pos++] <<  8;
    g_gameid           += buf[pos++];
    assert(pos == 17);

    info("Restarting game with %d players", g_num_players);
    g_last_timestamp = -1;
    g_local_timestamp = 0;
    g_players = std::vector<Player>(g_num_players);
    g_my_moves = std::vector<std::string> (
        g_my_names.size(), std::string((size_t)g_move_backlog, 0) );
    g_my_players = std::vector<int>(g_my_names.size(), -1);
    g_my_indices = std::vector<int>(g_num_players, -1);

#ifdef DEBUG
    {
        char path[32];

        /* Clear field for new game */
        memset(g_field, 0, sizeof(g_field));

        /* Open trace file for new game */
        if (fp_trace != NULL) fclose(fp_trace);
        sprintf(path, "trace/game-%08x.txt", g_gameid);
        fp_trace = fopen(path, "wt");
        if (fp_trace != NULL)
        {
            info("Opened trace file \"%s\"", path);
        }
        else
        {
            warn("Couldn't open file \"%s\" for writing", path);
        }
    }
#endif

    for (int n = 0; n < g_num_players; ++n)
    {
        if (pos + 10 > len) fatal("invalid STRT packet received");
        g_players[n].col = fl_rgb_color(buf[pos], buf[pos + 1], buf[pos + 2]);
        pos += 3;
        g_players[n].x = (256*buf[pos] + buf[pos + 1])/65536.0;
        pos += 2;
        g_players[n].y = (256*buf[pos] + buf[pos + 1])/65536.0;
        pos += 2;
        g_players[n].a = (256*buf[pos] + buf[pos + 1])/65536.0*(2*M_PI);
        pos += 2;
        int name_len = buf[pos++];
        if (pos + name_len > len) fatal("invalid STRT packet received");
        g_players[n].name.assign((char*)(buf + pos), name_len);
        pos += name_len;
        info( "Player %d: name=%s x=%.3f y=%.3f a=%.3f",
              n, g_players[n].name.c_str(),
              g_players[n].x, g_players[n].y, g_players[n].a );
        g_players[n].timestamp = 0;
        g_players[n].rng_base  = n^g_gameid;
        g_players[n].rng_carry = 0;

        for (int m = 0; m < (int)g_my_names.size(); ++m)
        {
            if (g_my_names[m] == g_players[n].name)
            {
                g_my_indices[n] = m;
                g_my_players[m] = n;
                break;
            }
        }
    }
    if (pos != len) warn("%d extra bytes in STRT packet", len - pos);

    /* Update gameid label & gameview*/
    g_window->setGameId(g_gameid);
    g_window->resetGameView(g_num_players);

    GameView *gv = g_window->gameView();
    for (int n = 0; n < g_num_players; ++n)
    {
        gv->setSpriteColor(n, g_players[n].col);
        gv->setSpriteType(n, Sprite::ARROW);
        gv->setSpriteLabel(n, g_players[n].name);
        player_reset_prediction(n);
    }
    update_sprites();
    g_window->scoreView()->update(g_players);
}

static void handle_SCOR(unsigned char *buf, size_t len)
{
    if (len < 1 + 8*(size_t)g_num_players)
    {
        error("(SCOR) packet too short");
    }

    size_t pos = 1;
    for (int n = 0; n < g_num_players; ++n)
    {
        g_players[n].score_tot = 256*buf[pos + 0] + buf[pos + 1];
        g_players[n].score_cur = 256*buf[pos + 2] + buf[pos + 3];
        g_players[n].score_avg = 256*buf[pos + 4] + buf[pos + 5];
        pos += 8;
    }

    g_window->scoreView()->update(g_players);
}

static void player_advance(int n, int turn_dir)
{
    Player &pl = g_players[n];

    /* First turn */
    pl.a += turn_dir*2.0*M_PI/g_turn_rate;

    /* Then move ahead */
    if (pl.timestamp >= g_warmup)
    {
        double nx = pl.x + 1e-3*g_move_rate*cos(pl.a);
        double ny = pl.y + 1e-3*g_move_rate*sin(pl.a);
        if (pl.hole == -1)
        {
            g_window->gameView()->line(pl.x, pl.y, nx, ny, pl.col);
        }
        pl.x = nx;
        pl.y = ny;
    }

#ifdef DEBUG
    if (pl.hole <= 0) field_plot(&g_field, pl.x, pl.y, n + 1);
#endif
}

static void player_move(int n, int move)
{
    Player &pl = g_players[n];

    /* Change sprite after warmup period ends */
    if (pl.timestamp == g_warmup)
    {
        g_window->gameView()->setSpriteLabel(n, std::string());
        g_window->gameView()->setSpriteType(n, Sprite::DOT);
    }

    /* Change hole making state according to RNG */
    if ( pl.hole == -1 && pl.timestamp >= g_warmup + g_hole_cooldown &&
         pl.timestamp - pl.solid_since >= g_hole_cooldown &&
         pl.rng_base%g_hole_probability == 0 )
    {
        pl.hole = g_hole_length_min +
                 pl.rng_base/g_hole_probability %
                 (g_hole_length_max - g_hole_length_min + 1);
    }

    switch (move)
    {
    case 0: fatal("0 move passed to player_move!"); return;
    default:
        error("invalid move (%d) interpreted as 1", (int)move);
        move = 1;
        /* falls through */
    case 1: player_advance(n,  0); break;  /* move ahead */
    case 2: player_advance(n, +1); break;  /* turn left */
    case 3: player_advance(n, -1); break;  /* turn right */
    case 4:                                     /* dead */
        if (!pl.dead)
        {
            pl.dead = true;
            g_window->gameView()->setSpriteType(n, Sprite::HIDDEN);
        }
        break;
    }
    pl.last_move = move;

#ifdef DEBUG
    if (!pl.dead && fp_trace != NULL)
    {
        fprintf(fp_trace, "%07d %3d %3d\n", pl.timestamp, n, move);
    }
#endif

    /* Update RNG */
    unsigned long long rng_next = pl.rng_base*1967773755ull + pl.rng_carry;
    pl.rng_base  = rng_next&0xffffffff;
    pl.rng_carry = rng_next>>32;

    /* Count down hole */
    if (pl.hole == 0) pl.solid_since = pl.timestamp;
    if (pl.hole >= 0) --pl.hole;

    /* Increment player timestamp */
    pl.timestamp++;
}

static void forward_to(int timestamp)
{
    if (timestamp <= g_local_timestamp) return;

    int delay = timestamp - g_local_timestamp;
    for (size_t n = 0; n < g_my_moves.size(); ++n)
    {
        int p = g_my_players[n];
        if (p < 0) continue;

        std::string &moves = g_my_moves[n];
        std::rotate(moves.begin(), moves.begin() + delay, moves.end());

        unsigned char m;
        if (Fl::event_key(g_my_keys[n][0]) && !Fl::event_key(g_my_keys[n][1]))
        {
            m = 2;
        }
        else
        if (Fl::event_key(g_my_keys[n][1]) && !Fl::event_key(g_my_keys[n][0]))
        {
            m = 3;
        }
        else
        {
            m = 1;
        }

        /* Fill in new moves */
        for (int pos = g_move_backlog - delay; pos < g_move_backlog; ++pos)
        {
            moves[pos] = m;
            player_update_prediction(p, m);
        }
    }
    g_local_timestamp = timestamp;

    /* Send new move packet */
    {
        char packet[4096];
        size_t pos = 0;
        packet[pos++] = MUCS_MOVE;
        packet[pos++] = (g_gameid >> 24)&255;
        packet[pos++] = (g_gameid >> 16)&255;
        packet[pos++] = (g_gameid >>  8)&255;
        packet[pos++] = (g_gameid >>  0)&255;
        packet[pos++] = (g_local_timestamp >> 24)&255;
        packet[pos++] = (g_local_timestamp >> 16)&255;
        packet[pos++] = (g_local_timestamp >>  8)&255;
        packet[pos++] = (g_local_timestamp >>  0)&255;
        for (size_t n = 0; n < g_my_moves.size(); ++n)
        {
            memcpy(packet + pos, g_my_moves[n].data(), g_move_backlog);
            pos += g_move_backlog;
        }
        g_cs->write(packet, pos, false);
    }
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

    /* Update estimated server time (at start of the round) */
    {
        double t = time_now() - 1.0*timestamp/g_data_rate;
        if (g_last_timestamp == -1 || t < g_server_time) g_server_time = t;
    }

    /* Update moves */
    size_t pos = 9 + g_num_players;
    for (int n = 0; n < g_num_players; ++n)
    {
        if (!buf[9 + n])
        {
            /* Server signaled this player died, and has not included
               its moves in the packet. */
            g_players[n].dead = true;
            continue;
        }

        /* Copying isn't strictly necessary here, but makes correct processing
          a lot easier! Don't change unless you know what you are doing! */
        char moves[256];
        assert((size_t)g_move_backlog <= sizeof(moves));
        memcpy(moves, buf + pos, g_move_backlog);
        pos += g_move_backlog;

        /* Check if we have missed any moves */
        if (timestamp - g_players[n].timestamp > g_move_backlog)
        {
            error("client out of sync!");
            g_window->gameView()->appendMessage("Client out-of-sync!");
            g_players[n].dead = true;
            continue;
        }

        /* Add missing moves */
        for ( int i = g_move_backlog - (timestamp - g_players[n].timestamp);
              i < g_move_backlog; ++i)
        {
            if (moves[i] == 0) break;  /* further moves not yet known */
            player_move(n, moves[i]);
        }

        if (g_my_indices[n] == -1) player_reset_prediction(n);
    }

    g_last_timestamp = timestamp;

    update_sprites();
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
    case MRSC_SCOR: return handle_SCOR(buf, len);
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

    /* Do timed events */
    double t = time_now();
    g_window->gameView()->updateTime(t);
    if (g_last_timestamp >= 0)
    {
        /* Estimate server timestamp */
        int server_timestamp = (int)floor((t - g_server_time)*g_data_rate);
        forward_to(server_timestamp + 1);
        update_sprites();
    }

    Fl::repeat_timeout(1.0/CLIENT_FPS, callback, arg);
}

static void disconnect()
{
    unsigned char msg[1] = { MRCS_QUIT };
    if (g_cs != NULL) g_cs->write(msg, sizeof(msg), true);
}

void send_chat_message(const std::string &msg)
{
    std::string packet = (char)MRCS_CHAT + msg;
    g_cs->write(packet.data(), packet.size(), true);
}

bool is_control_key(int key)
{
    for (int n = 0; n < g_num_players; ++n)
    {
        if (g_my_keys[n].contains(key)) return true;
    }
    return false;
}

void gui_fatal(const char *fmt, ...)
{
    char buf[1024];
    size_t pos;

    strcpy(buf, "A fatal error occured: ");
    pos = strlen(buf);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf + pos, sizeof(buf) - pos, fmt, ap);
    va_end(ap);

    info(buf);
    fl_alert(buf);
    exit(1);
}

/* Returns the configuration file path.

   On Windows:  %USERPROFILE%\Application Data\zatacka.cfg
   On UNIX:     $HOME/.zatackrc  (if $HOME is set)
   Fallback:    zatacka.cfg
*/
std::string get_config_path()
{
#ifdef WIN32
    TCHAR dir[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, dir)))
    {
        return (std::string)dir + "\\zatacka.cfg";
    }
#else
    /* Use HOME directory, if possible */
    char *home = getenv("HOME");
    if (home != NULL) return (std::string)home + "/.zatackarc";
#endif

    return "zatacka.cfg";
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* Initialization */
    time_reset();
    srand(time(NULL));

    Fl::visual(FL_DOUBLE|FL_INDEX);

    /* Configuration */
    std::string config_path = get_config_path();
    Config cfg;
    cfg.load_settings(config_path.c_str());
    do {
        if (!cfg.show_window()) return 0;
        cfg.save_settings(config_path.c_str());

        /* Try to connect to the server */
        g_cs = new ClientSocket(cfg.hostname().c_str(), cfg.port());
        if (!g_cs->connected())
        {
            fl_alert( "The network connection could not be established!\n"
                      "Please check the specified host name (%s) and port (%d)",
                      cfg.hostname().c_str(), cfg.port() );
        }
    } while (!g_cs->connected());

    /* Create window */
    g_window = new MainWindow(cfg.width(), cfg.height(), cfg.fullscreen());
    g_window->show();

    /* Set-up names */
    for (int n = 0; n < cfg.players(); ++n)
    {
        g_my_names.push_back(cfg.name(n));
        g_my_keys.push_back(KeyBindings(cfg.key(n, 0), cfg.key(n, 1)));
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

    Fl::add_timeout(0, callback, NULL);
    Fl::run();
    disconnect();
    return 0;
}
