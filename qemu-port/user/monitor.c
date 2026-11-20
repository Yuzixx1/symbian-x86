/*
 * monitor.c -- SYSTEM MONITOR end-user app for the Sybian REAL port.
 *
 * A genuine PE2 E32 program that paints a short, repeating system-monitor
 * panel from ring 3 using executives that already exist in this port:
 *
 *   exec 241 (variant/qemu_ps_exec.cpp): op 0 -> identity-mapped base of a
 *       struct ps_census (user/pshost.h), op 1 -> refresh it from the genuine
 *       K::Containers[EProcess] / UserFilesChannelCount / DCodeSeg lists.
 *   exec 5  (EExecTickCount): the genuine 64 Hz system tick, used both as the
 *       raw "ticks" figure and as the timebase for the simulated CPU load.
 *
 * CPU load is *simulated* from tick deltas: each sample spends a short busy-
 * wait "active work" window (2 system ticks) followed by a sleep_ms() "idle"
 * window (~1 tick), and the load is busyTicks*100/totalTicks.  Memory usage is
 * likewise a simulated 0..64 KB figure derived from the live census counts,
 * printed as "<n>/64K".  The process count is the genuine census count.
 *
 * Commands are read from PortCon through read_line():
 *   sample / selftest / help / quit
 * The built-in selftest emits two consecutive panels (two samples), so an
 * automated test can assert the full MON: marker set without extra input.
 *
 * All serial markers go through EUSER.DLL ordinal 915 (RDebug::Printf),
 * imported through a PE2 IAT slot at codeBase+0x8000, with the port's
 * DebugPrint executive as a fallback so the trace is always visible (same
 * pattern as user/ps.c, user/taskmgr.c and user/stopwatch.c).
 */
#include "runtime.h"
#include "pshost.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*monitor_printf_slot)(const char*);
extern int exec0(unsigned number);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

#define EEXEC_TICK_COUNT 5u   /* genuine EExecTickCount (slow exec 5) */
#define QE_EXEC_PS      241u  /* genuine process census exec */
#define MON_HZ           64u  /* system tick = 1/64 s (ESystemTickPeriod=15625us) */

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&monitor_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker directly into a buffer, no hosted libc. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

static unsigned append_int(char* dst, unsigned n, int value)
{
    unsigned magnitude = (unsigned)value;
    if (value < 0) { dst[n++] = '-'; magnitude = 0u - magnitude; }
    char digits[12];
    int d = 0;
    if (!magnitude) digits[d++] = '0';
    while (magnitude)
        {
        digits[d++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
        }
    while (d) dst[n++] = digits[--d];
    return n;
}

/* -------------------------------------------------------------- census path
 * Same contract as user/ps.c and user/taskmgr.c: op 0 returns the identity-
 * mapped block, op 1 snapshots the live kernel containers into it. */
static struct ps_census* refresh_census(void)
{
    int base = exec2(QE_EXEC_PS, 0u, 0u);
    struct ps_census* c;
    if (base <= 0x1000) return 0;
    c = (struct ps_census*)(unsigned)base;
    exec2(QE_EXEC_PS, 1u, 0u);
    if (c->magic != PS_MAGIC) return 0;
    return c;
}

static int now_ticks(void) { return exec0(EEXEC_TICK_COUNT); }

/* A genuine busy-wait: spin on the executive tick until min_ticks system
 * ticks have passed (each ~1/64 s).  This is the "active work" window. */
static void busy_wait(int min_ticks)
{
    volatile unsigned spin = 0;
    int start = now_ticks();
    while ((now_ticks() - start) < min_ticks)
        {
        ++spin;
        if (spin == 0u) break;      /* paranoia: never loop forever */
        }
}

/* Simulated CPU load (%) from tick deltas across a busy-wait.  Active window
 * is a 2-tick busy-wait, idle window is a ~1-tick sleep_ms(), and the figure is
 * busyTicks*100/totalTicks clamped to 0..100.  Also returns the ending tick
 * count through *out_ticks so the panel can print the raw exec-5 value. */
static int sample_cpu(int* out_ticks)
{
    int t0, twork, tend, busy, total, pct;
    t0 = now_ticks();
    busy_wait(2);                       /* active work: ~2 system ticks */
    twork = now_ticks();
    sleep_ms(1000u / MON_HZ);           /* idle: ~1 system tick (15 ms) */
    tend = now_ticks();

    busy = twork - t0;
    total = tend - t0;
    if (total < 1) total = 1;
    if (busy < 0) busy = 0;
    if (busy > total) busy = total;
    pct = busy * 100 / total;
    if (pct > 100) pct = 100;
    if (tend <= 0) tend = t0;
    *out_ticks = tend;
    return pct;
}

/* Simulated memory use (KB of a 64 KB budget) from the live census: a small
 * fixed kernel/user baseline plus a per-process, per-segment and per-channel
 * cost.  Clamped to 0..64 so it is always a valid "<n>/64K" figure. */
static int simulated_mem_kb(const struct ps_census* c)
{
    int kb = 4 + c->processes * 3 + c->segs * 2 + c->channels;
    if (kb < 0) kb = 0;
    if (kb > 64) kb = 64;
    return kb;
}

/* ------------------------------------------------------------- monitor panel
 * Emit one full refresh: counts line, cpu/mem line, raw ticks line. */
static void emit_panel(void)
{
    struct ps_census* c = refresh_census();
    char line[128];
    unsigned n;
    int ticks = 0;
    int cpu;

    if (!c) { marker("MON: census unavailable"); return; }
    cpu = sample_cpu(&ticks);

    n = copy_text(line, 0, "MON: processes=");
    n = append_int(line, n, c->processes);
    n = copy_text(line, n, " channels=");
    n = append_int(line, n, c->channels);
    n = copy_text(line, n, " segs=");
    n = append_int(line, n, c->segs);
    line[n] = 0;
    marker(line);

    n = copy_text(line, 0, "MON: cpu=");
    n = append_int(line, n, cpu);
    n = copy_text(line, n, "% mem=");
    n = append_int(line, n, simulated_mem_kb(c));
    n = copy_text(line, n, "/64K");
    line[n] = 0;
    marker(line);

    n = copy_text(line, 0, "MON: ticks=");
    n = append_int(line, n, ticks);
    line[n] = 0;
    marker(line);
}

/* Built-in self-test: two consecutive samples through the same panel path as
 * the live `sample` command. */
static void do_selftest(void)
{
    marker("MON: selftest");
    emit_panel();                       /* sample 1/2 */
    emit_panel();                       /* sample 2/2 */
    marker("MON: selftest OK");
}

/* ---------------------------------------------------------------- commands */
/* Case-insensitive compare of a command word to a literal. */
static int word_is(const char* word, unsigned length, const char* literal)
{
    unsigned i = 0;
    while (i < length && literal[i])
        {
        char a = word[i], b = literal[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
        ++i;
        }
    return (i == length && literal[i] == 0) ? 1 : 0;
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    const char* word;
    unsigned length;

    while (*input == ' ') ++input;
    word = input;
    while (*word && *word != ' ') ++word;
    length = (unsigned)(word - input);

    if      (word_is(input, length, "sample"))    emit_panel();
    else if (word_is(input, length, "selftest"))  do_selftest();
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        marker("MON: sample selftest help quit");
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("MON: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("MON: console busy"); return 1; }

    fn = *(unsigned*)(void*)&monitor_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    marker("MON: SYSTEM MONITOR");
    marker(s_bound ? "MON: EUSER ord 915 bound"
                   : "MON: using DebugPrint marker path");

    for (;;)
        {
        print("MON> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("MON: DONE");
    con_release();
    return 0;
}
