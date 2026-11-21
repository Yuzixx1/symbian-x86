#ifndef QPORT_USER_GUILIB_H
#define QPORT_USER_GUILIB_H

/* ---------------------------------------------------------------------------
 * guilib.h -- a small, reusable GUI toolkit for PE2 E32 user programs.
 *
 * The VBE linear framebuffer (~0xFD000000) is supervisor-only in the port's
 * flat page map, so ring-3 programs cannot write VRAM.  This library uses the
 * proven display-list transport from user/guishost.h: a low-RAM gui_shared
 * buffer obtained from slow exec 251 (arg1==0), submitted for painting with
 * exec 251 (arg1>0).  The framebuffer itself is discovered with execs 249/250.
 *
 * Rendering model: every draw call appends to a private BACK display list.
 * gui_clear() starts a fresh frame; gui_present() copies the whole back list
 * into the kernel's FRONT (shared) list and submits it in one atomic exec.
 * That build-offscreen / publish-atomically pair is the toolkit's double
 * buffer; it avoids ever exposing a half-drawn frame to the kernel painter.
 *
 * Coordinates are PIXELS on the 1024x768x32 LFB.  The kernel font is an 8x8
 * stroke glyph blown up 2x: one character cell is 16x16 px with an 18 px row
 * pitch (64x42 text grid).  gui_text() advances a full cell (16 px) per char.
 *
 * Input: gui_key() drains the PortCon lease set up by gui_init().  PS/2 in
 * this port drops extended scancodes, so ANSI escape arrows arriving on the
 * serial stream (ESC [ A/B/C/D) are decoded into GUI_KEY_UP/... values.
 * gui_mouse() reads the kernel crosshair position and the live PS/2 button
 * mask published in gui_shared (1=left, 2=right, 4=middle).
 * ------------------------------------------------------------------------ */

/* Freestanding types: no hosted stdint.h is available to PE2 programs. */
typedef unsigned int       uint32_t;
typedef unsigned short     uint16_t;
typedef unsigned char      uint8_t;

#define GUI_KEY_NONE        0
#define GUI_KEY_UP          128
#define GUI_KEY_DOWN        129
#define GUI_KEY_LEFT        130
#define GUI_KEY_RIGHT       131
#define GUI_KEY_ENTER       13
#define GUI_KEY_ESC         27
#define GUI_KEY_BACKSPACE   8

#define GUI_CELL_W          16      /* 8 px glyph * 2x scale */
#define GUI_CELL_H          16
#define GUI_ROW_PITCH       18

/* 0x00RRGGBB colours.  GUI_NAVY/GUI_TITLEBAR intentionally match the kernel
 * shell palette so the kernel's first-frame pixel readback reports OK. */
#define GUI_BLACK           0x00000000u
#define GUI_NAVY            0x00000080u
#define GUI_BLUE            0x000060A0u
#define GUI_TITLEBAR        0x000080C0u
#define GUI_CYAN            0x0000C0C0u
#define GUI_GREEN           0x0000FF00u
#define GUI_YELLOW          0x00FFFF00u
#define GUI_WHITE           0x00FFFFFFu
#define GUI_GRAY            0x00808080u
#define GUI_LIGHTGRAY       0x00C0C0C0u
#define GUI_RED             0x00FF3020u
#define GUI_MAGENTA         0x00FF00FFu

/* Setup / teardown.  gui_init() discovers the framebuffer, acquires the
 * PortCon input lease, and returns 0 on success, negative on failure. */
int  gui_init(void);
void gui_shutdown(void);
int  gui_fb_width(void);
int  gui_fb_height(void);

/* Input.  gui_key() never blocks: 0 means "no key available". */
int  gui_key(void);
void gui_mouse(int* x, int* y, int* buttons);

/* Drawing (all append to the current back-buffer display list). */
void gui_clear(uint32_t color);
void gui_rect(int x, int y, int w, int h, uint32_t color);
void gui_text(int x, int y, const char* s, uint32_t fg, uint32_t bg);
void gui_hline(int x, int y, int w, uint32_t color);
void gui_vline(int x, int y, int h, uint32_t color);

/* Widgets.  gui_window() returns a window id (>=0) or -1. */
int  gui_window(int x, int y, int w, int h, const char* title,
                int border, int fill);
void gui_button(int win, int x, int y, int w, int h,
                const char* label, int focused);

/* Publish the current frame: submit the list via exec 251 and swap buffers. */
void gui_present(void);
int  gui_pending_commands(void);

/* Helpers. */
uint32_t gui_rgb(uint8_t r, uint8_t g, uint8_t b);
int  gui_msgbox(const char* title, const char* text);  /* waits for a key */

#endif /* QPORT_USER_GUILIB_H */
