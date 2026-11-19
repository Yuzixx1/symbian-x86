#include "runtime.h"

/* REV <text> — reverse the command line. */
static U16 text[128];

int app_main(void)
{
    int n = command_line(text, 128);
    if (n < 0) { print("REV: line too long"); return 1; }
    char a[128];
    for (int i = 0; i < n && i < 127; ++i)
        a[i] = text[i] < 128 ? (char)text[i] : '?';
    a[n < 127 ? n : 127] = 0;
    int len = 0;
    while (a[len]) ++len;
    char out[160];
    int p = 0;
    const char* pre = "REV: ";
    while (*pre) out[p++] = *pre++;
    for (int i = 0; i < len; ++i)
        out[p++] = a[len - 1 - i];
    out[p] = 0;
    print(out);
    return 0;
}
