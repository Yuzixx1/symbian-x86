/* dbg2_main.cpp -- fine-grained E32Main for locating the etshell_ #PF: calls
 * the EUSER exports that CTrapCleanup::New() uses, one at a time, with an
 * ord-915 marker after each.  Debug component only. */
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>

static unsigned fast_exec(unsigned n)
	{
	unsigned r;
	__asm__ volatile("int $0x20" : "=a"(r) : "a"(n) : "memory");
	return r;
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("DBG2: start");
	unsigned h = fast_exec(1);   /* EFastExecHeap = 1 */
	RDebug::Printf("DBG2: fast exec Heap(1) -> %x", h);
	void* p = User::Alloc(64);
	RDebug::Printf("DBG2: User::Alloc -> %d", p != 0);
	if (p) User::Free(p);
	RDebug::Printf("DBG2: User::Free ok");
	TTrapHandler* prev = User::SetTrapHandler(0);
	RDebug::Printf("DBG2: SetTrapHandler(0) -> %d", prev != 0);
	prev = User::SetTrapHandler(prev);
	RDebug::Printf("DBG2: SetTrapHandler(prev) -> %d", prev != 0);
	CTrapCleanup* t = CTrapCleanup::New();
	RDebug::Printf("DBG2: CTrapCleanup::New -> %d", t != 0);
	if (t) delete t;
	RDebug::Printf("DBG2: done");
	return 0;
	}
