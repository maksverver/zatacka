#ifndef GAME_VIEW_H_INCLUDED
#define GAME_VIEW_H_INCLUDED

#include "common.h"
#include <string>

struct Sprite
{
    enum SpriteType { HIDDEN, ARROW, DOT };

    Sprite() : type(HIDDEN), x(), y(), a() { };

    bool visible() const { return type != HIDDEN; }

    SpriteType type;
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

    void dot(double x, double y, Fl_Color c);
    void line(double x1, double y1, double x2, double y2, Fl_Color c);

    void setSprite(int n, double x, double y, double a);
    void setSpriteColor(int n, Fl_Color col);
    void setSpriteType(int n, Sprite::SpriteType);
    void setSpriteLabel(int n, const std::string &label );

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
