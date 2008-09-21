#include "GameView.h"


GameView::GameView(int players, int x, int y, int w, int h)
    : Fl_Widget(x, y, w, h),
      offscr_created(false),
      players(players), sprites(players)
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

    /* Draw player sprites */
    for (int n = 0; n < players; ++n)
    {
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
    }

    fl_pop_clip();
}

void GameView::plot(int x, int y, Fl_Color c)
{
    fl_begin_offscreen(offscr);
    fl_color(c);
    fl_pie(x - 3, y - 3, 6, 6, 0, 360);
    fl_end_offscreen();
    damage(1, x - 3, y - 3, 6, 6);
}

void GameView::damageSprite(int n)
{
    damage(1, sprites[n].x - 12, sprites[n].y - 12, 24, 24);
}

void GameView::setSprite(int n, int x, int y, double a, Fl_Color col)
{
    assert(n >= 0 && (size_t)n < sprites.size());
    damageSprite(n);
    sprites[n].x = x;
    sprites[n].y = y;
    sprites[n].a = a;
    sprites[n].col = col;
    damageSprite(n);
}
