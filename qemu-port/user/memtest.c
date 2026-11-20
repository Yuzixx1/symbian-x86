/*
 * memtest.c -- memory test / heap stress utility for the Sybian REAL port.
 *
 * A genuine PE2 E32 program that stresses the userland malloc/free heap (the
 * chunk-backed allocator in user/malloc.c) from ring 3 and checks it for
 * corruption and overlap.  Three phases:
 *
 *   walk  -- allocate 128 small blocks, stamp an address-derived pattern into
 *            each, then verify every pattern before freeing.  Overlapping or
 *            aliased allocations corrupt a neighbour's stamp and are caught.
 *   churn -- 2000 allocate/write/verify/free cycles with sizes sweeping 8..1024
 *            bytes, exercising split, reuse and coalescing under repetition.
 *   bounds-- a separate private RChunk is used as a canary-guarded buffer: a
 *            logical buffer is written while an adjacent canary region must stay
 *            intact.  No hardware guard page exists in this port, so a genuine
 *            past-the-end write is not attempted (it would fault the process);
 *            the phase reports the skip marker after the canary check.
 *
 * All serial markers go through the genuine EUSER.DLL ordinal 915
 * (RDebug::Printf) PE2 import, with the DebugPrint executive as a fallback so
 * the trace is always visible (same pattern as user/calc.c and user/bench.c).
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*memtest_printf_slot)(const char*);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

#define WALK_N   128u
#define CHURN_N  2000u

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&memtest_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: no hosted libc, so build each marker directly into a buffer. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

static unsigned append_uint(char* dst, unsigned n, unsigned value)
{
    char digits[12];
    int d = 0;
    if (!value) digits[d++] = '0';
    while (value)
        {
        digits[d++] = (char)('0' + (value % 10u));
        value /= 10u;
        }
    while (d) dst[n++] = digits[--d];
    return n;
}

/* ------------------------------------------------------------ pattern engine
 * The byte value depends on the block's own address and on a per-block salt,
 * so two distinct allocations get different stamps.  If a write lands in a
 * neighbour's block (overlap) or memory is corrupted, verification fails. */
static unsigned char pattern_byte(void* p, unsigned j, unsigned salt)
{
    unsigned a = (unsigned)p;
    return (unsigned char)((a ^ (a >> 7) ^ (a >> 15) ^ (a >> 23))
                           + j * 31u + salt * 17u);
}

static void fill_pattern(unsigned char* p, unsigned size, unsigned salt)
{
    unsigned j;
    for (j = 0; j < size; ++j) p[j] = pattern_byte(p, j, salt);
}

static int verify_pattern(unsigned char* p, unsigned size, unsigned salt)
{
    unsigned j;
    for (j = 0; j < size; ++j)
        if (p[j] != pattern_byte(p, j, salt)) return 0;
    return 1;
}

/* ------------------------------------------------------------ phase 1: walk */
static int phase_walk(void)
{
    static void* blocks[WALK_N];
    static unsigned sizes[WALK_N];
    unsigned i;

    for (i = 0; i < WALK_N; ++i)
        {
        unsigned size = 24u + ((i * 13u) % 5u) * 8u;  /* 24..56 bytes */
        void* p = malloc(size);
        if (!p) { marker("MEM: FAIL walk"); return 0; }
        blocks[i] = p;
        sizes[i] = size;
        fill_pattern((unsigned char*)p, size, i + 1u);
        }
    for (i = 0; i < WALK_N; ++i)
        if (!verify_pattern((unsigned char*)blocks[i], sizes[i], i + 1u))
            { marker("MEM: FAIL walk"); return 0; }
    for (i = 0; i < WALK_N; ++i) free(blocks[i]);

    marker("MEM: walk pattern OK");
    return 1;
}

/* ------------------------------------------------------------ phase 2: churn */
static int phase_churn(void)
{
    unsigned i;
    char line[96];

    for (i = 0; i < CHURN_N; ++i)
        {
        unsigned size = 8u + ((i * 37u) % 1017u);     /* 8..1024 bytes */
        unsigned char* p = (unsigned char*)malloc(size);
        if (!p) { marker("MEM: FAIL churn"); return 0; }
        fill_pattern(p, size, i + 1u);
        if (!verify_pattern(p, size, i + 1u))
            {
            free(p);
            marker("MEM: FAIL churn");
            return 0;
            }
        free(p);
        }

    {
    unsigned n = 0;
    n = copy_text(line, n, "MEM: churn ");
    n = append_uint(line, n, CHURN_N);
    n = copy_text(line, n, " cycles OK");
    line[n] = 0;
    marker(line);
    }
    return 1;
}

/* ------------------------------------------------------------ phase 3: bounds */
static int phase_bounds(void)
{
    int handle = chunk_create(0, 4096u, 4096u);
    unsigned char* base;
    int size;
    unsigned i;
    unsigned char* canary;
    unsigned canary_size;
    unsigned char* logical;
    unsigned logical_size;

    if (handle < 0)
        {
        marker("MEM: bounds skipped (no guard page)");
        return 1;
        }
    base = (unsigned char*)chunk_base(handle);
    size = chunk_size(handle);
    if (!base || size < 1024)
        {
        exec2(106u, (unsigned)handle, 0u);   /* close chunk on early exit */
        marker("MEM: bounds skipped (no guard page)");
        return 1;
        }

    /* First 512 bytes are the logical buffer, next 512 are the canary. */
    logical = base;
    logical_size = 512u;
    canary = base + 512u;
    canary_size = 512u;

    /* Stamp the whole guarded region with a position-derived value. */
    for (i = 0; i < logical_size + canary_size; ++i)
        base[i] = (unsigned char)(i * 13u + 7u);

    /* Write only inside the logical buffer, right up to its last byte. */
    fill_pattern(logical, logical_size, 0xA5u);

    /* The adjacent canary must be untouched: any stray write past the logical
     * buffer would be detected here. */
    for (i = 0; i < canary_size; ++i)
        if (canary[i] != (unsigned char)((i + 512u) * 13u + 7u))
            {
            exec2(106u, (unsigned)handle, 0u);
            marker("MEM: FAIL bounds");
            return 0;
            }
    exec2(106u, (unsigned)handle, 0u);   /* EExecHandleClose */

    /* No hardware guard page is exposed by this kernel, so a genuine OOB
     * past-the-end write (which would fault the process) is not attempted. */
    marker("MEM: bounds canary OK (separate chunk)");
    marker("MEM: bounds skipped (no guard page)");
    return 1;
}

/* ------------------------------------------------------------ heap note */
static void heap_note(void)
{
    char line[128];
    unsigned n = 0;
    n = copy_text(line, n, "MEM: heap 64K cap (chunk-backed; no free-heap query exec)");
    line[n] = 0;
    marker(line);
}

int app_main(void)
{
    marker("MEM: MEMORY TEST START");

    if (!phase_walk())   { marker("MEM: FAIL walk");   return 1; }
    if (!phase_churn())  { marker("MEM: FAIL churn");  return 1; }
    if (!phase_bounds()) { marker("MEM: FAIL bounds"); return 1; }
    heap_note();

    marker("MEM: DONE");
    return 0;
}
