/*
 * heaptest.c -- real multi-MB user-heap proof for the EKA2 PC port.
 *
 * Exercises the chunk-backed allocator in user/malloc.c (now capped at 4 MiB
 * via a genuine RChunk, EExec 107/108) from ring 3:
 *
 *   large   -- allocate 1 / 2 / 3 MiB, stamp a pattern across every byte and
 *              verify it before freeing (proves the kernel really reserves
 *              multi-MB chunks, not just a 64 KiB static pool).
 *   repeat  -- allocate/verify/free 1 MiB several times (proves the free list
 *              and coalescing stay stable across large churn).
 *   calloc  -- zero-filled allocation is actually zero across its whole span.
 *   realloc -- grow 1 KiB to 1 MiB, old prefix preserved, new tail writable.
 *
 * All serial markers go through the genuine EUSER.DLL ordinal 915
 * (RDebug::Printf) PE2 import with the DebugPrint executive fallback, the
 * same pattern as user/memtest.c and user/selftest.c.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*heaptest_printf_slot)(const char*);

static void marker(const char* text)
{
    unsigned fn = *(volatile unsigned*)(void*)&heaptest_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* Freestanding: build numeric markers directly, no hosted libc. */
static void marker_n(const char* prefix, unsigned value)
{
    char buf[128];
    unsigned n = 0;
    char digits[12];
    int d = 0;
    while (*prefix && n < sizeof(buf) - 12u) buf[n++] = *prefix++;
    if (!value) digits[d++] = '0';
    while (value) { digits[d++] = (char)('0' + value % 10u); value /= 10u; }
    while (d) buf[n++] = digits[--d];
    buf[n] = 0;
    marker(buf);
}

static void fill_pattern(unsigned char* p, unsigned n, unsigned char seed)
{
    for (unsigned i = 0; i < n; ++i) p[i] = (unsigned char)(seed + i);
}

static int check_pattern(const unsigned char* p, unsigned n, unsigned char seed)
{
    for (unsigned i = 0; i < n; ++i)
        if (p[i] != (unsigned char)(seed + i)) return -1;
    return 0;
}

int app_main(void)
{
    static const unsigned big[3] = { 0x100000u, 0x200000u, 0x300000u };
    static const unsigned char seeds[3] = { 0x11, 0x42, 0x93 };

    marker("HEAP: LARGE HEAP TEST START");
    marker("HEAP: ceiling=4194304 (4 MiB RChunk)");

    /* Genuine multi-MB allocations, fully stamped and verified. */
    for (unsigned round = 0; round < 3; ++round)
        {
        unsigned n = big[round];
        unsigned char* p = (unsigned char*)malloc(n);
        if (!p) { marker("HEAP: FAIL large malloc"); return 1; }
        fill_pattern(p, n, seeds[round]);
        if (check_pattern(p, n, seeds[round]))
            { marker("HEAP: FAIL pattern corruption"); return 1; }
        marker_n("HEAP: alloc+verify OK bytes=", n);
        free(p);
        }

    /* Repeat 1 MiB cycles: free list must coalesce large spans. */
    for (unsigned r = 0; r < 4; ++r)
        {
        unsigned char* p = (unsigned char*)malloc(0x100000u);
        if (!p) { marker("HEAP: FAIL repeat malloc"); return 1; }
        fill_pattern(p, 0x100000u, (unsigned char)(0xA0 + r));
        if (check_pattern(p, 0x100000u, (unsigned char)(0xA0 + r)))
            { marker("HEAP: FAIL repeat corruption"); return 1; }
        free(p);
        }
    marker("HEAP: repeat 1MiB cycles OK");

    /* calloc: whole span must be zero. */
    {
    unsigned char* z = (unsigned char*)calloc(65536u, 1u);
    if (!z) { marker("HEAP: FAIL calloc"); return 1; }
    for (unsigned i = 0; i < 65536u; ++i)
        if (z[i]) { marker("HEAP: FAIL calloc nonzero"); return 1; }
    free(z);
    marker("HEAP: calloc zeros OK");
    }

    /* realloc: 1 KiB -> 1 MiB, old prefix preserved, new tail writable. */
    {
    unsigned char* q = (unsigned char*)malloc(1024u);
    if (!q) { marker("HEAP: FAIL realloc seed"); return 1; }
    fill_pattern(q, 1024u, 0x77);
    unsigned char* q2 = (unsigned char*)realloc(q, 0x100000u);
    if (!q2) { marker("HEAP: FAIL realloc grow"); return 1; }
    if (check_pattern(q2, 1024u, 0x77))
        { marker("HEAP: FAIL realloc prefix"); return 1; }
    fill_pattern(q2 + 1024u, 0x1000u, 0x2D);
    if (check_pattern(q2 + 1024u, 0x1000u, 0x2D))
        { marker("HEAP: FAIL realloc tail"); return 1; }
    free(q2);
    marker("HEAP: realloc grow OK");
    }

    /* One more 1 MiB after all that churn: coalescing really returned it. */
    {
    unsigned char* p = (unsigned char*)malloc(0x100000u);
    if (!p) { marker("HEAP: FAIL final malloc"); return 1; }
    fill_pattern(p, 0x100000u, 0x5A);
    if (check_pattern(p, 0x100000u, 0x5A))
        { marker("HEAP: FAIL final corruption"); return 1; }
    free(p);
    }

    marker("HEAP: DONE");
    marker("HEAP: RESULT PASS");
    return 0;
}
