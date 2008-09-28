#include "MainWindow.h"
#include <stdio.h>

MainWindow::MainWindow(int width, int height, bool fullscreen)
    : Fl_Window(width, height)
{
    label("Zatacka!");
    color(fl_gray_ramp(FL_NUM_GRAY/4));
    gv = new MainGameView(0, 2, 2, height - 4, height - 4);
    sv = new ScoreView(height, 0, width - height, height - 20);
    gid_box = new Fl_Box(height, height - 20, width - height, 20);
    gid_box->labelfont(FL_HELVETICA);
    gid_box->labelsize(12);
    gid_box->labelcolor(fl_gray_ramp(2*FL_NUM_GRAY/3));
    gid_box->align(FL_ALIGN_INSIDE);
    end();
    if (fullscreen) this->fullscreen();
}

MainWindow::~MainWindow()
{
}

int MainWindow::handle(int type)
{
    if (type == FL_KEYDOWN)
    {
        return gv->handleKeyPress(Fl::event_key());
    }
    return 0;
}

void MainWindow::setGameId(unsigned gid)
{
    sprintf(gid_box_label, "%08X", gid);
    gid_box->label(gid_box_label);
}

void MainWindow::resetGameView(int players)
{
    MainGameView *new_gv
        = new MainGameView(players, gv->x(), gv->y(), gv->w(), gv->h());
    remove(gv);
    delete gv;
    gv = new_gv;
    add(gv);
    gv->redraw();
}
