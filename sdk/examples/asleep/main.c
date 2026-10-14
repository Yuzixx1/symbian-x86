#include "runtime.h"

/* Sleep, clock, and random numbers, all from genuine kernel executives. */
static void print_unsigned(unsigned v)
{
    char digits[11];
    int n = 0;
    if (!v) digits[n++] = '0';
    else while (v) { digits[n++] = (char)('0' + v % 10); v /= 10; }
    while (n--) print_n(&digits[n], 1);
}

int app_main(void)
{
    int before = time_now();
    if (before < 0) { print("ASLEEP: no clock"); return 1; }
    print("ASLEEP: sleeping one second...");
    sleep_ms(1000);
    int after = time_now();
    print("ASLEEP: slept ");
    print_unsigned((unsigned)(after - before));
    print("s; random=");
    print_unsigned(random32() % 100);
    thread_yield();
    print("; yielded ok");
    return 0;
}
