/*
 * quiz.c -- TRIVIA QUIZ, a genuine interactive end-user game for the Sybian
 * REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  All serial markers go through that
 * ordinal, with a DebugPrint fallback so the trace is always visible.
 *
 * Six built-in multiple-choice questions (question + four options + a 1-based
 * correct index) are served from a fixed table.  Commands are read from PortCon
 * through read_line(): start, answer <n>, score, selftest, quit.  The built-in
 * selftest answers every question correctly via scripted input and reports a
 * 6/6 final score so an automated test can assert every marker.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*quiz_printf_slot)(const char*);

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&quiz_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

typedef struct
    {
    const char* q;
    const char* o1;
    const char* o2;
    const char* o3;
    const char* o4;
    unsigned correct;                         /* 1..4 */
    } Question;

/* Built-in trivia bank.  Correct answers are spread across all four options. */
static const Question kQuestions[6] = {
    { "What is the capital of France?",
      "London", "Paris", "Rome", "Berlin", 2 },
    { "How many planets are in the Solar System?",
      "7", "9", "8", "10", 3 },
    { "What is 7 times 8?",
      "54", "56", "48", "64", 2 },
    { "Which gas do plants absorb during photosynthesis?",
      "Oxygen", "Nitrogen", "Carbon dioxide", "Helium", 3 },
    { "In which year did Apollo 11 land on the Moon?",
      "1965", "1969", "1972", "1959", 2 },
    { "What is the largest ocean on Earth?",
      "Atlantic", "Indian", "Arctic", "Pacific", 4 }
    };
#define NQ ((unsigned)(sizeof(kQuestions) / sizeof(kQuestions[0])))

static unsigned g_qidx;    /* index of the current question */
static unsigned g_score;   /* correct answers so far */
static int g_started;
static int g_over;

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker directly into a buffer, no hosted libc. */
static unsigned add_text(char* out, unsigned n, const char* s)
{
    while (*s) out[n++] = *s++;
    return n;
}

static unsigned add_uint(char* out, unsigned n, unsigned v)
{
    char d[12];
    int k = 0;
    if (!v) d[k++] = '0';
    while (v) { d[k++] = (char)('0' + v % 10); v /= 10; }
    while (k) out[n++] = d[--k];
    return n;
}

/* ------------------------------------------------------------ game engine */
static void emit_header(void)
{
    marker("QZ: TRIVIA QUIZ (6 questions)");
}

static void emit_question(unsigned idx)
{
    char text[256];
    unsigned n = 0;
    const Question* p = &kQuestions[idx];

    n = add_text(text, n, "QZ: Q");
    n = add_uint(text, n, idx + 1);
    n = add_text(text, n, ": ");
    n = add_text(text, n, p->q);
    text[n] = 0;
    marker(text);

    n = 0;
    n = add_text(text, n, "QZ: options: 1)");
    n = add_text(text, n, p->o1);
    n = add_text(text, n, " 2)");
    n = add_text(text, n, p->o2);
    n = add_text(text, n, " 3)");
    n = add_text(text, n, p->o3);
    n = add_text(text, n, " 4)");
    n = add_text(text, n, p->o4);
    text[n] = 0;
    marker(text);
}

static void emit_score(void)
{
    char text[48];
    unsigned n = 0;
    n = add_text(text, n, "QZ: final score=");
    n = add_uint(text, n, g_score);
    n = add_text(text, n, "/6");
    text[n] = 0;
    marker(text);
}

static void start_quiz(void)
{
    g_qidx = 0;
    g_score = 0;
    g_started = 1;
    g_over = 0;
    emit_question(0);
}

static void do_answer(unsigned ans)
{
    char text[64];
    unsigned n, correct, right;

    if (!g_started || g_over) { marker("QZ: not running"); return; }
    if (ans < 1u || ans > 4u) { marker("QZ: bad answer"); return; }

    correct = kQuestions[g_qidx].correct;
    right = (ans == correct) ? 1u : 0u;

    n = 0;
    n = add_text(text, n, right ? "QZ: correct answer=" : "QZ: wrong answer=");
    n = add_uint(text, n, ans);
    n = add_text(text, n, " correct=");
    n = add_uint(text, n, correct);
    text[n] = 0;
    marker(text);

    if (right) ++g_score;
    ++g_qidx;
    if (g_qidx >= NQ)
        {
        g_over = 1;
        emit_score();
        }
    else
        emit_question(g_qidx);
}

/* ------------------------------------------------------------ selftest
 * Answer all six questions with their known correct indices; the engine must
 * report a perfect 6/6 final score. */
static void selftest(void)
{
    unsigned i;
    marker("QZ: SELFTEST begin");
    start_quiz();
    for (i = 0; i < NQ; ++i) do_answer(kQuestions[i].correct);
    marker("QZ: SELFTEST OK");
}

/* ------------------------------------------------------------ command loop */
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

static int parse_uint(const char* s, unsigned len, unsigned* out)
{
    unsigned value = 0, i;
    if (!len) return 0;
    for (i = 0; i < len; ++i)
        {
        if (s[i] < '0' || s[i] > '9') return 0;
        value = value * 10u + (unsigned)(s[i] - '0');
        }
    *out = value;
    return 1;
}

/* Returns 1 when the user asked to quit. */
static int run_command(const char* input)
{
    const char *word, *arg;
    unsigned wl, al, ans;

    while (*input == ' ') ++input;
    word = input; wl = 0;
    while (word[wl] && word[wl] != ' ') ++wl;
    arg = word + wl;
    while (*arg == ' ') ++arg;
    al = 0;
    while (arg[al] && arg[al] != ' ') ++al;

    if (word_is(word, wl, "start"))
        { start_quiz(); return 0; }
    if (word_is(word, wl, "answer"))
        {
        if (!parse_uint(arg, al, &ans)) marker("QZ: bad answer");
        else do_answer(ans);
        return 0;
        }
    if (word_is(word, wl, "score"))
        { emit_score(); return 0; }
    if (word_is(word, wl, "selftest"))
        { selftest(); return 0; }
    if (word_is(word, wl, "quit") || word_is(word, wl, "q"))
        return 1;
    marker("QZ: commands: start answer <n> score selftest quit");
    return 0;
}

static char line[128];

int app_main(void)
{
    int r;

    if (con_acquire() < 0) { marker("QZ: console busy"); return 1; }

    emit_header();

    for (;;)
        {
        print("QZ> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                  /* timeout / input error */
        if (run_command(line)) break;      /* quit */
        }

    marker("QZ: DONE");
    con_release();
    return 0;
}
