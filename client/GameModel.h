#ifndef GAME_MODEL_H_INCLUDED
#define GAME_MODEL_H_INCLUDED

#include "common.h"
#include <string>

struct Player
{
    Player()
        : col(), x(), y(), a(), timestamp(), last_move(),
          px(), py(), pa(), pt(),
          dead(), hole(-1), solid_since(), name(),
          score_cur(), score_tot(), score_avg(),
          rng_base(), rng_carry() { };

    Fl_Color    col;            /* color */
    double      x, y, a;        /* position (0 <= x,y < 1) and angle */
    int         timestamp;      /* last changed */
    int         last_move;      /* last move (used for predictions) */
    double      px, py, pa;     /* predicted position */
    int         pt;             /* predicted timestamp */
    bool        dead;           /* has died? */
    int         hole;           /* generating a hole */
    int         solid_since;    /* earliest timestamp after last hole */
    std::string name;           /* display name */
    int         score_cur;      /* score this round*/
    int         score_tot;      /* total score */
    int         score_avg;      /* moving average */

    /* Multiply-with-carry RNG for this player
       (used to determine random state transitions) */
    unsigned    rng_base;       /* base (current) value */
    unsigned    rng_carry;      /* last carry value */
};

#endif /* ndef GAME_MODEL_H_INCLUDED */
