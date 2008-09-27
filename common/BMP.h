#ifndef BMP_H_INCLUDED
#define BMP_H_INCLUDED

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Dumps an 8-bit bitmap image to the given file path.
   Returns wether or not the bitmap could be written succesfully. */
bool bmp_write( const char *path, unsigned char *image,
                unsigned width, unsigned height );

#ifdef __cplusplus
}
#endif

#endif /* ndef BMP_H_INCLUDED */
