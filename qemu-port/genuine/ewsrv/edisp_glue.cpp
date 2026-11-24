// Port glue for the GENUINE edisp display driver.
//
// kernelhwsrv/kernel/eka/drivers/edisp/epoc/generic/wd_generic.cpp (the X86
// source of edisp.mmp, minus the VGA-text backend) is compiled UNMODIFIED.
// On real hardware that driver gets its framebuffer geometry from HAL.DLL via
// HAL::Get(HALData::EDisplay*); this port has no HAL.DLL, so the display
// attributes are answered here from the QEMU VBE LFB through the existing
// ScreenInfo bridge: UserSvr::ScreenInfo -> Exec::HalFunction(EHalGroupDisplay,
// EDisplayHalScreenInfo) is answered by variant/qemu_display.cpp
// DisplayHalFunction(), which returns the live VBE LFB + pixel size (and calls
// display_ensure_graphics() so the LFB is up before EWSRV paints).
//
// Used by two builds:
//  * build_ewsrv.py: linked into the monolithic EWSRV.E32 (the port loader
//    binds only one DLL dependency, EUSER.DLL; dynamic ELoadLibrary is
//    KErrNotSupported, so genuine ws_win.cpp's RLibrary::Load("EDISP.DLL")
//    is replaced by a direct CScreenDriver::New() call in patched/ws_win.cpp,
//    and the genuine edisp code is linked in).  Defines EDISP_PORT_MARKER so
//    the startup log keeps the "EWSRV-EDISP: LFB=" marker.
//  * build_edisp.py: standalone EDISP.DLL (tools/genuine_build.py --type dll,
//    ordinal-1 export CScreenDriver::New() per kernel/eka/bx86/edisp.def).
//    Defines EDISP_STANDALONE + EDISP_USE_IAT_SLOT: genuine_build.py emits a
//    single EUSER import, so UserSvr::ScreenInfo (ord 1211) is called through
//    the PE2 IAT slot (--iat-sym edisp_screeninfo_slot) and the remaining
//    EUSER imports are stubbed locally below.
//
// Documented stubs:
//  * NewScreenDriverVgaText() returns NULL - the VGA text-mode backend
//    (epoc/vga/vgatext.cpp) is not usable with a VBE linear framebuffer and is
//    not compiled.  Genuine CScreenDriver::New() calls it only when
//    HAL::Get(EDisplayMode) returns 0; the HAL::Get below always reports
//    EColor16M, so the genuine generic pixel driver is always selected.
//  * HAL::Set() and all non-display HAL::Get() attributes return
//    KErrNotSupported (HAL.DLL absent; same behaviour as the previous EWSRV
//    shim for brightness/contrast/backlight/keyboard paths).
//  * Hardware-accelerated block fill/copy: UserSvr::HalFunction ->
//    EDisplayHalBlockFill/Copy is KErrNotSupported in the port display HAL,
//    so the genuine driver takes its software blit path (iHwAcc == 0).
//  * Bits-per-pixel is fixed at 32: the port only brings up -vga std VBE
//    modes (32bpp), and EDisplayHalScreenInfo carries no bpp field.
#include <e32std.h>
#include <e32base.h>
#include <e32svr.h>
#include <e32twin.h>
#include <hal.h>
#include <e32event_private.h>
class CScreenDriver;                   // w32disp.h pulls TColorIndex deps; a
                                       // pointer return only needs the fwd decl
#ifdef EDISP_PORT_MARKER
#include <e32debug.h>
#endif

#ifdef EDISP_USE_IAT_SLOT
// PE2 IAT slot for EUSER ordinal 1211 (UserSvr::ScreenInfo(TDes8&)), stamped
// at codeBase+iat_off by tools/genuine_build.py --iat-sym; the kernel loader
// overwrites the slot with the genuine export address at bind time.
extern "C" const TUint32 edisp_screeninfo_slot;
#endif

// --- the existing ScreenInfo bridge (variant/qemu_display.cpp:496) ----------
static TScreenInfoV01 sScreenInfo;
static TBool sScreenInfoFetched = EFalse;

static void FetchScreenInfo()
    {
    if (sScreenInfoFetched)
        return;
    sScreenInfoFetched = ETrue;
    // zero first: UserSvr::ScreenInfo returns void, so a missing display HAL
    // leaves iScreenAddressValid == 0 and the driver ctor fails cleanly.
    for (TUint i = 0; i < sizeof(sScreenInfo); ++i)
        ((TUint8*)&sScreenInfo)[i] = 0;
    TPckg<TScreenInfoV01> pckg(sScreenInfo);
#ifdef EDISP_USE_IAT_SLOT
    (*(void (**)(TDes8&))(&edisp_screeninfo_slot))(pckg);
#else
    UserSvr::ScreenInfo(pckg);
#endif
    }

// --- display HAL attributes answered from the VBE LFB -----------------------
TInt HAL::Get(HALData::TAttribute aAttribute, TInt& aValue)
    {
    switch (aAttribute)
        {
    case HALData::EDisplayMode:
        aValue = EColor16M;          // VBE LFB is 32bpp true colour
        return KErrNone;
    case HALData::EDisplayBitsPerPixel:
        aValue = 32;
        return KErrNone;
    case HALData::EDisplayIsPalettized:
    case HALData::EDisplayIsMono:
        aValue = EFalse;
        return KErrNone;
    case HALData::EDisplayMemoryAddress:
        {
        FetchScreenInfo();
        if (!sScreenInfo.iScreenAddressValid)
            return KErrNotSupported;
        aValue = (TInt)sScreenInfo.iScreenAddress;
#ifdef EDISP_PORT_MARKER
        RDebug::Printf("EWSRV-EDISP: LFB=%08x %dx%d bpp=32 mode=%d\r\n",
                       (unsigned)aValue, sScreenInfo.iScreenSize.iWidth,
                       sScreenInfo.iScreenSize.iHeight, (int)EColor16M);
#endif
        return KErrNone;
        }
    case HALData::EDisplayOffsetToFirstPixel:
        aValue = 0;
        return KErrNone;
    case HALData::EDisplayOffsetBetweenLines:
        FetchScreenInfo();
        aValue = sScreenInfo.iScreenSize.iWidth * 4;
        return KErrNone;
    case HALData::EDisplayXPixels:
        FetchScreenInfo();
        aValue = sScreenInfo.iScreenSize.iWidth;
        return KErrNone;
    case HALData::EDisplayYPixels:
        FetchScreenInfo();
        aValue = sScreenInfo.iScreenSize.iHeight;
        return KErrNone;
    default:
        return KErrNotSupported;    // no HAL.DLL in this port
        }
    }

TInt HAL::Set(HALData::TAttribute /*aAttribute*/, TInt /*aValue*/)
    { return KErrNotSupported; }     // palette writes: no-op in 32bpp

TInt HAL::Get(TInt /*aDeviceNumber*/, HALData::TAttribute aAttribute, TInt& aValue)
    { return HAL::Get(aAttribute, aValue); }

TInt HAL::Set(TInt /*aDeviceNumber*/, HALData::TAttribute aAttribute, TInt aValue)
    { return HAL::Set(aAttribute, aValue); }

// --- VGA text backend stub (see header comment) ------------------------------
CScreenDriver* NewScreenDriverVgaText()
    { return NULL; }

#ifdef EDISP_STANDALONE
// --- standalone EDISP.DLL: local stubs for the single-import build -----------
// tools/genuine_build.py emits one PE2 import (UserSvr::ScreenInfo, ord 1211);
// everything else the genuine wd_generic.cpp needs is provided here so the
// image links freestanding.  UserSvr::HalFunction stubbed -> the genuine
// driver probes hw accel, gets KErrNotSupported and uses software blits,
// exactly like the in-EWSRV build where the port display HAL answers the same.
TInt UserSvr::HalFunction(TInt /*aGroup*/, TInt /*aFunction*/, TAny* /*a1*/, TAny* /*a2*/)
    { return KErrNotSupported; }

static char   sEdispHeap[64 * 1024];
static unsigned sEdispUsed = 0;

static void* EdispAlloc(int aSize)
    {
    unsigned n = ((unsigned)aSize + 7u) & ~7u;
    if (!n || sEdispUsed + n > sizeof(sEdispHeap))
        return 0;
    void* p = &sEdispHeap[sEdispUsed];
    sEdispUsed += n;
    return p;
    }

void* operator new(unsigned int aSize)             { return EdispAlloc((int)aSize); }
void  operator delete(void* aPtr)                   { (void)aPtr; }
void  operator delete(void* aPtr, unsigned int)     { (void)aPtr; }
void* operator new[](unsigned int aSize)            { return EdispAlloc((int)aSize); }
void  operator delete[](void* aPtr)                 { (void)aPtr; }

// Remaining EUSER symbols the genuine driver references, stubbed locally
// (single-import image): memclr (e32cmn.h), User::AllocZ (e32std.h, used by
// the inline CBase::operator new), CBase dtor/Extension_ (vtable entries) and
// the TPtr8(buf,len,max) ctor used by TPckg<TScreenInfoV01>.
TAny* memclr(TAny* aTrg, unsigned int aLength)
    {
    TUint8* p = (TUint8*)aTrg;
    for (unsigned int i = 0; i < aLength; ++i)
        p[i] = 0;
    return aTrg;
    }
// e32cmn.h declares these IMPORT_C (C++ mangled); gcc emits calls to them
// from the inline Mem:: helpers even with -fno-builtin.
TAny* memset(TAny* aTrg, TInt aValue, unsigned int aLength)
    {
    TUint8* p = (TUint8*)aTrg;
    for (unsigned int i = 0; i < aLength; ++i)
        p[i] = (TUint8)aValue;
    return aTrg;
    }
TAny* memcpy(TAny* aTrg, const TAny* aSrc, unsigned int aLength)
    {
    TUint8* d = (TUint8*)aTrg;
    const TUint8* s = (const TUint8*)aSrc;
    for (unsigned int i = 0; i < aLength; ++i)
        d[i] = s[i];
    return aTrg;
    }
TAny* memmove(TAny* aTrg, const TAny* aSrc, unsigned int aLength)
    {
    TUint8* d = (TUint8*)aTrg;
    const TUint8* s = (const TUint8*)aSrc;
    if (d < s)
        for (unsigned int i = 0; i < aLength; ++i)
            d[i] = s[i];
    else
        for (unsigned int i = aLength; i > 0; --i)
            d[i - 1] = s[i - 1];
    return aTrg;
    }
TInt TRect::Width() const                           { return iBr.iX - iTl.iX; }
TInt TRect::Height() const                          { return iBr.iY - iTl.iY; }
TRect::TRect(const TSize& aSize)
    {
    iTl.iX = 0; iTl.iY = 0;
    iBr.iX = aSize.iWidth; iBr.iY = aSize.iHeight;
    }
TAny* User::AllocZ(TInt aSize)                      { return EdispAlloc(aSize); }
CBase::~CBase()                                     { }
TInt CBase::Extension_(TUint, TAny*&, TAny*)        { return KErrNotSupported; }
TPtr8::TPtr8(TUint8* aBuf, TInt aLength, TInt aMaxLength)
    {
    // TDesC8/iLength and TDes8/iMaxLength are private; i386 layout is
    // iLength@0 (TUint), iMaxLength@4 (TInt), iPtr@8 (TPtr8's own member).
    *(TUint*)((char*)this + 0) = (TUint)aLength;
    *(TInt*)((char*)this + 4)  = aMaxLength;
    iPtr = aBuf;
    }

// vtable references this (weak) symbol; give it a local definition so the
// PE2 packer sees no undefined relocations.
extern "C" void __cxa_pure_virtual() { for (;;) { } }

// link_dll.ld ENTRY(_start): a DLL has no process entry point.
extern "C" void _start() { }
#endif
