#ifndef GAME_VIEW_H_INCLUDED
#define GAME_VIEW_H_INCLUDED

#include "common.h"
#include <string>

struct Sprite
{
    Sprite() : visible(), x(), y(), a() { };

    bool visible;
    int x, y;
    double a;
    Fl_Color col;
    std::string label;
};

class GameView : public Fl_Widget
{
public:
    GameView(int sprites, int x, int y, int w, int h);
    ~GameView();

    void plot(int x, int y, Fl_Color c);
    void setSprite( int n, int x, int y, double a,
                    Fl_Color col, const std::string &label );
    void showSprite(int n);
    void hideSprite(int n);

protected:
    void useOffscreen();
    void draw();
    void damageSprite(int n);

private:
    bool offscr_created;
    Fl_Offscreen offscr;
    std::vector<Sprite> sprites;
};

#endif /* ndef GAME_VIEW_H_INCLUDED */
