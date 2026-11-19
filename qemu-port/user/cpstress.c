#include "runtime.h"
#include "ipc.h"

/* P-08 stress fixture: long-lived IPC client that keeps two sessions with
   outstanding async requests in flight, waiting in reverse order so surplus
   request-semaphore wakeups exercise the re-signal path on every cycle. The
   instance id (command-line argument) makes each process's markers unique so
   the host test can count completions and identify a kill victim. Deliberate
   sleeps keep the process alive long enough to be killed mid-test. */
static U16 command[32];

static unsigned instance_id(void)
{
    int length = command_line(command, 32);
    unsigned id = 0;
    for (int p = 0; p < length && command[p] >= '0' && command[p] <= '9'; ++p)
        id = id * 10u + (unsigned)(command[p] - '0');
    return id;
}

/* Emit "TAG <id> <value>" as a single line. */
static void emit(const char* tag, unsigned id, unsigned value)
{
    char line[80];
    unsigned n = 0;
    while (tag[n]) { line[n] = tag[n]; ++n; }
    char digits[12];
    unsigned k = 0;
    do { digits[k++] = (char)('0' + id % 10); id /= 10; } while (id);
    while (k) line[n++] = digits[--k];
    line[n++] = ' ';
    k = 0;
    do { digits[k++] = (char)('0' + value % 10); value /= 10; } while (value);
    while (k) line[n++] = digits[--k];
    print_n(line, n);
}

#define CYCLES 100

int app_main(void)
{
    unsigned id = instance_id();
    int s1 = ipc_connect("PortIpcCheck");
    if (s1 < 0) { emit("CPSTRESS: connect failed", id, 0); return 1; }
    int s2 = ipc_connect("PortIpcCheck");
    if (s2 < 0) { ipc_close(s1); emit("CPSTRESS: connect2 failed", id, 0); return 1; }
    emit("CPSTRESS: start", id, 0);
    static struct IpcArgs a, b;
    for (unsigned cycle = 0; cycle < CYCLES; ++cycle)
        {
        volatile int sa, sb;
        a.args[0] = id * 10000u + cycle;
        a.args[1] = a.args[2] = a.args[3] = 0; a.flags = 0;
        b.args[0] = 900000u + id * 1000u + cycle;
        b.args[1] = b.args[2] = b.args[3] = 0; b.flags = 0;
        if (ipc_call_async(s1, 0, &a, &sa))
            { emit("CPSTRESS: send1 failed", id, cycle); return 2; }
        if (ipc_call_async(s2, 0, &b, &sb))
            { emit("CPSTRESS: send2 failed", id, cycle); return 3; }
        if (ipc_wait_for(&sb) != (int)b.args[0])
            { emit("CPSTRESS: waitB failed", id, cycle); return 4; }
        if (ipc_wait_for(&sa) != (int)a.args[0])
            { emit("CPSTRESS: waitA failed", id, cycle); return 5; }
        if (cycle % 10 == 0) emit("CPSTRESS: tick", id, cycle);
        sleep_ms(20);
        }
    ipc_close(s1);
    ipc_close(s2);
    emit("CPSTRESS: done", id, CYCLES);
    return 0;
}
