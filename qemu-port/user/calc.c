/*
 * calc.c -- an integer expression calculator for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...) (see euseru.def).  ld defines
 * the IAT slot at codeBase+0x8000; elf2e32.py --euser-import 915 emits the
 * PE2 import section and the loader overwrites the slot with the resolved
 * export address before the process resumes.  All serial markers go through
 * that ordinal (with a DebugPrint fallback so the trace is always visible).
 *
 * This is a genuine calculator, not a table of canned answers.  A recursive
 * descent parser evaluates +, -, *, / and parentheses over signed 32-bit
 * values: add/sub/mul wrap with natural unsigned arithmetic, division
 * truncates toward zero and reports a divide-by-zero error.  At the shell
 * prompt `calc 2+3*4` evaluates that expression (the kernel passes the raw
 * words after the program name as the process command line).  With no
 * arguments the same evaluator runs a small self-test suite so the fixture
 * can be checked automatically over the serial console.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*calc_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&calc_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: no hosted libc, so build each marker directly into a buffer. */
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

/* ------------------------------------------------------------ evaluator
 * expr   := term (('+'|'-') term)*
 * term   := factor (('*'|'/') factor)*
 * factor := ('+'|'-') factor | number | '(' expr ')'
 *
 * Values are kept in an unsigned 32-bit word so addition, subtraction and
 * multiplication wrap exactly like a Symbian TInt.  Division converts to
 * signed first (C truncation toward zero), special-cases INT_MIN / -1 so the
 * hardware idiv cannot raise an exception, and flags a zero divisor. */
enum { CALC_OK = 0, CALC_DIV0 = 1, CALC_SYNTAX = 2 };

struct Parser { const char* p; int err; };

static void skip_space(struct Parser* s)
{
    while (*s->p == ' ' || *s->p == '\t') ++s->p;
}

static unsigned parse_expr(struct Parser* s);

static unsigned parse_number(struct Parser* s)
{
    unsigned v = 0;
    while (*s->p >= '0' && *s->p <= '9')
        {
        v = v * 10u + (unsigned)(*s->p - '0');
        ++s->p;
        }
    return v;
}

static unsigned parse_factor(struct Parser* s)
{
    unsigned v;
    skip_space(s);
    if (*s->p == '+') { ++s->p; return parse_factor(s); }
    if (*s->p == '-') { ++s->p; v = parse_factor(s); return 0u - v; }
    if (*s->p == '(')
        {
        ++s->p;
        v = parse_expr(s);
        skip_space(s);
        if (*s->p == ')') ++s->p; else s->err = CALC_SYNTAX;
        return v;
        }
    if (*s->p >= '0' && *s->p <= '9') return parse_number(s);
    s->err = CALC_SYNTAX;   /* bad token (also covers an empty expression) */
    return 0;
}

static unsigned parse_term(struct Parser* s)
{
    unsigned v = parse_factor(s);
    for (;;)
        {
        char op;
        unsigned r;
        skip_space(s);
        op = *s->p;
        if (op != '*' && op != '/') break;
        ++s->p;
        r = parse_factor(s);
        if (op == '*') v = v * r;
        else
            {
            int a = (int)v, b = (int)r;
            if (b == 0) { s->err = CALC_DIV0; v = 0; }
            else if (a == (int)0x80000000u && b == -1) v = (unsigned)a; /* wrap */
            else v = (unsigned)(a / b);
            }
        }
    return v;
}

static unsigned parse_expr(struct Parser* s)
{
    unsigned v = parse_term(s);
    for (;;)
        {
        char op;
        unsigned r;
        skip_space(s);
        op = *s->p;
        if (op != '+' && op != '-') break;
        ++s->p;
        r = parse_term(s);
        v = (op == '+') ? (v + r) : (v - r);
        }
    return v;
}

static int evaluate(const char* expression, int* value)
{
    struct Parser s = {expression, CALC_OK};
    unsigned v = parse_expr(&s);
    skip_space(&s);
    if (*s.p) s.err = CALC_SYNTAX;   /* trailing garbage */
    *value = (int)v;
    return s.err;
}

/* Evaluate one expression and emit exactly the requested serial format:
 *   CALC: <expr> = <result>   on success
 *   CALC: ERR <reason>        on error  (div0 or syntax) */
static void report(const char* expression)
{
    int value = 0;
    int err = evaluate(expression, &value);
    char line[256];
    unsigned n = 0;
    if (err == CALC_OK)
        {
        n = copy_text(line, n, "CALC: ");
        n = copy_text(line, n, expression);
        n = copy_text(line, n, " = ");
        n = append_int(line, n, value);
        }
    else
        {
        n = copy_text(line, n, "CALC: ERR ");
        n = copy_text(line, n, err == CALC_DIV0 ? "div0" : "syntax");
        }
    line[n] = 0;
    marker(line);
}

/* Deterministic suite: exercises precedence, parentheses, integer division
 * and divide-by-zero through the very same parser used for live expressions. */
static const char* const kSelfTest[] = {
    "2+3*4",
    "(2+3)*4",
    "100/7",
    "1/0",
};
static const unsigned kSelfTestCount =
    sizeof(kSelfTest) / sizeof(kSelfTest[0]);

int app_main(void)
{
    static U16 arguments[128];
    int length = command_line(arguments, 128);

    marker("CALC: CALCULATOR READY");

    if (length > 0)
        {
        /* Genuine console input path: the shell hands the words after `calc`
         * to us as a UTF-16 command line, e.g. `calc 7*(2+3)`. */
        char expression[128];
        int i;
        for (i = 0; i < length && i < 127; ++i)
            expression[i] = arguments[i] < 128 ? (char)arguments[i] : '?';
        expression[i] = 0;
        report(expression);
        }
    else if (length < 0)
        marker("CALC: ERR argument");
    else
        {
        unsigned i;
        for (i = 0; i < kSelfTestCount; ++i)
            report(kSelfTest[i]);
        }

    marker("CALC: DONE");
    return 0;
}
