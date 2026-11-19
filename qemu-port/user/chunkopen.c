#include "runtime.h"

/* Opens the global chunk left by CHUNK.E32 and verifies the shared value. */
int app_main(void)
{
    int g = chunk_open("PortChunkCheck");
    if (g < 0) { print("CHUNKOPEN: open failed"); return 1; }
    void* gp = chunk_base(g);
    if (!gp) { print("CHUNKOPEN: base failed"); return 1; }
    unsigned v = *(unsigned*)gp;
    if (v != 0x12345678u) { print("CHUNKOPEN: value mismatch"); return 1; }
    print("CHUNKOPEN: shared ok");
    return 0;
}
