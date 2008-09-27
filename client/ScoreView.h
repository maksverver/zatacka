#ifndef SCORE_VIEW_H_INCLUDED
#define SCORE_VIEW_H_INCLUDED

#include "common.h"
#include <vector>
#include <string>

class Player;

class ScoreView : public Fl_Group
{
public:
    ScoreView(int x, int y, int w, int h);
    ~ScoreView();

    void update(std::vector<Player> &players);

private:
    /* Copies of strings (because FLTK doesn't do memory management of labels) */
    std::vector<std::string> labels_name;
    std::vector<std::string> labels_score_cur;  /* current round's score */
    std::vector<std::string> labels_score_tot;  /* total score */
    std::vector<std::string> labels_score_avg;  /* moving average */
};

#endif /* ndef SCORE_VIEW_H_INCLUDED */
