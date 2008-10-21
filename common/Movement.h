#ifndef MOVEMENT_H_INCLUDED
#define MOVEMENT_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include "Protocol.h"
#include <stdbool.h>

typedef struct Position
{
    double x;  /* x coordinate (0 <= x <= 1) */
    double y;  /* y coordinate (0 <= y <= 1) */
    double a;  /* angle in radians (denormalized) */
} Position;

/* Updates the position with the given move.

   If move is not one of MOVE_TURN_LEFT, MOVE_TURN_RIGHT or MOVE_FORWARD,
   false is returned and the position is not changed.

   Otherwise, if move is MOVE_TURN_LEFT or MOVE_TURN_RIGHT, the angle is
   incremented or decremented by turn_rate (respectively); afterwards, the
   position is moved forward by move_rate, and true is returned.
*/
bool position_update( Position *position, Move move,
                      double move_rate, double turn_rate );

#ifdef __cplusplus
}
#endif

#endif /* ndef MOVEMENT_H_INCLUDED */

