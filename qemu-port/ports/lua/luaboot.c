/* luaboot.c -- tiny PE2 E32 launcher for the full LUA.E32 image.
 *
 * The interactive shell loads programs into a 64 KiB stack buffer, which is
 * too small for a real Lua 5.4 VM (its text alone is >110 KiB).  The R1
 * service manager installs exec 268, whose launcher reads images into a
 * 256 KiB kernel buffer.  This stub calls that genuine exec with a TPtrC8
 * path descriptor, so the unmodified full Lua interpreter can be started
 * from the serial shell with `luaboot`.
 */
#include "runtime.h"

struct PtrC8 { unsigned type_length; const void *pointer; };

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

int app_main(void)
{
    static const char path[] = "\\LUA.E32";
    struct PtrC8 d;
    int pid;

    d.type_length = 0x10000000u | (sizeof(path) - 1);
    d.pointer = path;
    pid = exec2(268u, (unsigned)&d, 0u);
    if (pid > 0)
        {
        print("LUABOOT: launched LUA.E32");
        return 0;
        }
    print("LUABOOT: ERROR launch failed");
    return 1;
}
