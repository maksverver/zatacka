#include "GameView.h"

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
    damage(1, sprites[n].x - 24, sprites[n].y - 12, 48, 36);

    int text_width = 12*sprites[n].label.size();
    damage(1, sprites[n].x - text_width/2, sprites[n].y - 12, text_width, 36);
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
