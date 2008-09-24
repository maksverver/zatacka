#include "ScoreView.h"
#include "GameModel.h"
#include <assert.h>
#include <algorithm>

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

std::string int_to_string(int i)
{
    char buf[32];
    sprintf(buf, "%d", i);
    return std::string(buf);
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

    /* Allocate strings */
    labels_name.clear();
    labels_score_cur.clear();
    labels_score_tot.clear();
    labels_score_avg.clear();
    for (size_t n = 0; n < players.size(); ++n)
    {
        labels_name.push_back("  " + ordered_players[n]->name);
        labels_score_cur.push_back(int_to_string(ordered_players[n]->score_cur));
        labels_score_tot.push_back(int_to_string(ordered_players[n]->score_tot));
        labels_score_avg.push_back(int_to_string(ordered_players[n]->score_avg));
    }

    /* Recreate widgets */
    begin();
    int x = this->x() + 5, y = this->y() + 5, w = this->w() - 10, h = 40;
    for (size_t n = 0; n < players.size(); ++n)
    {
        Fl_Color color = ordered_players[n]->col;

        /* Main box (with name) */
        Fl_Box *box = new Fl_Box(x, y, w, h);
        box->box(FL_RSHADOW_BOX);
        box->color(color);
        box->label(labels_name[n].c_str());
        box->labelfont(FL_HELVETICA);
        box->labelsize(20);
        box->labelcolor(FL_WHITE);
        box->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT);

        /* Main score box */
        box = new Fl_Box(x + w - 30, y + 2, 25, h);
        box->color(color);
        box->label(labels_score_avg[n].c_str());
        box->labelfont(FL_HELVETICA);
        box->labelsize(24);
        box->labelcolor(FL_WHITE);
        box->align(FL_ALIGN_INSIDE | FL_ALIGN_RIGHT);

        /* Sub score boxes */
        box = new Fl_Box(x + w - 50, y + 5, 17, 20);
        box->color(color);
        box->label(labels_score_cur[n].c_str());
        box->labelfont(FL_HELVETICA);
        box->labelsize(12);
        box->labelcolor(FL_WHITE);
        box->align(FL_ALIGN_INSIDE | FL_ALIGN_RIGHT);

        box = new Fl_Box(x + w - 50, y + 17, 17, 20);
        box->color(color);
        box->label(labels_score_tot[n].c_str());
        box->labelfont(FL_HELVETICA);
        box->labelsize(12);
        box->labelcolor(FL_WHITE);
        box->align(FL_ALIGN_INSIDE | FL_ALIGN_RIGHT);

        y += 50;
    }

    end();
    redraw();
}
