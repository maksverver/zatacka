#include "ScoreView.h"
#include "GameModel.h"
#include <algorithm>
#include <sstream>

class ScoreWidget : public Fl_Widget
{
public:
    ScoreWidget(int x, int y, int w, int h, const Player &pl);
    void draw();

private:
    Player pl;
};

ScoreWidget::ScoreWidget(int x, int y, int w, int h, const Player &pl)
    : Fl_Widget(x, y, w, h), pl(pl)
{
}

static void draw_text( const char *text, int font_size,
                       int x, int y, int w, int h, Fl_Align align )
{
    fl_font(FL_HELVETICA, font_size);
    fl_color(fl_gray_ramp(FL_NUM_GRAY/2));
    fl_draw(text, x - 1, y    , w, h, align, 0, 0);
    fl_draw(text, x    , y - 1, w, h, align, 0, 0);
    fl_draw(text, x + 1, y    , w, h, align, 0, 0);
    fl_draw(text, x    , y + 1, w, h, align, 0, 0);
    fl_color(FL_WHITE);
    fl_draw(text, x, y, w, h, align, 0, 0);
}

void ScoreWidget::draw()
{
    char buf[32];
    int x = this->x(), y = this->y(), w = this->w(), h = this->h();

    /* Draw box */
    fl_draw_box(FL_RSHADOW_BOX, x, y, w, h, (Fl_Color)pl.col);

    /* Draw player name */
    draw_text( pl.name, 20, x + 15, y + 2, w - 75, h - 2,
               (Fl_Align)(FL_ALIGN_LEFT | FL_ALIGN_CLIP) );

    /* Draw main score */
    sprintf(buf, "%d", pl.score_avg);
    draw_text(buf, 24, x + w - 30, y + 2, 20, h - 2, FL_ALIGN_RIGHT);

    /* Draw total and current-round score */
    sprintf(buf, "%d", pl.score_cur);
    draw_text(buf, 12, x + w - 55, y + 5, 17, 20, FL_ALIGN_RIGHT);
    sprintf(buf, "%d", pl.score_tot);
    draw_text(buf, 12, x + w - 55, y + 17, 17, 20, FL_ALIGN_RIGHT);

    /* Draw hole score */
    fl_color(fl_gray_ramp(FL_NUM_GRAY/2));
    for (int n = 0; n < pl.score_holes; ++n)
    {
        fl_pie(x + w - 60 - 10*n, y + 5 + 2*(n&1), 9, 9, 0, 360);
    }
    fl_color(FL_BLACK);
    for (int n = 0; n < pl.score_holes; ++n)
    {
        fl_arc(x + w - 60 - 10*n, y + 5 + 2*(n&1), 9, 9, 0, 360);
    }
}


/* Compares to players by:
    - highest average score (first)
    - current round's score (second)
    - total score (third)
    - name (lexicographically) (fourth) */
static bool player_cmp(const Player* const &a, const Player* const &b)
{
    if (a->score_avg != b->score_avg) return a->score_avg > b->score_avg;
    if (a->score_cur != b->score_cur) return a->score_cur > b->score_cur;
    if (a->score_tot != b->score_tot) return a->score_tot > b->score_tot;
    return a->name < b->name;
}

ScoreView::ScoreView(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h)
{
    end();
}

ScoreView::~ScoreView()
{
}

template<class T> std::string stringify(const T &v)
{
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

void ScoreView::update(std::vector<Player> &players)
{
    /* Order players by score (descending) */
    std::vector<const Player*> ordered_players;
    for (size_t n = 0; n < players.size(); ++n)
    {
        ordered_players.push_back(&players[n]);
    }
    std::sort(ordered_players.begin(), ordered_players.end(), player_cmp);

    /* Remove widgets */
    clear();

    /* Recreate widgets */
    begin();
    int x = this->x() + 5, y = this->y() + 5, w = this->w() - 10, h = 40;
    for (size_t n = 0; n < players.size(); ++n)
    {
        new ScoreWidget(x, y, w, h, *ordered_players[n]);
        y += 50;
    }
    end();
    redraw();
}
