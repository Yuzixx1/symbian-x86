/* Freestanding <stdio.h> shim for the doomgeneric E32 port.
 * The engine only needs a small printf/scan subset plus FILE-based WAD I/O.
 * No hosted libc is linked: every declaration here is implemented in
 * ports/doom/e32_shim.c. */
#ifndef E32_DOOM_STDIO_H
#define E32_DOOM_STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct E32_FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define EOF (-1)
#define BUFSIZ 8192
#define FILENAME_MAX 260

int printf(const char *fmt, ...);
int fprintf(FILE *stream, const char *fmt, ...);
int sprintf(char *str, const char *fmt, ...);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);
int vfprintf(FILE *stream, const char *fmt, va_list ap);
int vprintf(const char *fmt, va_list ap);
int fflush(FILE *stream);
int puts(const char *s);
int putchar(int c);
int fputs(const char *s, FILE *stream);
int fputc(int c, FILE *stream);
int fgetc(FILE *stream);
int ungetc(int c, FILE *stream);
char *fgets(char *s, int n, FILE *stream);

int sscanf(const char *str, const char *format, ...);
int remove(const char *pathname);
int rename(const char *oldpath, const char *newpath);

FILE *fopen(const char *path, const char *mode);
FILE *freopen(const char *path, const char *mode, FILE *stream);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);
void perror(const char *s);
int fileno(FILE *stream);

/* Nonstandard helpers used by Chocolate Doom (declared here for the shim). */
int asprintf(char **ret, const char *fmt, ...);

#endif /* E32_DOOM_STDIO_H */
