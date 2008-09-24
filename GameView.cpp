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
        if (!sprites[n].visible) continue;

        fl_push_matrix();
        fl_translate(sprites[n].x, sprites[n].y);
        fl_rotate(180/M_PI*sprites[n].a);
        fl_color(sprites[n].col);
        fl_begin_polygon();
        fl_vertex( -9,  7);
        fl_vertex(  9,  0);
        fl_vertex( -9, -7);
        fl_end_polygon();
        fl_pop_matrix();

        fl_font(FL_HELVETICA, 12);
        fl_color(FL_WHITE);
        fl_draw( sprites[n].label.c_str(),
                 sprites[n].x, sprites[n].y + 12, 0, 12,
                 FL_ALIGN_CENTER );
    }

    fl_pop_clip();
}

void GameView::dot(double x, double y, Fl_Color c)
{
    double w = 0.007*this->w(), h = 0.007*this->h();
    int ix = (int)(this->w()*x - 0.5*w);
    int iy = (int)(this->h()*(1.0 - y) - 0.5*h);
    int iw = (int)(w + 0.5);
    int ih = (int)(h + 0.5);

    fl_begin_offscreen(offscr);
    fl_color(c);
    fl_pie(ix, iy, iw, ih, 0, 360);
    fl_end_offscreen();
    damage(1, ix, iy, iw, ih);
}

void GameView::line(double x1, double y1, double x2, double y2, Fl_Color c)
{
    double dx = (x2 - x1)/4, dy = (y2 - y1)/4;
    for (int n = 0; n < 4; ++n)
    {
        dot(x1, y1, c);
        x1 += dx;
        y1 += dy;
    }
}

void GameView::damageSprite(int n)
{
    damage(1, sprites[n].x - 24, sprites[n].y - 12, 48, 36);

    int text_width = 12*sprites[n].label.size();
    damage(1, sprites[n].x - text_width/2, sprites[n].y - 12, text_width, 40);
}

void GameView::setSprite( int n, int x, int y, double a,
                          Fl_Color col, const std::string &label )
{
    assert(n >= 0 && (size_t)n < sprites.size());
    if (sprites[n].visible) damageSprite(n);
    sprites[n].x = x;
    sprites[n].y = y;
    sprites[n].a = a;
    sprites[n].col = col;
    sprites[n].label = label;
    if (sprites[n].visible) damageSprite(n);
}

void GameView::showSprite(int n)
{
    assert(n >= 0 && (size_t)n < sprites.size());
    if (!sprites[n].visible)
    {
        sprites[n].visible = true;
        damageSprite(n);
    }
}

void GameView::hideSprite(int n)
{
    assert(n >= 0 && (size_t)n < sprites.size());
    if (sprites[n].visible)
    {
        sprites[n].visible = false;
        damageSprite(n);
    }
}
