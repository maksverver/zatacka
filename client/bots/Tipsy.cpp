#include "SimpleSearch.h"
#include <stdlib.h>
#include <algorithm>

class Tipsy : public SimpleSearch
{
public:
    Move move( int timestamp, const Player *players,
               int player_index, const Field &field )
    {
        /* Shuffle moves around twice per second */
        if (rand()%(gp.data_rate) < 4) std::random_shuffle(moves, moves + 3);

        return SimpleSearch::move(timestamp, players, player_index, field);
    }
};

extern "C" PlayerController *create_bot() { return new Tipsy; }
