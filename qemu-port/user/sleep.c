#include "runtime.h"

/* Sleep-primitive probe: isolates User::After from console input. */
int app_main(void)
{
    print("SLEEP: before");
    sleep_us(5);
    print("SLEEP: after tiny1");
    sleep_us(5);
    print("SLEEP: after tiny2");
    sleep_ms(500);
    print("SLEEP: after 500ms");
    sleep_ms(0);
    print("SLEEP: after yield");
    thread_yield();
    print("SLEEP: done");
    return 0;
}
