// vga.h — text-mode console (Phase 6).
// Writes 80x25 cells at phys 0xB8000 (identity-mapped), scrolls, sets color
// attributes, updates the hardware cursor, and mirrors every byte to COM1
// so headless serial captures stay complete.
#ifndef __QEMU_VGA_H__
#define __QEMU_VGA_H__

#include "qemu_assp.h"

namespace Vga
    {
    static const TUint kCols = 80;
    static const TUint kRows = 25;
    // Color attributes (fg | bg<<4).
    static const TUint8 kText = 0x07;    // light gray on black
    static const TUint8 kPrompt = 0x0E;  // yellow
    static const TUint8 kBanner = 0x0B;  // cyan
    static const TUint8 kError = 0x0C;   // red
    static const TUint8 kDim = 0x08;     // dark gray

    void Init();
    void Clear();
    void SetAttr(TUint8 aAttr);
    void PutC(char aC);          // handles \n \r \b, scrolls, mirrors COM1
    void Puts(const char* aS);
    void PutHex(TUint aV);
    void PutDec(TUint aV);
    }

#endif
