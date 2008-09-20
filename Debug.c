#include "Debug.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define INFO  0
#define WARN  1
#define ERROR 2
#define FATAL 3

/* Prints an error message followed by a newline character,
   prefixed by the time in seconds elapsed since time_reset() was last called,
   and displaying the type of error. */
void message(int severity, const char *fmt, va_list ap)
{
    fprintf(stderr, "[%7.3f] ", time_now());
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

static struct timeval tv_start = { 0, 0 };

/* Resets the time counter */
void time_reset()
{
    (void)gettimeofday(&tv_start, NULL);
}

/* Returns the number of seconds since the last reset of the time counter */
double time_now()
{
    struct timeval tv;

    (void)gettimeofday(&tv, NULL);
    return (tv.tv_sec - tv_start.tv_sec) + 1e-6*(tv.tv_usec - tv_start.tv_usec);
}
