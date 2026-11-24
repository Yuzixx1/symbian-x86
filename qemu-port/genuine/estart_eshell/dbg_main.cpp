/* dbg_main.cpp -- instrumented E32Main for the etshell_ fault hunt: same call
 * sequence as the genuine ts_main.cpp but with an RDebug::Printf (EUSER ord
 * 915) marker between each step, so the serial log shows exactly which step
 * faults.  Used only by the "etshell_dbg" component; not part of the shipped
 * estart_/etshell_ images. */
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include "ts_std.h"

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("DBG: E32Main step0");
	CTrapCleanup* trapHandler = CTrapCleanup::New();
	RDebug::Printf("DBG: CTrapCleanup::New -> %d", trapHandler != 0);
	if (trapHandler == NULL)
		return KErrNoMemory;
	TInt r = KErrNone;
	TRAP(r, ShellFunction::TheShell = CShell::NewL());
	RDebug::Printf("DBG: CShell::NewL r=%d", (int)r);
	if (r != KErrNone)
		{ delete trapHandler; return r; }
	{
	_LIT(KF, "%- 10S%S\n");
	TPtrC16 n((const TUint16*)L"ATTRIB");
	TPtrC16 h((const TUint16*)L"Displays file attributes");
	CShell::OutputStringToConsole(ETrue, KF, &n, &h);
	RDebug::Printf("DBG: osc done");
	}
	TRAP(r, ShellFunction::TheShell->RunL());
	RDebug::Printf("DBG: RunL r=%d", (int)r);
	if (r != KErrNone)
		{ delete ShellFunction::TheShell; delete trapHandler; return r; }
	delete ShellFunction::TheShell;
	delete trapHandler;
	return KErrNone;
	}
