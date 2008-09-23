#include "ScoreView.h"
#include <assert.h>

ScoreView::ScoreView(int x, int y, int w, int h)
    : Fl_Group(x, y, w, h)
{
    end();
}

ScoreView::~ScoreView()
{
}

void ScoreView::update( std::vector<std::string> names,
                        std::vector<int> scores,
                        std::vector<Fl_Color> colors )
{

    clear();
    begin();

    int x = this->x() + 5, y = this->y() + 5, w = this->w() - 10, h = 40;
    for (size_t n = 0; n < names.size(); ++n)
    {
        assert(n < scores.size());
        assert(n < colors.size());

        Fl_Box *box = new Fl_Box(x, y, w, h);
        box->box(FL_RSHADOW_BOX);
        box->color(colors[n]);
        box->label(names[n].c_str());
        box->labelfont(FL_HELVETICA);
        box->labelsize(20);
        box->labelcolor(FL_WHITE);
        box->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT);

        char scoreStr[64];
        sprintf(scoreStr, "%d", scores[n]);
        box = new Fl_Box(x + w - 30, y + 5, 30, h);
        box->color(colors[n]);
        box->label(scoreStr);
        box->labelfont(FL_HELVETICA);
        box->labelsize(24);
        box->labelcolor(FL_WHITE);
        box->align(FL_ALIGN_INSIDE | FL_ALIGN_RIGHT);

        y += 50;
    }
    end();
    redraw();
}
