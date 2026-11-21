/*
 * zip32.c -- zlib-compatible gzip / deflate utility for the Sybian REAL port.
 *
 * The delivered PE2 E32 links against EUSER.DLL through one genuine PE2
 * import: ordinal 915 = RDebug::Printf(char const*, ...) (see
 * kernelhwsrv/kernel/eka/bx86gcc/euseru.def).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader binds the slot before the process resumes.  Markers go
 * through that ordinal, with a DebugPrint fallback so the serial log is
 * always visible.
 *
 * The codec in ports/miniz/ is a genuine, freestanding RFC1951 implementation
 * (LZ77 + fixed Huffman deflate, full stored/fixed/dynamic inflate).  File I/O
 * uses PortFile IPC (runtime.c file_open/file_read/file_save), like FCOPY.
 */
#include "runtime.h"
#include "miniz.h"

/* PE2 IAT slot, defined with --defsym in user.mk. */
extern void (*zip32_printf_slot)(const char*);

static int s_bound = 0;

/* Markers are plain strings (no % specifiers): the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&zip32_printf_slot;
    if (fn != 915u && fn != 0u)
        {
        s_bound = 1;
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
        }
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------ small string helpers (freestanding) ------- */
static unsigned str_append(char* out, unsigned n, const char* text)
{
    while (*text) out[n++] = *text++;
    out[n] = 0;
    return n;
}
static unsigned str_uint(char* out, unsigned n, unsigned value)
{
    char digits[12];
    int d = 0;
    if (!value) digits[d++] = '0';
    while (value) { digits[d++] = (char)('0' + value % 10); value /= 10; }
    while (d) out[n++] = digits[--d];
    out[n] = 0;
    return n;
}
static unsigned str_hex(char* out, unsigned n, unsigned value)
{
    static const char kHex[] = "0123456789abcdef";
    int shift;
    for (shift = 28; shift >= 0; shift -= 4)
        out[n++] = kHex[(value >> shift) & 0xFu];
    out[n] = 0;
    return n;
}
static int word_is(const char* w, unsigned len, const char* lit)
{
    unsigned i = 0;
    while (i < len && lit[i])
        {
        char a = w[i], b = lit[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
        ++i;
        }
    return (i == len && lit[i] == 0);
}
static const char* skip_spaces(const char* p) { while (*p == ' ') ++p; return p; }
static void copy_token(char* out, unsigned cap, const char* text)
{
    unsigned n = 0;
    text = skip_spaces(text);
    while (*text && *text != ' ' && n + 1 < cap) out[n++] = *text++;
    out[n] = 0;
}

/* ------------------------------ work buffers (static BSS) ---------------- */
#define ZP_SRC 16384u
#define ZP_ZCAP 20480u          /* fits deflate's fixed-Huffman + stored worst case */
static unsigned char g_src[ZP_SRC];
static unsigned char g_z[ZP_ZCAP];
static unsigned char g_dec[ZP_SRC];
static char g_msg[256];
static char g_line[128];

/* Read a whole small file into dst via PortFile (256-byte ChannelRequests). */
static int slurp(const char* path, unsigned char* dst, unsigned cap)
{
    int h = file_open(path);
    unsigned total = 0;
    if (h < 0) return -1;
    for (;;)
        {
        int n;
        unsigned want;
        if (total >= cap) break;
        want = cap - total;
        if (want > 256u) want = 256u;
        n = file_read(h, dst + total, want);
        if (n <= 0) break;
        total += (unsigned)n;
        }
    file_close(h);
    return (int)total;
}

static void crc_marker(const char* prefix, unsigned long crc)
{
    unsigned n = str_append(g_msg, 0, prefix);
    n = str_hex(g_msg, n, (unsigned)crc);
    marker(g_msg);
}

/* ------------------------------ commands --------------------------------- */

/* Compress an embedded 1 KiB repetitive buffer, decompress it, and compare
 * byte-for-byte.  Produces an unconditional selftest marker at startup too. */
static const char kSeed[] = "HELLO HELLO HELLO ZLIB ZLIB 0123456789 ";

static void do_selftest(void)
{
    unsigned len = 1024, i, cn = 0, dn = 0;
    unsigned long crc_a, crc_b;

    for (i = 0; i < len; ++i) g_src[i] = (unsigned char)kSeed[i % (sizeof(kSeed) - 1)];
    if (!mz_deflate(g_src, len, g_z, ZP_ZCAP, &cn))
        { marker("ZP: selftest FAIL deflate"); return; }
    if (!mz_inflate(g_z, cn, g_dec, ZP_SRC, &dn))
        { marker("ZP: selftest FAIL inflate"); return; }
    crc_a = mz_crc32(0, g_src, len);
    crc_b = mz_crc32(0, g_dec, dn);
    if (dn != len || crc_a != crc_b)
        { marker("ZP: selftest FAIL compare"); return; }

    {
    unsigned n = str_append(g_msg, 0, "ZP: selftest OK ");
    n = str_uint(g_msg, n, len);
    n = str_append(g_msg, n, " -> ");
    n = str_uint(g_msg, n, cn);
    n = str_append(g_msg, n, " bytes");
    marker(g_msg);
    }
    crc_marker("ZP: crc=0x", crc_a);
}

static void do_deflate(const char* src_arg, const char* dst_arg)
{
    char src[40], dst[40];
    unsigned n = 0, cn = 0;
    copy_token(src, sizeof(src), src_arg);
    copy_token(dst, sizeof(dst), dst_arg);
    if (!*src || !*dst) { marker("ZP: deflate: need src and dst"); return; }
    int count = slurp(src, g_src, ZP_SRC);
    if (count < 0) { marker("ZP: deflate: open src failed"); return; }
    if (!mz_deflate(g_src, (unsigned)count, g_z, ZP_ZCAP, &cn))
        { marker("ZP: deflate: codec failed"); return; }
    if (file_save(dst, g_z, cn) < 0) { marker("ZP: deflate: save failed"); return; }
    n = str_append(g_msg, 0, "ZP: deflate ");
    n = str_uint(g_msg, n, (unsigned)count);
    n = str_append(g_msg, n, " -> ");
    n = str_uint(g_msg, n, cn);
    marker(g_msg);
}

static void do_inflate(const char* src_arg, const char* dst_arg)
{
    char src[40], dst[40];
    unsigned n = 0, dn = 0;
    const unsigned char* payload = g_src;
    unsigned payload_len = 0;
    copy_token(src, sizeof(src), src_arg);
    copy_token(dst, sizeof(dst), dst_arg);
    if (!*src || !*dst) { marker("ZP: inflate: need src and dst"); return; }
    int count = slurp(src, g_src, ZP_SRC);
    if (count < 0) { marker("ZP: inflate: open src failed"); return; }

    /* Accept a raw deflate stream or a complete gzip member. */
    if (count >= 18 && (unsigned char)g_src[0] == 0x1fu &&
        (unsigned char)g_src[1] == 0x8bu && (unsigned char)g_src[2] == 0x08u)
        {
        payload = g_src + 10;
        payload_len = (unsigned)count - 18;
        }
    else
        {
        payload_len = (unsigned)count;
        }

    if (!mz_inflate(payload, payload_len, g_dec, ZP_SRC, &dn))
        { marker("ZP: inflate: codec failed"); return; }
    if (file_save(dst, g_dec, dn) < 0) { marker("ZP: inflate: save failed"); return; }

    n = str_append(g_msg, 0, "ZP: inflate ");
    n = str_uint(g_msg, n, payload_len);
    n = str_append(g_msg, n, " -> ");
    n = str_uint(g_msg, n, dn);
    n = str_append(g_msg, n, " OK");
    marker(g_msg);
    crc_marker("ZP: crc=0x", mz_crc32(0, g_dec, dn));
}

/* Write a genuine RFC1952 gzip member: 10-byte header + raw deflate + CRC32
 * and ISIZE trailer.  The output name is derived from the input (base.GZ). */
static void do_gzip(const char* file_arg)
{
    char name[40], out[40];
    unsigned i = 0, n = 0, cn = 0;
    unsigned long crc;
    static const unsigned char kGzHead[10] = {
        0x1f, 0x8b, 0x08, 0x00, 0, 0, 0, 0, 0x00, 0x03
    };
    unsigned char trailer[8];

    copy_token(name, sizeof(name), file_arg);
    if (!*name) { marker("ZP: gzip: no file"); return; }
    int count = slurp(name, g_src, ZP_SRC);
    if (count < 0) { marker("ZP: gzip: open failed"); return; }
    if (!mz_deflate(g_src, (unsigned)count, g_z, ZP_ZCAP, &cn))
        { marker("ZP: gzip: deflate failed"); return; }

    /* base name (before '.', at most 8 chars, uppercased) + ".GZ". */
    while (name[i] && name[i] != '.' && i < 8) ++i;
    unsigned j = 0;
    for (unsigned k = 0; k < i; ++k)
        {
        char c = name[k];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[j++] = c;
        }
    out[j++] = '.'; out[j++] = 'G'; out[j++] = 'Z'; out[j] = 0;

    crc = mz_crc32(0, g_src, (unsigned)count);
    for (unsigned k = 0; k < 4; ++k) trailer[k] = (unsigned char)(crc >> (8 * k));
    for (unsigned k = 0; k < 4; ++k)
        trailer[4 + k] = (unsigned char)(((unsigned)count >> (8 * k)) & 0xFFu);

    int h = file_create_write(out);
    if (h < 0) { marker("ZP: gzip: create failed"); return; }
    file_append(h, kGzHead, 10);
    file_append(h, g_z, cn);
    file_append(h, trailer, 8);
    file_commit(h);
    file_close(h);

    n = str_append(g_msg, 0, "ZP: gzip ");
    n = str_append(g_msg, n, name);
    n = str_append(g_msg, n, " -> ");
    n = str_append(g_msg, n, out);
    n = str_append(g_msg, n, " (");
    n = str_uint(g_msg, n, (unsigned)count);
    n = str_append(g_msg, n, " -> ");
    n = str_uint(g_msg, n, cn + 18u);
    n = str_append(g_msg, n, " bytes)");
    marker(g_msg);
    crc_marker("ZP: crc=0x", crc);
}

static void do_help(void)
{
    print("ZP: cmds: selftest  deflate <src> <dst>  inflate <src> <dst>"
          "  gzip <file>  help  quit");
}

static int run_command(const char* input)
{
    input = skip_spaces(input);
    const char* word = input;
    while (*word && *word != ' ') ++word;
    unsigned length = (unsigned)(word - input);
    const char* arg = skip_spaces(word);

    if (word_is(input, length, "selftest") || word_is(input, length, "test"))
        do_selftest();
    else if (word_is(input, length, "deflate") || word_is(input, length, "z"))
        {
        const char* second = arg;
        while (*second && *second != ' ') ++second;
        do_deflate(arg, second);
        }
    else if (word_is(input, length, "inflate") || word_is(input, length, "u"))
        {
        const char* second = arg;
        while (*second && *second != ' ') ++second;
        do_inflate(arg, second);
        }
    else if (word_is(input, length, "gzip"))
        do_gzip(arg);
    else if (word_is(input, length, "help") || word_is(input, length, "?"))
        do_help();
    else if (word_is(input, length, "quit") || word_is(input, length, "q"))
        return 1;
    else marker("ZP: unknown command");
    return 0;
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { marker("ZP: console busy"); return 1; }

    unsigned fn = *(unsigned*)(void*)&zip32_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    marker("ZP: MINIZ TOOL READY");
    marker(s_bound ? "ZP: EUSER ord 915 bound"
                   : "ZP: using DebugPrint marker path");
    do_selftest();

    for (;;)
        {
        print("ZP> ");
        r = read_line(g_line, sizeof(g_line), 60000);
        if (r < 0) break;
        if (run_command(g_line)) break;
        }

    marker("ZP: DONE");
    con_release();
    return 0;
}
