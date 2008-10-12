#include "GameView.h"
#include <algorithm>
#include <string.h>

GameView::GameView(int x, int y, int w, int h)
    : Fl_Widget(x, y, w, h), offscr_created(false)
{
    memset(field, 0, sizeof(field));
}

GameView::~GameView()
{
    if (offscr_created) fl_delete_offscreen(offscr);
}

void GameView::line( double px1, double py1, double pa1,
                     double px2, double py2, double pa2,
                     int n )
{
    Rect r;
    field_line(&field, px1, py1, pa1, px2, py2, pa2, n + 1, &r);

    int w = this->w(), h = this->h();
    int x1 = r.x1*w/FIELD_SIZE, x2 = r.x2*w/FIELD_SIZE + 1;
    int y1 = r.y1*h/FIELD_SIZE, y2 = r.y2*h/FIELD_SIZE + 1;

    fl_begin_offscreen(offscr);
    fl_color(sprites[n].col);
    for (int y = y1; y < y2; ++y)
    {
        for (int x = x1; x < x2; ++x)
        {
            if (field[FIELD_SIZE*y/h][FIELD_SIZE*x/w] == n + 1)
            {
                fl_point(x, h - 1 - y);
            }
        }
    }
    fl_end_offscreen();

    damage(this->x() + x1, this->y() + h - 1 - y2, x2 - x1, y2 - y1, 1);
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

void GameView::damageSprite(int n)
{
    damage(1, x() + sprites[n].x - 16, y() + sprites[n].y - 16, 32, 32);

    if (!sprites[n].label.empty())
    {
        int w = 0, h = 0;
        fl_font(FL_HELVETICA, 12);
        fl_measure(sprites[n].label.c_str(), w, h, 0);
        damage(1, x() + sprites[n].x - w/2 - 4, y() + sprites[n].y + 6, w + 8, 24);
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

void GameView::clear()
{
    if (offscr_created)
    {
        fl_begin_offscreen(offscr);
        fl_color(FL_BLACK);
        fl_rectf(0, 0, w(), h());
        fl_end_offscreen();
    }
    memset(field, 0, sizeof(field));
    damage(1);
}

void GameView::setSprites(int count)
{
    for (int n = count; n < getSprites(); ++n)
    {
        if (sprites[n].visible()) damageSprite(n);
    }
    sprites.resize(count);
}

bool GameView::writeFieldBitmap(const char *path)
{
    return bmp_write(path, &field[0][0], 1000, 1000);
}
