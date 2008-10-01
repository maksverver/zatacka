#ifndef MAINWINDOW_H_INCLUDED
#define MAINWINDOW_H_INCLUDED

#include "common.h"
#include "MainGameView.h"
#include "ScoreView.h"

class MainWindow : public Fl_Double_Window
{
public:
    MainWindow(int width, int height, bool fullscreen);
    ~MainWindow();
    int handle(int type);
    MainGameView *gameView() { return gv; };
    ScoreView *scoreView() { return sv; };
    void setGameId(unsigned gid);
    void resetGameView(int players);

private:
    MainGameView *gv;       /* the play area (with chat overlay) */
    ScoreView *sv;          /* the score view */
    Fl_Box *gid_box;        /* box displaying the current game id */

    char gid_box_label[32];   /* buffer for gid_box label */
};

#endif /* ndef MAINWINDOW_H_INCLUDED */
