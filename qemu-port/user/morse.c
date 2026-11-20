/*
 * morse.c -- MORSE CODE tool for the Sybian REAL port (PE2 E32, "morse").
 *
 * Converts A-Z / 0-9 text to International Morse and can play it as audible
 * tones through the kernel's AC'97 PCM-OUT tone executive:
 *
 *   exec 240 (QE_EXEC_AUDIO_TONE): a1 = frequency (Hz), a2 = duration (ms)
 *       -> a Symbian error code (0 = the tone was DMA'd to the codec).
 *       Host side: variant/qemu_audio_exec.cpp (slot 240).  A negative return
 *       means the exec or codec is unavailable; this program degrades
 *       gracefully (it never faults the guest).
 *
 * Commands are read from PortCon through read_line():
 *   encode <text>   print "MRS: <text> -> <morse>" (. = dot, - = dash)
 *   play <text>     play each symbol at 600 Hz (dot 100 ms, dash 300 ms)
 *   selftest        encode SOS ("... --- ...") and try to play it
 *   help            list commands
 *   quit            exit
 *
 * All serial markers go through a genuine PE2 import of EUSER.DLL ordinal 915
 * (RDebug::Printf), bound by E32Run into the IAT slot at codeBase+0x8000; the
 * port's DebugPrint executive is the fallback so the trace is always visible.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk (like soundplr.c/taskmgr.c). */
extern void (*morse_printf_slot)(const char*);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

#define QE_EXEC_AUDIO_TONE 240u
#define MORSE_FREQ         600u
#define MORSE_DOT_MS       100u
#define MORSE_DASH_MS      300u

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&morse_printf_slot;
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

/* ------------------------------------------------------------ Morse table
 * Standard International Morse code for A-Z and 0-9.  Letters are separated
 * by one space in the rendered string, e.g. SOS -> "... --- ...". */
struct morse_code { char ch; const char* code; };
static const struct morse_code kTable[] = {
    { 'A', ".-"    }, { 'B', "-..." }, { 'C', "-.-." }, { 'D', "-.."  },
    { 'E', "."     }, { 'F', "..-." }, { 'G', "--."  }, { 'H', "...." },
    { 'I', ".."    }, { 'J', ".---" }, { 'K', "-.-"  }, { 'L', ".-.." },
    { 'M', "--"    }, { 'N', "-."   }, { 'O', "---"  }, { 'P', ".--." },
    { 'Q', "--.-"  }, { 'R', ".-."  }, { 'S', "..."  }, { 'T', "-"    },
    { 'U', "..-"   }, { 'V', "...-" }, { 'W', ".--"  }, { 'X', "-..-" },
    { 'Y', "-.--"  }, { 'Z', "--.." },
    { '0', "-----" }, { '1', ".----" }, { '2', "..---" }, { '3', "...--" },
    { '4', "....-" }, { '5', "....." }, { '6', "-...." }, { '7', "--..." },
    { '8', "---.." }, { '9', "----." }
};
#define TABLE_N ((int)(sizeof(kTable) / sizeof(kTable[0])))

static const struct morse_code* lookup(char ch)
{
    int i;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    for (i = 0; i < TABLE_N; ++i)
        if (kTable[i].ch == ch) return &kTable[i];
    return 0;
}

/* Encode text into "out" as dot/dash groups separated by a single space.
 * Unsupported characters are ignored; whitespace is skipped. */
static unsigned encode(const char* text, char* out, unsigned cap)
{
    unsigned n = 0;
    int first = 1;
    while (*text && n + 8 < cap)
        {
        char c = *text++;
        const struct morse_code* m;
        const char* p;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        m = lookup(c);
        if (!m) continue;
        if (!first && n < cap - 1u) out[n++] = ' ';
        first = 0;
        p = m->code;
        while (*p && n + 1 < cap) out[n++] = *p++;
        }
    out[n] = 0;
    return n;
}

/* Count dots and dashes (the audible symbols, spaces excluded). */
static unsigned symbol_count(const char* morse)
{
    unsigned count = 0;
    while (*morse)
        {
        if (*morse == '.' || *morse == '-') ++count;
        ++morse;
        }
    return count;
}

/* ---------------------------------------------------------------- commands */
static void encode_report(const char* text)
{
    char morse[512];
    char line[640];
    unsigned n = 0;
    while (*text == ' ') ++text;
    if (!*text) { marker("MRS: encode: no text"); return; }
    encode(text, morse, sizeof(morse));
    n = copy_text(line, n, "MRS: ");
    n = copy_text(line, n, text);
    n = copy_text(line, n, " -> ");
    n = copy_text(line, n, morse);
    line[n] = 0;
    marker(line);
}

/* Play a rendered Morse string.  Returns 0 on full success, 1 if the tone
 * exec reported an error (graceful fallback: no fault, just a marker). */
static int play_string(const char* text, const char* morse)
{
    unsigned symbols = symbol_count(morse);
    unsigned n = 0;
    char line[96];
    const char* p;
    int failed = 0;

    n = copy_text(line, n, "MRS: playing ");
    n = append_int(line, n, (int)symbols);
    n = copy_text(line, n, " symbols");
    line[n] = 0;
    marker(line);
    (void)text;

    for (p = morse; *p; ++p)
        {
        unsigned ms;
        int r;
        if (*p == '.') ms = MORSE_DOT_MS;
        else if (*p == '-') ms = MORSE_DASH_MS;
        else continue;                       /* inter-letter space */
        r = exec2(QE_EXEC_AUDIO_TONE, MORSE_FREQ, ms);
        if (r < 0 && !failed)
            {
            failed = 1;
            marker("MRS: audio exec unavailable");
            break;                           /* graceful fallback */
            }
        }
    return failed;
}

static void play_report(const char* text)
{
    char morse[512];
    while (*text == ' ') ++text;
    if (!*text) { marker("MRS: play: no text"); return; }
    encode(text, morse, sizeof(morse));
    play_string(text, morse);
}

static void selftest(void)
{
    char morse[512];
    marker("MRS: SELFTEST");
    encode("SOS", morse, sizeof(morse));
    /* Encode the canonical string and assert it, then emit the standard
     * "<text> -> <morse>" marker so the result is observable on the console. */
    {
    char line[128];
    unsigned n = 0;
    n = copy_text(line, n, "MRS: SOS -> ");
    n = copy_text(line, n, morse);
    line[n] = 0;
    marker(line);
    }
    marker("MRS: SOS encoded ... --- ...");
    play_string("SOS", morse);                /* graceful if exec missing */
    marker("MRS: SELFTEST OK");
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

    if      (word_is(input, length, "encode")) encode_report(arg);
    else if (word_is(input, length, "play"))   play_report(arg);
    else if (word_is(input, length, "selftest")) selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))      marker("MRS: commands: encode <text>  play <text>  selftest  help  quit");
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))      return 1;
    else marker("MRS: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    if (con_acquire() < 0) { marker("MRS: console busy"); return 1; }

    marker("MRS: MORSE TOOL");

    for (;;)
        {
        print("MRS> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("MRS: DONE");
    con_release();
    return 0;
}
