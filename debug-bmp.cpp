#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Debug.h"

/* Ugly debugging code, which dumps fields to a file (for comparison with the
   server file). */

static unsigned char g_field[1000][1000];

void debug_dump_image(unsigned id)
{
    char path[64];
    FILE *fp;

    struct BMP_Header
    {
        char   bfType[2];
        int    bfSize;
        int    bfReserved;
        int    bfOffBits;

        int    biSize;
        int    biWidth;
        int    biHeight;
        short  biPlanes;
        short  biBitCount;
        int    biCompression;
        int    biSizeImage;
        int    biXPelsPerMeter;
        int    biYPelsPerMeter;
        int    biClrUsed;
        int    biClrImportant;
    } __attribute__((__packed__)) bmp_header;
    memset(&bmp_header, 0, sizeof(bmp_header));
    bmp_header.bfType[0] = 'B';
    bmp_header.bfType[1] = 'M';
    bmp_header.bfSize = 54 + 1000*1000 + 256*4;
    bmp_header.bfOffBits = 54 + 1024;
    bmp_header.biSize = 40;
    bmp_header.biWidth = 1000;
    bmp_header.biHeight = 1000;
    bmp_header.biPlanes = 1;
    bmp_header.biBitCount = 8;

    struct BMP_RGB {
        unsigned char b, g, r, a;
    } bmp_palette[256] = {
        {    0,   0,   0,   0 },
        {    0,   0, 255,   0 },
        {    0, 255,   0,   0 },
        {  255,   0,   0,   0 },
        {    0, 255, 255,   0 },
        {  255,   0, 255,   0 },
        {  255, 255,   0,   0 },
        {  255, 255, 255,   0 } };

    assert(sizeof(bmp_header) == 54);
    assert(sizeof(bmp_palette) == 1024);

    sprintf(path, "bmp/field-%08x.bmp", id);
    fp = fopen(path, "wb");
    if (fp == NULL) fatal("can't open %s for writing.", path);
    if (fwrite(&bmp_header, sizeof(bmp_header), 1, fp) != 1)
    {
        fatal("couldn't write BMP header");
    }
    if (fwrite(&bmp_palette, sizeof(bmp_palette), 1, fp) != 1)
    {
        fatal("couldn't write BMP palette data");
    }
    if (fwrite(g_field, 1000, 1000, fp) != 1000)
    {
        fatal("couldn't write BMP pixel data");
    }
    fclose(fp);
}

void debug_plot(double x, double y, int col)
{
    static const bool templ[7][7] = {
        { 0,0,1,1,1,0,0 },
        { 0,1,1,1,1,1,0 },
        { 1,1,1,1,1,1,1 },
        { 1,1,1,1,1,1,1 },
        { 1,1,1,1,1,1,1 },
        { 0,1,1,1,1,1,0 },
        { 0,0,1,1,1,0,0 } };

    int cx = 1000*x;
    int cy = 1000*y;

    for (int dx = -3; dx <= 3; ++dx)
    {
        for (int dy = -3; dy <= 3; ++dy)
        {
            if (!templ[dx + 3][dy + 3]) continue;
            int x = cx + dx, y = cy + dy;
            if (x >= 0 && x < 1000 && y >= 0 && y < 1000) g_field[y][x] ^= col;
        }
    }
}

void debug_reset_image()
{
    memset(g_field, 0, sizeof(g_field));
}
