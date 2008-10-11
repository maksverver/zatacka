#include "Field.h"
#include <stdbool.h>

/* Template used to plot dots */
static const char templ[7][7] = {
    { 0,0,1,1,1,0,0 },
    { 0,1,1,1,1,1,0 },
    { 1,1,1,1,1,1,1 },
    { 1,1,1,1,1,1,1 },
    { 1,1,1,1,1,1,1 },
    { 0,1,1,1,1,1,0 },
    { 0,0,1,1,1,0,0 } };

static int plot(Field *field, double x_in, double y_in, int col, bool update)
{
    int cx, cy, dx, dy, x, y, res;

    cx = 1000*x_in;
    cy = 1000*y_in;

    res = 0;
    for (dx = -3; dx <= 3; ++dx)
    {
        for (dy = -3; dy <= 3; ++dy)
        {
            if (!templ[dx + 3][dy + 3]) continue;
            x = cx + dx;
            y = cy + dy;
            if (x >= 0 && x < 1000 && y >= 0 && y < 1000)
            {
                if ((*field)[y][x] > res) res = (*field)[y][x];
                if (update) (*field)[y][x] = col;
            }
            else
            {
                res = 256;
            }
        }
    }
    return res;
}

int field_plot(Field *field, double x_in, double y_in, int col)
{
    return plot(field, x_in, y_in, col, true);
}

int field_test(Field *field, double x_in, double y_in)
{
    return plot(field, x_in, y_in, 0, false);
}
