#include "BMP.h"
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

static struct BMP_RGB bmp_palette[256] = {
    {    0,   0,   0,   0 },
    {    0,   0, 255,   0 },
    {    0, 255,   0,   0 },
    {  255,   0,   0,   0 },
    {    0, 255, 255,   0 },
    {  255,   0, 255,   0 },
    {  255, 255,   0,   0 },
    {  255, 255, 255,   0 } };


bool bmp_write( const char *path, unsigned char *image,
                unsigned width, unsigned height )
{
    FILE *fp;
    struct BMP_Header bmp_header;

    /* Fill BMP header structure */
    memset(&bmp_header, 0, sizeof(bmp_header));
    bmp_header.bfType[0]    = 'B';
    bmp_header.bfType[1]    = 'M';
    bmp_header.bfSize       = 54 + width*height+ 256*4;
    bmp_header.bfOffBits    = 54 + 1024;
    bmp_header.biSize       = 40;
    bmp_header.biWidth      = width;
    bmp_header.biHeight     = height;
    bmp_header.biPlanes     = 1;
    bmp_header.biBitCount   = 8;

    memset(&bmp_header, 0, sizeof(bmp_header));

    bmp_header.bfType[0] = 'B';
    bmp_header.bfType[1] = 'M';
    bmp_header.bfSize = 54 + width*height + 256*4;
    bmp_header.bfOffBits = 54 + 1024;
    bmp_header.biSize = 40;
    bmp_header.biWidth = width;
    bmp_header.biHeight = height;
    bmp_header.biPlanes = 1;
    bmp_header.biBitCount = 8;

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
