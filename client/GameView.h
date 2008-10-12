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
    GameView(int x, int y, int w, int h, bool antialiasing);
    ~GameView();

    void line( double x1, double y1, double a1,
               double x2, double y2, double a2,
               int n );

    Fl_Color spriteColor(int n) { return sprites[n].col; };
    Sprite::SpriteType spriteType(int n) { return sprites[n].type; }
    const std::string &spriteLabel(int n) { return sprites[n].label; }

    void setSprite(int n, double x, double y, double a);
    void setSpriteColor(int n, Fl_Color col);
    void setSpriteType(int n, Sprite::SpriteType);
    void setSpriteLabel(int n, const std::string &label );

    void clear();
    void setSprites(int count);
    int getSprites() { return (int)sprites.size(); }

    bool writeFieldBitmap(const char *path);

protected:
    void useOffscreen();
    void draw();
    void damageSprite(int n);

private:
    bool offscr_created;
    bool antialiasing;
    Fl_Offscreen offscr;
    std::vector<Sprite> sprites;
    unsigned char field[FIELD_SIZE][FIELD_SIZE];
};

#endif /* ndef GAME_VIEW_H_INCLUDED */
