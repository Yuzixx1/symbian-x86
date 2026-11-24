/* dbg3_main.cpp -- isolate the CConsoleBase shim: create a console via the
 * shim's Console::NewL and exercise Printf/Write directly. */
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <e32cons.h>

extern "C" int exec2(unsigned, unsigned, unsigned);

static void do_fmtlist(TDes& aBuf, TRefByValue<const TDesC> aFmt, ...)
	{
	VA_LIST list;
	VA_START(list, aFmt);
	aBuf.AppendFormatList(aFmt, list);
	VA_END(list);
	}

GLDEF_C TInt E32Main()
	{
	_LIT(KDes, "WORLD");
	CConsoleBase* con = Console::NewL(_L("DBG3"), TSize(80, 25));
	RDebug::Printf("DBG3: console -> %d", con != 0);
	con->Printf(_L("HELLO %S %d END"), &KDes, 42);
	con->Printf(_L("PLAIN"));
	con->Write(_L("RAWWRITE\n"));
	/* direct exec 86 DebugPrint, print_n style (proven path) */
	{
	const char* msg = "EXEC86-OK-LONG-LINE\n";
	unsigned len = 0; while (msg[len]) ++len;
	struct { unsigned tl; const void* p; } d = { 0x10000000u | len, msg };
	exec2(86, (unsigned)&d, 0);
	}
	{
	char sb[32];
	const char* m = "STACKBUF-OK";
	unsigned i = 0; while (m[i]) { sb[i] = m[i]; ++i; } sb[i] = 0;
	RDebug::Printf(sb);          /* ord 915 with a stack buffer, no % */
	}
	{
	TPtrC16 f = _L("HELLO %S %d END");
	RDebug::Printf("DBG3: lit len=%d", (int)f.Length());
	TPtrC16 p((const TUint16*)L"PTRCTOR");
	RDebug::Printf("DBG3: ptrc len=%d", (int)p.Length());
	}
	{
	TPtrC16 a((const TUint16*)L"MID");
	TBuf<0x200> b;
	do_fmtlist(b, _L("A%SB"), &a);
	RDebug::Printf("DBG3: fmtlist len=%d", (int)b.Length());
	con->Write(b);
	}
	{
	_LIT(KH, "HELP");
	TPtrC16 cmd((const TUint16*)L"help");
	RDebug::Printf("DBG3: cmp=%d len=%d", (int)cmd.CompareF(KH), (int)cmd.Length());
	TPtrC16 arg((const TUint16*)L"ARG");
	TBuf<0x100> b;
	b.AppendFormat(_L("X%SY %d\n"), &arg, 42);
	RDebug::Printf("DBG3: fmt len=%d", (int)b.Length());
	con->Write(b);
	}
	for (int k = 0; k < 8; ++k)
		{
		TKeyCode key = con->Getch();
		RDebug::Printf("DBG3: key=%x", (unsigned)key);
		if (key == 0x0d) break;
		}
	RDebug::Printf("DBG3: done");
	return 0;
	}
