#include "MainWindow.h"
#include <stdio.h>

MainWindow::MainWindow( int width, int height,
                        bool fullscreen, bool antialiasing )
    : Fl_Double_Window(width, height)
{
    label("Zatacka!");
    color(fl_gray_ramp(FL_NUM_GRAY/4));
    gv = new MainGameView(2, 2, height - 4, height - 4, antialiasing);
    sv = new ScoreView(height, 0, width - height, height - 40);
    gid_box = new Fl_Box(height, height - 20, width - height, 20);
    gid_box->labelfont(FL_HELVETICA);
    gid_box->labelsize(12);
    gid_box->labelcolor(fl_gray_ramp(2*FL_NUM_GRAY/3));
    gid_box->align(FL_ALIGN_INSIDE);
    fps_box = new Fl_Box(height, height - 40, width - height, 20);
    fps_box->labelfont(FL_HELVETICA);
    fps_box->labelsize(12);
    fps_box->labelcolor(fl_gray_ramp(2*FL_NUM_GRAY/3));
    fps_box->align(FL_ALIGN_INSIDE);
    end();
    if (fullscreen)
    {
        this->fullscreen();
    }
    else
    {
        size_range(100, 100, 0, 0, 0, 0, 0);
    }
}

MainWindow::~MainWindow()
{
}

int MainWindow::handle(int type)
{
    if (type == FL_KEYDOWN || type == FL_PUSH)
    {
        return gv->handleKeyPress(Fl::event_key());
    }
    return 0;
}

void MainWindow::resize(int x, int y, int w, int h)
{
    int gv_size = 3*w/4;
    if (gv_size > h) gv_size = h;
    gv->resize(5, 5, gv_size - 10, gv_size - 10);
    sv->resize(gv_size, 0, w - gv_size, h - 40);
    gid_box->resize(gv_size, h - 20, w - gv_size, 20);
    fps_box->resize(gv_size, h - 40, w - gv_size, 20);
    return Fl_Double_Window::resize(x, y, w, h);
}

void MainWindow::setGameId(unsigned gid)
{
    sprintf(gid_box_label, "%08X", gid);
    gid_box->label(gid_box_label);
}

void MainWindow::setFPS(double fps)
{
    if (fps > 999) fps = 999;
    sprintf(fps_box_label, "%.3f FPS", fps);
    fps_box->label(fps_box_label);
}

void MainWindow::resetGameView(int players)
{
    gv->clear();
    gv->setSprites(players);
}
