#include "SimpleSearch.h"
#include <stdlib.h>
#include <algorithm>

class Twirly : public SimpleSearch
{
public:
    Twirly()
    {
        moves[0] = MOVE_TURN_LEFT;
        moves[1] = MOVE_FORWARD;
        moves[2] = MOVE_TURN_RIGHT;
    }

    void restart(const GameParameters &gp)
    {
        /* Pick a random direction to circle in */
        if (rand()&1) std::swap(moves[0], moves[2]);

        return SimpleSearch::restart(gp);
    }
};

extern "C" PlayerController *create_bot() { return new Twirly; }
