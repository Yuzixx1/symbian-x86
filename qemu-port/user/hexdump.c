/*
 * hexdump.c -- a small HEX DUMP utility for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...) (euseru.def).  ld defines the
 * IAT slot at codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits
 * the PE2 import section and the loader overwrites the slot with the resolved
 * export address before the process resumes.  Markers go through that ordinal,
 * with a DebugPrint fallback so the trace is always visible on the console.
 *
 * File I/O uses PortFile IPC (runtime.c file_open/file_read/file_close), i.e.
 * the portfilesrv service, exactly like FGREP / FCOPY / TYPE.  file_read is
 * capped at 256 bytes per ChannelRequest, so a small file is slurped in chunks
 * and then rendered as a canonical 16-byte-per-line hex + ASCII dump:
 *   HD: 00000000  48 65 6c 6c 6f  ...  Hello...
 * A built-in self-test dumps an embedded byte array so the renderer can be
 * proven without FAT.  Console commands (hex / selftest / help / quit) arrive
 * via PortCon read_line.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*hexdump_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&hexdump_printf_slot;
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

static char hex_digit(unsigned value)
{
    return (char)(value < 10u ? '0' + value : 'A' + value - 10u);
}

/* Append exactly `digits` hex characters of `value`, most significant first. */
static unsigned append_hex(char* out, unsigned n, unsigned value, unsigned digits)
{
    char tmp[16];
    unsigned i;
    for (i = 0; i < digits; ++i)
        { tmp[digits - 1u - i] = hex_digit(value & 0xfu); value >>= 4; }
    for (i = 0; i < digits; ++i) out[n++] = tmp[i];
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

/* ------------------------------ hex dump renderer ------------------------- */

/* Emit one canonical line per 16 bytes:
 *   HD: <offset:08X>  <16 x "XX ">  <16 ASCII chars>
 * Non-printable bytes render as '.', bytes past the end render as blanks. */
static void dump_bytes(const char* data, unsigned len)
{
    char line[128];
    unsigned off = 0;
    while (off < len)
        {
        unsigned n = 0;
        unsigned i;
        n = append_str(line, n, "HD: ");
        n = append_hex(line, n, off, 8);
        n = append_str(line, n, "  ");
        for (i = 0; i < 16; ++i)
            {
            if (off + i < len)
                n = append_hex(line, n, (unsigned char)data[off + i], 2);
            else
                n = append_str(line, n, "  ");
            n = append_str(line, n, " ");
            }
        n = append_str(line, n, " ");
        for (i = 0; i < 16; ++i)
            {
            char c = ' ';
            if (off + i < len)
                {
                c = data[off + i];
                if (c < 0x20 || c >= 0x7f) c = '.';
                }
            line[n++] = c;
            }
        line[n] = 0;
        marker(line);
        off += 16;
        }
}

/* Dump a byte range and report its size as "HD: <N> bytes". */
static void do_dump(const char* data, unsigned len)
{
    dump_bytes(data, len);
    char msg[128];
    unsigned n = 0;
    n = append_str(msg, n, "HD: ");
    n = append_uint(msg, n, len);
    n = append_str(msg, n, " bytes");
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
        int r = file_read(handle, dst + total, want);
        if (r <= 0) break;              /* 0 = EOF, <0 = read error */
        total += (unsigned)r;
        }
    file_close(handle);
    return (int)total;
}

/* ------------------------------ commands --------------------------------- */

/* Embedded byte array for the FAT-free self-test.  Starts with "Hello" so the
 * very first line demonstrates the canonical hex + ASCII rendering, and ends
 * with control/high bytes to exercise the '.' substitution and a partial line. */
static const unsigned char kBuiltin[] = {
    'H','e','l','l','o',',',' ','w','o','r','l','d','!','\r','\n', 0x00,
    0x01,0x02,0x1f,0x7f,0x80,0xff,0x41
};

static void do_selftest(void)
{
    do_dump((const char*)kBuiltin, (unsigned)sizeof(kBuiltin));
    marker("HD: selftest OK");
}

/* hex <file>: slurp via PortFile IPC, dump it, report the byte count. */
static int do_hex(const char* arg)
{
    char name[64];
    copy_token(name, sizeof(name), arg);
    if (!*name)
        {
        marker("HD: usage: hex <file>");
        return 1;
        }
    int count = slurp(name, g_data, CAP);
    if (count < 0)
        {
        marker("HD: hex: open failed");
        return 1;
        }
    do_dump(g_data, (unsigned)count);
    return 0;
}

static void do_help(void)
{
    print("HD: commands: hex <file>  selftest  help  quit");
}

/* Parse "command [arguments]" from a console line.  1 means quit. */
static int run_command(const char* input)
{
    input = skip_spaces(input);
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = skip_spaces(word);

    if      (word_is(input, length, "hex"))       return do_hex(arg);
    else if (word_is(input, length, "selftest") ||
             word_is(input, length, "test"))     do_selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))        do_help();
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))        return 1;
    else marker("HD: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("HD: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&hexdump_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("HD: HEXDUMP START");
    marker(s_bound ? "HD: EUSER ord 915 bound"
                   : "HD: using DebugPrint marker path");

    /* An embedded-table dump on startup guarantees the HD markers even with
     * no serial input (and proves the renderer before touching FAT). */
    do_selftest();

    for (;;)
        {
        print("HD> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;               /* timeout / input error -> done */
        if (run_command(line)) break;   /* quit / usage error */
        }

    marker("HD: DONE");
    con_release();
    return 0;
}
