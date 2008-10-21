#include "Field.h"
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>

typedef struct Point
{
    int x, y;
} Point;

/* Draws a closed convex polygon with points given in counter-clockwise order.
   Returns the maximum overlapping value or 256 if out-of-bounds.
   If col is negative, the field is not modified; this is useful to test for
   collision without drawing.

   FIXME: correctness seems to depend on compiler settings used!
          figure out what causes this (excess double precision in field_line?)
          and how it can be fixed.

          (NB. this function has been modified since; this may no longer apply)
*/
__attribute__((__noinline__))
static int draw_poly(Field *field, const Point *pts, int npt, int col)
{
    int y, x, n, res;
    int prv_x1, prv_x2;
    int cur_x1, cur_x2;
    int nxt_x1, nxt_x2;
    int hit_x1, hit_x2;

    res = 0;

    /* Find min/max y coordinates */
    int y1, y2;
    y1 = y2 = pts[0].y;
    for (n = 1; n < npt; ++n)
    {
        if (pts[n].y < y1) y1 = pts[n].y;
        if (pts[n].y > y2) y2 = pts[n].y;
    }

    /* Clip y coordinates into field rectangle */
    if (y1 < 0) y1 = 0, res = 256;
    if (y2 >= FIELD_SIZE) y2 = FIELD_SIZE - 1, res = 256;

    /* Draw scanlines bounded by polygon */
    prv_x1 = cur_x1 = 1;
    prv_x2 = cur_x2 = 0;
    for (y = y1; y <= y2; ++y)
    {
        /* Clip against crossing edges */
        for (n = 0; n < npt;++n)
        {
            const Point *p = &pts[n], *q = &pts[(n + 1)%npt];

            int *sx;
            if (p->y <= y && q->y >= y)
            {
                /* Clip left side */
                sx = &nxt_x1;
            }
            else
            if (p->y >= y && q->y <= y)
            {
                /* Clip right side */
                sx = &nxt_x2;
            }
            else
            {
                continue;
            }

            /* this is essential to prevent false overlap detection; it ensures
               that a line contains the same points even if its endpoints are
               swapped (which wouldn't be true otherwise). */
            if (p->x > q->x || (p->x == q->x && p->y > q->y))
            {
                const Point *r = p;
                p = q;
                q = r;
            }

            /* this is a relatively poor estimation, especially when angles
               are close to 90 agrees. */
            *sx = p->x + (y - p->y) *
                         (q->x - p->x + (p->x <= q->x ? 1 : -1)) /
                         (q->y - p->y + (p->y <= q->y ? 1 : -1));
        }

        if (nxt_x1 < 0) nxt_x1 = 0, res = 256;
        if (nxt_x2 >= FIELD_SIZE) nxt_x2 = FIELD_SIZE - 1, res = 256;

        /* Hit-test current scanline inside the polygon */
        hit_x1 = cur_x1 + 1;
        hit_x2 = cur_x2 - 1;
        if (prv_x1 > hit_x1) hit_x1 = prv_x1;
        if (nxt_x1 > hit_x1) hit_x1 = nxt_x1;
        if (prv_x2 < hit_x2) hit_x2 = prv_x2;
        if (nxt_x2 < hit_x2) hit_x2 = nxt_x2;
        for (x = hit_x1; x <= hit_x2; ++x)
        {
            if ((*field)[y - 1][x] > res) res = (*field)[y - 1][x];
        }

        /* Fill in current scanline */
        if (col >= 0)
        {
            for (x = cur_x1; x <= cur_x2; ++x) (*field)[y - 1][x] = col;
        }

        prv_x1 = cur_x1, prv_x2 = cur_x2;
        cur_x1 = nxt_x1, cur_x2 = nxt_x2;
    }

    /* Fill in remaining last scanline */
    if (col >= 0)
    {
        for (x = cur_x1; x <= cur_x2; ++x) (*field)[y - 1][x] = col;;
    }

    return res;
}

int field_line( Field *field, const Position *p, const Position *q,
                int col, Rect *rect )
{
    const double th = 7e-3*FIELD_SIZE;    /* thickness */

    double dx1 = -sin(p->a), dy1 = cos(p->a);
    double dx2 = -sin(q->a), dy2 = cos(q->a);

    Point pts[4] = {
        { (int)(FIELD_SIZE*p->x + 0.5 - 0.5*th*dx1),
          (int)(FIELD_SIZE*p->y + 0.5 - 0.5*th*dy1) },
        { (int)(FIELD_SIZE*p->x + 0.5 + 0.5*th*dx1),
          (int)(FIELD_SIZE*p->y + 0.5 + 0.5*th*dy1) },
        { (int)(FIELD_SIZE*q->x + 0.5 + 0.5*th*dx2),
          (int)(FIELD_SIZE*q->y + 0.5 + 0.5*th*dy2) },
        { (int)(FIELD_SIZE*q->x + 0.5 - 0.5*th*dx2),
          (int)(FIELD_SIZE*q->y + 0.5 - 0.5*th*dy2) } };

    if (rect != NULL)
    {
        int n;
        rect->x1 = rect->x2 = pts[0].x;
        rect->y1 = rect->y2 = pts[0].y;
        for (n = 1; n < 4; ++n)
        {
            if (pts[n].x < rect->x1) rect->x1 = pts[n].x;
            if (pts[n].x > rect->x2) rect->x2 = pts[n].x;
            if (pts[n].y < rect->y1) rect->y1 = pts[n].y;
            if (pts[n].y > rect->y2) rect->y2 = pts[n].y;
        }
        rect->x2 += 1;
        rect->y2 += 1;
        if (rect->x1 < 0) rect->x1 = 0;
        if (rect->y1 < 0) rect->y1 = 0;
        if (rect->x2 > FIELD_SIZE) rect->x2 = FIELD_SIZE;
        if (rect->y2 > FIELD_SIZE) rect->y2 = FIELD_SIZE;
    }

    return draw_poly(field, pts, 4, col);
}
