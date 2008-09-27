#ifndef DEBUG_H_INCLUDED
#define DEBUG_H_INCLUDED

#include <stdlib.h>

#ifdef _MSC_VER
#define __attribute__(a)	
#endif

#ifdef __cplusplus
extern "C" {
#endif

void info(const char *fmt, ...);
void warn(const char *fmt, ...);
void error(const char *fmt, ...);
__attribute__((noreturn)) void fatal(const char *fmt, ...);

void hex_dump(unsigned char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ndef DEBUG_H_INCLUDED */
