#ifndef MAINWINDOW_H_INCLUDED
#define MAINWINDOW_H_INCLUDED

#include "common.h"
#include "MainGameView.h"
#include "ScoreView.h"

class MainWindow : public Fl_Double_Window
{
public:
    MainWindow(int width, int height, bool fullscreen, bool antialiasing);
    ~MainWindow();
    int handle(int type);
    void resize(int x, int y, int w, int h);
    MainGameView *gameView() { return gv; };
    ScoreView *scoreView() { return sv; };
    void setGameId(unsigned gid);
    void setFPS(double fps);
    void setTrafficStats( int bytes_in, int packets_in,
                          int bytes_out, int packets_out );
    void resetGameView(int players);

private:
    MainGameView *gv;       /* the play area (with chat overlay) */
    ScoreView *sv;          /* the score view */
    Fl_Box *gid_box;        /* box displaying the current game id */
    Fl_Box *fps_box;        /* box displaying the rendering framerate */
    Fl_Box *net_box;        /* box displaying inbound traffic stats */

    char gid_box_label[32];   /* buffer for gid_box label */
    char fps_box_label[32];   /* buffer for fps_box label */
    char net_box_label[64];   /* buffer for inbound traffic label */
};

#endif /* ndef MAINWINDOW_H_INCLUDED */
