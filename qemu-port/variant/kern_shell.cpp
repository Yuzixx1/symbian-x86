// kern_shell.cpp — VGA + PS/2 + EKA2> shell for the full Kern image.
//
// Adapted from the Phase-10 genuine_console.cpp (proven Set-1 map, VGA
// teletype, line editor), minus everything the genuine kernel now owns:
// no NKern::Init, no IDT surgery (genuine X86::Init1Interrupts owns the IDT),
// no KPrintf (genuine cutils.cia -> Asic::DebugOutput, which prints here).
// The shell runs as a plain genuine NThread spawned from Asic::Init3.

#define __INCLUDE_NTHREADBASE_DEFINES__
#include <x86.h>
#include <nk_trace.h>
#include <kernel/kern_priv.h>
#include <memmodel.h>
#include "qemu_variant.h"
#include "bootinfo.h"

static inline void Out8(unsigned short aPort, unsigned char aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
static inline unsigned char In8(unsigned short aPort)
    { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }
static inline void Out16(unsigned short aPort, unsigned short aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
static inline unsigned short In16(unsigned short aPort)
    { unsigned short v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

namespace QemuUart
    {
    void PutC(char aC);
    }

void VgaTeletype(char aC);
static void VgaInit();

// exec-268 hand-off state.  While a full-screen GUI program owns the LFB the
// mirror is muted; a NON-graphics child launched by that GUI program (DESKTOP
// -> CALC/FILEMGR/...) must get the display back.  More than one graphics app
// may own the screen at once (WSDL windowed client and any GUI child), so
// track EVERY live GUI owner's pid plus the shell's own pid.  The mirror is
// resumed only once ALL graphics owners die -- never merely because a second
// graphics app was launched.
static const TInt KMaxGuiMirrorPids = 4;
static TUint sGuiMirrorPids[KMaxGuiMirrorPids] = {0, 0, 0, 0};
static TInt  sGuiMirrorCount = 0;
static TUint sMirrorShellPid = 0;
static volatile TBool sChildTookMirror = EFalse;

static TBool IsGuiMirrorOwner(TUint aPid)
    {
    for (TInt i = 0; i < KMaxGuiMirrorPids; ++i)
        if (sGuiMirrorPids[i] && sGuiMirrorPids[i] == aPid) return ETrue;
    return EFalse;
    }

static void AddGuiMirrorOwner(TUint aPid)
    {
    if (!aPid || IsGuiMirrorOwner(aPid)) return;
    for (TInt i = 0; i < KMaxGuiMirrorPids; ++i)
        if (!sGuiMirrorPids[i])
            { sGuiMirrorPids[i] = aPid; ++sGuiMirrorCount; return; }
    }

static TInt ReapGuiMirrorOwners()
    {
    TInt reaped = 0;
    for (TInt i = 0; i < KMaxGuiMirrorPids; ++i)
        {
        if (sGuiMirrorPids[i] && E32ProcessDead(sGuiMirrorPids[i]))
            { sGuiMirrorPids[i] = 0; --sGuiMirrorCount; ++reaped; }
        }
    return reaped;
    }

// Raw char output: COM1 + VGA with NO line ending. Genuine Kern::Printf
// appends \r\n per call (A::DebugPrint), so per-char echo must bypass it
// or the prompt/input stacks vertically.
static void EmitRaw(char aC)
    {
    QemuUart::PutC(aC);
    VgaTeletype(aC);
    }

static void EmitRawS(const char* aS)
    {
    while (*aS) EmitRaw(*aS++);
    }

// Backspace must actually erase on every output path. A lone 0x08 only
// moves the cursor on a serial terminal (leaving the glyph visible), so
// follow it with a space to blank the cell and a second 0x08 to step back.
// VgaTeletype/FbPutC also blank the glyph on 0x08 for the VGA/LFB mirrors.
static void EmitBackspace()
    {
    EmitRaw('\b');
    EmitRaw(' ');
    EmitRaw('\b');
    }

// ---------------------------------------------------------------- VGA teletype
static volatile unsigned short* const kVram =
    reinterpret_cast<volatile unsigned short*>(0xB8000u);
static const TUint kCols = 80, kRows = 25, kAttr = 0x07;

// Boot console mode. TRUE = genuine BIOS VGA mode 3 (80x25 text at 0xB8000),
// which is how GRUB's `gfxpayload=text` leaves the hardware. In that state the
// console writes ONLY to kVram; no display_* accessor may be called from the
// output path, because every one of them calls display_init() and would switch
// the hardware to the 1024x768 LFB behind our back. A graphical E32 launch
// flips this to FALSE via EnsureVbeGraphics(); from then on the existing LFB
// text mirror is kept. There is no real-mode int10 thunk to switch all the way
// back to text after a GUI app exits, so the flag deliberately stays FALSE:
// console text then goes to both the (dead) 0xB8000 buffer and the LFB, exactly
// as before -- harmless, and the LFB remains the visible console.
static volatile TBool gTextMode = ETrue;

namespace
    {
    TUint sCol = 0, sRow = 0;

    void MoveCursor()
        {
        TUint pos = sRow * kCols + sCol;
        Out8(0x3D4, 0x0F);
        Out8(0x3D5, (unsigned char)(pos & 0xFF));
        Out8(0x3D4, 0x0E);
        Out8(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
        }

    void Scroll()
        {
        for (TUint r = 1; r < kRows; ++r)
            for (TUint c = 0; c < kCols; ++c)
                kVram[(r - 1) * kCols + c] = kVram[r * kCols + c];
        for (TUint c = 0; c < kCols; ++c)
            kVram[(kRows - 1) * kCols + c] = (unsigned short)((kAttr << 8) | ' ');
        }

    void NewLine()
        {
        sCol = 0;
        if (++sRow >= kRows)
            {
            sRow = kRows - 1;
            Scroll();
            }
        MoveCursor();
        }

    // Bochs VBE DISPI registers (QEMU "-vga std"). Programming them is the
    // protected-mode equivalent of int 10h AX=4F02h; used here only to assert
    // the LFB when a graphical app starts and to park it while at the text
    // prompt. qemu_display.cpp keeps its own copy of the same register table.
    inline void DispiWrite(unsigned short aIdx, unsigned short aVal)
        { Out16(0x1CEu, aIdx); Out16(0x1CFu, aVal); }

    // Standard IBM PC VGA 8x16 font: 256 glyphs x 16 bytes = 4096 bytes.
    // Source: Linux lib/fonts/font_8x16.c (fontdata_8x16, generated by
    // cpi2fnt from the IBM VGA Code Page 437 font). Scanline i of glyph N
    // is byte [N*16 + i]; QEMU's vga_draw_text() reads it from VRAM plane 2
    // at planar address N*32 + i (font_base = vram+2+SR3 map offset, 128
    // bytes per glyph, bitmap byte at font_ptr[4*i]).
    static const TUint8 kFont8x16[256 * 16] =
        {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x00
        0x00,0x00,0x7E,0x81,0xA5,0x81,0x81,0xBD,0x99,0x81,0x81,0x7E,0x00,0x00,0x00,0x00,  // 0x01
        0x00,0x00,0x7E,0xFF,0xDB,0xFF,0xFF,0xC3,0xE7,0xFF,0xFF,0x7E,0x00,0x00,0x00,0x00,  // 0x02
        0x00,0x00,0x00,0x00,0x6C,0xFE,0xFE,0xFE,0xFE,0x7C,0x38,0x10,0x00,0x00,0x00,0x00,  // 0x03
        0x00,0x00,0x00,0x00,0x10,0x38,0x7C,0xFE,0x7C,0x38,0x10,0x00,0x00,0x00,0x00,0x00,  // 0x04
        0x00,0x00,0x00,0x18,0x3C,0x3C,0xE7,0xE7,0xE7,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x05
        0x00,0x00,0x00,0x18,0x3C,0x7E,0xFF,0xFF,0x7E,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x06
        0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x3C,0x3C,0x18,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x07
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xE7,0xC3,0xC3,0xE7,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x08
        0x00,0x00,0x00,0x00,0x00,0x3C,0x66,0x42,0x42,0x66,0x3C,0x00,0x00,0x00,0x00,0x00,  // 0x09
        0xFF,0xFF,0xFF,0xFF,0xFF,0xC3,0x99,0xBD,0xBD,0x99,0xC3,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0x0A
        0x00,0x00,0x1E,0x0E,0x1A,0x32,0x78,0xCC,0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00,  // 0x0B
        0x00,0x00,0x3C,0x66,0x66,0x66,0x66,0x3C,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,  // 0x0C
        0x00,0x00,0x3F,0x33,0x3F,0x30,0x30,0x30,0x30,0x70,0xF0,0xE0,0x00,0x00,0x00,0x00,  // 0x0D
        0x00,0x00,0x7F,0x63,0x7F,0x63,0x63,0x63,0x63,0x67,0xE7,0xE6,0xC0,0x00,0x00,0x00,  // 0x0E
        0x00,0x00,0x00,0x18,0x18,0xDB,0x3C,0xE7,0x3C,0xDB,0x18,0x18,0x00,0x00,0x00,0x00,  // 0x0F
        0x00,0x80,0xC0,0xE0,0xF0,0xF8,0xFE,0xF8,0xF0,0xE0,0xC0,0x80,0x00,0x00,0x00,0x00,  // 0x10
        0x00,0x02,0x06,0x0E,0x1E,0x3E,0xFE,0x3E,0x1E,0x0E,0x06,0x02,0x00,0x00,0x00,0x00,  // 0x11
        0x00,0x00,0x18,0x3C,0x7E,0x18,0x18,0x18,0x7E,0x3C,0x18,0x00,0x00,0x00,0x00,0x00,  // 0x12
        0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x66,0x66,0x00,0x00,0x00,0x00,  // 0x13
        0x00,0x00,0x7F,0xDB,0xDB,0xDB,0x7B,0x1B,0x1B,0x1B,0x1B,0x1B,0x00,0x00,0x00,0x00,  // 0x14
        0x00,0x7C,0xC6,0x60,0x38,0x6C,0xC6,0xC6,0x6C,0x38,0x0C,0xC6,0x7C,0x00,0x00,0x00,  // 0x15
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0xFE,0xFE,0xFE,0x00,0x00,0x00,0x00,  // 0x16
        0x00,0x00,0x18,0x3C,0x7E,0x18,0x18,0x18,0x7E,0x3C,0x18,0x7E,0x00,0x00,0x00,0x00,  // 0x17
        0x00,0x00,0x18,0x3C,0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00,  // 0x18
        0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x3C,0x18,0x00,0x00,0x00,0x00,  // 0x19
        0x00,0x00,0x00,0x00,0x00,0x18,0x0C,0xFE,0x0C,0x18,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x1A
        0x00,0x00,0x00,0x00,0x00,0x30,0x60,0xFE,0x60,0x30,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x1B
        0x00,0x00,0x00,0x00,0x00,0x00,0xC0,0xC0,0xC0,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x1C
        0x00,0x00,0x00,0x00,0x00,0x28,0x6C,0xFE,0x6C,0x28,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x1D
        0x00,0x00,0x00,0x00,0x10,0x38,0x38,0x7C,0x7C,0xFE,0xFE,0x00,0x00,0x00,0x00,0x00,  // 0x1E
        0x00,0x00,0x00,0x00,0xFE,0xFE,0x7C,0x7C,0x38,0x38,0x10,0x00,0x00,0x00,0x00,0x00,  // 0x1F
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x20
        0x00,0x00,0x18,0x3C,0x3C,0x3C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00,  // 0x21
        0x00,0x66,0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x22
        0x00,0x00,0x00,0x6C,0x6C,0xFE,0x6C,0x6C,0x6C,0xFE,0x6C,0x6C,0x00,0x00,0x00,0x00,  // 0x23
        0x18,0x18,0x7C,0xC6,0xC2,0xC0,0x7C,0x06,0x06,0x86,0xC6,0x7C,0x18,0x18,0x00,0x00,  // 0x24
        0x00,0x00,0x00,0x00,0xC2,0xC6,0x0C,0x18,0x30,0x60,0xC6,0x86,0x00,0x00,0x00,0x00,  // 0x25
        0x00,0x00,0x38,0x6C,0x6C,0x38,0x76,0xDC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x26
        0x00,0x30,0x30,0x30,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x27
        0x00,0x00,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0x00,0x00,0x00,0x00,  // 0x28
        0x00,0x00,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0x00,0x00,0x00,0x00,  // 0x29
        0x00,0x00,0x00,0x00,0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x2A
        0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x2B
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x18,0x30,0x00,0x00,0x00,  // 0x2C
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x2D
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,  // 0x2E
        0x00,0x00,0x00,0x00,0x02,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00,0x00,0x00,0x00,  // 0x2F
        0x00,0x00,0x38,0x6C,0xC6,0xC6,0xD6,0xD6,0xC6,0xC6,0x6C,0x38,0x00,0x00,0x00,0x00,  // 0x30
        0x00,0x00,0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0x00,0x00,0x00,0x00,  // 0x31
        0x00,0x00,0x7C,0xC6,0x06,0x0C,0x18,0x30,0x60,0xC0,0xC6,0xFE,0x00,0x00,0x00,0x00,  // 0x32
        0x00,0x00,0x7C,0xC6,0x06,0x06,0x3C,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x33
        0x00,0x00,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x0C,0x1E,0x00,0x00,0x00,0x00,  // 0x34
        0x00,0x00,0xFE,0xC0,0xC0,0xC0,0xFC,0x06,0x06,0x06,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x35
        0x00,0x00,0x38,0x60,0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x36
        0x00,0x00,0xFE,0xC6,0x06,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0x00,0x00,0x00,0x00,  // 0x37
        0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x38
        0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7E,0x06,0x06,0x06,0x0C,0x78,0x00,0x00,0x00,0x00,  // 0x39
        0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,  // 0x3A
        0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x18,0x18,0x30,0x00,0x00,0x00,0x00,  // 0x3B
        0x00,0x00,0x00,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0x00,0x00,0x00,0x00,  // 0x3C
        0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x3D
        0x00,0x00,0x00,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x60,0x00,0x00,0x00,0x00,  // 0x3E
        0x00,0x00,0x7C,0xC6,0xC6,0x0C,0x18,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00,  // 0x3F
        0x00,0x00,0x00,0x7C,0xC6,0xC6,0xDE,0xDE,0xDE,0xDC,0xC0,0x7C,0x00,0x00,0x00,0x00,  // 0x40
        0x00,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00,  // 0x41
        0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0x66,0xFC,0x00,0x00,0x00,0x00,  // 0x42
        0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xC0,0xC0,0xC2,0x66,0x3C,0x00,0x00,0x00,0x00,  // 0x43
        0x00,0x00,0xF8,0x6C,0x66,0x66,0x66,0x66,0x66,0x66,0x6C,0xF8,0x00,0x00,0x00,0x00,  // 0x44
        0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00,  // 0x45
        0x00,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00,  // 0x46
        0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xDE,0xC6,0xC6,0x66,0x3A,0x00,0x00,0x00,0x00,  // 0x47
        0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00,  // 0x48
        0x00,0x00,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x49
        0x00,0x00,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0xCC,0xCC,0xCC,0x78,0x00,0x00,0x00,0x00,  // 0x4A
        0x00,0x00,0xE6,0x66,0x66,0x6C,0x78,0x78,0x6C,0x66,0x66,0xE6,0x00,0x00,0x00,0x00,  // 0x4B
        0x00,0x00,0xF0,0x60,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xFE,0x00,0x00,0x00,0x00,  // 0x4C
        0x00,0x00,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00,  // 0x4D
        0x00,0x00,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00,  // 0x4E
        0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x4F
        0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00,  // 0x50
        0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x0C,0x0E,0x00,0x00,  // 0x51
        0x00,0x00,0xFC,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00,  // 0x52
        0x00,0x00,0x7C,0xC6,0xC6,0x60,0x38,0x0C,0x06,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x53
        0x00,0x00,0x7E,0x7E,0x5A,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x54
        0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x55
        0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00,0x00,0x00,0x00,  // 0x56
        0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xD6,0xD6,0xD6,0xFE,0xEE,0x6C,0x00,0x00,0x00,0x00,  // 0x57
        0x00,0x00,0xC6,0xC6,0x6C,0x7C,0x38,0x38,0x7C,0x6C,0xC6,0xC6,0x00,0x00,0x00,0x00,  // 0x58
        0x00,0x00,0x66,0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x59
        0x00,0x00,0xFE,0xC6,0x86,0x0C,0x18,0x30,0x60,0xC2,0xC6,0xFE,0x00,0x00,0x00,0x00,  // 0x5A
        0x00,0x00,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0x00,0x00,0x00,0x00,  // 0x5B
        0x00,0x00,0x00,0x80,0xC0,0xE0,0x70,0x38,0x1C,0x0E,0x06,0x02,0x00,0x00,0x00,0x00,  // 0x5C
        0x00,0x00,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00,0x00,0x00,0x00,  // 0x5D
        0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x5E
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,  // 0x5F
        0x00,0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x60
        0x00,0x00,0x00,0x00,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x61
        0x00,0x00,0xE0,0x60,0x60,0x78,0x6C,0x66,0x66,0x66,0x66,0x7C,0x00,0x00,0x00,0x00,  // 0x62
        0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x63
        0x00,0x00,0x1C,0x0C,0x0C,0x3C,0x6C,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x64
        0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x65
        0x00,0x00,0x1C,0x36,0x32,0x30,0x78,0x30,0x30,0x30,0x30,0x78,0x00,0x00,0x00,0x00,  // 0x66
        0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0xCC,0x78,0x00,  // 0x67
        0x00,0x00,0xE0,0x60,0x60,0x6C,0x76,0x66,0x66,0x66,0x66,0xE6,0x00,0x00,0x00,0x00,  // 0x68
        0x00,0x00,0x18,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x69
        0x00,0x00,0x06,0x06,0x00,0x0E,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0x00,  // 0x6A
        0x00,0x00,0xE0,0x60,0x60,0x66,0x6C,0x78,0x78,0x6C,0x66,0xE6,0x00,0x00,0x00,0x00,  // 0x6B
        0x00,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x6C
        0x00,0x00,0x00,0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xD6,0xD6,0xC6,0x00,0x00,0x00,0x00,  // 0x6D
        0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00,  // 0x6E
        0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x6F
        0x00,0x00,0x00,0x00,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00,  // 0x70
        0x00,0x00,0x00,0x00,0x00,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0x0C,0x1E,0x00,  // 0x71
        0x00,0x00,0x00,0x00,0x00,0xDC,0x76,0x66,0x60,0x60,0x60,0xF0,0x00,0x00,0x00,0x00,  // 0x72
        0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x73
        0x00,0x00,0x10,0x30,0x30,0xFC,0x30,0x30,0x30,0x30,0x36,0x1C,0x00,0x00,0x00,0x00,  // 0x74
        0x00,0x00,0x00,0x00,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x75
        0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x00,0x00,0x00,0x00,  // 0x76
        0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xD6,0xD6,0xD6,0xFE,0x6C,0x00,0x00,0x00,0x00,  // 0x77
        0x00,0x00,0x00,0x00,0x00,0xC6,0x6C,0x38,0x38,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00,  // 0x78
        0x00,0x00,0x00,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7E,0x06,0x0C,0xF8,0x00,  // 0x79
        0x00,0x00,0x00,0x00,0x00,0xFE,0xCC,0x18,0x30,0x60,0xC6,0xFE,0x00,0x00,0x00,0x00,  // 0x7A
        0x00,0x00,0x0E,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x18,0x0E,0x00,0x00,0x00,0x00,  // 0x7B
        0x00,0x00,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00,  // 0x7C
        0x00,0x00,0x70,0x18,0x18,0x18,0x0E,0x18,0x18,0x18,0x18,0x70,0x00,0x00,0x00,0x00,  // 0x7D
        0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0x7E
        0x00,0x00,0x00,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xC6,0xFE,0x00,0x00,0x00,0x00,0x00,  // 0x7F
        0x00,0x00,0x3C,0x66,0xC2,0xC0,0xC0,0xC0,0xC0,0xC2,0x66,0x3C,0x18,0x70,0x00,0x00,  // 0x80
        0x00,0x00,0xCC,0x00,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x81
        0x00,0x0C,0x18,0x30,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x82
        0x00,0x10,0x38,0x6C,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x83
        0x00,0x00,0xCC,0x00,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x84
        0x00,0x60,0x30,0x18,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x85
        0x00,0x38,0x6C,0x38,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x86
        0x00,0x00,0x00,0x00,0x00,0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x18,0x70,0x00,0x00,  // 0x87
        0x00,0x10,0x38,0x6C,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x88
        0x00,0x00,0xC6,0x00,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x89
        0x00,0x60,0x30,0x18,0x00,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x8A
        0x00,0x00,0x66,0x00,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x8B
        0x00,0x18,0x3C,0x66,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x8C
        0x00,0x60,0x30,0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0x8D
        0x00,0xC6,0x00,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00,  // 0x8E
        0x38,0x6C,0x38,0x10,0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00,  // 0x8F
        0x0C,0x18,0x00,0xFE,0x66,0x62,0x68,0x78,0x68,0x62,0x66,0xFE,0x00,0x00,0x00,0x00,  // 0x90
        0x00,0x00,0x00,0x00,0x00,0xEC,0x36,0x36,0x7E,0xD8,0xD8,0x6E,0x00,0x00,0x00,0x00,  // 0x91
        0x00,0x00,0x3E,0x6C,0xCC,0xCC,0xFE,0xCC,0xCC,0xCC,0xCC,0xCE,0x00,0x00,0x00,0x00,  // 0x92
        0x00,0x10,0x38,0x6C,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x93
        0x00,0x00,0xC6,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x94
        0x00,0x60,0x30,0x18,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x95
        0x00,0x30,0x78,0xCC,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x96
        0x00,0x60,0x30,0x18,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0x97
        0x00,0x00,0xC6,0x00,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00,  // 0x98
        0x00,0xC6,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x99
        0x00,0xC6,0x00,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0x9A
        0x00,0x18,0x18,0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x18,0x18,0x00,0x00,0x00,0x00,  // 0x9B
        0x00,0x38,0x6C,0x64,0x60,0xF0,0x60,0x60,0x60,0x60,0xE6,0xFC,0x00,0x00,0x00,0x00,  // 0x9C
        0x00,0x00,0x66,0x66,0x3C,0x18,0x7E,0x18,0x7E,0x18,0x18,0x18,0x00,0x00,0x00,0x00,  // 0x9D
        0x00,0xF8,0xCC,0xCC,0xF8,0xC4,0xCC,0xDE,0xCC,0xCC,0xCC,0xC6,0x00,0x00,0x00,0x00,  // 0x9E
        0x00,0x0E,0x1B,0x18,0x18,0x18,0x7E,0x18,0x18,0x18,0xD8,0x70,0x00,0x00,0x00,0x00,  // 0x9F
        0x00,0x18,0x30,0x60,0x00,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0xA0
        0x00,0x0C,0x18,0x30,0x00,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00,0x00,0x00,0x00,  // 0xA1
        0x00,0x18,0x30,0x60,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0xA2
        0x00,0x18,0x30,0x60,0x00,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0x76,0x00,0x00,0x00,0x00,  // 0xA3
        0x00,0x00,0x76,0xDC,0x00,0xDC,0x66,0x66,0x66,0x66,0x66,0x66,0x00,0x00,0x00,0x00,  // 0xA4
        0x76,0xDC,0x00,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00,  // 0xA5
        0x00,0x00,0x3C,0x6C,0x6C,0x3E,0x00,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xA6
        0x00,0x00,0x38,0x6C,0x6C,0x38,0x00,0x7C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xA7
        0x00,0x00,0x30,0x30,0x00,0x30,0x30,0x60,0xC0,0xC6,0xC6,0x7C,0x00,0x00,0x00,0x00,  // 0xA8
        0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0xC0,0xC0,0xC0,0xC0,0x00,0x00,0x00,0x00,0x00,  // 0xA9
        0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x06,0x06,0x06,0x06,0x00,0x00,0x00,0x00,0x00,  // 0xAA
        0x00,0x60,0xE0,0x62,0x66,0x6C,0x18,0x30,0x60,0xDC,0x86,0x0C,0x18,0x3E,0x00,0x00,  // 0xAB
        0x00,0x60,0xE0,0x62,0x66,0x6C,0x18,0x30,0x66,0xCE,0x9A,0x3F,0x06,0x06,0x00,0x00,  // 0xAC
        0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x18,0x3C,0x3C,0x3C,0x18,0x00,0x00,0x00,0x00,  // 0xAD
        0x00,0x00,0x00,0x00,0x00,0x36,0x6C,0xD8,0x6C,0x36,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xAE
        0x00,0x00,0x00,0x00,0x00,0xD8,0x6C,0x36,0x6C,0xD8,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xAF
        0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44,0x11,0x44,  // 0xB0
        0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,0x55,0xAA,  // 0xB1
        0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,0xDD,0x77,  // 0xB2
        0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xB3
        0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xB4
        0x18,0x18,0x18,0x18,0x18,0xF8,0x18,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xB5
        0x36,0x36,0x36,0x36,0x36,0x36,0x36,0xF6,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xB6
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xB7
        0x00,0x00,0x00,0x00,0x00,0xF8,0x18,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xB8
        0x36,0x36,0x36,0x36,0x36,0xF6,0x06,0xF6,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xB9
        0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xBA
        0x00,0x00,0x00,0x00,0x00,0xFE,0x06,0xF6,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xBB
        0x36,0x36,0x36,0x36,0x36,0xF6,0x06,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xBC
        0x36,0x36,0x36,0x36,0x36,0x36,0x36,0xFE,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xBD
        0x18,0x18,0x18,0x18,0x18,0xF8,0x18,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xBE
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xF8,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xBF
        0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xC0
        0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xC1
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xC2
        0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xC3
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xC4
        0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xC5
        0x18,0x18,0x18,0x18,0x18,0x1F,0x18,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xC6
        0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x37,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xC7
        0x36,0x36,0x36,0x36,0x36,0x37,0x30,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xC8
        0x00,0x00,0x00,0x00,0x00,0x3F,0x30,0x37,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xC9
        0x36,0x36,0x36,0x36,0x36,0xF7,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xCA
        0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0xF7,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xCB
        0x36,0x36,0x36,0x36,0x36,0x37,0x30,0x37,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xCC
        0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xCD
        0x36,0x36,0x36,0x36,0x36,0xF7,0x00,0xF7,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xCE
        0x18,0x18,0x18,0x18,0x18,0xFF,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xCF
        0x36,0x36,0x36,0x36,0x36,0x36,0x36,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xD0
        0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xD1
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xD2
        0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xD3
        0x18,0x18,0x18,0x18,0x18,0x1F,0x18,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xD4
        0x00,0x00,0x00,0x00,0x00,0x1F,0x18,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xD5
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xD6
        0x36,0x36,0x36,0x36,0x36,0x36,0x36,0xFF,0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36,  // 0xD7
        0x18,0x18,0x18,0x18,0x18,0xFF,0x18,0xFF,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xD8
        0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xF8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xD9
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xDA
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xDB
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,  // 0xDC
        0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,0xF0,  // 0xDD
        0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,  // 0xDE
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xDF
        0x00,0x00,0x00,0x00,0x00,0x76,0xDC,0xD8,0xD8,0xD8,0xDC,0x76,0x00,0x00,0x00,0x00,  // 0xE0
        0x00,0x00,0x78,0xCC,0xCC,0xCC,0xD8,0xCC,0xC6,0xC6,0xC6,0xCC,0x00,0x00,0x00,0x00,  // 0xE1
        0x00,0x00,0xFE,0xC6,0xC6,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0x00,0x00,0x00,0x00,  // 0xE2
        0x00,0x00,0x00,0x00,0x00,0xFE,0x6C,0x6C,0x6C,0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,  // 0xE3
        0x00,0x00,0xFE,0xC6,0x60,0x30,0x18,0x18,0x30,0x60,0xC6,0xFE,0x00,0x00,0x00,0x00,  // 0xE4
        0x00,0x00,0x00,0x00,0x00,0x7E,0xD8,0xD8,0xD8,0xD8,0xD8,0x70,0x00,0x00,0x00,0x00,  // 0xE5
        0x00,0x00,0x00,0x00,0x00,0x66,0x66,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0xC0,0x00,  // 0xE6
        0x00,0x00,0x00,0x00,0x76,0xDC,0x18,0x18,0x18,0x18,0x18,0x18,0x00,0x00,0x00,0x00,  // 0xE7
        0x00,0x00,0x7E,0x18,0x3C,0x66,0x66,0x66,0x66,0x3C,0x18,0x7E,0x00,0x00,0x00,0x00,  // 0xE8
        0x00,0x00,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x6C,0x38,0x00,0x00,0x00,0x00,  // 0xE9
        0x00,0x00,0x38,0x6C,0xC6,0xC6,0xC6,0x6C,0x6C,0x6C,0x6C,0xEE,0x00,0x00,0x00,0x00,  // 0xEA
        0x00,0x00,0x1E,0x30,0x18,0x0C,0x3E,0x66,0x66,0x66,0x66,0x3C,0x00,0x00,0x00,0x00,  // 0xEB
        0x00,0x00,0x00,0x00,0x00,0x7E,0xDB,0xDB,0xDB,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xEC
        0x00,0x00,0x00,0x03,0x06,0x7E,0xDB,0xDB,0xF3,0x7E,0x60,0xC0,0x00,0x00,0x00,0x00,  // 0xED
        0x00,0x00,0x1C,0x30,0x60,0x60,0x7C,0x60,0x60,0x60,0x30,0x1C,0x00,0x00,0x00,0x00,  // 0xEE
        0x00,0x00,0x00,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x00,0x00,0x00,0x00,  // 0xEF
        0x00,0x00,0x00,0x00,0xFE,0x00,0x00,0xFE,0x00,0x00,0xFE,0x00,0x00,0x00,0x00,0x00,  // 0xF0
        0x00,0x00,0x00,0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00,0x7E,0x00,0x00,0x00,0x00,  // 0xF1
        0x00,0x00,0x00,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00,0x7E,0x00,0x00,0x00,0x00,  // 0xF2
        0x00,0x00,0x00,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00,0x7E,0x00,0x00,0x00,0x00,  // 0xF3
        0x00,0x00,0x0E,0x1B,0x1B,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,  // 0xF4
        0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0xD8,0xD8,0xD8,0x70,0x00,0x00,0x00,  // 0xF5
        0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x7E,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xF6
        0x00,0x00,0x00,0x00,0x00,0x76,0xDC,0x00,0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xF7
        0x00,0x38,0x6C,0x6C,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xF8
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xF9
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xFA
        0x00,0x0F,0x0C,0x0C,0x0C,0x0C,0x0C,0xEC,0x6C,0x6C,0x3C,0x1C,0x00,0x00,0x00,0x00,  // 0xFB
        0x00,0x6C,0x36,0x36,0x36,0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xFC
        0x00,0x3C,0x66,0x0C,0x18,0x32,0x7E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xFD
        0x00,0x00,0x00,0x00,0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0x7E,0x00,0x00,0x00,0x00,0x00,  // 0xFE
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  // 0xFF
        };

    // Reload the character generator into VRAM plane 2 after VgaForceText3().
    // VBE/GRUB boot paths can leave font RAM all zeros, so the correctly
    // written 0xB8000 cells render as blank rows.  QEMU finds glyph N at
    // vram+2+128*N with SR3=0 (map 0), scanline i at font_ptr[4*i] -- planar
    // address N*32+i, byte lane 2.  With SR4=SEQ_MODE (no word/odd-even
    // remap), SR2=plane 2 only, GR5=write mode 0 with odd/even OFF and
    // GR6=0x0C (B8000 window, chain odd/even OFF), a byte written to
    // 0xB8000+X lands exactly in plane 2 at planar address X.
    static void LoadVgaFont()
        {
        // Save the mode-3 state installed by VgaForceText3() for restore.
        Out8(0x3C4, 2); const TUint8 oldMap = In8(0x3C5);
        Out8(0x3C4, 4); const TUint8 oldMem = In8(0x3C5);
        Out8(0x3CE, 0); const TUint8 oldSr = In8(0x3CF);
        Out8(0x3CE, 1); const TUint8 oldSrEn = In8(0x3CF);
        Out8(0x3CE, 3); const TUint8 oldRot = In8(0x3CF);
        Out8(0x3CE, 5); const TUint8 oldMode = In8(0x3CF);
        Out8(0x3CE, 6); const TUint8 oldMisc = In8(0x3CF);
        Out8(0x3CE, 8); const TUint8 oldBit = In8(0x3CF);

        // Write mode 0, no set/reset, no rotate, full bit mask.
        Out8(0x3C4, 4); Out8(0x3C5, 0x06);   // EXT_MEM | SEQ_MODE, no chain4
        Out8(0x3C4, 2); Out8(0x3C5, 0x04);   // plane 2 (font) only
        Out8(0x3CE, 0); Out8(0x3CF, 0x00);
        Out8(0x3CE, 1); Out8(0x3CF, 0x00);
        Out8(0x3CE, 3); Out8(0x3CF, 0x00);
        Out8(0x3CE, 5); Out8(0x3CF, 0x00);   // write mode 0, odd/even OFF
        Out8(0x3CE, 6); Out8(0x3CF, 0x0C);   // B8000 window, odd/even OFF
        Out8(0x3CE, 8); Out8(0x3CF, 0xFF);

        volatile unsigned char* const dst =
            reinterpret_cast<volatile unsigned char*>(0xB8000u);
        for (TUint g = 0; g < 256; ++g)
            {
            const TUint8* src = kFont8x16 + g * 16;
            volatile unsigned char* p = dst + g * 32;   // planar N*32 + i
            for (TUint i = 0; i < 16; ++i)
                p[i] = src[i];
            }

        // Restore the mode-3 mapping so the B8000 text cells stay visible.
        Out8(0x3CE, 8); Out8(0x3CF, oldBit);
        Out8(0x3CE, 6); Out8(0x3CF, oldMisc);
        Out8(0x3CE, 5); Out8(0x3CF, oldMode);
        Out8(0x3CE, 3); Out8(0x3CF, oldRot);
        Out8(0x3CE, 1); Out8(0x3CF, oldSrEn);
        Out8(0x3CE, 0); Out8(0x3CF, oldSr);
        Out8(0x3C4, 2); Out8(0x3C5, oldMap);
        Out8(0x3C4, 4); Out8(0x3C5, oldMem);

        // Sparse marker: once per guest, never on every text-mode init.
        static TBool sFontMarked = EFalse;
        if (!sFontMarked)
            {
            sFontMarked = ETrue;
            const char* m = "VGA: font loaded\r\n";
            while (*m) QemuUart::PutC(*m++);
            }
        }

    // Restore the standard BIOS mode 3 (80x25, 16-colour text) scanout. GRUB
    // boots with `gfxpayload=text`, but boot-time display hooks may have
    // programmed VBE before the shell starts; re-asserting the full mode-3
    // register set here guarantees the EKA2> prompt is real VGA text on screen.
    // Writing the same registers when the hardware is already in text mode is
    // harmless, so this is safe whether or not a boot hook touched VBE.
    void VgaForceText3()
        {
        DispiWrite(4, 0);                       // DISPI LFB off: use VGA CRTC
        Out8(0x3C2, 0x67);                      // 28.322MHz, 31.5kHz, colour
        // Full standard BIOS mode-3 tables (QEMU vgabios vgatables.h index 0x18).
        // SR1 bit3 clear => 9-dot chars; QEMU renders 80x9=720px wide.
        static const TUint8 kSeq[] =
            {0x03, 0x00, 0x03, 0x00, 0x02};
        for (TUint i = 0; i < sizeof(kSeq); ++i)
            { Out8(0x3C4, (TUint8)i); Out8(0x3C5, kSeq[i]); }
        // CRTC 80x25. Unlock indices 0..7 FIRST (bit 7 of index 0x11 clear):
        // writing them while protected leaves the VBE horizontal timings in
        // place, which QEMU reports as a bogus wide/blank mode.
        Out8(0x3D4, 0x11);
        Out8(0x3D5, (TUint8)(In8(0x3D5) & 0x7Fu));
        // QEMU: width = CR1+1 = 80 chars; char width = 9 (SR1 bit3 clear);
        // char height = (CR9 & 0x1f)+1 = 16; rows = (V_DISP_END+1)/16 = 25
        // => 720x400. CR2..CR6 / CR0x14..0x17 are the BIOS sync/timing values.
        static const TUint8 kCrtc[] =
            {0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,
             0x00,0x4F,0x0D,0x0E,0x00,0x00,0x00,0x00,
             0x9C,0x8E,0x8F,0x28,0x1F,0x96,0xB9,0xA3,0xFF};
        for (TUint i = 0; i < sizeof(kCrtc); ++i)
            { Out8(0x3D4, (TUint8)i); Out8(0x3D5, kCrtc[i]); }
        // Attribute controller: classic BIOS text-mode palette mapping and
        // mode 0x0C (text, 16 colours). 0x3DA resets the AC flip-flop first.
        static const TUint8 kAttr[] =
            {0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
             0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
             0x0C,0x00,0x0F,0x08};
        (void)In8(0x3DA);
        for (TUint i = 0; i < sizeof(kAttr); ++i)
            { Out8(0x3C0, (TUint8)i); Out8(0x3C0, kAttr[i]); }
        Out8(0x3C0, 0x20);                      // enable video output
        // Graphics controller: GR5=odd/even host (char plane 0, attr plane 1),
        // GR6=0x0E selects the 0xB8000 text window mapped to VRAM offset 0,
        // GR7=read plane 0. Without these the 0xB8000 cells are written to the
        // wrong VRAM offset and the text renderer sees a blank screen.
        static const TUint8 kGfx[] =
            {0x00,0x00,0x00,0x00,0x00,0x10,0x0E,0x0F,0xFF};
        for (TUint i = 0; i < sizeof(kGfx); ++i)
            { Out8(0x3CE, (TUint8)i); Out8(0x3CF, kGfx[i]); }
        // Font RAM is not part of the register tables: VBE boot paths zero it.
        // Reload the 8x16 glyph bitmaps into plane 2 before any text is drawn.
        LoadVgaFont();
        }

    // Once a graphical app has switched VBE on (gTextMode == EFalse), mirror
    // the same teletype into the LFB with the scaled 5x7 stroke font so the
    // shell stays visible on screen as well as on COM1. Cell geometry comes
    // from qemu_display.cpp so
    // the glyph and the row/column advances always agree: with SCALE=2 the
    // glyph is 16x16 and we step 18px per row (2px gap), not 16px with an
    // 8px glyph as before.  The FB grid is derived from the framebuffer size
    // (64x42 at 1024x768) so large text never runs off the right edge.
    const TUint kFxCellW = QEMU_DISPLAY_CELL_W, kFxCellH = QEMU_DISPLAY_CELL_H;
    TUint gFbW = 0, gFbH = 0, gFbBpp = 0, gFxCol = 0, gFxRow = 0;
    TUint gFxCols = 0, gFxRows = 0;
    TBool gFxReady = EFalse;
    // Genuine KPrintf appends "\r\n" to every line, so most shell lines arrive
    // as "\r\n\r\n".  Collapse the duplicate in the LFB mirror only: COM1 and
    // the 0xB8000 buffer stay byte-for-byte unchanged.
    TBool gFxLineFresh = ETrue;
    // While a full-screen graphical E32 program owns the display, every
    // KPrintf/DebugPrint character (shell banners, "started" notices and the
    // GUI app's own RDebug markers) must go to COM1 only. Writing any of it
    // into the LFB mirror would paint text over the GUI and, once the 42-row
    // grid fills, FbScroll() would shift the entire framebuffer up 18px.
    volatile TBool gFbMirrorSuspended = EFalse;

    void FbEnsure()
        {
        // Never touch display_* while the real VGA text console is active:
        // every accessor calls display_init() and would flip the hardware to
        // the LFB. This is the boot-time guard that keeps 0xB8000 authoritative.
        if (gTextMode || gFxReady)
            return;
        TUint w = 0, h = 0, b = 0;
        display_geometry(w, h, b);
        if (!w || !h)
            return;
        gFbW = w; gFbH = h; gFbBpp = b;
        gFxCols = w / kFxCellW;
        gFxRows = h / kFxCellH;
        gFxReady = ETrue;
        }

    void FbCellClear(TUint aCol, TUint aRow)
        { display_rect(aCol * kFxCellW, aRow * kFxCellH,
                       kFxCellW, kFxCellH, 0u); }

    void FbScroll()
        {
        volatile unsigned char* fb =
            (volatile unsigned char*)display_lfb();
        TUint pitch = gFbW * (gFbBpp ? (gFbBpp >> 3) : 4u);
        if (!fb || !pitch)
            return;
        for (TUint y = kFxCellH; y < gFbH; ++y)
            {
            volatile TUint32* dst = (volatile TUint32*)(fb + (y - kFxCellH) * pitch);
            volatile TUint32* src = (volatile TUint32*)(fb + y * pitch);
            for (TUint i = 0; i < pitch / 4; ++i)
                dst[i] = src[i];
            }
        display_rect(0, gFbH - kFxCellH, gFbW, kFxCellH, 0u);
        }

    void FbPutC(char aC)
        {
        if (gTextMode)
            return;   // boot console: kVram is the screen, there is no LFB yet
        // A graphical parent (DESKTOP) keeps its own RDebug markers on COM1
        // even after a child has taken the mirror back over.
        TUint pid = 0;
        if (sGuiMirrorCount)
            {
            DThread* self = &Kern::CurrentThread();
            if (self && self->iOwningProcess)
                pid = (TUint)self->iOwningProcess->iId;
            }
        if (pid && IsGuiMirrorOwner(pid))
            return;
        if (gFbMirrorSuspended)
            {
            // The mute was installed for the GUI parent.  If some OTHER user
            // process is printing (an exec-268 child such as CALC), hand the
            // display to it: resume the mirror, wipe the GUI pixels and let
            // this character -- and everything after it -- through.  The
            // shell's own thread must not trigger this (its launch chatter
            // belongs on COM1 only).
            if (!pid || pid == sMirrorShellPid)
                return;
            QemuFbMirrorSet(0);
            sChildTookMirror = ETrue;
            VgaInit();
            // COM1-only marker; never KPrintf from inside this output path.
            const char* m = "SH: mirror resumed for child\r\n";
            while (*m) QemuUart::PutC(*m++);
            }
        FbEnsure();
        if (!gFxReady)
            return;
        if (aC == '\n')
            {
            gFxCol = 0;
            if (gFxLineFresh)
                return;       // doubled CRLF from KPrintf: no extra blank row
            gFxLineFresh = ETrue;
            if (++gFxRow >= gFxRows)
                { gFxRow = gFxRows - 1; FbScroll(); }
            return;
            }
        if (aC == '\r') { gFxCol = 0; return; }
        if (aC == '\b')
            {
            if (gFxCol)
                { --gFxCol; FbCellClear(gFxCol, gFxRow); }
            return;
            }
        if (gFxCol >= gFxCols || gFxRow >= gFxRows)
            return;
        FbCellClear(gFxCol, gFxRow);
        display_putc(gFxCol * kFxCellW, gFxRow * kFxCellH,
                     aC, 0x00C0C0C0u, 0u);
        gFxLineFresh = EFalse;
        if (++gFxCol >= gFxCols)
            FbPutC('\n');
        }
    }

void VgaTeletype(char aC)
    {
    // Text-mode boot: write ONLY the real 80x25 text buffer. The LFB mirror is
    // used only once a graphical app has switched VBE on; the kVram path below
    // still runs in that state (the buffer is dead, but writing it is harmless
    // and keeps the two console paths in sync).
    if (!gTextMode)
        FbPutC(aC);  // LFB mirror for VBE graphics mode; COM1 handled by caller
    if (aC == '\n') { NewLine(); return; }
    if (aC == '\r') { sCol = 0; MoveCursor(); return; }
    if (aC == '\b')
        {
        if (sCol > 0)
            {
            --sCol;
            kVram[sRow * kCols + sCol] = (unsigned short)((kAttr << 8) | ' ');
            MoveCursor();
            }
        return;
        }
    kVram[sRow * kCols + sCol] = (unsigned short)((kAttr << 8) | (unsigned char)aC);
    if (++sCol >= kCols)
        NewLine();
    else
        MoveCursor();
    }

// Called by display_set_mode() after a VBE switch: drop the cached LFB grid
// so FbEnsure() re-reads the new display_geometry() and the next character
// starts at (0,0) on the freshly cleared framebuffer.
void FbMirrorReset()
    {
    gFxCol = 0; gFxRow = 0; gFxReady = EFalse; gFxLineFresh = ETrue;
    }

// Public switch used by the shell launcher (see RunImage/ShellMain below).
void QemuFbMirrorSet(int aOn)
    {
    gFbMirrorSuspended = aOn ? ETrue : EFalse;
    }

static void VgaInit()
    {
    for (TUint i = 0; i < kCols * kRows; ++i)
        kVram[i] = (unsigned short)((kAttr << 8) | ' ');
    sCol = 0;
    sRow = 0;
    MoveCursor();
    if (gTextMode)
        return;    // text console: 0xB8000 is the whole screen, no LFB to clear
    FbEnsure();
    if (gFxReady)
        {
        display_clear(0u);
        gFxCol = 0;
        gFxRow = 0;
        gFxLineFresh = ETrue;
        }
    }

// ------------------------------------------------------------- keyboard ring
static volatile unsigned char sRing[256];
static volatile TUint sHead = 0, sTail = 0;

// Ctrl+C abort (Linux-style ^C): byte 0x03 (ETX) is swallowed at every input
// source and only raises this flag. The shell checks it while a child runs —
// including while a program holds the console lease — kills the foreground
// E32 process and returns to the prompt. No signal machinery in the guest.
static volatile TBool sAbortPending = EFalse;
static TUint sForegroundPid = 0;

// Strong definition of the gating hook declared weak in qemu_variant_int.cpp.
// In text mode the shell line editor relies on hardware typematic for Backspace
// repeat, so keep it.  Once a graphical E32 app (DOOM) has switched VBE on,
// suppress kernel-level repeats: they would fill the 256-byte input ring and
// delay the key-up byte.  DOOM supplies its own software typematic anyway.
extern "C" int QemuKbdTypematicSuppress()
    {
    return gTextMode ? 0 : 1;
    }

extern "C" void QemuKbdPush(char aC)
    {
    if ((unsigned char)aC == 0x03) { sAbortPending = ETrue; return; }
    TUint next = (sHead + 1) & 255u;
    if (next == sTail) return;
    sRing[sHead] = (unsigned char)aC;
    sHead = next;
    }

// Separate USB ring: source-tagged so USB delivery is measurable
// (the `N key(s) from USB HID` counter in `usb` output).
static volatile unsigned char sURing[256];
static volatile TUint sUHead = 0, sUTail = 0;

extern "C" void QemuKbdPushUsb(char aC)
    {
    if ((unsigned char)aC == 0x03) { sAbortPending = ETrue; return; }
    TUint next = (sUHead + 1) & 255u;
    if (next == sUTail) return;
    sURing[sUHead] = (unsigned char)aC;
    sUHead = next;
    }

static int UsbKbdPoll()
    {
    if (sUHead == sUTail) return -1;
    char c = (char)sURing[sUTail];
    sUTail = (sUTail + 1) & 255u;
    return (int)(unsigned char)c;
    }

static int KbdPoll()
    {
    if (sHead == sTail) return -1;
    char c = (char)sRing[sTail];
    sTail = (sTail + 1) & 255u;
    return (int)(unsigned char)c;
    }

static int SerialPoll()
    {
    if (!(In8(0x3FD) & 0x01)) return -1;
    int c = (int)In8(0x3F8);
    if (c == 0x03) { sAbortPending = ETrue; return -1; }  // ^C fallback path
    return c;
    }

// Shared input stream for the PortCon console service (variant/usercon.cpp).
// Identical poll order to the shell loop, including the USB feeds that move
// controller data into the USB ring. Callers must hold the input lease so
// the shell and a program never split the stream.
int ConPollChar()
    {
    int c = KbdPoll();
    if (c < 0) c = SerialPoll();
    UsbPoll();
    XhciKeyboardPoll();
    if (c < 0)
        c = UsbKbdPoll();
    return c;
    }

// ------------------------------------------------------------------- shell
extern "C" TUint qemu_cpu_features;  // bootcpu_gas.o BSS (CPUID report)

static char sLine[128];
static TUint sLen = 0;

// ---- line-editor state (command history, escape sequences, prompt handoff) ----
static const TUint kHistMax = 16;
static char sHist[kHistMax][sizeof(sLine)];  // ring of recent commands
static TUint sHistHead = 0;                   // next free ring slot
static TUint sHistCount = 0;                  // valid entries
static TBool sHistBrowsing = EFalse;          // Up/Down recall in progress
static TUint sHistPos = 0;                    // 0=oldest .. sHistCount=new line
static char sHistSaved[sizeof(sLine)];        // in-progress line before recall
static TUint sHistSavedLen = 0;
static TUint sEscState = 0;                   // 0=normal 1=ESC 2=ESC '['

// Prompt handoff: a child that leases PortCon owns the console, so the shell
// must not spray its own prompt over the child's (e.g. FILEMGR's "FM> ").
static TBool sJustLaunched = EFalse;         // E32Run succeeded on this command
static TBool sPromptNeeded = EFalse;         // emit a prompt when console is free
static TBool sWasLeased = EFalse;            // edge-detect con_release()

// Multi-line `write` state (4KB static buffer, BSS).
static TUint8 wBuf[4096];
static TUint wLen = 0;
static char wName[128];
static char wDrive = 0;
static TBool wMode = EFalse;

static void StartMultiline(const char* aName, char aDrive)
    {
    wLen = 0;
    wMode = ETrue;
    int k = 0;
    while (aName[k] && k < 127)
        {
        wName[k] = aName[k];
        ++k;
        }
    wName[k] = 0;
    wDrive = aDrive;
    KPrintf("type lines, lone . ends\r\n");
    }

static int StrEq(const char* a, const char* b)
    { while (*a && *a == *b) { ++a; ++b; } return *a == *b; }

static char Up(char c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
static TBool Same(const char* a, const char* b)
    {
    while (*a && Up(*a) == Up(*b)) { ++a; ++b; }
    return *a == *b;
    }

// VBE mode numbers are hexadecimal (e.g. 115, 118, 117); accept an optional
// 0x prefix.  Returns EFalse on empty/invalid input.
static TBool ParseVbeMode(const char* aText, TInt& aMode)
    {
    if (!aText || !*aText)
        return EFalse;
    if (aText[0] == '0' && (aText[1] == 'x' || aText[1] == 'X'))
        aText += 2;
    if (!*aText)
        return EFalse;
    TInt v = 0;
    while (*aText)
        {
        char c = *aText++;
        TInt d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return EFalse;
        v = v * 16 + d;
        if (v > 0xFFFF) return EFalse;
        }
    aMode = v;
    return ETrue;
    }

struct TCommandHelp { const char* name; const char* usage; const char* detail; };
static const TCommandHelp kHelp[] =
    {
    {"help", "HELP [command]", "Show the command list or detailed command help."},
    {"dir", "DIR [/W] [X:][path [pattern]]", "List a directory without changing the prompt. *. * and ? filter names; shows sizes plus file/dir totals."},
    {"cd", "CD [X:][path|..|\\]", "Change directory. CD alone returns to the root."},
    {"type", "TYPE [X:]path", "Display a text file in this or another directory."},
    {"write", "WRITE [X:]path [text...]", "Replace a file; omit text for multiline input, end with a lone dot."},
    {"copy", "COPY [/Y] [X:]source [Y:]dest", "Copy a file (up to 64 KiB). /Y permits overwrite; output names must be 8.3."},
    {"ren", "REN [X:]old [X:]new", "Rename an 8.3 file in its directory; no overwrite or data copying."},
    {"del", "DEL [X:]file", "Delete one file or empty directory (ERASE is an alias)."},
    {"mkdir", "MKDIR [X:]path", "Create an 8.3 directory at the given path."},
    {"rmdir", "RMDIR [X:]name", "Remove an empty directory (RD is an alias)."},
    {"mount", "MOUNT [device [partition]]", "Mount all FAT32 partitions, or one device (0-7) and partition (0-3)."},
    {"vol", "VOL", "Show mounted drives and their backing devices."},
    {"drive", "DRIVE [X]", "Show drives or switch to a mounted drive; X: also switches."},
    {"format", "FORMAT device yes", "DESTROYS the whole specified disk and creates one FAT32 partition."},
    {"args", "ARGS id", "Show a process's command line (see PS for IDs)."},
    {"kill", "KILL id", "Terminate a user process; use PS to find its ID."},
    {"abort", "ABORT (or CTRL C, !)", "Kill the current foreground program (same as typing ^C on the console) and return to the prompt."},
    {"reap", "REAP", "Release dead user-image code segments."},
    {"exits", "EXITS", "Show recent user-process exit records (survives reaping)."},
    {"wait", "WAIT id [seconds]", "Block until a process dies (default 30s, max 300s), then report its exit."},
    {"version", "VERSION", "Show kernel and CPU information."},
    {"uptime", "UPTIME", "Show milliseconds since boot."},
    {"date", "DATE [YYYY-MM-DD HH:MM:SS]", "Show RTC date and time; with both arguments, set the CMOS clock (2000-2099) and read it back."},
    {"meminfo", "MEMINFO", "Show heap, RAM and object usage."},
    {"ps", "PS", "List processes and IDs."},
    {"threads", "THREADS", "List kernel threads."},
    {"pci", "PCI", "List PCI devices."},
    {"display", "DISPLAY [TEST]", "Show the VBE framebuffer mode/LFB; TEST clears blue and draws a border + EKA2 GUI banner."},
    {"vbe", "VBE [mode]", "List supported VBE graphics modes (0x118 default 1024x768x32); with a hex mode (e.g. 115, 118, 117) reprogram Bochs DISPI and switch."},
    {"mouse", "MOUSE [INFO|POLL|TEST|SHARED|PRESS [mask]]", "Show the PS/2 mouse cursor/buttons/packets; INFO details the device, POLL drains the 8042 once, TEST injects a synthetic packet, SHARED shows the button word userland reads, PRESS injects a button packet (mask 1=L 2=R 4=M)."},

    {"kbd", "KBD [ARM]", "Show PS/2 IRQ1 ISR/key counters and PIC/8042 state; ARM rebinds IRQ1, flushes stale 8042 bytes and unmasks it."},
    {"nic", "NIC [ARP [count]]", "Show e1000 link/MAC; NIC ARP sends ARP requests and waits for replies."},
    {"vnet", "VNET [INFO]", "Show virtio-net PCI/MAC/queue state (alternative NIC backend to e1000)."},
    {"netbridge", "NETBRIDGE [ms]", "Pump the kernel<->userland e1000 frame bridge for up to ms milliseconds (default 6000)."},
    {"ide", "IDE", "List ATA/AHCI disks and FAT32 partitions."},
    {"ahci", "AHCI", "Show SATA controller details."},
    {"vblk", "VBLK [INFO|TEST]", "Show virtio-blk PCI/capacity/queue state; TEST reads sector 0 through the virtqueue."},
    {"rng", "RNG [INFO|TEST]", "Show virtio-RNG PCI/queue state; TEST fetches 16 random bytes through the virtqueue."},
    {"audio", "AUDIO [PLAY]", "Show AC97 audio BARS/vendor ID; PLAY outputs a 1kHz test tone."},
    {"usb", "USB [POLL|INFO]", "Probe HID keyboards; POLL dumps one debug report, INFO shows OHCI controller/ports."},
    {"ehci", "EHCI [INFO]", "Show EHCI USB2 controller version/port count; INFO dumps per-port connect/enable/speed."},
    {"xhci", "XHCI", "Show USB3 controller status; XHCIPORTS lists ports."},
    {"xhciports", "XHCIPORTS", "List USB3 root-port status."},
    {"reboot", "REBOOT", "Restart via the 8042 controller."},
    {"segs", "SEGS", "List code segments (diagnostic)."},
    {"pzomb", "PZOMB id", "Inspect a lingering process (diagnostic)."},
    {"heaptest", "HEAPTEST", "Test kernel heap allocation (diagnostic)."},
    {"xhcistart", "XHCISTART", "Start USB3 controller (diagnostic)."},
    {"xhcinoop", "XHCINOOP", "Send USB3 no-op (diagnostic)."},
    {"xhcislot", "XHCISLOT port", "Enable USB3 slot on root port 1-8."},
    {"xhciaddr", "XHCIADDR port", "Address a USB3 device on root port 1-8."},
    {"xhcidesc", "XHCIDESC port", "Read USB3 device descriptor on port 1-8."},
    {"xhcikbd", "XHCIKBD port", "Initialize a direct USB3 keyboard on port 1-8."}
    };

static void PrintHelp(const char* aName)
    {
    if (!aName)
        {
        KPrintf("EKA2 DOS-style command shell  (HELP command for details)\r\n");
        KPrintf("Files:    DIR  CD  TYPE  WRITE  COPY  REN  DEL  MKDIR  RMDIR  CLS\r\n");
        KPrintf("Drives:   MOUNT  VOL  DRIVE  X:  FORMAT\r\n");
        KPrintf("System:   VERSION  DATE  UPTIME  MEMINFO  PS  THREADS  KILL  ABORT  REAP  EXITS  WAIT  REBOOT\r\n");
        KPrintf("Programs: type a file name to run it, e.g. CALC.EXE, CALC.E32 or CALC args\r\n");
        KPrintf("Hardware: PCI  DISPLAY  VBE  MOUSE  IDE  AHCI  VBLK  RNG  AUDIO  USB  EHCI  XHCI  XHCIPORTS  NIC  VNET\r\n");
        KPrintf("Paths: X:name selects a mounted drive; quote names containing spaces.\r\n");
        return;
        }
    if (Same(aName, "md")) aName = "mkdir";
    if (Same(aName, "rd")) aName = "rmdir";
    if (Same(aName, "erase")) aName = "del";
    if (Same(aName, "rename")) aName = "ren";
    if (Same(aName, "cls"))
        { KPrintf("CLS\r\n  Clear the VGA screen and serial terminal.\r\n"); return; }
    for (TUint i = 0; i < sizeof(kHelp) / sizeof(kHelp[0]); ++i)
        if (Same(aName, kHelp[i].name))
            { KPrintf("%s\r\n  %s\r\n", kHelp[i].usage, kHelp[i].detail); return; }
    KPrintf("help: unknown command %s\r\n", aName);
    }

// ----------------------------------------------------------- line editor helpers
static TUint StrLen2(const char* aS)
    {
    TUint n = 0;
    while (aS[n]) ++n;
    return n;
    }

// sHistPos is a logical index: 0 = oldest entry, sHistCount-1 = newest,
// sHistCount = the (possibly empty) line being typed right now.
static const char* HistEntry(TUint aPos)
    {
    TUint slot = (sHistHead + kHistMax - sHistCount + aPos) % kHistMax;
    return sHist[slot];
    }

static void HistAdd(const char* aLine, TUint aLen)
    {
    if (!aLen)
        return;
    if (sHistCount)
        {
        const char* last = HistEntry(sHistCount - 1);
        if (StrLen2(last) == aLen)
            {
            TUint i = 0;
            while (i < aLen && Up(last[i]) == Up(aLine[i])) ++i;
            if (i == aLen) return;  // don't record consecutive duplicates
            }
        }
    TUint k = 0;
    for (; k < aLen && k < sizeof(sLine) - 1; ++k)
        sHist[sHistHead][k] = aLine[k];
    sHist[sHistHead][k] = 0;
    sHistHead = (sHistHead + 1) % kHistMax;
    if (sHistCount < kHistMax) ++sHistCount;
    }

// Erase the currently displayed line and redraw it with aText. The cursor
// always returns to the same column, so the edit stays on one row.
static void RedrawLine(const char* aText, TUint aLen)
    {
    while (sLen > 0) { --sLen; EmitBackspace(); }
    if (aLen > sizeof(sLine) - 1) aLen = sizeof(sLine) - 1;
    for (TUint i = 0; i < aLen; ++i) sLine[i] = aText[i];
    sLine[aLen] = 0;
    sLen = aLen;
    EmitRawS(sLine);
    }

static void HistPrev()
    {
    if (!sHistCount) return;
    if (!sHistBrowsing)
        {
        for (TUint i = 0; i < sLen; ++i) sHistSaved[i] = sLine[i];
        sHistSaved[sLen] = 0;
        sHistSavedLen = sLen;
        sHistBrowsing = ETrue;
        sHistPos = sHistCount;
        }
    if (sHistPos > 0) --sHistPos;
    const char* e = HistEntry(sHistPos);
    RedrawLine(e, StrLen2(e));
    }

static void HistNext()
    {
    if (!sHistBrowsing) return;
    if (sHistPos < sHistCount)
        {
        ++sHistPos;
        if (sHistPos == sHistCount)
            RedrawLine(sHistSaved, sHistSavedLen);
        else
            {
            const char* e = HistEntry(sHistPos);
            RedrawLine(e, StrLen2(e));
            }
        }
    }

// Tab completes the first word against built-in command names. Directory
// entries are not enumerated by an exported API, so command completion is the
// supported form (ambiguous prefixes list all matches).
static void EmitUpperS(const char* aS)
    {
    while (*aS) EmitRaw(Up(*aS++));
    }

static void CompleteLine()
    {
    if (!sLen) return;
    for (TUint i = 0; i < sLen; ++i)
        if (sLine[i] == ' ' || sLine[i] == '\t') return;  // only word 1
    char prefix[sizeof(sLine)];
    for (TUint i = 0; i < sLen; ++i) prefix[i] = Up(sLine[i]);
    prefix[sLen] = 0;
    const char* matches[128];
    TUint n = 0;
    for (TUint i = 0; i < sizeof(kHelp) / sizeof(kHelp[0]) && n < 128; ++i)
        {
        const char* name = kHelp[i].name;
        TUint k = 0;
        while (prefix[k] && name[k] && Up(name[k]) == prefix[k]) ++k;
        if (!prefix[k]) matches[n++] = name;
        }
    static const char* const kExtra[] = { "cls", "erase", "rd", "rename", 0 };
    for (TUint j = 0; kExtra[j] && n < 128; ++j)
        {
        const char* name = kExtra[j];
        TUint k = 0;
        while (prefix[k] && name[k] && Up(name[k]) == prefix[k]) ++k;
        if (!prefix[k]) matches[n++] = name;
        }
    if (!n) return;
    TUint common = 0;
    for (;;)
        {
        char ch = Up(matches[0][common]);
        if (!ch) break;
        TBool all = ETrue;
        for (TUint i = 1; i < n; ++i)
            if (Up(matches[i][common]) != ch) { all = EFalse; break; }
        if (!all) break;
        ++common;
        }
    if (common > sLen)
        {
        char done[sizeof(sLine)];
        for (TUint k = 0; k < common; ++k) done[k] = Up(matches[0][k]);
        RedrawLine(done, common);
        }
    if (n > 1)
        {
        EmitRawS("\r\n");
        for (TUint i = 0; i < n; ++i) { EmitUpperS(matches[i]); EmitRawS("\r\n"); }
        FatPrompt();
        EmitRawS(sLine);
        }
    }

// Optional "X:" drive prefix on a path arg: switches to that volume and
// returns the inner path. *aPrev gets the previous drive (0 if none) for
// VolRestore. Returns 0 when the prefix names no mounted volume.
static const char* VolPrefix(const char* aPath, char* aPrev)
    {
    *aPrev = FatCurLetter();
    if (aPath && aPath[0] && aPath[1] == ':')
        {
        char l = aPath[0];
        if (l >= 'a' && l <= 'z')
            l -= 32;
        if (l < 'A' || l > 'Z')
            return 0;
        if (FatUse(l) != KErrNone)
            return 0;
        return aPath + 2;
        }
    return aPath;
    }

static void VolRestore(char aPrev)
    {
    if (aPrev)
        FatUse(aPrev);
    }

static TInt WriteMultiline()
    {
    UserFilesLock();
    char prev = FatCurLetter();
    TInt r = FatUse(wDrive);
    if (r == KErrNone) r = FatWritePath(wName, wBuf, wLen);
    VolRestore(prev);
    UserFilesUnlock();
    return r;
    }

static const char* ErrName(TInt r)
    {
    switch (r)
        {
    case KErrNone: return "ok";
    case KErrNotFound: return "not found";
    case KErrGeneral: return "general error";
    case KErrNoMemory: return "disk full";
    case KErrNotSupported: return "not supported";
    case KErrArgument: return "bad argument";
    case KErrTimedOut: return "timed out";
    case KErrNotReady: return "not mounted";
    case KErrCorrupt: return "corrupt";
    case KErrInUse: return "in use (dir not empty?)";
    case KErrAlreadyExists: return "already exists";
    case KErrOverflow: return "too deep";
    case KErrTooBig: return "too big";
    default: return "error";
        }
    }

// The existing FAT reader and E32 loader use bounded buffers. Keep COPY
// bounded as well; do not truncate a source file silently.
static TUint8 sCopyBuf[65536];

static void CopyFile(int argc, char** argv, TBool aRename)
    {
    const char* verb = aRename ? "ren" : "copy";
    int first = 1;
    TBool overwrite = EFalse;
    if (!aRename && argc > 1 && Same(argv[1], "/Y"))
        { overwrite = ETrue; first = 2; }
    if (argc != first + 2)
        { PrintHelp(verb); return; }
    char oldDrive = FatCurLetter(), fromDrive = 0, toDrive = 0;
    const char* src = VolPrefix(argv[first], &fromDrive);
    if (!src || !*src)
        { KPrintf("%s: no such source drive\r\n", verb); VolRestore(oldDrive); return; }
    char srcLetter = FatCurLetter();
    TInt r = FatExistsPath(src);
    if (r != KErrNone)
        { KPrintf("%s: source %s\r\n", verb, r == KErrAlreadyExists ? "is a directory" : ErrName(r));
          VolRestore(oldDrive); return; }
    TUint n = 0;
    if (!aRename)
        {
        r = FatReadPath(src, sCopyBuf, sizeof(sCopyBuf), n);
        if (r != KErrNone)
            { KPrintf("%s: read %s\r\n", verb, ErrName(r)); VolRestore(oldDrive); return; }
        }
    const char* dst = VolPrefix(argv[first + 1], &toDrive);
    if (!dst || !*dst)
        { KPrintf("%s: no such destination drive\r\n", verb); VolRestore(oldDrive); return; }
    if (aRename && srcLetter != FatCurLetter())
        { KPrintf("ren: source and destination must be on the same drive\r\n"); VolRestore(oldDrive); return; }
    if (srcLetter == FatCurLetter() && Same(src, dst))
        { KPrintf("%s: source and destination are the same\r\n", verb); VolRestore(oldDrive); return; }
    if (aRename)
        {
        // FatRenamePath resolves both paths relative to this drive's CWD.
        // The caller may have switched drives to read the source; a rename
        // always requires the same drive and directory.
        r = FatRenamePath(argv[first][1] == ':' ? argv[first] + 2 : argv[first], dst);
        KPrintf("ren: %s\r\n", r == KErrNone ? "done" : ErrName(r));
        VolRestore(oldDrive);
        return;
        }
    r = FatExistsPath(dst);
    if (r == KErrAlreadyExists || (r == KErrNone && !overwrite))
        { KPrintf("%s: destination already exists%s\r\n", verb,
                  aRename ? "" : " (use COPY /Y to overwrite)"); VolRestore(oldDrive); return; }
    if (r != KErrNotFound && r != KErrNone)
        { KPrintf("%s: destination %s\r\n", verb, ErrName(r)); VolRestore(oldDrive); return; }
    r = FatWritePath(dst, sCopyBuf, n);
    if (r != KErrNone)
        { KPrintf("%s: write %s\r\n", verb, ErrName(r)); VolRestore(oldDrive); return; }
    KPrintf("copy: %u bytes\r\n", n);
    VolRestore(oldDrive);
    }

// Two-digit raw emitter for `date` (genuine %02d support is unconfirmed,
// so pad manually through the newline-free path).
static void Emit2(TInt aV)
    {
    EmitRaw((char)('0' + (aV / 10) % 10));
    EmitRaw((char)('0' + aV % 10));
    }

// Four-digit raw emitter (year: genuine KPrintf ends the line per call).
static void Emit4(TInt aV)
    {
    EmitRaw((char)('0' + (aV / 1000) % 10));
    EmitRaw((char)('0' + (aV / 100) % 10));
    Emit2(aV % 100);
    }

static TBool LeapYear(TInt aYear)
    { return (aYear % 4 == 0) && ((aYear % 100 != 0) || (aYear % 400 == 0)); }

static TInt MonthDays(TInt aMon, TInt aYear)
    {
    static const TInt mdays[12] =
        {31,28,31,30,31,30,31,31,30,31,30,31};
    if (aMon < 1 || aMon > 12)
        return 0;
    return mdays[aMon - 1] + ((aMon == 2 && LeapYear(aYear)) ? 1 : 0);
    }

static void PrintDate(TInt aSeconds)
    {
    TInt days = aSeconds / 86400;
    TInt rem = aSeconds % 86400;
    TInt year = 2000;
    for (;;)
        {
        TInt ydays = LeapYear(year) ? 366 : 365;
        if (days < ydays) break;
        days -= ydays;
        ++year;
        }
    TInt mon = 1;
    for (;;)
        {
        TInt md = MonthDays(mon, year);
        if (days < md) break;
        days -= md;
        ++mon;
        }
    Emit4(year);
    EmitRaw('-');
    Emit2(mon);
    EmitRaw('-');
    Emit2(days + 1);
    EmitRaw(' ');
    Emit2(rem / 3600);
    EmitRaw(':');
    Emit2((rem % 3600) / 60);
    EmitRaw(':');
    Emit2(rem % 60);
    EmitRawS("\r\n");
    }

static TBool ParseDigits(const char* aText, TInt aCount, TInt& aOut)
    {
    TInt v = 0;
    for (TInt i = 0; i < aCount; ++i)
        {
        char c = aText[i];
        if (c < '0' || c > '9')
            return EFalse;
        v = v * 10 + (c - '0');
        }
    aOut = v;
    return ETrue;
    }

// Strict YYYY-MM-DD, CMOS range 2000-2099, real calendar day.
static TBool ParseDate(const char* aText, TInt& aYear, TInt& aMon, TInt& aDay)
    {
    if (!aText)
        return EFalse;
    TInt n = 0;
    while (aText[n]) ++n;
    if (n != 10 || aText[4] != '-' || aText[7] != '-')
        return EFalse;
    if (!ParseDigits(aText, 4, aYear) || !ParseDigits(aText + 5, 2, aMon) ||
        !ParseDigits(aText + 8, 2, aDay))
        return EFalse;
    if (aYear < 2000 || aYear > 2099 || aMon < 1 || aMon > 12)
        return EFalse;
    TInt md = MonthDays(aMon, aYear);
    return (aDay >= 1 && aDay <= md) ? ETrue : EFalse;
    }

// Strict HH:MM:SS.
static TBool ParseTime(const char* aText, TInt& aHour, TInt& aMin, TInt& aSec)
    {
    if (!aText)
        return EFalse;
    TInt n = 0;
    while (aText[n]) ++n;
    if (n != 8 || aText[2] != ':' || aText[5] != ':')
        return EFalse;
    if (!ParseDigits(aText, 2, aHour) || !ParseDigits(aText + 3, 2, aMin) ||
        !ParseDigits(aText + 6, 2, aSec))
        return EFalse;
    return (aHour <= 23 && aMin <= 59 && aSec <= 59) ? ETrue : EFalse;
    }

// Ctrl+C / ABORT: kill the foreground E32 child (if any) and come back to
// the prompt. Reclaiming the console lease unconditionally is safe: the
// victim is being killed and can no longer consume input.
static void AbortForeground()
    {
    sAbortPending = EFalse;
    TUint pid = sForegroundPid;
    sForegroundPid = 0;
    if (pid)
        {
        TInt r = E32Kill(pid);
        if (r == KErrNone)
            KPrintf("\r\naborted pid=%u\r\n", pid);
        else
            KPrintf("\r\naborted (kill %u: %s)\r\n", pid, ErrName(r));
        }
    else
        KPrintf("\r\naborted\r\n");
    if (ConInputLeased())
        ConForceRelease();
    sLen = 0;
    }

// Called from the shell's input loop: only acts when an input source has
// raised the ^C flag. The lease branch calls this before halting so a
// program that owns the console cannot hide the request from the shell.
// The typed ABORT command reprompts via the main loop instead.
static void CheckAbort()
    {
    if (sAbortPending)
        {
        AbortForeground();
        // Let the main loop emit exactly one prompt now that the console is
        // free (avoids doubling with the lease-release edge below).
        sPromptNeeded = ETrue;
        }
    }

// Programs that repaint the whole VBE framebuffer. While one of these runs,
// the console->LFB mirror is muted so shell banners, "started" notices and the
// program's own RDebug markers stay on COM1 instead of scrolling the GUI off
// screen. The base name is compared case-insensitively, ignoring any directory
// prefix and the .EXE/.E32 extension.
static TBool IsGraphicsImage(const char* aName)
    {
    static const char* const kGraphics[] = {
        "DESKTOP", "GUISHELL", "GUIMENU", "GUIHELLO", "BREAKOUT", "SAVER",
        "DTCALC", "DTEDIT", "WSDL",
        // DOOM.E32 renders full-screen via the LFB and floods the console
        // with engine printf/DebugPrint during startup. Without this the
        // LFB mirror scrolls text over the game and flickers. DOOM is a
        // single process (no helper), so one owner slot suffices.
        "DOOM"
        };
    if (!aName) return EFalse;
    const char* base = aName;
    for (const char* p = aName; *p; ++p)
        if (*p == '\\' || *p == '/') base = p + 1;
    for (TUint i = 0; i < sizeof(kGraphics) / sizeof(kGraphics[0]); ++i)
        {
        const char* n = kGraphics[i];
        TUint k = 0;
        while (n[k] && Up(base[k]) == n[k]) ++k;
        if (n[k]) continue;
        if (base[k] && base[k] != '.' && base[k] != '\\' && base[k] != '/')
            continue;
        return ETrue;
        }
    return EFalse;
    }

// Promote the boot text console to the VBE LFB exactly when a graphical E32
// image is about to run. display_init() is idempotent (gReady/gTried), so it
// is safe whether or not a boot hook already probed the display. VgaForceText3()
// parked the DISPI LFB at boot, so re-assert the 1024x768x32 geometry here;
// this is also the switch-back path if the hardware was ever returned to text.
static void EnsureVbeGraphics()
    {
    if (!gTextMode)
        return;                      // already switched by an earlier app
    TInt r = display_init();
    if (r != KErrNone || !display_fb_size())
        return;                      // no framebuffer: keep the text console
    DispiWrite(4, 0);                // disable LFB while reprogramming
    DispiWrite(1, 1024);             // XRes
    DispiWrite(2, 768);              // YRes
    DispiWrite(3, 32);               // Bpp
    DispiWrite(5, 0);                // bank
    DispiWrite(6, 1024);             // VirtWidth
    DispiWrite(13, 0);               // YOffset
    DispiWrite(4, 0x41);             // enable | linear framebuffer
    gTextMode = EFalse;
    VgaInit();                       // seed the LFB text mirror at (0,0)
    }

// Run an E32 image as a user process. Symbian-style: the name is tried as
// given; a bare name (no dot) is also tried with ".EXE" and then ".E32"
// appended (real Symbian EXE name wins when both exist). A name that already
// carries an extension, e.g. CALC.EXE, is opened exactly as typed. The bytes
// are still genuine E32 images — .EXE is just the on-disk name. aTail (the
// raw words after the name) becomes the process command line.
static void RunImage(const char* aName, const char* aTail)
    {
    static TUint8 eBuf[512 * 1024];  // fit large E32s (DOOM ~338 KiB) + margin
    char cand[3][164];
    int n = 0;
    while (aName[n] && n < 150) { cand[0][n] = aName[n]; ++n; }
    cand[0][n] = 0;
    int nCand = 1;
    int hasDot = 0;
    for (int i = 0; cand[0][i]; ++i)
        if (cand[0][i] == '.') { hasDot = 1; break; }
    if (!hasDot)
        {
        // Candidate 1: ".EXE" (preferred Symbian spelling), candidate 2: the
        // historical ".E32" spelling. The loop below tries each candidate on
        // the active drive first, so the first format found wins.
        for (int suffix = 0; suffix < 2; ++suffix)
            {
            char* out = cand[1 + suffix];
            int k = 0;
            while (cand[0][k] && k < 150) { out[k] = cand[0][k]; ++k; }
            out[k++] = '.';
            out[k++] = 'E';
            out[k++] = (char)(suffix == 0 ? 'X' : '3');
            out[k++] = (char)(suffix == 0 ? 'E' : '2');
            out[k] = 0;
            }
        nCand = 3;
        }
    TUint got = 0;
    TInt r = KErrNotFound;
    char prev = 0;
    const char* inner = 0;
    for (int c = 0; c < nCand && r != KErrNone; ++c)
        {
        const char* path = cand[c];
        if (path[0] && path[1] == ':')
            {
            // Explicit drive: honour only that volume (existing behaviour).
            char pv = 0;
            const char* in = VolPrefix(path, &pv);
            if (!in || !in[0]) { VolRestore(pv); continue; }
            TUint g = 0;
            TInt rr = FatReadPath(in, eBuf, sizeof(eBuf), g);
            if (rr == KErrNone) { r = KErrNone; prev = pv; inner = in; got = g; break; }
            VolRestore(pv);
            }
        else
            {
            // Bare name: try the active drive first, then every other mounted
            // drive. Leave the found volume active for E32Run and restore the
            // caller's drive afterwards.
            char original = FatCurLetter();
            prev = original;
            inner = path;
            TUint g = 0;
            if (FatReadPath(path, eBuf, sizeof(eBuf), g) == KErrNone)
                { r = KErrNone; got = g; break; }
            for (char l = 'A'; l <= 'Z'; ++l)
                {
                if (l == original || FatUse(l) != KErrNone)
                    continue;
                g = 0;
                if (FatReadPath(path, eBuf, sizeof(eBuf), g) == KErrNone)
                    { r = KErrNone; got = g; break; }
                }
            if (r == KErrNone)
                break;
            VolRestore(original);
            }
        }
    if (r != KErrNone)
        { KPrintf("%s: not found\r\n", aName); return; }
    TInt reaped = E32Reap();
    if (reaped)
        KPrintf("reaped %d dead user image(s)\r\n", reaped);
    // Mute the LFB mirror BEFORE E32Run's chatter for graphical images: the
    // child starts on Resume() and its RDebug markers share the same console
    // path, so every launch-time print must be COM1-only from here on.
    const TBool graphics = IsGraphicsImage(aName);
    if (graphics)
        {
        // Bring the framebuffer up BEFORE E32Run: DOOM/GUISHELL render straight
        // into the LFB from their first instruction after Resume().
        EnsureVbeGraphics();
        QemuFbMirrorSet(1);
        sChildTookMirror = EFalse;
        DThread* self = &Kern::CurrentThread();
        sMirrorShellPid = (self && self->iOwningProcess)
                        ? (TUint)self->iOwningProcess->iId : 0u;
        }
    TInt rr = E32Run(eBuf, got, inner, aTail);
    if (rr == KErrNone)
        {
        sForegroundPid = E32LastPid();
        if (graphics)
            AddGuiMirrorOwner(sForegroundPid);   // resume when all owners die
        sJustLaunched = ETrue;   // post-Exec prompt must be deferred if leased
        KPrintf("%s started (user process, pid=%u)\r\n", aName, sForegroundPid);
        }
    else
        {
        if (graphics)
            { QemuFbMirrorSet(0); VgaInit(); }
        KPrintf("%s (%s)\r\n", aName, ErrName(rr));
        }
    VolRestore(prev);
    }

// D-06 NIC bring-up test: build a 42-byte Ethernet/ARP "who-has 10.0.2.2"
// request, post it with e1000_tx(), then poll e1000_recv() for QEMU SLIRP's
// ARP reply. Repeats aCount times to prove sustained TX/RX (the RX tail fix).
static void NicArpTest(TInt aCount)
    {
    TUint8 mac[6];
    e1000_read_mac(mac);
    TUint8 frame[64];
    memset(frame, 0, sizeof(frame));
    for (TInt i = 0; i < 6; ++i)
        { frame[i] = 0xFF; frame[6 + i] = mac[i]; }
    frame[12] = 0x08; frame[13] = 0x06;             // EtherType ARP
    frame[14] = 0x00; frame[15] = 0x01;             // ARP hardware type Ethernet
    frame[16] = 0x08; frame[17] = 0x00;             // ARP protocol type IPv4
    frame[18] = 6; frame[19] = 4;                   // hardware/protocol lengths
    frame[20] = 0x00; frame[21] = 0x01;             // ARP operation: request
    for (TInt i = 0; i < 6; ++i)
        frame[22 + i] = mac[i];                      // sender hardware address
    frame[28] = 10; frame[29] = 0; frame[30] = 2; frame[31] = 15;  // 10.0.2.15
    frame[38] = 10; frame[39] = 0; frame[40] = 2; frame[41] = 2;   // 10.0.2.2

    TInt passed = 0;
    for (TInt attempt = 0; attempt < aCount; ++attempt)
        {
        TInt tr = e1000_tx(frame, 42);
        KPrintf("nic arp: try %d tx=%d (%s)\r\n", attempt + 1, tr,
            tr == KErrNone ? "OK" : "FAIL");
        if (tr != KErrNone)
            continue;
        TUint8 rx[256];
        for (TInt poll = 0; poll < 400; ++poll)
            {
            NKern::Sleep(10);                        // 10ms tick; ~4s max
            TInt n = e1000_recv(rx, sizeof(rx));
            if (n >= 42 && rx[12] == 0x08 && rx[13] == 0x06 &&
                rx[20] == 0x00 && rx[21] == 0x02)    // ARP reply
                {
                KPrintf("nic arp: try %d RX reply len=%d sha=%02x:%02x:%02x:%02x:%02x:%02x spa=%u.%u.%u.%u tpa=%u.%u.%u.%u\r\n",
                    attempt + 1, n, rx[22], rx[23], rx[24], rx[25],
                    rx[26], rx[27], rx[28], rx[29], rx[30], rx[31],
                    rx[38], rx[39], rx[40], rx[41]);
                ++passed;
                break;
                }
            }
        }
    KPrintf("nic arp: RESULT %s (%d/%d ARP replies)\r\n",
        passed == aCount ? "PASS" : "FAIL", passed, aCount);
    }

static void Exec()
    {
    sLine[sLen] = 0;
    // Keep EXEC's command line verbatim before the generic tokenizer
    // inserts NULs and reaches its eight-word limit.
    static char original[sizeof(sLine)];
    memcpy(original, sLine, sLen + 1);
    const char* execTail = "";
    // Tokenize into argv (in place; `write` rejoins the tail).
    // Double quotes group spaces: type "long name.txt".
    char* argv[8];
    int argc = 0;
    for (char* p = sLine; *p && argc < 8;)
        {
        while (*p == ' ' || *p == '\t')
            { *p = 0; ++p; }
        if (!*p)
            break;
        if (*p == '"')
            {
            *p = 0;
            ++p;
            argv[argc++] = p;
            while (*p && *p != '"')
                ++p;
            if (*p == '"')
                { *p = 0; ++p; }
            }
        else
            {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t')
                ++p;
            }
        if (argc == 1)
            {
            // Raw words after the first token become a program's command
            // line (DOS-style implicit execution).
            execTail = original + (p - sLine);
            while (*execTail == ' ' || *execTail == '\t')
                ++execTail;
            }
        }
    if (!argc)
        return;
    // Save the raw first token (case preserved) for implicit execution.
    char progName[128];
    {
    int k = 0;
    for (const char* p = argv[0]; *p && k < 127; ++p)
        progName[k++] = *p;
    progName[k] = 0;
    }
    for (char* p = argv[0]; *p; ++p)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    const char* cmd = argv[0];
    if (argc == 2 && Same(argv[1], "/?"))
        { PrintHelp(cmd); return; }
    // Bare "X:" switches to drive X (DOS-style), e.g. `d:`.
    if (cmd[0] && cmd[1] == ':' && !cmd[2] &&
        ((cmd[0] >= 'A' && cmd[0] <= 'Z') || (cmd[0] >= 'a' && cmd[0] <= 'z')))
        {
        char l = (char)(cmd[0] & ~0x20);
        TInt r = FatUse(l);
        if (r != KErrNone)
            KPrintf("%c: no such drive\r\n", l);
        return;
        }
    if (StrEq(cmd, "help"))
        PrintHelp(argc > 1 ? argv[1] : 0);
    else if (StrEq(cmd, "cls"))
        { VgaInit(); EmitRawS("\033[2J\033[H"); }
    else if (StrEq(cmd, "copy"))
        CopyFile(argc, argv, EFalse);
    else if (StrEq(cmd, "ren") || StrEq(cmd, "rename"))
        CopyFile(argc, argv, ETrue);
    else if (StrEq(cmd, "version"))
        {
        KPrintf("Symbian EKA2 full kernel (genuine), x86 bare-metal on QEMU i386\r\n");
        TUint cr0;
        __asm__ __volatile__("mov %0, cr0" : "=r"(cr0));
        KPrintf("  feats=0x%x CR0=0x%x tick=1ms\r\n", qemu_cpu_features, cr0);
        }
    else if (StrEq(cmd, "uptime"))
        {
        TUint ms = NKern::TickCount();
        KPrintf("uptime: %u ms (%u s)\r\n", ms, ms / 1000u);
        }
    else if (StrEq(cmd, "meminfo"))
        {
        // NB: RHeapK::Available() faults KErrNotSupported by design on the
        // fixed kernel heap; report totals from HeapInfo + RHeapK instead.
        TInt ramFreeBlocks = MM::RamAllocator ? MM::RamAllocator->Avail() : -1;
        KPrintf("heap: base=0x%x max=%d total=%d\r\n",
            (TUint)K::HeapInfo.iBase, K::HeapInfo.iMaxSize,
            ((RHeapK*)K::Allocator)->TotalAllocSize());
        KPrintf("ram: free=%d blocks (%d bytes) user pool\r\n",
            ramFreeBlocks, ramFreeBlocks * 4096);
        KPrintf("objs: threads=%d procs=%d chunks=%d timers=%d\r\n",
            K::Containers[EThread]->Count(), K::Containers[EProcess]->Count(),
            K::Containers[EChunk]->Count(), K::Containers[ETimer]->Count());
        KPrintf("msgpool: free=%d\r\n", K::MsgInfo.iFreeMessageCount);
        }
    else if (StrEq(cmd, "heaptest"))
        {
        // RHeapK alloc/free sanity: total must rise then fall back.
        RHeapK* hk = (RHeapK*)K::Allocator;
        TInt b0 = hk->TotalAllocSize();
        TUint8* p = (TUint8*)Kern::AllocZ(2268);
        TInt b1 = hk->TotalAllocSize();
        if (p)
            Kern::Free(p);
        TInt b2 = hk->TotalAllocSize();
        KPrintf("heaptest: %d -> %d -> %d\r\n", b0, b1, b2);
        }
    else if (StrEq(cmd, "pci"))
        {
        PciList();
        }
    else if (StrEq(cmd, "display"))
        {
        if (argc > 1 && StrEq(argv[1], "test"))
            display_test();
        else
            display_info();
        }
    else if (StrEq(cmd, "vbe"))
        {
        if (argc < 2)
            display_mode_list();
        else
            {
            TInt mode = 0;
            if (!ParseVbeMode(argv[1], mode))
                KPrintf("vbe: invalid mode '%s' (try 118, 115, 117)\r\n", argv[1]);
            else
                {
                TInt r = display_set_mode(mode);
                if (r == KErrNone)
                    display_info();
                else
                    KPrintf("vbe: cannot switch to 0x%x (r=%d)\r\n", mode, r);
                }
            }
        }
    else if (StrEq(cmd, "mouse"))
        {
        if (argc > 1 && StrEq(argv[1], "poll"))
            mouse_poll();
        else if (argc > 1 && StrEq(argv[1], "test"))
            {
            TInt r = mouse_test();
            KPrintf("mouse test: %s\r\n", r == KErrNone ? "PASS" : "FAIL");
            }
        else if (argc > 1 && StrEq(argv[1], "shared"))
            {
            TUint b = 0, s = 0;
            KPrintf("mouse shared: buttons=%u (1=L 2=R 4=M) button_seq=%u\r\n", b, s);
            }
        else if (argc > 1 && StrEq(argv[1], "press"))
            {
            TUint mask = 1u;   /* default: left button */
            if (argc > 2 && argv[2][0] >= '0' && argv[2][0] <= '7')
                mask = (TUint)(argv[2][0] - '0');
            mouse_inject_buttons(mask);
            }
        else
            mouse_info();
        }
    else if (StrEq(cmd, "kbd"))
        {
        if (argc > 1 && StrEq(argv[1], "arm"))
            QemuKbdArm();
        TUint isr = 0, sc = 0, keys = 0, imr = 0, st = 0;
        QemuKbdStats(isr, sc, keys, imr, st);
        KPrintf("kbd: PS/2 IRQ1 isr=%u scancodes=%u keys=%u\r\n",
            isr, sc, keys);
        KPrintf("kbd: PIC IMR=0x%02x IRQ1=%s 8042 status=0x%02x outfull=%d\r\n",
            imr, (imr & 0x02u) ? "MASKED" : "enabled", st, (st & 0x01u) ? 1 : 0);
        KPrintf("kbd: input ring head=%u tail=%u (PS/2+COM1+USB)\r\n",
            sHead, sTail);
        }
    else if (StrEq(cmd, "nic"))
        {
        TInt r = e1000_init();                       // idempotent boot init
        if (r != KErrNone)
            { KPrintf("nic: e1000_init failed %d\r\n", r); }
        else
            {
            TUint8 mac[6];
            e1000_read_mac(mac);
            KPrintf("nic: e1000 MAC=%02x:%02x:%02x:%02x:%02x:%02x link=%d\r\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                (TInt)e1000_link_up());
            if (argc > 1 && StrEq(argv[1], "arp"))
                {
                TInt count = 3;
                if (argc > 2 && argv[2][0] >= '1' && argv[2][0] <= '9')
                    count = argv[2][0] - '0';
                NicArpTest(count);
                }
            }
        }
    else if (StrEq(cmd, "vnet"))
        {
        TInt r = vnet_init();                       // idempotent boot init
        if (r != KErrNone)
            KPrintf("vnet: init failed %d\r\n", r);
        else
            {
            vnet_info();
            if (argc > 1 && StrEq(argv[1], "info"))
                KPrintf("vnet: info ok\r\n");
            }
        }
    else if (StrEq(cmd, "vblk"))
        {
        TInt r = vblk_init();                       // idempotent boot init
        if (r != KErrNone)
            KPrintf("vblk: init failed %d\r\n", r);
        else if (argc > 1 && StrEq(argv[1], "test"))
            {
            // Read sector 0 through the virtqueue and show its first bytes.
            static TUint8 vbBuf[512];
            static const TUint8 kWmagic[8] =
                { 'V','B','L','K','W','R','T','!' };
            memset(vbBuf, 0, sizeof(vbBuf));
            r = vblk_read(0, vbBuf, 1);
            KPrintf("vblk test: read=%d first=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                r, vbBuf[0], vbBuf[1], vbBuf[2], vbBuf[3],
                vbBuf[4], vbBuf[5], vbBuf[6], vbBuf[7]);
            // Write a pattern to sector 1, then read it back through the queue.
            memset(vbBuf, 0, sizeof(vbBuf));
            for (TInt i = 0; i < 8; ++i) vbBuf[i] = kWmagic[i];
            TInt wr = vblk_write(1, vbBuf, 1);
            memset(vbBuf, 0, sizeof(vbBuf));
            TInt rb = vblk_read(1, vbBuf, 1);
            KPrintf("vblk test: write=%d readback=%d first=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                wr, rb, vbBuf[0], vbBuf[1], vbBuf[2], vbBuf[3],
                vbBuf[4], vbBuf[5], vbBuf[6], vbBuf[7]);
            }
        else
            {
            vblk_info();
            if (argc > 1 && StrEq(argv[1], "info"))
                KPrintf("vblk: info ok\r\n");
            }
        }
    else if (StrEq(cmd, "rng"))
        {
        TInt r = vrng_init();                       // idempotent boot init
        if (r != KErrNone)
            KPrintf("rng: init failed %d\r\n", r);
        else if (argc > 1 && StrEq(argv[1], "test"))
            {
            // Fetch 16 bytes of hardware entropy through the virtqueue.
            static TUint8 rbBuf[64];
            memset(rbBuf, 0, sizeof(rbBuf));
            r = vrng_get(rbBuf, 16);
            KPrintf("rng test: get=%d bytes=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                r, rbBuf[0], rbBuf[1], rbBuf[2], rbBuf[3],
                rbBuf[4], rbBuf[5], rbBuf[6], rbBuf[7]);
            KPrintf("rng test: bytes=%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                rbBuf[8], rbBuf[9], rbBuf[10], rbBuf[11],
                rbBuf[12], rbBuf[13], rbBuf[14], rbBuf[15]);
            }
        else
            {
            vrng_info();
            if (argc > 1 && StrEq(argv[1], "info"))
                KPrintf("rng: info ok\r\n");
            }
        }
    else if (StrEq(cmd, "netbridge"))
        {
        TInt ms = 6000;
        if (argc > 1)
            {
            TInt n = 0;
            for (const char* q = argv[1]; *q >= '0' && *q <= '9'; ++q)
                n = n * 10 + (*q - '0');
            if (n > 0 && n <= 120000) ms = n;
            }
        NetBridgePollLoop(ms);
        }
    else if (StrEq(cmd, "threads"))
        {
        DObjectCon* c = K::Containers[EThread];
        c->Wait();
        TInt n = c->Count();
        KPrintf("%d threads:\r\n", n);
        for (TInt i = 0; i < n; ++i)
            {
            DThread* t = (DThread*)(*c)[i];
            TBuf<48> name;
            t->FullName(name);
            char narrow[48];
            TInt k = 0;
            for (; k < name.Length() && k < 47; ++k)
                narrow[k] = (char)(name[k] & 0x7F);
            narrow[k] = 0;
            const char* st = "?";
            switch (t->iNThread.iNState)
                {
            case NThreadBase::EReady: st = "READY"; break;
            case NThreadBase::ESuspended: st = "SUSP"; break;
            case NThreadBase::EWaitFastSemaphore: st = "WSEM"; break;
            case NThreadBase::ESleep: st = "SLEEP"; break;
            case NThreadBase::EBlocked: st = "BLOCK"; break;
            case NThreadBase::EDead: st = "DEAD"; break;
            case NThreadBase::EWaitDfc: st = "DFC"; break;
                }
            KPrintf("  %s pri=%d %s\r\n", narrow,
                (TInt)t->iNThread.iPriority, st);
            }
        c->Signal();
        }
    else if (StrEq(cmd, "ps"))
        {
        KPrintf("PortFile channels: %d\r\n", UserFilesChannelCount());
        DObjectCon* c = K::Containers[EProcess];
        c->Wait();
        // Show only LIVE processes. A killed/exited process stays in the
        // container as a zombie until E32Reap drops its code-seg creator ref;
        // DProcess::Die sets iExitType synchronously, so a non-pending exit
        // type is the earliest reliable "not live" signal. Post-mortem status
        // remains available via EXITS / WAIT / REAP.
        TInt n = c->Count();
        TInt live = 0;
        for (TInt i = 0; i < n; ++i)
            {
            DProcess* p = (DProcess*)(*c)[i];
            if (p->iExitType != EExitPending)
                continue;
            ++live;
            }
        KPrintf("%d processes:\r\n", live);
        for (TInt i = 0; i < n; ++i)
            {
            DProcess* p = (DProcess*)(*c)[i];
            if (p->iExitType != EExitPending)
                continue;
            TBuf<48> name;
            p->FullName(name);
            char narrow[48];
            TInt k = 0;
            for (; k < name.Length() && k < 47; ++k)
                narrow[k] = (char)(name[k] & 0x7F);
            narrow[k] = 0;
            KPrintf("  %s id=%d\r\n", narrow, (TInt)p->iId);
            }
        c->Signal();
        }
    else if (StrEq(cmd, "args"))
        {
        // Show a process's genuine command line (DProcess::iCommandLine).
        if (argc < 2)
            PrintHelp("args");
        else
            {
            TUint id = 0;
            for (const char* p = argv[1]; *p >= '0' && *p <= '9'; ++p)
                id = id * 10 + (TUint)(*p - '0');
            NKern::ThreadEnterCS();
            DObjectCon* c = K::Containers[EProcess];
            c->Wait();
            DProcess* found = NULL;
            for (TInt i = 0; i < c->Count(); ++i)
                {
                DProcess* p = (DProcess*)(*c)[i];
                if ((TUint)p->iId == id)
                    { found = p; break; }
                }
            if (!found)
                KPrintf("args: no such process\r\n");
            else if (!found->iCommandLine || !found->iCommandLine->Length())
                KPrintf("args: id=%u (empty)\r\n", id);
            else
                {
                KPrintf("args: id=%u len=%d ", id,
                    found->iCommandLine->Length() / 2);
                const TUint8* t = found->iCommandLine->Ptr();
                for (TInt k = 0; k + 1 < found->iCommandLine->Length(); k += 2)
                    EmitRaw((char)t[k]);
                EmitRawS("\r\n");
                }
            c->Signal();
            NKern::ThreadLeaveCS();
            }
        }
    else if (StrEq(cmd, "pzomb"))
        {
        // Zombie inspector: why does a dead-thread process object linger?
        if (argc < 2)
            PrintHelp("pzomb");
        else
            {
            TUint id = 0;
            for (const char* p = argv[1]; *p >= '0' && *p <= '9'; ++p)
                id = id * 10 + (TUint)(*p - '0');
            NKern::ThreadEnterCS();
            DObjectCon* c = K::Containers[EProcess];
            c->Wait();
            DProcess* found = NULL;
            for (TInt i = 0; i < c->Count(); ++i)
                {
                DProcess* p = (DProcess*)(*c)[i];
                if ((TUint)p->iId == id)
                    { found = p; break; }
                }
            if (!found)
                KPrintf("pzomb: no such process\r\n");
            else
                {
                DThread* ft = found->iThreadQ.IsEmpty() ? NULL
                    : found->FirstThread();
                KPrintf("pzomb id=%u obj=%x thread=%x access=%d exit=%d threadsempty=%d handles=%d/%d\r\n",
                    id, found, ft, found->AccessCount(), (TInt)found->iExitType,
                    (TInt)found->iThreadQ.IsEmpty(),
                    found->iHandles.Count(), found->iHandles.ActiveCount());
                // NOTE: all dereferences above and below happen while the
                // process container lock is held, pinning `found` against
                // destruction (cf. E32Kill which Opens before unlock).
                DMutex* m = c->Lock();
                TThreadMutexCleanup& cu = m->iCleanup;
                DThread* holder = cu.iThread;
                TBuf<32> hn;
                if (holder)
                    holder->FullName(hn);
                char nh[32];
                TInt k = 0;
                for (; k < hn.Length() && k < 31; ++k)
                    nh[k] = (char)(hn[k] & 0x7F);
                nh[k] = 0;
                KPrintf("pzomb conmutex heldby=%s hold=%d wait=%d resetting=%d\r\n",
                    holder ? nh : "(free)", m->iHoldCount, m->iWaitCount,
                    (TInt)m->iResetting);
                DCodeSeg* sg = found->CodeSeg();
                if (sg)
                    KPrintf("pzomb seg=%x access=%d attach=%x markloaded=%d\r\n",
                        sg, sg->iAccessCount, sg->iAttachProcess,
                        (TInt)((sg->iMark & 0x10) != 0));
                else
                    KPrintf("pzomb seg=(none)\r\n");
                }
            c->Signal();
            NKern::ThreadLeaveCS();
            }
        }
    else if (StrEq(cmd, "date"))
        {
        if (argc == 1)
            {
            TInt s = QemuRtcSeconds();
            if (s < 0)
                {
                KPrintf("rtc unreadable\r\n");
                }
            else
                {
                PrintDate(s);
                }
            }
        else if (argc == 3)
            {
            // DATE YYYY-MM-DD HH:MM:SS: validate fully, convert with the
            // same Gregorian rules as the RTC reader, set CMOS, read back.
            TInt year, mon, day, hour, minute, sec;
            if (!ParseDate(argv[1], year, mon, day) ||
                !ParseTime(argv[2], hour, minute, sec))
                {
                KPrintf("date: use DATE YYYY-MM-DD HH:MM:SS\r\n");
                }
            else
                {
                TInt days = 0;
                for (TInt y = 2000; y < year; ++y)
                    days += LeapYear(y) ? 366 : 365;
                for (TInt m = 1; m < mon; ++m)
                    days += MonthDays(m, year);
                days += day - 1;
                TInt want = days * 86400 + hour * 3600 + minute * 60 + sec;
                TInt r = QemuSetRtcSeconds(want);
                if (r != KErrNone)
                    KPrintf("date: %s\r\n", ErrName(r));
                else
                    {
                    TInt back = QemuRtcSeconds();
                    if (back == want)
                        PrintDate(back);
                    else
                        KPrintf("date: set failed (read %d, want %d)\r\n", back, want);
                    }
                }
            }
        else
            PrintHelp("date");
        }
    else if (StrEq(cmd, "ide"))
        {
        int n = AtaCount();
        KPrintf("%d ATA disk(s):\r\n", n);
        for (int d = 0; d < 8; ++d)
            {
            if (!AtaPresent(d))
                continue;
            KPrintf("  dev %d: %s %u sectors (%uMB)%s\r\n", d, AtaModel(d),
                AtaSectors(d), AtaSectors(d) / 2048,
                d >= 4 ? " [AHCI]" : "");
            FatParts(d);
            }
        }
    else if (StrEq(cmd, "ahci"))
        {
        AhciInfo();
        }
    else if (StrEq(cmd, "audio"))
        {
        if (argc > 1 && StrEq(argv[1], "play"))
            {
            TInt r = audio_play_test();
            if (r != KErrNone)
                KPrintf("audio play: %s\r\n", ErrName(r));
            }
        else
            audio_info();
        }
    else if (StrEq(cmd, "xhci"))
        {
        XhciInfo();
        }
    else if (StrEq(cmd, "xhcistart"))
        {
        KPrintf("xhci: start %s\r\n", ErrName(XhciStart()));
        }
    else if (StrEq(cmd, "xhcinoop"))
        {
        KPrintf("xhci: no-op %s\r\n", ErrName(XhciNoOp()));
        }
    else if (StrEq(cmd, "xhciports"))
        {
        XhciPorts();
        }
    else if (StrEq(cmd, "xhcislot"))
        {
        if (argc < 2 || argv[1][0] < '1' || argv[1][0] > '8' || argv[1][1])
            PrintHelp("xhcislot");
        else
            KPrintf("xhci: enable slot %s\r\n",
                ErrName(XhciEnableSlot(argv[1][0] - '1')));
        }
    else if (StrEq(cmd, "xhciaddr"))
        {
        if (argc < 2 || argv[1][0] < '1' || argv[1][0] > '8' || argv[1][1])
            PrintHelp("xhciaddr");
        else
            KPrintf("xhci: address %s\r\n",
                ErrName(XhciAddress(argv[1][0] - '1')));
        }
    else if (StrEq(cmd, "xhcidesc"))
        {
        if (argc < 2 || argv[1][0] < '1' || argv[1][0] > '8' || argv[1][1])
            PrintHelp("xhcidesc");
        else
            KPrintf("xhci: get descriptor %s\r\n",
                ErrName(XhciGetDescriptor(argv[1][0] - '1', 1, 8)));
        }
    else if (StrEq(cmd, "xhcikbd"))
        {
        if (argc < 2 || argv[1][0] < '1' || argv[1][0] > '8' || argv[1][1])
            PrintHelp("xhcikbd");
        else
            KPrintf("xhci: keyboard %s\r\n",
                ErrName(XhciKeyboardInit(argv[1][0] - '1')));
        }
    else if (StrEq(cmd, "ehci"))
        {
        TInt r = ehci_init();                       // idempotent boot init
        if (r != KErrNone)
            KPrintf("ehci: init failed %d\r\n", r);
        else
            {
            ehci_info();
            if (argc > 1 && StrEq(argv[1], "info"))
                KPrintf("ehci: info ok\r\n");
            }
        }
    else if (StrEq(cmd, "usb"))
        {
        if (argc > 1 && StrEq(argv[1], "poll"))
            {
            UsbDebugPoll();
            return;
            }
        if (argc > 1 && StrEq(argv[1], "info"))
            {
            OhciInfo();
            return;
            }
        TInt r = UsbInit();
        if (r == KErrNotSupported)
            {
            TInt xr = XhciAutoKeyboard();
            if (xr != KErrNone && xr != KErrNotFound)
                KPrintf("usb: xhci init %s\r\n", ErrName(xr));
            if (xr == KErrNotFound)
                KPrintf("usb: no USB HID keyboard (xhciports for ports)\r\n");
            }
        else if (r != KErrNone && r != KErrNotFound)
            KPrintf("usb: init %s\r\n", ErrName(r));
        UsbList();
        XhciKeyboardInfo();
        }
    else if (StrEq(cmd, "mount"))
        {
        int dev = (argc > 1) ? (argv[1][0] - '0') : -1;
        int part = (argc > 2) ? (argv[2][0] - '0') : -1;
        if (dev < 0)
            {
            // No args: mount every FAT32 partition found, then list.
            AtaInit();
            for (int d = 0; d < 8; ++d)
                {
                if (!AtaPresent(d))
                    continue;
                for (int p = 0; p < 4; ++p)
                    {
                    if (FatMount(d, p) == KErrNone)
                        KPrintf("mounted %c: dev %d part %d\r\n",
                            FatCurLetter(), d, p);
                    }
                }
            FatList();
            }
        else
            {
            TInt r = FatMount(dev, part);
            if (r == KErrNone)
                KPrintf("mounted %c:\r\n", FatCurLetter());
            else
                KPrintf("mount: %s\r\n", ErrName(r));
            }
        }
    else if (StrEq(cmd, "vol"))
        {
        FatList();
        }
    else if (StrEq(cmd, "drive"))
        {
        if (argc < 2)
            {
            char l = FatCurLetter();
            if (l)
                KPrintf("current drive %c:\r\n", l);
            FatList();
            }
        else
            {
            char l = argv[1][0];
            if (l >= 'a' && l <= 'z')
                l -= 32;
            TInt r = FatUse(l);
            if (r != KErrNone)
                KPrintf("drive: no such drive %c:\r\n", l);
            }
        }
    else if (StrEq(cmd, "dir"))
        {
        TBool wide = EFalse;
        const char* path = 0;
        for (int i = 1; i < argc; ++i)
            {
            if (Same(argv[i], "/W")) wide = ETrue;
            else if (!path) path = argv[i];
            else { PrintHelp("dir"); return; }
            }
        char prev = 0;
        const char* arg = path ? VolPrefix(path, &prev) : 0;
        if (path && !arg)
            KPrintf("dir: no such drive\r\n");
        else
            {
            TInt r = FatDirPath(arg, wide);
            if (r != KErrNone)
                KPrintf("dir: %s\r\n", ErrName(r));
            }
        VolRestore(prev);
        }
    else if (StrEq(cmd, "cd"))
        {
        // `cd D:` / `cd D:dir` switches drives (stays switched); plain cd is local.
        char prev = 0;
        const char* arg = (argc > 1) ? VolPrefix(argv[1], &prev) : 0;
        if (argc > 1 && !arg)
            KPrintf("cd: no such drive\r\n");
        else
            {
            TInt r = FatCdPath(arg);
            if (r != KErrNone)
                {
                KPrintf("cd: %s\r\n", ErrName(r));
                VolRestore(prev);
                }
            }
        }
    else if (StrEq(cmd, "type"))
        {
        if (argc < 2)
            PrintHelp("type");
        else
            {
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("type: no such drive\r\n");
            else
                {
                TInt r = FatTypePath(name);
                if (r != KErrNone)
                    KPrintf("type: %s\r\n", ErrName(r));
                }
            VolRestore(prev);
            }
        }
    else if (StrEq(cmd, "write"))
        {
        if (argc == 2)
            {
            // Multi-line mode: lines accumulate until a lone ".".
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("write: no such drive\r\n");
            else
                StartMultiline(name, FatCurLetter());
            VolRestore(prev);
            }
        else if (argc < 3)
            PrintHelp("write");
        else
            {
            // Rejoin the tail with single spaces + CRLF.
            char text[512];
            int k = 0;
            for (int i = 2; i < argc && k < 498; ++i)
                {
                if (i > 2 && k < 498)
                    text[k++] = ' ';
                for (const char* p = argv[i]; *p && k < 498; ++p)
                    text[k++] = *p;
                }
            text[k++] = '\r';
            text[k++] = '\n';
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("write: no such drive\r\n");
            else
                {
                TInt r = FatWritePath(name, (const TUint8*)text, (TUint)k);
                if (r == KErrNone)
                    KPrintf("wrote %d bytes\r\n", k);
                else
                    KPrintf("write: %s\r\n", ErrName(r));
                }
            VolRestore(prev);
            }
        }
    else if (StrEq(cmd, "del") || StrEq(cmd, "erase") ||
             StrEq(cmd, "rmdir") || StrEq(cmd, "rd"))
        {
        TBool dirOnly = StrEq(cmd, "rmdir") || StrEq(cmd, "rd");
        if (argc < 2)
            PrintHelp(dirOnly ? "rmdir" : "del");
        else
            {
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("del: no such drive\r\n");
            else
                {
                TInt r = dirOnly ? FatExistsPath(name) : KErrAlreadyExists;
                if (dirOnly && r == KErrNone)
                    KPrintf("rmdir: not a directory\r\n");
                else if (dirOnly && r != KErrAlreadyExists)
                    KPrintf("rmdir: %s\r\n", ErrName(r));
                else
                    {
                    r = FatDelPath(name);
                if (r == KErrNone)
                    KPrintf("deleted\r\n");
                else
                    KPrintf("del: %s\r\n", ErrName(r));
                    }
                }
            VolRestore(prev);
            }
        }
    else if (StrEq(cmd, "mkdir") || StrEq(cmd, "md"))
        {
        if (argc < 2)
            PrintHelp("mkdir");
        else
            {
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("mkdir: no such drive\r\n");
            else
                {
                TInt r = FatMkdirPath(name);
                if (r == KErrNone)
                    KPrintf("created\r\n");
                else
                    KPrintf("mkdir: %s\r\n", ErrName(r));
                }
            VolRestore(prev);
            }
        }
    else if (StrEq(cmd, "format"))
        {
        if (argc < 3 || !Same(argv[2], "yes"))
            PrintHelp("format");
        else
            {
            int dev = argv[1][0] - '0';
            KPrintf("formatting dev %d...\r\n", dev);
            TInt r = FatFormat(dev);
            if (r == KErrNone)
                KPrintf("formatted + mounted %c:\r\n", FatCurLetter());
            else
                KPrintf("format: %s\r\n", ErrName(r));
            }
        }
    else if (StrEq(cmd, "kill"))
        {
        // Kill a user process by container id (see ps). Never the kernel.
        if (argc < 2)
            PrintHelp("kill");
        else
            {
            TUint id = 0;
            for (const char* p = argv[1]; *p >= '0' && *p <= '9'; ++p)
                id = id * 10 + (TUint)(*p - '0');
            TInt r = E32Kill(id);
            if (r == KErrNone)
                KPrintf("killed %u\r\n", id);
            else
                KPrintf("kill: %s\r\n", ErrName(r));
            }
        }
    else if (StrEq(cmd, "abort") || StrEq(cmd, "ctrlc") || Same(cmd, "!"))
        {
        // Portable fallback for hosts where QEMU serial swallows byte 0x03:
        // kill the foreground child directly (see also ^C on the console).
        AbortForeground();
        }
    else if (StrEq(cmd, "gboot"))
        {
        TInt r = R1GCommand(argc == 2 ? argv[1] : "help");
        KPrintf("gboot: result=%d\r\n", r);
        }
    else if (StrEq(cmd, "reap"))
        {
        // Post-mortem code-seg cleanup (also runs automatically on exec).
        TInt n = E32Reap();
        KPrintf("reaped %d\r\n", n);
        }
    else if (StrEq(cmd, "exits"))
        {
        // Recent exit records: survive reaping, unlike `ps` rows.
        E32PrintExits();
        }
    else if (StrEq(cmd, "wait"))
        {
        // Block until a user process dies or the timeout elapses, then
        // report its recorded exit. Polls liveness with hlt between checks
        // (IRQs wake us ~1ms); bounded so a spinner cannot hang the shell.
        if (argc < 2)
            PrintHelp("wait");
        else
            {
            TUint id = 0;
            for (const char* p = argv[1]; *p >= '0' && *p <= '9'; ++p)
                id = id * 10 + (TUint)(*p - '0');
            TUint seconds = 30;
            if (argc >= 3)
                {
                seconds = 0;
                for (const char* p = argv[2]; *p >= '0' && *p <= '9'; ++p)
                    seconds = seconds * 10 + (TUint)(*p - '0');
                if (seconds > 300) seconds = 300;
                }
            TUint32 t0 = NKern::TickCount();
            TBool done = EFalse;
            for (;;)
                {
                if (E32ProcessDead(id))
                    {
                    // Reap first so the ring (and `ps`) settle, then report.
                    E32Reap();
                    TUint8 type = 0;
                    TInt reason = 0;
                    char name[32];
                    if (E32FindExit(id, type, reason, name))
                        KPrintf("wait: %s id=%u type=%d reason=%d\r\n",
                            name[0] ? name : "?", id,
                            (TInt)type, reason);
                    else
                        KPrintf("wait: id=%u dead, no record\r\n", id);
                    done = ETrue;
                    break;
                    }
                if ((TUint32)(NKern::TickCount() - t0) >= seconds * 1000u)
                    break;
                __asm__ __volatile__("hlt");
                }
            if (!done)
                KPrintf("wait: id=%u still alive after %u s\r\n", id, seconds);
            }
        }
    else if (StrEq(cmd, "segs"))
        {
        // Code-seg census: every live DCodeSeg with its refcount.
        DCodeSeg::Wait();
        TInt n = 0;
        for (SDblQueLink* pL = DCodeSeg::GlobalList.First();
             pL != &DCodeSeg::GlobalList.iA; pL = pL->iNext)
            {
            DCodeSeg* pS = _LOFF(pL, DCodeSeg, iLink);
            char narrow[40];
            TInt k = 0;
            if (pS->iFileName)
                {
                const TText* fn = pS->iFileName->Ptr();
                TInt fl = pS->iFileName->Length();
                for (; k < fl && k < 39; ++k)
                    narrow[k] = (char)(fn[k] & 0x7F);
                }
            narrow[k] = 0;
            KPrintf("  %s access=%d attach=%d\r\n",
                narrow, pS->iAccessCount,
                pS->iAttachProcess ? (TInt)pS->iAttachProcess->iId : -1);
            ++n;
            }
        DCodeSeg::Signal();
        KPrintf("%d code segs\r\n", n);
        }
    else if (StrEq(cmd, "reboot"))
        {
        KPrintf("rebooting via 8042...\r\n");
        TUint32 t0 = NKern::TickCount();
        while ((TUint32)(NKern::TickCount() - t0) < 200) { }
        Out8(0x64, 0xFE);
        KPrintf("reboot failed (8042 ignored), halting\r\n");
        for (;;)
            __asm__ __volatile__("hlt");
        }
    else
        // DOS-style: an unrecognised first word is a program to run.
        RunImage(progName, execTail);
    }

// ------------------------------------------------------------ boot-to-desktop
// GRUB's "Symbian EKA2 Desktop (graphical)" entry boots with
//   multiboot /boot/kernfull.elf desktop=1
// so Multiboot1 flags bit 2 is set and MBI+16 points at the NUL-terminated
// command line. BootInfo::Capture() saved the MBI pointer in HwInit0; latch
// the token in a constructor (also run from HwInit0) so it is stable before
// SVCMGR or the user-page allocator can recycle GRUB's low-memory strings.
static TBool sBootDesktop = EFalse;

static TBool CmdlineHasToken(const char* aCmd, const char* aToken)
    {
    if (!aCmd) return EFalse;
    for (const char* p = aCmd; *p; ++p)
        {
        const char* t = aToken;
        const char* q = p;
        while (*t && Up(*q) == Up(*t)) { ++q; ++t; }
        if (!*t) return ETrue;
        }
    return EFalse;
    }

static TBool ReadBootDesktopFlag()
    {
    TUint mbi = BootInfo::infoAddr;
    if (mbi < 0x1000u) return EFalse;
    if (!(*(volatile TUint*)mbi & 0x04u)) return EFalse;  // MBI cmdline present
    TUint cp = *(volatile TUint*)(mbi + 16);
    if (cp < 0x1000u) return EFalse;
    return CmdlineHasToken((const char*)cp, "desktop");
    }

__attribute__((constructor)) static void LatchBootDesktopFlag()
    { sBootDesktop = ReadBootDesktopFlag(); }

static void ShellMain(TAny* /*aPtr*/)
    {
    // Asic::Init1 re-masks the whole 8259 after boot constructors run, and a
    // key pressed during boot can strand a scancode in the 8042 output buffer.
    // Re-arm PS/2 IRQ1 here (idempotent) before the input loop starts so the
    // QEMU VGA window and COM1 are both live from the first prompt.
    QemuKbdArm();
    // Re-assert the BIOS 80x25 text scanout before the first prompt. Boot-time
    // display hooks may have already programmed the Bochs VBE LFB; this parks
    // it and makes 0xB8000 the visible screen again. From here on all console
    // output stays in kVram until a graphical app calls EnsureVbeGraphics().
    VgaForceText3();
    VgaInit();
    TInt files = UserFilesInit();
    if (files != KErrNone)
        { KPrintf("PortFile init failed %d\r\n", files); return; }
    TInt con = UserConInit();
    if (con != KErrNone)
        { KPrintf("PortCon init failed %d\r\n", con); return; }
    // GRUB "Desktop (graphical)" entry: pass desktop=1 on the Multiboot
    // cmdline to auto-run DESKTOP.E32 (from the same FAT32 programs image)
    // instead of waiting at the text-shell prompt.
    if (sBootDesktop)
        {
        KPrintf("Boot desktop: starting DESKTOP.E32...\r\n");
        RunImage("DESKTOP", "");
        }
    KPrintf("Symbian EKA2 x86 full kernel -- interactive console\r\n");
    KPrintf("sched by TheScheduler, IRQs via genuine vectors + variant Bind\r\n");
    FatPrompt();
    for (;;)
        {
        // Ctrl+C is latched by the input ISR/poll even while a program owns
        // the console, so check it at the top of every iteration before
        // deciding whether to sleep. A ^C kills the foreground child and
        // returns us to the prompt regardless of who holds the lease.
        CheckAbort();
        // A graphical child has exited: unmute the LFB mirror, wipe the GUI
        // pixels and repaint the shell prompt. gTextMode stays FALSE: there is
        // no real-mode int10 thunk to switch the hardware back to text, so the
        // console keeps going to both the dead 0xB8000 buffer and the LFB.
        // E32ProcessDead() also covers a
        // GUI app that never leased the console, so the mirror is never left
        // muted after it leaves the screen.
        if (sGuiMirrorCount && ReapGuiMirrorOwners() && !sGuiMirrorCount)
            {
            if (sChildTookMirror)
                {
                // A non-graphics child already resumed the mirror and painted
                // its text; clearing here would erase it before the user can
                // see it.  Just append the prompt below the child's output.
                sChildTookMirror = EFalse;
                QemuFbMirrorSet(0);
                }
            else
                {
                QemuFbMirrorSet(0);
                VgaInit();
                }
            sPromptNeeded = ETrue;
            }
        // While a user program leases console input, the shell must not
        // touch the stream (it would steal program keystrokes). A dead
        // owner is reclaimed here so a crash without release cannot wedge
        // input forever; the next keystroke then reaches the shell.
        TBool leased = ConInputLeased();
        if (leased)
            {
            if (!ConOwnerAlive())
                {
                ConForceRelease();
                KPrintf("\r\nconsole lease released (owner died)\r\n");
                leased = EFalse;
                }
            else
                {
                sWasLeased = ETrue;
                __asm__ __volatile__("hlt");
                continue;
                }
            }
        if (sWasLeased)
            {
            // The child called con_release(): the console is ours again.
            sWasLeased = EFalse;
            sPromptNeeded = ETrue;
            }
        if (sPromptNeeded)
            {
            sPromptNeeded = EFalse;
            FatPrompt();
            }
        int c = KbdPoll();
        if (c < 0) c = SerialPoll();  // fallback: IRQ4 normally fills the ring
        UsbPoll();  // USB HID keyboards feed the USB ring via QemuKbdPushUsb
        XhciKeyboardPoll();
        if (c < 0)
            c = UsbKbdPoll();
        if (c < 0)
            {
            __asm__ __volatile__("hlt");
            continue;
            }
        // Escape sequences from a serial terminal: ESC [ A/B = Up/Down.
        // (PS/2 E0-prefixed arrow scancodes are discarded by the ISR, so this
        // is the path that actually delivers arrow keys to the shell.)
        if (sEscState == 2)
            {
            sEscState = 0;
            if (c == 'A') HistPrev();
            else if (c == 'B') HistNext();
            continue;
            }
        if (c == 27) { sEscState = 1; continue; }
        if (sEscState == 1)
            {
            sEscState = 0;
            if (c == '[') { sEscState = 2; continue; }
            while (sLen > 0) { --sLen; EmitBackspace(); }  // lone ESC clears
            }
        if (c == '\r') c = '\n';
        if (c == '\n')
            {
            KPrintf("\n");
            HistAdd(sLine, sLen);
            sHistBrowsing = EFalse;
            sHistPos = sHistCount;
            if (wMode)
                {
                sLine[sLen] = 0;
                if (sLen == 1 && sLine[0] == '.')
                    {
                    TInt r = WriteMultiline();
                    if (r == KErrNone)
                        KPrintf("wrote %u bytes\r\n", wLen);
                    else
                        KPrintf("write: %s\r\n", ErrName(r));
                    wMode = EFalse;
                    }
                else if (wLen + sLen + 2 < sizeof(wBuf))
                    {
                    for (TUint i = 0; i < sLen; ++i)
                        wBuf[wLen++] = (TUint8)sLine[i];
                    wBuf[wLen++] = '\r';
                    wBuf[wLen++] = '\n';
                    }
                else
                    {
                    TInt r = WriteMultiline();
                    if (r == KErrNone)
                        KPrintf("buffer full, wrote %u bytes\r\n", wLen);
                    else
                        KPrintf("write: %s\r\n", ErrName(r));
                    wMode = EFalse;
                    }
                }
            else
                {
                UserFilesLock();
                Exec();
                UserFilesUnlock();
                }
            sLen = 0;
            if (sJustLaunched)
                {
                // Give the freshly resumed child a few scheduler ticks to
                // acquire the PortCon lease. If it did, it owns the console
                // (and prints its own prompt): defer ours until it releases.
                // Programs that never lease the console keep the old prompt.
                sJustLaunched = EFalse;
                for (TInt k = 0; k < 5 && !ConInputLeased(); ++k)
                    NKern::Sleep(1);
                }
            if (ConInputLeased())
                {
                sWasLeased = ETrue;
                sPromptNeeded = ETrue;
                }
            else
                FatPrompt();
            }
        else if (c == '\b' || c == 127)  // BS or DEL
            {
            if (sLen > 0) { --sLen; EmitBackspace(); }
            }
        else if (c == 16)                // Ctrl-P: previous command
            HistPrev();
        else if (c == 14)                // Ctrl-N: next command
            HistNext();
        else if (c == 9)                 // Tab: complete the first word
            CompleteLine();
        else if (c >= 32 && c < 127)
            {
            if (sLen < sizeof(sLine) - 1)
                {
                sLine[sLen++] = (char)c;
                EmitRaw((char)c);
                }
            }
        }
    }

extern "C" void* memclr(void* aTrg, unsigned aLength);

_LIT(KLitShellName, "Shell");

static TInt ShellDThreadFn(TAny* /*aPtr*/)
    {
    ShellMain(0);
    return 0;
    }

void ShellSpawnFull()
    {
    // Spawned from Asic::Init3 on the supervisor thread as a genuine DThread
    // (EThreadSupervisor, kernel process): DThreads can hold container
    // mutexes, which the `threads`/`ps` commands need. Uses the sanctioned
    // Kern::ThreadCreate path (cf. device drivers), not raw NKern threads.
    SThreadCreateInfo info;
    memclr(&info, sizeof(info));
    info.iType = EThreadSupervisor;
    info.iFunction = ShellDThreadFn;
    info.iPtr = 0;
    info.iSupervisorStack = 0;  // auto-allocate (cf. supervisor thread)
    info.iSupervisorStackSize = 0x8000;
    info.iInitialThreadPriority = 10;
    info.iName.Set(KLitShellName);
    info.iTotalSize = sizeof(info);
    KPrintf("KERN spawning shell DThread...\r\n");
    NKern::ThreadEnterCS();
    TInt r = Kern::ThreadCreate(info);
    DThread* pT = (DThread*)info.iHandle;
    if (r == KErrNone)
        Kern::ThreadResume(*pT);
    NKern::ThreadLeaveCS();
    if (r != KErrNone)
        KPrintf("KERN shell spawn failed: %d\r\n", r);
    }
