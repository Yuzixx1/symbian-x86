// vga.cpp — 80x25 text driver at 0xB8000.
#include "vga.h"

typedef unsigned short TUint16;

static volatile TUint16* const kVram =
    reinterpret_cast<volatile TUint16*>(0xB8000u);
static TUint sCol = 0, sRow = 0;
static TUint8 sAttr = Vga::kText;

static inline void Out8(TUint16 aPort, TUint8 aVal)
    { __asm__ __volatile__("outb %0,%1" : : "a"(aVal), "Nd"(aPort)); }

static void MoveCursor()
    {
    TUint pos = sRow * Vga::kCols + sCol;
    Out8(0x3D4, 0x0F);
    Out8(0x3D5, (TUint8)(pos & 0xFF));
    Out8(0x3D4, 0x0E);
    Out8(0x3D5, (TUint8)((pos >> 8) & 0xFF));
    }

void Vga::Init()
    {
    Clear();
    }

void Vga::Clear()
    {
    for (TUint i = 0; i < kCols * kRows; ++i)
        kVram[i] = (TUint16)((sAttr << 8) | ' ');
    sCol = 0;
    sRow = 0;
    MoveCursor();
    }

void Vga::SetAttr(TUint8 aAttr) { sAttr = aAttr; }

static void Scroll()
    {
    for (TUint r = 1; r < Vga::kRows; ++r)
        for (TUint c = 0; c < Vga::kCols; ++c)
            kVram[(r - 1) * Vga::kCols + c] = kVram[r * Vga::kCols + c];
    for (TUint c = 0; c < Vga::kCols; ++c)
        kVram[(Vga::kRows - 1) * Vga::kCols + c] =
            (TUint16)((sAttr << 8) | ' ');
    }

static void NewLine()
    {
    sCol = 0;
    if (++sRow >= Vga::kRows)
        {
        sRow = Vga::kRows - 1;
        Scroll();
        }
    MoveCursor();
    }

void Vga::PutC(char aC)
    {
    // Mirror first so serial captures survive any VGA fault.
    QemuUart::PutC(aC);
    if (aC == '\n') { NewLine(); return; }
    if (aC == '\r') { sCol = 0; MoveCursor(); return; }
    if (aC == '\b')
        {
        if (sCol > 0)
            {
            --sCol;
            kVram[sRow * kCols + sCol] = (TUint16)((sAttr << 8) | ' ');
            MoveCursor();
            }
        return;
        }
    kVram[sRow * kCols + sCol] = (TUint16)((sAttr << 8) | (TUint8)aC);
    if (++sCol >= kCols)
        NewLine();
    else
        MoveCursor();
    }

void Vga::Puts(const char* aS) { while (*aS) PutC(*aS++); }

void Vga::PutHex(TUint aV)
    {
    for (int i = 28; i >= 0; i -= 4)
        {
        TUint n = (aV >> i) & 0xF;
        PutC((char)(n < 10 ? '0' + n : 'A' + n - 10));
        }
    }

void Vga::PutDec(TUint aV)
    {
    char buf[12];
    int n = 0;
    if (aV == 0) { PutC('0'); return; }
    while (aV > 0 && n < 11) { buf[n++] = (char)('0' + aV % 10); aV /= 10; }
    while (n > 0) PutC(buf[--n]);
    }
