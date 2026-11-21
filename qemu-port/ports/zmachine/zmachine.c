/*
 * zmachine.c -- a real Z-machine interpreter for the Sybian REAL port.
 *
 * Implements the Z-machine Standard 1.1 instruction set for versions 1-3
 * (the format used by Infocom's Zork I/II/III, Mini-Zork, Enchanter, ...):
 * ZSCII text decoding with abbreviations, the object/property table, the
 * dictionary and lexical analysis, routine frames/locals/stack, all v1-3
 * opcodes and the v3 status line.  Version 3 story files are limited to
 * 128 KiB, so a single static image holds both the story and its mutable
 * dynamic memory.  Nothing here allocates from the process heap.
 *
 * Save/restore to disk, timed input, sound and the version 4+ screen model
 * are deliberately out of scope: the v1-3 opcodes for them are accepted and
 * degrade gracefully (save/restore simply fail, window opcodes are no-ops).
 *
 * The file is freestanding C; the only external dependency is the zm_io
 * callback table supplied by user/zmachine32.c.
 */
#include "zmachine.h"

#ifdef ZM_TRACE
#include <stdio.h>
#define TRACE(...) fprintf(stderr, __VA_ARGS__)
#else
#define TRACE(...) ((void)0)
#endif

typedef unsigned char zbyte;
typedef unsigned short zword;

#define STORY_CAP ZM_GAME_CAPACITY      /* 131072 */
#define STACK_WORDS 1024u
#define MAX_FRAMES 64u
#define DYN_SNAP 32768u
#define LINE_SCRATCH 256u

static zbyte story[STORY_CAP];
static zword stack_[STACK_WORDS];
static unsigned sp;

typedef struct {
    unsigned long ret_pc;
    unsigned char store_var;   /* 0xff means "throw result away" */
    unsigned char nlocals;
    unsigned short sp_base;
    zword locals[15];
} zm_frame;
static zm_frame frames[MAX_FRAMES];
static unsigned nframes;

static zbyte dyn0[DYN_SNAP];   /* pristine dynamic memory, for @restart */
static unsigned long dyn0_len;

static const zm_io *io;
static unsigned version;
static unsigned long file_len;
static unsigned long high_mem;
static unsigned long globals_addr;
static unsigned long objects_addr;
static unsigned long dict_addr;
static unsigned long abbrev_addr;
static unsigned long static_addr;
static unsigned long pc;
static unsigned long steps;
static int running;
static int fatal;

static unsigned long rng_state;

/* ------------------------------------------------------------------ memory */

static zword rd_word(unsigned long a)
{
    if (a + 1 >= STORY_CAP) return 0;
    return (zword)((story[a] << 8) | story[a + 1]);
}

static void wr_word(unsigned long a, zword v)
{
    if (a + 1 >= STORY_CAP) return;
    story[a] = (zbyte)(v >> 8);
    story[a + 1] = (zbyte)v;
}

static zbyte rd_byte(unsigned long a)
{
    return a < STORY_CAP ? story[a] : 0;
}

static void wr_byte(unsigned long a, zbyte v)
{
    if (a < STORY_CAP) story[a] = v;
}

/* Both addresses used by print_paddr and by abbreviation entries are
 * "packed" addresses: word addresses for versions 1 to 3. */
static unsigned long unpack(unsigned long p) { return p << 1; }

/* ------------------------------------------------------------- variables */

static zword pop_stack(void)
{
    if (!sp) { fatal = 1; return 0; }
    return stack_[--sp];
}

static void push_stack(zword v)
{
    if (sp >= STACK_WORDS) { fatal = 1; return; }
    stack_[sp++] = v;
}

static zword get_var(zword var)
{
    if (var == 0) return pop_stack();
    if (var <= 15)
        return nframes ? frames[nframes - 1].locals[var - 1] : 0;
    return rd_word(globals_addr + 2u * (var - 16u));
}

static void set_var(zword var, zword value)
{
    if (var == 0) { push_stack(value); return; }
    if (var <= 15)
        {
        if (nframes) frames[nframes - 1].locals[var - 1] = value;
        return;
        }
    wr_word(globals_addr + 2u * (var - 16u), value);
}

/* ------------------------------------------------------------------ output */

static void out_str(const char *s)
{
    while (*s) io->out_char((unsigned char)*s++);
}

static void out_uint(int v)
{
    char buf[16];
    unsigned i = 0;
    unsigned long n;
    if (v < 0) { io->out_char('-'); n = (unsigned long)(-(long)v); }
    else n = (unsigned long)v;
    do { buf[i++] = (char)('0' + n % 10); n /= 10; } while (n);
    while (i) io->out_char((unsigned char)buf[--i]);
}

static void out_zscii(unsigned c)
{
    io->out_char(c);
}

/* ------------------------------------------------------------------- text */

/* Default V2+ alphabets.  Index 0 is Z-character 6.  A2[0] is a space but
 * Z-character 6 is handled as the 10-bit ZSCII escape before this table is
 * consulted, and Z-character 7 (A2[1]) is a newline. */
static const char *const alphabet_[3] = {
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    " ^0123456789.,!?_#'\"/\\-:()"
};

static void print_string_at(unsigned long byte_addr, int abbrev_depth);

static void print_abbrev(unsigned index, int depth)
{
    zword pa = rd_word(abbrev_addr + 2u * index);
    if (depth < 2 && pa) print_string_at(unpack(pa), depth + 1);
}

/* Decode a string of Z-characters, advancing *addr.  The bit-15 flag on the
 * last word terminates the string.  This is exactly the Standard 1.1
 * section 3 algorithm for versions 2-4. */
static void print_string(unsigned long *addr, int depth)
{
    int shift = 0;
    int status = 0;     /* 0 normal, 1 after a shift, 2/3 ZSCII escape */
    zbyte prev = 0;
    zword code;
    do
        {
        int i;
        code = rd_word(*addr);
        *addr += 2;
        for (i = 10; i >= 0; i -= 5)
            {
            zbyte c = (zbyte)((code >> i) & 0x1f);
            if (status == 2) { status = 3; prev = c; continue; }
            if (status == 3)
                {
                unsigned z = ((unsigned)prev << 5) | c;
                out_zscii(z & 0x3ff);
                status = 0; shift = 0; prev = c;
                continue;
                }
            if (status == 1)
                {
                print_abbrev((unsigned)(32u * (prev - 1u) + c), depth);
                status = 0; shift = 0; prev = c;
                continue;
                }
            if (shift == 2 && c == 6) { status = 2; prev = c; continue; }
            if (shift == 2 && c == 7) { io->out_char('\n'); shift = 0; prev = c; continue; }
            if (c == 0) { out_zscii(' '); shift = 0; }
            else if (c >= 6)
                {
                out_zscii((unsigned char)alphabet_[shift][c - 6]);
                shift = 0;
                }
            else if (c <= 3) status = 1;    /* abbreviation: next Z-character */
            else shift = (c & 1u) ? 2 : 1;  /* 4 -> A1, 5 -> A2 */
            prev = c;
            }
        }
    while (!(code & 0x8000));
}

static void print_string_at(unsigned long byte_addr, int depth)
{
    print_string(&byte_addr, depth);
}

/* ----------------------------------------------------------- object table */

static unsigned long obj_entry(unsigned obj)
{
    if (!obj || obj > 255) return 0;
    return objects_addr + 62u + (obj - 1u) * 9u;
}

static zbyte obj_parent(unsigned obj)
{
    unsigned long e = obj_entry(obj);
    return e ? rd_byte(e + 4) : 0;
}

static zbyte obj_sibling(unsigned obj)
{
    unsigned long e = obj_entry(obj);
    return e ? rd_byte(e + 5) : 0;
}

static zbyte obj_child(unsigned obj)
{
    unsigned long e = obj_entry(obj);
    return e ? rd_byte(e + 6) : 0;
}

static void set_obj_parent(unsigned obj, zbyte v)
{
    unsigned long e = obj_entry(obj); if (e) wr_byte(e + 4, v);
}
static void set_obj_sibling(unsigned obj, zbyte v)
{
    unsigned long e = obj_entry(obj); if (e) wr_byte(e + 5, v);
}
static void set_obj_child(unsigned obj, zbyte v)
{
    unsigned long e = obj_entry(obj); if (e) wr_byte(e + 6, v);
}

static unsigned long prop_addr(unsigned obj, unsigned prop)
{
    unsigned long e = obj_entry(obj);
    unsigned long p;
    if (!e) return 0;
    p = rd_word(e + 7);
    if (!p || p >= STORY_CAP) return 0;
    p += 1u + 2u * (unsigned)rd_byte(p);   /* skip the short-name header */
    while (p < STORY_CAP && rd_byte(p))
        {
        zbyte sz = rd_byte(p);
        unsigned pn = sz & 0x1fu;
        unsigned len = (sz >> 5) + 1u;
        if (pn == prop) return p + 1;
        if (pn < prop) return 0;            /* properties are descending */
        p += 1u + len;
        }
    return 0;
}

static unsigned prop_len(unsigned long addr)
{
    return addr ? (unsigned)(rd_byte(addr - 1) >> 5) + 1u : 0;
}

static zword get_prop(unsigned obj, unsigned prop)
{
    unsigned long a = prop_addr(obj, prop);
    if (!a) return rd_word(objects_addr + 2u * prop);   /* property default */
    if (prop_len(a) == 1) return rd_byte(a);
    return rd_word(a);
}

static void put_prop(unsigned obj, unsigned prop, zword value)
{
    unsigned long a = prop_addr(obj, prop);
    if (!a) return;
    if (prop_len(a) == 1) wr_byte(a, (zbyte)value);
    else wr_word(a, value);
}

static unsigned next_prop(unsigned obj, unsigned prop)
{
    unsigned long e = obj_entry(obj);
    unsigned long p;
    if (!e) return 0;
    p = rd_word(e + 7);
    if (!p || p >= STORY_CAP) return 0;
    p += 1u + 2u * (unsigned)rd_byte(p);
    while (p < STORY_CAP && rd_byte(p))
        {
        zbyte sz = rd_byte(p);
        unsigned pn = sz & 0x1fu;
        unsigned len = (sz >> 5) + 1u;
        if (prop == 0) return pn;
        if (pn == prop)
            {
            p += 1u + len;
            return p < STORY_CAP && rd_byte(p) ? (unsigned)(rd_byte(p) & 0x1fu) : 0u;
            }
        p += 1u + len;
        }
    return 0;
}

static void print_obj(unsigned obj)
{
    unsigned long e = obj_entry(obj);
    unsigned long p;
    if (!e) return;
    p = rd_word(e + 7);
    if (!p || p >= STORY_CAP) return;
    p += 1;
    print_string(&p, 0);
}

static void remove_obj(unsigned obj)
{
    unsigned parent;
    unsigned long e = obj_entry(obj);
    if (!e) return;
    parent = obj_parent(obj);
    if (!parent) return;
        {
        unsigned long pe = obj_entry(parent);
        unsigned c = pe ? rd_byte(pe + 6) : 0;
        if (c == obj) wr_byte(pe + 6, obj_sibling(obj));
        else
            {
            while (c && obj_sibling(c) != obj) c = obj_sibling(c);
            if (c) set_obj_sibling(c, obj_sibling(obj));
            }
        }
    set_obj_parent(obj, 0);
    set_obj_sibling(obj, 0);
}

static void insert_obj(unsigned obj, unsigned dest)
{
    if (!obj_entry(obj) || !obj_entry(dest)) return;
    remove_obj(obj);
    set_obj_parent(obj, (zbyte)dest);
    set_obj_sibling(obj, obj_child(dest));
    set_obj_child(dest, (zbyte)obj);
}

/* -------------------------------------------------------------- dictionary */

static unsigned dict_nentries_;
static unsigned long dict_entries_;
static zbyte dict_entry_len_;
static zbyte dict_seps_[16];
static unsigned dict_nseps_;

static void dict_init(void)
{
    unsigned i;
    dict_nseps_ = rd_byte(dict_addr);
    if (dict_nseps_ > 16) dict_nseps_ = 16;
    for (i = 0; i < dict_nseps_; ++i)
        dict_seps_[i] = rd_byte(dict_addr + 1u + i);
    dict_entry_len_ = rd_byte(dict_addr + 1u + dict_nseps_);
    dict_nentries_ = rd_word(dict_addr + 2u + dict_nseps_);
    dict_entries_ = dict_addr + 4u + dict_nseps_;
    if (dict_entry_len_ < 4) dict_entry_len_ = 7;
}

static int is_separator(unsigned char c)
{
    unsigned i;
    for (i = 0; i < dict_nseps_; ++i)
        if ((unsigned char)dict_seps_[i] == c) return 1;
    return 0;
}

/* Encode a lower-case word into the four bytes used by a v1-3 dictionary
 * entry: six Z-characters packed into two words, last word marked terminal. */
static void encode_word(const char *s, unsigned len, zbyte out[4])
{
    zbyte z[6] = { 5, 5, 5, 5, 5, 5 };
    unsigned i = 0, k;
    for (k = 0; k < len && i < 6; ++k)
        {
        unsigned char c = (unsigned char)s[k];
        int set, idx;
        int found = 0;
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        for (set = 0; set < 3 && !found; ++set)
            for (idx = 0; idx < 26; ++idx)
                if (c == (unsigned char)alphabet_[set][idx])
                    { found = 1; break; }
        /* Breaking out of the inner loop still runs the outer loop's ++set,
         * so undo that increment when a character was actually found. */
        if (found) --set;
        else { set = 2; idx = 15; }   /* '?' */
        if (set && i < 6) z[i++] = (zbyte)((set == 1) ? 4 : 5);
        if (i < 6) z[i++] = (zbyte)(idx + 6);
        }
    {
    zword w0 = (zword)((z[0] << 10) | (z[1] << 5) | z[2]);
    zword w1 = (zword)((z[3] << 10) | (z[4] << 5) | z[5]);
    w1 |= 0x8000;
    out[0] = (zbyte)(w0 >> 8); out[1] = (zbyte)w0;
    out[2] = (zbyte)(w1 >> 8); out[3] = (zbyte)w1;
    }
}

static zword dict_lookup(const zbyte enc[4])
{
    unsigned i;
    for (i = 0; i < dict_nentries_; ++i)
        {
        unsigned long e = dict_entries_ + (unsigned long)i * dict_entry_len_;
        if (e + 4 > STORY_CAP) break;
        if (rd_byte(e) == enc[0] && rd_byte(e + 1) == enc[1] &&
            rd_byte(e + 2) == enc[2] && rd_byte(e + 3) == enc[3])
            return (zword)e;
        }
    return 0;
}

/* Write the v1-3 parse buffer: [max, count, (addr, len, pos) ...]. */
static void tokenise(unsigned long text_buf, unsigned long parse_buf)
{
    unsigned max_words = rd_byte(parse_buf);
    unsigned len = 0;
    unsigned cap = rd_byte(text_buf);
    /* V1-V4 text buffers hold a zero-terminated string; byte 0 is the
     * capacity, not the length.  Scan for the terminator like Frotz. */
    while (len < cap && rd_byte(text_buf + 1u + len)) ++len;
    if (max_words > 32) max_words = 32;   /* early v3 games store 240 here */
    unsigned i = 0, n = 0;
    wr_byte(parse_buf + 1, 0);
    while (i < len && n < max_words)
        {
        unsigned start;
        unsigned char c;
        while (i < len && rd_byte(text_buf + 1u + i) == ' ') ++i;
        if (i >= len) break;
        start = i;
        c = (unsigned char)rd_byte(text_buf + 1u + i);
        if (is_separator(c))
            {
            ++i;
            wr_word(parse_buf + 2u + 4u * n, 0);
            wr_byte(parse_buf + 4u + 4u * n, 1);
            wr_byte(parse_buf + 5u + 4u * n, (zbyte)(start + 1u));
            ++n;
            continue;
            }
        while (i < len)
            {
            unsigned char d = (unsigned char)rd_byte(text_buf + 1u + i);
            if (d == ' ' || is_separator(d)) break;
            ++i;
            }
        {
        unsigned wlen = i - start;
        zbyte enc[4];
        unsigned long a;
        {
        char word[LINE_SCRATCH];
        unsigned k;
        if (wlen >= LINE_SCRATCH) wlen = LINE_SCRATCH - 1u;
        for (k = 0; k < wlen; ++k)
            {
            char ch = (char)rd_byte(text_buf + 1u + start + k);
            if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
            word[k] = ch;
            }
        word[wlen] = 0;
        encode_word(word, wlen, enc);
        }
        a = dict_lookup(enc);
        TRACE("tokenise len=%u enc=%02x%02x%02x%02x dict=%u first=%u\n",
              wlen, enc[0], enc[1], enc[2], enc[3], (unsigned)a,
              (unsigned)rd_byte(text_buf + 1u + start));
        wr_word(parse_buf + 2u + 4u * n, (zword)a);
        wr_byte(parse_buf + 4u + 4u * n, (zbyte)wlen);
        wr_byte(parse_buf + 5u + 4u * n, (zbyte)(start + 1u));
        ++n;
        }
    }
    wr_byte(parse_buf + 1, (zbyte)n);
}

/* -------------------------------------------------------------- status line */

static void show_status(void)
{
    zword room = rd_word(globals_addr);
    zword a = rd_word(globals_addr + 2u);
    zword b = rd_word(globals_addr + 4u);
    int time_game = (rd_byte(1) & 0x02u) != 0;
    out_str("[");
    if (room) print_obj(room);
    out_str(" | ");
    if (time_game)
        {
        out_str("Time "); out_uint(a); io->out_char(':'); out_uint(b);
        }
    else
        {
        out_str("Score "); out_uint((int)(short)a);
        out_str(" Turns "); out_uint((int)(short)b);
        }
    out_str("]");
    io->out_char('\n');
}

/* ----------------------------------------------------------- opcode tables */

enum { F_LONG, F_1OP, F_0OP, F_VAR2, F_VAR, F_EXT };

static int has_store(int form, int num)
{
    if (form == F_LONG || form == F_VAR2)
        switch (num) {
        case 8: case 9: case 15: case 16: case 17: case 18: case 19:
        case 20: case 21: case 22: case 23: case 24: case 25: return 1;
        }
    else if (form == F_1OP)
        switch (num) {
        case 1: case 2: case 3: case 4: case 8: case 14: case 15: return 1;
        }
    else if (form == F_VAR)
        switch (num) {
        case 0: case 7: case 17: case 18: case 31: return 1;
        }
    return 0;
}

static int has_branch(int form, int num)
{
    if (form == F_LONG || form == F_VAR2)
        switch (num) {
        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 10: return 1;
        }
    else if (form == F_1OP)
        switch (num) {
        case 0: case 1: case 2: return 1;
        }
    else if (form == F_0OP)
        switch (num) {
        case 5: case 6: case 13: case 15: return 1;
        }
    return 0;
}

/* ----------------------------------------------------------- random numbers */

static zword zrandom(zword range)
{
    if ((short)range < 0)
        {
        rng_state = (unsigned long)(-(int)(short)range);
        if (!rng_state) rng_state = 1;
        return 0;
        }
    rng_state = rng_state * 1103515245u + 12345u;
    if (!range) return (zword)(rng_state >> 16);
    return (zword)(((rng_state >> 16) % (unsigned long)(unsigned short)range) + 1u);
}

/* ---------------------------------------------------------- initialisation */

unsigned char *zm_game_buffer(void) { return story; }
unsigned zm_game_capacity(void) { return (unsigned)STORY_CAP; }
unsigned long zm_instructions(void) { return steps; }
unsigned zm_game_version(void) { return version; }
unsigned long zm_game_length(void) { return file_len; }

int zm_init(const zm_io *io_)
{
    unsigned i;
    io = io_;
    version = rd_byte(0);
    if (version == 0 || version > 3) return -1;
    file_len = (unsigned long)rd_word(0x1a) * 2u;
    if (file_len < 64 || file_len > STORY_CAP) return -1;
    /* For V1-V5 the header stores byte addresses (not word addresses);
     * only packed routine/string addresses need the *2 conversion. */
    high_mem = rd_word(0x04);
    pc = rd_word(0x06);
    dict_addr = rd_word(0x08);
    objects_addr = rd_word(0x0a);
    globals_addr = rd_word(0x0c);
    static_addr = rd_word(0x0e);
    abbrev_addr = rd_word(0x18);
    if (pc < 64 || pc >= file_len) return -1;
    if (objects_addr + 62u >= file_len) return -1;
    if (globals_addr + 480u >= file_len) return -1;
    dict_init();
    sp = 0; nframes = 0; steps = 0; fatal = 0; running = 1;
    rng_state = 0x9e3779b9u;
    dyn0_len = static_addr < DYN_SNAP ? static_addr : DYN_SNAP;
    for (i = 0; i < dyn0_len; ++i) dyn0[i] = story[i];
    return 0;
}

static void restart_game(void)
{
    unsigned i;
    for (i = 0; i < dyn0_len; ++i) story[i] = dyn0[i];
    sp = 0; nframes = 0;
    pc = rd_word(0x06);
    rng_state = 0x9e3779b9u;
}

/* ------------------------------------------------------------- interpreter */

static void do_return(zword value)
{
    zm_frame *f;
    if (!nframes) { running = 0; return; }
    f = &frames[--nframes];
    sp = f->sp_base;
    pc = f->ret_pc;
    if (f->store_var != 0xff) set_var(f->store_var, value);
}

static int call_routine(unsigned long packed, int nargs, zword *args,
                        unsigned char store_var)
{
    unsigned long addr;
    zm_frame *f;
    int i;
    if (!packed) return 0;              /* calling routine 0 returns false */
    addr = unpack(packed);
    if (addr >= file_len) return 0;
    if (nframes >= MAX_FRAMES) { fatal = 1; return 0; }
    f = &frames[nframes++];
    f->ret_pc = pc;
    f->store_var = store_var;
    f->sp_base = (unsigned short)sp;
    f->nlocals = (unsigned char)(rd_byte(addr) & 0x0fu);
    addr++;
    for (i = 0; i < f->nlocals; ++i)
        {
        f->locals[i] = rd_word(addr);
        addr += 2;
        }
    for (i = 0; i < 15; ++i)
        if (i >= f->nlocals) f->locals[i] = 0;
    for (i = 0; i < nargs && i < 15; ++i) f->locals[i] = args[i];
    pc = addr;
    return 1;
}

int zm_run(void)
{
    while (running && !fatal)
        {
        zbyte opbyte = rd_byte(pc++);
        int form, num, nops = 0;
        zbyte types = 0;
        zword op[8];
        unsigned char store_var = 0xff;
        int branch_valid = 0, branch_on_true = 0, branch_cond = 0;
        int offset = 0;
        zword result = 0;
        int skip_store = 0;
        int i;

        ++steps;
        if (steps > 20000000ul) { fatal = 1; break; }
        TRACE("step=%lu pc_before=%lu op=%02x sp=%u frames=%u\n",
              steps, pc - 1, opbyte, sp, nframes);

        if (opbyte < 0x80)
            {
            form = F_LONG; num = opbyte & 0x1fu;
            types = (zbyte)(((opbyte & 0x40u) ? 2u : 1u) << 6 |
                            ((opbyte & 0x20u) ? 2u : 1u) << 4);
            }
        else if (opbyte < 0xc0)
            {
            if ((opbyte & 0x30u) == 0x30u) { form = F_0OP; num = opbyte & 0x0fu; }
            else { form = F_1OP; num = opbyte & 0x0fu; }
            types = (zbyte)((opbyte >> 4) << 6);
            }
        else
            {
            num = opbyte & 0x1fu;
            if (opbyte & 0x20u) form = F_VAR; else form = F_VAR2;
            types = rd_byte(pc++);
            }

        {
        int ntypes = (form == F_LONG) ? 2 :
                     (form == F_1OP || form == F_0OP) ? 1 : 4;
        for (i = 0; i < ntypes && nops < 8; ++i)
            {
            unsigned t = (unsigned)((types >> (6 - 2 * i)) & 3u);
            if (t == 3) break;
            if (t == 0) { op[nops] = rd_word(pc); pc += 2; }
            else if (t == 1) op[nops] = rd_byte(pc++);
            else op[nops] = get_var(rd_byte(pc++));
            ++nops;
            }
        for (i = nops; i < 8; ++i) op[i] = 0;
        }

        TRACE("  form=%d num=%d nops=%d op0=%04x op1=%04x pc=%lu\n",
              form, num, nops, op[0], op[1], pc);
        if (has_store(form, num)) store_var = rd_byte(pc++);
        if (has_branch(form, num))
            {
            int b = rd_byte(pc++);
            branch_valid = 1;
            branch_on_true = (b & 0x80u) != 0;
            if (b & 0x40u) offset = b & 0x3fu;
            else
                {
                offset = ((b & 0x3fu) << 8) | rd_byte(pc++);
                if (offset & 0x2000) offset -= 0x4000;
                }
            }

        switch (form)
            {
        /* ------------------------------------------------ 2OP opcodes */
        case F_LONG:
        case F_VAR2:
            switch (num)
                {
            case 1:  branch_cond = (op[0] == op[1]); break;   /* je (extra operands) */
            case 2:  branch_cond = (short)op[0] < (short)op[1]; break;
            case 3:  branch_cond = (short)op[0] > (short)op[1]; break;
            case 4:  { zword v = (zword)(get_var(op[0]) - 1); set_var(op[0], v);
                       branch_cond = (short)v < (short)op[1]; break; }
            case 5:  { zword v = (zword)(get_var(op[0]) + 1); set_var(op[0], v);
                       branch_cond = (short)v > (short)op[1]; break; }
            case 6:  branch_cond = (obj_parent(op[0]) == (zbyte)op[1]); break;
            case 7:  branch_cond = ((op[0] & op[1]) == op[1]); break;
            case 8:  result = (zword)(op[0] | op[1]); break;
            case 9:  result = (zword)(op[0] & op[1]); break;
            case 10: { unsigned long e = obj_entry(op[0]);
                       branch_cond = e && (rd_byte(e + (op[1] >> 3)) &
                                           (0x80u >> (op[1] & 7u))); break; }
            case 11: { unsigned long e = obj_entry(op[0]);
                       if (e && op[1] < 32) wr_byte(e + (op[1] >> 3),
                            (zbyte)(rd_byte(e + (op[1] >> 3)) | (0x80u >> (op[1] & 7u))));
                       break; }
            case 12: { unsigned long e = obj_entry(op[0]);
                       if (e && op[1] < 32) wr_byte(e + (op[1] >> 3),
                            (zbyte)(rd_byte(e + (op[1] >> 3)) & ~(0x80u >> (op[1] & 7u))));
                       break; }
            case 13: set_var(op[0], op[1]); break;
            case 14: insert_obj(op[0], op[1]); break;
            case 15: result = rd_word((unsigned long)op[0] + 2u * (unsigned long)op[1]); break;
            case 16: result = rd_byte((unsigned long)op[0] + (unsigned long)op[1]); break;
            case 17: result = get_prop(op[0], op[1]); break;
            case 18: result = (zword)prop_addr(op[0], op[1]); break;
            case 19: result = (zword)next_prop(op[0], op[1]); break;
            case 20: result = (zword)(op[0] + op[1]); break;
            case 21: result = (zword)(op[0] - op[1]); break;
            case 22: result = (zword)(op[0] * op[1]); break;
            case 23: if (!op[1]) { fatal = 1; break; }
                     result = (zword)((short)op[0] / (short)op[1]); break;
            case 24: if (!op[1]) { fatal = 1; break; }
                     result = (zword)((short)op[0] % (short)op[1]); break;
            case 25: { zword a[1]; a[0] = op[1];
                       skip_store = call_routine(op[0], 1, a, store_var); break; }
            default: break;
                }
            /* VAR-form je may carry up to four operands. */
            if (num == 1 && nops > 2)
                {
                int k;
                branch_cond = 0;
                for (k = 1; k < nops; ++k)
                    if (op[0] == op[k]) { branch_cond = 1; break; }
                }
            break;

        /* ------------------------------------------------ 1OP opcodes */
        case F_1OP:
            switch (num)
                {
            case 0:  branch_cond = (op[0] == 0); break;
            case 1:  result = obj_sibling(op[0]);
                     branch_cond = (result != 0); break;
            case 2:  result = obj_child(op[0]);
                     branch_cond = (result != 0); break;
            case 3:  result = obj_parent(op[0]); break;
            case 4:  result = (zword)prop_len(op[0]); break;
            case 5:  set_var(op[0], (zword)(get_var(op[0]) + 1)); break;
            case 6:  set_var(op[0], (zword)(get_var(op[0]) - 1)); break;
            case 7:  print_string_at(op[0], 0); break;
            case 8:  skip_store = call_routine(op[0], 0, 0, store_var); break;
            case 9:  remove_obj(op[0]); break;
            case 10: print_obj(op[0]); break;
            case 11: do_return(op[0]); break;
            case 12: pc = (unsigned long)((long)pc + (short)op[0] - 2); break;
            case 13: print_string_at(unpack(op[0]), 0); break;
            case 14: result = get_var(op[0]); break;
            case 15: result = (zword)~op[0]; break;
            default: break;
                }
            break;

        /* ------------------------------------------------ 0OP opcodes */
        case F_0OP:
            switch (num)
                {
            case 0: do_return(1); break;
            case 1: do_return(0); break;
            case 2: print_string(&pc, 0); break;
            case 3: print_string(&pc, 0); io->out_char('\n'); do_return(1); break;
            case 4: break;                                  /* nop */
            case 5: break;                                  /* save: fail */
            case 6: break;                                  /* restore: fail */
            case 7: restart_game(); break;
            case 8: do_return(pop_stack()); break;
            case 9: if (sp) --sp; break;
            case 10: running = 0; break;                    /* quit */
            case 11: io->out_char('\n'); break;
            case 12: show_status(); break;
            case 13: { unsigned long x; unsigned long sum = 0;
                       for (x = 64; x < file_len; ++x) sum += rd_byte(x);
                       branch_cond = ((sum & 0xffffu) == rd_word(0x1c)); break; }
            case 15: branch_cond = 1; break;                /* piracy: gullible */
            default: break;
                }
            break;

        /* ------------------------------------------------ VAR opcodes */
        case F_VAR:
            switch (num)
                {
            case 0:  { zword a[8]; int k;
                       for (k = 1; k < nops && k < 8; ++k) a[k - 1] = op[k];
                       skip_store = call_routine(op[0], nops ? nops - 1 : 0, a,
                                                 store_var); break; }
            case 1:  wr_word((unsigned long)op[0] + 2u * (unsigned long)op[1], op[2]); break;
            case 2:  wr_byte((unsigned long)op[0] + (unsigned long)op[1], (zbyte)op[2]); break;
            case 3:  put_prop(op[0], op[1], op[2]); break;
            case 4:  { unsigned long txt = op[0], parse = op[1];
                       unsigned max = rd_byte(txt), len, k;
                       char buf[256];
                       int r;
                       io->flush();             /* emit the '>' prompt line */
                       show_status();
                       io->flush();
                       if (max > 254) max = 254;
                       /* V1-V4: byte 0 holds (capacity - 1) and must survive
                        * the read; the game finds the length via byte 1's
                        * zero terminator.  Do not overwrite byte 0. */
                       if (max) --max;
                       r = io->read_line(buf, max + 1u);
                       if (r < 0) { running = 0; break; }  /* EOF -> quit */
                       len = (unsigned)r;
                       if (len > max) len = max;
                       for (k = 0; k < len; ++k)
                           {
                           char ch = buf[k];
                           if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
                           wr_byte(txt + 1u + k, (zbyte)ch);
                           }
                       wr_byte(txt + 1u + len, 0);
                       TRACE("read txt=%lu max0=%u len=%u bytes=%02x %02x %02x %02x %02x %02x %02x %02x\n",
                             txt, (unsigned)rd_byte(txt), len,
                             rd_byte(txt + 1), rd_byte(txt + 2), rd_byte(txt + 3),
                             rd_byte(txt + 4), rd_byte(txt + 5), rd_byte(txt + 6),
                             rd_byte(txt + 7), rd_byte(txt + 8));
                       tokenise(txt, parse);
                       break; }
            case 5:  out_zscii(op[0] & 0x3ffu); break;
            case 6:  out_uint((int)(short)op[0]); break;
            case 7:  result = zrandom(op[0]); break;
            case 8:  push_stack(op[0]); break;
            case 9:  set_var(op[0], pop_stack()); break;
            case 10: case 11: case 13: case 14: break;      /* window/streams */
            case 17: { zword x = op[0], t = op[1]; int n = (int)(short)op[2];
                       int k; result = 0;
                       for (k = 0; k < n; ++k)
                           if (rd_word((unsigned long)t + 2u * (unsigned)k) == x)
                               { result = (zword)((unsigned long)t + 2u * (unsigned)k); break; }
                       branch_cond = (result != 0); break; }
            case 18: result = (zword)~op[0]; break;
            default: break;
                }
            break;

        case F_EXT:
        default:
            break;                                          /* v5+ / unknown */
            }

        if (!skip_store && store_var != 0xff) set_var(store_var, result);
        if (branch_valid)
            {
            int take = branch_on_true ? branch_cond : !branch_cond;
            if (take)
                {
                if (offset == 0) do_return(0);
                else if (offset == 1) do_return(1);
                else pc = (unsigned long)((long)pc + offset - 2);
                }
            }
        }

    if (fatal) return -1;
    io->flush();
    return 0;
}
