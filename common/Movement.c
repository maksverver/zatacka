#include "Movement.h"
#include <math.h>

bool position_update( Position *position, Move move,
                      double move_rate, double turn_rate )
{
    long double da, dl;

    if (move == MOVE_FORWARD)         da =  0;
    else if (move == MOVE_TURN_LEFT)  da =  turn_rate;
    else if (move == MOVE_TURN_RIGHT) da = -turn_rate;
    else return false;

    dl = move_rate * (da ? sinl(da/2)/(da/2) : 1);

    position->x += dl*cosl(position->a + da/2);
    position->y += dl*sinl(position->a + da/2);
    position->a += da;

    return true;
}
