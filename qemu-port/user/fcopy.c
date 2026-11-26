/*
 * fcopy.c -- a small FILE COPY / FILE UTILITY for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...)
 * (kernelhwsrv/kernel/eka/bx86gcc/euseru.def).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  Markers go through that ordinal, with
 * a DebugPrint fallback so the trace is always visible on the serial console.
 *
 * File I/O uses PortFile IPC (runtime.c file_open/file_read/file_save), i.e.
 * the portfilesrv service, exactly like TYPE, TEXTEDIT, FILEVIEW and FILEMGR.
 * file_read is capped at 256 bytes per ChannelRequest, so copies/dumps loop in
 * 256-byte chunks.  There is no delete request on the PortFile logical device,
 * so "del" reports FC: del unsupported rather than risking an unknown exec.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*fcopy_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&fcopy_printf_slot;
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

/* Copy a single space-delimited token into a NUL-terminated buffer. */
static void copy_token(char* out, unsigned cap, const char* text)
{
    unsigned n = 0;
    text = skip_spaces(text);
    while (*text && *text != ' ' && n + 1 < cap) out[n++] = *text++;
    out[n] = 0;
}

/* ------------------------------ file utilities --------------------------- */

#define CAP 8192
static char g_data[CAP + 1];   /* copy/cat staging buffer */
static char scratch[256];
static char line[128];

/* Read an entire (small) file into dst, up to cap bytes; return byte count or
 * -1 on open failure.  Used for copies, dumps and size probes. */
static int slurp(const char* path, char* dst, unsigned cap, int dump)
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
        if (n <= 0) break;          /* 0 = EOF, <0 = read error */
        if (dump) print_n(dst + total, (unsigned)n);
        total += (unsigned)n;
        }
    file_close(handle);
    return (int)total;
}

static void do_copy(const char* src_arg, const char* dst_arg)
{
    char src[64], dst[64];
    copy_token(src, sizeof(src), src_arg);
    copy_token(dst, sizeof(dst), dst_arg);
    if (!*src || !*dst) { marker("FC: copy: need src and dst"); return; }
    int count = slurp(src, g_data, CAP, 0);
    if (count < 0) { marker("FC: copy: open src failed"); return; }
    if (file_save(dst, g_data, (unsigned)count) < 0)
        { marker("FC: copy: save dst failed"); return; }
    char msg[256];
    unsigned n = 0;
    n = append_str(msg, n, "FC: copied ");
    n = append_str(msg, n, src);
    n = append_str(msg, n, " -> ");
    n = append_str(msg, n, dst);
    n = append_str(msg, n, " (");
    n = append_uint(msg, n, (unsigned)count);
    n = append_str(msg, n, " bytes)");
    marker(msg);
}

/* Dump a file's contents through EUSER ordinal 915 (with DebugPrint fallback),
 * then report its name and byte count. */
static void do_cat(const char* name_arg)
{
    char name[64];
    copy_token(name, sizeof(name), name_arg);
    if (!*name) { marker("FC: cat: no file"); return; }
    int count = slurp(name, g_data, CAP, 0);
    if (count < 0) { marker("FC: cat: open failed"); return; }
    g_data[count] = 0;
    if (count) marker(g_data);      /* file contents via EUSER ord 915 */
    char msg[192];
    unsigned n = append_str(msg, 0, "FC: cat ");
    n = append_str(msg, n, name);
    n = append_str(msg, n, " (");
    n = append_uint(msg, n, (unsigned)count);
    n = append_str(msg, n, " bytes)");
    marker(msg);
}

/* The PortFile ChannelRequest device exposes open/read/create/append/commit/
 * chdir/getcwd but no delete request (runtime.c).  Report that explicitly. */
static void do_del(const char* name)
{
    (void)name;
    marker("FC: del unsupported");
}

/* Known small FAT files.  Missing files (and files larger than the 64 KiB
 * PortFile snapshot, e.g. EUSER.DLL) are skipped. */
static const char* const kFiles[] = {
    "AUTOEXEC.BAT", "CONFIG.SYS",
    "README.TXT", "NOTES.TXT", "HELLO.TXT", "FCOPY.TXT", "OUT.TXT",
    "FCOPY.E32", "FILEMGR.E32", "TEXTEDIT.E32"
};
#define NFILES ((int)(sizeof(kFiles) / sizeof(kFiles[0])))

static void do_dir(void)
{
    char msg[160];
    int listed = 0;
    int i;
    for (i = 0; i < NFILES; ++i)
        {
        int size = slurp(kFiles[i], scratch, sizeof(scratch), 0);
        if (size < 0) continue;
        unsigned n = append_str(msg, 0, "FC: ");
        n = append_str(msg, n, kFiles[i]);
        n = append_str(msg, n, " ");
        n = append_uint(msg, n, (unsigned)size);
        marker(msg);
        ++listed;
        }
    unsigned n = append_str(msg, 0, "FC: listed ");
    n = append_uint(msg, n, (unsigned)listed);
    n = append_str(msg, n, " files");
    marker(msg);
}

static void do_help(void)
{
    print("FC: commands: copy <src> <dst>  cat <file>  del <file>  dir"
          "  help  quit  selftest");
}

/* Built-in self-test: save an embedded payload to FCOPY.TXT, read it back and
 * compare byte-for-byte.  Runs at startup so the marker is unconditional. */
static const char kPayload[] = "FC selftest payload 12345\r\n";

static void do_selftest(void)
{
    unsigned len = sizeof(kPayload) - 1;
    if (file_save("FCOPY.TXT", kPayload, len) < 0)
        { marker("FC: selftest FAIL"); return; }
    char check[64];
    int total = slurp("FCOPY.TXT", check, len, 0);
    int ok = (total == (int)len);
    for (unsigned i = 0; ok && i < len; ++i)
        if (check[i] != kPayload[i]) ok = 0;
    marker(ok ? "FC: selftest OK" : "FC: selftest FAIL");
}

/* ------------------------------ command loop ----------------------------- */

static int run_command(const char* input)
{
    input = skip_spaces(input);
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = skip_spaces(word);

    if (word_is(input, length, "copy"))
        {
        const char* second = arg;
        while (*second && *second != ' ') ++second;
        do_copy(arg, second);
        }
    else if (word_is(input, length, "cat"))       do_cat(arg);
    else if (word_is(input, length, "del"))       do_del(arg);
    else if (word_is(input, length, "dir"))       do_dir();
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        do_help();
    else if (word_is(input, length, "selftest") || word_is(input, length, "test"))
        do_selftest();
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("FC: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("FC: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&fcopy_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("FC: FILE UTILITY START");
    marker(s_bound ? "FC: EUSER ord 915 bound"
                   : "FC: using DebugPrint marker path");

    do_selftest();

    for (;;)
        {
        print("FC> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;               /* timeout / input error -> done */
        if (run_command(line)) break;   /* quit */
        }

    marker("FC: DONE");
    con_release();
    return 0;
}
