#include "Config.h"

Config::Config()
{
}

Config::~Config()
{
}

void start_cb(Fl_Widget *button, void *arg)
{
    (void)button;

    Config *cfg = (Config*)arg;
    cfg->start = true;
    cfg->win->hide();
}

bool Config::show_window()
{
    start = false;

    win = new Fl_Window(300, 500);
    win->label("Configuration");

    Fl_Group *display = new Fl_Group(10, 30, 280, 80, "Display");
    display->box(FL_DOWN_FRAME);
    w_resolution = new Fl_Choice(100, 50, 170, 20, "Resolution");
    w_resolution->add("640x480", "", NULL);
    w_resolution->add("800x600", "", NULL);
    w_resolution->add("1024x768", "", NULL);
    w_resolution->add("1280x960", "", NULL);
    w_resolution->add("1680x1050", "", NULL);
    w_resolution->value(1);
    w_fullscreen = new Fl_Check_Button(100, 70, 170, 20, "Fullscreen");
    display->end();

    Fl_Group *network = new Fl_Group(10, 140, 280, 80, "Network");
    network->box(FL_DOWN_FRAME);
    w_host = new Fl_Input(100, 160, 160, 20, "Host name");
    w_host->value("localhost");
    w_port = new Fl_Input(100, 180, 160, 20, "Port number");
    w_port->value("12321");
    network->end();

    Fl_Group *players = new Fl_Group(10, 250, 280, 180, "Players");
    players->box(FL_DOWN_FRAME);
    for (int n = 0; n < 4; ++n)
    {
        static const char *keys[4][2] = {
            { "@<-", "@->" }, { "A", "D" }, { "M", "." }, { "4", "6" } };

        w_players[n] = new Fl_Check_Button(20, 270 + n*40, 20, 20);
        w_names[n] = new Fl_Input(40, 270+ n*40, 160, 20);
        w_names[n]->deactivate();
        w_keys[n][0] = new Fl_Button(210, 270+ n*40, 30, 20, keys[n][0]);
        w_keys[n][0]->deactivate();
        w_keys[n][1] = new Fl_Button(250, 270+ n*40, 30, 20, keys[n][1]);
        w_keys[n][1]->deactivate();
    }
    players->end();

    (new Fl_Button(10, 450, 280, 30, "Start"))->callback(start_cb, this);

    win->end();
    win->show();
    Fl::run();
    delete win;

    return start;
}
