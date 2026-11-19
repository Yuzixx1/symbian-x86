#include "runtime.h"

/* GUESS — interactive number game. Leases the console, thinks of a number,
   reads guesses, says higher/lower, celebrates. */
static char name[64], line[64];

static void puts_(const char* s) { print(s); }

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { print("GUESS: console busy"); return 1; }
    unsigned target = random32() % 100 + 1;
    puts_("GUESS: I picked 1-100. Type a number, or quit.\n");
    for (;;)
        {
        puts_("? ");
        r = read_line(line, sizeof(line), 120000);
        if (r < 0) { puts_("GUESS: timed out"); break; }
        int i = 0;
        while (line[i] == ' ') ++i;
        if (line[i] == 'q' || line[i] == 'Q') { puts_("GUESS: bye"); break; }
        int v = 0, any = 0;
        while (line[i] >= '0' && line[i] <= '9') { v = v * 10 + (line[i] - '0'); ++i; any = 1; }
        if (!any) { puts_("GUESS: numbers only"); continue; }
        if ((unsigned)v == target) { puts_("GUESS: correct!"); break; }
        puts_((unsigned)v < target ? "GUESS: higher\n" : "GUESS: lower\n");
        }
    con_release();
    (void)name;
    return 0;
}
