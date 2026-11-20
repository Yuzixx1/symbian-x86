/*
 * pwvault.c -- a simple PASSWORD MANAGER / CREDENTIAL VAULT for the port.
 *
 * Commands (PortCon):
 *   unlock <code>              unlock with the master code (1234)
 *   lock                       lock the vault again
 *   add <service> <user> <pass>  store a credential (requires unlock)
 *   list                       dump all credentials (requires unlock)
 *   get <service>              look up one credential (requires unlock)
 *   save                       persist VAULT.DAT (requires unlock)
 *   load                       reload VAULT.DAT (requires unlock)
 *   selftest / test            end-to-end proof (unlock/add/list/save/load)
 *   help / ? , quit / q
 *
 * Persistence is an XOR-obfuscated, pipe-delimited record file (VAULT.DAT)
 * written/read over PortFile IPC (runtime.c file_save/file_read), exactly like
 * TYPE / TEXTEDIT / FCOPY / NOTEPAD.  Every byte is XORed with a fixed 4-byte
 * key so plaintext passwords never appear in the on-disk image.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  Markers go through that ordinal, with a DebugPrint
 * fallback so the trace is always visible on the serial console.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*pv_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&pv_printf_slot;
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

/* Parse an unsigned decimal code; returns 0 when there is no valid digit. */
static unsigned parse_code(const char* text)
{
    unsigned value = 0, digits = 0;
    text = skip_spaces(text);
    while (*text >= '0' && *text <= '9')
        {
        value = value * 10u + (unsigned)(*text - '0');
        ++digits; ++text;
        }
    return digits ? value : 0u;
}

/* ------------------------------ vault state ------------------------------ */

#define MASTER_CODE 1234u
#define MAX_ENTRIES 16
#define FIELD_CAP   32
#define CAP         8192

typedef struct
{
    char service[FIELD_CAP];
    char user[FIELD_CAP];
    char pass[FIELD_CAP];
} Entry;

static Entry g_entries[MAX_ENTRIES];
static unsigned g_count = 0;
static int g_unlocked = 0;
static char line[160];

static void marker_num(const char* prefix, unsigned value, const char* suffix)
{
    char msg[192];
    unsigned n = 0;
    n = append_str(msg, n, prefix);
    n = append_uint(msg, n, value);
    n = append_str(msg, n, suffix);
    marker(msg);
}

static void marker_entry(const char* prefix, const Entry* e)
{
    char msg[3 * FIELD_CAP + 32];
    unsigned n = 0;
    n = append_str(msg, n, prefix);
    n = append_str(msg, n, e->service);
    n = append_str(msg, n, " ");
    n = append_str(msg, n, e->user);
    n = append_str(msg, n, " ");
    n = append_str(msg, n, e->pass);
    marker(msg);
}

/* ------------------------------ XOR obfuscation -------------------------- */

/* Repeating fixed key; none of the bytes are zero, so obfuscation is visible
 * in every byte of the stored record file. */
static const unsigned char kXorKey[4] = { 0xA5u, 0x5Au, 0xC3u, 0x3Cu };

static void xor_bytes(char* buffer, unsigned length)
{
    unsigned i;
    for (i = 0; i < length; ++i)
        buffer[i] = (char)((unsigned char)buffer[i] ^ kXorKey[i & 3u]);
}

/* ------------------------------ serialization ---------------------------- */

/* Flatten the in-memory vault to "service|user|pass\n" records. */
static unsigned serialize(char* out, unsigned cap)
{
    unsigned n = 0, i;
    for (i = 0; i < g_count; ++i)
        {
        if (n + 3 * FIELD_CAP + 3 >= cap) break;
        n = append_str(out, n, g_entries[i].service);
        out[n++] = '|';
        n = append_str(out, n, g_entries[i].user);
        out[n++] = '|';
        n = append_str(out, n, g_entries[i].pass);
        out[n++] = '\n';
        }
    out[n] = 0;
    return n;
}

/* Parse the decoded record text back into the in-memory vault. */
static void deserialize(const char* text, unsigned length)
{
    unsigned i = 0;
    g_count = 0;
    while (i < length && g_count < MAX_ENTRIES)
        {
        unsigned start = i;
        while (i < length && text[i] != '\n' && text[i] != '\r') ++i;
        unsigned lineLen = i - start;
        while (i < length && (text[i] == '\n' || text[i] == '\r')) ++i;
        if (!lineLen) continue;

        char fields[3][FIELD_CAP] = { { 0 }, { 0 }, { 0 } };
        unsigned flen[3] = { 0, 0, 0 };
        unsigned p = start, field = 0;
        while (p < start + lineLen && field < 3)
            {
            if (text[p] == '|') { ++field; ++p; continue; }
            if (flen[field] < FIELD_CAP - 1)
                fields[field][flen[field]++] = text[p];
            ++p;
            }
        if (field != 2) continue;   /* malformed record: skip */
        unsigned k;
        for (k = 0; k < 3; ++k) fields[k][flen[k]] = 0;
        Entry* e = &g_entries[g_count++];
        for (k = 0; k < FIELD_CAP; ++k)
            {
            e->service[k] = fields[0][k];
            e->user[k]    = fields[1][k];
            e->pass[k]    = fields[2][k];
            }
        }
}

/* Slurp a small file's raw bytes in 256-byte chunks (file_read's hard cap). */
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
        if (n <= 0) break;          /* 0 = EOF, <0 = read error */
        total += (unsigned)n;
        }
    file_close(handle);
    return (int)total;
}

/* ------------------------------ commands --------------------------------- */

static void do_unlock(const char* arg)
{
    if (parse_code(arg) != MASTER_CODE)
        { marker("PV: bad code"); return; }
    g_unlocked = 1;
    marker("PV: unlocked");
}

static void do_lock(void)
{
    g_unlocked = 0;
    marker("PV: locked");
}

static void do_add(const char* args)
{
    if (!g_unlocked) { marker("PV: locked"); return; }
    if (g_count >= MAX_ENTRIES) { marker("PV: vault full"); return; }
    char service[FIELD_CAP], user[FIELD_CAP], pass[FIELD_CAP];
    copy_token(service, sizeof(service), args);
    copy_token(user, sizeof(user), after_token(args));
    copy_token(pass, sizeof(pass), after_token(after_token(args)));
    if (!*service || !*user || !*pass)
        { marker("PV: usage: add <service> <user> <pass>"); return; }
    Entry* e = &g_entries[g_count++];
    unsigned i;
    for (i = 0; i < FIELD_CAP; ++i)
        {
        e->service[i] = service[i];
        e->user[i]    = user[i];
        e->pass[i]    = pass[i];
        }
    char msg[FIELD_CAP + 32];
    unsigned n = append_str(msg, 0, "PV: added ");
    n = append_str(msg, n, service);
    (void)n;
    marker(msg);
}

static void do_list(void)
{
    if (!g_unlocked) { marker("PV: locked"); return; }
    if (!g_count) { marker("PV: (empty)"); return; }
    unsigned i;
    for (i = 0; i < g_count; ++i) marker_entry("PV: ", &g_entries[i]);
}

static void do_get(const char* arg)
{
    if (!g_unlocked) { marker("PV: locked"); return; }
    char service[FIELD_CAP];
    copy_token(service, sizeof(service), arg);
    if (!*service) { marker("PV: usage: get <service>"); return; }
    unsigned i;
    for (i = 0; i < g_count; ++i)
        {
        unsigned j = 0;
        while (service[j] && g_entries[i].service[j] == service[j]) ++j;
        if (!service[j] && !g_entries[i].service[j])
            { marker_entry("PV: found ", &g_entries[i]); return; }
        }
    char msg[FIELD_CAP + 32];
    unsigned n = append_str(msg, 0, "PV: not found ");
    n = append_str(msg, n, service);
    (void)n;
    marker(msg);
}

static void do_save(void)
{
    if (!g_unlocked) { marker("PV: locked"); return; }
    static char plain[CAP];
    unsigned length = serialize(plain, sizeof(plain));
    xor_bytes(plain, length);           /* obfuscate before touching FAT */
    if (file_save("VAULT.DAT", plain, length) < 0)
        { marker("PV: save failed"); return; }
    marker_num("PV: saved ", g_count, " entries");
}

static void do_load(void)
{
    if (!g_unlocked) { marker("PV: locked"); return; }
    static char raw[CAP + 1];
    int total = slurp("VAULT.DAT", raw, CAP);
    if (total < 0) { marker("PV: load failed"); return; }
    xor_bytes(raw, (unsigned)total);    /* de-obfuscate the stored records */
    raw[total] = 0;
    deserialize(raw, (unsigned)total);
    marker_num("PV: loaded ", g_count, " entries");
}

static void do_help(void)
{
    print("PV: commands: unlock <code> | lock | add <service> <user> <pass>"
          " | list | get <service> | save | load | selftest | help | quit");
}

/* ------------------------------ built-in self-test ----------------------- */

static int find_entry(const char* service, const char* user, const char* pass)
{
    unsigned i;
    for (i = 0; i < g_count; ++i)
        {
        const Entry* e = &g_entries[i];
        unsigned j = 0;
        while (service[j] && e->service[j] == service[j]) ++j;
        if (service[j] || e->service[j]) continue;
        j = 0;
        while (user[j] && e->user[j] == user[j]) ++j;
        if (user[j] || e->user[j]) continue;
        j = 0;
        while (pass[j] && e->pass[j] == pass[j]) ++j;
        if (!pass[j] && !e->pass[j]) return 1;
        }
    return 0;
}

static void do_selftest(void)
{
    g_count = 0;
    do_lock();                          /* PV: locked */
    do_unlock("1234");                  /* PV: unlocked */
    do_add("mail alice p@ss");          /* PV: added mail */
    do_add("bank bob s3cret");          /* PV: added bank */
    do_list();                          /* PV: mail alice p@ss / PV: bank ... */
    do_save();                          /* PV: saved 2 entries */

    /* Prove the bytes on FAT are obfuscated: no plaintext password appears. */
    static char raw[CAP + 1];
    int total = slurp("VAULT.DAT", raw, CAP);
    int hidden = 1;
    if (total >= 4)
        {
        raw[total] = 0;
        unsigned i;
        for (i = 0; i + 4u < (unsigned)total; ++i)
            {
            if (raw[i] == 'm' && raw[i + 1] == 'a' && raw[i + 2] == 'i' &&
                raw[i + 3] == 'l') { hidden = 0; break; }
            }
        }
    marker(hidden ? "PV: obfuscated" : "PV: obfuscation FAIL");

    /* Wipe memory, then reload from the obfuscated file and verify. */
    g_count = 0;
    do_load();                          /* PV: loaded 2 entries */
    int ok = (g_count == 2 &&
              find_entry("mail", "alice", "p@ss") &&
              find_entry("bank", "bob", "s3cret"));
    marker(ok ? "PV: selftest OK" : "PV: selftest FAIL");
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

    if      (word_is(input, length, "unlock"))   do_unlock(arg);
    else if (word_is(input, length, "lock"))     do_lock();
    else if (word_is(input, length, "add"))      do_add(arg);
    else if (word_is(input, length, "list") ||
             word_is(input, length, "ls"))       do_list();
    else if (word_is(input, length, "get"))      do_get(arg);
    else if (word_is(input, length, "save"))     do_save();
    else if (word_is(input, length, "load"))     do_load();
    else if (word_is(input, length, "selftest") ||
             word_is(input, length, "test"))     do_selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))        do_help();
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))        return 1;
    else marker("PV: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("PV: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&pv_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("PV: PASSWORD VAULT");
    marker(s_bound ? "PV: EUSER ord 915 bound"
                   : "PV: using DebugPrint marker path");
    marker("PV: locked");

    for (;;)
        {
        print("PV> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;               /* timeout / input error */
        if (run_command(line)) break;   /* quit */
        }

    marker("PV: DONE");
    con_release();
    return 0;
}
