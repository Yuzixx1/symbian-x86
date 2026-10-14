#include "runtime.h"

/* Print a file: `exec FILEVIEW.E32 NOTES.TXT`. Relative paths resolve
   against the process working directory (inherited from the shell). */
static U16 command[128];
static char path[128], chunk[256];

int app_main(void)
{
    int length = command_line(command, 128);
    if (length <= 0) { print("FILEVIEW: give a path"); return 1; }
    int first = command[0] == '"' ? 1 : 0;
    if (first && command[length - 1] == '"') --length;
    int n = 0;
    for (int i = first; i < length && n < 127; ++i)
        path[n++] = (char)command[i];
    path[n] = 0;
    int handle = file_open(path);
    if (handle < 0) { print("FILEVIEW: open failed"); return 1; }
    int count;
    while ((count = file_read(handle, chunk, sizeof(chunk))) > 0)
        print_n(chunk, count);
    file_close(handle);
    if (count < 0) { print("FILEVIEW: read failed"); return 1; }
    print("FILEVIEW: EOF");
    return 0;
}
