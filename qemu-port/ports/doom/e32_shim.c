/* e32_shim.c -- freestanding libc-compatible shims for the doomgeneric E32.
 *
 * Everything the engine's 62k lines actually reference is implemented here:
 *  - printf/fprintf/sprintf/snprintf/vsnprintf/vfprintf -> local formatter,
 *    serial output via print() (DebugPrint exec 86);
 *  - fopen/fclose/fread/fwrite/fseek/ftell/feof -> runtime.h file_open,
 *    file_size, file_read_at, file_seek, file_append, file_commit (the genuine
 *    PortBigFile streaming path, so a 28 MB WAD is readable);
 *  - string/memory/ctype/stdlib helpers implemented locally;
 *  - malloc/realloc/free/calloc come from user/malloc.c (linked separately);
 *  - sin/fabs are tiny local approximations (DOOM's renderer is fixed-point).
 *
 * Marker path: doom_marker() calls the PE2 EUSER.DLL ordinal-915 import when
 * the IAT slot was bound by E32Run, and always calls print() as a fallback.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "e32_shim.h"
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

/* PE2 IAT slot, defined with --defsym in user/user.mk and bound by
 * elf2e32.py --euser-import 915. */
extern void (*doom_printf_slot)(const char *);

int errno = 0;
int math_errhandling = 0;

/* ------------------------------------------------------------------ markers */
void doom_marker(const char *text)
{
    unsigned fn = *(volatile unsigned *)(void *)&doom_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char *))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

void doom_marker_n(const char *prefix, unsigned value)
{
    char buf[160];
    unsigned n = 0, d = 0;
    char digits[12];
    while (*prefix && n < sizeof(buf) - 13u) buf[n++] = *prefix++;
    if (!value) digits[d++] = '0';
    while (value) { digits[d++] = (char)('0' + value % 10u); value /= 10u; }
    while (d) buf[n++] = digits[--d];
    buf[n] = 0;
    doom_marker(buf);
}

void doom_marker_2(const char *prefix, unsigned a, unsigned b)
{
    char buf[192];
    unsigned n = 0, d = 0;
    char digits[12];
    unsigned vals[2];
    int k;
    vals[0] = a; vals[1] = b;
    while (*prefix && n < sizeof(buf) - 26u) buf[n++] = *prefix++;
    for (k = 0; k < 2; ++k)
        {
        unsigned v = vals[k];
        if (k) buf[n++] = ' ';
        d = 0;
        if (!v) digits[d++] = '0';
        while (v) { digits[d++] = (char)('0' + v % 10u); v /= 10u; }
        while (d) buf[n++] = digits[--d];
        }
    buf[n] = 0;
    doom_marker(buf);
}

void doom_shim_init(void) { errno = 0; }

/* ----------------------------------------------------------------- formatter */
struct fmt_out { char *buf; int cap; int used; };

static void fo_emit(struct fmt_out *o, char c)
{
    if (o->used + 1 < o->cap) o->buf[o->used] = c;
    ++o->used;
}

static void fo_pad(struct fmt_out *o, char c, int n)
{
    while (n-- > 0) fo_emit(o, c);
}

static char hexdigit(unsigned v) { return (char)(v < 10u ? '0' + v : 'a' + v - 10u); }

static void emit_number(struct fmt_out *o, unsigned long long value, unsigned base,
                        int upper, int width, int prec, int left, int zeropad,
                        int negative, int signchar)
{
    char tmp[80];
    int n = 0, pad, start = o->used, written, signlen;
    if (!value) tmp[n++] = '0';
    while (value)
        {
        unsigned d = (unsigned)(value % base);
        tmp[n++] = upper ? (char)(hexdigit(d) - ('a' - 'A')) : hexdigit(d);
        value /= base;
        }
    pad = prec > n ? prec - n : 0;
    signlen = (negative || signchar) ? 1 : 0;
    written = n + pad + signlen;
    if (!left && !zeropad && width > written) fo_pad(o, ' ', width - written);
    if (negative) fo_emit(o, '-');
    else if (signchar) fo_emit(o, '+');
    if (!left && zeropad && prec < 0 && width > written)
        fo_pad(o, '0', width - written);
    fo_pad(o, '0', pad);
    while (n) fo_emit(o, tmp[--n]);
    if (left && width > written) fo_pad(o, ' ', width - written);
    (void)start;
}

int e32_vformat(char *buf, int cap, const char *fmt, va_list ap)
{
    struct fmt_out o;
    if (!buf || cap <= 0) return 0;
    o.buf = buf; o.cap = cap; o.used = 0;
    buf[0] = 0;

    while (*fmt)
        {
        char c = *fmt++;
        int left = 0, zeropad = 0, signchar = 0;
        int width = 0, prec = -1;
        char length = 0;

        if (c != '%') { fo_emit(&o, c); continue; }
        c = *fmt++;
        if (!c) break;
        if (c == '%') { fo_emit(&o, '%'); continue; }

        for (;;)
            {
            if (c == '-') left = 1;
            else if (c == '0') zeropad = 1;
            else if (c == '+') signchar = 1;
            else if (c == ' ') { /* space flag: ignored */ }
            else if (c == '#') { /* alternate form: ignored */ }
            else break;
            c = *fmt++;
            }
        while (c >= '0' && c <= '9')
            { width = width * 10 + (c - '0'); c = *fmt++; }
        if (c == '.')
            {
            c = *fmt++;
            prec = 0;
            while (c >= '0' && c <= '9')
                { prec = prec * 10 + (c - '0'); c = *fmt++; }
            }
        if (c == 'l')
            { length = 'l'; c = *fmt++; if (c == 'l') { length = 'L'; c = *fmt++; } }
        else if (c == 'z' || c == 't')
            { length = 'z'; c = *fmt++; }

        switch (c)
            {
        case 'd': case 'i':
            {
            long long v;
            if (length == 'L') v = va_arg(ap, long long);
            else if (length == 'l') v = va_arg(ap, long);
            else if (length == 'z') v = va_arg(ap, int);
            else v = va_arg(ap, int);
            int neg = v < 0;
            unsigned long long uv = neg ? (unsigned long long)(-(v + 1)) + 1u
                                        : (unsigned long long)v;
            emit_number(&o, uv, 10u, 0, width, prec, left, zeropad, neg, signchar);
            break;
            }
        case 'u':
            {
            unsigned long long v = length == 'L' ? va_arg(ap, unsigned long long)
                : length == 'l' ? va_arg(ap, unsigned long)
                : (unsigned long long)va_arg(ap, unsigned int);
            emit_number(&o, v, 10u, 0, width, prec, left, zeropad, 0, 0);
            break;
            }
        case 'x': case 'X':
            {
            unsigned long long v = length == 'L' ? va_arg(ap, unsigned long long)
                : length == 'l' ? va_arg(ap, unsigned long)
                : (unsigned long long)va_arg(ap, unsigned int);
            emit_number(&o, v, 16u, c == 'X', width, prec, left, zeropad, 0, 0);
            break;
            }
        case 'o':
            {
            unsigned long long v = length == 'l' ? va_arg(ap, unsigned long)
                : (unsigned long long)va_arg(ap, unsigned int);
            emit_number(&o, v, 8u, 0, width, prec, left, zeropad, 0, 0);
            break;
            }
        case 'c':
            { char ch = (char)va_arg(ap, int); fo_emit(&o, ch); break; }
        case 's':
            {
            const char *s = va_arg(ap, const char *);
            int n = 0, start = o.used;
            if (!s) s = "(null)";
            while (s[n] && (prec < 0 || n < prec)) ++n;
            if (!left && width > n) fo_pad(&o, ' ', width - n);
            while (n--) fo_emit(&o, *s++);
            if (left && width > o.used - start) fo_pad(&o, ' ', width - (o.used - start));
            break;
            }
        case 'p':
            {
            unsigned v = (unsigned)(uintptr_t)va_arg(ap, void *);
            fo_emit(&o, '0'); fo_emit(&o, 'x');
            emit_number(&o, v, 16u, 0, 0, -1, 0, 0, 0, 0);
            break;
            }
        default:
            fo_emit(&o, c);
            break;
            }
        }

    if (o.used >= o.cap) o.buf[o.cap - 1] = 0;
    else o.buf[o.used] = 0;
    return o.used;
}

/* ------------------------------------------------------------------- stdio */
static void serial_line(const char *text)
{
    char buf[600];
    unsigned n = 0;
    if (!text) text = "";
    while (*text && n < sizeof(buf) - 2u)
        {
        if (*text == '\r') { ++text; continue; }
        buf[n++] = *text++;
        }
    while (n && buf[n - 1] == '\n') --n;   /* print() supplies the newline */
    buf[n] = 0;
    print(buf);
}

int printf(const char *fmt, ...)
{
    char buf[600];
    va_list ap; int r;
    va_start(ap, fmt);
    r = e32_vformat(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    serial_line(buf);
    return r;
}

int vprintf(const char *fmt, va_list ap)
{
    char buf[600];
    int r = e32_vformat(buf, sizeof(buf), fmt, ap);
    serial_line(buf);
    return r;
}

int vfprintf(FILE *stream, const char *fmt, va_list ap)
{
    char buf[600];
    int r = e32_vformat(buf, sizeof(buf), fmt, ap);
    if (!stream || stream == stdout || stream == stderr || stream == stdin)
        serial_line(buf);
    else
        fwrite(buf, 1, (size_t)(r < (int)sizeof(buf) ? r : (int)sizeof(buf)), stream);
    return r;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt);
    r = vfprintf(stream, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *str, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt);
    r = e32_vformat(str, 0x7fffffff, fmt, ap);
    va_end(ap);
    return r;
}

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt);
    r = e32_vformat(str, (int)size, fmt, ap);
    va_end(ap);
    return r;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    return e32_vformat(str, (int)size, fmt, ap);
}

int puts(const char *s) { serial_line(s); return 0; }
int putchar(int c)
{
    char b[2];
    b[0] = (char)c; b[1] = 0;
    serial_line(b);
    return c;
}
int fflush(FILE *stream) { (void)stream; return 0; }
void perror(const char *s) { if (s) { char b[160]; unsigned n = 0;
    while (*s && n < sizeof(b) - 20u) b[n++] = *s++;
    b[n++] = ':';
    const char *m = strerror(errno);
    while (*m && n < sizeof(b) - 2u) b[n++] = *m++;
    b[n] = 0; serial_line(b); } }

/* ---------------------------------------------------------------- FILE layer */
struct E32_FILE {
    int handle;             /* runtime.h logical channel, -1 when closed */
    unsigned pos;           /* current byte offset */
    unsigned size;          /* file length */
    int eof, err;
    int writable;
};

static FILE std_in  = { -1, 0, 0, 0, 0, 0 };
static FILE std_out = { -1, 0, 0, 0, 0, 0 };
static FILE std_err = { -1, 0, 0, 0, 0, 0 };
FILE *stdin = &std_in;
FILE *stdout = &std_out;
FILE *stderr = &std_err;

static int mode_write(const char *mode)
{
    return mode && (mode[0] == 'w' || mode[0] == 'a' || mode[0] == '+');
}

/* The runtime parser (DPortBigFile::OpenPath) accepts both '/' and '\', and
 * a "X:" prefix selects that mounted volume.  DOOM's d_iwad.c, however,
 * joins directory strings with DIR_SEPARATOR_S ('/'), so a Windows-style
 * "X:\FDOOM1.WAD" can come back as a mixed "X:\/FDOOM1.WAD".  Fold every
 * separator to '/' and collapse duplicates before handing the name to the
 * runtime.  A drive-qualified absolute path then reaches R1Fat::Open with
 * the root-relative leaf the FAT lookup expects. */
static void e32_norm_path(const char *src, char *dst, unsigned cap)
{
    unsigned n = 0;
    if (!src || !cap) { if (cap) dst[0] = 0; return; }
    while (*src && n + 1u < cap)
        {
        char c = *src++;
        if (c == '\\') c = '/';
        if (c == '/' && n && dst[n - 1u] == '/') continue;
        dst[n++] = c;
        }
    dst[n] = 0;
}

FILE *fopen(const char *path, const char *mode)
{
    int h;
    FILE *f;
    char norm[260];
    if (!path || !mode) { errno = EINVAL; return 0; }
    e32_norm_path(path, norm, sizeof(norm));
    h = mode_write(mode) ? file_create_write(norm) : file_open(norm);
    if (h < 0) { errno = ENOENT; return 0; }
    f = (FILE *)malloc(sizeof(struct E32_FILE));
    if (!f) { file_close(h); errno = ENOMEM; return 0; }
    f->handle = h;
    f->pos = 0;
    f->size = mode_write(mode) ? 0u : (unsigned)file_size(h);
    f->eof = f->err = 0;
    f->writable = mode_write(mode);
    if (mode[0] == 'a') f->pos = f->size;
    return f;
}

FILE *freopen(const char *path, const char *mode, FILE *stream)
{
    if (stream && stream != stdin && stream != stdout && stream != stderr)
        fclose(stream);
    return fopen(path, mode);
}

int fclose(FILE *stream)
{
    if (!stream || stream == stdin || stream == stdout || stream == stderr)
        return 0;
    if (stream->handle >= 0)
        {
        if (stream->writable) file_commit(stream->handle);
        file_close(stream->handle);
        stream->handle = -1;
        }
    free(stream);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    unsigned want, got;
    if (!stream || stream->handle < 0 || !ptr) return 0;
    want = (unsigned)(size * nmemb);
    if (!want) return 0;
    got = (unsigned)file_read_at(stream->handle, stream->pos, ptr, want);
    if (got == (unsigned)-1 || (int)got < 0) { stream->err = 1; return 0; }
    stream->pos += got;
    if (got < want) stream->eof = 1;
    return size ? got / (unsigned)size : 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    unsigned total;
    if (!stream || !ptr || !stream->writable) return 0;
    total = (unsigned)(size * nmemb);
    if (!total) return 0;
    if (file_append(stream->handle, ptr, total) < 0) { stream->err = 1; return 0; }
    stream->pos += total;
    return nmemb;
}

int fseek(FILE *stream, long offset, int whence)
{
    if (!stream) return -1;
    switch (whence)
        {
    case SEEK_SET: break;
    case SEEK_CUR: offset += (long)stream->pos; break;
    case SEEK_END: offset += (long)stream->size; break;
    default: return -1;
        }
    if (offset < 0) return -1;
    stream->pos = (unsigned)offset;
    stream->eof = 0;
    if (!stream->writable) file_seek(stream->handle, stream->pos);
    return 0;
}

long ftell(FILE *stream) { return stream ? (long)stream->pos : -1L; }
void rewind(FILE *stream) { if (stream) { stream->pos = 0; stream->eof = 0; } }
int feof(FILE *stream) { return stream ? stream->eof : 0; }
int ferror(FILE *stream) { return stream ? stream->err : 0; }
void clearerr(FILE *stream) { if (stream) { stream->eof = stream->err = 0; } }
int fileno(FILE *stream) { return stream ? stream->handle : -1; }

int fgetc(FILE *stream)
{
    unsigned char c;
    if (fread(&c, 1, 1, stream) != 1) return EOF;
    return c;
}
int ungetc(int c, FILE *stream)
{
    if (!stream || c == EOF || stream->pos == 0) return EOF;
    --stream->pos;
    stream->eof = 0;
    return c;
}
char *fgets(char *s, int n, FILE *stream)
{
    int i = 0;
    if (!s || n <= 0) return 0;
    while (i < n - 1)
        {
        int c = fgetc(stream);
        if (c == EOF) break;
        s[i++] = (char)c;
        if (c == '\n') break;
        }
    if (!i) return 0;
    s[i] = 0;
    return s;
}
int fputs(const char *s, FILE *stream)
{
    if (!stream || stream == stdout || stream == stderr || stream == stdin)
        { serial_line(s); return 0; }
    { unsigned n = 0; while (s && s[n]) ++n; fwrite(s, 1, n, stream); }
    return 0;
}
int fputc(int c, FILE *stream)
{
    char b[2]; b[0] = (char)c; b[1] = 0;
    return fputs(b, stream);
}
int asprintf(char **ret, const char *fmt, ...)
{
    char buf[1024];
    va_list ap; int r;
    va_start(ap, fmt);
    r = e32_vformat(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    *ret = strdup(buf);
    return r;
}

/* ------------------------------------------------------------------ stdlib */
void exit(int status)
{
    doom_marker("DOOM: DONE");
    process_exit(status);
}
void abort(void) { exit(-1); }
int atexit(void (*func)(void)) { (void)func; return 0; }

static int digitval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

long strtol(const char *nptr, char **endptr, int base)
{
    const char *p = nptr;
    unsigned long v = 0;
    int neg = 0, any = 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    if (*p == '+' || *p == '-') { neg = *p == '-'; ++p; }
    if (!base) base = (*p == '0') ? 8 : 10;
    if (base == 16 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { p += 2; }
    for (;;)
        {
        int d = digitval((unsigned char)*p);
        if (d < 0 || d >= base) break;
        v = v * (unsigned long)base + (unsigned long)d;
        ++p; any = 1;
        }
    if (endptr) *endptr = (char *)(any ? p : nptr);
    return neg ? -(long)v : (long)v;
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    return (unsigned long)strtol(nptr, endptr, base);
}
int atoi(const char *s) { return (int)strtol(s, 0, 10); }
long atol(const char *s) { return strtol(s, 0, 10); }
int abs(int j) { return j < 0 ? -j : j; }
long labs(long j) { return j < 0 ? -j : j; }
div_t div(int a, int b) { div_t r; r.quot = a / b; r.rem = a % b; return r; }
ldiv_t ldiv(long a, long b) { ldiv_t r; r.quot = a / b; r.rem = a % b; return r; }

char *getenv(const char *name)
{
    if (!name) return 0;
    if (!strcmp(name, "DOOMWADDIR") || !strcmp(name, "DOOMWADPATH") ||
        !strcmp(name, "HOME") || !strcmp(name, "TEMP") || !strcmp(name, "TMP"))
        return (char *)"X:/";   /* the staged WAD lives on the X: volume */
    if (!strcmp(name, "USER") || !strcmp(name, "USERNAME"))
        return (char *)"doom";
    return 0;
}

int system(const char *cmd) { (void)cmd; return -1; }
double atof(const char *s)
{
    double v = 0.0;
    int sign = 1;
    while (*s == ' ') ++s;
    if (*s == '-') { sign = -1; ++s; } else if (*s == '+') ++s;
    while (*s >= '0' && *s <= '9') { v = v * 10.0 + (double)(*s - '0'); ++s; }
    if (*s == '.')
        {
        double f = 0.1;
        ++s;
        while (*s >= '0' && *s <= '9') { v += f * (double)(*s - '0'); f *= 0.1; ++s; }
        }
    return sign < 0 ? -v : v;
}

static unsigned s_rand = 1;
int rand(void) { s_rand = s_rand * 1103515245u + 12345u; return (int)((s_rand >> 1) & RAND_MAX); }
void srand(unsigned seed) { s_rand = seed ? seed : 1u; }

void qsort(void *base, size_t n, size_t sz,
           int (*cmp)(const void *, const void *))
{
    unsigned char *a = (unsigned char *)base;
    size_t i, j;
    for (i = 1; i < n; ++i)
        {
        unsigned char *key = a + i * sz;
        j = i;
        while (j > 0 && cmp(a + (j - 1) * sz, key) > 0)
            {
            size_t k;
            for (k = 0; k < sz; ++k)
                { unsigned char t = a[j * sz + k];
                  a[j * sz + k] = a[(j - 1) * sz + k];
                  a[(j - 1) * sz + k] = t; }
            --j;
            }
        }
}

void *bsearch(const void *key, const void *base, size_t n, size_t sz,
              int (*cmp)(const void *, const void *))
{
    const unsigned char *a = (const unsigned char *)base;
    while (n)
        {
        size_t mid = n >> 1;
        int c = cmp(key, a + mid * sz);
        if (!c) return (void *)(a + mid * sz);
        if (c < 0) n = mid;
        else { a += (mid + 1) * sz; n -= mid + 1; }
        }
    return 0;
}

/* -------------------------------------------------------- string/memory/ctype */
void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}
void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) while (n--) *d++ = *s++;
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dst;
}
void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    unsigned char v = (unsigned char)c;
    while (n--) *d++ = v;
    return dst;
}
int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    while (n--)
        {
        if (*x != *y) return (int)*x - (int)*y;
        ++x; ++y;
        }
    return 0;
}
void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;
    while (n--) if (*p == (unsigned char)c) return (void *)p; else ++p;
    return 0;
}
size_t strlen(const char *s) { size_t n = 0; while (s[n]) ++n; return n; }
size_t strnlen(const char *s, size_t m) { size_t n = 0; while (n < m && s[n]) ++n; return n; }
char *strcpy(char *d, const char *s) { while ((*d++ = *s++) != 0) {} return d; }
char *strncpy(char *d, const char *s, size_t n)
{
    size_t i = 0;
    for (; i < n && s[i]; ++i) d[i] = s[i];
    while (i < n) d[i++] = 0;
    return d;
}
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }
char *strncat(char *d, const char *s, size_t n)
{
    char *p = d + strlen(d);
    while (n-- && *s) *p++ = *s++;
    *p = 0;
    return d;
}
int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}
int strncmp(const char *a, const char *b, size_t n)
{
    while (n--)
        {
        if (*a != *b) return (int)(unsigned char)*a - (int)(unsigned char)*b;
        if (!*a) return 0;
        ++a; ++b;
        }
    return 0;
}
char *strchr(const char *s, int c)
{
    while (*s) { if (*s == (char)c) return (char *)s; ++s; }
    return c == 0 ? (char *)s : 0;
}
char *strrchr(const char *s, int c)
{
    const char *p = s + strlen(s);
    while (p >= s) { if (*p == (char)c) return (char *)p; --p; }
    return 0;
}
char *strstr(const char *h, const char *n)
{
    size_t nl = strlen(n);
    while (*h) { if (!strncmp(h, n, nl)) return (char *)h; ++h; }
    return 0;
}
char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc((unsigned)n);
    if (p) memcpy(p, s, n);
    return p;
}
char *strndup(const char *s, size_t n)
{
    char *p = (char *)malloc((unsigned)n + 1u);
    if (p) { memcpy(p, s, n); p[n] = 0; }
    return p;
}
char *strerror(int e)
{
    switch (e)
        {
    case ENOENT: return (char *)"no such file";
    case ENOMEM: return (char *)"no memory";
    case EACCES: return (char *)"access denied";
    case EINVAL: return (char *)"bad argument";
    case EISDIR: return (char *)"is a directory";
    case ETIMEDOUT: return (char *)"timed out";
    default: return (char *)"error";
        }
}
static int lowerchar(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
int strcasecmp(const char *a, const char *b)
{
    for (;;)
        {
        int x = lowerchar((unsigned char)*a), y = lowerchar((unsigned char)*b);
        if (x != y) return x - y;
        if (!*a) return 0;
        ++a; ++b;
        }
}
int strncasecmp(const char *a, const char *b, size_t n)
{
    while (n--)
        {
        int x = lowerchar((unsigned char)*a), y = lowerchar((unsigned char)*b);
        if (x != y) return x - y;
        if (!*a) return 0;
        ++a; ++b;
        }
    return 0;
}
char *strpbrk(const char *s, const char *a)
{
    while (*s) { const char *p = a; while (*p) { if (*p == *s) return (char *)s; ++p; } ++s; }
    return 0;
}
size_t strspn(const char *s, const char *a)
{
    size_t n = 0;
    while (s[n]) { const char *p = a; while (*p && *p != s[n]) ++p; if (!*p) break; ++n; }
    return n;
}
size_t strcspn(const char *s, const char *a)
{
    size_t n = 0;
    while (s[n]) { const char *p = a; while (*p && *p != s[n]) ++p; if (*p) break; ++n; }
    return n;
}
char *strtok(char *s, const char *delim)
{
    static char *save;
    char *start;
    if (s) save = s;
    if (!save) return 0;
    start = save + strspn(save, delim);
    if (!*start) { save = 0; return 0; }
    save = start + strcspn(start, delim);
    if (*save) *save++ = 0;
    else save = 0;
    return start;
}
int bcmp(const void *a, const void *b, size_t n) { return memcmp(a, b, n); }
void bcopy(const void *s, void *d, size_t n) { memmove(d, s, n); }
void bzero(void *s, size_t n) { memset(s, 0, n); }

int isalnum(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isprint(int c) { return c >= 32 && c <= 126; }
int isgraph(int c) { return c > 32 && c <= 126; }
int ispunct(int c) { return isprint(c) && !isalnum(c) && c != ' '; }
int iscntrl(int c) { return c < 32 || c == 127; }
int tolower(int c) { return isupper(c) ? c + 32 : c; }
int toupper(int c) { return islower(c) ? c - 32 : c; }

/* ------------------------------------------------------------------- sscanf */
int sscanf(const char *str, const char *fmt, ...)
{
    va_list ap;
    const char *f = fmt;
    int count = 0;
    va_start(ap, fmt);
    while (*str && *f)
        {
        if (*f == ' ')
            { while (*f == ' ') ++f; while (*str == ' ' || *str == '\t') ++str; continue; }
        if (*f != '%')
            { if (*f != *str) break; ++f; ++str; continue; }
        ++f;
        if (*f == 'x' || *f == 'X' || *f == 'd' || *f == 'i' || *f == 'o')
            {
            char conv = *f++;
            int base = 10;
            int *out = va_arg(ap, int *);
            int neg = 0;
            unsigned long v = 0;
            while (*str == ' ' || *str == '\t') ++str;
            if (*str == '+' || *str == '-') { neg = *str == '-'; ++str; }
            if (conv == 'x' || conv == 'X') base = 16;
            else if (conv == 'o') base = 8;
            else if (conv == 'i' && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
                { base = 16; str += 2; }
            else if (conv == 'i' && str[0] == '0') base = 8;
            for (;;)
                {
                int d = digitval((unsigned char)*str);
                if (d < 0 || d >= base) break;
                v = v * (unsigned long)base + (unsigned long)d;
                ++str;
                }
            *out = neg ? -(int)v : (int)v;
            ++count;
            }
        else break;
        }
    va_end(ap);
    return count;
}

/* ------------------------------------------------------------------- libm-lite */
double fabs(double x) { return x < 0.0 ? -x : x; }
float fabsf(float x) { return x < 0.0f ? -x : x; }
/* Unused by the active renderer (finesine tables come from tables.c), but
 * provided so a stray reference still links.  Range-reduced cubic. */
double sin(double x)
{
    double y, y2;
    int k;
    while (x > 3.14159265358979323846) x -= 6.28318530717958647692;
    while (x < -3.14159265358979323846) x += 6.28318530717958647692;
    y = x;
    y2 = y * y;
    k = 1;
    { double term = y, sum = y;
      for (; k < 6; ++k)
          { term *= -y2 / (double)((2 * k) * (2 * k + 1)); sum += term; }
      return sum; }
}
float sinf(float x) { return (float)sin((double)x); }
double cos(double x) { return sin(x + 1.57079632679489661923); }
double tan(double x) { return sin(x) / cos(x); }
double sqrt(double x)
{
    double g = x > 0.0 ? x : 1.0;
    int i;
    if (x <= 0.0) return 0.0;
    for (i = 0; i < 16; ++i) g = 0.5 * (g + x / g);
    return g;
}
double floor(double x) { int i = (int)x; return (x < 0.0 && x != (double)i) ? (double)(i - 1) : (double)i; }
double ceil(double x) { int i = (int)x; return (x > 0.0 && x != (double)i) ? (double)(i + 1) : (double)i; }
double pow(double x, double y) { (void)x; (void)y; return 1.0; }
double atan2(double y, double x) { (void)y; (void)x; return 0.0; }
double fmod(double x, double y) { while (x >= y) x -= y; return x; }
double log(double x) { (void)x; return 0.0; }
double exp(double x) { (void)x; return 1.0; }

/* --------------------------------------------------------------- misc libc */
int access(const char *p, int m) { FILE *f = fopen(p, "r"); (void)m; if (!f) return -1; fclose(f); return 0; }
int mkdir(const char *p, unsigned mode) { (void)p; (void)mode; return 0; }  /* read-only X: */
int remove(const char *p) { (void)p; return -1; }
int rename(const char *a, const char *b) { (void)a; (void)b; return -1; }
int isatty(int fd) { (void)fd; return 0; }
int stat(const char *p, struct stat *s)
{
    FILE *f = fopen(p, "r");
    if (!f) return -1;
    if (s) { s->st_size = (long)f->size; s->st_mode = 0100000; }
    fclose(f);
    return 0;
}
char *realpath(const char *path, char *resolved) { if (resolved) { strcpy(resolved, path); return resolved; } return 0; }

/* ------------------------------------------------------------------ DOOM heap
 * DOOM's zone allocator asks for 6 MiB and the RGBA frame another 1 MiB.  The
 * generic 4 MiB user heap cannot hold that, so the E32 builds its own private
 * RChunk-backed first-fit allocator here (genuine EExec 107/108, same design
 * as user/malloc.c but capped at 16 MiB).  user/malloc.o is deliberately NOT
 * linked into DOOM.E32 to avoid duplicate malloc/free symbols.
 */
#define DHEAP_ALIGN 8u
#define DHEAP_INITIAL (64u * 1024u)
#define DHEAP_STEP (64u * 1024u)
#define DHEAP_MAX (16u * 1024u * 1024u)

struct dblk { unsigned size; struct dblk *next; };
static struct dblk *d_free_list;
static int d_heap_handle = -1;
static unsigned char *d_heap_base;
static unsigned d_heap_size;

static unsigned d_align(unsigned n) { return (n + DHEAP_ALIGN - 1u) & ~(DHEAP_ALIGN - 1u); }

static void d_insert(struct dblk *b)
{
    struct dblk **prev = &d_free_list;
    while (*prev && *prev < b) prev = &(*prev)->next;
    b->next = *prev;
    *prev = b;
    if (b->next && (unsigned char *)b + b->size == (unsigned char *)b->next)
        { b->size += b->next->size; b->next = b->next->next; }
    {
    struct dblk *p = d_free_list;
    while (p && p->next != b) p = p->next;
    if (p && p != b && (unsigned char *)p + p->size == (unsigned char *)b)
        { p->size += b->size; p->next = b->next; }
    }
}

static void d_add_span(unsigned char *start, unsigned len)
{
    struct dblk *b = (struct dblk *)start;
    b->size = len;
    d_insert(b);
}

static int d_init(void)
{
    unsigned base, aligned, size;
    int h;
    unsigned caps[3] = { DHEAP_MAX, 8u * 1024u * 1024u, 4u * 1024u * 1024u };
    unsigned k;
    for (k = 0; k < 3; ++k)
        {
        h = chunk_create(0, DHEAP_INITIAL, caps[k]);
        if (h >= 0 && chunk_size(h) >= (int)(sizeof(struct dblk) + DHEAP_ALIGN))
            break;
        if (h >= 0) chunk_adjust(h, 0);       /* release the undersized chunk */
        h = -1;
        }
    if (h < 0) return 0;
    size = (unsigned)chunk_size(h);
    if (size < sizeof(struct dblk) + DHEAP_ALIGN) return 0;
    base = (unsigned)chunk_base(h);
    aligned = d_align(base);
    if (!aligned || aligned - base + sizeof(struct dblk) + DHEAP_ALIGN > size) return 0;
    d_heap_handle = h;
    d_heap_base = (unsigned char *)aligned;
    d_heap_size = size - (aligned - base);
    d_add_span(d_heap_base, d_heap_size);
    return 1;
}

static int d_grow(unsigned need)
{
    unsigned target = d_heap_size;
    while (target - d_heap_size < need && target < DHEAP_MAX) target += DHEAP_STEP;
    if (target > DHEAP_MAX) target = DHEAP_MAX;
    if (target <= d_heap_size) return 0;
    if (chunk_adjust(d_heap_handle, target) != 0) return 0;
    d_add_span(d_heap_base + d_heap_size, target - d_heap_size);
    d_heap_size = target;
    return 1;
}

void *malloc(size_t size)
{
    unsigned want;
    if (!size) size = 1;
    want = d_align((unsigned)size + (unsigned)sizeof(struct dblk));
    if (want > DHEAP_MAX - sizeof(struct dblk)) return 0;
    if (!d_free_list && !d_init()) return 0;
    for (;;)
        {
        struct dblk **prev = &d_free_list;
        while (*prev)
            {
            struct dblk *b = *prev;
            if (b->size >= want)
                {
                if (b->size - want >= sizeof(struct dblk) + DHEAP_ALIGN)
                    {
                    struct dblk *rest = (struct dblk *)((unsigned char *)b + want);
                    rest->size = b->size - want;
                    rest->next = b->next;
                    b->size = want;
                    *prev = rest;
                    }
                else *prev = b->next;
                return (unsigned char *)b + sizeof(struct dblk);
                }
            prev = &b->next;
            }
        if (!d_grow(want)) return 0;
        }
}

static struct dblk *d_block_of(void *ptr)
{
    struct dblk *b;
    if (!ptr) return 0;
    b = (struct dblk *)((unsigned char *)ptr - sizeof(struct dblk));
    if ((unsigned char *)b < d_heap_base ||
        (unsigned char *)b >= d_heap_base + d_heap_size ||
        ((unsigned)ptr & (DHEAP_ALIGN - 1u)) ||
        b->size < sizeof(struct dblk) + DHEAP_ALIGN || b->size > d_heap_size)
        return 0;
    return b;
}

void free(void *ptr)
{
    struct dblk *b = d_block_of(ptr);
    if (b) d_insert(b);
}

void *calloc(size_t count, size_t size)
{
    unsigned total;
    unsigned char *p;
    if (size && count > DHEAP_MAX / size) return 0;
    total = (unsigned)(count * size);
    p = (unsigned char *)malloc(total ? total : 1u);
    if (!p) return 0;
    { unsigned i; for (i = 0; i < total; ++i) p[i] = 0; }
    return p;
}

void *realloc(void *ptr, size_t new_size)
{
    struct dblk *b;
    unsigned old_size, copy, i;
    unsigned char *p;
    if (!ptr) return malloc(new_size);
    if (!new_size) { free(ptr); return 0; }
    b = d_block_of(ptr);
    if (!b) return 0;
    old_size = b->size - (unsigned)sizeof(struct dblk);
    if ((unsigned)new_size <= old_size) return ptr;
    p = (unsigned char *)malloc(new_size);
    if (!p) return 0;
    copy = old_size < (unsigned)new_size ? old_size : (unsigned)new_size;
    for (i = 0; i < copy; ++i) p[i] = ((unsigned char *)ptr)[i];
    free(ptr);
    return p;
}
