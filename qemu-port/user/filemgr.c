/*
 * filemgr.c -- a small text-mode FILE MANAGER for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  Markers go through that ordinal, with a DebugPrint
 * fallback so the trace is always visible on the serial console.
 *
 * There is no directory-enumeration executive on the PortFile logical device,
 * so "dir" opens a known set of small files on the FAT disk (via the same
 * file_open/file_read PortFile IPC used by TYPE and FILEVIEW) and reports each
 * file's name and byte size.  Files too large for the 64 KiB PortFile snapshot
 * (e.g. EUSER.DLL) or absent entries are simply skipped.  Console commands
 * (dir / cat / help / quit) are read from PortCon through read_line().
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*euser_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&euser_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
    print(text);                              /* DebugPrint fallback */
}

/* Known candidate files.  Small files that exist are listed; missing files or
 * files beyond the PortFile 64 KiB snapshot are skipped by scan_file. */
static const char* const kFiles[] = {
    "AUTOEXEC.BAT", "CONFIG.SYS",
    "README.TXT", "NOTES.TXT", "HELLO.TXT",
    "FILEMGR.E32", "TEXTEDIT.E32",
    "EUSER.DLL", "PORTFILESRV.E32"
};
#define NFILES ((int)(sizeof(kFiles) / sizeof(kFiles[0])))

static char scratch[256];
static char line[128];

/* Open a file and read it in 256-byte chunks (file_read's hard cap), summing
 * the byte count.  When dump != 0 the bytes are echoed to the console (cat).
 * Returns the total size, or -1 when the file cannot be opened/read. */
static int scan_file(const char* path, int dump)
{
    int handle = file_open(path);
    if (handle < 0) return -1;
    int total = 0;
    for (;;)
        {
        int n = file_read(handle, scratch, sizeof(scratch));
        if (n <= 0) break;              /* 0 = EOF, <0 = read error */
        if (dump) print_n(scratch, (unsigned)n);
        total += n;
        }
    file_close(handle);
    return total;
}

/* Build "FM: <name> <size>" into out (plain string, no % specifiers). */
static void format_entry(char* out, const char* name, unsigned size)
{
    unsigned n = 0;
    const char* p = "FM: ";
    while (*p) out[n++] = *p++;
    p = name;
    while (*p) out[n++] = *p++;
    out[n++] = ' ';
    char digits[12];
    int d = 0;
    if (!size) digits[d++] = '0';
    while (size) { digits[d++] = (char)('0' + size % 10); size /= 10; }
    while (d) out[n++] = digits[--d];
    out[n] = 0;
}

static void list_files(void)
{
    char text[160];
    int count = 0;
    int i;
    for (i = 0; i < NFILES; ++i)
        {
        int size = scan_file(kFiles[i], 0);
        if (size < 0) continue;         /* missing or too large: skip */
        format_entry(text, kFiles[i], (unsigned)size);
        marker(text);
        ++count;
        }
    /* Dedicated "listed N files" marker. */
    unsigned n = 0;
    const char* p = "FM: listed ";
    while (*p) text[n++] = *p++;
    char digits[12];
    int d = 0;
    if (!count) digits[d++] = '0';
    while (count) { digits[d++] = (char)('0' + count % 10); count /= 10; }
    while (d) text[n++] = digits[--d];
    p = " files";
    while (*p) text[n++] = *p++;
    text[n] = 0;
    marker(text);
}

static void show_file(const char* name)
{
    while (*name == ' ') ++name;
    if (!*name) { marker("FM: cat: no file"); return; }
    int size = scan_file(name, 1);
    if (size < 0) { marker("FM: cat: open failed"); return; }
    char text[160];
    format_entry(text, name, (unsigned)size);
    marker(text);
}

static void help(void)
{
    print("FM: commands: dir  cat <file>  help  quit");
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

/* Parse "command [argument]" from a line read from the console.
 * Returns 1 when the user asked to quit, 0 otherwise. */
static int run_command(const char* input)
{
    while (*input == ' ') ++input;
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = word;
    while (*arg == ' ') ++arg;
    if      (word_is(input, length, "dir"))  list_files();
    else if (word_is(input, length, "cat"))  show_file(arg);
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        help();
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("FM: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("FM: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&euser_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("FM: FILE MANAGER START");
    marker(s_bound ? "FM: EUSER ord 915 bound"
                   : "FM: using DebugPrint marker path");

    /* A listing on startup guarantees the FM markers even with no input. */
    list_files();

    for (;;)
        {
        con_puts_raw("FM> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;               /* timeout / input error -> done */
        if (run_command(line)) break;   /* quit */
        }

    marker("FM: DONE");
    con_release();
    return 0;
}
