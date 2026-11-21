/*
 * basic.c -- a small, genuine line-oriented BASIC interpreter for the Sybian
 * REAL port.  This is NOT a table of canned answers: programs are stored as
 * numbered source lines, tokenised on the fly by a recursive-descent parser,
 * and executed by a real interpreter loop with variables, arithmetic,
 * comparisons, conditional/imperative jumps and FOR/NEXT loops.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * At the `BASIC>` prompt the user types LOAD DEMO, RUN, LIST, NEW, QUIT, or a
 * numbered BASIC line (e.g. `10 PRINT "HELLO"`).  The built-in DEMO program is
 * a number-guessing game: it draws a secret with a deterministic RND(n)
 * function, reads guesses with INPUT, prints TOO LOW / TOO HIGH hints, and
 * finishes with WIN or LOSE.  The interpreter itself is therefore running a
 * real game written in BASIC source.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*basic_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&basic_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: no hosted libc, so build each string directly into a buffer. */
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

static void emit_pref_int(const char* prefix, int value)
{
    char buf[192];
    unsigned n = copy_text(buf, 0, prefix);
    n = append_int(buf, n, value);
    buf[n] = 0;
    marker(buf);
}

/* Case-insensitive whole-word match; skips leading spaces; reports the
 * character just after the word in *end (when supplied). */
static int word_at(const char* s, const char* w, const char** end)
{
    while (*s == ' ' || *s == '\t') ++s;
    while (*w)
        {
        char a = *s, b = *w;
        if (a >= 'a' && a <= 'z') a = (char)(a - 32);
        if (b >= 'a' && b <= 'z') b = (char)(b - 32);
        if (a != b) return 0;
        ++s; ++w;
        }
    if ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')
        || (*s >= '0' && *s <= '9') || *s == '_') return 0;
    if (end) *end = s;
    return 1;
}

/* ------------------------------------------------------------ program store */
#define MAXLINES 100
#define SRCLEN   96

struct Line { int no; char src[SRCLEN]; };

static struct Line g_lines[MAXLINES];
static int g_count;
static int g_vars[26];
static unsigned g_seed;

static void prog_clear(void) { g_count = 0; }

static int find_line(int no)
{
    int i;
    for (i = 0; i < g_count; ++i)
        if (g_lines[i].no == no) return i;
    return -1;
}

/* Insert in ascending numeric order; replace a line with the same number. */
static int store_line(int no, const char* src)
{
    int at = 0;
    int i;
    while (at < g_count && g_lines[at].no < no) ++at;
    if (at < g_count && g_lines[at].no == no)
        {
        unsigned n = 0;
        while (src[n] && n < SRCLEN - 1) { g_lines[at].src[n] = src[n]; ++n; }
        g_lines[at].src[n] = 0;
        return 1;
        }
    if (g_count >= MAXLINES) return 0;
    for (i = g_count; i > at; --i) g_lines[i] = g_lines[i - 1];
    g_lines[at].no = no;
    {
        unsigned n = 0;
        while (src[n] && n < SRCLEN - 1) { g_lines[at].src[n] = src[n]; ++n; }
        g_lines[at].src[n] = 0;
    }
    ++g_count;
    return 1;
}

/* The built-in NUMBER GUESSING GAME, expressed as ordinary BASIC source.
 * RND(100) draws the secret; INPUT reads the player's guess. */
static const struct { int no; const char* src; } kDemo[] = {
    {  10, "REM NUMBER GUESSING GAME" },
    {  20, "LET S = RND(100)" },
    {  30, "LET T = 0" },
    {  40, "PRINT \"GUESS A NUMBER FROM 1 TO 100\"" },
    {  50, "LET T = T + 1" },
    {  60, "PRINT \"GUESS?\"" },
    {  70, "INPUT G" },
    {  80, "IF G = S THEN 200" },
    {  90, "IF G < S THEN 120" },
    { 100, "PRINT \"TOO HIGH\"" },
    { 110, "GOTO 130" },
    { 120, "PRINT \"TOO LOW\"" },
    { 130, "IF T < 10 THEN 50" },
    { 140, "PRINT \"LOSE\"" },
    { 150, "END" },
    { 200, "PRINT \"WIN\"" },
    { 210, "END" }
};
#define NDEMO ((int)(sizeof(kDemo) / sizeof(kDemo[0])))

static int load_demo(void)
{
    int i;
    prog_clear();
    for (i = 0; i < NDEMO; ++i)
        if (!store_line(kDemo[i].no, kDemo[i].src)) return 0;
    return g_count;
}

/* ------------------------------------------------------------ expression
 * comp   := add (('='|'<>'|'<'|'>'|'<='|'>=') add)?
 * add    := mul (('+'|'-') mul)*
 * mul    := unary (('*'|'/'|MOD) unary)*
 * unary  := ('+'|'-') unary | primary
 * primary:= number | RND '(' expr ')' | '(' expr ')' | variable A-Z
 *
 * Values are wrapping 32-bit words.  Comparisons yield 1/0.  RND(n) uses a
 * self-contained deterministic LCG so a RUN is reproducible. */
enum { E_OK = 0, E_DIV0 = 1, E_SYNTAX = 2 };

struct P { const char* s; int err; };

static void ws(struct P* p)
{
    while (*p->s == ' ' || *p->s == '\t') ++p->s;
}

static int take_word(struct P* p, const char* w)
{
    const char* e;
    if (!word_at(p->s, w, &e)) return 0;
    p->s = e;
    return 1;
}

static int parse_expr(struct P* p);

static int parse_primary(struct P* p)
{
    const char* e;
    ws(p);
    /* RND(n): deterministic pseudo-random integer in 1..n. */
    if (word_at(p->s, "RND", &e) && *e == '(')
        {
        int n;
        p->s = e + 1;
        n = parse_expr(p);
        ws(p);
        if (*p->s == ')') ++p->s; else p->err = E_SYNTAX;
        if (n <= 0) return 0;
        g_seed = g_seed * 1103515245u + 12345u;
        return (int)((g_seed >> 16) % (unsigned)n) + 1;
        }
    if (*p->s >= '0' && *p->s <= '9')
        {
        int v = 0;
        while (*p->s >= '0' && *p->s <= '9')
            { v = v * 10 + (*p->s - '0'); ++p->s; }
        return v;
        }
    if (*p->s == '(')
        {
        int v;
        ++p->s;
        v = parse_expr(p);
        ws(p);
        if (*p->s == ')') ++p->s; else p->err = E_SYNTAX;
        return v;
        }
    {
        char c = *p->s;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            {
            char upper = (c >= 'a') ? (char)(c - 32) : c;
            ++p->s;
            return g_vars[upper - 'A'];
            }
    }
    p->err = E_SYNTAX;
    return 0;
}

static int parse_unary(struct P* p)
{
    ws(p);
    if (*p->s == '+') { ++p->s; return parse_unary(p); }
    if (*p->s == '-') { ++p->s; return 0 - parse_unary(p); }
    return parse_primary(p);
}

static int parse_mul(struct P* p)
{
    int v = parse_unary(p);
    for (;;)
        {
        int r;
        ws(p);
        if (*p->s == '*') { ++p->s; r = parse_unary(p); v = v * r; continue; }
        if (*p->s == '/')
            {
            ++p->s;
            r = parse_unary(p);
            if (r == 0) { p->err = E_DIV0; v = 0; }
            else if (v == (int)0x80000000u && r == -1) v = v; /* wrap */
            else v = v / r;
            continue;
            }
        if (take_word(p, "MOD"))
            {
            r = parse_unary(p);
            if (r == 0) { p->err = E_DIV0; v = 0; }
            else if (r == -1) v = 0;          /* avoid idiv overflow on INT_MIN */
            else v = v % r;
            continue;
            }
        break;
        }
    return v;
}

static int parse_add(struct P* p)
{
    int v = parse_mul(p);
    for (;;)
        {
        char op;
        int r;
        ws(p);
        op = *p->s;
        if (op != '+' && op != '-') break;
        ++p->s;
        r = parse_mul(p);
        v = (op == '+') ? (v + r) : (v - r);
        }
    return v;
}

static int parse_expr(struct P* p)
{
    int v = parse_add(p);
    for (;;)
        {
        int op = 0, r;
        ws(p);
        if (*p->s == '=') { op = 1; ++p->s; }
        else if (*p->s == '<' && p->s[1] == '>') { op = 2; p->s += 2; }
        else if (*p->s == '<' && p->s[1] == '=') { op = 5; p->s += 2; }
        else if (*p->s == '>' && p->s[1] == '=') { op = 6; p->s += 2; }
        else if (*p->s == '<') { op = 3; ++p->s; }
        else if (*p->s == '>') { op = 4; ++p->s; }
        if (!op) break;
        r = parse_add(p);
        switch (op)
            {
            case 1: v = (v == r); break;
            case 2: v = (v != r); break;
            case 3: v = (v <  r); break;
            case 4: v = (v >  r); break;
            case 5: v = (v <= r); break;
            case 6: v = (v >= r); break;
            }
        }
    return v;
}

static int eval(const char* text, int* out)
{
    struct P p;
    int v;
    p.s = text;
    p.err = E_OK;
    v = parse_expr(&p);
    ws(&p);
    if (*p.s) p.err = E_SYNTAX;
    *out = v;
    return p.err;
}

/* ------------------------------------------------------------ FOR/NEXT state
 * A small stack of active FOR contexts.  `resume` is the program index of the
 * first statement after the FOR, so NEXT can jump back to it. */
#define MAXFORS 16
struct ForCtx { int var, end, step, resume; };
static struct ForCtx g_fors[MAXFORS];
static int g_forDepth;

static void push_for(int var, int end, int step, int resume)
{
    if (g_forDepth >= MAXFORS) return;
    g_fors[g_forDepth].var = var;
    g_fors[g_forDepth].end = end;
    g_fors[g_forDepth].step = step;
    g_fors[g_forDepth].resume = resume;
    ++g_forDepth;
}

/* Index of the statement just after the NEXT matching the FOR at forIdx. */
static int after_next(int forIdx)
{
    int depth = 0;
    int i;
    for (i = forIdx + 1; i < g_count; ++i)
        {
        const char* e;
        const char* s = g_lines[i].src;
        if (word_at(s, "FOR", &e)) ++depth;
        else if (word_at(s, "NEXT", &e))
            {
            if (depth == 0) return i + 1;
            --depth;
            }
        }
    return g_count;
}

/* ------------------------------------------------------------ output helpers */
static void emit_out(const char* text)
{
    char buf[192];
    unsigned n = copy_text(buf, 0, "BA: ");
    n = copy_text(buf, n, text);
    buf[n] = 0;
    marker(buf);
}

static void emit_out_int(int value)
{
    char buf[64];
    unsigned n = copy_text(buf, 0, "BA: ");
    n = append_int(buf, n, value);
    buf[n] = 0;
    marker(buf);
}

static int read_var_index(struct P* p)
{
    char c;
    ws(p);
    c = *p->s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        {
        char upper = (c >= 'a') ? (char)(c - 32) : c;
        ++p->s;
        return upper - 'A';
        }
    p->err = E_SYNTAX;
    return 0;
}

/* Execute one statement.  *target receives a program INDEX to jump to, -1 for
 * fall-through, or -2 for "skip to after the matching NEXT".  Return 0 to
 * continue, 1 to stop (END / input failure), 2 on a syntax error. */
static int exec_stmt(const char* text, int pc, int* target)
{
    struct P p;
    const char* e;
    p.s = text;
    p.err = E_OK;
    *target = -1;
    ws(&p);
    if (!*p.s) return 0;

    if (word_at(p.s, "REM", &e)) return 0;
    if (word_at(p.s, "END", &e)) return 1;

    if (word_at(p.s, "LET", &e))
        {
        int idx, v;
        p.s = e;
        idx = read_var_index(&p);
        ws(&p);
        if (*p.s != '=') p.err = E_SYNTAX;
        else { ++p.s; if (eval(p.s, &v) != E_OK) p.err = E_SYNTAX; else g_vars[idx] = v; }
        return p.err ? 2 : 0;
        }

    if (word_at(p.s, "PRINT", &e))
        {
        p.s = e;
        ws(&p);
        if (*p.s == '"')
            {
            char buf[160];
            unsigned n = 0;
            ++p.s;
            while (*p.s && *p.s != '"' && n < sizeof(buf) - 1) buf[n++] = *p.s++;
            buf[n] = 0;
            if (*p.s == '"') ++p.s; else p.err = E_SYNTAX;
            emit_out(buf);
            }
        else
            {
            int v;
            if (eval(p.s, &v) != E_OK) p.err = E_SYNTAX;
            else emit_out_int(v);
            }
        return p.err ? 2 : 0;
        }

    if (word_at(p.s, "INPUT", &e))
        {
        int idx, v = 0, sign = 1, k = 0;
        char in[64];
        int r;
        p.s = e;
        idx = read_var_index(&p);
        if (p.err) return 2;
        r = read_line(in, sizeof(in), 60000);
        if (r < 0) return 1;                 /* input failure: stop the program */
        while (in[k] == ' ') ++k;
        if (in[k] == '-') { sign = -1; ++k; }
        else if (in[k] == '+') { ++k; }
        while (in[k] >= '0' && in[k] <= '9') { v = v * 10 + (in[k] - '0'); ++k; }
        g_vars[idx] = sign * v;
        return 0;
        }

    if (word_at(p.s, "GOTO", &e))
        {
        int no, idx;
        p.s = e;
        if (eval(p.s, &no) != E_OK) return 2;
        idx = find_line(no);
        if (idx < 0) return 2;
        *target = idx;
        return 0;
        }

    if (word_at(p.s, "IF", &e))
        {
        int cond, no;
        p.s = e;
        cond = parse_expr(&p);
        if (p.err) return 2;
        if (!take_word(&p, "THEN")) return 2;
        no = parse_expr(&p);
        if (p.err) return 2;
        if (cond)
            {
            int idx = find_line(no);
            if (idx < 0) return 2;
            *target = idx;
            }
        return 0;
        }

    if (word_at(p.s, "FOR", &e))
        {
        int idx, start, end, step = 1;
        p.s = e;
        idx = read_var_index(&p);
        ws(&p);
        if (*p.s != '=') return 2;
        ++p.s;
        start = parse_expr(&p);
        if (p.err) return 2;
        if (!take_word(&p, "TO")) return 2;
        end = parse_expr(&p);
        if (p.err) return 2;
        if (take_word(&p, "STEP")) { step = parse_expr(&p); if (p.err) return 2; }
        if (step == 0) return 2;
        g_vars[idx] = start;
        if ((step > 0 && start > end) || (step < 0 && start < end))
            *target = -2;                     /* skip the whole loop */
        else
            push_for(idx, end, step, pc + 1);
        return 0;
        }

    if (word_at(p.s, "NEXT", &e))
        {
        int var, d, v;
        struct ForCtx* fc;
        p.s = e;
        var = read_var_index(&p);
        if (p.err) return 2;
        d = g_forDepth - 1;
        while (d >= 0 && g_fors[d].var != var) --d;
        if (d < 0) return 2;
        g_forDepth = d;                       /* drop this loop and any nested */
        fc = &g_fors[d];
        v = g_vars[fc->var] + fc->step;
        g_vars[fc->var] = v;
        if ((fc->step > 0 && v <= fc->end) || (fc->step < 0 && v >= fc->end))
            *target = fc->resume;             /* loop again */
        return 0;                             /* otherwise fall through */
        }

    return 2;                                 /* unknown statement */
}

/* ------------------------------------------------------------ RUN engine */
static void run_program(void)
{
    int pc = 0;
    int steps = 0;
    int i;

    if (!g_count)
        {
        marker("BA: ERROR empty");
        return;
        }
    emit_pref_int("BA: RUN ", g_count);
    for (i = 0; i < 26; ++i) g_vars[i] = 0;
    g_forDepth = 0;
    g_seed = 0x1234ABCDu;                     /* reproducible RND sequence */

    while (pc >= 0 && pc < g_count && steps < 10000)
        {
        int target = -1;
        int rc;
        ++steps;
        rc = exec_stmt(g_lines[pc].src, pc, &target);
        if (rc == 1) break;                   /* END or input failure */
        if (rc == 2)
            {
            emit_pref_int("BA: ERROR ", g_lines[pc].no);
            break;
            }
        if (target == -2)
            pc = after_next(pc);
        else if (target >= 0)
            pc = target;
        else
            ++pc;
        }
    if (steps >= 10000) marker("BA: ERROR limit");
}

/* ------------------------------------------------------------ commands */
static void list_program(void)
{
    int i;
    if (!g_count) { marker("BA: ERROR empty"); return; }
    for (i = 0; i < g_count; ++i)
        {
        char buf[192];
        unsigned n = copy_text(buf, 0, "BA: ");
        n = append_int(buf, n, g_lines[i].no);
        buf[n++] = ' ';
        n = copy_text(buf, n, g_lines[i].src);
        buf[n] = 0;
        marker(buf);
        }
}

/* Treat the first word of a line as a command.  Returns 1 to quit. */
static int run_command(const char* line)
{
    const char* e;
    char buf[192];
    unsigned n;

    /* Command echo: BA: > <line> */
    n = copy_text(buf, 0, "BA: > ");
    n = copy_text(buf, n, line);
    buf[n] = 0;
    marker(buf);

    /* A leading number enters a numbered BASIC source line. */
    {
        const char* q = line;
        while (*q == ' ' || *q == '\t') ++q;
        if (*q >= '0' && *q <= '9')
            {
            int no = 0;
            while (*q >= '0' && *q <= '9') { no = no * 10 + (*q - '0'); ++q; }
            if (store_line(no, q)) marker("BA: OK");
            else marker("BA: ERROR full");
            return 0;
            }
        }

    if (word_at(line, "LOAD", &e))
        {
        if (word_at(e, "DEMO", &e))
            {
            int nlines = load_demo();
            emit_pref_int("BA: LOADED DEMO ", nlines);
            }
        else marker("BA: ERROR load");
        return 0;
        }
    if (word_at(line, "RUN", &e))  { run_program(); return 0; }
    if (word_at(line, "LIST", &e)) { list_program(); return 0; }
    if (word_at(line, "NEW", &e))  { prog_clear(); marker("BA: NEW"); return 0; }
    if (word_at(line, "QUIT", &e)) { return 1; }
    marker("BA: ERROR cmd");
    return 0;
}

/* ------------------------------------------------------------ entry point */
int app_main(void)
{
    char line[128];
    int r;

    marker("BA: BASIC READY");
    if (con_acquire() < 0)
        {
        marker("BA: ERROR console");
        marker("BA: DONE");
        return 1;
        }

    for (;;)
        {
        con_puts_raw("BASIC> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                     /* timeout / input error */
        if (run_command(line)) break;         /* QUIT */
        }

    marker("BA: DONE");
    con_release();
    return 0;
}
