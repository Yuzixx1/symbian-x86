/* Freestanding <stdlib.h> shim for the doomgeneric E32 port. */
#ifndef E32_DOOM_STDLIB_H
#define E32_DOOM_STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void exit(int status);
void abort(void);
int atexit(void (*func)(void));
int atoi(const char *nptr);
long atol(const char *nptr);
double atof(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
char *getenv(const char *name);
int abs(int j);
long labs(long j);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));
int rand(void);
void srand(unsigned seed);
int system(const char *cmd);
char *mkdtemp(char *template);
char *realpath(const char *path, char *resolved_path);

#endif /* E32_DOOM_STDLIB_H */
