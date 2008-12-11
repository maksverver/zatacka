#include "Debug.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define INFO  0
#define WARN  1
#define ERROR 2
#define FATAL 3

/* Prints an error message followed by a newline character,
   prefixed by the time in seconds elapsed since time_reset() was last called,
   and displaying the type of error. */
void message(int severity, const char *fmt, va_list ap)
{
    switch (severity)
    {
    case WARN:  fprintf(stderr, "WARNING: "); break;
    case ERROR: fprintf(stderr, "ERROR: "); break;
    case FATAL: fprintf(stderr, "FATAL ERROR: "); break;
    }
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void info(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    message(INFO, fmt, ap);
    va_end(ap);
}

void warn(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    message(WARN, fmt, ap);
    va_end(ap);
}

void error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    message(ERROR, fmt, ap);
    va_end(ap);
}

void fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    message(FATAL, fmt, ap);
    va_end(ap);
    abort();
}

void hex_dump(unsigned char *buf, size_t len)
{
    size_t row, rows, i, n;

    rows = len/16 + (len%16 != 0);
    for (row = 0; row < rows; ++row)
    {
        printf("%04x  ", (int)(16*row));
        for (n = 0; n < 16; ++n)
        {
            i = 16*row + n;
            if (i < len)
            {
                printf("%02x ", buf[i]);
            }
            else
            {
                printf("   ");
            }
            if (n == 7) printf(" ");
        }
        printf("  ");
        for (n = 0; n < 16; ++n)
        {
            i = 16*row + n;
            if (i < len)
            {
                printf("%c", (buf[i] < 32 || buf[i] > 126 ? '.' : buf[i]));
            }
            else
            {
                printf("  ");
            }
            if (n == 8) printf(" ");
        }
        printf("\n");
    }
}
