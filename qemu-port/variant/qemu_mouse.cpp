// qemu_mouse.cpp — real PS/2 mouse (aux port) driver for the QEMU port.
//
// Today the port only has a PS/2 keyboard ring (console/kbd.cpp, IRQ1).  This
// driver adds genuine auxiliary-device input: it programs the 8042 controller
// on I/O 0x64/0x60, enables the mouse (Set-1 style 3-byte packets, optionally
// 4-byte with a wheel), decodes button/delta packets in an IRQ12 ISR, keeps a
// clamped software cursor, and paints a crosshair on the VBE framebuffer via
// display_rect() (variant/qemu_display.cpp).
//
// Interrupt routing: the 16-slot variant dispatcher (qemu_variant_int.cpp)
// owns IRQ12 (slave PIC IR4, remapped to vector 0x38 by Init1).  We Bind our
// ISR with Interrupt::Bind(12, ...) and unmask it with QemuIrq::Enable(12)
// (the direct PIC read-modify-write used by Asic::Init3).  We deliberately do
// NOT use Interrupt::Enable(12): its shadow mask starts at 0xFF/0xFF and would
// write 0xFB/0xEF to the PIC, re-masking the already-live tick/keyboard/COM1
// IRQs.  QemuIrq::Enable reads the *current* mask and clears only IRQ12.
//
// Timing note: the constructor boots at HwInit0 (before Asic::Init1 remaps and
// re-masks the PIC), so its QemuIrq::Enable(12) is later undone by RemapPic.
// mouse_init() is idempotent; the first shell-time call re-binds, un-masks the
// master cascade (IR2) and IRQ12, and flushes stale 8042 bytes so the 8259 sees
// a fresh edge.  `mouse poll` is a polled fallback; `mouse test` injects a
// synthetic AUX packet to verify the IRQ12 path end-to-end.
//
// On every decoded packet the ISR publishes gButtons/gButtonSeq into the
// userland-visible gui_shared via gui_publish_buttons() (qemu_gui_exec.cpp),
// so ring-3 desktops can read clicks without issuing a display-list exec.
//
// Public API (qemu_variant.h): mouse_init / mouse_info / mouse_render_cursor /
// mouse_poll / mouse_test / mouse_inject_buttons.

#include <x86.h>
#include <e32hal.h>
#include <e32err.h>
#include <nk_trace.h>
#include <kernel/kernel.h>
#include "qemu_variant.h"

// Direct PIC mask helper (variant/qemu_assp.cpp) — see note above.
namespace QemuIrq
    {
    void Enable(int aIrq);
    void Disable(int aIrq);
    }

namespace
    {
    const TInt kScreenW = 1024;   // VBE mode 0x118 primary geometry
    const TInt kScreenH = 768;

    // Cursor colours (0x00RRGGBB).  Erase uses the display_test GUI blue so
    // the crosshair can be cleared by simply redrawing its cells.
    const TUint kCursorFg = 0x00FFFF00u;   // yellow crosshair
    const TUint kCursorBg = 0x00000080u;   // GUI background blue

    // 8042 status register bits.
    const TUint8 kStatOutFull = 0x01u;     // output buffer has data for 0x60
    const TUint8 kStatInFull  = 0x02u;     // input buffer still busy
    const TUint8 kStatAux     = 0x20u;     // output byte came from the mouse

    volatile TInt   gX = kScreenW / 2, gY = kScreenH / 2;
    volatile TUint  gButtons = 0;          // bit0=L, bit1=R, bit2=M
    volatile TUint  gButtonSeq = 0;        // increments on each button change
    volatile TUint  gPackets = 0;
    volatile TInt   gWheel = 0;
    volatile TUint8 gMouseId = 0;
    volatile TInt   gPacketSize = 3;
    volatile TBool  gEventPending = EFalse;
    volatile TBool  gInited = EFalse;
    volatile TBool  gAckOk = ETrue;
    volatile TBool  gIrqBound = EFalse;
    volatile TBool  gArmed = EFalse;           // re-arm done from shell context
    volatile TUint  gIsrCount = 0;             // diagnostic: ISR entries
    volatile TUint  gIsrBytes = 0;             // diagnostic: bytes read in ISR

    // Crosshair erase state — shell context only.
    TInt  gPrevX = -1, gPrevY = -1;
    TBool gCursorDrawn = EFalse;

    // Packet assembly — ISR context (or the polled fallback with IRQ off).
    TUint8         gPkt[4];
    volatile TUint gPktIdx = 0;

    inline void Out8(unsigned short aPort, TUint8 aVal)
        { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
    inline TUint8 In8(unsigned short aPort)
        { TUint8 v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

    void KbcWaitInput()
        {
        for (TInt i = 0; i < 100000 && (In8(0x64) & kStatInFull); ++i)
            __asm__ __volatile__("nop");
        }
    void KbcWaitOutput()
        {
        for (TInt i = 0; i < 100000 && !(In8(0x64) & kStatOutFull); ++i)
            __asm__ __volatile__("nop");
        }
    void KbcCmd(TUint8 aCmd)
        { KbcWaitInput(); Out8(0x64, aCmd); }

    void FlushOutput()
        {
        for (TInt i = 0; i < 32; ++i)
            {
            if (!(In8(0x64) & kStatOutFull))
                break;
            (void)In8(0x60);
            }
        }

    // Send a command to the mouse itself (0xD4 then the byte on 0x60) and
    // wait for the 0xFA ACK.  Returns ETrue on ACK.
    TBool MouseCmd(TUint8 aCmd)
        {
        KbcWaitInput(); Out8(0x64, 0xD4u);
        KbcWaitInput(); Out8(0x60, aCmd);
        KbcWaitOutput();
        TUint8 ack = In8(0x60);
        return ack == 0xFAu;
        }

    static void ClampCursor()
        {
        if (gX < 0) gX = 0;
        if (gX >= kScreenW) gX = kScreenW - 1;
        if (gY < 0) gY = 0;
        if (gY >= kScreenH) gY = kScreenH - 1;
        }

    // Feed one raw byte from the auxiliary device into the packet decoder.
    // Runs in IRQ12 context; only volatile scalars are touched.
    void MouseByte(TUint8 aByte)
        {
        if (gPktIdx == 0)
            {
            if (!(aByte & 0x08u))
                return;                       // not a packet header: resync
            gPkt[0] = aByte;
            gPktIdx = 1;
            return;
            }
        gPkt[gPktIdx++] = aByte;
        if (gPktIdx < (TUint)gPacketSize)
            return;

        TUint8 flags = gPkt[0];
        TInt dx = (gPkt[1] & 0x80u) ? (TInt)gPkt[1] - 256 : (TInt)gPkt[1];
        TInt dy = (gPkt[2] & 0x80u) ? (TInt)gPkt[2] - 256 : (TInt)gPkt[2];
        TUint newButtons = (TUint)(flags & 0x07u);
        if (newButtons != gButtons)
            { gButtons = newButtons; ++gButtonSeq; }
        // Publish to the userland-visible shared struct right here in IRQ12
        // context: a click must not depend on anybody running a DRAW exec.
        gui_publish_buttons(gButtons, gButtonSeq);
        gX += dx;
        gY -= dy;                             // PS/2 +dy is "up" on screen
        if (gPacketSize == 4)
            gWheel = (gPkt[3] & 0x80u) ? (TInt)gPkt[3] - 256 : (TInt)gPkt[3];
        ClampCursor();
        ++gPackets;
        gEventPending = ETrue;
        gPktIdx = 0;
        }

    extern "C" void MouseIsr(TAny* /*aPtr*/)
        {
        ++gIsrCount;
        for (TInt i = 0; i < 16; ++i)
            {
            TUint8 st = In8(0x64);
            if (!(st & kStatOutFull))
                break;                        // output empty
            if (!(st & kStatAux))
                break;                        // keyboard byte: leave it for IRQ1
            TUint8 b = In8(0x60);
            ++gIsrBytes;
            MouseByte(b);
            }
        }

    void DrawCursor(TInt aX, TInt aY, TUint aColor)
        {
        TInt hx = aX - 8, hy = aY - 8;
        if (hx < 0) hx = 0;
        if (hy < 0) hy = 0;
        display_rect((TUint)hx, (TUint)aY, (TUint)(17), 1u, aColor);  // horiz arm
        display_rect((TUint)aX, (TUint)hy, 1u, (TUint)(17), aColor);  // vert arm
        display_rect((TUint)aX, (TUint)aY, 3u, 3u, aColor);          // centre dot
        }
    }

TInt mouse_init()
    {
    if (gInited)
        {
        // Re-arm on EVERY call, not just the first.  Init1/Init3 remap and
        // re-mask the PIC after the HwInit0 constructors, so the boot-time
        // arm (including gui_init()'s call) is always undone.  A one-shot
        // guard used to let that boot call consume the only re-arm and leave
        // IRQ12 masked for the whole session — no packets, no clicks.
        gArmed = ETrue;
        // Re-arm path (shell-time call).  Init1 remasked the PIC after the
        // boot constructor ran, so re-bind and re-unmask IRQ12 here.  Drain
        // any stale 8042 output byte FIRST: a byte left in the buffer holds
        // the AUX IRQ line high, and the edge-triggered 8259 would then never
        // see a low->high transition when new mouse packets arrive.
        QemuIrq::Disable(12);
        FlushOutput();
        gPktIdx = 0;
        // IRQ12 is on the slave 8259 (IR4); the master cascade is IR2.  Some
        // boot paths leave IR2 masked, so make sure it is open before relying
        // on any slave IRQ.
        Out8(0x21, (TUint8)(In8(0x21) & ~0x04u));
        Interrupt::Bind(12, MouseIsr, 0);
        QemuIrq::Enable(12);
        gIrqBound = ETrue;
        return KErrNone;
        }
    gInited = ETrue;                          // guard against re-entry

    // --- 8042 / auxiliary-device initialisation (IRQ12 still masked here so
    // ACKs and the device ID cannot be stolen by the ISR) ---
    gAckOk = ETrue;
    KbcCmd(0xADu);                            // disable keyboard
    KbcCmd(0xA7u);                            // disable aux device
    FlushOutput();
    KbcCmd(0xA8u);                            // enable aux device
    KbcCmd(0xAEu);                            // enable keyboard

    // Configuration byte (0x20 read / 0x60 write): set bit 1 = AUX IRQ enable.
    KbcCmd(0x20u);
    KbcWaitOutput();
    TUint8 cfg = In8(0x60);
    cfg |= 0x02u;
    KbcCmd(0x60u);
    KbcWaitInput();
    Out8(0x60, cfg);

    if (!MouseCmd(0xF6u))                     // set default parameters
        gAckOk = EFalse;
    if (!MouseCmd(0xF2u))                     // read device ID
        gAckOk = EFalse;
    KbcWaitOutput();
    TUint8 ack = In8(0x60);                   // expect 0xFA
    (void)ack;
    KbcWaitOutput();
    gMouseId = In8(0x60);                     // 0x00 = 3-byte, 0x03/0x04 = wheel
    gPacketSize = (gMouseId == 0x03u || gMouseId == 0x04u) ? 4 : 3;
    if (!MouseCmd(0xF4u))                     // enable streaming (last)
        gAckOk = EFalse;

    FlushOutput();                            // drop any pre-init stray bytes
    gPktIdx = 0;

    // Bind + unmask here (constructor time).  Init1 will remask the PIC, so
    // this is only a head start; the shell-time re-arm above does the real
    // unmask after the PIC is fully configured.
    Interrupt::Bind(12, MouseIsr, 0);
    QemuIrq::Enable(12);
    gIrqBound = ETrue;

    KPrintf("MOUSE: PS/2 init id=0x%02x pktsize=%d ack=%s irq12=bound\r\n",
        (TUint)gMouseId, (TInt)gPacketSize, gAckOk ? "OK" : "FAIL");
    return KErrNone;
    }

void mouse_render_cursor()
    {
    // Re-publish on every kernel-side touch too: gui_init() zeroes the shared
    // region at boot, so this restores the button word even if no packet has
    // arrived since then (and even when the cursor did not move below).
    gui_publish_buttons(gButtons, gButtonSeq);
    TInt x = (TInt)gX, y = (TInt)gY;
    if (gCursorDrawn)
        {
        if (x == gPrevX && y == gPrevY)
            return;                           // nothing moved
        DrawCursor(gPrevX, gPrevY, kCursorBg); // clear the old crosshair
        }
    DrawCursor(x, y, kCursorFg);
    gPrevX = x;
    gPrevY = y;
    gCursorDrawn = ETrue;
    }

// Called by display_set_mode() after a VBE switch: the old crosshair was
// wiped by display_clear(), so forget its cached position and clamp the
// logical pointer into the new geometry (ISR clamping still uses 0x118 bounds).
void MouseGeometryReset()
    {
    TUint w = 0, h = 0, b = 0;
    display_geometry(w, h, b);
    if (!w || !h)
        return;
    if (gX >= (TInt)w) gX = (TInt)(w / 2);
    if (gY >= (TInt)h) gY = (TInt)(h / 2);
    gCursorDrawn = EFalse;
    }

void mouse_info()
    {
    mouse_init();                             // idempotent; re-arm IRQ12
    mouse_render_cursor();
    const char* lb = (gButtons & 1u) ? "L" : "-";
    const char* rb = (gButtons & 2u) ? "R" : "-";
    const char* mb = (gButtons & 4u) ? "M" : "-";
    KPrintf("mouse: x=%d y=%d buttons=%s%s%s packets=%u wheel=%d pending=%d\r\n",
        (TInt)gX, (TInt)gY, lb, rb, mb, (TUint)gPackets, (TInt)gWheel,
        (TInt)gEventPending);
    KPrintf("mouse: id=0x%02x pktsize=%d ack=%s irq12=%s screen=%dx%d mode=IRQ12\r\n",
        (TUint)gMouseId, (TInt)gPacketSize, gAckOk ? "OK" : "FAIL",
        gIrqBound ? "bound" : "unbound", kScreenW, kScreenH);
    KPrintf("mouse: isr=%u isrbytes=%u\r\n",
        (TUint)gIsrCount, (TUint)gIsrBytes);
    TUint sharedBtns = 0, sharedSeq = 0;
    gui_button_state(sharedBtns, sharedSeq);
    KPrintf("mouse: shared buttons=%u seq=%u (driver buttons=%u seq=%u) %s\r\n",
        sharedBtns, sharedSeq, (TUint)gButtons, (TUint)gButtonSeq,
        (sharedBtns == (TUint)gButtons && sharedSeq == gButtonSeq) ? "OK" : "STALE");
    }

// Polled fallback / diagnostic: drain the 8042 output buffer once with IRQ12
// disabled so the ISR and this routine cannot split a packet.  Normally the
// IRQ12 handler does all of this; `mouse poll` is only needed if IRQ delivery
// is unavailable.
void mouse_poll()
    {
    mouse_init();
    QemuIrq::Disable(12);
    for (TInt i = 0; i < 32; ++i)
        {
        TUint8 st = In8(0x64);
        if (!(st & kStatOutFull))
            break;
        if (!(st & kStatAux))
            break;
        MouseByte(In8(0x60));
        }
    QemuIrq::Enable(12);
    mouse_render_cursor();
    KPrintf("mouse: poll packets=%u x=%d y=%d buttons=%u\r\n",
        (TUint)gPackets, (TInt)gX, (TInt)gY, (TUint)gButtons);
    }

// Self-test: inject one synthetic 3-byte PS/2 packet through the 8042's
// "write AUX output buffer" command (0xD3).  QEMU raises IRQ12 for each byte,
// so a rising packet count proves the real ISR path (not just the polled
// fallback).  One byte is injected at a time with a delay so the ISR can drain
// the single-entry output buffer before the next byte is queued.
TInt mouse_test()
    {
    mouse_init();                             // ensure IRQ12 is armed
    TUint before = gPackets;
    static const TUint8 kTestPkt[3] = {0x08u, 0x0Au, 0xF6u};  // sync,dx=+10,dy=-10
    for (TInt j = 0; j < 3; ++j)
        {
        KbcWaitInput();
        Out8(0x64, 0xD3u);                    // write byte as if from the mouse
        KbcWaitInput();
        Out8(0x60, kTestPkt[j]);
        for (volatile TInt k = 0; k < 200000; ++k) { }
        }
    mouse_render_cursor();
    TBool ok = gPackets > before;
    KPrintf("MOUSE TEST: injected 3 AUX bytes, packets %u -> %u x=%d y=%d isr=%u %s\r\n",
        before, (TUint)gPackets, (TInt)gX, (TInt)gY, (TUint)gIsrCount,
        ok ? "OK" : "FAIL");
    return ok ? KErrNone : KErrGeneral;
    }

// Inject one synthetic 3-byte AUX packet with the supplied button bits held
// (0x08 = always-on sync bit, OR the button mask).  Drives the real IRQ12 ISR
// -> gui_shared.buttons path from the shell so it can be tested without a
// physical pointer.  mask: 1=left, 2=right, 4=middle.
TInt mouse_inject_buttons(TUint aMask)
    {
    mouse_init();
    TUint beforeSeq = gButtonSeq;
    const TUint8 pkt[3] = {(TUint8)(0x08u | (aMask & 0x07u)), 0x00u, 0x00u};
    for (TInt j = 0; j < 3; ++j)
        {
        KbcWaitInput();
        Out8(0x64, 0xD3u);                    // write byte as if from the mouse
        KbcWaitInput();
        Out8(0x60, pkt[j]);
        for (volatile TInt k = 0; k < 200000; ++k) { }
        }
    mouse_render_cursor();
    TUint sharedBtns = 0, sharedSeq = 0;
    gui_button_state(sharedBtns, sharedSeq);
    KPrintf("MOUSE PRESS: mask=%u packets=%u driver buttons=%u seq=%u->%u "
        "shared buttons=%u seq=%u %s\r\n",
        aMask, (TUint)gPackets, (TUint)gButtons, beforeSeq, (TUint)gButtonSeq,
        sharedBtns, sharedSeq,
        (sharedBtns == (TUint)gButtons && sharedSeq == gButtonSeq) ? "OK" : "FAIL");
    return (sharedBtns == (TUint)gButtons) ? KErrNone : KErrGeneral;
    }

// Boot hook: HwInit0 walks .init_array after paging + ROM are ready.  Safe and
// idempotent (mouse_init guards on gInited; the shell re-arms the PIC mask).
__attribute__((constructor)) static void MouseBootInit()
    { mouse_init(); }
