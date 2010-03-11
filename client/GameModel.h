#ifndef GAME_MODEL_H_INCLUDED
#define GAME_MODEL_H_INCLUDED

#include <string>

#include "../common/Movement.h"
#include "../common/Protocol.h"

struct GameParameters
{
    unsigned gameid;        /* current game id */
    int data_rate;          /* moves per second */
    double turn_rate;       /* radians per move */
    double move_rate;       /* units per move */
    double line_width;      /* thickness of line */
    int warmup;             /* number of turns to wait before moving */
    int score_rounds;       /* number of rounds for the moving average score */
    int hole_probability;   /* probability of a hole (1/g_hole_probability) */
    int hole_length_min;    /* minimum length of a hole (in turns) */
    int hole_length_max;    /* maximum length of a hole (in turns) */
    int hole_cooldown;      /* minimum number of turns between holes */
    int move_backlog;       /* number of moves to cache and send/receive */
    int num_players;        /* number of players in the game */
};

struct Player
{
    unsigned    col;            /* color */
    Position    pos;            /* player position */
    int         timestamp;      /* last changed */
    int         last_move;      /* last move (used for predictions) */
    Position    ppos;           /* predicted position */
    int         pt;             /* predicted timestamp */
    bool        dead;           /* has died? */
    int         hole;           /* generating a hole */
    int         solid_since;    /* earliest timestamp after last hole */
    const char  *name;          /* display name */
    int         score_cur;      /* score this round */
    int         score_tot;      /* total score */
    int         score_avg;      /* moving average */
    int         score_holes;    /* number of holes passed through */

    /* Multiply-with-carry RNG for this player
       (used to determine random state transitions) */
    unsigned    rng_base;       /* base (current) value */
    unsigned    rng_carry;      /* last carry value */
};

#endif /* ndef GAME_MODEL_H_INCLUDED */
