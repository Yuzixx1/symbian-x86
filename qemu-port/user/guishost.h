#ifndef QPORT_USER_GUIHOST_H
#define QPORT_USER_GUIHOST_H

/* ---------------------------------------------------------------------------
 * Kernel <-> userland desktop-shell transport (variant/qemu_gui_exec.cpp).
 *
 * The VBE linear framebuffer lives in the high half (~0xFD000000).  The flat
 * 4 MiB PDE map marks everything at/above 0x80000000 U/S=0 (supervisor-only),
 * so a ring-3 PE2 process cannot write VRAM directly.  Instead the shell
 * builds a small display list in a low, identity-mapped RAM buffer (the same
 * proven trick netbridge uses for its frame rings) and asks the kernel to
 * execute it through dedicated slow executives.
 *
 * Slow exec slots (the 64-bit atomic family 247-251 is invalid in this x86
 * build; 247/248 already belong to netbridge):
 *   249 -> framebuffer physical base (u32, returned directly)
 *   250 -> framebuffer byte size     (u32, returned directly)
 *   251 -> multiplexed GUI exec:
 *          arg1 == 0 : return the address of struct gui_shared (low phys)
 *          arg1  > 0 : execute arg1 display-list commands, refresh the PS/2
 *                      mouse crosshair and publish its x/y in the struct.
 *
 * Mouse buttons are published by the IRQ12 PS/2 ISR (variant/qemu_mouse.cpp)
 * straight into gui_shared.buttons / gui_shared.button_seq; no exec call is
 * needed to observe a press.  buttons is a bitmask (1=left,2=right,4=middle);
 * button_seq increments on every button-state change so a desktop can detect
 * a click even if press+release happen between two polls.  Both fields re-use
 * the old reserved word / a tail slot, so every pre-existing field keeps its
 * offset and GUI_VERSION stays 1.
 *
 * GC_BLIT (opcode 3) -- real bulk RGBA blit, appended 2026-11-21.  A full
 * 640x400x4 frame is 1,024,000 bytes, far larger than the GUI_MAX_CMDS list,
 * so the source lives in a SEPARATE low-RAM user buffer whose physical base
 * and byte size are published in the two tail fields below.  gui_shared's new
 * fields are appended after button_seq (all prior offsets, including cmds and
 * button_seq, are unchanged) so GUI_VERSION stays 1; old binaries simply
 * ignore the tail and never emit GC_BLIT.  ABI contract for a blit:
 *   gui_shared.blit_src_phys  physical (== virtual in low RAM) source base,
 *                             0x1000 <= phys < 0x80000000, kernel validates
 *   gui_shared.blit_src_size  allocated source bytes (<= 640*400*4); the
 *                             kernel rejects a blit whose w*h*4 + src_offset
 *                             exceeds it
 *   gc_blit_cmd.{x,y,w,h}     destination rect in the VBE LFB (0,0 top-left)
 *   gc_blit_cmd.src_offset    byte offset into the source buffer
 * Source pixels are RGBA8888 (R,G,B,A byte order); the kernel writes each as
 * a 0x00RRGGBB word into the 32bpp LFB, so the A byte is ignored and R/B are
 * correctly oriented on screen.  One GC_BLIT in a one-command exec paints the
 * whole frame; clipping/scales are the caller's job (no new exec slot used).
 * ------------------------------------------------------------------------ */

#define QE_EXEC_GUI_FBBASE      249u
#define QE_EXEC_GUI_FBSIZE      250u
#define QE_EXEC_GUI_DRAW        251u

#define GUI_MAGIC               0x47554931u   /* "GUI1" */
#define GUI_VERSION             1u
/* NOTE: this is the kernel<->user ABI capacity (struct gui_shared below) AND
 * the per-exec command budget checked by qemu_gui_exec.cpp.  One complete
 * desktop repaint (wallpaper + 8 icons + windows + taskbar + Start menu) is
 * ~440 commands, far above the old 160.  Keeping it at 160 forced cmd() to
 * auto-flush 3 lists per frame, so the kernel presented half-painted frames.
 * Raising it lets a full repaint be submitted in a SINGLE exec (no partial
 * frames); the extra ~11 KB of gGui/guilib back-buffer is static BSS only. */
#define GUI_MAX_CMDS            640u

/* Display-list opcodes. */
#define GC_CLEAR                0u   /* a = 0x00RRGGBB colour                  */
#define GC_RECT                 1u   /* a=x b=y c=w d=h e=colour              */
#define GC_PUTC                 2u   /* a=x b=y c=char d=fg e=bg              */
#define GC_BLIT                 3u   /* bulk RGBA source -> LFB, see ABI above */

struct gui_cmd {
    volatile unsigned type;
    volatile unsigned a, b, c, d, e;
};

/* GC_BLIT overlays struct gui_cmd exactly (6 u32): a=x b=y c=w d=h
 * e=src_offset.  Source pixels come from gui_shared.blit_src_phys, not from
 * this command, so a full frame takes ONE display-list slot. */
struct gc_blit_cmd {
    volatile unsigned type;
    volatile unsigned x, y, w, h;
    volatile unsigned src_offset;
};

struct gui_shared {
    volatile unsigned magic;          /* GUI_MAGIC once gui_init() ran        */
    volatile unsigned version;
    volatile unsigned count;          /* kernel: commands executed last call  */
    volatile unsigned draw_calls;     /* kernel: number of DRAW execs served  */
    volatile int      fb_w, fb_h;     /* kernel: framebuffer geometry         */
    volatile int      mouse_x, mouse_y; /* kernel: PS/2 cursor (GC exec scan) */
    volatile unsigned buttons;        /* kernel: PS/2 button mask 1=L 2=R 4=M */
                                      /* (re-used old `reserved` word: no     */
                                      /*  offset change for existing fields) */
    struct gui_cmd    cmds[GUI_MAX_CMDS];
    volatile unsigned button_seq;     /* kernel: increments on button change  */
                                      /* (tail append: cmds offset unchanged) */
    /* GC_BLIT source descriptor (tail append; ABI documented at file top).
     * User writes both fields before an exec-251 list containing GC_BLIT. */
    volatile unsigned blit_src_phys;  /* physical base of RGBA source buffer  */
    volatile unsigned blit_src_size;  /* byte size of that source buffer      */
};

#endif /* QPORT_USER_GUIHOST_H */
