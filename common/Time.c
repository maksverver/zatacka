#include "Time.h"
#include <stdlib.h>

#ifdef _MSC_VER
#include <time.h>
#else
#include <sys/time.h>
#endif


#ifdef _MSC_VER

static clock_t c_start = 0;

/* Resets the time counter */
void time_reset()
{
    c_start = clock();
}

double time_now()
{
    return (double)(clock() - c_start)/(double)CLOCKS_PER_SEC;
}

#else

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

#endif
