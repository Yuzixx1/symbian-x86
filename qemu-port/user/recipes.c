/*
 * recipes.c -- a small RECIPE BOOK end-user app for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...).  ld defines the IAT slot at
 * codeBase+0x8000 (0x408000); elf2e32.py --euser-import 915 emits the PE2
 * import section and the loader overwrites the slot with the resolved export
 * address before the process resumes.  Markers go through that ordinal, with
 * a DebugPrint fallback so the trace is always visible on the serial console.
 *
 * The book starts with five built-in recipes (name, ingredient list, steps).
 * "list" prints every name/ingredient, "show <name>" adds the steps, "save"
 * serialises all names + ingredients to RECIPES.TXT on the FAT disk and
 * "load" reads it back, both through PortFile IPC (runtime.c
 * file_open/file_read/file_save), i.e. the portfilesrv service exactly like
 * ADDRBOOK / FCOPY / TEXTEDIT / FILEMGR.  file_read is capped at 256 bytes per
 * ChannelRequest, so load slurps in 256-byte chunks.  Console commands
 * (list / show / save / load / selftest / help / quit) arrive via PortCon
 * read_line().
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*recipes_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&recipes_printf_slot;
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

/* Case-insensitive equality of two NUL-terminated strings. */
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

/* ------------------------------ the recipe book --------------------------- */

#define MAX_RECIPES 16
#define NAME_CAP   32
#define ING_CAP    40
#define MAX_INGS   12
#define DISK_CAP   8192

struct Recipe {
    char name[NAME_CAP];
    char ings[MAX_INGS][ING_CAP];
    unsigned ingN;
};

static struct Recipe g_recipes[MAX_RECIPES];
static unsigned g_count = 0;
static char g_disk[DISK_CAP + 1];   /* save/load staging buffer */
static char line[128];

static const char kPath[] = "RECIPES.TXT";

/* ---- five built-in recipes: name, ingredient list and preparation steps -- */

static const char* const kPancakes[] = {
    "flour", "milk", "egg", "baking powder", "salt"
};
static const char* const kSoup[] = {
    "tomatoes", "onion", "garlic", "vegetable broth", "basil"
};
static const char* const kSalad[] = {
    "romaine lettuce", "croutons", "parmesan", "caesar dressing", "black pepper"
};
static const char* const kRice[] = {
    "rice", "eggs", "soy sauce", "peas", "scallions"
};
static const char* const kCake[] = {
    "flour", "cocoa powder", "sugar", "eggs", "butter", "baking powder"
};

struct Builtin {
    const char* name;
    const char* const* ings;
    unsigned ingN;
    const char* steps;
};

static const struct Builtin kBuiltins[] = {
    { "Pancakes", kPancakes, 5,
      "Mix the dry ingredients, whisk milk with the egg, combine and cook on a hot griddle." },
    { "Tomato Soup", kSoup, 5,
      "Saute onion and garlic, add tomatoes and broth, simmer, then blend until smooth." },
    { "Caesar Salad", kSalad, 5,
      "Chop the lettuce, toss with dressing, then top with croutons and parmesan." },
    { "Fried Rice", kRice, 5,
      "Scramble the eggs, add cold rice and peas, then stir in soy sauce and scallions." },
    { "Chocolate Cake", kCake, 6,
      "Cream butter and sugar, beat in the eggs, fold in dry ingredients and bake at 180C." },
};
#define NBUILTIN ((unsigned)(sizeof(kBuiltins) / sizeof(kBuiltins[0])))

/* ------------------------------ in-memory book ---------------------------- */

static void clear_recipes(void) { g_count = 0; }

static void add_recipe_n(const char* name, unsigned nameLen)
{
    if (!nameLen || g_count >= MAX_RECIPES) return;
    unsigned at = g_count++;
    copy_n(g_recipes[at].name, NAME_CAP, name, nameLen);
    g_recipes[at].ingN = 0;
}

static void add_ingredient_n(unsigned ri, const char* ing, unsigned len)
{
    if (ri >= g_count || !len || g_recipes[ri].ingN >= MAX_INGS) return;
    unsigned k = g_recipes[ri].ingN++;
    copy_n(g_recipes[ri].ings[k], ING_CAP, ing, len);
}

static void init_builtin(void)
{
    unsigned b, j;
    clear_recipes();
    for (b = 0; b < NBUILTIN; ++b)
        {
        unsigned at = g_count;
        add_recipe_n(kBuiltins[b].name, cstrlen(kBuiltins[b].name));
        for (j = 0; j < kBuiltins[b].ingN; ++j)
            add_ingredient_n(at, kBuiltins[b].ings[j],
                             cstrlen(kBuiltins[b].ings[j]));
        }
}

/* Index of a recipe name (case-insensitive), or -1 when absent. */
static int find_index(const char* name)
{
    unsigned i;
    for (i = 0; i < g_count; ++i)
        if (name_eq(g_recipes[i].name, name)) return (int)i;
    return -1;
}

static const char* steps_for(const char* name)
{
    unsigned b;
    for (b = 0; b < NBUILTIN; ++b)
        if (name_eq(kBuiltins[b].name, name)) return kBuiltins[b].steps;
    return 0;
}

/* ------------------------------ marker builders --------------------------- */

static void marker_name(const char* name)
{
    char msg[NAME_CAP + 8];
    unsigned n = append_str(msg, 0, "RB: ");
    n = append_str(msg, n, name);
    marker(msg);
}

static void marker_ing(const char* ing)
{
    char msg[ING_CAP + 12];
    unsigned n = append_str(msg, 0, "RB:   - ");
    n = append_str(msg, n, ing);
    marker(msg);
}

static void marker_count(const char* tag, unsigned value)
{
    char msg[64];
    unsigned n = append_str(msg, 0, tag);
    n = append_uint(msg, n, value);
    n = append_str(msg, n, " recipes");
    marker(msg);
}

/* ------------------------------ commands ---------------------------------- */

static void list_recipes(void)
{
    unsigned i, j;
    if (!g_count) { marker("RB: (empty)"); return; }
    for (i = 0; i < g_count; ++i)
        {
        marker_name(g_recipes[i].name);
        for (j = 0; j < g_recipes[i].ingN; ++j)
            marker_ing(g_recipes[i].ings[j]);
        }
}

static void show_recipe(const char* name)
{
    if (!*name) { marker("RB: usage: show <name>"); return; }
    int at = find_index(name);
    if (at < 0) { marker("RB: not found"); return; }
    unsigned i = (unsigned)at, j;
    marker_name(g_recipes[i].name);
    for (j = 0; j < g_recipes[i].ingN; ++j)
        marker_ing(g_recipes[i].ings[j]);
    const char* steps = steps_for(g_recipes[i].name);
    if (steps)
        {
        char msg[192];
        unsigned n = append_str(msg, 0, "RB: steps: ");
        n = append_str(msg, n, steps);
        print(msg);
        }
}

/* Serialise every recipe as "@name\r\n" followed by "- ingredient\r\n" lines.
 * Only names and ingredients go to disk (steps stay in the built-in table). */
static void save_recipes(void)
{
    unsigned n = 0, i, j;
    for (i = 0; i < g_count; ++i)
        {
        if (n + NAME_CAP + 8 > DISK_CAP) break;
        g_disk[n++] = '@';
        n = append_str(g_disk, n, g_recipes[i].name);
        n = append_str(g_disk, n, "\r\n");
        for (j = 0; j < g_recipes[i].ingN; ++j)
            {
            if (n + ING_CAP + 8 > DISK_CAP) break;
            n = append_str(g_disk, n, "- ");
            n = append_str(g_disk, n, g_recipes[i].ings[j]);
            n = append_str(g_disk, n, "\r\n");
            }
        }
    if (file_save(kPath, g_disk, n) < 0)
        { marker("RB: save failed"); return; }
    marker_count("RB: saved ", g_count);
}

/* Slurp RECIPES.TXT in 256-byte reads and rebuild the in-memory book. */
static void load_recipes(void)
{
    int handle = file_open(kPath);
    if (handle < 0) { marker("RB: load failed"); return; }
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

    clear_recipes();
    unsigned i = 0;
    int current = -1;
    while (i < total)
        {
        unsigned start = i;
        while (i < total && g_disk[i] != '\n') ++i;
        unsigned len = i - start;
        if (len && g_disk[start + len - 1] == '\r') --len;  /* strip CR */
        const char* p = g_disk + start;
        if (len >= 1 && p[0] == '@')
            {
            add_recipe_n(p + 1, len - 1);
            current = (int)g_count - 1;
            }
        else if (len >= 2 && p[0] == '-' && p[1] == ' ' && current >= 0)
            add_ingredient_n((unsigned)current, p + 2, len - 2);
        if (i < total) ++i;             /* skip the '\n' */
        }
    marker_count("RB: loaded ", g_count);
}

static void do_selftest(void)
{
    unsigned b, j;
    init_builtin();
    list_recipes();                     /* RB: <name> / RB:   - <ingredient> */
    save_recipes();                     /* RB: saved 5 recipes */
    clear_recipes();                    /* prove the data survives only on FAT */
    load_recipes();                     /* RB: loaded 5 recipes */

    int ok = (g_count == NBUILTIN);
    for (b = 0; ok && b < NBUILTIN; ++b)
        {
        int at = find_index(kBuiltins[b].name);
        if (at < 0 || g_recipes[at].ingN != kBuiltins[b].ingN)
            { ok = 0; break; }
        for (j = 0; j < kBuiltins[b].ingN; ++j)
            if (!name_eq(g_recipes[at].ings[j], kBuiltins[b].ings[j]))
                { ok = 0; break; }
        }
    marker(ok ? "RB: selftest OK" : "RB: selftest FAIL");
}

static void do_help(void)
{
    print("RB: commands: list  show <name>  save  load  selftest  help  quit");
}

/* Parse "command [arguments]" from a console line.  1 means quit. */
static int run_command(const char* input)
{
    input = skip_spaces(input);
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = skip_spaces(word);

    if      (word_is(input, length, "list"))     list_recipes();
    else if (word_is(input, length, "show"))     { char n[NAME_CAP];
                                                   copy_token(n, sizeof(n), arg);
                                                   show_recipe(n); }
    else if (word_is(input, length, "save"))     save_recipes();
    else if (word_is(input, length, "load"))     load_recipes();
    else if (word_is(input, length, "selftest") ||
             word_is(input, length, "test"))     do_selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))        do_help();
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))        return 1;
    else marker("RB: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("RB: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&recipes_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("RB: RECIPE BOOK");
    marker(s_bound ? "RB: EUSER ord 915 bound"
                   : "RB: using DebugPrint marker path");

    /* A listing on startup guarantees the RB name/ingredient markers even
     * with no console input.  The built-in table is the initial book. */
    init_builtin();
    list_recipes();

    for (;;)
        {
        print("RB> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) break;               /* timeout / input error -> done */
        if (run_command(line)) break;   /* quit */
        }

    marker("RB: DONE");
    con_release();
    return 0;
}
