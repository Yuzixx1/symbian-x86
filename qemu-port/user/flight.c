/*
 * flight.c -- a FLIGHT SIMULATOR / altitude tracker for the Sybian REAL port
 * (PE2 E32, "flight").
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  All serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible (same pattern as
 * user/stopwatch.c, user/snake.c and user/filemgr.c).
 *
 * The simulator keeps a tiny deterministic aircraft state --- altitude (ft),
 * speed (knots), heading (deg) and fuel (%) --- and drives it from PortCon
 * through read_line():
 *   climb <n>       increase altitude by n feet
 *   descend <n>     decrease altitude by n feet (never below zero)
 *   throttle <n>    change speed by n knots (n may be negative)
 *   turn <deg>      change heading by deg degrees (normalised to 0..359)
 *   status          report the current state
 *   autopilot       run 5 scripted ticks that update altitude/speed/fuel
 *   selftest        reset, run the autopilot and print the resulting status
 *   help            list commands
 *   quit            exit
 * The built-in selftest is fully scripted so an automated QEMU test can assert
 * every marker (startup, five ticks, the final status and DONE) without any
 * interaction beyond `selftest` and `quit`.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*flight_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&flight_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
    print(text);                              /* DebugPrint fallback */
}

/* The status marker ends in a literal '%'.  EUSER's RDebug::Printf parses its
 * argument with AppendFormatList, so a bare '%' consumes a phantom variadic
 * argument and faults.  Escape each '%' as '%%' for that path only; the raw
 * text (single '%') still goes to DebugPrint so both traces read identically. */
static void marker_pct(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&flight_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        char esc[160];
        unsigned n = 0;
        const char* p = text;
        s_bound = 1;
        while (*p && n + 2u < sizeof(esc))
            {
            if (*p == '%') esc[n++] = '%';
            esc[n++] = *p++;
            }
        esc[n] = 0;
        ((void (*)(const char*))fn)(esc);    /* EUSER.DLL ordinal 915 */
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
        digits[d++] = (char)('0' + (magnitude % 10u));
        magnitude /= 10u;
        }
    while (d) dst[n++] = digits[--d];
    return n;
}

/* ------------------------------------------------------------- flight state */
static int s_altitude = 0;      /* feet above sea level */
static int s_speed = 120;       /* knots */
static int s_heading = 0;       /* degrees, 0..359 (0 = north) */
static int s_fuel = 100;        /* percent remaining, 0..100 */

static void clamp_state(void)
{
    if (s_altitude < 0) s_altitude = 0;
    if (s_speed < 0) s_speed = 0;
    while (s_heading < 0) s_heading += 360;
    while (s_heading >= 360) s_heading -= 360;
    if (s_fuel < 0) s_fuel = 0;
    if (s_fuel > 100) s_fuel = 100;
}

static void reset_state(void)
{
    s_altitude = 0;
    s_speed = 120;
    s_heading = 0;
    s_fuel = 100;
}

/* ------------------------------------------------------------------ markers */
/* "FL: ALT=<n>ft SPD=<n>kt HDG=<n> FUEL=<n>%" */
static void emit_status(void)
{
    char line[96];
    unsigned n = copy_text(line, 0, "FL: ALT=");
    n = append_int(line, n, s_altitude);
    n = copy_text(line, n, "ft SPD=");
    n = append_int(line, n, s_speed);
    n = copy_text(line, n, "kt HDG=");
    n = append_int(line, n, s_heading);
    n = copy_text(line, n, " FUEL=");
    n = append_int(line, n, s_fuel);
    n = copy_text(line, n, "%");
    line[n] = 0;
    marker_pct(line);
}

/* "FL: autopilot tick <n>" */
static void emit_tick(int tick)
{
    char line[48];
    unsigned n = copy_text(line, 0, "FL: autopilot tick ");
    n = append_int(line, n, tick);
    line[n] = 0;
    marker(line);
}

/* Five scripted ticks.  Each tick updates altitude, speed and fuel, then the
 * shared clamp keeps the state inside its legal range.  The table is fixed so
 * the selftest always reaches ALT=5000ft SPD=200kt HDG=0 FUEL=65%. */
static void do_autopilot(void)
{
    static const int kAlt[5]  = {  500,  1000,    0,  1500,  2000 };
    static const int kSpd[5]  = {   20,    30,   10,   -20,    40 };
    static const int kFuel[5] = {   -5,    -6,   -7,    -8,    -9 };
    int i;
    for (i = 0; i < 5; ++i)
        {
        emit_tick(i + 1);
        s_altitude += kAlt[i];
        s_speed    += kSpd[i];
        s_fuel     += kFuel[i];
        clamp_state();
        }
}

/* Deterministic built-in self-test through the same state machine as the live
 * commands: reset, fly the autopilot, report status and assert the result. */
static void do_selftest(void)
{
    int ok;
    marker("FL: selftest");
    reset_state();
    do_autopilot();
    emit_status();
    ok = (s_altitude == 5000 && s_speed == 200
          && s_heading == 0 && s_fuel == 65);
    marker(ok ? "FL: selftest OK" : "FL: selftest FAIL");
}

/* ---------------------------------------------------------- command parsing */
/* Case-insensitive compare of a command word to a literal. */
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

/* Parse a decimal integer (optional leading sign); *ok is cleared when no
 * digit was found.  Used by climb/descend/throttle/turn arguments. */
static int parse_int(const char** pp, int* ok)
{
    const char* p = *pp;
    int value = 0, sign = 1, any = 0;
    while (*p == ' ') ++p;
    if (*p == '-') { sign = -1; ++p; }
    else if (*p == '+') ++p;
    while (*p >= '0' && *p <= '9')
        {
        any = 1;
        value = value * 10 + (*p - '0');
        ++p;
        }
    *pp = p;
    if (!any) *ok = 0;
    return sign * value;
}

/* Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    const char* word;
    const char* arg;
    unsigned length;
    int ok = 1;

    while (*input == ' ') ++input;
    word = input;
    while (*word && *word != ' ') ++word;
    length = (unsigned)(word - input);
    arg = word;
    while (*arg == ' ') ++arg;

    if (word_is(input, length, "climb"))
        {
        int n = parse_int(&arg, &ok);
        if (ok) { s_altitude += n; clamp_state(); emit_status(); }
        else marker("FL: climb: bad argument");
        }
    else if (word_is(input, length, "descend"))
        {
        int n = parse_int(&arg, &ok);
        if (ok) { s_altitude -= n; clamp_state(); emit_status(); }
        else marker("FL: descend: bad argument");
        }
    else if (word_is(input, length, "throttle"))
        {
        int n = parse_int(&arg, &ok);
        if (ok) { s_speed += n; clamp_state(); emit_status(); }
        else marker("FL: throttle: bad argument");
        }
    else if (word_is(input, length, "turn"))
        {
        int n = parse_int(&arg, &ok);
        if (ok) { s_heading += n; clamp_state(); emit_status(); }
        else marker("FL: turn: bad argument");
        }
    else if (word_is(input, length, "status"))
        emit_status();
    else if (word_is(input, length, "autopilot"))
        { do_autopilot(); emit_status(); }
    else if (word_is(input, length, "selftest"))
        do_selftest();
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        marker("FL: commands: climb <n> descend <n> throttle <n> turn <deg> status autopilot selftest help quit");
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("FL: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    unsigned fn;

    if (con_acquire() < 0) { marker("FL: console busy"); return 1; }

    fn = *(unsigned*)(void*)&flight_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;

    marker("FL: FLIGHT SIM");
    marker(s_bound ? "FL: EUSER ord 915 bound"
                   : "FL: using DebugPrint marker path");
    emit_status();

    for (;;)
        {
        print("FL> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("FL: DONE");
    con_release();
    return 0;
}
