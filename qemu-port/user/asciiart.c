/*
 * asciiart.c -- ASCII ART generator for the Sybian REAL port (PE2 E32, "E32").
 *
 * An interactive end-user tool driven from PortCon through read_line():
 *   banner <word>   render the word as big block letters with a built-in 5x5
 *                   pixel font (A-Z / 0-9), five rows per word using '#' and
 *                   ' '; supported glyphs are joined by one space column.
 *   cowsay <text>   draw a small cow speech bubble containing the text.
 *   selftest        render the banner "EKA2" plus one cowsay line.
 *   help            list the commands.
 *   quit            exit.
 *
 * All serial markers go through a genuine PE2 import of EUSER.DLL ordinal 915
 * (RDebug::Printf), bound by E32Run into the IAT slot at codeBase+0x8000.  The
 * port's DebugPrint executive is the fallback so the trace is always visible
 * on the serial console.  Marker format (one plain string, no % specifiers):
 *   AA: ASCII ART               startup
 *   AA: banner <word>           banner header
 *   AA: <##### ...>             each of the five rendered rows
 *   AA: cowsay                  cow header
 *   AA: DONE                    shutdown
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk (like morse.c/snakE.c). */
extern void (*asciiart_printf_slot)(const char*);

/* Markers are plain strings so the variadic EUSER printf receives exactly one
 * argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&asciiart_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: no hosted libc. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

/* ------------------------------------------------------------ 5x5 pixel font
 * Each glyph is five rows; one byte per row, bits 4..0 = columns 0..4 (bit 4
 * is the leftmost column).  A set bit renders as '#', a clear bit as ' '.
 * Indices 0..25 are A-Z, 26..35 are 0-9. */
static const unsigned char kFont[36][5] = {
    /* A */ { 0x0E,0x11,0x1F,0x11,0x11 },
    /* B */ { 0x1E,0x11,0x1E,0x11,0x1E },
    /* C */ { 0x0F,0x10,0x10,0x10,0x0F },
    /* D */ { 0x1E,0x11,0x11,0x11,0x1E },
    /* E */ { 0x1F,0x10,0x1E,0x10,0x1F },
    /* F */ { 0x1F,0x10,0x1E,0x10,0x10 },
    /* G */ { 0x0F,0x10,0x13,0x11,0x0E },
    /* H */ { 0x11,0x11,0x1F,0x11,0x11 },
    /* I */ { 0x1F,0x04,0x04,0x04,0x1F },
    /* J */ { 0x07,0x01,0x01,0x11,0x0E },
    /* K */ { 0x11,0x12,0x1C,0x12,0x11 },
    /* L */ { 0x10,0x10,0x10,0x10,0x1F },
    /* M */ { 0x11,0x1B,0x15,0x11,0x11 },
    /* N */ { 0x11,0x19,0x15,0x13,0x11 },
    /* O */ { 0x0E,0x11,0x11,0x11,0x0E },
    /* P */ { 0x1E,0x11,0x1E,0x10,0x10 },
    /* Q */ { 0x0E,0x11,0x15,0x13,0x0D },
    /* R */ { 0x1E,0x11,0x1E,0x13,0x11 },
    /* S */ { 0x0F,0x10,0x0E,0x01,0x1E },
    /* T */ { 0x1F,0x04,0x04,0x04,0x04 },
    /* U */ { 0x11,0x11,0x11,0x11,0x0E },
    /* V */ { 0x11,0x11,0x11,0x0A,0x04 },
    /* W */ { 0x11,0x11,0x15,0x1B,0x11 },
    /* X */ { 0x11,0x0A,0x04,0x0A,0x11 },
    /* Y */ { 0x11,0x0A,0x04,0x04,0x04 },
    /* Z */ { 0x1F,0x02,0x04,0x08,0x1F },
    /* 0 */ { 0x0E,0x13,0x15,0x19,0x0E },
    /* 1 */ { 0x04,0x0C,0x04,0x04,0x1F },
    /* 2 */ { 0x0E,0x11,0x02,0x04,0x1F },
    /* 3 */ { 0x1E,0x01,0x0E,0x01,0x1E },
    /* 4 */ { 0x11,0x11,0x1F,0x01,0x01 },
    /* 5 */ { 0x1F,0x10,0x1E,0x01,0x1E },
    /* 6 */ { 0x0E,0x10,0x1E,0x11,0x0E },
    /* 7 */ { 0x1F,0x01,0x02,0x04,0x04 },
    /* 8 */ { 0x0E,0x11,0x0E,0x11,0x0E },
    /* 9 */ { 0x0E,0x11,0x0F,0x01,0x0E },
};

/* Case-insensitive glyph lookup.  Returns -1 for unsupported characters. */
static int glyph_index(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    return -1;
}

/* Render one 5-row banner.  Each supported glyph contributes five pixel
 * columns plus one separating space column; unsupported characters are
 * skipped.  Emits the header marker followed by exactly five row markers. */
static void banner(const char* word)
{
    char line[256];
    char row[256];
    unsigned n;
    int r;

    while (*word == ' ') ++word;
    if (!*word) { marker("AA: banner: no word"); return; }

    n = 0;
    n = copy_text(line, n, "AA: banner ");
    n = copy_text(line, n, word);
    line[n] = 0;
    marker(line);

    for (r = 0; r < 5; ++r)
        {
        const char* p = word;
        unsigned col = 0;
        int first = 1;
        while (*p && col + 6u < sizeof(row))
            {
            int idx = glyph_index(*p++);
            if (idx < 0) continue;
            if (!first) row[col++] = ' ';        /* one column between letters */
            first = 0;
                {
                unsigned char bits = kFont[idx][r];
                int b;
                for (b = 4; b >= 0; --b)
                    row[col++] = (bits & (1u << b)) ? '#' : ' ';
                }
            }
        row[col] = 0;

        n = 0;
        n = copy_text(line, n, "AA: ");
        n = copy_text(line, n, row);
        line[n] = 0;
        marker(line);
        }
}

/* A small cow speech bubble around the supplied text. */
static void cowsay(const char* text)
{
    char line[256];
    unsigned n;

    while (*text == ' ') ++text;
    if (!*text) text = "moo";

    marker("AA: cowsay");
    marker("AA:  --------------------");
    n = 0;
    n = copy_text(line, n, "AA:  < ");
    n = copy_text(line, n, text);
    n = copy_text(line, n, " >");
    line[n] = 0;
    marker(line);
    marker("AA:  --------------------");
    marker("AA:          \\   ^__^");
    marker("AA:           \\  (oo)\\_______");
    marker("AA:              (__)\\       )\\/\\");
    marker("AA:                  ||----w |");
    marker("AA:                  ||     ||");
}

static void selftest(void)
{
    banner("EKA2");
    cowsay("Hello from EKA2!");
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

    if      (word_is(input, length, "banner"))   banner(arg);
    else if (word_is(input, length, "cowsay"))   cowsay(arg);
    else if (word_is(input, length, "selftest")) selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))        marker("AA: commands: banner <word>  cowsay <text>  selftest  help  quit");
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))        return 1;
    else marker("AA: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    if (con_acquire() < 0) { marker("AA: console busy"); return 1; }

    marker("AA: ASCII ART");

    for (;;)
        {
        print("AA> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;                 /* quit */
        }

    marker("AA: DONE");
    con_release();
    return 0;
}
