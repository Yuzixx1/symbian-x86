/* shims.cpp -- port-layer shims for the genuine ESTART / ESHELL E32 builds.
 *
 * kernelhwsrv/ is READ-ONLY: the genuine estart.cpp / ts_*.cpp are compiled
 * unmodified.  This file supplies, as LOCAL definitions (so the builder never
 * emits an EUSER thunk for them), the small client sides that genuine Symbian
 * would import from EFSRV.DLL / HAL.DLL / DOMAINCLI.DLL / the console
 * implementation DLL, plus the C++ runtime allocation symbols.  Every EUSER
 * import that CAN work against our kernel (User::*, RDebug::Printf ord 915,
 * RProcess, RProperty, ...) is left undefined here and bound to the real
 * staged EUSER.DLL by the pack_multi PE2 import table.
 *
 * Each stub below documents the genuine symbol it replaces.
 */
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <e32cons.h>
#include <e32keys.h>
#include <f32file.h>
#include <hal.h>
#include <domainmanager.h>
#include <stdarg.h>

extern "C" int exec2(unsigned, unsigned, unsigned);
extern "C" int exec4(unsigned, unsigned, unsigned, unsigned, unsigned);
extern "C" void sleep_ms(int);   /* user/runtime.c: timer-based, wakes reliably */
extern "C" void process_exit(int) __attribute__((noreturn));

/* ------------------------------------------------------------------ C++ rt */
/* Replaces the compsupp operator new/delete, which are not EUSER exports in
 * this build; route them through the genuine EUSER User::Alloc/User::Free
 * (thunked imports, ord 646/644 in bx86gcc/euseru.def). */
extern "C" void* euser_alloc(int) __asm__("_ZN4User5AllocEi");
extern "C" void  euser_free(void*) __asm__("_ZN4User4FreeEPv");
void* operator new(unsigned int aSize)             { return euser_alloc((int)aSize); }
void* operator new[](unsigned int aSize)           { return euser_alloc((int)aSize); }
void  operator delete(void* aPtr)                  { euser_free(aPtr); }
void  operator delete(void* aPtr, unsigned int)    { euser_free(aPtr); }
void  operator delete[](void* aPtr)                { euser_free(aPtr); }
void  operator delete[](void* aPtr, unsigned int)  { euser_free(aPtr); }
extern "C" void __cxa_pure_virtual()               { process_exit(KErrGeneral); }
/* static destructors: the glue runs .init_array manually; ignore atexit */
extern "C" void* __dso_handle = &__dso_handle;
extern "C" int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }

/* ------------------------------------------------------- PortCon console */
/* Replaces the console implementation DLL that genuine Console::NewL loads
 * ("econs*"): we present a CConsoleBase whose output goes through the REAL
 * genuine EUSER RDebug::RawPrint (PE2 ord 2184 -> DebugPrint exec, raw, no
 * added newline) and whose input comes from the port's PortCon lease
 * (exec 10 fn 0/1), the same mechanism the port's INTERACT.E32 fixture uses. */
struct PtrC8Shim { unsigned type_length; const void* pointer; };

static int  g_con = -1;      /* PortCon channel handle, opened lazily */
static int  g_conX = 0, g_conY = 0;
static bool g_leased = false;

static void con_open()
	{
	if (g_con >= 0) return;
	PtrC8Shim name = { 0x10000007u, "PortCon" };
	struct { unsigned version; int unit; void* physical; void* info; } create = { 1, -1, 0, 0 };
	g_con = exec4(132, (unsigned)&name, (unsigned)&create, 1, 0);
	}

static void con_raw(int c)   /* PortCon raw echo: exactly one char, no newline */
	{
	con_open();
	if (g_con < 0) return;
	if (c == '\n') exec4(10, (unsigned)g_con, 3, '\r', 0);
	exec4(10, (unsigned)g_con, 3, (unsigned)c, 0);
	}

static int con_tryread()
	{
	con_open();
	if (g_con < 0) return KErrNotFound;
	return exec4(10, (unsigned)g_con, 0, 0, 0);
	}

static void con_acquire_once()
	{
	con_open();
	if (g_con < 0 || g_leased) return;
	exec4(10, (unsigned)g_con, 1, 0, 0);   /* acquire the input lease */
	g_leased = true;
	}

static void port_write(const TDesC& aDes)   /* line-edit echo: raw PortCon chars */
	{
	for (TInt i = 0; i < aDes.Length(); ++i)
		{
		TUint c = aDes[i];
		con_raw(c < 256 ? (int)c : '?');
		}
	}

/* narrow a TDesC16 to ASCII into a C buffer */
static char* narrow(const TDesC& aDes, char* aBuf, int aCap)
	{
	TInt n = aDes.Length() < aCap - 1 ? aDes.Length() : aCap - 1;
	for (TInt i = 0; i < n; ++i)
		{
		TUint c = aDes[i];
		aBuf[i] = c < 128 ? (char)c : '?';
		}
	aBuf[n] = 0;
	return aBuf;
	}

/* minimal Symbian-style formatter for CConsoleBase::Printf: handles the
 * conversions eshell/estart actually use (%% %c %d %i %u %x %X %s %S %p) */
static void port_printf(const TDesC& aFmt, va_list ap)
	{
	char buf[512];
	TInt o = 0;
	TInt n = aFmt.Length();
	for (TInt i = 0; i < n && o < (TInt)sizeof(buf) - 40; ++i)
		{
		TUint c = aFmt[i];
		if (c != '%') { buf[o++] = c < 128 ? (char)c : '?'; continue; }
		++i;
		/* skip flags/width/precision/length */
		while (i < n && (aFmt[i]=='-'||aFmt[i]=='+'||aFmt[i]=='0'||aFmt[i]==' '||aFmt[i]=='#')) ++i;
		while (i < n && aFmt[i] >= '0' && aFmt[i] <= '9') ++i;
		if (i < n && aFmt[i]=='.') { ++i; while (i < n && aFmt[i] >= '0' && aFmt[i] <= '9') ++i; }
		while (i < n && (aFmt[i]=='l'||aFmt[i]=='h'||aFmt[i]=='L')) ++i;
		if (i >= n) break;
		TUint conv = aFmt[i];
		char tmp[48];
		const char* s = 0;
		switch (conv)
			{
			case '%': buf[o++] = '%'; break;
			case 'c': tmp[0] = (char)va_arg(ap, int); tmp[1] = 0; s = tmp; break;
			case 'd': case 'i':
				{ int v = va_arg(ap, int);
				  TInt k = 0; if (v < 0) { buf[o++]='-'; v = -v; }
				  do { tmp[k++] = (char)('0' + v % 10); v /= 10; } while (v);
				  while (k) buf[o++] = tmp[--k]; }
				break;
			case 'u':
				{ unsigned v = va_arg(ap, unsigned);
				  TInt k = 0; if (!v) tmp[k++]='0';
				  while (v) { tmp[k++] = (char)('0' + v % 10); v /= 10; }
				  while (k) buf[o++] = tmp[--k]; }
				break;
			case 'x': case 'X':
				{ unsigned v = va_arg(ap, unsigned); const char* h = conv=='x' ? "0123456789abcdef" : "0123456789ABCDEF";
				  TInt k = 0; if (!v) tmp[k++]='0';
				  while (v) { tmp[k++] = h[v & 15]; v >>= 4; }
				  while (k) buf[o++] = tmp[--k]; }
				break;
			case 'p':
				{ unsigned v = (unsigned)(TUintPtr)va_arg(ap, void*);
				  TInt k = 0; buf[o++]='0'; buf[o++]='x';
				  const char* h = "0123456789abcdef";
				  if (!v) tmp[k++]='0';
				  while (v) { tmp[k++] = h[v & 15]; v >>= 4; }
				  while (k) buf[o++] = tmp[--k]; }
				break;
			case 's': s = va_arg(ap, const char*); break;
			case 'S': { const TDesC16* d = va_arg(ap, const TDesC16*); s = narrow(*d, tmp, sizeof(tmp)); } break;
			default: buf[o++] = '?'; break;
			}
		if (s) while (*s && o < (TInt)sizeof(buf) - 2) buf[o++] = *s++;
		}
	buf[o] = 0;
	/* strip a trailing CR/LF: genuine RDebug::Printf terminates each call,
	 * then emit the formatted line through the REAL EUSER ord 915, with '%'
	 * doubled so the text is printed literally. */
	while (o > 0 && (buf[o-1] == '\n' || buf[o-1] == '\r')) buf[--o] = 0;
	if (!o) return;
	char esc[600];
	TInt e = 0;
	for (TInt i = 0; i < o && e < (TInt)sizeof(esc) - 2; ++i)
		{
		if (buf[i] == '%') esc[e++] = '%';
		esc[e++] = buf[i];
		}
	esc[e] = 0;
	RDebug::Printf(esc);   /* genuine EUSER.DLL ordinal 915 */
	}

/* -- VA_LIST formatter shims -------------------------------------------------
 * e32def.h typedefs VA_LIST as TInt8*[1] (mangled "PPa"), but the staged
 * EUSER.DLL exports the char* ("Pc") variants of TDes16::FormatList /
 * AppendFormatList; calling the real export from this ABI panics the process.
 * Provide local definitions (port layer) with a minimal Symbian-style
 * formatter: %%, %c, %d/%i, %u, %x/%X, %p, %s (narrow), %S (wide). */
static void wide_append(TDes16& aDes, TChar aC, TDes16Overflow* aOv)
	{
	if (aDes.Length() < aDes.MaxLength())
		aDes.Append(aC);
	else if (aOv)
		aOv->Overflow(aDes);
	}

static void wide_append_n(TDes16& aDes, const TText16* aS, TInt aLen, TDes16Overflow* aOv)
	{
	for (TInt i = 0; i < aLen; ++i)
		wide_append(aDes, aS[i], aOv);
	}

static void wide_append_narrow(TDes16& aDes, const char* aS, TDes16Overflow* aOv)
	{
	while (*aS)
		{ wide_append(aDes, TChar((TUint8)*aS), aOv); ++aS; }
	}

static void wide_format(TDes16& aDes, const TDesC16& aFmt, VA_LIST aList, TDes16Overflow* aOv)
	{
	TInt n = aFmt.Length();
	for (TInt i = 0; i < n; ++i)
		{
		TUint c = aFmt[i];
		if (c != '%') { wide_append(aDes, TChar(c), aOv); continue; }
		++i;
		while (i < n && (aFmt[i]=='-'||aFmt[i]=='+'||aFmt[i]=='0'||aFmt[i]==' '||aFmt[i]=='#')) ++i;
		while (i < n && aFmt[i] >= '0' && aFmt[i] <= '9') ++i;
		if (i < n && aFmt[i]=='.') { ++i; while (i < n && aFmt[i] >= '0' && aFmt[i] <= '9') ++i; }
		while (i < n && (aFmt[i]=='l'||aFmt[i]=='h'||aFmt[i]=='L')) ++i;
		if (i >= n) break;
		TUint conv = aFmt[i];
		TBuf<32> num;
		switch (conv)
			{
			case '%': wide_append(aDes, TChar('%'), aOv); break;
			case 'c': wide_append(aDes, TChar(VA_ARG(aList, int)), aOv); break;
			case 'd': case 'i':
				{ int v = VA_ARG(aList, int);
				  num.AppendNum(v);
				  wide_append_n(aDes, num.Ptr(), num.Length(), aOv); }
				break;
			case 'u':
				{ num.AppendNum(VA_ARG(aList, unsigned));
				  wide_append_n(aDes, num.Ptr(), num.Length(), aOv); }
				break;
			case 'x': case 'X':
				{ num.AppendNum(VA_ARG(aList, unsigned), EHex);
				  if (conv=='X') num.UpperCase();
				  wide_append_n(aDes, num.Ptr(), num.Length(), aOv); }
				break;
			case 'p':
				{ num.AppendNum((unsigned)(TUintPtr)VA_ARG(aList, void*), EHex);
				  wide_append_n(aDes, num.Ptr(), num.Length(), aOv); }
				break;
			case 's': wide_append_narrow(aDes, VA_ARG(aList, const char*), aOv); break;
			case 'S':
				{ const TDesC16* d = VA_ARG(aList, const TDesC16*);
				  /* Symbian descriptors are length-based, NOT NUL-terminated:
				   * scanning Ptr() for a 0 word runs past the data into the
				   * 0x29292929-prefilled user stack and overflows the target
				   * descriptor (this is what faults the genuine PS command). */
				  wide_append_n(aDes, d->Ptr(), d->Length(), aOv); }
				break;
			default: wide_append(aDes, TChar('?'), aOv); break;
			}
		}
	}

void TDes16::FormatList(const TDesC16& aFormat, VA_LIST aList)
	{
	TDes16& self = *const_cast<TDes16*>(this);
	self.Zero();
	wide_format(self, aFormat, aList, 0);
	}

void TDes16::AppendFormatList(const TDesC16& aFormat, VA_LIST aList, TDes16Overflow* aOverflowHandler)
	{
	wide_format(*const_cast<TDes16*>(this), aFormat, aList, aOverflowHandler);
	}

/* -- CConsoleBase / Console replacements (e32cons.h IMPORT_C symbols) ------ */
CConsoleBase::CConsoleBase() {}
CConsoleBase::~CConsoleBase() {}

TKeyCode CConsoleBase::Getch()
	{
	con_acquire_once();
	for (;;)
		{
		int c = con_tryread();
		if (c == KErrNotReady) { sleep_ms(20); continue; }
		if (c < 0) { sleep_ms(20); continue; }
		if (c == '\r' || c == '\n') return EKeyEnter;
		if (c == 8 || c == 127) return EKeyBackspace;
		if (c == 9) return EKeyTab;
		if (c == 27) return EKeyEscape;
		return TKeyCode(c & 0xff);
		}
	}

void CConsoleBase::Printf(TRefByValue<const TDesC> aFmt, ...)
	{
	va_list ap;
	va_start(ap, aFmt);
	port_printf(aFmt, ap);
	va_end(ap);
	}

void CConsoleBase::SetPos(TInt aX) { g_conX = aX; }
void CConsoleBase::SetPos(TInt aX, TInt aY) { g_conX = aX; g_conY = aY; }
TInt CConsoleBase::WhereX() const { return g_conX; }
TInt CConsoleBase::WhereY() const { return g_conY; }

/* concrete console: all pure virtuals of CConsoleBase route to PortCon */
class CPortConsole : public CConsoleBase
	{
public:
	virtual TInt Create(const TDesC&, TSize) { return KErrNone; }
	virtual void Read(TRequestStatus&) {}
	virtual void ReadCancel() {}
	virtual void Write(const TDesC& aDes) { port_write(aDes); }
	virtual TPoint CursorPos() const { return TPoint(g_conX, g_conY); }
	virtual void SetCursorPosAbs(const TPoint& aP) { g_conX = aP.iX; g_conY = aP.iY; }
	virtual void SetCursorPosRel(const TPoint& aP) { g_conX += aP.iX; g_conY += aP.iY; }
	virtual void SetCursorHeight(TInt) {}
	virtual void SetTitle(const TDesC&) {}
	virtual void ClearScreen() { con_raw(12); }
	virtual void ClearToEndOfLine() {}
	virtual TSize ScreenSize() const { return TSize(80, 25); }
	virtual TKeyCode KeyCode() const { return EKeyNull; }
	virtual TUint KeyModifiers() const { return 0; }
	};

static CPortConsole g_portConsole;

CConsoleBase* Console::NewL(const TDesC&, TSize)
	{
	con_acquire_once();   /* hold the PortCon lease so Write echo works */
	return &g_portConsole;   /* replaces EUSER ord 1115 (loads econs*.dll) */
	}

/* ------------------------------------------- EFSRV.DLL client-side stubs */
/* The genuine code links RFs/RFile/CFileMan/... from EFSRV.DLL.  Our port's
 * file server is being built in parallel; these hand stubs cover the startup
 * and shell-prompt path.  Everything NOT defined here gets a generic
 * "return 0" stub generated by the builder (documented in stubs.S). */
TInt RFs::Connect(TInt)                       { return KErrNone; }  /* _ZN3RFs7ConnectEi */
void RFs::Close()                             { }                  /* _ZN3RFs5CloseEv */
TInt RFs::DriveList(TDriveList& aList) const  { aList.FillZ(KMaxDrives); aList[2] = 1; return KErrNone; }
TInt RFs::DriveList(TDriveList& aList, TUint) const { return DriveList(aList); }
TInt RFs::SessionPath(TDes& aPath) const      { aPath = _L("C:\\"); return KErrNone; }
TInt RFs::SetSessionPath(const TDesC&)        { return KErrNone; }
TInt RFs::Entry(const TDesC&, TEntry&) const  { return KErrNotFound; } /* no AUTOEXEC.BAT */
TInt RFs::CharToDrive(TChar aChar, TInt& aDrive)
	{
	TInt c = aChar & 0xdf;             /* upper-case */
	if (c < 'A' || c > 'Z') return KErrArgument;
	aDrive = c - 'A';
	return KErrNone;
	}
TChar RFs::GetSystemDriveChar()               { return 'C'; }
TInt RFs::SetSystemDrive(TDriveNumber)        { return KErrNone; }
void RFs::StartupInitComplete(TRequestStatus& aStat) { aStat = KErrNone; } /* completes the wait */

/* CFileMan: hand back a valid object so ~CShell's delete sees a real vtable */
CFileBase::CFileBase(RFs&) {}
CFileBase::~CFileBase() {}
CFileMan::CFileMan(RFs& aFs) : CFileBase(aFs) {}
CFileMan::~CFileMan() {}
class CPortFileMan : public CFileMan
	{
public:
	CPortFileMan(RFs& aFs) : CFileMan(aFs) {}
	};
static CPortFileMan* g_portFileMan = 0;
CFileMan* CFileMan::NewL(RFs& aFs)                    { if (!g_portFileMan) g_portFileMan = new CPortFileMan(aFs); return g_portFileMan; }
CFileMan* CFileMan::NewL(RFs& aFs, MFileManObserver*) { return NewL(aFs); }

/* ------------------------------------------------------- HAL.DLL stubs */
/* Replaces HAL::Get/HAL::Set (halservices/hal): report "no HAL data file",
 * success with value 0 -- estart only asserts KErrNone on EMachineUid. */
TInt HAL::Get(HALData::TAttribute, TInt& aValue)      { aValue = 0; return KErrNone; }
TInt HAL::Set(HALData::TAttribute, TInt)              { return KErrNone; }
TInt HAL::Get(TInt, HALData::TAttribute a, TInt& v)   { return HAL::Get(a, v); }
TInt HAL::Set(TInt, HALData::TAttribute a, TInt v)    { return HAL::Set(a, v); }

/* -------------------------------------------------- DOMAINCLI.DLL stubs */
/* Replaces RDmDomainManager (domainmanager.h): no DOMAINSRV in this port. */
TInt RDmDomainManager::Connect()                                   { return KErrNone; }
void RDmDomainManager::Close()                                     { }
TInt RDmDomainManager::WaitForInitialization()                     { return KErrNone; }
void RDmDomainManager::RequestSystemTransition(TPowerState, TRequestStatus& s) { s = KErrNone; }
void RDmDomainManager::RequestDomainTransition(TDmDomainId, TPowerState, TRequestStatus& s) { s = KErrNone; }
