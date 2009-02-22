#include "Config.h"
#include <fstream>

/* Default key assignment */
static const int default_keys[4][2] = {
    { 86, 88 },   /* Left, Right */
    {  0,  3 },   /* A, D */
    { 12, 45 },   /* M, / */
    { 29, 31 } }; /* 4, 6 */

Config::Config()
{
    /* Set some default values */
    m_fullscreen = false;
    m_antialiasing = true;

    m_hostname = "localhost";
    m_port = 12321;
    m_reliable_only = false;

    m_num_players = 1;
    m_player_index[0] = 0;
    {
        static char buf[16];
        sprintf(buf, "Player-%04x", rand()&0xffff);
        m_names[0].assign(buf);
    }

    for (int n = 0; n < 4; ++n)
    {
        m_keys[n][0] = default_keys[n][0];
        m_keys[n][1] = default_keys[n][1];
    }
}

Config::~Config()
{
}

bool Config::copy_settings()
{
    /* Copy settings */

    /* Display settings */
    m_fullscreen    = w_fullscreen->value();
    m_antialiasing  = w_antialiasing->value();

    /* Network config */
    m_hostname = w_hostname->value();
    m_port = atoi(w_port->value());
    m_reliable_only = w_reliable_only->value();

    /* Players */
    m_num_players = 0;
    for (int n = 0; n < 4; ++n)
    {
        m_names[n] = w_names[n]->value();
        if (!w_players[n]->value() || m_names[n].empty()) continue;
        m_player_index[m_num_players++] = n;
    }

    return true;
}

void start_cb(Fl_Widget *w, void *arg)
{
    (void)w;

    Config *cfg = (Config*)arg;
    if (cfg->copy_settings())
    {
        /* Close window succesfully */
        cfg->start = true;
        cfg->win->hide();
    }
}

void player_check_button_cb(Fl_Widget *w, void *arg)
{
    Config *cfg = (Config*)arg;
    Fl_Check_Button *b = (Fl_Check_Button*)w;

    /* Find player number */
    int n = 0;
    while (n < 4 && cfg->w_players[n] != b) ++n;
    assert(n < 4);

    if (b->value() == 0)
    {
        cfg->w_names[n]->deactivate();
        cfg->w_keys[n][0]->deactivate();
        cfg->w_keys[n][1]->deactivate();
    }
    if (b->value() == 1)
    {
        cfg->w_names[n]->activate();
        cfg->w_keys[n][0]->activate();
        cfg->w_keys[n][1]->activate();
    }
}

void key_button_cb(Fl_Widget *w, void *arg)
{
    Fl_Button *w_button = (Fl_Button*)w;
    Config *cfg = (Config*)arg;

    /* Find the player and key index to be changed */
    int p, i;
    for (p = 0; p < 4; ++p)
    {
        for (i = 0; i < 2; ++i)
        {
            if (cfg->w_keys[p][i] == w_button) goto found;
        }
    }
    assert(0);

found:
    /* Display key binding window */
    KeyWindow *kw = new KeyWindow(300, 60, "Key Binding");
    new Fl_Box(0, 0, 300, 60, "Please press a key or mouse button...");
    kw->end();
    kw->show();
    while (kw->visible()) Fl::wait();

    /* Get new key */
    int key_index = kw->key();
    int key = key_codes[key_index];
    if (key == 0) return;   /* canceled */

    /* If the key is already in use, swap it */
    for (int q = 0; q < 4; ++q)
    {
        for (int j = 0; j < 2; ++j)
        {
            if ((q != p || i != j) && cfg->m_keys[q][j] == key_index)
            {
                cfg->m_keys[q][j] = cfg->m_keys[p][i];
                cfg->w_keys[q][j]->label(cfg->w_keys[p][i]->label());
            }
        }
    }


    /* Now assign new key and label */
    cfg->m_keys[p][i] = key_index;
    w_button->label(key_labels[key_index]);
}

bool Config::show_window()
{
    start = false;

    win = new Fl_Window(300, 490);
    win->label("Configuration");
    win->set_modal();

    Fl_Group *display = new Fl_Group(10, 30, 280, 60, "Display");
    display->box(FL_DOWN_FRAME);
    w_antialiasing = new Fl_Check_Button(110, 40, 170, 20, "&Anti-aliasing");
    w_antialiasing->value(m_antialiasing);
    w_fullscreen = new Fl_Check_Button(110, 60, 170, 20, "&Fullscreen");
    w_fullscreen->value(m_fullscreen);
    display->end();

    Fl_Group *network = new Fl_Group(10, 120, 280, 90, "Network");
    network->box(FL_DOWN_FRAME);
    w_hostname = new Fl_Input(110, 140, 160, 20, "&Hostname: ");
    w_hostname->value(m_hostname.c_str());
    w_port = new Fl_Input(110, 160, 160, 20, "&Port: ");
    char port_buf[32];
    sprintf(port_buf, "%d", m_port);
    w_port->value(port_buf);
    w_reliable_only = new Fl_Check_Button(110, 180, 160, 20,
        "&Reliable conn. only");
    w_reliable_only->value(m_reliable_only);
    network->end();

    Fl_Group *players = new Fl_Group(10, 240, 280, 180, "Players");
    players->box(FL_DOWN_FRAME);
    int i = 0;
    for (int n = 0; n < 4; ++n)
    {
        w_players[n] = new Fl_Check_Button(20, 260 + n*40, 20, 20);
        w_names[n] = new Fl_Input(40, 260 + n*40, 100, 20);
        w_keys[n][0] = new Fl_Button(150, 260 + n*40, 60, 20, key_labels[m_keys[n][0]]);
        w_keys[n][1] = new Fl_Button(220, 260 + n*40, 60, 20, key_labels[m_keys[n][1]]);
        if (i < m_num_players && m_player_index[i] == n)
        {
            w_players[n]->value(1);
            ++i;
        }
        else
        {
            w_names[n]->deactivate();
            w_keys[n][0]->deactivate();
            w_keys[n][1]->deactivate();
        }
        w_names[n]->value(m_names[n].c_str());
        w_players[n]->callback(player_check_button_cb, this);
        w_keys[n][0]->callback(key_button_cb, this);
        w_keys[n][1]->callback(key_button_cb, this);
    }
    players->end();

    Fl_Button *w_start = new Fl_Button(10, 440, 280, 30, "&Start");
    w_start->callback(start_cb, this);
    Fl::focus(w_start);

    win->end();
    win->show();
    Fl::run();
    delete win;

    return start;
}

bool Config::parse_setting(std::string &key, std::string &value, int i, int j)
{
    if (key == "fullscreen")
    {
        m_fullscreen = (bool)atoi(value.c_str());
        return true;
    }

    if (key == "antialiasing")
    {
        m_antialiasing = (bool)atoi(value.c_str());
        return true;
    }

    if (key == "hostname")
    {
        m_hostname = value;
        return true;
    }

    if (key == "port")
    {
        m_port = atoi(value.c_str());
        return true;
    }

    if (key == "tcp_only")
    {
        m_reliable_only = (bool)atoi(value.c_str());
        return true;
    }

    if (key == "names" && (0 <= i && i < 4) && (j == 0))
    {
        m_names[i] = value;
        return true;
    }

    if (key == "keys" && (0 <= i && i < 4) && (0 <= j && j < 2))
    {
        int key_index = atoi(value.c_str());
        if (key_index >= 0 && key_index < NUM_KEYS)
        {
            m_keys[i][j] = key_index;
        }
        return true;
    }

    if (key == "player" && (0 <= i && i < 4) && (j == 0))
    {
        if (atoi(value.c_str()) != 0)
        {
            for (int n = 0; n < m_num_players; ++n)
            {
                if (m_player_index[n] == i) return true; /* already active */
            }
            m_player_index[m_num_players++] = i;
        }
        else
        {
            for (int n = 0; n < m_num_players; ++n)
            {
                if (m_player_index[n] == i)
                {
                    /* Remove player */
                    for ( ; n + 1 < m_num_players; ++n)
                    {
                        m_player_index[n] = m_player_index[n + 1];
                    }
                    --m_num_players;
                }
            }
        }
        return true;
    }

    return false;
}

bool Config::load_settings(const char *path)
{
    bool res = false;
    std::ifstream ifs(path);
    std::string line;
    while (getline(ifs, line))
    {
        if (line.empty() || line[0] == '#') continue;
        size_t sep = line.find('=');
        if (sep == std::string::npos) continue;
        std::string key(line, 0, sep), value(line, sep + 1);
        int i = 0, j = 0;
        if ((sep = key.find(':')) != std::string::npos)
        {
            std::string si(key, sep + 1, key.find(':', sep + 1));
            i = atoi(si.c_str());
            if ((sep = key.find(':', sep + 1)) != std::string::npos)
            {
                std::string sj(key, sep + 1, key.find(':', sep + 1));
                j = atoi(sj.c_str());
            }
            key.erase(key.find(':'));
        }

        if (parse_setting(key, value, i, j)) res = true;
    }
    return res;
}

bool Config::save_settings(const char *path)
{
    std::ofstream ofs(path);
    ofs << "fullscreen=" << m_fullscreen << '\n';
    ofs << "antialiasing=" << m_antialiasing << '\n';
    ofs << "hostname=" << m_hostname << '\n';
    ofs << "port=" << m_port << '\n';
    ofs << "tcp_only=" << m_reliable_only << '\n';
    for (int p = 0; p < 4; ++p)
    {
        ofs << "names:" << p << '=' << m_names[p] << '\n';
        for (int i = 0; i < 2; ++i)
        {
            ofs << "keys:" << p << ':' << i << '=' << m_keys[p][i] << '\n';
        }
    }
    for (int n = 0; n < 4; ++n)
    {
        bool active = false;
        for (int m = 0; m < m_num_players; ++m)
        {
            if (m_player_index[m] == n) active = true;
        }
        ofs << "player:" << n << '=' << (int)active << '\n';
    }
    return ofs;
}
