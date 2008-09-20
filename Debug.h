#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

void info(const char *fmt, ...);
void warn(const char *fmt, ...);
void error(const char *fmt, ...);
__attribute__((noreturn)) void fatal(const char *fmt, ...);

void time_reset();
double time_now();

#ifdef __cplusplus
}
#endif

#endif /* ndef DEBUG_H_INCLUDED */
