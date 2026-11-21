/*
 * bignum.c -- arbitrary-precision base-10 arithmetic for the bc subset.
 * Freestanding: no libc.  All scratch memory comes from a BcArena.
 */
#include "bignum.h"

/* ------------------------------------------------------------------ arena */
void bc_arena_init(BcArena* a, unsigned char* mem, int size)
{
    a->base = mem;
    a->size = size;
    a->used = 0;
}

void bc_arena_reset(BcArena* a)
{
    a->used = 0;
}

void* bc_alloc(BcArena* a, int n)
{
    int aligned = (n + 3) & ~3;
    if (aligned < 0 || a->used + aligned > a->size) return 0;
    {
        unsigned char* p = a->base + a->used;
        a->used += aligned;
        return p;
    }
}

/* ------------------------------------------------------------- small utils */
static int imax(int a, int b) { return a > b ? a : b; }

/* allocate a fresh BcNum with len digits */
static BcNum* new_num(BcArena* a, int len)
{
    BcNum* n = (BcNum*)bc_alloc(a, sizeof(BcNum));
    if (!n) return 0;
    n->d = (unsigned char*)bc_alloc(a, len > 0 ? len : 1);
    n->len = len > 0 ? len : 1;
    n->scale = 0;
    n->neg = 0;
    if (!n->d) return 0;
    return n;
}

/* Strip leading (most-significant) zeros; keep at least one digit. */
static void normalize(BcNum* n)
{
    while (n->len > 1 && n->d[n->len - 1] == 0) --n->len;
    if (n->len == 1 && n->d[0] == 0) n->neg = 0;
}

/* Compare magnitudes of two digit strings (LSD first). */
static int mag_cmp(const unsigned char* a, int la, const unsigned char* b, int lb)
{
    int i;
    if (la != lb) return la < lb ? -1 : 1;
    for (i = la - 1; i >= 0; --i)
        {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
        }
    return 0;
}

/* out = a + b, with a,b LSD first.  out must hold max(la,lb)+1 bytes. */
static int mag_add(const unsigned char* a, int la,
                   const unsigned char* b, int lb, unsigned char* out)
{
    int i, carry = 0, n = imax(la, lb);
    for (i = 0; i < n; ++i)
        {
        int s = carry + (i < la ? a[i] : 0) + (i < lb ? b[i] : 0);
        out[i] = (unsigned char)(s % 10);
        carry = s / 10;
        }
    if (carry) out[n++] = (unsigned char)carry;
    return n;
}

/* out = a - b, assuming a >= b.  out must hold la bytes. */
static int mag_sub(const unsigned char* a, int la,
                   const unsigned char* b, int lb, unsigned char* out)
{
    int i, borrow = 0;
    for (i = 0; i < la; ++i)
        {
        int d = (i < la ? a[i] : 0) - borrow - (i < lb ? b[i] : 0);
        if (d < 0) { d += 10; borrow = 1; } else borrow = 0;
        out[i] = (unsigned char)d;
        }
    while (la > 1 && out[la - 1] == 0) --la;
    return la;
}

/* out = a * b using an int accumulator then base-10 normalisation. */
static int mag_mul(const unsigned char* a, int la,
                   const unsigned char* b, int lb, unsigned char* out)
{
    int i, j, carry, n = la + lb;
    int* acc = (int*)0; /* accumulator allocated by caller via static scratch */
    /* Use a local fixed accumulator: NDIG is bounded by the arena design.
       We accumulate into out-as-int through a temporary int buffer. */
    static int accbuf[1024];
    if (n > 1024) n = 1024;
    acc = accbuf;
    for (i = 0; i < n; ++i) acc[i] = 0;
    for (i = 0; i < la; ++i)
        for (j = 0; j < lb; ++j)
            acc[i + j] += (int)a[i] * (int)b[j];
    carry = 0;
    for (i = 0; i < n; ++i)
        {
        int v = acc[i] + carry;
        out[i] = (unsigned char)(v % 10);
        carry = v / 10;
        }
    while (carry && n < 1024) { out[n++] = (unsigned char)(carry % 10); carry /= 10; }
    while (n > 1 && out[n - 1] == 0) --n;
    return n;
}

/* Copy a digit string, prepending k zero (low-order) digits:
   result = original * 10^k.  out must hold len+k bytes. */
static int shift_up(const unsigned char* d, int len, int k, unsigned char* out)
{
    int i;
    for (i = 0; i < k; ++i) out[i] = 0;
    for (i = 0; i < len; ++i) out[k + i] = d[i];
    return len + k;
}

/* ------------------------------------------------------------ constructors */
BcNum* bc_num_from_int(BcArena* a, int v)
{
    unsigned magnitude;
    BcNum* n;
    int len = 1, i;
    if (v < 0) magnitude = 0u - (unsigned)v; else magnitude = (unsigned)v;
    n = new_num(a, 12);
    if (!n) return 0;
    {
        unsigned tmp[12];
        i = 0;
        if (magnitude == 0) tmp[i++] = 0;
        while (magnitude) { tmp[i++] = magnitude % 10u; magnitude /= 10u; }
        len = i;
        for (i = 0; i < len; ++i) n->d[i] = (unsigned char)tmp[i];
    }
    n->len = len;
    n->neg = v < 0;
    normalize(n);
    return n;
}

BcNum* bc_num_from_string(BcArena* a, const char* s, int begin, int end)
{
    BcNum* n;
    unsigned char* tmp;
    int i = begin, neg = 0, scale = 0, len = 0, after = 0, k;
    /* leading sign */
    if (i < end && s[i] == '-') { neg = 1; ++i; }
    else if (i < end && s[i] == '+') ++i;
    tmp = (unsigned char*)bc_alloc(a, (end - begin) + 2);
    n = new_num(a, (end - begin) + 2);
    if (!n || !tmp) return 0;
    while (i < end)
        {
        char c = s[i];
        if (c >= '0' && c <= '9')
            {
            tmp[len++] = (unsigned char)(c - '0');
            if (after) ++scale;
            }
        else if (c == '.' && !after)
            {
            after = 1;
            }
        else
            break;   /* stop at first non-number character */
        ++i;
        }
    /* decimal input is most-significant first; the engine stores LSD first */
    for (k = 0; k < len; ++k) n->d[k] = tmp[len - 1 - k];
    if (len == 0) { n->d[0] = 0; n->len = 1; }
    else n->len = len;
    n->scale = scale;
    n->neg = neg;
    normalize(n);
    return n;
}

BcNum* bc_num_copy(BcArena* a, const BcNum* n)
{
    BcNum* r = new_num(a, n->len);
    int i;
    if (!r) return 0;
    for (i = 0; i < n->len; ++i) r->d[i] = n->d[i];
    r->len = n->len;
    r->scale = n->scale;
    r->neg = n->neg;
    return r;
}

BcNum* bc_num_negate(BcArena* a, const BcNum* n)
{
    BcNum* r = bc_num_copy(a, n);
    if (r && !bc_num_is_zero(r)) r->neg = !r->neg;
    return r;
}

int bc_num_is_zero(const BcNum* n)
{
    return n->len == 1 && n->d[0] == 0;
}

int bc_num_cmp(const BcNum* a, const BcNum* b)
{
    int s = imax(a->scale, b->scale);
    unsigned char* ad;
    unsigned char* bd;
    int la, lb, c;
    BcArena tmp;
    unsigned char scratch[2048];
    /* align scales by multiplying the smaller-scale coefficient by 10^diff */
    bc_arena_init(&tmp, scratch, sizeof(scratch));
    ad = (unsigned char*)bc_alloc(&tmp, a->len + s - a->scale + 2);
    bd = (unsigned char*)bc_alloc(&tmp, b->len + s - b->scale + 2);
    if (!ad || !bd) return 0;
    la = shift_up(a->d, a->len, s - a->scale, ad);
    lb = shift_up(b->d, b->len, s - b->scale, bd);
    c = mag_cmp(ad, la, bd, lb);
    if (bc_num_is_zero(a) && bc_num_is_zero(b)) return 0;
    if (a->neg != b->neg && !bc_num_is_zero(a) && !bc_num_is_zero(b))
        return a->neg ? -1 : 1;
    /* same sign: larger magnitude is larger for positive, smaller for negative */
    if (a->neg) return -c;
    return c;
}

int bc_num_to_int(const BcNum* n)
{
    long long v = 0;
    int i;
    int integer_digits = n->len - n->scale;
    if (integer_digits <= 0) return 0;
    if (integer_digits > 18) integer_digits = 18;
    for (i = n->len - 1; i >= n->len - integer_digits; --i)
        {
        v = v * 10 + n->d[i];
        if (v > 2000000000LL) v = 2000000000LL;
        }
    if (n->neg) v = -v;
    if (v > 2147483647LL) v = 2147483647LL;
    if (v < -2147483648LL) v = -2147483648LL;
    return (int)v;
}

int bc_num_write(const BcNum* n, char* buf, int cap)
{
    int k = 0, i;
    if (cap <= 0) return 0;
    if (n->neg && k < cap - 1) buf[k++] = '-';
    /* integer part: digits with index >= scale */
    if (n->len <= n->scale)
        {
        if (k < cap - 1) buf[k++] = '0';
        }
    else
        {
        for (i = n->len - 1; i >= n->scale; --i)
            if (k < cap - 1) buf[k++] = (char)('0' + n->d[i]);
        }
    if (n->scale > 0)
        {
        if (k < cap - 1) buf[k++] = '.';
        for (i = 0; i < n->scale; ++i)
            {
            int idx = n->scale - 1 - i;
            int digit = idx < n->len ? n->d[idx] : 0;
            if (k < cap - 1) buf[k++] = (char)('0' + digit);
            }
        }
    buf[k] = 0;
    return k;
}

/* ------------------------------------------------------------------ add/sub */
BcNum* bc_num_add(BcArena* a, const BcNum* x, const BcNum* y)
{
    int s = imax(x->scale, y->scale);
    unsigned char* ad;
    unsigned char* bd;
    unsigned char* sum;
    int la, lb, ln, neg, c;
    BcNum* r;
    if (!x || !y) return 0;
    ad = (unsigned char*)bc_alloc(a, x->len + s - x->scale + 2);
    bd = (unsigned char*)bc_alloc(a, y->len + s - y->scale + 2);
    if (!ad || !bd) return 0;
    la = shift_up(x->d, x->len, s - x->scale, ad);
    lb = shift_up(y->d, y->len, s - y->scale, bd);
    sum = (unsigned char*)bc_alloc(a, imax(la, lb) + 2);
    if (!sum) return 0;
    if (x->neg == y->neg)
        {
        ln = mag_add(ad, la, bd, lb, sum);
        neg = x->neg;
        }
    else
        {
        c = mag_cmp(ad, la, bd, lb);
        if (c >= 0) { ln = mag_sub(ad, la, bd, lb, sum); neg = x->neg; }
        else        { ln = mag_sub(bd, lb, ad, la, sum); neg = y->neg; }
        }
    r = new_num(a, ln);
    if (!r) return 0;
    for (c = 0; c < ln; ++c) r->d[c] = sum[c];
    r->len = ln;
    r->scale = s;
    r->neg = neg;
    normalize(r);
    return r;
}

BcNum* bc_num_sub(BcArena* a, const BcNum* x, const BcNum* y)
{
    BcNum* ny = bc_num_negate(a, y);
    if (!ny) return 0;
    return bc_num_add(a, x, ny);
}

/* ------------------------------------------------------------------- multiply */
BcNum* bc_num_mul(BcArena* a, const BcNum* x, const BcNum* y)
{
    unsigned char* prod;
    int pn;
    BcNum* r;
    if (!x || !y) return 0;
    if (bc_num_is_zero(x) || bc_num_is_zero(y)) return bc_num_from_int(a, 0);
    prod = (unsigned char*)bc_alloc(a, x->len + y->len + 4);
    if (!prod) return 0;
    pn = mag_mul(x->d, x->len, y->d, y->len, prod);
    r = new_num(a, pn);
    if (!r) return 0;
    {
        int i;
        for (i = 0; i < pn; ++i) r->d[i] = prod[i];
    }
    r->len = pn;
    r->scale = x->scale + y->scale;
    r->neg = x->neg ^ y->neg;
    normalize(r);
    return r;
}

/* --------------------------------------------------------------- division
   Schoolbook long division of the (scaled) integer coefficients.  The
   remainder stays below the divisor, so each quotient digit needs at most
   nine subtractions.  Truncation is toward zero, matching GNU bc. */
static void divmod_mag(BcArena* a, const unsigned char* num, int nlen,
                       const unsigned char* den, int dlen,
                       unsigned char** qout, int* qlenout)
{
    unsigned char* q = (unsigned char*)bc_alloc(a, nlen + 2);
    unsigned char* rem = (unsigned char*)bc_alloc(a, dlen + 3);
    int remlen = 1, i;
    if (!q || !rem) { *qout = 0; *qlenout = 1; return; }
    for (i = 0; i < nlen + 2; ++i) q[i] = 0;
    rem[0] = 0;
    for (i = nlen - 1; i >= 0; --i)
        {
        int qd = 0, j;
        /* rem = rem*10 + num[i] */
        if (remlen == 1 && rem[0] == 0)
            {
            rem[0] = num[i];
            }
        else
            {
            for (j = remlen; j > 0; --j) rem[j] = rem[j - 1];
            rem[0] = 0;
            ++remlen;
            rem[0] = num[i];
            }
        while (mag_cmp(rem, remlen, den, dlen) >= 0)
            {
            remlen = mag_sub(rem, remlen, den, dlen, rem);
            ++qd;
            }
        q[i] = (unsigned char)qd;
        }
    *qout = q;
    *qlenout = nlen;
}

BcNum* bc_num_div(BcArena* a, const BcNum* x, const BcNum* y, int scale)
{
    unsigned char* nd;
    unsigned char* dd;
    int nlen, dlen, qlen, k;
    unsigned char* q;
    BcNum* r;
    if (!x || !y || bc_num_is_zero(y)) return bc_num_from_int(a, 0);
    if (scale > 512) scale = 512;
    k = y->scale - x->scale + scale;
    if (k >= 0)
        {
        nd = (unsigned char*)bc_alloc(a, x->len + k + 2);
        if (!nd) return 0;
        nlen = shift_up(x->d, x->len, k, nd);
        dd = y->d;
        dlen = y->len;
        }
    else
        {
        nd = x->d;
        nlen = x->len;
        dd = (unsigned char*)bc_alloc(a, y->len - k + 2);
        if (!dd) return 0;
        dlen = shift_up(y->d, y->len, -k, dd);
        }
    divmod_mag(a, nd, nlen, dd, dlen, &q, &qlen);
    if (!q) return bc_num_from_int(a, 0);
    r = new_num(a, qlen);
    if (!r) return 0;
    {
        int i;
        for (i = 0; i < qlen; ++i) r->d[i] = q[i];
    }
    r->len = qlen;
    r->scale = scale;
    r->neg = x->neg ^ y->neg;
    normalize(r);
    return r;
}

BcNum* bc_num_mod(BcArena* a, const BcNum* x, const BcNum* y)
{
    BcNum* q = bc_num_div(a, x, y, 0);
    BcNum* t;
    BcNum* r;
    if (!q || bc_num_is_zero(y)) return bc_num_from_int(a, 0);
    t = bc_num_mul(a, q, y);
    if (!t) return 0;
    r = bc_num_sub(a, x, t);
    if (!r) return 0;
    if (r->scale < imax(x->scale, y->scale)) r->scale = imax(x->scale, y->scale);
    normalize(r);
    return r;
}

BcNum* bc_num_pow(BcArena* a, const BcNum* base, int exp)
{
    BcNum* r = bc_num_from_int(a, 1);
    const BcNum* b = base;
    int i;
    int rscale = 0;
    if (exp < 0) return bc_num_from_int(a, 0);
    if (exp > 4096) exp = 4096;
    for (i = 0; i < exp; ++i)
        {
        r = bc_num_mul(a, r, b);
        if (!r) return 0;
        }
    rscale = base->scale * exp;
    if (rscale > 512) rscale = 512;
    r->scale = rscale;
    return r;
}
