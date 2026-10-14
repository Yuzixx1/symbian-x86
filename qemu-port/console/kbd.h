// kbd.h — PS/2 Set-1 keyboard on IRQ1 (Phase 6).
// ISR translates scancodes to ASCII into a ring buffer; the shell thread
// drains it with Kbd_Poll(). Serial RX is polled separately so headless
// QEMU (-serial stdio) is fully interactive without PS/2.
#ifndef __QEMU_KBD_H__
#define __QEMU_KBD_H__

#include "qemu_assp.h"

namespace Kbd
    {
    void Takeover();          // IDT 0x21 gate + unmask IRQ1
    int Poll();               // -1 if empty, else ASCII byte (PS/2 ring)
    int SerialPoll();         // -1 if empty, else byte from COM1 RBR
    TUint Dropped();          // overflow count (diagnostic)
    }

#endif
