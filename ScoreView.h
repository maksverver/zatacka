#ifndef SCORE_VIEW_H_INCLUDED
#define SCORE_VIEW_H_INCLUDED

#include "common.h"
#include <vector>
#include <string>

class ScoreView : public Fl_Group
{
public:
    ScoreView(int x, int y, int w, int h);
    ~ScoreView();

    void update( std::vector<std::string> names,
                 std::vector<int> scores,
                 std::vector<Fl_Color> colors );
};

#endif /* ndef SCORE_VIEW_H_INCLUDED */
