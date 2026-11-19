#include "runtime.h"

/* CALC <a> <op> <b> — four-function calculator on the command line.
   Example: CALC 12 * 7   ->   CALC: 84 */
static U16 text[64];
static char out[64];

static int atoi_(const char* s, int* sign)
{
    int v = 0;
    *sign = 1;
    if (*s == '-') { *sign = -1; ++s; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); ++s; }
    return v;
}

int app_main(void)
{
    int n = command_line(text, 64);
    if (n < 0) { print("CALC: line too long"); return 1; }
    /* Convert the UTF-16 tail to ASCII for parsing. */
    char a[64];
    for (int i = 0; i < n && i < 63; ++i)
        a[i] = text[i] < 128 ? (char)text[i] : '?';
    a[n < 63 ? n : 63] = 0;
    int i = 0;
    while (a[i] == ' ') ++i;
    int s1, s2;
    int x = atoi_(a + i, &s1);
    while (a[i] >= '0' && a[i] <= '9') ++i;
    if (a[i] == '-') ++i;
    while (a[i] >= '0' && a[i] <= '9') ++i;
    while (a[i] == ' ') ++i;
    char op = a[i++];
    while (a[i] == ' ') ++i;
    int y = atoi_(a + i, &s2);
    x *= s1; y *= s2;
    long result = 0;
    switch (op)
        {
    case '+': result = (long)x + y; break;
    case '-': result = (long)x - y; break;
    case '*': result = (long)x * y; break;
    case '/':
        if (y == 0) { print("CALC: divide by zero"); return 1; }
        result = (long)x / y; break;
    default: print("CALC: use CALC a + b (ops + - * /)"); return 1;
        }
    char* p = out;
    const char* pre = "CALC: ";
    while (*pre) *p++ = *pre++;
    if (result < 0) { *p++ = '-'; result = -result; }
    char digits[16];
    int k = 0;
    if (result == 0) digits[k++] = '0';
    while (result) { digits[k++] = (char)('0' + result % 10); result /= 10; }
    while (k) *p++ = digits[--k];
    *p = 0;
    print(out);
    return 0;
}
