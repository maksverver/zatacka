#ifndef FIELD_H_INCLUDED
#define FIELD_H_INCLUDED

#include "Movement.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FIELD_SIZE (2000)

typedef unsigned char (Field)[FIELD_SIZE][FIELD_SIZE];

/* A rectangle from (x1,y1) to (x2,y2) (exclusive) */
typedef struct Rect
{
    int x1, y1, x2, y2;
} Rect;

/* Draws a line segment from (x1,y1) to (x2,y2), with starting angle `a1`
   and ending angle `a2`, `th` pixels wide, using a using color `col`.

   Returns the maximum value of the colors of the overlapping pixels,
   or 256 if the segment falls (partially) outside the field.

   If col is set to -1, no lines are drawn.

   If rect is not NULL, it is updated to reflect the rectangle of pixels
   affected by the drawing operation.
*/
int field_line_th( Field *field, const Position *p, const Position *q,
                   double th, int col, Rect *rect );

#ifdef __cplusplus
}
#endif

#endif /* ndef FIELD_H_INCLUDED */
