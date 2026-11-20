/*
 * notepad.c -- an ADVANCED NOTEPAD for the Sybian REAL port.
 *
 * A multi-file, line-oriented editor with literal search and replace:
 *   new             clear the in-memory buffer
 *   open <file>     load a FAT text file (PortFile IPC) into the buffer
 *   insert <text>   append a line of text to the buffer
 *   show            dump the buffer to the serial console
 *   find <substr>   count literal substring matches
 *   replace <old> <new>   replace every literal occurrence
 *   save <file>     write the buffer back to a FAT file (PortFile IPC)
 *   quit            finish
 *   selftest        built-in end-to-end proof (insert/find/replace/save/open)
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...)
 * (kernelhwsrv/kernel/eka/bx86gcc/euseru.def).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  Serial markers go through that ordinal, with a
 * DebugPrint fallback so the trace is always visible on the console.
 *
 * File I/O uses PortFile IPC (runtime.c file_open/file_read/file_save), i.e.
 * the portfilesrv service, exactly like TYPE / TEXTEDIT / FGREP / FCOPY.
 * file_read is capped at 256 bytes per ChannelRequest, so files are slurped in
 * chunks.  Console commands arrive via PortCon read_line.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*notepad_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&notepad_printf_slot;
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

/* ------------------------------ editor state ----------------------------- */

#define CAP 8192
static char g_buffer[CAP + 1];   /* the in-memory document */
static unsigned g_len = 0;
static char g_tmp[CAP + 1];      /* replacement scratch buffer */
static char line[160];

static void buffer_clear(void)
{
    g_len = 0;
    g_buffer[0] = 0;
}

static void marker_num(const char* prefix, unsigned value, const char* suffix)
{
    char msg[192];
    unsigned n = 0;
    n = append_str(msg, n, prefix);
    n = append_uint(msg, n, value);
    n = append_str(msg, n, suffix);
    marker(msg);
}

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

static void do_new(void)
{
    buffer_clear();
    print("NP: new buffer");
}

static void do_open(const char* name_arg)
{
    char name[64];
    copy_token(name, sizeof(name), name_arg);
    if (!*name) { marker("NP: usage: open <file>"); return; }
    int count = slurp(name, g_buffer, CAP);
    if (count < 0)
        {
        marker("NP: open failed");
        return;
        }
    g_len = (unsigned)count;
    g_buffer[g_len] = 0;
    marker_num("NP: loaded ", g_len, " bytes");
}

static void do_insert(const char* text)
{
    unsigned n = cstrlen(text);
    /* Each insert is one line: append the text then a CRLF terminator. */
    if (g_len + n + 2 >= CAP) { marker("NP: buffer full"); return; }
    unsigned i;
    for (i = 0; i < n; ++i) g_buffer[g_len + i] = text[i];
    g_buffer[g_len + n]     = '\r';
    g_buffer[g_len + n + 1] = '\n';
    g_len += n + 2;
    g_buffer[g_len] = 0;
    marker("NP: inserted");
}

static void do_show(void)
{
    if (!g_len) { print("NP: (empty)"); return; }
    print("NP: --- buffer ---");
    print_n(g_buffer, g_len);
    print("NP: --- end ---");
}

/* Count non-overlapping case-sensitive literal occurrences. */
static int count_occurrences(const char* text, unsigned textLen,
                             const char* needle, unsigned needleLen)
{
    if (!needleLen || needleLen > textLen) return 0;
    int matches = 0;
    unsigned i = 0;
    while (i + needleLen <= textLen)
        {
        unsigned j = 0;
        while (j < needleLen && text[i + j] == needle[j]) ++j;
        if (j == needleLen) { ++matches; i += needleLen; }
        else ++i;
        }
    return matches;
}

static void do_find(const char* needle)
{
    if (!*needle) { marker("NP: usage: find <substr>"); return; }
    int n = count_occurrences(g_buffer, g_len, needle, cstrlen(needle));
    marker_num("NP: found ", (unsigned)n, " matches");
}

/* Replace every literal occurrence of old with new.  Returns the number of
 * replacements.  The document is rebuilt into g_tmp, then copied back. */
static int replace_all(const char* old, const char* newText)
{
    unsigned oldLen = cstrlen(old), newLen = cstrlen(newText);
    if (!oldLen || oldLen > g_len) return 0;
    unsigned src = 0, dst = 0, count = 0;
    while (src < g_len)
        {
        int match = 0;
        if (src + oldLen <= g_len)
            {
            unsigned j = 0;
            while (j < oldLen && g_buffer[src + j] == old[j]) ++j;
            match = (j == oldLen);
            }
        if (match)
            {
            if (dst + newLen >= CAP) break;   /* out of room: stop */
            unsigned k;
            for (k = 0; k < newLen; ++k) g_tmp[dst++] = newText[k];
            src += oldLen;
            ++count;
            }
        else
            {
            if (dst + 1 >= CAP) break;
            g_tmp[dst++] = g_buffer[src++];
            }
        }
    g_tmp[dst] = 0;
    for (unsigned i = 0; i <= dst; ++i) g_buffer[i] = g_tmp[i];
    g_len = dst;
    return (int)count;
}

static void do_replace(const char* args)
{
    char old[64], newText[64];
    copy_token(old, sizeof(old), args);
    copy_token(newText, sizeof(newText), after_token(args));
    if (!*old || !*newText)
        { marker("NP: usage: replace <old> <new>"); return; }
    int n = replace_all(old, newText);
    marker_num("NP: replaced ", (unsigned)n, "");
}

static void do_save(const char* name_arg)
{
    char name[64];
    copy_token(name, sizeof(name), name_arg);
    if (!*name) { marker("NP: usage: save <file>"); return; }
    if (file_save(name, g_buffer, g_len) < 0)
        { marker("NP: save failed"); return; }
    char msg[128];
    unsigned n = 0;
    n = append_str(msg, n, "NP: saved ");
    n = append_str(msg, n, name);
    marker(msg);
}

/* ------------------------------ built-in self-test ----------------------- */

static const char kLine[] = "NP selftest alpha line";

static void do_selftest(void)
{
    buffer_clear();
    do_insert(kLine);                       /* NP: inserted */
    do_find("alpha");                       /* NP: found 1 matches */

    int f = count_occurrences(g_buffer, g_len, "alpha", 5);
    if (f != 1) { marker("NP: selftest FAIL (find)"); return; }

    do_replace("alpha BETA");               /* NP: replaced 1 */
    int r = count_occurrences(g_buffer, g_len, "BETA", 4);
    if (r != 1) { marker("NP: selftest FAIL (replace)"); return; }

    do_save("NOTE2.TXT");                   /* NP: saved NOTE2.TXT */
    do_open("NOTE2.TXT");                   /* NP: loaded <N> bytes */

    /* Round-trip verification against the reloaded document. */
    int beta = count_occurrences(g_buffer, g_len, "BETA", 4);
    int alpha = count_occurrences(g_buffer, g_len, "alpha", 5);
    marker(beta == 1 && alpha == 0
           ? "NP: selftest OK" : "NP: selftest FAIL (reload)");
}

static void do_help(void)
{
    print("NP: commands: new | open <file> | insert <text> | show |"
          " find <substr> | replace <old> <new> | save <file> |"
          " selftest | quit");
}

/* ------------------------------ command loop ----------------------------- */

/* Parse "command [arguments]" from a console line.  1 means quit. */
static int run_command(const char* input)
{
    input = skip_spaces(input);
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = skip_spaces(word);

    if      (word_is(input, length, "new"))        do_new();
    else if (word_is(input, length, "open"))       do_open(arg);
    else if (word_is(input, length, "insert"))     do_insert(arg);
    else if (word_is(input, length, "show") ||
             word_is(input, length, "print"))      do_show();
    else if (word_is(input, length, "find"))       do_find(arg);
    else if (word_is(input, length, "replace"))    do_replace(arg);
    else if (word_is(input, length, "save"))       do_save(arg);
    else if (word_is(input, length, "selftest") ||
             word_is(input, length, "test"))       do_selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))          do_help();
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))          return 1;
    else marker("NP: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("NP: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&notepad_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("NP: NOTEPAD READY");
    marker(s_bound ? "NP: EUSER ord 915 bound"
                   : "NP: using DebugPrint marker path");

    for (;;)
        {
        print("NP> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;                    /* timeout / input error */
        if (run_command(line)) break;        /* quit */
        }

    marker("NP: DONE");
    con_release();
    return 0;
}
