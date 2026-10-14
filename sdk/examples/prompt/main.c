#include "runtime.h"

/* Interactive program: lease the console, ask a question, greet, release.
   While leased the shell stays quiet; always release before returning. */
static char name[64];

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { print("PROMPT: console busy"); return 1; }
    print("What is your name? ");
    r = read_line(name, sizeof(name), 60000);
    if (r < 0) { print("PROMPT: timed out"); con_release(); return 1; }
    print("Hello ");
    print(name);
    print("!");
    con_release();
    return 0;
}
