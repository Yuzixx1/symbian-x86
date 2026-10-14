#include "runtime.h"

/* Leave both handles open: the genuine thread handle table must close
   them when app_main returns. Also check independent read positions. */
int app_main(void)
{
    int first = file_open("C:\\HELLO.TXT");
    int second = file_open("C:\\HELLO.TXT");
    char a[8], b[8];
    if (first < 0 || second < 0 || first == second)
        { print("FILECHECK: open failed"); return 1; }
    if (file_read(first, a, sizeof(a)) != 8 ||
        file_read(second, b, sizeof(b)) != 8)
        { print("FILECHECK: read failed"); return 2; }
    for (unsigned i = 0; i < sizeof(a); ++i)
        if (a[i] != b[i])
            { print("FILECHECK: shared position"); return 3; }
    print("FILECHECK: independent handles; exiting without close");
    return 0;
}
