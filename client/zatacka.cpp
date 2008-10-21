#include "common.h"
#include "ClientSocket.h"
#include "Config.h"
#include "GameModel.h"
#include "MainWindow.h"
#include "KeyboardPlayerController.h"
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

/* Global variables*/
MainWindow *g_window;   /* main window */
ClientSocket *g_cs;     /* client connection to the server */
GameParameters g_gp;    /* game parameters */

int g_last_timestamp;   /* last timestamp received */
int g_local_timestamp;  /* local estimated timestamp */
double g_server_time;   /* estimated server time at timestamp 0 */

std::vector<std::string> g_my_names;    /* my player names */
std::vector<std::string> g_my_moves;    /* my recent moves (each g_gp.move_backlog long) */
std::vector<int>         g_my_players;  /* indices of my players in g_players */
std::vector<int>         g_my_indices;
std::vector<PlayerController*> g_my_controllers;
std::vector<int>         g_my_keys;     /* all keys in use */

std::vector<Player>      g_players;         /* all players in the game */
std::vector<std::string> g_names;           /* .. and their names */

double g_frame_time;     /* time at which frame counter was last reset */
int g_frame_counter;     /* frames counted since last reset */

#ifdef DEBUG
FILE *fp_trace;
#endif

static void player_reset_prediction(int n)
{
    Player &pl = g_players[n];
    pl.ppos.x = g_players[n].pos.x;
    pl.ppos.y = g_players[n].pos.y;
    pl.ppos.a = g_players[n].pos.a;
    pl.pt = g_players[n].timestamp;
}

static bool player_update_prediction(int n, Move move)
{
    Player &pl = g_players[n];
    if (pl.dead) return false;

    double move_rate = pl.pt >= g_gp.warmup ? g_gp.move_rate : 0;
    position_update(&pl.ppos, move, move_rate, g_gp.turn_rate);
    ++pl.pt;
    return true;
}

static void update_sprites()
{
    for (int n = 0; n < g_gp.num_players; ++n)
    {
        Player &pl = g_players[n];
        while (pl.pt < g_local_timestamp)
        {
            if (!player_update_prediction(n, (Move)pl.last_move)) break;
        }
        g_window->gameView()->setSprite(n, pl.ppos.x, pl.ppos.y, pl.ppos.a);
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
        for (int n = 0; n < g_gp.num_players; ++n)
        {
            if (g_players[n].name == name)
            {
                col = (Fl_Color)g_players[n].col;
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
    if (g_gp.gameid != 0)
    {
        char path[32];
        sprintf(path, "bmp/field-%08x.bmp", g_gp.gameid);
        if (g_window->gameView()->writeFieldBitmap(path))
        {
            info("Field dumped to file \"%s\"", path);
        }
        else
        {
            info("Could not write field to file \"%s\"!", path);
        }
    }
#endif

    /* Read game parameters */
    size_t pos = 1;
    g_gp.data_rate         = buf[pos++];
    g_gp.turn_rate         = 2.0*M_PI/buf[pos++];
    g_gp.move_rate         = 1e-3*buf[pos++];
    g_gp.warmup            = buf[pos++];
    g_gp.score_rounds      = buf[pos++];
    g_gp.hole_probability  = buf[pos++]*256;
    g_gp.hole_probability += buf[pos++];
    g_gp.hole_length_min   = buf[pos++];
    g_gp.hole_length_max   = buf[pos++] + g_gp.hole_length_min;
    g_gp.hole_cooldown     = buf[pos++];
    g_gp.move_backlog      = buf[pos++];
    g_gp.num_players       = buf[pos++];
    g_gp.gameid            = buf[pos++] << 24;
    g_gp.gameid           += buf[pos++] << 16;
    g_gp.gameid           += buf[pos++] <<  8;
    g_gp.gameid           += buf[pos++];
    assert(pos == 17);

    info("Restarting game with %d players", g_gp.num_players);
    g_last_timestamp = -1;
    g_local_timestamp = 0;
    g_players = std::vector<Player>(g_gp.num_players);
    g_names   = std::vector<std::string>(g_gp.num_players);
    g_my_moves = std::vector<std::string> (
        g_my_names.size(), std::string((size_t)g_gp.move_backlog, 0) );
    g_my_players = std::vector<int>(g_my_names.size(), -1);
    g_my_indices = std::vector<int>(g_gp.num_players, -1);

#ifdef DEBUG
    {
        char path[32];

        /* Open trace file for new game */
        if (fp_trace != NULL) fclose(fp_trace);
        sprintf(path, "trace/game-%08x.txt", g_gp.gameid);
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

    for (int n = 0; n < g_gp.num_players; ++n)
    {
        memset(&g_players[n], 0, sizeof(g_players[n]));

        if (pos + 10 > len) fatal("invalid STRT packet received");
        g_players[n].col = fl_rgb_color(buf[pos], buf[pos + 1], buf[pos + 2]);
        pos += 3;
        g_players[n].pos.x = (256*buf[pos] + buf[pos + 1])/65536.0;
        pos += 2;
        g_players[n].pos.y = (256*buf[pos] + buf[pos + 1])/65536.0;
        pos += 2;
        g_players[n].pos.a = (256*buf[pos] + buf[pos + 1])/65536.0*(2*M_PI);
        pos += 2;
        int name_len = buf[pos++];
        if (pos + name_len > len) fatal("invalid STRT packet received");
        g_names[n].assign((char*)(buf + pos), name_len);
        g_players[n].name = g_names[n].c_str();
        pos += name_len;
        info( "Player %d: name=%s x=%.3f y=%.3f a=%.3f", n, g_players[n].name,
              g_players[n].pos.x, g_players[n].pos.y, g_players[n].pos.a );
        g_players[n].timestamp = 0;
        g_players[n].rng_base  = n^g_gp.gameid;
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

    /* Update gameid label & gameview */
    g_window->setGameId(g_gp.gameid);
    g_window->resetGameView(g_gp.num_players);

    GameView *gv = g_window->gameView();
    for (int n = 0; n < g_gp.num_players; ++n)
    {
        gv->setSpriteColor(n, (Fl_Color)g_players[n].col);
        gv->setSpriteType(n, Sprite::HIDDEN);
        gv->setSpriteLabel(n, std::string());
        player_reset_prediction(n);
    }
    update_sprites();
    g_window->scoreView()->update(g_players);
    g_window->gameView()->setWarmup(true);

    /* Reinitialize controllers */
    std::vector<PlayerController*>::iterator it;
    for (it = g_my_controllers.begin(); it != g_my_controllers.end(); ++it)
    {
        (*it)->restart(g_gp);
    }
}

static void handle_SCOR(unsigned char *buf, size_t len)
{
    if (len < 1 + 8*(size_t)g_gp.num_players)
    {
        error("(SCOR) packet too short");
    }

    size_t pos = 1;
    for (int n = 0; n < g_gp.num_players; ++n)
    {
        g_players[n].score_tot = 256*buf[pos + 0] + buf[pos + 1];
        g_players[n].score_cur = 256*buf[pos + 2] + buf[pos + 3];
        g_players[n].score_avg = 256*buf[pos + 4] + buf[pos + 5];
        if (g_players[n].score_holes != buf[pos + 6])
        {
            g_players[n].score_holes = buf[pos + 6];
        }
        pos += 8;
    }

    g_window->scoreView()->update(g_players);
}

static void player_move(int n, Move move)
{
    Player &pl = g_players[n];

    /* Change sprite after warmup period ends */
    if (pl.timestamp == g_gp.warmup)
    {
        g_window->gameView()->setSpriteLabel(n, std::string());
        g_window->gameView()->setSpriteType(n, Sprite::DOT);
    }

    /* Display sprite during warmup after first move */
    if ((move == 2 || move == 3) && (pl.timestamp < g_gp.warmup))
    {
        g_window->gameView()->setSpriteType(n, Sprite::ARROW);
        g_window->gameView()->setSpriteLabel(n, g_players[n].name);
    }

    /* Change hole making state according to RNG */
    if ( pl.hole == 0 && pl.timestamp >= g_gp.warmup + g_gp.hole_cooldown &&
         pl.timestamp - pl.solid_since >= g_gp.hole_cooldown &&
         pl.rng_base%g_gp.hole_probability == 0 )
    {
        pl.hole = g_gp.hole_length_min +
                 pl.rng_base/g_gp.hole_probability %
                 (g_gp.hole_length_max - g_gp.hole_length_min + 1);

    }

    switch (move)
    {
    case MOVE_NONE:
        fatal("MOVE_NONE passed to player_move!");
        return;

    default:
        error("invalid move (%d) interpreted as MOVE_FORWARD", (int)move);
        move = MOVE_FORWARD;
        /* falls through */
    case MOVE_FORWARD:
    case MOVE_TURN_LEFT:
    case MOVE_TURN_RIGHT:
        {
            double move_rate = pl.timestamp < g_gp.warmup ? 0 : g_gp.move_rate;
            Position npos = pl.pos;
            position_update(&npos, move, move_rate, g_gp.turn_rate);
            if (move_rate > 0 && pl.hole == 0)
            {
                g_window->gameView()->line(&pl.pos, &npos, n);
            }
            g_players[n].pos = npos;
        };
        break;

    case MOVE_DEAD:
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
    if (pl.hole > 0)
    {
        --pl.hole;
        if (pl.hole == 0) pl.solid_since = pl.timestamp + 1;
    }

    /* Increment player timestamp */
    pl.timestamp++;
}

static void forward_to(int timestamp)
{
    const Field &field = g_window->gameView()->field();

    if (g_local_timestamp >= timestamp) return;

    while (g_local_timestamp < timestamp)
    {
        for (size_t n = 0; n < g_my_moves.size(); ++n)
        {
            int p = g_my_players[n];
            if (p < 0) continue;

            std::string &moves = g_my_moves[n];
            std::rotate(moves.begin(), moves.begin() + 1, moves.end());

            /* Fill in new moves */
            PlayerController *pc = g_my_controllers[n];
            Move m = pc->move(g_local_timestamp, &g_players[0], p, field);
            moves[g_gp.move_backlog - 1] = (unsigned char)m;
            player_update_prediction(p, m);

            /* Hide warmup message if client has joined */
            if ( g_local_timestamp < g_gp.warmup &&
                (m == MOVE_TURN_LEFT || m == MOVE_TURN_RIGHT) )
            {
                g_window->gameView()->setWarmup(false);
            }
        }

        ++g_local_timestamp;
    }

    /* Send new move packet */
    {
        char packet[4096];
        size_t pos = 0;
        packet[pos++] = MUCS_MOVE;
        packet[pos++] = (g_gp.gameid >> 24)&255;
        packet[pos++] = (g_gp.gameid >> 16)&255;
        packet[pos++] = (g_gp.gameid >>  8)&255;
        packet[pos++] = (g_gp.gameid >>  0)&255;
        packet[pos++] = (g_local_timestamp >> 24)&255;
        packet[pos++] = (g_local_timestamp >> 16)&255;
        packet[pos++] = (g_local_timestamp >>  8)&255;
        packet[pos++] = (g_local_timestamp >>  0)&255;
        for (size_t n = 0; n < g_my_moves.size(); ++n)
        {
            memcpy(packet + pos, g_my_moves[n].data(), g_gp.move_backlog);
            pos += g_gp.move_backlog;
        }
        g_cs->write(packet, pos, false);
    }
}

static void handle_MOVE(unsigned char *buf, size_t len)
{
    if (len < (size_t)9 + g_gp.num_players)
    {
        error("(MOVE) packet too small");
        return;
    }

    unsigned gameid = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
    if (gameid != g_gp.gameid)
    {
        warn("(MOVE) packet with invalid game id (received %d; expected %d)",
            gameid, g_gp.gameid);
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

    if (g_last_timestamp < g_gp.warmup && timestamp >= g_gp.warmup)
    {
        g_window->gameView()->setWarmup(false);
    }

    /* Update estimated server time (at start of the round) */
    {
        double t = time_now() - 1.0*timestamp/g_gp.data_rate;
        if (g_last_timestamp == -1 || t < g_server_time) g_server_time = t;
    }

    /* Update moves */
    size_t pos = 9 + g_gp.num_players;
    for (int n = 0; n < g_gp.num_players; ++n)
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
        assert((size_t)g_gp.move_backlog <= sizeof(moves));
        memcpy(moves, buf + pos, g_gp.move_backlog);
        pos += g_gp.move_backlog;

        /* Check if we have missed any moves */
        if (timestamp - g_players[n].timestamp > g_gp.move_backlog)
        {
            error("client out of sync!");
            g_window->gameView()->appendMessage("Client out-of-sync!");
            g_players[n].dead = true;
            continue;
        }

        /* Add missing moves */
        for ( int i = g_gp.move_backlog - (timestamp - g_players[n].timestamp);
              i < g_gp.move_backlog; ++i)
        {
            if (moves[i] == 0) break;  /* further moves not yet known */
            player_move(n, (Move)moves[i]);
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
        int server_timestamp = (int)floor((t - g_server_time)*g_gp.data_rate);
        forward_to(server_timestamp + 1);
        update_sprites();
    }

    /* Calculate FPS */
    g_frame_counter += 1;
    t = time_now();
    if (t > g_frame_time + 1)
    {
        if (g_frame_time > 0)
        {
            g_window->setFPS(g_frame_counter/(t - g_frame_time));
        }
        g_frame_time    = t;
        g_frame_counter = 0;
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
    return find(g_my_keys.begin(), g_my_keys.end(), key) != g_my_keys.end();
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

#ifndef WIN32
#include <dlfcn.h>
static PlayerController *load_bot_posix(const char *name)
{
    char path[64];

    /* Ensure filename is sensible */
    if (strchr(name, '/') || strchr(name, '.') || strlen(name) > 40)
    {
        return NULL;
    }

    sprintf(path, "bots/%s.so", name);
    void *dlh = dlopen(path, RTLD_NOW);
    if (dlh == NULL)
    {
        info("dlopen failed -- %s", dlerror());
        return NULL;
    }

    void *func = dlsym(dlh, "create_bot");
    if (func == NULL)
    {
        info("dlsym failed -- %s", dlerror());
        dlclose(dlh);
        return NULL;
    }

    PlayerController *pc = ((PlayerController*(*)())func)();
    if (pc == NULL) info("%s: create_bot() returned NULL", path);
    return pc;
}
#endif /* ndef WIN32 */

#ifdef WIN32
static PlayerController *load_bot_win32(const char *name)
{
    char path[64];

    /* Ensure filename is sensible */
    if (strchr(name, '/') || strchr(name, '.') || strchr(name, '\\') || strlen(name) > 40)
    {
        return NULL;
    }

    sprintf(path, "bots\\%s.dll", name);
    HMODULE hModule = LoadLibrary(path);
    if (hModule == NULL)
    {
        info("Couldn't load library %s", path);
        return NULL;
    }

    void *func = (void*)GetProcAddress(hModule, "create_bot");
    if (func == NULL)
    {
        info("Couldn't find function create_bot() in %s", path);
        FreeLibrary(hModule);
        return NULL;
    }

    PlayerController *pc = ((PlayerController*(*)())func)();
    if (pc == NULL) info("%s: create_bot() returned NULL", path);
    return pc;
}
#endif /* def WIN32 */

static PlayerController *load_bot(const char *name)
{
#ifdef WIN32
    return load_bot_win32(name);
#else
    return load_bot_posix(name);
#endif
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
    g_window = new MainWindow( cfg.width(), cfg.height(),
                                cfg.fullscreen(), cfg.antialiasing() );
    g_window->show();

    /* Set-up local players */
    for (int n = 0; n < cfg.players(); ++n)
    {
        PlayerController *pc = load_bot(cfg.name(n).c_str());
        if (pc == NULL)
        {
            int key_left  = cfg.key(n, 0);
            int key_right = cfg.key(n, 1);
            pc = new KeyboardPlayerController(key_left, key_right);
            g_my_keys.push_back(key_left);
            g_my_keys.push_back(key_right);
        }
        g_my_controllers.push_back(pc);
        g_my_names.push_back(cfg.name(n));
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
