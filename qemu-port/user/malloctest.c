#include "runtime.h"

/* Allocator fixture: alignment, patterns, OOM, reuse, coalescing. */
static int check_pattern(unsigned char* p, unsigned n, unsigned char seed)
{
    for (unsigned i = 0; i < n; ++i)
        if (p[i] != (unsigned char)(seed + i)) return -1;
    return 0;
}

static void fill_pattern(unsigned char* p, unsigned n, unsigned char seed)
{
    for (unsigned i = 0; i < n; ++i) p[i] = (unsigned char)(seed + i);
}

int app_main(void)
{
    /* Alignment + pattern integrity across several blocks. */
    unsigned char* a = malloc(100);
    unsigned char* b = malloc(2000);
    unsigned char* c = malloc(1);
    if (!a || !b || !c) { print("MALLOC: initial alloc failed"); return 1; }
    if (((unsigned)a & 7) || ((unsigned)b & 7) || ((unsigned)c & 7))
        { print("MALLOC: misaligned"); return 1; }
    fill_pattern(a, 100, 0x10);
    fill_pattern(b, 2000, 0x40);
    fill_pattern(c, 1, 0x90);
    /* Free the middle, allocate a smaller piece: must reuse the hole. */
    free(b);
    unsigned char* d = malloc(500);
    if (!d) { print("MALLOC: reuse failed"); return 1; }
    fill_pattern(d, 500, 0xC0);
    if (check_pattern(a, 100, 0x10) || check_pattern(c, 1, 0x90) ||
        check_pattern(d, 500, 0xC0))
        { print("MALLOC: corruption"); return 1; }
    free(a); free(c); free(d);
    /* OOM must return NULL, not fault. */
    if (malloc(100000)) { print("MALLOC: oversize succeeded"); return 1; }
    /* Coalescing: drain the pool in small pieces, free all, drain again. */
    {
    static void* slots[64];
    int n = 0;
    while (n < 64)
        {
        void* p = malloc(200);
        if (!p) break;
        slots[n++] = p;
        }
    if (!n) { print("MALLOC: drain failed"); return 1; }
    for (int i = 0; i < n; ++i) free(slots[i]);
    int m = 0;
    while (m < 64)
        {
        void* p = malloc(200);
        if (!p) break;
        slots[m++] = p;
        }
    if (m != n) { print("MALLOC: no coalesce"); return 1; }
    for (int i = 0; i < m; ++i) free(slots[i]);
    }
    free(0); /* must be a safe no-op */
    print("MALLOC: allocator verified");
    return 0;
}
