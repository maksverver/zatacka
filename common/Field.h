#ifndef FIELD_H_INCLUDED
#define FIELD_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#define FIELD_SIZE (1000)

typedef unsigned char (Field)[FIELD_SIZE][FIELD_SIZE];

/* Draw a line segment with from (x1,y1) to (x2,y2), with starting angle `a1`
   and ending angle `a2`, using color `col` and the default thickness.
   Returns the maximum value of the colors of the overlapping pixels,
   or 256 if the segment falls (partially) outside the field.

   If col is set to -1, no lines are drawn.
*/
int field_line( Field *field,
                double x1, double y1, double a1,
                double x2, double y2, double a2,
                int col );

#ifdef __cplusplus
}
#endif

#endif /* ndef FIELD_H_INCLUDED */
