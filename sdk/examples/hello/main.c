#include "runtime.h"

/* Minimal program: print and exit. The return value becomes the process
   exit reason (visible via the shell `exits` command). */
int app_main(void)
{
    print("Hello from an SDK app!");
    return 0;
}
