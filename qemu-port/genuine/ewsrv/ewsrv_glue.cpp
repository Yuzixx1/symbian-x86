// Port startup glue for the genuine EWSRV E32.  app_main() is called by the
// port's user/start.S after the loader has bound all EUSER PE2 imports.
#include <e32std.h>
#include <e32debug.h>

extern "C" int app_main(void);
extern "C" void process_exit(int) __attribute__((noreturn));

GLREF_C TInt E32Main();   // genuine ws_main.cpp entry point
// Genuine static constructors (CEvent) are folded into .data by link_ewsrv.ld.
extern "C" void (*__init_array_start)(void);
extern "C" void (*__init_array_end)(void);
void EwsrvInstallHeap();

extern "C" int app_main(void)
    {
    EwsrvInstallHeap();
    for (void (**f)(void) = &__init_array_start; f != &__init_array_end; ++f)
        (*f)();
    RDebug::Printf("EWSRV: genuine window server starting\r\n");
    TInt r = E32Main();
    RDebug::Printf("EWSRV: E32Main returned %d\r\n", (int)r);
    process_exit(r);
    return (int)r;
    }
