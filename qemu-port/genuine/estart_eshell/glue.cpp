/* glue.cpp -- port startup glue for the genuine ESTART / ESHELL E32 images.
 *
 * user/start.S calls app_main(); the genuine sources export E32Main().  This
 * file bridges the two, runs the genuine static constructors, and prints the
 * component banner through RDebug::Printf -- which the builder imports from
 * the staged genuine EUSER.DLL as PE2 ordinal 915 (bx86gcc/euseru.def).
 *
 * Build with -DAPP_TAG="estart_" or -DAPP_TAG="etshell_".
 */
#include <e32std.h>
#include <e32debug.h>
#include <u32std.h>

extern "C" int app_main(void);
extern "C" void process_exit(int) __attribute__((noreturn));

GLREF_C TInt E32Main();   // genuine estartmain.cpp / ts_main.cpp entry point

extern "C" void (*__init_array_start)(void);
extern "C" void (*__init_array_end)(void);

#ifndef APP_TAG
#define APP_TAG "genuine"
#endif

/* Our E32Run (variant/e32load.cpp) calls Kern::ProcessCreate directly, which
 * does NOT create the user thread heap (genuine RProcess::Create does that via
 * UserHeap::SetupThreadHeap, euser/epoc/up_utl.cpp).  Without it Exec::Heap()
 * returns 0 and the first User::Alloc/new derefs NULL.  So the glue creates
 * the thread heap through the genuine EUSER export (ord 1360 in
 * bx86gcc/euseru.def) before entering the genuine E32Main. */
extern "C" TInt euser_setup_thread_heap(TInt, SStdEpocThreadCreateInfo&)
	__asm__("_ZN8UserHeap15SetupThreadHeapEiR24SStdEpocThreadCreateInfo");

static void ensure_thread_heap()
	{
	static SStdEpocThreadCreateInfo info;   /* zero-initialised static */
	info.iAllocator = 0;
	info.iHeapInitialSize = 0x1000;
	info.iHeapMaxSize = 0x100000;
	TInt r = euser_setup_thread_heap(0, info);
	RDebug::Printf(APP_TAG ": thread heap via EUSER SetupThreadHeap r=%d", (int)r);
	}

extern "C" int app_main(void)
	{
	for (void (**f)(void) = &__init_array_start; f != &__init_array_end; ++f)
		(*f)();
	RDebug::Printf(APP_TAG ": genuine E32 image loaded, E32Main starting (EUSER ord 915)");
	ensure_thread_heap();
	TInt r = E32Main();
	RDebug::Printf(APP_TAG ": E32Main returned %d", (int)r);
	process_exit(r);
	return (int)r;
	}
