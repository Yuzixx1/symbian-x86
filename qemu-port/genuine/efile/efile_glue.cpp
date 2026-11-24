// Port startup glue + minimal heap for the genuine EFILE.EXE build.
// app_main() is entered by user/start.S after the loader bound all PE2 EUSER
// imports; it installs an RAllocator-shaped bump allocator via fast exec 2,
// runs genuine static constructors, then enters the genuine sf_main.cpp E32Main.
// This is port glue only: no kernelhwsrv/ source is modified.
// gcc.h + the x86 variant header are normally force-included by
// f32_efsrv_build.py; include them here so genuine_build.compile_one (which
// has no -include support) sees the same SDK configuration as EFILE.A.
#include <gcc.h>
#include <variant/port_x86_rfile64_v1.hrh>
#include <e32std.h>
#include <e32debug.h>
#include <hal.h>

extern "C" int app_main(void);
extern "C" void process_exit(int) __attribute__((noreturn));
GLREF_C TInt E32Main();
extern "C" void (*__init_array_start)(void);
extern "C" void (*__init_array_end)(void);
extern "C" void* __dso_handle = 0;
extern "C" void EfileInstallHeap(void);

// Exec::HeapSwitch(RAllocator*) -- fast exec 2 (number in EAX, arg in ECX).
extern "C" void ExecHeapSwitch(void*) __asm__("_ZN4Exec10HeapSwitchEP10RAllocator");

static char sHeap[8 * 1024 * 1024];
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
    { (void)aPtr; return QAlloc(0, aSize); }
static int QInt0(void*) { return 0; }
static int QInt1(void*, int) { return 0; }

// MAllocator::DebugFunction is vtable slot 8 (e32cmn.h:2630-2643: Alloc, Free,
// ReAlloc, AllocLen, Compress, Reset, AllocSize, Available, DebugFunction,
// Extension_).  Genuine sf_ldr.cpp:366 does
//     User::Allocator().MaxLength() -> DebugFunction(EGetMaxLength)
// to size the loader thread's max heap; returning 0 trips USER 111
// (EThrdHeapMaxLessThanMin) in RThread::Create.  Report the full bump heap.
enum { EPortDebugGetMaxLength = 49 };  // RAllocator::EGetMaxLength (e32cmn.h:2760)
static int QDebugFunction(void*, int aFunc, void*, void*)
    {
    if (aFunc == EPortDebugGetMaxLength) return (int)sizeof(sHeap);
    return 0;
    }
static void* sHeapVtable[10] = {
    (void*)QAlloc, (void*)QFree, (void*)QReAlloc,
    (void*)QInt0, (void*)QInt0, (void*)QInt0, (void*)QInt0,
    (void*)QInt1, (void*)QDebugFunction, (void*)QInt1,
};
struct TQAllocator { void* iVtable; } sQAllocator = { sHeapVtable };

// Switch through the genuine exported User::SwitchAllocator, not the raw
// fast-exec stub: the stub only updates the kernel thread's iAllocator, while
// genuine EUSER caches the heap user-side in LocalThreadData()->iHeap (see
// us_exec.cpp:2166-2171).  User::Allocator() reads that cache, so with the raw
// stub sf_ldr.cpp:365-366 queried the default heap (max length 0) and never
// reached this fake vtable.  SwitchAllocator updates both and is already a
// bound EUSER import, so the import count is unchanged.
extern "C" void EfileInstallHeap(void)
    {
    User::SwitchAllocator((RAllocator*)&sQAllocator);
    }

// The genuine sfile StartupThread calls User::SetCritical(ESystemCritical),
// then panics in this port because Z:\Sys\Bin\ESTART.EXE is not installed.
// A critical thread panic takes down the whole kernel (K::ESystemThreadPanic),
// so neutralize ONLY the criticality marker: the genuine thread may still
// panic, but its CLogon completes and the file server continues starting.
extern "C" void UserSetCriticalShim(int) __asm__("_ZN4User11SetCriticalENS_9TCriticalE");
extern "C" void UserSetCriticalShim(int) { }

// Diagnostics / port shim: the genuine startup thread panics in this port
// (no Z:\Sys\Bin\ESTART.EXE, no media drivers).  A panic in the file-server
// process takes down the whole kernel via ESystemThreadPanic, so log it and
// return instead; genuine code then continues with its error path.
extern "C" void UserPanicShim(const TDesC16&, int) __asm__("_ZN4User5PanicERK7TDesC16i");
extern "C" void UserPanicShim(const TDesC16&, int aReason)
    { RDebug::Printf("EFILE: port-shim User::Panic reason=%d\r\n", aReason); }

// __cxa_atexit comes from the host C++ runtime normally; no destructors are
// registered in this freestanding image, so a no-op is sufficient.
extern "C" int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }

// HAL.DLL is not part of this single-DLL PE2 image; report unsupported.
TInt HAL::Get(HALData::TAttribute, TInt&) { return KErrNotSupported; }

void* operator new(unsigned int aSize) { return QAlloc(0, (int)aSize); }
void operator delete(void* aPtr) { QFree(0, aPtr); }
void operator delete(void* aPtr, unsigned int) { QFree(0, aPtr); }
void* operator new[](unsigned int aSize) { return QAlloc(0, (int)aSize); }
void operator delete[](void* aPtr) { QFree(0, aPtr); }

// System-drive invariant for the genuine session path.  Genuine
// RFs::GetSystemDriveChar() is compiled from EFSRV.A into this image (not an
// EUSER import): 'A' + RFs::GetSystemDrive().  GetSystemDrive() reads the
// P&S property (category KFileServerUidValue=0x100039e3, key
// KSystemDriveKey=0x10283049) and falls back to EDriveC ('C') whenever that
// read fails.  In this port the property is undefined and EFILE's secure ID
// (UID3 0x100000bf) is not the category, so RProperty::Define() returns
// KErrPermissionDenied (-46) -- no port-side write is possible or needed:
// the genuine fallback already makes CServerFs::NewSessionL build
// "C:\\Private\\<SID>\\".  Log the value once so gate runs can verify it.
extern "C" int EfileGetSystemDriveChar(void) __asm__("_ZN3RFs18GetSystemDriveCharEv");

static void EfileLogSystemDrive(void)
    {
    RDebug::Printf("EFILE: system drive char=%d ('%c')\r\n",
                   (int)EfileGetSystemDriveChar(),
                   (char)EfileGetSystemDriveChar());
    }

// ---------------------------------------------------------------------------
// x86 port shim: suppress genuine user-thread re-entry into app_main/E32Main.
//
// The genuine EKA2 x86 user-thread start path
// (kernelhwsrv/kernel/eka/kernel/x86/ckernel.cia:109-123) does NOT jump to the
// function passed to RThread::Create().  Instead it enters user mode at the
// process entry point: DCodeSeg::iEntryPtVeneer for the first thread, or
// DProcess::iReentryPoint for every later thread.  On a real Symbian build the
// EUSER entry veneer decodes the SThreadCreateInfo handed to it on the stack
// and calls the requested thread function.  This port's entry point is our own
// user/start.S -> app_main, which has no such dispatcher, so every
// RThread::Create() issued by the genuine startup code re-enters app_main.
//
// In EFILE that happens twice: InitLoader() (sf_ldr.cpp:367) creates the
// loader thread, and commonInitialize() (sf_main.cpp:517) creates the
// StartupThread.  Both re-entries ran E32Main() again; E32Main's
// ServerIsRunning() shortcut made them return 0 immediately, then start.S
// called process_exit(), tearing down the one real file-server instance and
// detaching its client sessions (F32CLIENT SessionPath r=-15, KErrServer-
// Terminated).  A trampoline that dispatches to the real thread function was
// tried and rejected: genuine EUSER TLS lives at absolute low addresses that
// are mapped supervisor-only, so a user-side dispatcher faults (#PF).  See
// docs/EFILE_GUARD_VS_TRAMPOLINE.md and AGENTS.md.
//
// Narrowest x86-only fix: exactly one app_main (the original process thread)
// runs the genuine E32Main.  Re-entrant threads return immediately so their
// Rendezvous()/Logon() complete with KErrNone, InitLoader() and commonInitialize()
// proceed, and the single genuine CServerFs installs its scheduler and stays
// alive.  No kernelhwsrv/ source is changed.
static volatile int sEfileAppMainOwner = 0;

extern "C" int app_main(void)
    {
    if (__sync_lock_test_and_set(&sEfileAppMainOwner, 1))
        {
        RDebug::Printf("EFILE: re-entrant app_main suppressed\r\n");
        return 0;
        }
    EfileInstallHeap();
    for (void (**f)(void) = &__init_array_start; f != &__init_array_end; ++f)
        (*f)();
    EfileLogSystemDrive();
    RDebug::Printf("EFILE: genuine file server starting\r\n");
    TInt r = E32Main();
    RDebug::Printf("EFILE: E32Main returned %d\r\n", (int)r);
    process_exit(r);
    return (int)r;
    }
