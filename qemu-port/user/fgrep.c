/*
 * fgrep.c -- a small FILE SEARCH / GREP utility for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...) (euseru.def).  ld defines the
 * IAT slot at codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits
 * the PE2 import section and the loader overwrites the slot with the resolved
 * export address before the process resumes.  Markers go through that ordinal,
 * with a DebugPrint fallback so the trace is always visible on the console.
 *
 * File I/O uses PortFile IPC (runtime.c file_open/file_read/file_close), i.e.
 * the portfilesrv service, exactly like FILEMGR / FCOPY / TYPE.  file_read is
 * capped at 256 bytes per ChannelRequest, so a small text file is slurped in
 * chunks and then scanned line by line for a literal substring.  Every matching
 * line is reported with its 1-based line number.  A built-in self-test searches
 * an embedded string table so the matching logic can be proven without FAT.
 * Console commands (find / selftest / help / quit) arrive via PortCon read_line.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*fgrep_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&fgrep_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------ small string helpers (freestanding) ------- */

static unsigned cstrlen(const char* s)
{
    unsigned n = 0;
    while (s[n]) ++n;
    return n;
}

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

/* Return a pointer just past the first space-delimited token. */
static const char* after_token(const char* p)
{
    p = skip_spaces(p);
    while (*p && *p != ' ') ++p;
    return p;
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

/* Case-sensitive literal substring search within a bounded byte range. */
static int contains(const char* text, unsigned textLen,
                    const char* needle, unsigned needleLen)
{
    if (!needleLen || needleLen > textLen) return 0;
    unsigned i;
    for (i = 0; i + needleLen <= textLen; ++i)
        {
        unsigned j = 0;
        while (j < needleLen && text[i + j] == needle[j]) ++j;
        if (j == needleLen) return 1;
        }
    return 0;
}

/* Scan a text buffer line by line.  Emits:
 *   FG: searching <name> for '<needle>'
 *   FG: <name>:<line>: <text>     (for every matching line)
 *   FG: <N> matches
 * Returns the number of matches. */
static int search_text(const char* name, const char* data, unsigned len,
                       const char* needle)
{
    char msg[384];
    unsigned n = 0;
    n = append_str(msg, n, "FG: searching ");
    n = append_str(msg, n, name);
    n = append_str(msg, n, " for '");
    n = append_str(msg, n, needle);
    n = append_str(msg, n, "'");
    marker(msg);

    unsigned needleLen = cstrlen(needle);
    unsigned matches = 0;
    unsigned lineNo = 0;
    unsigned i = 0;
    char text[256];
    while (i < len)
        {
        unsigned start = i;
        while (i < len && data[i] != '\n') ++i;
        unsigned lineLen = i - start;
        if (lineLen && data[start + lineLen - 1] == '\r') --lineLen;
        ++lineNo;
        if (contains(data + start, lineLen, needle, needleLen))
            {
            unsigned k = 0;
            while (k < lineLen && k + 1 < sizeof(text))
                { text[k] = data[start + k]; ++k; }
            text[k] = 0;
            n = 0;
            n = append_str(msg, n, "FG: ");
            n = append_str(msg, n, name);
            n = append_str(msg, n, ":");
            n = append_uint(msg, n, lineNo);
            n = append_str(msg, n, ": ");
            n = append_str(msg, n, text);
            marker(msg);
            ++matches;
            }
        if (i < len && data[i] == '\n') ++i;   /* skip line terminator */
        }

    n = 0;
    n = append_str(msg, n, "FG: ");
    n = append_uint(msg, n, matches);
    n = append_str(msg, n, " matches");
    marker(msg);
    return (int)matches;
}

/* ------------------------------ commands --------------------------------- */

/* Embedded string table for the FAT-free self-test: searching "alpha" must
 * find exactly two lines (BUILTIN lines 1 and 4). */
static const char kBuiltin[] =
    "alpha bravo charlie\r\n"
    "delta echo foxtrot\r\n"
    "golf hotel india\r\n"
    "alpha again here\r\n";

static void do_selftest(void)
{
    int n = search_text("BUILTIN", kBuiltin, sizeof(kBuiltin) - 1, "alpha");
    marker(n == 2 ? "FG: selftest OK" : "FG: selftest FAIL");
}

/* find <file> <substring>.  Returns 1 when the app should stop (usage/error). */
static int do_find(const char* arg)
{
    char name[64], needle[64];
    copy_token(name, sizeof(name), arg);
    copy_token(needle, sizeof(needle), after_token(arg));
    if (!*name || !*needle)
        {
        marker("FG: usage: find <file> <substring>");
        marker("FG: DONE");
        return 1;
        }
    int count = slurp(name, g_data, CAP);
    if (count < 0)
        {
        marker("FG: find: open failed");
        marker("FG: usage: find <file> <substring>");
        marker("FG: DONE");
        return 1;
        }
    g_data[count] = 0;
    (void)search_text(name, g_data, (unsigned)count, needle);
    return 0;
}

static void do_help(void)
{
    print("FG: commands: find <file> <substring>  selftest  help  quit");
}

/* Parse "command [arguments]" from a console line.  1 means quit. */
static int run_command(const char* input)
{
    input = skip_spaces(input);
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = skip_spaces(word);

    if      (word_is(input, length, "find"))     return do_find(arg);
    else if (word_is(input, length, "selftest") ||
             word_is(input, length, "test"))   do_selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))      do_help();
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))      return 1;
    else marker("FG: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("FG: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&fgrep_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("FG: FILE SEARCH START");
    marker(s_bound ? "FG: EUSER ord 915 bound"
                   : "FG: using DebugPrint marker path");

    /* An embedded-table search on startup guarantees the FG markers even with
     * no serial input (and proves the matching engine before touching FAT). */
    do_selftest();

    for (;;)
        {
        print("FG> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;               /* timeout / input error -> done */
        if (run_command(line)) break;   /* quit / usage error */
        }

    marker("FG: DONE");
    con_release();
    return 0;
}
