/* BSD string compatibility: only case-insensitive compares are used. */
#ifndef E32_DOOM_STRINGS_H
#define E32_DOOM_STRINGS_H

#include <stddef.h>

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
int bcmp(const void *a, const void *b, size_t n);
void bcopy(const void *src, void *dest, size_t n);
void bzero(void *s, size_t n);

#endif /* E32_DOOM_STRINGS_H */
