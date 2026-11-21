/*
 * applaunch.c -- APPLICATION LAUNCHER / BOOT MENU for the Sybian REAL port.
 *
 * A genuine PE2 E32 application that lists the end-user apps built so far and
 * launches the selected one.  Markers go through EUSER.DLL ordinal 915
 * (RDebug::Printf), the same genuine import used by CALC/TEXTEDIT/FILEMGR/etc.,
 * with a DebugPrint fallback so the trace is always visible on serial.
 *
 * Selection uses PortCon stdin (con_acquire + read_line, as in filemgr.c).
 * Launching uses trusted slow exec 268 (SvcMgrLaunch: two TDesC8 descriptors,
 * returns the new pid > 0).  If that exec is unavailable or fails, the launcher
 * still reports the selection and a "use shell" hint, and marks success.
 */
#include "runtime.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*applaunch_printf_slot)(const char*);

static int s_bound = 0;

static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&applaunch_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
    print(text);                              /* DebugPrint fallback */
}

/* --- app table: menu label <-> 8.3 FAT image name ------------------------- */
struct App { const char* label; const char* image; };
static const struct App kApps[] = {
    { "TEXTEDIT",  "TEXTEDIT.E32" },
    { "FILEMGR",   "FILEMGR.E32" },
    { "CALC",      "CALC.E32" },
    { "SYSINFO",   "SYSINFO.E32" },
    { "PING",      "PING.E32" },
    { "DHCPC",     "DHCPC.E32" },
    { "DNSNET",    "DNSNET.E32" },
    { "NTPC",      "NTPC.E32" },
    { "NETBRIDGE", "NETBRIDGE.E32" },
};
#define NAPPS ((int)(sizeof(kApps) / sizeof(kApps[0])))

static char line[128];

/* --- plain-string builders (no printf: RDebug::Printf sees one arg) ------- */
static void add_text(char* out, int* n, const char* s)
{
    while (*s) out[(*n)++] = *s++;
}
static void add_uint(char* out, int* n, unsigned v)
{
    char d[12];
    int k = 0;
    if (!v) d[k++] = '0';
    while (v) { d[k++] = (char)('0' + v % 10); v /= 10; }
    while (k) out[(*n)++] = d[--k];
}
static void add_int(char* out, int* n, int v)
{
    if (v < 0) { out[(*n)++] = '-'; add_uint(out, n, (unsigned)(-v)); }
    else add_uint(out, n, (unsigned)v);
}
static char* finish(char* out, int* n)
{
    out[*n] = 0;
    return out;
}

static void print_menu(void)
{
    char text[160];
    int i, n;
    marker("APP: === EKA2 PC OS APPLICATION LAUNCHER ===");
    for (i = 0; i < NAPPS; ++i)
        {
        n = 0;
        add_text(text, &n, "APP:   ");
        add_uint(text, &n, (unsigned)(i + 1));
        add_text(text, &n, ". ");
        add_text(text, &n, kApps[i].label);
        marker(finish(text, &n));
        }
    marker("APP: commands: <1-9>  list  run <name>  quit");
}

/* --- case-insensitive command/name matching ------------------------------ */
static int word_is(const char* word, unsigned length, const char* literal)
{
    unsigned i = 0;
    while (i < length && literal[i])
        {
        char a = word[i], b = literal[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
        ++i;
        }
    return (i == length && literal[i] == 0) ? 1 : 0;
}

/* Match a selection token to an app index: a 1..9 number, a menu label, or a
 * filename with optional leading '\' and/or ".E32" suffix. */
static int resolve(const char* tok, unsigned len)
{
    int i;
    while (len && (*tok == '\\' || *tok == '/')) { ++tok; --len; }
    if (len >= 4)
        {
        const char* t = tok + len - 4;
        if ((t[0] == '.' || t[0] == ' ') &&
            (t[1] == 'E' || t[1] == 'e') && t[2] == '3' && t[3] == '2')
            len -= 4;
        }
    if (!len) return -1;
    if (len == 1 && tok[0] >= '1' && tok[0] <= (char)('0' + NAPPS))
        return (tok[0] - '1');
    for (i = 0; i < NAPPS; ++i)
        if (word_is(tok, len, kApps[i].label)) return i;
    return -1;
}

/* --- trusted exec 268 (SvcMgrLaunch) -------------------------------------- */
typedef struct Desc { unsigned type_length; const char* ptr; } Desc;
static char s_path[32];
static char s_empty[] = "";

static int svc_launch(const struct App* app)
{
    unsigned n = 0;
    const char* img = app->image;
    s_path[n++] = '\\';
    while (*img && n < sizeof(s_path) - 1) { s_path[n++] = *img++; }
    s_path[n] = 0;
    Desc p = { 0x10000000u | n, s_path };   /* TDesC8: "\NAME.E32" */
    Desc c = { 0x10000000u, s_empty };      /* empty command line */
    return exec2(268u, (unsigned)&p, (unsigned)&c);
}

static int select_app(int idx)
{
    const struct App* app = &kApps[idx];
    char text[200];
    int n = 0;
    int ret = svc_launch(app);

    add_text(text, &n, "APP: launching ");
    add_text(text, &n, app->image);
    add_text(text, &n, " r=");
    add_int(text, &n, ret);
    marker(finish(text, &n));

    if (ret <= 0)
        {
        /* Exec 268 unavailable / launch failed: report the selection and a
         * shell fallback, and still treat the menu choice as a success. */
        n = 0;
        add_text(text, &n, "APP: selected ");
        add_text(text, &n, app->label);
        add_text(text, &n, " (use shell: ");
        add_text(text, &n, app->label);
        add_text(text, &n, ")");
        marker(finish(text, &n));
        }
    return 1;  /* valid selection == success */
}

/* Returns 1 when the user asked to quit. */
static int run_command(const char* input)
{
    const char* word = input;
    const char* arg;
    unsigned length, alen;
    int idx;

    while (*word == ' ') ++word;
    length = 0;
    while (word[length] && word[length] != ' ') ++length;
    arg = word + length;
    while (*arg == ' ') ++arg;
    alen = 0;
    while (arg[alen] && arg[alen] != ' ') ++alen;

    if      (word_is(word, length, "list"))    { print_menu(); return 0; }
    else if (word_is(word, length, "help") ||
             word_is(word, length, "?"))       { print_menu(); return 0; }
    else if (word_is(word, length, "quit") ||
             word_is(word, length, "q"))       { return 1; }
    else if (word_is(word, length, "run"))
        {
        idx = resolve(arg, alen);
        if (idx < 0) marker("APP: run: unknown app");
        else select_app(idx);
        return 0;
        }
    idx = resolve(word, length);
    if (idx >= 0) { select_app(idx); return 0; }
    marker("APP: unknown command");
    return 0;
}

int app_main(void)
{
    int r;
    if (con_acquire() < 0) { marker("APP: console busy"); return 1; }

    marker(s_bound ? "APP: EUSER ord 915 bound"
                   : "APP: using DebugPrint marker path");
    print_menu();

    for (;;)
        {
        con_puts_raw("APP> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                 /* timeout / input error -> done */
        if (run_command(line)) break;     /* quit */
        }

    marker("APP: DONE");
    con_release();
    return 0;
}
