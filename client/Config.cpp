#include "Config.h"
#include "KeyWindow.h"

/* Possible window sizes */
static const int res_count = 8;
static const int res_default = 1;
static const char * const res_str[res_count] = {
    "640x480", "800x600", "1024x768", "1280x960",
    "1280x1024", "1400x1050", "1600x1200", "1680x1050" };
static const int res_width[res_count]  = {
      640,  800, 1024, 1280,
     1280, 1400, 1600, 1680 };
static const int res_height[res_count] = {
      480,  600,  768,  960,
     1024, 1050, 1200, 1050 };

/* Key sets (static for now) */
static const char *key_str[4][2] = {
    { "@<-", "@->" }, { "A", "D" }, { "M", "." }, { "4", "6" } };
static const int key_num[4][2] = {
    { FL_Left, FL_Right }, { 'a', 'd' }, { 'm', '.' }, { '4', '6' } };


Config::Config()
{
    /* Set some default values */
    m_resolution = 1;
    m_fullscreen = false;
    m_width  = res_width[m_resolution];
    m_height = res_height[m_resolution];

    m_hostname = "localhost";
    m_port = 12321;

    m_num_players = 1;
    m_player_index[0] = 0;
    {
        static char buf[16];
        sprintf(buf, "Player-%04x", rand()&0xffff);
        m_names[0].assign(buf);
    }

    for (int n = 0; n < 4; ++n)
    {
        m_keys[n][0] = key_num[n][0];
        m_keys[n][1] = key_num[n][1];
    }
}

Config::~Config()
{
}

bool Config::copy_settings()
{
    /* Copy settings */
    m_resolution = w_resolution->value();
    m_fullscreen = w_fullscreen->value();
    m_width  = res_width[m_resolution];
    m_height = res_height[m_resolution];

    /* Network config */
    m_hostname = w_hostname->value();
    m_port = atoi(w_port->value());

    /* Players */
    m_num_players = 0;
    for (int n = 0; n < 4; ++n)
    {
        m_names[n] = w_names[n]->value();
        if (!w_players[n]->value() || m_names[n].empty()) continue;
        /* TODO: custom keys */
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
    KeyWindow *kw = new KeyWindow(100, 50, "Key Binding");
    new Fl_Box(0, 0, 100, 50, "Please press a key or mouse button...");
    kw->end();
    kw->show();
    while (kw->visible()) Fl::wait();

    /* Get new key */
    int key = kw->key();
    if (key == 0) return;   /* canceled */

    /* If the key is already in use, swap it */
    for (int q = 0; q < 4; ++q)
    {
        for (int j = 0; j < 2; ++j)
        {
            if ((q != p || i != j) && cfg->m_keys[q][j] == key)
            {
                cfg->m_keys[q][j] = cfg->m_keys[p][i];
                cfg->w_keys[q][j]->label(cfg->w_keys[p][i]->label());
            }
        }
    }


    /* Now assign new key and label */
    cfg->m_keys[p][i] = key;

    /* NB. we don't copy the label string here, because it is statically
            allocated by the KeyWindow. If that ever changes, this code
            must be updated too! */
    w_button->label(kw->key_label());
}

bool Config::show_window()
{
    start = false;

    win = new Fl_Window(300, 500);
    win->label("Configuration");
    win->set_modal();

    Fl_Group *display = new Fl_Group(10, 30, 280, 80, "Display");
    display->box(FL_DOWN_FRAME);
    w_resolution = new Fl_Choice(110, 50, 170, 20, "Window size: ");
    for (int n = 0; n < res_count; ++n)
    {
        w_resolution->add(res_str[n], "", NULL);
    }
    w_resolution->value(m_resolution);
    w_fullscreen = new Fl_Check_Button(110, 70, 170, 20, "Fullscreen");
    w_fullscreen->value(m_fullscreen);
    display->end();

    Fl_Group *network = new Fl_Group(10, 140, 280, 80, "Network");
    network->box(FL_DOWN_FRAME);
    w_hostname = new Fl_Input(110, 160, 160, 20, "Host name: ");
    w_hostname->value(m_hostname.c_str());
    w_port = new Fl_Input(110, 180, 160, 20, "Port number: ");
    char port_buf[32];
    sprintf(port_buf, "%d", m_port);
    w_port->value(port_buf);
    network->end();

    Fl_Group *players = new Fl_Group(10, 250, 280, 180, "Players");
    players->box(FL_DOWN_FRAME);
    int i = 0;
    for (int n = 0; n < 4; ++n)
    {
        w_players[n] = new Fl_Check_Button(20, 270 + n*40, 20, 20);
        w_names[n] = new Fl_Input(40, 270+ n*40, 100, 20);
        w_keys[n][0] = new Fl_Button(150, 270 + n*40, 60, 20, key_str[n][0]);
        w_keys[n][1] = new Fl_Button(220, 270 + n*40, 60, 20, key_str[n][1]);
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

    Fl_Button *w_start = new Fl_Button(10, 450, 280, 30, "Start");
    w_start->callback(start_cb, this);

    win->end();
    win->show();
    Fl::run();
    delete win;

    return start;
}
