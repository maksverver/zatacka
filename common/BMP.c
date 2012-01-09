#include "BMP.h"
#include "Colors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct BMP_RGB {
    unsigned char b, g, r, a;
};

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
} __attribute__((__packed__));

static struct BMP_RGB bmp_palette[256];

static void bmp_initialize()
{
    static int initialized = 0;
    int n;

    if (initialized) return;
    initialized = true;

    /* Initialize color palette using standard player colors */
    for (n = 1; n < 256; ++n)
    {
        bmp_palette[n].r = g_colors[(n - 1)%NUM_COLORS].r;
        bmp_palette[n].g = g_colors[(n - 1)%NUM_COLORS].g;
        bmp_palette[n].b = g_colors[(n - 1)%NUM_COLORS].b;
        bmp_palette[n].a = 0;
    }
}

bool bmp_write( const char *path, unsigned char *image,
                unsigned width, unsigned height )
{
    FILE *fp;
    struct BMP_Header bmp_header;

    bmp_initialize();

    /* Fill BMP header structure */
    memset(&bmp_header, 0, sizeof(bmp_header));
    bmp_header.bfType[0]    = 'B';
    bmp_header.bfType[1]    = 'M';
    bmp_header.bfSize       = 54 + width*height + 256*4;
    bmp_header.bfOffBits    = 54 + 1024;
    bmp_header.biSize       = 40;
    bmp_header.biWidth      = width;
    bmp_header.biHeight     = height;
    bmp_header.biPlanes     = 1;
    bmp_header.biBitCount   = 8;

    assert(sizeof(bmp_header) == 54);
    assert(sizeof(bmp_palette) == 1024);

    fp = fopen(path, "wb");
    if (fp == NULL) return false;

    if ( fwrite(&bmp_header, sizeof(bmp_header), 1, fp) != 1 ||
         fwrite(&bmp_palette, sizeof(bmp_palette), 1, fp) != 1 ||
         fwrite(image, width, height, fp) != height )
    {
        /* Writing failed */
        fclose(fp);
        return false;
    }

    fclose(fp);

    return true;
}
