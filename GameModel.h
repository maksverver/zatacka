#ifndef GAME_MODEL_H_INCLUDED
#define GAME_MODEL_H_INCLUDED

#include "common.h"
#include <string>

struct Player
{
    Player()
        : col(), x(), y(), a(), dead(), name(), timestamp(),
          score_cur(), score_tot(), score_avg() { };

    Fl_Color    col;            /* color */
    double      x, y, a;        /* position (0 <= x,y < 1) and angle */
    bool        dead;           /* has died? */
    std::string name;           /* display name */
    int         timestamp;      /* last changed */
    int         score_cur;      /* score this round*/
    int         score_tot;      /* total score */
    int         score_avg;      /* moving average */
};

#endif /* ndef GAME_MODEL_H_INCLUDED */
