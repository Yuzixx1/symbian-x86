// Port-layer shims for the genuine EWSRV E32 build.  These are NOT Symbian
// source: they supply the handful of symbols that genuine EWSRV would normally
// import from HAL.DLL, DOMAINCLI.DLL and EFSRV.DLL.  Our kernel loader supports
// only one DLL dependency (EUSER), so those small client sides are provided
// here.  Everything else is compiled unmodified from kernelhwsrv/.
#include <e32std.h>
#include <e32base.h>
#include <hal.h>
#include <f32file.h>
#include <domainmanager.h>

// -- Heap.  The port kernel does not install an RAllocator on user threads, so
//    EUSER.DLL's User::Alloc dereferences a NULL thread allocator.  Install a
//    minimal RAllocator-shaped bump allocator via Exec::HeapSwitch (fast exec
//    2) before any genuine EUSER code runs.  Its vtable[0]=Alloc, [1]=Free.
extern "C" void* ExecHeapSwitch(void*) __asm__("_ZN4Exec10HeapSwitchEP10RAllocator");

static char   sHeap[512 * 1024];
static unsigned sUsed = 0;

static void* QAlloc(void*, int aSize)
    {
    unsigned n = ((unsigned)aSize + 7u) & ~7u;
    if (!n || sUsed + n > sizeof(sHeap)) return 0;
    void* p = &sHeap[sUsed];
    sUsed += n;
    return p;
    }
static void QFree(void*, void*) { }
static void* QReAlloc(void*, void* aPtr, int aSize, int)
    {
    (void)aPtr;
    return QAlloc(0, aSize);
    }
static int QInt0(void*) { return 0; }
static int QInt1(void*, int) { return 0; }
static void* sHeapVtable[10] = {
    (void*)QAlloc, (void*)QFree, (void*)QReAlloc,
    (void*)QInt0, (void*)QInt0, (void*)QInt0, (void*)QInt0,
    (void*)QInt1, (void*)QInt1, (void*)QInt1,
};
struct TQAllocator { void* iVtable; } sQAllocator = { sHeapVtable };

void EwsrvInstallHeap() { ExecHeapSwitch(&sQAllocator); }

void* operator new(unsigned int aSize)  { return QAlloc(0, (int)aSize); }
void operator delete(void* aPtr)        { QFree(0, aPtr); }
void operator delete(void* aPtr, unsigned int) { QFree(0, aPtr); }
void* operator new[](unsigned int aSize){ return QAlloc(0, (int)aSize); }
void operator delete[](void* aPtr)      { QFree(0, aPtr); }

// -- HAL: we have no HAL.DLL.  Display attributes are answered by the QEMU
//    display HAL through UserSvr::HalFunction (already in EUSER IAT); the rest
//    report "not supported" so EWSRV skips optional brightness/contrast paths.
TInt HAL::Get(HALData::TAttribute /*aAttribute*/, TInt& /*aValue*/)
    { return KErrNotSupported; }
TInt HAL::Set(HALData::TAttribute /*aAttribute*/, TInt /*aValue*/)
    { return KErrNotSupported; }
TInt HAL::Get(TInt, HALData::TAttribute aAttribute, TInt& aValue)
    { return HAL::Get(aAttribute, aValue); }
TInt HAL::Set(TInt, HALData::TAttribute aAttribute, TInt aValue)
    { return HAL::Set(aAttribute, aValue); }

// -- Domain manager client: no DOMAINSRV in this port; transitions are no-ops.
TInt RDmDomainManager::Connect()                                    { return KErrNone; }
void RDmDomainManager::Close()                                      { }
void RDmDomainManager::RequestSystemTransition(TPowerState, TRequestStatus& aStatus)
    { aStatus = KErrNone; }
void RDmDomainManager::RequestDomainTransition(TDmDomainId, TPowerState, TRequestStatus& aStatus)
    { aStatus = KErrNone; }

// -- File-server client subset used only by CNotifierManager::RegisterL() to
//    scan \system\tnotifiers\.  Returning KErrNotFound skips the scan cleanly.
TInt RFs::Connect(TInt)   { return KErrNone; }
void RFs::Close()         { }
TFindFile::TFindFile(RFs& aFs) : iFs(&aFs) { }
TInt TFindFile::FindWildByDir(const TDesC&, const TDesC&, CDir*&) { return KErrNotFound; }
TInt TFindFile::FindWild(CDir*&)                                  { return KErrNotFound; }
TParse::TParse() { }
TInt TParse::SetNoWild(const TDesC&, const TDesC*, const TDesC*) { return KErrNone; }
const TDesC& TParseBase::FullName() const { return TPtrC(0, 0); }
TPtrC TParseBase::DriveAndPath() const    { return TPtrC(0, 0); }
TPtrC TParseBase::NameAndExt() const      { return TPtrC(0, 0); }
TInt CDir::Count() const                  { return 0; }
const TEntry& CDir::operator[](TInt) const
    { static TEntry e; return e; }
