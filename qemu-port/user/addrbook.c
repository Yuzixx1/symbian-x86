/*
 * addrbook.c -- a small ADDRESS BOOK end-user app for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...) (euseru.def).  ld defines the
 * IAT slot at codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits
 * the PE2 import section and the loader overwrites the slot with the resolved
 * export address before the process resumes.  Markers go through that ordinal,
 * with a DebugPrint fallback so the trace is always visible on the console.
 *
 * The book is an in-memory array of (name, phone) pairs.  "save" serialises it
 * to ADDR.TXT on the FAT disk and "load" reads it back, both through PortFile
 * IPC (runtime.c file_open/file_read/file_save), i.e. the portfilesrv service,
 * exactly like TYPE / TEXTEDIT / FGREP / FCOPY.  file_read is capped at 256
 * bytes per ChannelRequest, so load slurps in 256-byte chunks.  Console
 * commands (add / list / find / save / load / selftest / quit) arrive through
 * PortCon read_line().
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*addrbook_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&addrbook_printf_slot;
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

/* Case-insensitive equality of two NUL-terminated names. */
static int name_eq(const char* a, const char* b)
{
    while (*a && *b)
        {
        char x = *a++, y = *b++;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
        }
    return (*a == 0 && *b == 0) ? 1 : 0;
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

/* Bounded copy of a length-delimited token out of the load staging buffer. */
static void copy_n(char* out, unsigned cap, const char* text, unsigned len)
{
    unsigned i = 0;
    while (i < len && i + 1 < cap) { out[i] = text[i]; ++i; }
    out[i] = 0;
}

/* ------------------------------ the address book -------------------------- */

#define MAX_ENTRIES 48
#define NAME_CAP   32
#define PHONE_CAP  32
#define DISK_CAP   4096

struct Entry {
    char name[NAME_CAP];
    char phone[PHONE_CAP];
};

static struct Entry g_entries[MAX_ENTRIES];
static unsigned g_count = 0;
static char g_disk[DISK_CAP + 1];   /* save/load staging buffer */
static char line[128];

static const char kPath[] = "ADDR.TXT";

static void clear_book(void) { g_count = 0; }

/* Index of a name (case-insensitive), or -1 when absent. */
static int find_index(const char* name)
{
    unsigned i;
    for (i = 0; i < g_count; ++i)
        if (name_eq(g_entries[i].name, name)) return (int)i;
    return -1;
}

static void marker_added(const char* name)
{
    char msg[80];
    unsigned n = append_str(msg, 0, "AB: added ");
    n = append_str(msg, n, name);
    marker(msg);
}

static void print_entry(unsigned i)
{
    char msg[NAME_CAP + PHONE_CAP + 8];
    unsigned n = append_str(msg, 0, "AB: ");
    n = append_str(msg, n, g_entries[i].name);
    n = append_str(msg, n, " ");
    n = append_str(msg, n, g_entries[i].phone);
    marker(msg);
}

static void marker_count(const char* tag, unsigned value)
{
    char msg[64];
    unsigned n = append_str(msg, 0, tag);
    n = append_uint(msg, n, value);
    n = append_str(msg, n, " entries");
    marker(msg);
}

/* Add a new (name, phone) pair, or update the phone of an existing name. */
static void add_entry(const char* name, const char* phone)
{
    if (!*name || !*phone)
        { marker("AB: usage: add <name> <phone>"); return; }
    int at = find_index(name);
    if (at < 0)
        {
        if (g_count >= MAX_ENTRIES)
            { marker("AB: book full"); return; }
        at = (int)g_count++;
        copy_n(g_entries[at].name, NAME_CAP, name, cstrlen(name));
        }
    copy_n(g_entries[at].phone, PHONE_CAP, phone, cstrlen(phone));
    marker_added(g_entries[at].name);
}

static void list_entries(void)
{
    if (!g_count) { marker("AB: (empty)"); return; }
    unsigned i;
    for (i = 0; i < g_count; ++i) print_entry(i);
}

static void find_entry(const char* name)
{
    if (!*name) { marker("AB: usage: find <name>"); return; }
    int at = find_index(name);
    if (at < 0) { marker("AB: not found"); return; }
    print_entry((unsigned)at);
}

/* Serialise the book as "<name> <phone>\r\n" lines into ADDR.TXT on FAT. */
static void save_book(void)
{
    unsigned n = 0;
    unsigned i;
    for (i = 0; i < g_count; ++i)
        {
        n = append_str(g_disk, n, g_entries[i].name);
        n = append_str(g_disk, n, " ");
        n = append_str(g_disk, n, g_entries[i].phone);
        n = append_str(g_disk, n, "\r\n");
        }
    if (file_save(kPath, g_disk, n) < 0)
        { marker("AB: save failed"); return; }
    marker_count("AB: saved ", g_count);
}

/* Slurp ADDR.TXT in 256-byte reads and rebuild the in-memory book. */
static void load_book(void)
{
    int handle = file_open(kPath);
    if (handle < 0) { marker("AB: load failed"); return; }
    unsigned total = 0;
    for (;;)
        {
        unsigned room = DISK_CAP - total;
        if (!room) break;
        unsigned want = room < 256 ? room : 256;
        int r = file_read(handle, g_disk + total, want);
        if (r <= 0) break;              /* 0 = EOF, <0 = read error */
        total += (unsigned)r;
        }
    file_close(handle);
    g_disk[total] = 0;

    clear_book();
    unsigned i = 0;
    while (i < total)
        {
        unsigned start = i;
        while (i < total && g_disk[i] != '\n') ++i;
        unsigned len = i - start;
        if (len && g_disk[start + len - 1] == '\r') --len;  /* strip CR */
        if (len && g_count < MAX_ENTRIES)
            {
            unsigned s = 0;
            while (s < len && g_disk[start + s] != ' ') ++s;
            unsigned nameLen = s;
            if (s < len) ++s;                  /* skip the separating space */
            unsigned phoneLen = len - s;
            if (nameLen && phoneLen)
                {
                unsigned at = g_count++;
                copy_n(g_entries[at].name, NAME_CAP, g_disk + start, nameLen);
                copy_n(g_entries[at].phone, PHONE_CAP, g_disk + start + s,
                       phoneLen);
                }
            }
        if (i < total) ++i;                    /* skip the '\n' */
        }
    marker_count("AB: loaded ", g_count);
}

/* ------------------------------ commands --------------------------------- */

static void do_add(const char* arg)
{
    char name[NAME_CAP], phone[PHONE_CAP];
    copy_token(name, sizeof(name), arg);
    const char* second = arg;
    while (*second && *second != ' ') ++second;
    copy_token(phone, sizeof(phone), second);
    add_entry(name, phone);
}

static void do_selftest(void)
{
    clear_book();
    add_entry("Alice", "5551000");
    add_entry("Bob", "5552000");
    save_book();                       /* AB: saved 2 entries */
    clear_book();                      /* prove the data survives only on FAT */
    load_book();                       /* AB: loaded 2 entries */
    list_entries();                    /* AB: Alice ... / AB: Bob ... */

    int ok = (g_count == 2);
    if (ok)
        {
        int a = find_index("Alice"), b = find_index("Bob");
        ok = (a >= 0 && b >= 0) &&
             name_eq(g_entries[a].phone, "5551000") &&
             name_eq(g_entries[b].phone, "5552000");
        }
    marker(ok ? "AB: selftest OK" : "AB: selftest FAIL");
}

static void do_help(void)
{
    print("AB: commands: add <name> <phone>  list  find <name>  save  load"
          "  selftest  help  quit");
}

/* Parse "command [arguments]" from a console line.  1 means quit. */
static int run_command(const char* input)
{
    input = skip_spaces(input);
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = skip_spaces(word);

    if      (word_is(input, length, "add"))      do_add(arg);
    else if (word_is(input, length, "list"))     list_entries();
    else if (word_is(input, length, "find"))     { char n[NAME_CAP];
                                                   copy_token(n, sizeof(n), arg);
                                                   find_entry(n); }
    else if (word_is(input, length, "save"))     save_book();
    else if (word_is(input, length, "load"))     load_book();
    else if (word_is(input, length, "selftest") ||
             word_is(input, length, "test"))     do_selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))        do_help();
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))        return 1;
    else marker("AB: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("AB: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&addrbook_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("AB: ADDRESS BOOK");
    marker(s_bound ? "AB: EUSER ord 915 bound"
                   : "AB: using DebugPrint marker path");

    for (;;)
        {
        print("AB> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;               /* timeout / input error -> done */
        if (run_command(line)) break;   /* quit */
        }

    marker("AB: DONE");
    con_release();
    return 0;
}
