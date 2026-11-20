/*
 * wc.c -- a small WORD COUNT utility for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...) (euseru.def).  ld defines the
 * IAT slot at codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits
 * the PE2 import section and the loader overwrites the slot with the resolved
 * export address before the process resumes.  Markers go through that ordinal,
 * with a DebugPrint fallback so the trace is always visible on the console.
 *
 * File I/O uses PortFile IPC (runtime.c file_open/file_read/file_close), i.e.
 * the portfilesrv service, exactly like FGREP / HEXDUMP / FCOPY / TYPE.
 * file_read is capped at 256 bytes per ChannelRequest, so a small text file is
 * slurped in chunks and then counted in one pass: lines (newline-separated,
 * with a final unterminated line counted), whitespace-delimited words, and
 * characters (bytes).  A built-in self-test counts an embedded string so the
 * engine can be proven without FAT.  Console commands (wc / selftest / help /
 * quit) arrive via PortCon read_line.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*wc_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&wc_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------ small string helpers (freestanding) ------- */

static unsigned append_str(char* out, unsigned n, const char* text)
{
    while (*text) out[n++] = *text++;
    out[n] = 0;
    return n;
}

static unsigned append_uint(char* out, unsigned n, unsigned value)
{
    char digits[12];
    int d = 0;
    if (!value) digits[d++] = '0';
    while (value) { digits[d++] = (char)('0' + value % 10); value /= 10; }
    while (d) out[n++] = digits[--d];
    out[n] = 0;
    return n;
}

/* Case-insensitive compare of a command word to a literal. */
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

static const char* skip_spaces(const char* p)
{
    while (*p == ' ') ++p;
    return p;
}

/* Copy one space-delimited token into a NUL-terminated buffer. */
static void copy_token(char* out, unsigned cap, const char* text)
{
    unsigned n = 0;
    text = skip_spaces(text);
    while (*text && *text != ' ' && n + 1 < cap) out[n++] = *text++;
    out[n] = 0;
}

/* ------------------------------ counting engine --------------------------- */

/* Whitespace that separates words (space, tab, CR, LF, VT, FF). */
static int is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r'
        || c == '\v' || c == '\f';
}

/* One pass over a byte range: lines = number of '\n' plus one for a final
 * unterminated line; words = maximal runs of non-whitespace; chars = bytes. */
static void count_bytes(const char* data, unsigned len,
                        unsigned* out_lines, unsigned* out_words,
                        unsigned* out_chars)
{
    unsigned lines = 0, words = 0;
    int in_word = 0;
    unsigned i;
    for (i = 0; i < len; ++i)
        {
        char c = data[i];
        if (c == '\n') ++lines;
        if (is_space(c))
            in_word = 0;
        else if (!in_word)
            { ++words; in_word = 1; }
        }
    if (len && data[len - 1] != '\n') ++lines;  /* final unterminated line */
    *out_lines = lines;
    *out_words = words;
    *out_chars = len;
}

/* Emit "WC: <name>: <lines> lines <words> words <chars> chars". */
static void report(const char* name, const char* data, unsigned len)
{
    unsigned lines, words, chars;
    count_bytes(data, len, &lines, &words, &chars);
    char msg[256];
    unsigned n = 0;
    n = append_str(msg, n, "WC: ");
    n = append_str(msg, n, name);
    n = append_str(msg, n, ": ");
    n = append_uint(msg, n, lines);
    n = append_str(msg, n, " lines ");
    n = append_uint(msg, n, words);
    n = append_str(msg, n, " words ");
    n = append_uint(msg, n, chars);
    n = append_str(msg, n, " chars");
    marker(msg);
}

/* ------------------------------ file utilities --------------------------- */

#define CAP 8192
static char g_data[CAP + 1];   /* whole (small) file staging buffer */
static char line[128];

/* Read an entire small file into dst (up to cap bytes).  Returns the byte
 * count, or -1 when the file cannot be opened/read. */
static int slurp(const char* path, char* dst, unsigned cap)
{
    int handle = file_open(path);
    if (handle < 0) return -1;
    unsigned total = 0;
    for (;;)
        {
        unsigned room = cap - total;
        if (!room) break;
        unsigned want = room < 256 ? room : 256;
        int n = file_read(handle, dst + total, want);
        if (n <= 0) break;              /* 0 = EOF, <0 = read error */
        total += (unsigned)n;
        }
    file_close(handle);
    return (int)total;
}

/* ------------------------------ commands --------------------------------- */

/* Embedded string for the FAT-free self-test: "Hello world\nSecond line" is
 * 2 lines, 4 whitespace-delimited words and 23 characters. */
static const char kBuiltin[] = "Hello world\nSecond line";

static void do_selftest(void)
{
    unsigned len = (unsigned)(sizeof(kBuiltin) - 1);
    report("BUILTIN", kBuiltin, len);
    unsigned lines, words, chars;
    count_bytes(kBuiltin, len, &lines, &words, &chars);
    marker((lines == 2u && words == 4u && chars == 23u)
           ? "WC: selftest OK" : "WC: selftest FAIL");
}

/* wc <file>: slurp via PortFile IPC, count and report.  Returns 1 on error. */
static int do_wc(const char* arg)
{
    char name[64];
    copy_token(name, sizeof(name), arg);
    if (!*name)
        {
        marker("WC: usage: wc <file>");
        return 1;
        }
    int count = slurp(name, g_data, CAP);
    if (count < 0)
        {
        marker("WC: wc: open failed");
        return 1;
        }
    report(name, g_data, (unsigned)count);
    return 0;
}

static void do_help(void)
{
    print("WC: commands: wc <file>  selftest  help  quit");
}

/* Parse "command [arguments]" from a console line.  1 means quit. */
static int run_command(const char* input)
{
    input = skip_spaces(input);
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = skip_spaces(word);

    if      (word_is(input, length, "wc"))    return do_wc(arg);
    else if (word_is(input, length, "selftest") ||
             word_is(input, length, "test"))  do_selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))     do_help();
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))     return 1;
    else marker("WC: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("WC: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&wc_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("WC: WORD COUNT");
    marker(s_bound ? "WC: EUSER ord 915 bound"
                   : "WC: using DebugPrint marker path");

    /* An embedded-string count on startup guarantees the WC markers even with
     * no serial input (and proves the engine before touching FAT). */
    do_selftest();

    for (;;)
        {
        print("WC> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;               /* timeout / input error -> done */
        if (run_command(line)) break;   /* quit / usage error */
        }

    marker("WC: DONE");
    con_release();
    return 0;
}
