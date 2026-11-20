/*
 * clock.c -- end-user CLOCK / TIME utility for the Sybian REAL port.
 *
 * A genuine PE2 E32 program.  All serial markers go through EUSER.DLL ordinal
 * 915 (RDebug::Printf) imported through a PE2 IAT slot at codeBase+0x8000,
 * with the port's DebugPrint executive as a fallback so the trace is always
 * visible on the serial console (same pattern as user/calc.c and sysinfo.c).
 *
 * Uptime comes from the genuine EExecTickCount executive (slow exec 5).  That
 * exec is Kern::TickCount(), which reports SYSTEM ticks, not the nanokernel
 * tick: the i8254 PIT / NKern runs at 1000 Hz (ENanoTickPeriod=1000us) but the
 * system tick period is ESystemTickPeriod=15625us, i.e. 1/64 s = 64 Hz
 * (kernel/eka/kernel/stimer.cpp: pQ->iTickPeriod=15625).  So one exec-5 tick is
 * 1/64 second and CLK_HZ is 64.  The raw tick count is always printed alongside
 * the derived HH:MM:SS uptime so the true rate is visible.
 *
 * The sampled "segments" advance with sleep_ms() (the genuine TimerAfter exec,
 * 133/53, used by runtime.c).  An optional wall-clock face is derived from
 * EExecTimeNow (exec 68) via time_now(), the same RTC source used by ntpc.c.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*clock_printf_slot)(const char*);
extern int exec0(unsigned number);

#define EEXEC_TICK_COUNT 5u   /* genuine EExecTickCount (slow exec 5) */
#define CLK_HZ          64u   /* system tick = 1/64 s (ESystemTickPeriod=15625us) */

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&clock_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
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

static unsigned append_2(char* dst, unsigned n, unsigned value)
{
    dst[n++] = (char)('0' + (value / 10) % 10);
    dst[n++] = (char)('0' + (value % 10));
    return n;
}

/* Render "HH:MM:SS" from a raw tick count at CLK_HZ ticks per second. */
static unsigned append_uptime(char* dst, unsigned n, int ticks)
{
    unsigned total = (ticks > 0) ? (unsigned)ticks / CLK_HZ : 0u;
    n = append_2(dst, n, total / 3600u);
    dst[n++] = ':';
    n = append_2(dst, n, (total / 60u) % 60u);
    dst[n++] = ':';
    n = append_2(dst, n, total % 60u);
    return n;
}

/* Emit "CLK: <label> ticks=<n> uptime=<HH:MM:SS>" as one EUSER-915 marker. */
static void report(const char* label, int ticks)
{
    char line[128];
    unsigned n = 0;
    n = copy_text(line, n, "CLK: ");
    n = copy_text(line, n, label);
    n = copy_text(line, n, " ticks=");
    n = append_int(line, n, ticks);
    n = copy_text(line, n, " uptime=");
    n = append_uptime(line, n, ticks);
    line[n] = 0;
    marker(line);
}

/* Optional wall-clock face from the genuine RTC (EExecTimeNow, exec 68).
 * time_now() returns seconds since 2000-01-01; add the 1970->2000 offset and
 * reduce modulo 86400 to get the UTC time of day. */
static void report_clock_face(void)
{
    int secs = time_now();
    char line[64];
    unsigned n, tod, hh, mm, ss;
    if (secs <= 0)
        {
        marker("CLK: face unavailable (EExecTimeNow 68)");
        return;
        }
    tod = ((unsigned)secs + 946684800u) % 86400u;
    hh = tod / 3600u;
    mm = (tod / 60u) % 60u;
    ss = tod % 60u;
    n = copy_text(line, 0, "CLK: face ");
    n = append_2(line, n, hh);
    line[n++] = ':';
    n = append_2(line, n, mm);
    line[n++] = ':';
    n = append_2(line, n, ss);
    n = copy_text(line, n, " UTC");
    line[n] = 0;
    marker(line);
}

int app_main(void)
{
    char line[128];
    unsigned n, seg;
    int start_ticks;

    marker("CLK: CLOCK UTILITY START");

    start_ticks = exec0(EEXEC_TICK_COUNT);
    n = copy_text(line, 0, "CLK: ticks=");
    n = append_int(line, n, start_ticks);
    n = copy_text(line, n, " uptime=");
    n = append_uptime(line, n, start_ticks);
    n = copy_text(line, n, " (HZ=");
    n = append_int(line, n, (int)CLK_HZ);
    n = copy_text(line, n, ")");
    line[n] = 0;
    marker(line);

    /* Three sampled readings, each one second apart via the genuine timer. */
    marker("CLK: segments 1/2/3");
    for (seg = 1; seg <= 3u; ++seg)
        {
        char label[16];
        unsigned p = 0;
        label[p++] = 's';
        label[p++] = 'e';
        label[p++] = 'g';
        label[p++] = 'm';
        label[p++] = 'e';
        label[p++] = 'n';
        label[p++] = 't';
        label[p++] = ' ';
        label[p++] = (char)('0' + seg);
        label[p] = 0;
        sleep_ms(1000);
        report(label, exec0(EEXEC_TICK_COUNT));
        }

    report_clock_face();
    marker("CLK: DONE");
    return 0;
}
