#include "common.h"
#include "Audio.h"
#include "ClientSocket.h"
#include "Config.h"
#include "GameModel.h"
#include "MainWindow.h"
#include "KeyboardPlayerController.h"
#include "ScoreView.h"
#include <algorithm>
#include <vector>
#include <string>
#include <utility>
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
Config g_config;        /* game config window */
int g_feats;            /* protocol features used */

int g_server_timestamp;   /* last timestamp received */
int g_local_timestamp;    /* local estimated timestamp */
double g_server_time;     /* estimated server time at timestamp 0 */

std::vector<std::string> g_my_names;    /* my player names */
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

#ifdef WITH_AUDIO
Audio *g_audio;
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

static void handle_FEAT(unsigned char *buf, size_t len)
{
    if (len < 6) fatal("(FEAT) packet too short");

    if (buf[1] != 3)
    {
        fatal( "(FEAT) incompatible server protocol (%d; expected 3)",
               (int)buf[1], 3 );
    }

    /* Parse feats */
    g_feats &= buf[5];

    if (g_feats & FEAT_NODELAY)
    {
        if (g_cs->set_nodelay(true))
            info("TCP_NODELAY socket option set.");
        else
            warn("Could not set TCP_NODELAY socket option!");
    }

    /* Set-up local players */
    for (int n = 0; n < g_config.players(); ++n)
    {
        PlayerController *pc = NULL;

        if (g_feats & FEAT_BOTS)
        {
            pc = PlayerController::load_bot(g_config.name(n).c_str());
        }

        if (pc == NULL)
        {
            int key_left  = g_config.key(n, 0);
            int key_right = g_config.key(n, 1);
            pc = new KeyboardPlayerController(key_left, key_right);
            g_my_keys.push_back(key_left);
            g_my_keys.push_back(key_right);
        }
        g_my_controllers.push_back(pc);
        g_my_names.push_back(g_config.name(n));
    }

    /* Send JOIN message */
    {
        char packet[4096];
        size_t pos = 0;

        packet[pos++] = MRCS_JOIN;
        packet[pos++] = g_my_names.size();
        for (size_t n = 0; n < g_my_names.size(); ++n)
        {
            int flags = 0;
            if (!g_my_controllers[n]->human()) flags |= PLFL_BOT;
            packet[pos++] = flags;
            packet[pos++] = g_my_names[n].size();
            memcpy(packet + pos, g_my_names[n].data(), g_my_names[n].size());
            pos += g_my_names[n].size();
        }
        g_cs->write(packet, pos, true);
    }

}

static void handle_QUIT(unsigned char *buf, size_t len)
{
    std::string msg = "Disconnected by server!";
    if (len > 1) msg += "\nReason: " + std::string((char*)buf + 1, len - 1);
    fatal("%s", msg.c_str());
}

static void handle_CHAT(unsigned char *buf, size_t len)
{
    if (len < 2) return;

    size_t name_len = buf[1];
    if (name_len > len - 2)
    {
        error("(CHAT) invalid name length");
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

    /* Send to player controllers */
    for (size_t n = 0; n < g_my_controllers.size(); ++n)
    {
        if (name != g_my_names[n]) g_my_controllers[n]->listen(name, text);
    }
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
    g_gp.line_width        = 1e-3*buf[pos++];
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
    assert(pos == 18);

    info("Restarting game with %d players", g_gp.num_players);
    g_server_timestamp = 0;
    g_local_timestamp = 0;
    g_players = std::vector<Player>(g_gp.num_players);
    g_names   = std::vector<std::string>(g_gp.num_players);
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
    g_window->resetGameView(g_gp.num_players, g_gp.line_width);

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

    /* Acknowledge game start */
    unsigned char msg[1] = { MRCS_STRT };
    g_cs->write(msg, sizeof(msg), true);
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
#ifdef WITH_AUDIO
            if (g_audio) g_audio->play_sample(5 + rand()%3);
#endif /* def WITH_AUDIO */
            g_players[n].score_holes = buf[pos + 6];
        }
        pos += 8;
    }

    g_window->scoreView()->update(g_players);
}

static void send_chat_message(const std::string &name, const std::string &text)
{
    std::string packet;
    packet += (char)MRCS_CHAT;
    packet += (char)(name.size());
    packet += name;
    packet += text;
    g_cs->write(packet.data(), packet.size(), true);
}

static void player_move(int n, Move move)
{
    Player &pl = g_players[n];
    Position npos = pl.pos;
    const double move_rate = pl.timestamp < g_gp.warmup ? 0 : g_gp.move_rate;
    const double turn_rate = g_gp.turn_rate;

    /* Change sprite after warmup period ends */
    if (pl.timestamp == g_gp.warmup)
    {
        g_window->gameView()->setSpriteLabel(n, std::string());
        g_window->gameView()->setSpriteType(n, Sprite::DOT);
    }

    /* Display sprite during warmup after first move */
    if ( (move == MOVE_TURN_LEFT || move == MOVE_TURN_RIGHT) &&
         (pl.timestamp < g_gp.warmup) )
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
    default:
        error("invalid move (%d) interpreted as MOVE_FORWARD", (int)move);
        move = MOVE_FORWARD;
        /* falls through */
    case MOVE_FORWARD:
    case MOVE_TURN_LEFT:
    case MOVE_TURN_RIGHT:
        {
            position_update(&npos, move, move_rate, turn_rate);
            if (move_rate > 0 && pl.hole == 0)
            {
                g_window->gameView()->drawLine(&pl.pos, &npos, n);
            }
        } break;

    case MOVE_DEAD:
        if (!pl.dead)
        {
            pl.dead = true;
            g_window->gameView()->setSpriteType(n, Sprite::HIDDEN);
#ifdef WITH_AUDIO
            if (g_audio)
            {
                if (pl.timestamp > g_gp.warmup) g_audio->play_sample(1 + rand()%2);
            }
#endif /* def WITH_AUDIO */
        }
        break;
    }

    /* Notify controllers of move (useful for bots): */
    for (size_t m = 0; m < g_my_controllers.size(); ++m)
    {
        g_my_controllers[m]->watch_player( n, pl.timestamp, move,
            move_rate, turn_rate, pl.hole == 0, pl.pos, npos );
    }

    /* Update common player state: */
    pl.last_move = move;
    g_players[n].pos = npos;

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
    bool have_players = false;

    for (size_t n = 0; n < g_my_players.size(); ++n)
    {
        int p = g_my_players[n];
        if (p >= 0 && !g_players[p].dead)
        {
            have_players = true;
            break;
        }
    }

    if (!have_players)
    {
        g_local_timestamp = timestamp;
        return;
    }

    while (g_local_timestamp < timestamp)
    {
        /* Create new move packet: */
        char packet[1 + g_my_players.size()];
        size_t packet_len = 0;
        packet[packet_len++] = MRCS_MOVE;

        for (size_t n = 0; n < g_my_players.size(); ++n)
        {
            int p = g_my_players[n];
            if (p < 0) continue;

            /* Fill in new moves */
            Move m = MOVE_FORWARD;
            if (!g_players[p].dead)
            {
                /* Use player controller to get next move */
                PlayerController *pc = g_my_controllers[n];
                m = pc->move(g_local_timestamp, &g_players[0], p, field);
            }
            player_update_prediction(p, m);

            /* Hide warmup message if client has joined */
            if ( g_local_timestamp < g_gp.warmup &&
                 (m == MOVE_TURN_LEFT || m == MOVE_TURN_RIGHT) )
            {
                g_window->gameView()->setWarmup(false);
            }

            packet[packet_len++] = (char)m;
        }

        /* Process chat messages */
        std::string text;
        for (size_t n = 0; n < g_my_controllers.size(); ++n)
        {
            while (g_my_controllers[n]->retrieve_message(&text))
            {
                send_chat_message(g_my_names[n], text);
            }
        }

        /* Send new move packet */
        g_cs->write(packet, packet_len, false);

        ++g_local_timestamp;
    }
}

static void handle_FFWD(unsigned char *buf, size_t len)
{
    if (len < 5 + (size_t)g_gp.num_players)
    {
        error( "(FFWD) invalid packet length (%d) for %d players",
               len, g_gp.num_players );
        return;
    }

    /* Decode server timestamp */
    int timestamp = (buf[1] << 24) | (buf[2] << 16) | (buf[3] << 8) | buf[4];
    if (timestamp < 0)
    {
        error("(FFWD) invalid timestamp (%d)", timestamp);
        return;
    }
    g_local_timestamp = g_server_timestamp = timestamp;
    g_server_time = time_now() - 1.0*g_server_timestamp/g_gp.data_rate;

    /* Decode compressed player moves */
    size_t pos = 5;
    for (int n = 0; n < g_gp.num_players; ++n)
    {
        size_t end = pos;
        while (end < len && buf[end] != 0) ++end;
        if (end >= len)
        {
            error("(FFWD) unterminated move data");
            break;
        }
        for ( ; pos < end; ++pos)
        {
            int m = buf[pos] >> 6, r = buf[pos]&0x3f;
            if (m < MOVE_FORWARD || m > MOVE_DEAD || r < 1)
            {
                error("(FFWD) invalid compressed move (%d)", (int)buf[pos]);
            }
            else
            {
                do player_move(n, (Move)m); while (--r > 0);
            }
        }
        player_reset_prediction(n);
        pos = end + 1;
    }
    update_sprites();
}

static void handle_MOVE(unsigned char *buf, size_t len)
{
    if (len%2 != 1)
    {
        error("(MOVE) invalid packet length (%d bytes)", len);
        return;
    }

    ++g_server_timestamp;

    if (g_server_timestamp == g_gp.warmup)
    {
        g_window->gameView()->setWarmup(false);
    }

    /* Update estimated server time */
    {
        double t = time_now() - 1.0*g_server_timestamp/g_gp.data_rate;
        if (g_server_timestamp == 1 || t < g_server_time) g_server_time = t;
    }

    /* Update moves */
    for (size_t pos = 1; pos < len; pos += 2)
    {
        size_t n = buf[pos];
        int m = buf[pos + 1];
        if (n >= g_players.size())
        {
            error("(MOVE) invalid player index (%ld)", (long)n);
        }
        else
        if (m < MOVE_FORWARD && m > MOVE_DEAD)
        {
            error("(MOVE) invalid move number (%d)", (int)m);
        }
        else
        {
            player_move(n, (Move)m);
            if (g_my_indices[n] == -1) player_reset_prediction(n);
        }
    }

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
    case MRSC_FEAT: return handle_FEAT(buf, len);
    case MRSC_QUIT: return handle_QUIT(buf, len);
    case MRSC_CHAT: return handle_CHAT(buf, len);
    case MRSC_STRT: return handle_STRT(buf, len);
    case MRSC_MOVE: return handle_MOVE(buf, len);
    case MRSC_SCOR: return handle_SCOR(buf, len);
    case MRSC_FFWD: return handle_FFWD(buf, len);
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
    if (len < 0) error("ClientSocket::read() failed!");

    /* Do timed events */
    g_window->gameView()->updateTime(time_now());

    if (g_server_timestamp > 0)
    {
        /* Estimate server timestamp */
        double t = time_now();
        int server_timestamp = (int)floor((t - g_server_time)*g_gp.data_rate);
        forward_to(server_timestamp + 1);
        update_sprites();
    }

    /* Calculate FPS */
    g_frame_counter += 1;
    double t = time_now();
    if (t > g_frame_time + 1)
    {
        if (g_frame_time > 0)
        {
            g_window->setFPS(g_frame_counter/(t - g_frame_time));
        }
        g_frame_time    = t;
        g_frame_counter = 0;
        g_window->setTrafficStats(
            g_cs->get_bytes_received(), g_cs->get_packets_received(),
            g_cs->get_bytes_sent(), g_cs->get_packets_sent() );
        g_cs->clear_stats();
    }

#ifdef WITH_AUDIO
    if (g_audio) g_audio->update();
#endif

    Fl::repeat_timeout(1.0/CLIENT_FPS, callback, arg);
}

static void disconnect()
{
    unsigned char msg[1] = { MRCS_QUIT };
    if (g_cs != NULL) g_cs->write(msg, sizeof(msg), true);
}

/* Send a chat message, assuming it is sent by a human player.
   (This function is called from MainWindow) */
void send_chat_message(const std::string &text)
{
    for (size_t n = 0; n < g_my_controllers.size(); ++n)
    {
        if (g_my_controllers[n]->human())
        {
            send_chat_message(g_my_names[n], text);
            break;
        }
    }
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

    error("%s", buf);
    fl_alert("%s", buf);
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
    g_config.load_settings(config_path.c_str());
    do {
        if (!g_config.show_window()) return 0;
        g_config.save_settings(config_path.c_str());

        /* Initialize requested protocol features:*/
        g_feats = FEAT_BOTS | FEAT_NODELAY;

        /* Try to connect to the server */
        g_cs = new ClientSocket( g_config.hostname().c_str(), g_config.port(),
                                 g_config.reliable_only() );
        if (!g_cs->connected())
        {
            fl_alert( "The network connection could not be established!\n"
                      "Please check the specified host name (%s) and port (%d)",
                      g_config.hostname().c_str(), g_config.port() );
        }

    } while (!g_cs->connected());

    /* Send FEAT packet */
    char feat_packet[6] = { MRCS_FEAT, 3, 0, 0, 0, (char)g_feats };
    g_cs->write(feat_packet, 6, true);

#ifdef WITH_AUDIO
    /* Initialize audio */
    g_audio = Audio::instance();
    if (g_audio) g_audio->play_music("pizzaworm.mod");
#endif

    /* Create main window */
    g_window = new MainWindow(800, 600, g_config.fullscreen(), g_config.antialiasing());
    g_window->show();

    Fl::add_timeout(0, callback, NULL);
    Fl::run();
    disconnect();
    return 0;
}
