/*
 * ps.c -- end-user PROCESS VIEWER for the Sybian REAL port.
 *
 * A genuine PE2 E32 program that reports the EKA2 process census from
 * userland -- the same three counts the kernel shell's `ps`/`segs` commands
 * print (processes, PortFile channels, code segments), plus a bounded list of
 * process ids and names.  The data comes from a genuine kernel executive:
 * slow exec 241 (variant/qemu_ps_exec.cpp) snapshots K::Containers[EProcess],
 * UserFilesChannelCount() and DCodeSeg::GlobalList into an identity-mapped
 * struct (user/pshost.h) that this ring-3 process reads directly.
 *
 * All serial markers go through EUSER.DLL ordinal 915 (RDebug::Printf),
 * imported through a PE2 IAT slot at codeBase+0x8000, with the port's
 * DebugPrint executive as a fallback so the trace is always visible
 * (same pattern as user/sysinfo.c).
 */
#include "runtime.h"
#include "pshost.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*ps_printf_slot)(const char*);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

#define QE_EXEC_PS 241u

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&ps_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker into a buffer, no hosted libc. */
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

static void report_counts(const struct ps_census* c)
{
    char line[128];
    unsigned n = 0;
    n = copy_text(line, n, "PS: processes=");
    n = append_int(line, n, c->processes);
    n = copy_text(line, n, " channels=");
    n = append_int(line, n, c->channels);
    n = copy_text(line, n, " segs=");
    n = append_int(line, n, c->segs);
    line[n] = 0;
    marker(line);
}

static void report_processes(const struct ps_census* c)
{
    char line[PS_NAME_LEN + 32];
    int i;
    int listed = c->listed;
    if (listed > PS_MAX_PROCS) listed = PS_MAX_PROCS;
    for (i = 0; i < listed; ++i)
        {
        unsigned n = 0;
        const char* src;
        n = copy_text(line, n, "PS: pid=");
        n = append_int(line, n, c->proc[i].pid);
        n = copy_text(line, n, " name=");
        src = c->proc[i].name;
        if (!*src) src = "?";
        n = copy_text(line, n, src);
        line[n] = 0;
        marker(line);
        }
}

int app_main(void)
{
    struct ps_census* c;

    marker("PS: PROCESS VIEWER START");

    /* op 0: ask the kernel for the identity-mapped census block. */
    int base = exec2(QE_EXEC_PS, 0u, 0u);
    c = (struct ps_census*)(unsigned)base;
    if (base <= 0x1000)
        {
        marker("PS: process exec unavailable");
        marker("PS: DONE");
        return 0;
        }

    /* op 1: snapshot the live containers into that block. */
    exec2(QE_EXEC_PS, 1u, 0u);
    if (c->magic != PS_MAGIC)
        {
        marker("PS: process exec unavailable");
        marker("PS: DONE");
        return 0;
        }

    report_counts(c);
    report_processes(c);
    marker("PS: DONE");
    return 0;
}
