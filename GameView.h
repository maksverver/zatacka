#ifndef GAME_VIEW_H_INCLUDED
#define GAME_VIEW_H_INCLUDED

#include "common.h"

struct Sprite
{
    int x, y;
    double a;
    Fl_Color col;
};

class GameView : public Fl_Widget
{
public:
    GameView(int players, int x, int y, int w, int h);
    ~GameView();

    void plot(int x, int y, Fl_Color c);
    void setSprite(int n, int x, int y, double a, Fl_Color col);

protected:
    void useOffscreen();
    void draw();
    void damageSprite(int n);

private:
    bool offscr_created;
    Fl_Offscreen offscr;
    int players;
    std::vector<Sprite> sprites;
};

#endif /* ndef GAME_VIEW_H_INCLUDED */
