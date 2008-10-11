#ifndef FIELD_H_INCLUDED
#define FIELD_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char (Field)[1000][1000];

/* Plot a dot at the given position in the given color.
   Returns the maximum value of the colors of the overlapping pixels,
   or 256 if the dot falls(partially) outside the field. */
int field_plot(Field *field, double x, double y, int col);

/* Returns the maximum value of the colors of the overlapping pixels,
   or 256 if the dot falls(partially outside the field.
   Does not update the field. */
int field_test(Field *field, double x, double y);

#ifdef __cplusplus
}
#endif

#endif /* ndef FIELD_H_INCLUDED */
