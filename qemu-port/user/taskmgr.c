/*
 * taskmgr.c -- interactive TASK MANAGER for the Sybian REAL port.
 *
 * A genuine PE2 E32 program that lists the live EKA2 process census and lets
 * the user kill a process by container id, all from ring 3 through two small
 * dedicated slow executives that already exist in this port:
 *
 *   exec 241 (variant/qemu_ps_exec.cpp): op 0 -> identity-mapped base of a
 *       struct ps_census (user/pshost.h), op 1 -> refresh it from the genuine
 *       K::Containers[EProcess] / UserFilesChannelCount / DCodeSeg lists.
 *   exec 269 (variant/e32load.cpp, SvcMgrKillExec): kill(pid, 0) ->
 *       E32Kill(pid), the same path the shell KILL command uses.  E32Kill
 *       refuses the caller's own process, so this program never kills itself.
 *
 * Commands are read from PortCon through read_line(): list / kill <pid> /
 * selftest / help / quit.  A built-in selftest refreshes and lists processes
 * without killing anything, so an automated test can assert all TM: markers
 * without disturbing real services.
 *
 * All serial markers go through EUSER.DLL ordinal 915 (RDebug::Printf),
 * imported through a PE2 IAT slot at codeBase+0x8000, with the port's
 * DebugPrint executive as a fallback so the trace is always visible (same
 * pattern as user/ps.c and user/filemgr.c).
 */
#include "runtime.h"
#include "pshost.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*taskmgr_printf_slot)(const char*);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

#define QE_EXEC_PS    241u
#define QE_EXEC_KILL  269u

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&taskmgr_printf_slot;
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

/* ------------------------------------------------------------- census path */
static struct ps_census* refresh_census(void)
{
    /* op 0: ask the kernel for the identity-mapped census block. */
    int base = exec2(QE_EXEC_PS, 0u, 0u);
    struct ps_census* c;
    if (base <= 0x1000) return 0;
    c = (struct ps_census*)(unsigned)base;

    /* op 1: snapshot the live containers into that block. */
    exec2(QE_EXEC_PS, 1u, 0u);
    if (c->magic != PS_MAGIC) return 0;
    return c;
}

static void list_processes(const struct ps_census* c)
{
    char line[PS_NAME_LEN + 32];
    int i;
    int listed = c->listed;
    if (listed > PS_MAX_PROCS) listed = PS_MAX_PROCS;
    for (i = 0; i < listed; ++i)
        {
        unsigned n = 0;
        const char* src;
        n = copy_text(line, n, "TM: pid=");
        n = append_int(line, n, c->proc[i].pid);
        n = copy_text(line, n, " ");
        src = c->proc[i].name;
        if (!*src) src = "?";
        n = copy_text(line, n, src);
        line[n] = 0;
        marker(line);
        }
}

/* Refresh and print the live process list.  Always emits process lines (or a
 * single "unavailable" line) so a scripted run can assert the TM markers. */
static void show_processes(void)
{
    struct ps_census* c = refresh_census();
    if (!c) { marker("TM: census unavailable"); return; }
    list_processes(c);
}

/* ---------------------------------------------------------------- commands */
static void do_kill(const char* arg)
{
    int pid = 0;
    int r;
    char line[64];
    unsigned n = 0;

    while (*arg == ' ') ++arg;
    if (!*arg) { marker("TM: kill: no pid"); return; }
    while (*arg >= '0' && *arg <= '9')
        {
        pid = pid * 10 + (*arg - '0');
        ++arg;
        }
    if (*arg) { marker("TM: kill: bad pid"); return; }

    r = exec2(QE_EXEC_KILL, (unsigned)pid, 0u);
    n = copy_text(line, n, "TM: killed pid=");
    n = append_int(line, n, pid);
    n = copy_text(line, n, " r=");
    n = append_int(line, n, r);
    line[n] = 0;
    marker(line);

    /* Refresh and show the remaining processes after the kill. */
    show_processes();
}

static void selftest(void)
{
    marker("TM: SELFTEST");
    show_processes();               /* list only -- never calls exec 269 */
    marker("TM: SELFTEST OK");
}

/* Case-insensitive compare of a NUL-terminated command word to a literal. */
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

/* Returns 1 when the user asked to quit. */
static int run_command(const char* input)
{
    const char* word;
    const char* arg;
    unsigned length;

    while (*input == ' ') ++input;
    word = input;
    while (*word && *word != ' ') ++word;
    length = (unsigned)(word - input);
    arg = word;
    while (*arg == ' ') ++arg;

    if      (word_is(input, length, "kill"))     do_kill(arg);
    else if (word_is(input, length, "list") ||
             word_is(input, length, "ps"))       show_processes();
    else if (word_is(input, length, "selftest")) selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))        marker("TM: commands: list kill <pid> selftest quit");
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))        return 1;
    else marker("TM: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    if (con_acquire() < 0) { marker("TM: console busy"); return 1; }

    marker("TM: TASK MANAGER");

    /* A listing on startup guarantees the TM process markers even with no
     * input, and shows which id belongs to which process before any kill. */
    show_processes();

    for (;;)
        {
        print("TM> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("TM: DONE");
    con_release();
    return 0;
}
