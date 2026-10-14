#include "runtime.h"

static U16 command[128];
static char arg[128], cwd[128], data[64];

static int first_arg(int length)
{
    int p = 0;
    while (p < length && (command[p] == ' ' || command[p] == '\t')) ++p;
    int quoted = p < length && command[p] == '"';
    if (quoted) ++p;
    int n = 0;
    while (p < length)
        {
        U16 c = command[p];
        if (quoted ? c == '"' : c == ' ' || c == '\t') break;
        if (c == '"' || c > 127 || n == 127) return -1;
        arg[n++] = (char)c;
        ++p;
        }
    if (quoted)
        {
        if (p == length) return -1;
        ++p;
        }
    arg[n] = 0;
    while (p < length && (command[p] == ' ' || command[p] == '\t')) ++p;
    if (p != length) return -1;
    return n;
}

int app_main(void)
{
    int length = command_line(command, 128);
    if (length < 0) { print("CWD: command line failed"); return 1; }
    int r = get_cwd(cwd, sizeof(cwd));
    if (r < 0) { print("CWD: getcwd failed"); return 1; }
    cwd[r] = 0;
    print("CWD:");
    print(cwd);
    if (length > 0)
        {
        if (first_arg(length) < 0) { print("CWD: bad argument"); return 1; }
        r = change_dir(arg);
        if (r < 0) { print("CWD: chdir failed"); return 1; }
        r = get_cwd(cwd, sizeof(cwd));
        if (r < 0) { print("CWD: getcwd after failed"); return 1; }
        cwd[r] = 0;
        print("CWD-NEW:");
        print(cwd);
        }
    /* Relative open proves the (possibly changed) CWD resolves. */
    int handle = file_open("NOTE.TXT");
    if (handle < 0) { print("CWD: relative open failed"); return 1; }
    int count = file_read(handle, data, sizeof(data));
    file_close(handle);
    if (count <= 0) { print("CWD: relative read failed"); return 1; }
    print("CWD: relative open ok");
    return 0;
}
