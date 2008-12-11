#include "SimpleSearch.h"
#include <math.h>

static const int max_depth = 7;

SimpleSearch::SimpleSearch()
{
    moves[0] = MOVE_FORWARD;
    moves[1] = MOVE_TURN_LEFT;
    moves[2] = MOVE_TURN_RIGHT;
}

void SimpleSearch::restart(const GameParameters &gp_arg)
{
    gp = gp_arg;
}

int SimpleSearch::search(Position pos, int depth, Move *move_out)
{
    if (depth == max_depth) return max_depth;

    Move best_move = MOVE_FORWARD;
    int best_depth = depth;

    for (int i = 0; i < 3 && best_depth < max_depth; ++i)
    {
        int col = 0;
        Position old_pos, new_pos = pos;
        for (int pass = 0; pass < 2 + 2*depth && col == 0; ++pass)
        {
            old_pos = new_pos;
            position_update(&new_pos, moves[i], gp.move_rate, gp.turn_rate);
            col += field_line( (Field*)field, &old_pos, &new_pos, -1, NULL);
        }

        if (col == 0)
        {
            int new_depth = search(new_pos, depth + 1, NULL);
            if (new_depth > best_depth)
            {
                best_move = moves[i];
                best_depth = new_depth;
            }
        }
    }

    if (move_out != NULL) *move_out = best_move;
    return best_depth;
}

Move SimpleSearch::move( int timestamp, const Player *players,
                         int player_index, const Field &field_arg )
{
    const Position &pos = players[player_index].ppos;

    field = &field_arg;

    /* Join game (even if we start out in the right direction) */
    if (timestamp == 0) return MOVE_TURN_LEFT;

    if (timestamp < gp.warmup)
    {
        /* Turn towards center of field */
        double a = atan2(0.5 - pos.y, 0.5 - pos.x);
        double b = pos.a - a;
        while (b < -M_PI) b += 2.0*M_PI;
        while (b > +M_PI) b -= 2.0*M_PI;
        if (b < -0.1) return MOVE_TURN_LEFT;
        if (b > +0.1) return MOVE_TURN_RIGHT;
        return timestamp == 0 ? MOVE_TURN_LEFT : MOVE_FORWARD;
    }
    else
    {
        /* Search for path that doesn't kill me soon */
        Move best_move;
        search(pos, 0, &best_move);
        return best_move;
    }
}
