#ifndef PLAYER_CONTROLLER_H_INCLUDED
#define PLAYER_CONTROLLER_H_INCLUDED

#include <common/Field.h>
#include "GameModel.h"

class PlayerController
{
public:
    PlayerController() { };
    virtual ~PlayerController() { };

    /* Called before the start of each game. */
    virtual void restart(const GameParameters &gp) = 0;

    /* Determine a valid move and return it.

       `timestamp' is the timestamp for the move (starting from 0); values
       below gp.warmup are in the warmup period, during which the player cannot
       move forward (but must return MOVE_FORWARD if he decides not to turn).

       `players' is the list of players (an array of length gp.num_players).

       `player_index' is the index of the controlled player into `players'.

       `field' is the current game field; the player with index i is represented
       by value (i + 1), since zeroes indicate free space.

       Implementation should return a valid move (not MOVE_NONE or MOVE_DEAD).
    */
    virtual Move move( int timestamp, const Player *players,
                       int player_index, const Field *field ) = 0;
};

#endif /* ndef PLAYER_CONTROLLER_H_INCLUDED */
