#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include <assert.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void info(const char *fmt, ...);
void warn(const char *fmt, ...);
void error(const char *fmt, ...);
__attribute__((noreturn)) void fatal(const char *fmt, ...);

void time_reset();
double time_now();

void hex_dump(unsigned char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ndef DEBUG_H_INCLUDED */
