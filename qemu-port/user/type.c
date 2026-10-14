#include "runtime.h"

int app_main(void)
{
    U16 command[128];
    char path[128], data[256];
    int length = command_line(command, 128);
    if (length <= 0) { print("TYPE: use an absolute path, e.g. C:\\HELLO.TXT"); return 1; }
    int first = command[0] == '"' ? 1 : 0;
    if (first && command[length - 1] == '"') --length;
    int n = 0;
    for (int i = first; i < length; ++i) path[n++] = (char)command[i];
    path[n] = 0;
    int handle = file_open(path);
    if (handle < 0) { print("TYPE: open failed"); return 1; }
    int count;
    while ((count = file_read(handle, data, sizeof(data))) > 0)
        print_n(data, count);
    file_close(handle);
    print(count < 0 ? "TYPE: read failed" : "TYPE: EOF");
    return count < 0;
}
