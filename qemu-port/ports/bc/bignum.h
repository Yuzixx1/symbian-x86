/*
 * bignum.h -- arbitrary-precision base-10 number engine for the bc subset.
 *
 * This is a from-scratch, freestanding (no libc) implementation of the
 * arbitrary-precision integer arithmetic used by GNU bc.  Numbers are stored
 * as decimal digit arrays (least-significant digit first) plus a decimal
 * scale (number of digits after the point) and a sign.  Memory comes from a
 * caller-supplied bump arena, so there is no malloc/free dependency.
 *
 * The semantics (truncating division at the current scale, scale of the
 * quotient = requested scale, scale of product = sum of operand scales,
 * truncating remainder) follow GNU bc 1.07.  This is NOT GNU source code.
 */
#ifndef PORT_BC_BIGNUM_H
#define PORT_BC_BIGNUM_H

/* A decimal number: value = (neg ? -1 : 1) * digits * 10^-scale.
   digits are stored least-significant first, d[len-1] is the most
   significant significant digit (except that zero is len==1, d[0]==0). */
typedef struct {
    unsigned char* d;   /* decimal digits, LSD first                       */
    int len;            /* number of significant digits (>= 1)             */
    int scale;          /* digits after the decimal point                   */
    int neg;            /* 1 when the value is negative (zero is positive)  */
} BcNum;

/* A bump allocator.  The caller owns the backing memory. */
typedef struct {
    unsigned char* base;
    int size;
    int used;
} BcArena;

void bc_arena_init(BcArena* a, unsigned char* mem, int size);
void bc_arena_reset(BcArena* a);
void* bc_alloc(BcArena* a, int n);          /* 4-byte aligned; 0 on OOM */

/* Constructors (all results live in the supplied arena). */
BcNum* bc_num_from_int(BcArena* a, int v);
BcNum* bc_num_from_string(BcArena* a, const char* s, int begin, int end);
BcNum* bc_num_copy(BcArena* a, const BcNum* n);
BcNum* bc_num_negate(BcArena* a, const BcNum* n);

/* Queries. */
int  bc_num_is_zero(const BcNum* n);
int  bc_num_cmp(const BcNum* a, const BcNum* b);   /* -1 / 0 / 1 */
int  bc_num_to_int(const BcNum* n);                /* truncates toward zero */
int  bc_num_write(const BcNum* n, char* buf, int cap); /* decimal text, NUL terminated */

/* Arithmetic.  div produces a quotient with exactly "scale" fractional
   digits (truncated toward zero).  mod has scale max(scale(x),scale(y)). */
BcNum* bc_num_add(BcArena* a, const BcNum* x, const BcNum* y);
BcNum* bc_num_sub(BcArena* a, const BcNum* x, const BcNum* y);
BcNum* bc_num_mul(BcArena* a, const BcNum* x, const BcNum* y);
BcNum* bc_num_div(BcArena* a, const BcNum* x, const BcNum* y, int scale);
BcNum* bc_num_mod(BcArena* a, const BcNum* x, const BcNum* y);
BcNum* bc_num_pow(BcArena* a, const BcNum* base, int exp);

#endif /* PORT_BC_BIGNUM_H */
