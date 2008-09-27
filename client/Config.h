#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

#include "common.h"
#include <string>

class Config
{
public:
    Config();
    ~Config();

    bool show_window();

    bool fullscreen() const { return m_fullscreen; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    const std::string &hostname() const { return m_hostname; }
    int port() const { return m_port; }
    int players() const { return m_num_players; }
    const std::string &name(int n) const { return m_names[m_player_index[n]]; }
    int key(int n, int m) const { return m_keys[m_player_index[n]][m]; }

protected:
    bool copy_settings();

private:
    bool start;

    /* Game window configuration */
    int  m_resolution;
    bool m_fullscreen;
    int  m_width, m_height;

    /* Network config */
    std::string m_hostname;
    int m_port;

    /* Players */
    int m_num_players;
    int m_player_index[4];
    std::string m_names[4];
    int m_keys[4][2];

    /* GUI */
    Fl_Window *win;
    Fl_Group *w_display;
    Fl_Choice *w_resolution;
    Fl_Check_Button *w_fullscreen;
    Fl_Input *w_hostname;
    Fl_Input *w_port;
    Fl_Check_Button *w_players[4];
    Fl_Input *w_names[4];
    Fl_Button *w_keys[4][2];

    friend void start_cb(Fl_Widget *button, void *arg);
};


#endif /* ndef CONFIG_H_INCLUDED */

