#include "runtime.h"

/* FIB <n> — print the first n Fibonacci numbers. */
static U16 text[32];
static char out[64];

int app_main(void)
{
    int n = command_line(text, 32);
    int v = 0, any = 0;
    for (int i = 0; i < n; ++i)
        {
        U16 c = text[i];
        if (c >= '0' && c <= '9') { v = v * 10 + (c - '0'); any = 1; }
        else if (c == ' ' && !any) continue;
        else break;
        }
    if (!any || v <= 0 || v > 30) { print("FIB: use FIB n (1-30)"); return 1; }
    unsigned a = 0, b = 1;
    int p = 0;
    const char* pre = "FIB: ";
    while (*pre) out[p++] = *pre++;
    for (int i = 0; i < v; ++i)
        {
        char digits[12];
        int k = 0;
        unsigned x = a;
        if (!x) digits[k++] = '0';
        while (x) { digits[k++] = (char)('0' + x % 10); x /= 10; }
        while (k) out[p++] = digits[--k];
        if (i + 1 < v) out[p++] = ' ';
        unsigned next = a + b;
        a = b; b = next;
        }
    out[p] = 0;
    print(out);
    return 0;
}
