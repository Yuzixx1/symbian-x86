// qemu_gui_exec.cpp — kernel-side host for the ring-3 GUI desktop shell.
//
// The VBE framebuffer (variant/qemu_display.cpp) is at a high physical address
// (~0xFD000000).  The flat 4 MiB PDE map gives U/S=0 to everything at/above
// 0x80000000 (mmu/pd.cpp), so a ring-3 E32 process cannot touch VRAM.  This
// module bridges that gap exactly the way qemu_netbridge.cpp bridges the NIC:
// a low, identity-mapped RAM buffer plus dedicated slow executives.
//
// Slow execs (the 64-bit atomic family 247..251 is invalid on this x86 build;
// 247/248 are already netbridge's):
//   249 -> framebuffer physical base (u32)
//   250 -> framebuffer byte size (u32)
//   251 -> arg1==0: return the gui_shared address; arg1>0: run a display list,
//          refresh the PS/2 mouse crosshair and publish its scanned x/y.
//          The PS/2 IRQ12 ISR (qemu_mouse.cpp) also publishes button state
//          directly into gui_shared.buttons/button_seq via the bridge below.
//
// kernelhwsrv/ is read-only; the genuine EpocSlowExecTable is patched at boot
// just like SvcMgr (268/269/270) and netbridge (247/248).  Idempotent.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"
#include "../user/guishost.h"

extern const TUint32 EpocSlowExecTable[];

namespace
    {
    __attribute__((aligned(4096))) static struct gui_shared gGui;
    static TBool gInstalled = EFalse;

    // Cursor colour painted by the PS/2 mouse driver (qemu_mouse.cpp).
    const TUint kCursorYellow = 0x00FFFF00u;

    // exec 249: framebuffer physical base. SvcMgr/exec-268 callers and direct
    // test E32s reach here without the shell's RunImage switch, so activate the
    // LFB on first use (idempotent, no-op once the shell already switched).
    static TInt GuiFbBaseExec(TInt, TInt)
        { display_ensure_graphics(); return (TInt)display_lfb(); }

    // exec 250: framebuffer byte size.
    static TInt GuiFbSizeExec(TInt, TInt)
        { display_ensure_graphics(); return (TInt)display_fb_size(); }

    // Find the PS/2 crosshair by looking for its yellow horizontal and
    // vertical arms in VRAM.  Returns ETrue when a plausible crosshair is
    // present.  Runs in kernel context (the LFB is readable from ring 0).
    static TBool ScanCursor(TInt& aX, TInt& aY)
        {
        TUint w = (TUint)gGui.fb_w, h = (TUint)gGui.fb_h;
        if (!w || !h)
            return EFalse;
        TInt bestRow = -1, bestRowN = 0;
        TInt bestCol = -1, bestColN = 0;
        for (TUint y = 0; y < h; ++y)
            {
            TInt n = 0;
            for (TUint x = 0; x < w; ++x)
                if (display_getpixel(x, y) == kCursorYellow)
                    ++n;
            if (n > bestRowN && n >= 8)
                { bestRowN = n; bestRow = (TInt)y; }
            }
        for (TUint x = 0; x < w; ++x)
            {
            TInt n = 0;
            for (TUint y = 0; y < h; ++y)
                if (display_getpixel(x, y) == kCursorYellow)
                    ++n;
            if (n > bestColN && n >= 8)
                { bestColN = n; bestCol = (TInt)x; }
            }
        if (bestRow < 0 || bestCol < 0)
            return EFalse;
        aX = bestCol;
        aY = bestRow;
        return ETrue;
        }

    // Draw a guaranteed-visible white box cursor at the kernel cursor pos.
    // We cannot reset the mouse driver's cached "drawn" state, so we paint our
    // own marker instead of relying on the crosshair surviving the desktop.
    static void DrawShellCursor(TInt aX, TInt aY)
        {
        const TUint white = 0x00FFFFFFu;
        display_rect((TUint)(aX - 7), (TUint)(aY - 7), 15u, 1u, white);
        display_rect((TUint)(aX - 7), (TUint)(aY + 7), 15u, 1u, white);
        display_rect((TUint)(aX - 7), (TUint)(aY - 7), 1u, 15u, white);
        display_rect((TUint)(aX + 7), (TUint)(aY - 7), 1u, 15u, white);
        display_rect((TUint)(aX - 1), (TUint)(aY - 1), 3u, 3u, white);
        }

    // GC_BLIT: bulk RGBA -> 32bpp LFB (user/guishost.h ABI).  The source is a
    // separate low-RAM user buffer described by gui_shared.blit_src_phys/size;
    // it never lives in the small command list.  Everything is validated here
    // in ring 0 before a single destination byte is touched.
    static TUint gBlitCount = 0;

    static void BlitRgba(const struct gc_blit_cmd& aCmd)
        {
        const TUint srcPhys = (TUint)gGui.blit_src_phys;
        const TUint srcSize = (TUint)gGui.blit_src_size;
        const TUint x = aCmd.x, y = aCmd.y;
        const TUint w = aCmd.w, h = aCmd.h;
        const TUint offset = aCmd.src_offset;

        TUint fw = 0, fh = 0, bpp = 0;
        display_geometry(fw, fh, bpp);
        const TUint kMaxBytes = 640u * 400u * 4u;   // 1,024,000

        // Source descriptor bounds: low, identity-mapped RAM only, no wrap.
        if (srcPhys < 0x1000u || srcPhys >= 0x80000000u ||
            srcSize < 4u || srcSize > kMaxBytes ||
            srcSize > 0x80000000u - srcPhys)
            return;
        // Only the 32bpp VBE modes can accept an RGBA8888 frame verbatim.
        if (bpp != 32u || fw == 0u || fh == 0u)
            return;
        // Destination rect: non-empty, <= one DOOM-class frame, fully on screen
        // (subtract-form avoids x+w / y+h overflow).
        if (w == 0u || h == 0u || w > 640u || h > 400u ||
            x >= fw || y >= fh || w > fw - x || h > fh - y)
            return;
        const TUint bytes = w * h * 4u;
        if (bytes < 4u || bytes > kMaxBytes || bytes > srcSize ||
            offset > srcSize - bytes)
            return;

        // Sparse kernel marker: once per 100 blits (~frames).  Printed before
        // the copy so the console's LFB text mirror cannot smear this frame.
        if ((gBlitCount++ % 100u) == 0u)
            KPrintf("GCB: blit %ux%u %u bytes\r\n", w, h, bytes);

        const TUint8* src = (const TUint8*)srcPhys + offset;
        volatile TUint8* lfb = (volatile TUint8*)display_lfb();
        if (!lfb)
            return;
        // gPitch is private to qemu_display.cpp; for every supported 32bpp
        // mode it equals fb_width*4 (1024*4 = 4096), so gPitch/4 == fb_width.
        const TUint pitch = fw * 4u;
        for (TUint row = 0; row < h; ++row)
            {
            const TUint8* s = src + row * w * 4u;
            volatile TUint32* d =
                (volatile TUint32*)(lfb + (y + row) * pitch + x * 4u);
            for (TUint col = 0; col < w; ++col)
                {
                // RGBA8888 source -> 0x00RRGGBB LFB word (A ignored).
                d[col] = ((TUint)s[0] << 16) | ((TUint)s[1] << 8) | (TUint)s[2];
                s += 4;
                }
            }
        }

    // exec 251: multiplexed GUI host.
    static TInt GuiDrawExec(TInt aArg, TInt)
        {
        display_ensure_graphics();   // first display list may be the first graphics use
        if (aArg == 0)
            {
            mouse_init();                       // re-arm IRQ12 after PIC remap
            return (TInt)&gGui;                 // shared display-list buffer
            }

        TUint n = (TUint)aArg;
        if (n > GUI_MAX_CMDS)
            return KErrArgument;

        // Make sure a crosshair exists in VRAM before we scan it.  On the
        // first call the mouse driver's cached state is empty so it paints;
        // later calls repaint only if the pointer actually moved.
        mouse_render_cursor();
        // The IRQ12 ISR now publishes the authoritative pointer position
        // straight into gGui.mouse_x/y (gui_publish_position), exactly as it
        // already does for buttons.  Trust those values; the VRAM crosshair
        // scan is only a fallback for a driver that never published (e.g.
        // before the first AUX packet).  Using the published coordinates also
        // avoids the old failure where GC_CLEAR wiped the scanned crosshair
        // and the cursor snapped back to screen centre.
        TInt mx = gGui.mouse_x, my = gGui.mouse_y;
        if ((mx < 0 || mx >= (TInt)gGui.fb_w ||
             my < 0 || my >= (TInt)gGui.fb_h))
            {
            TInt sx, sy;
            if (ScanCursor(sx, sy))
                { mx = sx; my = sy; gGui.mouse_x = mx; gGui.mouse_y = my; }
            else
                { mx = (TInt)gGui.fb_w / 2; my = (TInt)gGui.fb_h / 2; }
            }

        // Execute the user's display list in kernel (ring 0) context.
        for (TUint i = 0; i < n; ++i)
            {
            const struct gui_cmd& c = gGui.cmds[i];
            switch (c.type)
                {
                case GC_CLEAR:
                    display_clear(c.a);
                    break;
                case GC_RECT:
                    display_rect(c.a, c.b, c.c, c.d, c.e);
                    break;
                case GC_PUTC:
                    display_putc(c.a, c.b, (char)c.c, c.d, c.e);
                    break;
                case GC_BLIT:
                    {
                    const struct gc_blit_cmd& b = (const struct gc_blit_cmd&)c;
                    BlitRgba(b);
                    break;
                    }
                default:
                    break;
                }
            }

        // Overlay our own cursor on top of the freshly drawn desktop.  The
        // position is now clamped/published above, so this always tracks the
        // real pointer instead of freezing at the last scanned crosshair.
        DrawShellCursor(mx, my);

        // First frame: read straight back from VRAM (proves real LFB writes,
        // not just I/O register fiddling) and hand the result to the test.
        if (gGui.draw_calls == 0u)
            {
            TUint bg = display_getpixel(0, 0);
            TUint title = display_getpixel(100, 20);
            KPrintf("GUI: pixel readback bg=0x%06x title=0x%06x %s\r\n",
                bg, title,
                (bg == 0x00000080u && title == 0x000080C0u) ? "OK" : "MISMATCH");
            }

        gGui.count = n;
        ++gGui.draw_calls;
        return KErrNone;
        }

    static void GuiExecInstall()
        {
        TUint32* tab = (TUint32*)EpocSlowExecTable;
        // Need entries 0..251; this build's table count is 271.
        if (tab[0] < 252u)
            {
            KPrintf("gui: exec table count %u, install skipped\r\n", tab[0]);
            return;
            }
        const TInt slots[3] = {(TInt)QE_EXEC_GUI_FBBASE,
                               (TInt)QE_EXEC_GUI_FBSIZE,
                               (TInt)QE_EXEC_GUI_DRAW};
        TInt (*fns[3])(TInt, TInt) = {&GuiFbBaseExec, &GuiFbSizeExec,
                                      &GuiDrawExec};
        for (TInt i = 0; i < 3; ++i)
            {
            const TUint s = (TUint)slots[i];
            const TUint fnIdx = 4u + 2u * s;
            // Only take slots still pointing at InvalidExecHandler (tab[1]).
            if (tab[fnIdx] != tab[1] && tab[fnIdx] != 0u)
                {
                KPrintf("gui: slot %d not invalid (%x), skip\r\n",
                    slots[i], tab[fnIdx]);
                continue;
                }
            tab[3u + 2u * s] = 0;                        // flags
            tab[fnIdx] = (TUint32)(TLinAddr)fns[i];      // handler
            }
        }
    }  // namespace

// IRQ12 ISR bridge: publish the decoded PS/2 button mask into the same shared
// struct userland maps via exec 251(0).  Two volatile stores only — safe from
// interrupt context, and no display-list exec is required to see a click.
void gui_publish_buttons(TUint aButtons, TUint aSeq)
    {
    gGui.buttons = aButtons;
    gGui.button_seq = aSeq;
    }

// IRQ12 ISR bridge for the pointer position.  Two volatile stores only — safe
// from interrupt context, and no DRAW exec is needed for a ring-3 desktop to
// observe mouse movement (the old VRAM crosshair scan was fragile: GC_CLEAR
// wiped the crosshair, so coordinates went stale between redraws).
void gui_publish_position(TInt aX, TInt aY)
    {
    gGui.mouse_x = aX;
    gGui.mouse_y = aY;
    }

// Diagnostic readback of exactly the words a user program sees.
void gui_button_state(TUint& aButtons, TUint& aSeq)
    {
    aButtons = gGui.buttons;
    aSeq = gGui.button_seq;
    }

TInt gui_init()
    {
    if (gInstalled)
        return KErrNone;
    gInstalled = ETrue;

    TInt r = display_init();
    if (r != KErrNone)
        return r;
    // At boot this is a no-op: display_init just enabled the DISPI LFB (or the
    // display boot hook did), so the enable bit reads back set. The shell then
    // parks the LFB in text mode; the exec handlers above re-enable it lazily
    // by reading that same bit, which is why no sticky "activated" flag is used.
    display_ensure_graphics();
    mouse_init();                                 // idempotent; ensures a cursor

    // Zero the shared region, then publish its header.
    TUint8* p = (TUint8*)&gGui;
    for (TUint i = 0; i < sizeof(gGui); ++i)
        p[i] = 0;
    TUint w = 0, h = 0, bpp = 0;
    display_geometry(w, h, bpp);
    gGui.fb_w = (TInt)w;
    gGui.fb_h = (TInt)h;
    gGui.mouse_x = (TInt)(w / 2);
    gGui.mouse_y = (TInt)(h / 2);
    gGui.magic = GUI_MAGIC;
    gGui.version = GUI_VERSION;

    GuiExecInstall();

    KPrintf("GUI: execs %u/%u/%u installed fb=0x%08x size=%u "
        "geometry=%ux%ux%u shared=0x%08x\r\n",
        (TUint)QE_EXEC_GUI_FBBASE, (TUint)QE_EXEC_GUI_FBSIZE,
        (TUint)QE_EXEC_GUI_DRAW, display_lfb(), display_fb_size(),
        w, h, bpp, (TUint)&gGui);
    return KErrNone;
    }

// Called by display_set_mode() after a VBE switch: the guishost shared block
// (user/guishost.h) must advertise the new geometry and a valid cursor pos.
void GuiGeometryReset()
    {
    TUint w = 0, h = 0, bpp = 0;
    display_geometry(w, h, bpp);
    gGui.fb_w = (TInt)w;
    gGui.fb_h = (TInt)h;
    if (!w || !h)
        return;
    if ((TUint)gGui.mouse_x >= w)
        gGui.mouse_x = (TInt)(w / 2);
    if ((TUint)gGui.mouse_y >= h)
        gGui.mouse_y = (TInt)(h / 2);
    }

// Boot hook: HwInit0 walks .init_array after paging + ROM are ready.
__attribute__((constructor)) static void GuiBootInit()
    { gui_init(); }
