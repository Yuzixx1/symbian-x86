/*
 * bc32.c -- the bc arbitrary-precision calculator language as an R1 E32.
 *
 * This is a genuine user process: freestanding C, no hosted libc, built into
 * a PE2 E32 that imports EUSER.DLL ordinal 915 (RDebug::Printf) for its serial
 * markers.  The interpreter itself lives in ports/bc/ (a faithful, from-
 * scratch bc-compatible subset -- see ports/bc/README).
 *
 * On startup the process runs a built-in script (scale=10; 22/7; 2^100;
 * a factorial defined with define/while) so the fixture can be checked without
 * input, then drops into an interactive `bc> ` REPL over read_line().  Every
 * result line is emitted as a BC: marker through EUSER ord 915.
 */
#include "runtime.h"
#include "bcinterp.h"

/* PE2 IAT slot for EUSER.DLL ordinal 915, defined with --defsym in user.mk.
   Before load it holds the ordinal (915); afterwards the resolved address. */
extern void (*bc32_printf_slot)(const char*);

/* All marker text is assembled before this single call: RDebug::Printf and
   the DebugPrint fallback both terminate a line, so we never split a line. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&bc32_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                             /* DebugPrint fallback */
}

static void bc_emit(const char* text) { marker(text); }

/* The built-in script that runs before the REPL. */
static const char* const kStartup[] = {
    "scale=10",
    "22/7",
    "2^100",
    "define fact(n) { s=1; while (n>1) { s=s*n; n=n-1; } return s; }",
    "fact(10)",
};
static const unsigned kStartupCount = sizeof(kStartup) / sizeof(kStartup[0]);

/* Lines are accumulated until braces balance, so multi-line define/if/while
   blocks work just like the real interpreter. */
static char g_script[512];
static unsigned g_script_len;

static void script_reset(void)
{
    g_script_len = 0;
    g_script[0] = 0;
}

static void script_append(const char* line)
{
    unsigned i = 0;
    while (line[i] && g_script_len + 1 < sizeof(g_script))
        g_script[g_script_len++] = line[i++];
    if (g_script_len + 1 < sizeof(g_script))
        g_script[g_script_len++] = '\n';
    g_script[g_script_len] = 0;
}

/* Return 1 when the accumulated script has balanced braces (a complete
   statement).  Comments are not expected in the test scripts. */
static int script_complete(void)
{
    int depth = 0;
    unsigned i;
    for (i = 0; i < g_script_len; ++i)
        {
        if (g_script[i] == '{') ++depth;
        else if (g_script[i] == '}') --depth;
        }
    return depth <= 0;
}

int app_main(void)
{
    char line[256];
    unsigned i;

    marker("BC: BC32 READY");
    bc_setup(bc_emit);

    for (i = 0; i < kStartupCount; ++i)
        bc_run(kStartup[i]);
    marker("BC: SELFTEST DONE");

    if (con_acquire() < 0)
        {
        marker("BC: console unavailable");
        marker("BC: DONE");
        return 1;
        }

    marker("BC: READY");
    script_reset();
    for (;;)
        {
        con_puts_raw("bc> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (line[0] == 'q' && line[1] == 'u' && line[2] == 'i'
            && line[3] == 't' && line[4] == 0)
            break;
        script_append(line);
        if (script_complete())
            {
            bc_run(g_script);
            script_reset();
            }
        }

    con_release();
    marker("BC: DONE");
    return 0;
}
