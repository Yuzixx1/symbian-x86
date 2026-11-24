/*
 * hal_glue.cpp -- port glue + minimal stubs for building the GENUINE HAL user
 * library (hal_main.cpp + userhal.cpp + generated hal_values/hal_config.cpp)
 * as HALU.DLL on the ported x86 kernel.
 *
 * kernelhwsrv/ is untouched; this file lives in qemu-port/genuine/hal/ and
 * only supplies what the port loader cannot import: our elf2e32.py pipeline
 * supports exactly ONE EUSER.DLL import (ordinal 915, RDebug::Printf, bound
 * through the PE2 IAT slot `hal_printf_slot`), so every other EUSER symbol
 * the genuine HAL code references is stubbed here:
 *
 *   stub                          reason
 *   ----                          ------
 *   User::Panic                   genuine EUSER export; never reached in the
 *                                 smoke run (prints a marker + parks if hit)
 *   User::Alloc / User::Free      bump allocator over a static 64 KiB heap
 *                                 (no process heap in the freestanding image)
 *   RProperty::Define/Set/Get     in-image property table (KUidSystemCategory,
 *                                 KUidHalPropertyKeyBase+i) so HAL::Get/Set on
 *                                 settable attributes work without the
 *                                 kernel property service
 *   UserSvr::HalFunction x2       returns KErrNotSupported: the port kernel
 *                                 has no variant/display HAL behind it yet
 *   __cxa_pure_virtual            freestanding C++ runtime glue
 *   operator new/delete           forwarded to User::Alloc
 *
 * The DLL entry `_start` prints load markers through the EUSER IAT slot,
 * exercises the genuine HAL::Get()/HAL::Set() code paths, and exits the
 * process cleanly via exec 115 (same as user/start.S process_exit).
 */
#include <e32std.h>
#include <u32std.h>
#include <e32svr.h>
#include <e32property.h>
#include <kernel/hal_int.h>

/* PE2 IAT slot at 0x400000+iat_off, defined by ld --defsym in genuine_build.py.
   Before load it holds ordinal 915; after EUSER.DLL binding it holds the
   resolved RDebug::Printf address. */
extern void (*hal_printf_slot)(const char*, ...);

#define marker(...) do { \
    unsigned fn_ = *(unsigned*)(void*)&hal_printf_slot; \
    if (fn_ && fn_ != 915u) \
        ((void (*)(const char*, ...))fn_)(__VA_ARGS__); \
} while (0)

/* ---- process exit (twin of user/start.S process_exit, exec 115) -------- */
static __attribute__((noreturn)) void hal_exit(int code)
{
    __asm__ volatile(
        "mov $115, %%eax\n\t"
        "mov %0, %%ebx\n\t"
        "mov $0xffff8001, %%ecx\n\t"
        "xor %%edx, %%edx\n\t"
        "xor %%esi, %%esi\n\t"
        "int $0x21\n\t"
        :: "r"(code) : "eax", "ebx", "ecx", "edx", "esi", "memory");
    for (;;) {}
}

/* ---- EUSER stubs (see header comment) ---------------------------------- */
void User::Panic(const TDesC& /*aCategory*/, TInt aReason)
{
    marker("HALU: User::Panic stub reached, reason=%d\r\n", aReason);
    for (;;) {} /* a real panic must never return */
}

static unsigned char sHeap[64 * 1024];
static unsigned sHeapUsed;

TAny* User::Alloc(TInt aSize)
{
    if (aSize < 0)
        return NULL;
    unsigned need = ((unsigned)aSize + 3u) & ~3u;
    if (sHeapUsed + need > sizeof(sHeap))
        return NULL;
    void* p = sHeap + sHeapUsed;
    sHeapUsed += need;
    return p;
}
void User::Free(TAny* /*aCell*/) {}

/* In-image RProperty table for KUidSystemCategory / KUidHalPropertyKeyBase+i. */
struct Prop { TUint iKey; TInt iValue; TBool iSet; };
static Prop sProps[HAL::ENumHalAttributes + 8];

static Prop* propFind(TUint aKey)
{
    for (TInt i = 0; i < (TInt)(sizeof(sProps) / sizeof(sProps[0])); ++i)
        if (sProps[i].iSet && sProps[i].iKey == aKey)
            return &sProps[i];
    return NULL;
}
static Prop* propAlloc(TUint aKey)
{
    for (TInt i = 0; i < (TInt)(sizeof(sProps) / sizeof(sProps[0])); ++i)
        if (!sProps[i].iSet)
            { sProps[i].iKey = aKey; sProps[i].iSet = ETrue; sProps[i].iValue = 0; return &sProps[i]; }
    return NULL;
}

TInt RProperty::Define(TUid /*aCategory*/, TUint aKey, TInt /*aAttr*/,
                       const TSecurityPolicy& /*aReadPolicy*/,
                       const TSecurityPolicy& /*aWritePolicy*/, TInt /*aPreallocated*/)
{
    if (propFind(aKey))
        return KErrAlreadyExists;
    if (!propAlloc(aKey))
        return KErrNoMemory;
    return KErrNone;
}
TInt RProperty::Set(TUid /*aCategory*/, TUint aKey, TInt aValue)
{
    Prop* p = propFind(aKey);
    if (!p)
        p = propAlloc(aKey);
    if (!p)
        return KErrNoMemory;
    p->iValue = aValue;
    return KErrNone;
}
TInt RProperty::Get(TUid /*aCategory*/, TUint aKey, TInt& aValue)
{
    Prop* p = propFind(aKey);
    if (!p)
        return KErrNotFound;
    aValue = p->iValue;
    return KErrNone;
}

/* The port kernel has no HAL executive behind UserSvr::HalFunction yet. */
TInt UserSvr::HalFunction(TInt /*aGroup*/, TInt /*aFunction*/, TAny* /*a1*/, TAny* /*a2*/)
{ return KErrNotSupported; }
TInt UserSvr::HalFunction(TInt /*aGroup*/, TInt /*aFunction*/, TAny* /*a1*/, TAny* /*a2*/,
                          TInt /*aDeviceNumber*/)
{ return KErrNotSupported; }

/* Descriptor constructors: genuine implementations live in EUSER
   (eka/common/des8.cpp, euser/us_func.cpp); the bodies below are the same
   one-line initialisations, stubbed because our pipeline imports only
   EUSER ord 915. */
/* Genuine inline descriptor ctors live only in eka/common/des8.cpp; the
   definitions below are those same one-liners (des8.cpp:80-101). */
inline TDesC8::TDesC8(TInt aType, TInt aLength)
    : iLength(aLength | (aType << KShiftDesType8)) {}
inline TDes8::TDes8(TInt aType, TInt aLength, TInt aMaxLength)
    : TDesC8(aType, aLength), iMaxLength(aMaxLength) {}
TPtr8::TPtr8(TUint8* aBuf, TInt aLength, TInt aMaxLength)
    : TDes8(EPtr, aLength, aMaxLength), iPtr(aBuf) {}
TBufBase8::TBufBase8(TInt aLength, TInt aMaxLength)
    : TDes8(EBuf, aLength, aMaxLength) {}
TVersion::TVersion() : iMajor(0), iMinor(0), iBuild(0) {}

extern "C" void __cxa_pure_virtual()
{
    marker("HALU: __cxa_pure_virtual stub reached\r\n");
    for (;;) {}
}

void* operator new(unsigned int aSize) { return User::Alloc((TInt)aSize); }
void operator delete(void* /*aPtr*/) {}
void operator delete(void* /*aPtr*/, unsigned int) {}
void* operator new[](unsigned int aSize) { return User::Alloc((TInt)aSize); }
void operator delete[](void* /*aPtr*/) {}

/* ---- entry: smoke markers + exercise genuine HAL code ------------------
   --type dll builds use _start directly (no port startup); the exe-typed
   smoke twin (hal_smoke.E32, -DHAL_AS_EXE_SMOKE) is entered through the
   port's user/start.S -> app_main. */
static int hal_smoke_main(void)
{
    marker("HALU: genuine hal_main.cpp+userhal.cpp loaded (HALU.DLL image, x86 port)\r\n");

    TInt value = -1;
    TInt r = HAL::Get(HALData::ECPU, value);          /* genuine HAL::Get */
    marker("HALU: HAL::Get(ECPU) -> r=%d value=%d (ECPU_X86=%d)\r\n",
           (int)r, (int)value, (int)HALData::ECPU_X86);

    r = HAL::Get(HALData::EMemoryPageSize, value);     /* settable, via RProperty stub */
    marker("HALU: HAL::Get(EMemoryPageSize) -> r=%d value=%d\r\n", (int)r, (int)value);

    /* ELanguageIndex is settable with no implementation -> RProperty path
       (stubbed in-image), so Set/Get round-trips through WriteWord/ReadWord. */
    r = HAL::Set(HALData::ELanguageIndex, 7);
    TInt check = -1;
    HAL::Get(HALData::ELanguageIndex, check);
    marker("HALU: HAL::Set/Get(ELanguageIndex) -> r=%d roundtrip=%d\r\n", (int)r, (int)check);

    TInt n = 0;
    HAL::SEntry* entries = NULL;
    r = HAL::GetAll(n, entries);                       /* genuine GetAll, User::Alloc stub */
    marker("HALU: HAL::GetAll -> r=%d entries=%d ptr=%08x\r\n", (int)r, (int)n, (unsigned)entries);

    marker("HALU SMOKE: done, exiting cleanly\r\n");
    return 0;
}

#ifdef HAL_AS_EXE_SMOKE
extern "C" int app_main(void)
{
    return hal_smoke_main();   /* port start.S calls process_exit afterwards */
}
#else
extern "C" int _start(void)
{
    int r = hal_smoke_main();
    hal_exit(r);
    return r;
}
#endif
