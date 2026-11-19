#include "runtime.h"

/* Chunk fixture: private chunk write/read/adjust, then a global chunk
   parked alive so a second program can open it. */
int app_main(void)
{
    int priv = chunk_create(0, 1024, 4096);
    if (priv < 0) { print("CHUNK: private create failed"); return 1; }
    void* p = chunk_base(priv);
    if (!p) { print("CHUNK: base failed"); return 1; }
    if (chunk_size(priv) != 1024) { print("CHUNK: size mismatch"); return 1; }
    *(unsigned*)p = 0x11111111u;
    if (*(unsigned*)p != 0x11111111u) { print("CHUNK: readback failed"); return 1; }
    if (chunk_adjust(priv, 2048) || chunk_size(priv) != 2048)
        { print("CHUNK: adjust failed"); return 1; }
    print("CHUNK: private ok");
    int g = chunk_create("PortChunkCheck", 4096, 4096);
    if (g < 0) { print("CHUNK: global create failed"); return 1; }
    void* gp = chunk_base(g);
    if (!gp) { print("CHUNK: global base failed"); return 1; }
    *(unsigned*)gp = 0x12345678u;
    print("CHUNK: global ready");
    thread_park();  /* hold the global chunk alive for the opener */
    return 0;
}
