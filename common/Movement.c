#include "Movement.h"
#include <math.h>

bool position_update( Position *position, Move move,
                      double move_rate, double turn_rate )
{
    double da, dl;

    if (move == MOVE_FORWARD)         da =  0;
    else if (move == MOVE_TURN_LEFT)  da =  turn_rate;
    else if (move == MOVE_TURN_RIGHT) da = -turn_rate;
    else return false;

    dl = move_rate * (da ? sin(turn_rate/2)/(turn_rate/2) : 1);

    position->x += dl*cosl(position->a + 0.5*da);
    position->y += dl*sinl(position->a + 0.5*da);
    position->a += da;

    return true;
}
