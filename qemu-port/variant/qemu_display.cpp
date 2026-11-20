// qemu_display.cpp — real graphics framebuffer + display HAL for the QEMU port.
//
// Today the port only has an 80x25 VGA text console at 0xB8000.  This driver
// adds a linear framebuffer so the OS can actually draw pixels.  It sets a
// VESA-mode-sized framebuffer through the Bochs VBE DISPI register interface
// (I/O 0x1CE/0x1CF) which QEMU's standard VGA implements.  This is the exact
// register path a real-mode `int 10h AX=4F02h` VBE BIOS call programs, but it
// needs no 16-bit/real-mode thunk and is therefore safe from protected mode
// with the flat 4G identity map already enabled by HwInit0.
//
// Primary mode: 1024x768x32bpp (the geometry of VBE mode 0x118).  Fallback:
// legacy VGA mode 13h (320x200x8 at 0xA0000) for non-Bochs VBE VGA cards.
// The LFB physical base is read from the VGA PCI BAR (SeaBIOS assigns
// 0xE0000000 on q35); 0xE0000000 is used as a last-resort default.
//
// Public API (qemu_variant.h): display_init / display_info / display_clear /
// display_rect / display_putc / display_test / QemuDisplayRegisterHal.
// A static constructor makes display_init() idempotent at boot.

#include <x86.h>
#include <e32hal.h>
#include <e32err.h>
#include <nk_trace.h>
#include <kernel/kernel.h>
#include "qemu_variant.h"

namespace
    {
    // Bochs VBE DISPI registers (QEMU "-vga std").
    enum
        {
        kDispiIndexPort = 0x1CEu,
        kDispiDataPort  = 0x1CFu,
        kIdxId          = 0u,
        kIdxXRes        = 1u,
        kIdxYRes        = 2u,
        kIdxBpp         = 3u,
        kIdxEnable      = 4u,
        kIdxBank        = 5u,
        kIdxVirtWidth   = 6u,
        kIdxYOffset     = 13u,
        kDispiIdMin     = 0xB0C0u,
        kDispiIdMax     = 0xB0CFu,
        kEnEnable       = 0x01u,
        kEnLfb          = 0x40u,
        kEnNoClear      = 0x20u,
        kLfbDefault     = 0xE0000000u
        };

    TUint8* volatile gLfb = 0;   // linear framebuffer (identity-mapped phys)
    TUint gWidth = 0, gHeight = 0, gBpp = 0, gPitch = 0;
    TUint gMode = 0;             // VBE mode actually configured
    TUint gDispiId = 0;          // 0 when the fallback path was used
    TBool gReady = EFalse;
    TBool gTried = EFalse;

    inline void Out8(unsigned short aPort, TUint8 aVal)
        { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
    inline TUint8 In8(unsigned short aPort)
        { TUint8 v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }
    inline void Out16(unsigned short aPort, TUint16 aVal)
        { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
    inline TUint16 In16(unsigned short aPort)
        { TUint16 v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

    void DispiWrite(TUint aIdx, TUint aVal)
        { Out16(kDispiIndexPort, (TUint16)aIdx); Out16(kDispiDataPort, (TUint16)aVal); }
    TUint16 DispiRead(TUint aIdx)
        { Out16(kDispiIndexPort, (TUint16)aIdx); return In16(kDispiDataPort); }

    // SeaBIOS puts the standard VGA LFB in a prefetchable 32-bit PCI BAR whose
    // base lands in the high half.  Pick that; fall back to QEMU's known LFB.
    TUint32 VgaLfbBase()
        {
        TUint bus = 0, dev = 0, fn = 0;
        if (!PciFindClass(0x03u, 0x00u, &bus, &dev, &fn))
            return kLfbDefault;
        for (TUint reg = 0x10u; reg <= 0x24u; reg += 4u)
            {
            TUint32 bar = PciRead32(bus, dev, fn, reg);
            if (bar & 1u)              // I/O BAR
                continue;
            TUint32 base = bar & 0xFFFFFFF0u;
            if (base >= 0x80000000u && base != 0xFFFFFFF0u)
                return base;
            }
        return kLfbDefault;
        }

    // Legacy VGA mode 13h register tables (320x200x8 at phys 0xA0000).  Used
    // only when Bochs VBE DISPI is unavailable (non-std VGA).
    const TUint8 kSeq13[]  = {0x00,0x01,0x0F,0x00,0x0E};
    const TUint8 kCrtc13[] = {0x5F,0x4F,0x4F,0x50,0x52,0x0F,0x0B,0x3E,
                              0x00,0x4F,0x0D,0x0E,0x00,0x00,0x00,0x00,
                              0x9C,0x8F,0x14,0x9C,0x00,0x96,0xB9,0xA2,0xFF};
    const TUint8 kGc13[]   = {0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0F,0xFF};

    void SetMode13()
        {
        // Unlock CRTC registers 0-7.
        Out8(0x3D4, 0x11); Out8(0x3D5, (TUint8)(In8(0x3D5) & ~0x80u));
        for (TUint i = 0; i < sizeof(kSeq13); ++i)
            { Out8(0x3C4, (TUint8)i);  Out8(0x3C5, kSeq13[i]); }
        for (TUint i = 0; i < sizeof(kCrtc13); ++i)
            { Out8(0x3D4, (TUint8)i);  Out8(0x3D5, kCrtc13[i]); }
        for (TUint i = 0; i < sizeof(kGc13); ++i)
            { Out8(0x3CE, (TUint8)i);  Out8(0x3CF, kGc13[i]); }
        for (TUint i = 0; i < 16; ++i)
            { Out8(0x3C0, (TUint8)i);  Out8(0x3C0, (TUint8)i); }
        Out8(0x3C0, 0x41); Out8(0x3C0, 0x00);  // 64-color palette, video on
        Out8(0x3C0, 0x20);                     // enable attribute palette
        Out8(0x3C2, 0x63);                     // 70Hz, 8-bit, colour
        Out8(0x3D4, 0x11); Out8(0x3D5, (TUint8)(In8(0x3D5) | 0x80u));
        }

    // Minimal 5x7 stroke font -> rendered into 8x8 cells with a 1px margin.
    struct TGlyph { TUint8 iCh; TUint8 iRows[7]; };
    static const TGlyph kGlyphs[] = {
        {'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
        {'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
        {'C',{0x0F,0x10,0x10,0x10,0x10,0x10,0x0F}},
        {'D',{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
        {'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
        {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
        {'G',{0x0F,0x10,0x10,0x17,0x11,0x11,0x0F}},
        {'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
        {'I',{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},
        {'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
        {'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
        {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
        {'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
        {'N',{0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
        {'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
        {'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
        {'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}},
        {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
        {'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},
        {'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
        {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
        {'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},
        {'W',{0x11,0x11,0x11,0x15,0x15,0x1B,0x11}},
        {'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
        {'Y',{0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},
        {'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
        {'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
        {'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
        {'2',{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},
        {'3',{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},
        {'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
        {'5',{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
        {'6',{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
        {'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
        {'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
        {'9',{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
        {'-',{0x00,0x00,0x00,0x1F,0x00,0x00,0x00}},
        {'.',{0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}},
        {':',{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}},
        {'/',{0x01,0x02,0x02,0x04,0x08,0x08,0x10}},
        };

    const TUint8* Glyph(TUint8 aCh)
        {
        if (aCh >= 'a' && aCh <= 'z') aCh = (TUint8)(aCh - 32);
        for (TUint i = 0; i < sizeof(kGlyphs) / sizeof(kGlyphs[0]); ++i)
            if (kGlyphs[i].iCh == aCh)
                return kGlyphs[i].iRows;
        return 0;   // space / unsupported -> blank
        }

    void PutPixel(TUint aX, TUint aY, TUint aColor)
        {
        if (!gLfb || aX >= gWidth || aY >= gHeight)
            return;
        volatile TUint8* p = gLfb + aY * gPitch + aX * (gBpp >> 3);
        if (gBpp == 32)
            *(volatile TUint32*)p = (TUint32)aColor;   // 0x00RRGGBB -> B,G,R,X
        else if (gBpp == 24)
            { p[0] = (TUint8)aColor; p[1] = (TUint8)(aColor >> 8); p[2] = (TUint8)(aColor >> 16); }
        else if (gBpp == 16)
            *(volatile TUint16*)p = (TUint16)aColor;
        else
            *p = (TUint8)aColor;                       // 8bpp palette index
        }

    TUint GetPixel(TUint aX, TUint aY)
        {
        if (!gLfb || aX >= gWidth || aY >= gHeight)
            return 0;
        const volatile TUint8* p = gLfb + aY * gPitch + aX * (gBpp >> 3);
        if (gBpp == 32)
            return *(const volatile TUint32*)p;
        if (gBpp == 16)
            return *(const volatile TUint16*)p;
        if (gBpp == 24)
            return (TUint)p[0] | ((TUint)p[1] << 8) | ((TUint)p[2] << 16);
        return *p;
        }
    }

TInt display_init()
    {
    if (gReady)
        return KErrNone;
    if (gTried)
        return gReady ? KErrNone : KErrNotSupported;
    gTried = ETrue;

    gDispiId = DispiRead(kIdxId);
    if (gDispiId >= kDispiIdMin && gDispiId <= kDispiIdMax)
        {
        // Disable, program geometry, then enable with LFB + no-clear.
        DispiWrite(kIdxEnable, 0);
        DispiWrite(kIdxXRes, 1024);
        DispiWrite(kIdxYRes, 768);
        DispiWrite(kIdxBpp, 32);
        DispiWrite(kIdxBank, 0);
        DispiWrite(kIdxVirtWidth, 1024);
        DispiWrite(kIdxYOffset, 0);
        DispiWrite(kIdxEnable, kEnEnable | kEnLfb | kEnNoClear);

        if (DispiRead(kIdxXRes) == 1024 && DispiRead(kIdxYRes) == 768 &&
            DispiRead(kIdxBpp) == 32)
            {
            gLfb = (TUint8* volatile)VgaLfbBase();
            gWidth = 1024; gHeight = 768; gBpp = 32;
            gPitch = gWidth * 4; gMode = 0x118; gReady = ETrue;
            }
        }
    if (!gReady)
        {
        // Fallback: legacy VGA mode 13h (works on any VGA, no VBE needed).
        SetMode13();
        gLfb = (TUint8* volatile)0xA0000u;
        gWidth = 320; gHeight = 200; gBpp = 8;
        gPitch = 320; gMode = 0x13; gDispiId = 0; gReady = ETrue;
        }
    KPrintf("DISPLAY: VBE mode 0x%x set (Bochs DISPI id=0x%x, fallback=%d)\r\n",
        gMode, gDispiId, gDispiId ? 0 : 1);
    KPrintf("DISPLAY: LFB base=0x%08x %ux%ux%u pitch=%u\r\n",
        (TUint)gLfb, gWidth, gHeight, gBpp, gPitch);
    return KErrNone;
    }

void display_info()
    {
    TInt r = display_init();
    if (r != KErrNone || !gReady)
        { KPrintf("display: no framebuffer (r=%d)\r\n", r); return; }
    KPrintf("display: framebuffer LFB=0x%08x mode=0x%x %ux%ux%u pitch=%u\r\n",
        (TUint)gLfb, gMode, gWidth, gHeight, gBpp, gPitch);
    KPrintf("display: VBE DISPI id=0x%x (%s), %s\r\n", gDispiId,
        gDispiId ? "Bochs VBE" : "legacy VGA",
        gBpp == 32 ? "32bpp RGBX" : "8bpp palette");
    }

void display_clear(TUint aColor)
    {
    if (!gReady || !gLfb)
        return;
    for (TUint y = 0; y < gHeight; ++y)
        for (TUint x = 0; x < gWidth; ++x)
            PutPixel(x, y, aColor);
    }

void display_rect(TUint aX, TUint aY, TUint aW, TUint aH, TUint aColor)
    {
    if (!gReady || !gLfb)
        return;
    TUint x1 = aX + aW, y1 = aY + aH;
    if (x1 > gWidth) x1 = gWidth;
    if (y1 > gHeight) y1 = gHeight;
    for (TUint y = aY; y < y1; ++y)
        for (TUint x = aX; x < x1; ++x)
            PutPixel(x, y, aColor);
    }

void display_putc(TUint aX, TUint aY, char aCh, TUint aFg, TUint aBg)
    {
    if (!gReady)
        return;
    const TUint8* g = Glyph((TUint8)aCh);
    for (TUint row = 0; row < 8; ++row)
        {
        TUint8 bits = (g && row < 7) ? (TUint8)(g[row] << 2) : 0;
        for (TUint col = 0; col < 8; ++col)
            PutPixel(aX + col, aY + row, (bits & (1u << (7 - col))) ? aFg : aBg);
        }
    }

TInt display_test()
    {
    TInt r = display_init();
    if (r != KErrNone || !gReady)
        { KPrintf("DISPLAY TEST: no framebuffer (r=%d)\r\n", r); return r; }

    // Blue background, green border, white-on-blue banner.
    display_clear(0x00000080u);
    display_rect(8, 8, gWidth - 16, 4, 0x0000FF00u);
    display_rect(8, gHeight - 12, gWidth - 16, 4, 0x0000FF00u);
    display_rect(8, 8, 4, gHeight - 16, 0x0000FF00u);
    display_rect(gWidth - 12, 8, 4, gHeight - 16, 0x0000FF00u);
    const char* s = "EKA2 GUI READY";
    for (TUint i = 0; s[i]; ++i)
        display_putc(40 + i * 9, 40, s[i], 0x00FFFFFFu, 0x00000080u);

    KPrintf("DISPLAY TEST: cleared blue, border + text drawn on %ux%ux%u LFB=0x%08x\r\n",
        gWidth, gHeight, gBpp, (TUint)gLfb);
    // Read straight back from the LFB: proves real VRAM writes, not just I/O.
    TUint bg = GetPixel(0, 0);      // inside the border -> blue
    TUint bd = GetPixel(10, 10);   // on the top border -> green
    KPrintf("DISPLAY TEST: pixel readback bg=0x%06x border=0x%06x %s\r\n",
        bg, bd, (bg == 0x00000080u && bd == 0x0000FF00u) ? "OK" : "MISMATCH");
    KPrintf("DISPLAY TEST: EKA2-GUI-READY framebuffer OK\r\n");
    return KErrNone;
    }

// Read-only accessors for the GUI desktop shell (variant/qemu_gui_exec.cpp).
// They expose the already-initialised framebuffer to kernel-side exec handlers;
// userland reaches them only through the dedicated slow execs, never directly
// (the LFB sits in the U/S=0 high half of the flat PDE map).
TUint32 display_lfb()
    { display_init(); return (TUint32)gLfb; }

TUint display_fb_size()
    { display_init(); return gReady ? gWidth * gHeight * (gBpp >> 3) : 0; }

void display_geometry(TUint& aW, TUint& aH, TUint& aBpp)
    { display_init(); aW = gWidth; aH = gHeight; aBpp = gBpp; }

TUint display_getpixel(TUint aX, TUint aY)
    { return gReady ? GetPixel(aX, aY) : 0; }

// The display HAL handler lets user-side UserSvr::ScreenInfo / HalFunction
// reach the framebuffer.  Registered from Asic::Init3 (after K::Init3 has
// allocated K::HalEntryArray) via QemuDisplayRegisterHal().
static TInt DisplayHalFunction(TAny*, TInt aFunction, TAny* a1, TAny*)
    {
    if (aFunction == EDisplayHalScreenInfo)
        {
        // Mirrors TScreenInfoV01's 24-byte layout exactly (avoids pulling the
        // private e32event_private.h header into this freestanding driver).
        struct TQScreenInfo
            {
            TInt  iWindowHandleValid;
            TAny* iWindowHandle;
            TInt  iScreenAddressValid;
            TAny* iScreenAddress;
            TInt  iWidth;
            TInt  iHeight;
            };
        TQScreenInfo info;
        for (TUint i = 0; i < sizeof(info); ++i)
            ((TUint8*)&info)[i] = 0;
        info.iScreenAddressValid = ETrue;
        info.iScreenAddress = (TAny*)gLfb;
        info.iWidth = (TInt)gWidth;
        info.iHeight = (TInt)gHeight;
        Kern::InfoCopy(*(TDes8*)a1, TPtrC8((const TUint8*)&info, sizeof(info)));
        return KErrNone;
        }
    if (aFunction == EDisplayHalWsRegisterSwitchOnScreenHandling)
        return KErrNone;
    return KErrNotSupported;
    }

void QemuDisplayRegisterHal()
    {
    TInt r = Kern::AddHalEntry(EHalGroupDisplay, DisplayHalFunction, 0);
    KPrintf("DISPLAY: EHalGroupDisplay(4) registered r=%d\r\n", r);
    }

// Boot hook: HwInit0 walks .init_array after paging + ROM are ready.  Safe and
// idempotent (display_init guards on gReady/gTried).
__attribute__((constructor)) static void DisplayBootInit()
    { display_init(); }
