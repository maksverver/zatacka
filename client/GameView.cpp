#include "GameView.h"
#include <algorithm>
#include <string.h>

GameView::GameView(int x, int y, int w, int h, bool aa)
    : Fl_Widget(x, y, w, h), antialiasing(aa),
      line_width(),  offscr_created(false)
{
    memset(m_field, 0, sizeof(m_field));
}

GameView::~GameView()
{
    if (offscr_created) fl_delete_offscreen(offscr);
}

void GameView::resize(int x, int y, int w, int h)
{
    if (offscr_created && (w != this->w() || h != this->h()))
    {
        fl_delete_offscreen(offscr);
        offscr_created = false;
    }
    return Fl_Widget::resize(x, y, w, h);
}

void GameView::renderOffscreen(int x1, int y1, int x2, int y2)
{
    int w = this->w(), h = this->h();
    fl_begin_offscreen(offscr);
    fl_color(FL_BLACK);
    fl_rectf(x1, h - y2, x2 - x1, y2 - y1);
    if (antialiasing)
    {
        for (int y = y1; y < y2; ++y)
        {
            for (int x = x1; x < x2; ++x)
            {
                /* Simple anti-aliasing (sample nine subpixels) */
                unsigned r = 0, g = 0, b = 0;
                for (int dy = 0; dy < 3; ++dy)
                {
                    for (int dx = 0; dx < 3; ++dx)
                    {
                        int i = m_field [FIELD_SIZE*(3*y + dy)/(3*h)]
                                        [FIELD_SIZE*(3*x + dx)/(3*w)];
                        if (i > 0)
                        {
                            Fl_Color c = sprites[i - 1].col;
                            r += (c >> 24)&255;
                            g += (c >> 16)&255;
                            b += (c >>  8)&255;
                        }
                    }
                }

                if (r + g + b > 0)
                {
                    fl_color(fl_rgb_color(r/9, g/9, b/9));
                    fl_point(x, h - 1 - y);
                }
            }
        }
    }
    else
    {
        /* Non anti-aliased display (may render faster) */
        for (int y = y1; y < y2; ++y)
        {
            for (int x = x1; x < x2; ++x)
            {
                int n = m_field[FIELD_SIZE*y/h][FIELD_SIZE*x/w];
                if (n > 0)
                {
                    fl_color(n == 0 ? FL_BLACK : sprites[n - 1].col);
                    fl_point(x, h - 1 - y);
                }
            }
        }
    }
    fl_end_offscreen();

//    damage(this->x() + x1, this->y() + h - y2, x2 - x1, y2 - y1, 1);
    damage(1);
}

void GameView::setLineWidth(double lw)
{
    if (lw < 0) lw = 0;
    if (lw > 1) lw = 1;
    line_width = lw;
}

void GameView::drawLine(const Position *p, const Position *q, int n)
{
    Rect r;
    field_line_th(&m_field, p, q, FIELD_SIZE*line_width, n + 1, &r);

    int w = this->w(), h = this->h();
    int x1 = r.x1*w/FIELD_SIZE, x2 = (r.x2*w + FIELD_SIZE - 1)/FIELD_SIZE;
    int y1 = r.y1*h/FIELD_SIZE, y2 = (r.y2*h + FIELD_SIZE - 1)/FIELD_SIZE;
    if (offscr_created) renderOffscreen(x1, y1, x2, y2);
}

void GameView::draw()
{
    fl_push_clip(x(), y(), w(), h());

    if (!offscr_created)
    {
        offscr = fl_create_offscreen(w(), h());
        offscr_created = true;
        renderOffscreen(0, 0, w(), h());
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
            fl_scale(this->w());
            fl_color(sprites[n].col);
            fl_begin_polygon();
            fl_vertex( -2*line_width,  1.5*line_width );
            fl_vertex(  2*line_width,               0 );
            fl_vertex( -2*line_width, -1.5*line_width );
            fl_end_polygon();
            fl_pop_matrix();
        }

        if (sprites[n].type == Sprite::DOT)
        {
            fl_push_matrix();
            fl_translate(x() + sprites[n].x, y() + sprites[n].y);
            fl_rotate(180/M_PI*sprites[n].a);
            fl_scale(this->w());
            fl_color(sprites[n].col);
            fl_begin_polygon();
            fl_vertex( -1.5*line_width,  line_width );
            fl_vertex( 0.75*line_width,           0 );
            fl_vertex( -1.5*line_width, -line_width );
            fl_end_polygon();
            fl_pop_matrix();
        }

        if (!sprites[n].label.empty())
        {
            int font_size = w()/50 + 1;
            fl_font(FL_HELVETICA, font_size);
            fl_color(FL_WHITE);
            fl_draw( sprites[n].label.c_str(),
                     sprites[n].x, sprites[n].y + 5*font_size/4, 0, font_size,
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
        int font_size = this->w()/50 + 1;
        fl_font(FL_HELVETICA, font_size);
        fl_measure(sprites[n].label.c_str(), w, h, 0);
        // damage(1, x() + sprites[n].x - w/2 - 4, y() + sprites[n].y + 6, w + 8, 24);
        damage(1);
    }
}

void GameView::setSprite(int n, double x, double y, double a)
{
    assert(n >= 0 && (size_t)n < sprites.size());
    if (sprites[n].visible()) damageSprite(n);
    sprites[n].x = (int)(this->w()*x);
    sprites[n].y = this->h() - 1 - (int)(this->h()*y);
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
    memset(&m_field, 0, sizeof(m_field));
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
    return bmp_write(path, &m_field[0][0], FIELD_SIZE, FIELD_SIZE);
}
