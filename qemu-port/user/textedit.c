/*
 * textedit.c -- a small line-oriented text editor for the Sybian REAL port.
 *
 * The delivered E32 LINKS AGAINST EUSER.DLL through one genuine PE2 import:
 * ordinal 915 = RDebug::Printf(char const*, ...)
 * (kernelhwsrv/kernel/eka/bx86gcc/euseru.def).  ld defines the IAT slot at
 * codeBase+0x8000; elf2e32.py --euser-import 915 emits the PE2 import section
 * and the loader overwrites the slot with the resolved export address before
 * the process resumes.  Serial markers go through that ordinal (with a
 * DebugPrint fallback so the trace is always visible).
 *
 * File open/save uses PortFile IPC (runtime.c file_open/file_read/file_save),
 * i.e. the portfilesrv service, exactly like TYPE and FILEVIEW.
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

#define CAP 4096
static char buffer[CAP];
static unsigned length = 0;
static unsigned cursor = 0;
static char line[128];

static void banner(void)
{
    unsigned fn = *(unsigned*)(void*)&euser_printf_slot;
    if (fn != 915u && fn != 0u) s_bound = 1;
    print("TEXTEDIT: ready (h=help, q=quit)");
    marker(s_bound ? "TEXTEDIT: EUSER ord 915 bound"
                   : "TEXTEDIT: using DebugPrint marker path");
}

static void status(void)
{
    char text[64];
    unsigned n = 0, v;
    char digits[12];
    int d;
    const char* tag = "TEXTEDIT: len=";
    while (*tag) text[n++] = *tag++;
    v = length; d = 0;
    if (!v) digits[d++] = '0';
    while (v) { digits[d++] = (char)('0' + v % 10); v /= 10; }
    while (d) text[n++] = digits[--d];
    tag = " cur=";
    while (*tag) text[n++] = *tag++;
    v = cursor; d = 0;
    if (!v) digits[d++] = '0';
    while (v) { digits[d++] = (char)('0' + v % 10); v /= 10; }
    while (d) text[n++] = digits[--d];
    print_n(text, n);
}

static void show_buffer(void)
{
    if (!length) { print("TEXTEDIT: (empty)"); return; }
    print("TEXTEDIT: --- buffer ---");
    print_n(buffer, length);
    print("TEXTEDIT: --- end ---");
}

static void insert(const char* text, unsigned at_end)
{
    unsigned n = 0;
    while (text[n] && n < sizeof(line)) ++n;
    if (at_end) cursor = length;
    if (length + n >= CAP) { marker("TEXTEDIT: buffer full"); return; }
    unsigned i;
    for (i = length; i > cursor; --i) buffer[i] = buffer[i - 1];
    for (i = 0; i < n; ++i) buffer[cursor + i] = text[i];
    cursor += n;
    length += n;
    buffer[length] = 0;
    status();
}

static void backspace(void)
{
    if (!cursor) { marker("TEXTEDIT: at start"); return; }
    for (unsigned i = cursor - 1; i + 1 < length; ++i) buffer[i] = buffer[i + 1];
    --cursor; --length;
    buffer[length] = 0;
    status();
}

static void delete_at(void)
{
    if (cursor >= length) { marker("TEXTEDIT: at end"); return; }
    for (unsigned i = cursor; i + 1 < length; ++i) buffer[i] = buffer[i + 1];
    --length;
    buffer[length] = 0;
    status();
}

static void open_file(const char* path)
{
    int handle = file_open(path);
    if (handle < 0) { marker("TEXTEDIT: open failed"); return; }
    int count = 0;
    length = 0;
    while (length < CAP - 1 &&
           (count = file_read(handle, buffer + length, CAP - 1 - length)) > 0)
        length += (unsigned)count;
    file_close(handle);
    buffer[length] = 0;
    cursor = 0;
    marker(count < 0 ? "TEXTEDIT: read error" : "TEXTEDIT: opened");
    status();
}

static void save_file(const char* path)
{
    int r = file_save(path, buffer, length);
    marker(r < 0 ? "TEXTEDIT: save failed" : "TEXTEDIT: saved");
    status();
}

/* Parse "X argument" from the line read from the console. */
static void run_command(const char* input)
{
    char op = input[0];
    const char* arg = input + 1;
    while (*arg == ' ') ++arg;
    switch (op)
        {
        case 'h': case '?':
            print("TEXTEDIT: h=help p=print w=status a<sp>text=append"
                  " i<sp>text=insert <=left >=right H=home E=end"
                  " x=delete b=backspace c=clear o<sp>path=open"
                  " s<sp>path=save q=quit");
            break;
        case 'p': show_buffer(); break;
        case 'w': status(); break;
        case 'a': insert(arg, 1); break;
        case 'i': insert(arg, 0); break;
        case '<': if (cursor) --cursor; status(); break;
        case '>': if (cursor < length) ++cursor; status(); break;
        case 'H': cursor = 0; status(); break;
        case 'E': cursor = length; status(); break;
        case 'x': delete_at(); break;
        case 'b': backspace(); break;
        case 'c': length = cursor = 0; buffer[0] = 0;
                  marker("TEXTEDIT: cleared"); break;
        case 'o': open_file(arg); break;
        case 's': save_file(arg); break;
        default:  marker("TEXTEDIT: unknown command"); break;
        }
}

int app_main(void)
{
    int r = con_acquire();
    if (r < 0) { print("TEXTEDIT: console busy"); return 1; }
    banner();
    for (;;)
        {
        print("TEXTEDIT> ");
        r = read_line(line, sizeof(line), 60000);
        if (r < 0) { marker("TEXTEDIT: input error"); break; }
        if (line[0] == 'q') { marker("TEXTEDIT: goodbye"); break; }
        run_command(line);
        }
    con_release();
    return 0;
}
