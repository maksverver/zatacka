#include "Movement.h"
#include <math.h>

bool position_update( Position *position, Move move,
                      double move_rate, double turn_rate )
{
    long double na = position->a;

    switch (move)
    {
    case MOVE_FORWARD:
        break;

    case MOVE_TURN_LEFT:
        na += turn_rate;
        break;

    case MOVE_TURN_RIGHT:
        na -= turn_rate;
        break;

    default:
        return false;
    }

    position->x += (long double)move_rate*cosl(0.5*(position->a + na));
    position->y += (long double)move_rate*sinl(0.5*(position->a + na));
    position->a = na;

    return true;
}
