#include "doomgeneric.h"

extern uint64_t sleep(uint64_t msec);
extern uint64_t getticks();
extern void _render(void* src, size_t size);

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);

    while (1)
    {
	doomgeneric_Tick();
    }

    return 0;
}

void DG_Init(){

}

void DG_DrawFrame() {
  //  DG_ScreenBuffer;
  // DOOMGENERIC_RESX, DOOMGENERIC_RESY, 4
  // write screenbuffer to framebuffer
  _render(DG_ScreenBuffer, DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
}

void DG_SleepMs(uint32_t sleepMs){
  sleep((uint64_t)sleepMs);
}

uint32_t DG_GetTicksMs(){
  return getticks();
}

int DG_GetKey(int* pressed, unsigned char*){
}

void DG_SetWindowTitle(const char* title){

}
