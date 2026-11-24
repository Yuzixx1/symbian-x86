/*
 * hello.cpp -- harness for the genuine-source proof.
 *
 * It is deliberately tiny. The interesting code (EmptyFunction and
 * __cxa_pure_virtual) is compiled from the UNMODIFIED genuine EUSER source
 * kernelhwsrv/kernel/eka/euser/epoc/x86/uc_utl.cpp; this file only
 *   - supplies the Panic() stub that source needs,
 *   - calls the genuine EmptyFunction(),
 *   - prints markers through the PE2 IAT slot seeded with EUSER.DLL ord 915
 *     (RDebug::Printf), the import mechanism the port already uses.
 */
#include <e32std.h>
#include <e32panic.h>

/* PE2 IAT slot at 0x400000+0x8000, defined by ld --defsym in genuine_build.py.
   Before load the slot holds 915; after EUSER.DLL binding it holds the
   resolved RDebug::Printf address. */
extern void (*genuine_printf_slot)(const char*);

/* Genuine uc_utl.cpp needs this; it is never reached in this proof. */
void Panic(TCdtPanic aPanic);
void Panic(TCdtPanic) {}

/* Genuine EUSER export from uc_utl.cpp. */
void EmptyFunction();

static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&genuine_printf_slot;
    if (fn && fn != 915u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
}

extern "C" int app_main(void)
{
    TInt genuine_int = KMaxTInt;            /* genuine e32std.h type/constant */
    (void)genuine_int;
    EmptyFunction();                        /* genuine compiled code */
    marker("GENUINE: e32std.h compiled with gcc -m32 -ffreestanding");
    marker("GENUINE: EUSER uc_utl.cpp compiled + called");
    marker("GENUINE HELLO: printf via EUSER.DLL ordinal 915");
    return 0;
}
