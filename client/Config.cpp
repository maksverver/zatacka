#include "Config.h"

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

static void player_check_button_cb(Fl_Widget *w, void *arg)
{
    Fl_Check_Button *b = (Fl_Check_Button*)w;
    Fl_Widget *wa = (Fl_Widget*)arg;
    if (b->value() == 0) wa->deactivate();
    if (b->value() == 1) wa->activate();
}

bool Config::show_window()
{
    start = false;

    win = new Fl_Window(300, 500);
    win->label("Configuration");

    Fl_Group *display = new Fl_Group(10, 30, 280, 80, "Display");
    display->box(FL_DOWN_FRAME);
    w_resolution = new Fl_Choice(110, 50, 170, 20, "Window size: ");
    for (int n = 0; n < res_count; ++n)
    {
        w_resolution->add(res_str[n], "", NULL);
    }
    w_resolution->value(m_resolution);
    w_fullscreen = new Fl_Check_Button(110, 70, 170, 20, "Fullscreen");
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
        w_names[n] = new Fl_Input(40, 270+ n*40, 160, 20);

        if (i < m_num_players && m_player_index[i] == n)
        {
            w_players[n]->value(1);
            ++i;
        }
        else
        {
            w_names[n]->deactivate();
        }
        w_names[n]->value(m_names[n].c_str());
        w_players[n]->callback(player_check_button_cb, w_names[n]);
        w_keys[n][0] = new Fl_Button(210, 270+ n*40, 30, 20, key_str[n][0]);
        w_keys[n][0]->deactivate();
        w_keys[n][1] = new Fl_Button(250, 270+ n*40, 30, 20, key_str[n][1]);
        w_keys[n][1]->deactivate();
    }
    players->end();

    Fl_Button *w_start = new Fl_Button(10, 450, 280, 30, "Start");
    w_start->callback(start_cb, this);
    Fl::focus(w_start);

    win->end();
    win->show();
    Fl::run();
    delete win;

    return start;
}
