/*
 * zmachine32.c -- Z-machine interpreter front end for the Sybian REAL port.
 *
 * Loads a version 3 Infocom story from FAT (ZORK.Z3, or the bundled Mini-Zork
 * demo MINIZORK.Z3) through PortFile IPC, then runs ports/zmachine/zmachine.c
 * with serial I/O.  All output is tagged "ZM: " and crosses the real PE2
 * import of EUSER.DLL ordinal 915 (RDebug::Printf), with a DebugPrint
 * fallback.  Game commands arrive through runtime.h read_line() while this
 * process holds the PortCon input lease.
 *
 * The story bytes are read straight into the interpreter's static 128 KiB
 * image; this program performs no malloc() calls at all.
 */
#include "runtime.h"
#include "zmachine.h"

/* PE2 IAT slot at 0x408000, bound by elf2e32.py --euser-import 915. */
extern void (*zm_printf_slot)(const char*);

static char out_line[256];
static unsigned out_len;
static int bound915;

/* Emit a plain (format-free) "ZM: ..." marker line.  RDebug::Printf is
 * variadic, so pass the text through "%s" rather than as a format string:
 * game output may legally contain '%'. */
static void marker(const char* text)
{
    char msg[280];
    unsigned n = 0;
    const char* p = "ZM: ";
    while (*p && n + 1 < sizeof(msg)) msg[n++] = *p++;
    p = text;
    while (*p && n + 1 < sizeof(msg)) msg[n++] = *p++;
    msg[n] = 0;
        {
        unsigned fn = *(unsigned*)(void*)&zm_printf_slot;
        if (fn != 915u && fn != 0u)
            ((void (*)(const char*, const char*))fn)("%s", msg);
        }
    print(msg);                                  /* DebugPrint fallback */
}

static void flush_line(void)
{
    if (!out_len) return;
    out_line[out_len] = 0;
    marker(out_line);
    out_len = 0;
}

/* One ZSCII output code from the interpreter.  Newlines end a marker line;
 * otherwise printable ASCII is accumulated (ZSCII 13/10 are line breaks). */
static void out_char(unsigned zscii)
{
    if (zscii == 13 || zscii == 10)
        {
        flush_line();
        return;
        }
    if (zscii == 9) zscii = ' ';
    if (zscii >= 32 && zscii <= 126)
        {
        if (out_len + 1 >= sizeof(out_line)) flush_line();
        out_line[out_len++] = (char)zscii;
        }
}

static int read_command(char* buffer, unsigned capacity)
{
    return read_line(buffer, capacity, 60000);
}

/* Read a whole story file into the interpreter's static image.  file_read
 * is limited to 256 bytes per PortFile request, so loop in 256-byte chunks. */
static int load_story(const char* path, unsigned char* dst, unsigned cap)
{
    int handle = file_open(path);
    unsigned total = 0;
    if (handle < 0) return -1;
    for (;;)
        {
        unsigned room = cap - total;
        int n;
        if (!room) break;
        n = file_read(handle, dst + total, room < 256u ? room : 256u);
        if (n <= 0) break;
        total += (unsigned)n;
        }
    file_close(handle);
    return total > 64 ? (int)total : -1;
}

int app_main(void)
{
    unsigned char* story;
    unsigned cap;
    const char* name = 0;
    int length = -1;
    zm_io io;
    int rc;

    if (con_acquire() < 0)
        {
        marker("console busy");
        return 1;
        }

        {
        unsigned fn = *(unsigned*)(void*)&zm_printf_slot;
        if (fn != 915u && fn != 0u) bound915 = 1;
        }

    marker("Z-MACHINE READY");
    marker(bound915 ? "EUSER ord 915 bound"
                    : "using DebugPrint marker path");

    story = zm_game_buffer();
    cap = zm_game_capacity();

    /* A real Zork I .z3 can be dropped on FAT as ZORK.Z3; the bundled
     * freely-distributable Infocom demo is used when it is absent. */
    length = load_story("ZORK.Z3", story, cap);
    if (length > 0) name = "ZORK.Z3";
    else
        {
        length = load_story("MINIZORK.Z3", story, cap);
        if (length > 0) name = "MINIZORK.Z3";
        }
    if (length < 0)
        {
        marker("no story file (need ZORK.Z3 or MINIZORK.Z3)");
        marker("DONE");
        con_release();
        return 1;
        }

    io.out_char = out_char;
    io.read_line = read_command;
    io.flush = flush_line;

    if (zm_init(&io) != 0)
        {
        marker("unsupported or invalid story version");
        marker("DONE");
        con_release();
        return 1;
        }
        {
        char msg[64];
        unsigned n = 0;
        const char* p = "game ";
        while (*p) msg[n++] = *p++;
        p = name;
        while (*p) msg[n++] = *p++;
        p = " version=";
        while (*p) msg[n++] = *p++;
        msg[n++] = (char)('0' + zm_game_version() % 10);
        msg[n] = 0;
        marker(msg);
        }

    rc = zm_run();
    flush_line();
    marker(rc < 0 ? "halted rc=-1" : "halted rc=0");
    marker("DONE");
    con_release();
    return 0;
}
