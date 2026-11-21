/* Minimal freestanding <stdio.h> for the Lua 5.4 E32 port.
 * No hosted FILE machinery: the only stream Lua actually uses is the
 * line-buffered debug console (see e32_shim.h).  The FILE entry points below
 * exist so lauxlib.c's unused luaL_loadfilex/luaL_fileresult code links.
 */
#ifndef QPORT_LUA_STDIO_H
#define QPORT_LUA_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct { int fd; } FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define EOF (-1)
#define BUFSIZ 512

int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int sprintf(char *str, const char *format, ...);
int vsprintf(char *str, const char *format, va_list ap);
int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int fputs(const char *s, FILE *stream);
int puts(const char *s);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
FILE *fopen(const char *path, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int fclose(FILE *stream);
int fflush(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
int getc(FILE *stream);
char *strerror(int errnum);

#endif /* QPORT_LUA_STDIO_H */
