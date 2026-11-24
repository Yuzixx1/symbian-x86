// Minimal genuine-RConsole client for the genuine EWSRV E32.  It uses the
// real RWsSession-compatible RConsole API from econs (co_cli.cpp) and draws a
// titled text window on the framebuffer owned by EWSRV.
#include <e32std.h>
#include <e32debug.h>
#include <e32cons.h>
#include <e32twin.h>

extern "C" int app_main(void);
extern "C" void process_exit(int) __attribute__((noreturn));
void EwsrvInstallHeap();

extern "C" int app_main(void)
    {
    EwsrvInstallHeap();
    RDebug::Printf("EWCLIENT: app_main started\r\n");
    RConsole console;
    TInt cr = console.Create();
    RDebug::Printf("EWCLIENT: Create r=%d\r\n", (int)cr);
    TSize scr(0,0);
    TInt sr = console.ScreenSize(scr);
    RDebug::Printf("EWCLIENT: ScreenSize r=%d %dx%d\r\n", (int)sr, (int)scr.iWidth, (int)scr.iHeight);
    TInt r = console.Init(_L("GENUINE EWSRV WINDOW"), TSize(44, 12));
    RDebug::Printf("EWCLIENT: Init r=%d\r\n", (int)r);
    if (r != KErrNone) { process_exit(r); }
    r = console.SetTitle(_L("GENUINE EWSRV WINDOW"));
    RDebug::Printf("EWCLIENT: SetTitle r=%d\r\n", (int)r);
    r = console.Write(_L("HELLO FROM GENUINE EWSRV"));
    RDebug::Printf("EWCLIENT: Write r=%d\r\n", (int)r);
    r = console.SetCursorPosAbs(TPoint(2, 3));
    RDebug::Printf("EWCLIENT: SetCursorPos r=%d\r\n", (int)r);
    r = console.Write(_L("E32 WINDOW SERVER OK"));
    RDebug::Printf("EWCLIENT: Write2 r=%d\r\n", (int)r);
    RDebug::Printf("EWCLIENT: window drawn\r\n");
    for (volatile int i = 0; i < 2000000; ++i) { }   // let the server composite
    process_exit(0);
    }
