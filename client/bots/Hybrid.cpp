#include "SimpleSearch.h"
#include <stdlib.h>
#include <FL/Fl.H>
#include <stdio.h>

class Hybrid : public SimpleSearch
{
public:
    Move move( int timestamp, const Player *players,
               int player_index, const Field &field )
    {
        bool left  = Fl::event_key(FL_Left);
        bool right = Fl::event_key(FL_Right);
        if (left && !right)
        {
            moves[0] = MOVE_TURN_LEFT;
            moves[1] = MOVE_FORWARD;
            moves[2] = MOVE_TURN_RIGHT;
        }
        else
        if (right && !left)
        {
            moves[0] = MOVE_TURN_RIGHT;
            moves[1] = MOVE_FORWARD;
            moves[2] = MOVE_TURN_LEFT;
        }
        else
        {
            moves[0] = MOVE_FORWARD;
            moves[1] = MOVE_TURN_LEFT;
            moves[2] = MOVE_TURN_RIGHT;
        }

        if (timestamp < gp.warmup)
            return moves[0];
        else
            return SimpleSearch::move(timestamp, players, player_index, field);
    }
};

extern "C" PlayerController *create_bot() { return new Hybrid; }
