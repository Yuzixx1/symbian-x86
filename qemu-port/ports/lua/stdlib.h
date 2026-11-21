/* Minimal freestanding <stdlib.h> for the Lua 5.4 E32 port. */
#ifndef QPORT_LUA_STDLIB_H
#define QPORT_LUA_STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
void *calloc(size_t nmemb, size_t size);

double strtod(const char *nptr, char **endptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
int atoi(const char *nptr);
long atol(const char *nptr);

void abort(void);
void exit(int status);

static __inline__ int abs(int x) { return x < 0 ? -x : x; }
static __inline__ long labs(long x) { return x < 0 ? -x : x; }
static __inline__ long long llabs(long long x) { return x < 0 ? -x : x; }

#endif /* QPORT_LUA_STDLIB_H */
