#include "runtime.h"

extern int exec2(unsigned, unsigned, unsigned);

static U16 command[32];

static unsigned parse_pid(void)
{
    int length = command_line(command, 32);
    unsigned pid = 0;
    for (int p = 0; p < length && command[p] >= '0' && command[p] <= '9'; ++p)
        pid = pid * 10u + (unsigned)(command[p] - '0');
    return pid;
}

/* P-08 stress helper: a genuine user process kills another user process by
   container id via the port kill executive (269 -> E32Kill).
   This makes the mid-test kill peer-to-peer rather than shell-driven, so the
   surviving clients must cope with a session-owner disappearing concurrently. */
int app_main(void)
{
    unsigned pid = parse_pid();
    if (!pid) { print("KILLPROC: no pid"); return 1; }
    /* Port kill executive (269, installed by the service manager glue):
       kill(pid, 0) -> E32Kill(pid), the same path the shell KILL uses. */
    int r = exec2(269u, pid, 0u);
    if (r < 0) { print("KILLPROC: kill failed"); return 2; }
    print("KILLPROC: killed");
    return 0;
}
