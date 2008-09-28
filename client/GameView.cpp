#include "GameView.h"
#include <algorithm>

GameView::GameView(int sprites, int x, int y, int w, int h)
    : Fl_Widget(x, y, w, h), offscr_created(false), sprites(sprites)
{
}

GameView::~GameView()
{
    if (offscr_created) fl_delete_offscreen(offscr);
}

void GameView::draw()
{
    fl_push_clip(x(), y(), w(), h());

    if (!offscr_created)
    {
        offscr = fl_create_offscreen(w(), h());
        offscr_created = true;
        fl_begin_offscreen(offscr);
        fl_color(FL_BLACK);
        fl_rectf(0, 0, w(), h());
        fl_end_offscreen();
    }

    /* Draw background */
    fl_copy_offscreen(x(), y(), w(), h(), offscr, 0, 0);

    /* Draw bounding rectangle */
    fl_color(fl_gray_ramp(FL_NUM_GRAY/2));
    fl_rect(x(), y(), w(), h());

    /* Draw player sprites */
    for (size_t n = 0; n < sprites.size(); ++n)
    {
        if (!sprites[n].visible()) continue;

        if (sprites[n].type == Sprite::ARROW)
        {
            fl_push_matrix();
            fl_translate(x() + sprites[n].x, y() + sprites[n].y);
            fl_rotate(180/M_PI*sprites[n].a);
            fl_color(sprites[n].col);
            fl_begin_polygon();
            fl_vertex( -9,  7);
            fl_vertex(  9,  0);
            fl_vertex( -9, -7);
            fl_end_polygon();
            fl_pop_matrix();
        }

        if (sprites[n].type == Sprite::DOT)
        {
            fl_push_matrix();
            fl_translate(x() + sprites[n].x, y() + sprites[n].y);
            fl_rotate(180/M_PI*sprites[n].a);
            fl_scale(1e-3*this->w());
            fl_color(sprites[n].col);
            fl_begin_polygon();
            fl_vertex(-10,  7);
            fl_vertex(  5,  0);
            fl_vertex(-10, -7);
            fl_end_polygon();
            fl_pop_matrix();
            /*
            int r = (int)ceil(0.004*this->w());
            int ix = x() + sprites[n].x - r;
            int iy = x() + sprites[n].y - r;
            int iw = 2*r + 1;
            int ih = 2*r + 1;

            fl_color(sprites[n].col);
            fl_pie(ix, iy, iw, ih, 0, 360);
            */
        }

        if (!sprites[n].label.empty())
        {
            fl_font(FL_HELVETICA, 12);
            fl_color(FL_WHITE);
            fl_draw( sprites[n].label.c_str(),
                     sprites[n].x, sprites[n].y + 12, 0, 12,
                     FL_ALIGN_CENTER, NULL, 0);
        }
    }

    fl_pop_clip();
}

void GameView::dot(double x, double y, Fl_Color c)
{
    int r = (int)ceil(0.003*this->w());
    int ix = (int)(this->w()*x) - r;
    int iy = (int)(this->h()*(1.0 - y)) - 1 - r;
    int iw = 2*r + 1;
    int ih = 2*r + 1;

    fl_begin_offscreen(offscr);
    fl_color(c);
    fl_pie(ix, iy, iw, ih, 0, 360);
    fl_end_offscreen();
    damage(1, ix, iy, iw, ih);
}

void GameView::line(double x1, double y1, double x2, double y2, Fl_Color c)
{
    double dx = (x2 - x1)/4, dy = (y2 - y1)/4;
    for (int n = 0; n < 5; ++n)
    {
        dot(x1, y1, c);
        x1 += dx;
        y1 += dy;
    }
}

void GameView::damageSprite(int n)
{
    damage(1, sprites[n].x - 24, sprites[n].y - 12, 48, 36);

    if (!sprites[n].label.empty())
    {
        int w = 0, h = 0;
        fl_font(FL_HELVETICA, 12);
        fl_measure(sprites[n].label.c_str(), w, h, 0);
        damage(1, sprites[n].x - w/2 - 1, sprites[n].y + 12 - h, w + 2, 2*h);
    }
}

void GameView::setSprite(int n, double x, double y, double a)
{
    assert(n >= 0 && (size_t)n < sprites.size());
    if (sprites[n].visible()) damageSprite(n);
    sprites[n].x = (int)(this->w()*x);
    sprites[n].y = (int)(this->h()*(1.0 - y)) - 1;
    sprites[n].a = a;
    if (sprites[n].visible()) damageSprite(n);
}

void GameView::setSpriteColor(int n, Fl_Color col)
{
    if (sprites[n].visible()) damageSprite(n);
    sprites[n].col = col;
    if (sprites[n].visible()) damageSprite(n);
}

void GameView::setSpriteType(int n, Sprite::SpriteType type)
{
    if (sprites[n].visible()) damageSprite(n);
    sprites[n].type = type;
    if (sprites[n].visible()) damageSprite(n);
}

void GameView::setSpriteLabel(int n, const std::string &label)
{
    if (sprites[n].visible()) damageSprite(n);
    sprites[n].label = label;
    if (sprites[n].visible()) damageSprite(n);
}
