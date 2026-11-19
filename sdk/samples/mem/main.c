#include "runtime.h"

/* MEM — allocate, fill, verify, free, repeat. Exercises the user heap. */
static char out[64];

static void putu_(char* p, unsigned v)
{
    char digits[12];
    int k = 0;
    if (!v) digits[k++] = '0';
    while (v) { digits[k++] = (char)('0' + v % 10); v /= 10; }
    while (k) *p++ = digits[--k];
}

int app_main(void)
{
    unsigned total = 0;
    for (int round = 0; round < 3; ++round)
        {
        char* blocks[8];
        int n = 0;
        while (n < 8)
            {
            char* p = malloc(256);
            if (!p) break;
            for (int i = 0; i < 256; ++i) p[i] = (char)(round + i);
            blocks[n++] = p;
            }
        int ok = 1;
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < 256; ++j)
                if ((unsigned char)blocks[i][j] != (unsigned char)(round + j)) ok = 0;
        for (int i = 0; i < n; ++i) free(blocks[i]);
        total += (unsigned)n;
        if (!ok) { print("MEM: corruption"); return 1; }
        }
    char* p = out;
    const char* pre = "MEM: ";
    while (*pre) *p++ = *pre++;
    putu_(p, total);
    while (*p) ++p;
    *p++ = ' '; *p++ = 'b'; *p++ = 'l'; *p++ = 'o'; *p++ = 'c'; *p++ = 'k'; *p++ = 's'; *p = 0;
    print(out);
    return 0;
}
