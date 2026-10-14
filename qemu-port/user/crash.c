#include "runtime.h"

/* Fault-attribution fixture: deliberately fault so the reaper's exit line
   names the program. Uses UD2 (invalid opcode), the same convention the
   early ring-3 demos used to exit before RThread::Exit existed. */
int app_main(void)
{
    print("CRASH: about to fault");
    __asm__ __volatile__("ud2");
    print("CRASH: survived (should be unreachable)");
    return 0;
}
