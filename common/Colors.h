#ifndef COLORS_H_INCLUDED
#define COLORS_H_INCLUDED

/* The color palette used by the server to assign player colors is defined here.
   Colors are recycled (i.e. the i-th player is assigned color i%NUM_COLORS). */

typedef struct RGB
{
    unsigned char r, g, b;
} RGB;

#define NUM_COLORS (16)

extern const RGB g_colors[NUM_COLORS];

#endif /* ndef COLORS_H_INCLUDED */
