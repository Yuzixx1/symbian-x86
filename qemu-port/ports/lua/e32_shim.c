/* e32_shim.c -- freestanding C runtime shims for Lua 5.4.6 on PE2/E32.
 *
 * Everything here is either a tiny freestanding replacement for a libc
 * primitive, an x87 math routine, or the bridge from Lua's stdout macros to
 * user/runtime.h print_n() (EUSER ordinal 915).  No hosted libc is linked.
 */
#include "e32_shim.h"

#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "runtime.h"

int errno = 0;
static FILE e32_stdin_obj, e32_stdout_obj, e32_stderr_obj;
FILE *stdin = &e32_stdin_obj;
FILE *stdout = &e32_stdout_obj;
FILE *stderr = &e32_stderr_obj;

/* ------------------------------------------------------------------ */
/* Line-buffered console output.  DebugPrint appends CR/LF to every call, */
/* so each print_n() carries exactly one line of Lua text.                */
/* ------------------------------------------------------------------ */
#define OUT_CAP 2048
static char out_buf[OUT_CAP];
static unsigned out_len;

void e32_lua_write(const char *s, size_t n)
{
    while (n--)
        {
        char c = *s++;
        if (out_len < OUT_CAP - 1) out_buf[out_len++] = c;
        else if (c == '\n')
            {
            print_n(out_buf, out_len);
            out_len = 0;
            }
        }
}

void e32_lua_flush_lines(void)
{
    unsigned start = 0, i;
    for (i = 0; i < out_len; ++i)
        if (out_buf[i] == '\n')
            {
            unsigned len = i - start;
            if (len) print_n(out_buf + start, len);
            else print_n("", 0);
            start = i + 1;
            }
    if (start)
        {
        out_len -= start;
        if (out_len) for (i = 0; i < out_len; ++i) out_buf[i] = out_buf[start + i];
        }
}

void e32_lua_flush(void)
{
    e32_lua_flush_lines();
    if (out_len)
        {
        print_n(out_buf, out_len);
        out_len = 0;
        }
}

void e32_lua_reset(void) { out_len = 0; }

/* ------------------------------------------------------------------ */
/* Lua allocator: the genuine chunk-backed user heap in user/malloc.c.   */
/* ------------------------------------------------------------------ */
void *e32_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud; (void)osize;
    if (nsize == 0) { free(ptr); return NULL; }
    if (ptr == NULL) return malloc(nsize);
    {
    void *p = malloc(nsize);
    if (p)
        {
        size_t n = osize < nsize ? osize : nsize;
        /* osize is supplied by Lua and is the old block size. */
        memcpy(p, ptr, n);
        free(ptr);
        }
    return p;
    }
}

/* ------------------------------------------------------------------ */
/* Freestanding string.h                                               */
/* ------------------------------------------------------------------ */
void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) while (n--) *d++ = *s++;
    else if (d > s) { d += n; s += n; while (n--) *--d = *--s; }
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
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

size_t strlen(const char *s) { const char *p = s; while (*p) ++p; return (size_t)(p - s); }

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { ++a; ++b; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n--)
        {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca != cb || !ca) return (int)ca - (int)cb;
        ++a; ++b;
        }
    return 0;
}

char *strchr(const char *s, int c)
{
    for (; *s; ++s) if ((unsigned char)*s == (unsigned char)c) return (char *)s;
    return c == 0 ? (char *)s : NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    if (c == 0) return (char *)s + strlen(s);
    for (; *s; ++s) if ((unsigned char)*s == (unsigned char)c) last = s;
    return (char *)last;
}

char *strstr(const char *h, const char *n)
{
    size_t ln = strlen(n);
    if (!ln) return (char *)h;
    for (; *h; ++h) if (!strncmp(h, n, ln)) return (char *)h;
    return NULL;
}

char *strpbrk(const char *s, const char *accept)
{
    for (; *s; ++s)
        {
        const char *a = accept;
        while (*a) { if (*a == *s) return (char *)s; ++a; }
        }
    return NULL;
}

size_t strspn(const char *s, const char *accept)
{
    size_t n = 0;
    for (; *s; ++s)
        {
        const char *a = accept;
        while (*a && *a != *s) ++a;
        if (!*a) break;
        ++n;
        }
    return n;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t n = 0;
    for (; *s; ++s)
        {
        const char *r = reject;
        while (*r && *r != *s) ++r;
        if (*r) break;
        ++n;
        }
    return n;
}

int strcoll(const char *a, const char *b) { return strcmp(a, b); }

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s;
    while (n--) { if (*p == (unsigned char)c) return (void *)p; ++p; }
    return NULL;
}

char *strcpy(char *d, const char *s) { while ((*d++ = *s++)) {} return d; }
char *strncpy(char *d, const char *s, size_t n)
{
    while (n && (*d++ = *s++)) --n;
    if (n) while (n--) *d++ = 0;
    return d;
}
char *strcat(char *d, const char *s) { strcpy(d + strlen(d), s); return d; }

char *strerror(int e)
{
    switch (e)
        {
        case EDOM: return "domain error";
        case ERANGE: return "result too large";
        case EINVAL: return "invalid argument";
        case ENOENT: return "no such file";
        case EACCES: return "permission denied";
        default: return "error";
        }
}

/* ------------------------------------------------------------------ */
/* x87 math (no libm)                                                  */
/* ------------------------------------------------------------------ */
double fabs(double x)
{
    union { double d; unsigned int u[2]; } v;
    v.d = x; v.u[1] &= 0x7fffffffu; return v.d;
}

double sqrt(double x)
{
    double r;
    __asm__ __volatile__("fldl %1\n\tfsqrt\n\tfstpl %0" : "=m"(r) : "m"(x));
    return r;
}

static double round_mode(double x, int mode)
{
    short old, cw;
    double r;
    __asm__ __volatile__("fnstcw %0" : "=m"(old));
    cw = (short)((old & ~0x0c00) | mode);
    __asm__ __volatile__("fldcw %0" :: "m"(cw));
    __asm__ __volatile__("fldl %1\n\tfrndint\n\tfstpl %0" : "=m"(r) : "m"(x));
    __asm__ __volatile__("fldcw %0" :: "m"(old));
    return r;
}

double floor(double x) { return round_mode(x, 0x0400); } /* toward -inf */
double ceil(double x)  { return round_mode(x, 0x0800); } /* toward +inf */

double fmod(double x, double y)
{
    double r;
    if (y == 0.0) return __builtin_nanf("");
    __asm__ __volatile__(
        "fldl %1\n\t"
        "fldl %2\n\t"
        "1:\n\t"
        "fprem\n\t"
        "fnstsw %%ax\n\t"
        "testb $4, %%ah\n\t"
        "jnz 1b\n\t"
        "fstp %%st(1)\n\t"
        "fstpl %0"
        : "=m"(r) : "m"(x), "m"(y) : "ax");
    return r;
}

double ldexp(double x, int e)
{
    double r; int ex = e;
    __asm__ __volatile__(
        "fldl %1\n\t"
        "fildl %2\n\t"
        "fscale\n\t"
        "fstp %%st(1)\n\t"
        "fstpl %0"
        : "=m"(r) : "m"(x), "m"(ex));
    return r;
}

double frexp(double x, int *e)
{
    double ex, m;
    if (x == 0.0) { *e = 0; return 0.0; }
    __asm__ __volatile__(
        "fldl %2\n\t"
        "fxtract\n\t"
        "fstpl %0\n\t"
        "fstpl %1"
        : "=m"(ex), "=m"(m) : "m"(x));
    *e = (int)ex + 1;
    return m * 0.5;
}

double pow(double x, double y)
{
    double r;
    if (x == 0.0) return (y > 0.0) ? 0.0 : __builtin_huge_val();
    if (x < 0.0)
        {
        if (y != floor(y)) return __builtin_nanf("");
        r = pow(-x, y);
        return (y == 2.0 * floor(y / 2.0)) ? r : -r;
        }
    __asm__ __volatile__(
        "fldl %1\n\t"        /* y  */
        "fldl %2\n\t"        /* x, y */
        "fyl2x\n\t"          /* y*log2(x) */
        "fld %%st(0)\n\t"
        "frndint\n\t"        /* int part */
        "fxch\n\t"
        "fsub %%st(1)\n\t"   /* fraction */
        "f2xm1\n\t"
        "fld1\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fscale\n\t"
        "fstp %%st(1)\n\t"
        "fstpl %0"
        : "=m"(r) : "m"(y), "m"(x));
    return r;
}

static double _e32_log2(double x);

/* Simple but adequate transcendental approximations for math.sin/cos/etc. */
double sin(double x)
{
    double r;
    __asm__ __volatile__("fldl %1\n\tfsin\n\tfstpl %0" : "=m"(r) : "m"(x));
    return r;
}
double cos(double x)
{
    double r;
    __asm__ __volatile__("fldl %1\n\tfcos\n\tfstpl %0" : "=m"(r) : "m"(x));
    return r;
}
double tan(double x)
{
    double s, c;
    s = sin(x); c = cos(x);
    return (c == 0.0) ? __builtin_huge_val() : s / c;
}
double exp(double x) { return pow(2.7182818284590452354, x); }
double log(double x) { return (x > 0.0) ? _e32_log2(x) * 0.6931471805599453094 : __builtin_nanf(""); }
double log10(double x) { return log(x) / 2.302585092994045684; }
double atan(double x) { return x - x*x*x/3.0 + x*x*x*x*x/5.0 - x*x*x*x*x*x*x/7.0; }
double atan2(double y, double x) { (void)y; (void)x; return 0.0; }
double asin(double x) { (void)x; return 0.0; }
double acos(double x) { (void)x; return 0.0; }

static double _e32_log2(double x) /* natural log via x87: log(x)=log2(x)*ln2? see log() */
{
    double r;
    __asm__ __volatile__(
        "fldl %1\n\t"
        "fldln2\n\t"
        "fxch\n\t"
        "fyl2x\n\t"
        "fstpl %0"
        : "=m"(r) : "m"(x));
    return r;
}

/* ------------------------------------------------------------------ */
/* Number parsing                                                      */
/* ------------------------------------------------------------------ */
static double pow10(int n)
{
    double r = 1.0;
    if (n > 0) { while (n--) r *= 10.0; }
    else { while (n++) r /= 10.0; }
    return r;
}

double strtod(const char *s, char **endptr)
{
    const char *orig = s, *digits_start;
    int neg = 0, nd = 0, exp = 0;
    double r = 0.0, scale = 1.0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') ++s;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); ++s; }
    digits_start = s;
    while (*s >= '0' && *s <= '9') { r = r * 10.0 + (*s - '0'); ++s; ++nd; }
    if (*s == '.')
        {
        ++s;
        while (*s >= '0' && *s <= '9')
            {
            r = r * 10.0 + (*s - '0');
            scale *= 10.0;
            ++s; ++nd;
            }
        }
    if (!nd)
        {
        /* inf / nan (case-insensitive) */
        const char *p = orig;
        while (*p == ' ') ++p;
        if (*p == '+' || *p == '-') ++p;
        if ((p[0] == 'i' || p[0] == 'I') && (p[1] == 'n' || p[1] == 'N') && (p[2] == 'f' || p[2] == 'F'))
            { if (endptr) *endptr = (char *)(p + 3); return neg ? -__builtin_huge_val() : __builtin_huge_val(); }
        if ((p[0] == 'n' || p[0] == 'N') && (p[1] == 'a' || p[1] == 'A') && (p[2] == 'n' || p[2] == 'N'))
            { if (endptr) *endptr = (char *)(p + 3); return __builtin_nanf(""); }
        if (endptr) *endptr = (char *)orig;
        return 0.0;
        }
    if (*s == 'e' || *s == 'E')
        {
        const char *q = s + 1;
        int eneg = 0, eval = 0;
        if (*q == '+' || *q == '-') { eneg = (*q == '-'); ++q; }
        if (q != s + 1 && *q >= '0' && *q <= '9')
            {
            while (*q >= '0' && *q <= '9') { if (eval < 10000) eval = eval * 10 + (*q - '0'); ++q; }
            exp = eneg ? -eval : eval;
            s = q;
            }
        }
    (void)digits_start;
    r /= scale;
    if (exp) r *= pow10(exp);
    if (endptr) *endptr = (char *)s;
    return neg ? -r : r;
}

static unsigned long strtoul_base(const char *s, char **endptr, int base,
                                  int *negative)
{
    const char *p = s;
    unsigned long v = 0;
    int neg = 0, any = 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); ++p; }
    if (base == 0)
        {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
        else if (p[0] == '0') { base = 8; }
        else base = 10;
        }
    for (;;)
        {
        int d;
        if (*p >= '0' && *p <= '9') d = *p - '0';
        else if (*p >= 'a' && *p <= 'f') d = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'F') d = *p - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * (unsigned long)base + (unsigned long)d;
        ++p; any = 1;
        }
    if (negative) *negative = neg;
    if (endptr) *endptr = any ? (char *)p : (char *)s;
    return v;
}

long strtol(const char *s, char **endptr, int base)
{
    int neg;
    unsigned long v = strtoul_base(s, endptr, base, &neg);
    return neg ? -(long)v : (long)v;
}

unsigned long strtoul(const char *s, char **endptr, int base)
{
    int neg;
    unsigned long v = strtoul_base(s, endptr, base, &neg);
    return neg ? (unsigned long)(-(long)v) : v;
}

int atoi(const char *s) { return (int)strtol(s, NULL, 10); }
long atol(const char *s) { return strtol(s, NULL, 10); }

/* realloc/calloc are supplied by the shared user/malloc.c heap in this port. */
void abort(void) { for (;;) { } }
void exit(int status) { (void)status; process_exit(0); }

/* ------------------------------------------------------------------ */
/* time / FILE stubs (used only by unused lauxlib file helpers)         */
/* ------------------------------------------------------------------ */
time_t time(time_t *t) { int v = time_now(); if (t) *t = v; return v; }
clock_t clock(void) { return (clock_t)random32(); }
FILE *fopen(const char *path, const char *mode)
{ (void)path; (void)mode; errno = ENOENT; return NULL; }
FILE *freopen(const char *path, const char *mode, FILE *f)
{ (void)path; (void)mode; errno = ENOENT; return f; }
int fclose(FILE *f) { (void)f; return 0; }
int fflush(FILE *f) { (void)f; return 0; }
int feof(FILE *f) { (void)f; return 0; }
int ferror(FILE *f) { (void)f; return 0; }
int getc(FILE *f) { (void)f; return EOF; }
size_t fread(void *p, size_t s, size_t n, FILE *f)
{ (void)p; (void)s; (void)n; (void)f; return 0; }
size_t fwrite(const void *p, size_t s, size_t n, FILE *f)
{ (void)f; e32_lua_write((const char *)p, s * n); return n; }
int fputs(const char *s, FILE *f) { (void)f; e32_lua_write(s, strlen(s)); return 1; }
int puts(const char *s) { e32_lua_write(s, strlen(s)); e32_lua_write("\n", 1); return 1; }
int printf(const char *fmt, ...)
{
    char buf[512]; va_list ap; int n;
    va_start(ap, fmt); n = vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    if (n > 0) e32_lua_write(buf, (size_t)((unsigned)n < sizeof(buf) ? n : (int)sizeof(buf)));
    return n;
}
int fprintf(FILE *f, const char *fmt, ...) { (void)f; va_list ap; int n; char buf[512];
    va_start(ap, fmt); n = vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    if (n > 0) e32_lua_write(buf, (size_t)((unsigned)n < sizeof(buf) ? n : (int)sizeof(buf)));
    return n; }

/* ------------------------------------------------------------------ */
/* A compact printf formatter (the only one Lua needs).                 */
/* ------------------------------------------------------------------ */
static void putpad(char **o, char c, int n, char *limit)
{ while (n-- > 0 && *o < limit) *(*o)++ = c; }

static char *u64dec(unsigned long long v, char *end)
{
    char *p = end;
    do { *--p = (char)('0' + (int)(v % 10)); v /= 10; } while (v);
    return p;
}

static char *u64base(unsigned long long v, unsigned base, int upper, char *end)
{
    const char *digits = upper ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               : "0123456789abcdefghijklmnopqrstuvwxyz";
    char *p = end;
    do { *--p = digits[v % base]; v /= base; } while (v);
    return p;
}

/* Emit a decimal float into a temporary buffer; returns digits string and
   decimal exponent (value = 0.<digits> * 10^exp10), plus its length. */
static void dec_digits(double x, int prec, char *digits, int *ndig, int *exp10)
{
    int e = 0;
    double p10 = 1.0, scaled;
    unsigned long long v;
    char *p = digits + 32;
    if (x == 0.0) { *ndig = 1; digits[0] = '0'; *exp10 = 1; return; }
    if (x >= 10.0) { while (x >= p10 * 10.0 && p10 < 1e300) { p10 *= 10.0; ++e; } }
    else if (x < 1.0) { while (x < p10 && p10 > 1e-300) { p10 /= 10.0; --e; } }
    *exp10 = e + 1;
    {
    double s = pow10(prec - 1 - e);
    if (s > 1e18) s = 1e18;
    if (s < 1e-18) s = 1e-18;
    scaled = x * s + 0.5;
    }
    v = (scaled >= 1.8446744073709552e19) ? 18446744073709551615ULL : (unsigned long long)scaled;
    p = u64dec(v, p);
    *ndig = (int)((digits + 32) - p);
    if (*ndig > prec) { memmove(digits, p + (*ndig - prec), prec); *ndig = prec; }
    else memmove(digits, p, (size_t)*ndig);
    while (*ndig < prec) digits[(*ndig)++] = '0';
}

static int e10_estimate(double x);

static char *fmt_float(char *o, double val, int conv, int prec, int flags)
{
    char digits[40], tmp[80];
    int ndig = 0, e10 = 0, neg = 0, i, len;
    if (val != val) { memcpy(o, "nan", 3); return o + 3; }
    if (val == __builtin_huge_val() || val == -__builtin_huge_val())
        { if (val < 0) *o++ = '-'; memcpy(o, "inf", 3); return o + 3; }
    if (val < 0) { neg = 1; val = -val; }
    if (conv == 'g' || conv == 'G')
        {
        int use_exp;
        if (prec == 0) prec = 1;
        dec_digits(val, prec, digits, &ndig, &e10);
        use_exp = (e10 - 1 < -4 || e10 - 1 >= prec);
        if (use_exp)
            {
            /* one digit before the point */
            tmp[len = 0] = digits[0]; ++len;
            {
            int last = ndig;
            while (last > 1 && digits[last - 1] == '0' && !(flags & 1)) --last;
            if (last > 1 || (flags & 1)) { tmp[len++] = '.'; for (i = 1; i < last; ++i) tmp[len++] = digits[i]; }
            }
            tmp[len++] = (conv == 'G') ? 'E' : 'e';
            {
            int ex = e10 - 1, absx = ex < 0 ? -ex : ex;
            tmp[len++] = ex < 0 ? '-' : '+';
            if (absx < 10) tmp[len++] = '0';
            { char *p = u64dec((unsigned long long)absx, tmp + 32); while (p < tmp + 32) tmp[len++] = *p++; }
            }
            memcpy(o, tmp, (size_t)len); o += len;
            }
        else
            {
            int point = e10, last = ndig;
            while (last > 0 && digits[last - 1] == '0' && !(flags & 1)) --last;
            if (point <= 0)
                {
                tmp[len = 0] = '0'; ++len;
                if (last > 0 || (flags & 1)) { tmp[len++] = '.'; for (i = point; i < 0; ++i) tmp[len++] = '0'; for (i = 0; i < last; ++i) tmp[len++] = digits[i]; }
                }
            else if (point >= last)
                {
                for (i = 0; i < point; ++i) tmp[i] = (i < last) ? digits[i] : '0';
                len = point;
                if (flags & 1) { tmp[len++] = '.'; }
                }
            else
                {
                for (i = 0; i < point; ++i) tmp[i] = digits[i];
                len = point; tmp[len++] = '.';
                for (i = point; i < last; ++i) tmp[len++] = digits[i];
                }
            memcpy(o, tmp, (size_t)len); o += len;
            }
        }
    else if (conv == 'f' || conv == 'F')
        {
        int point, i;
        if (prec < 0) prec = 6;
        if (prec > 17) prec = 17;
        dec_digits(val, e10_estimate(val) + prec + 1, digits, &ndig, &e10);
        point = e10;
        if (point <= 0)
            {
            *o++ = '0'; *o++ = '.';
            for (i = point; i < 0; ++i) *o++ = '0';
            for (i = 0; i < ndig && i < prec - (-point); ++i) *o++ = digits[i];
            while (i++ < prec) *o++ = '0';
            }
        else
            {
            for (i = 0; i < point; ++i) *o++ = (i < ndig) ? digits[i] : '0';
            *o++ = '.';
            for (i = point; i < point + prec; ++i)
                *o++ = (i < ndig) ? digits[i] : '0';
            }
        }
    else /* e / E */
        {
        int last;
        if (prec < 0) prec = 6;
        dec_digits(val, prec + 1, digits, &ndig, &e10);
        *o++ = digits[0]; *o++ = '.';
        for (i = 1; i <= prec; ++i) *o++ = (i < ndig) ? digits[i] : '0';
        *o++ = (conv == 'E') ? 'E' : 'e';
        *o++ = (e10 - 1) < 0 ? '-' : '+';
        { int a = e10 - 1; if (a < 0) a = -a;
          if (a < 10) *o++ = '0';
          { char *p = u64dec((unsigned long long)a, tmp + 32); while (p < tmp + 32) *o++ = *p++; } }
        (void)last;
        }
    if (neg) { /* sign is inserted by caller via 'lead' */ }
    return o;
}

static int e10_estimate(double x)
{
    int e = 0;
    if (x <= 0) return 0;
    while (x >= 10.0 && e < 308) { x /= 10.0; ++e; }
    return e;
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
    char buf[512], lead[8], body[128];
    char *o = buf;
    const char *f = fmt;
    int total = 0;
    while (*f)
        {
        int flags = 0, width = 0, prec = -1, minus = 0, zero = 0, alt = 0;
        int length = 0, ch, blen = 0, llen, pad;
        char *bp = body;
        if (*f != '%') { if (o < buf + sizeof(buf) - 1) *o++ = *f++; ++total; continue; }
        ++f;
        if (*f == '%') { if (o < buf + sizeof(buf) - 1) *o++ = '%'; ++f; ++total; continue; }
        for (;;)
            {
            if (*f == '-') { minus = 1; ++f; }
            else if (*f == '+') { flags |= 2; ++f; }
            else if (*f == ' ') { flags |= 4; ++f; }
            else if (*f == '0') { zero = 1; ++f; }
            else if (*f == '#') { alt = 1; flags |= 1; ++f; }
            else break;
            }
        if (*f == '*') { width = va_arg(ap, int); ++f; if (width < 0) { minus = 1; width = -width; } }
        else while (*f >= '0' && *f <= '9') { width = width * 10 + (*f++ - '0'); }
        if (*f == '.')
            {
            ++f;
            if (*f == '*') { prec = va_arg(ap, int); ++f; }
            else { prec = 0; while (*f >= '0' && *f <= '9') prec = prec * 10 + (*f++ - '0'); }
            if (prec < 0) prec = 0;
            }
        if (*f == 'h') { length = -1; ++f; if (*f == 'h') { length = -2; ++f; } }
        else if (*f == 'l') { length = 1; ++f; if (*f == 'l') { length = 2; ++f; } }
        else if (*f == 'L' || *f == 'q') { length = 2; ++f; }
        else if (*f == 'z' || *f == 't' || *f == 'j') { length = 2; ++f; }
        ch = *f++;
        lead[0] = 0;
        switch (ch)
            {
            case 'd': case 'i':
                {
                long long v = (length == 2) ? va_arg(ap, long long)
                            : (length == 1) ? va_arg(ap, long)
                            : va_arg(ap, int);
                unsigned long long u;
                if (v < 0) { lead[0] = '-'; u = (unsigned long long)(-v); }
                else { u = (unsigned long long)v; if (flags & 2) lead[0] = '+'; else if (flags & 4) lead[0] = ' '; }
                bp = u64dec(u, body + sizeof(body));
                blen = (int)((body + sizeof(body)) - bp);
                break;
                }
            case 'u': case 'o': case 'x': case 'X':
                {
                unsigned long long v = (length == 2) ? va_arg(ap, unsigned long long)
                            : (length == 1) ? va_arg(ap, unsigned long)
                            : va_arg(ap, unsigned int);
                unsigned base = (ch == 'o') ? 8 : (ch == 'x' || ch == 'X') ? 16 : 10;
                bp = u64base(v, base, (ch == 'X'), body + sizeof(body));
                if (alt && base == 16 && v) { *--bp = (ch == 'X') ? 'X' : 'x'; *--bp = '0'; }
                else if (alt && base == 8 && *bp != '0') *--bp = '0';
                blen = (int)((body + sizeof(body)) - bp);
                break;
                }
            case 'c':
                body[0] = (char)va_arg(ap, int);
                bp = body; blen = 1;
                break;
            case 's':
                {
                const char *s = va_arg(ap, const char *);
                size_t n = strlen(s ? s : "(null)");
                if (prec >= 0 && (size_t)prec < n) n = (size_t)prec;
                if (n > sizeof(body) - 1) n = sizeof(body) - 1;
                memcpy(body, s ? s : "(null)", n); body[n] = 0;
                bp = body; blen = (int)n;
                break;
                }
            case 'p':
                {
                unsigned long v = (unsigned long)va_arg(ap, void *);
                bp = u64base(v, 16, 0, body + sizeof(body));
                *--bp = 'x'; *--bp = '0';
                blen = (int)((body + sizeof(body)) - bp);
                break;
                }
            case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
                {
                double v = va_arg(ap, double);
                int neg = 0;
                if (v < 0) { neg = 1; v = -v; }
                bp = body;
                {
                char *end = fmt_float(bp, v, ch, prec, alt ? 1 : 0);
                blen = (int)(end - bp);
                }
                if (neg) lead[0] = '-';
                else if (flags & 2) lead[0] = '+';
                else if (flags & 4) lead[0] = ' ';
                break;
                }
            default:
                body[0] = ch; bp = body; blen = 1;
                break;
            }
        llen = lead[0] ? 1 : 0;
        pad = width - blen - llen;
        if (pad < 0) pad = 0;
#define EMIT_BODY() do { if (o - buf + blen < (int)sizeof(buf)) { memcpy(o, bp, (size_t)blen); o += blen; } } while (0)
        if (minus)
            {
            if (llen) *o++ = lead[0];
            EMIT_BODY();
            putpad(&o, ' ', pad, buf + sizeof(buf) - 1);
            }
        else if (zero)
            {
            if (llen) *o++ = lead[0];
            putpad(&o, '0', pad, buf + sizeof(buf) - 1);
            EMIT_BODY();
            }
        else
            {
            putpad(&o, ' ', pad, buf + sizeof(buf) - 1);
            if (llen && o < buf + sizeof(buf) - 1) *o++ = lead[0];
            EMIT_BODY();
            }
#undef EMIT_BODY
        total += blen + llen + pad;
        }
    if (size)
        {
        size_t n = (size_t)(o - buf);
        if (n >= size) n = size - 1;
        memcpy(str, buf, n);
        str[n] = 0;
        }
    return total;
}

int snprintf(char *str, size_t size, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = vsnprintf(str, size, fmt, ap); va_end(ap);
    return r;
}
int sprintf(char *str, const char *fmt, ...)
{
    va_list ap; int r;
    va_start(ap, fmt); r = vsnprintf(str, (size_t)-1, fmt, ap); va_end(ap);
    return r;
}
int vsprintf(char *str, const char *fmt, va_list ap)
{
    return vsnprintf(str, (size_t)-1, fmt, ap);
}
