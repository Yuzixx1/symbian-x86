#ifndef QPORT_USER_GUILIB_H
#define QPORT_USER_GUILIB_H

/* ---------------------------------------------------------------------------
 * guilib.h -- a small, reusable GUI toolkit for PE2 E32 user programs.
 *
 * WINSRV_MIGRATION_PLAN.md chunk B backend: the toolkit now runs on the
 * GENUINE EWSRV client path (user/ws_e32_client.c, chunk A) instead of the
 * winsrv slow execs 249/250/251.  gui_init() opens a genuine session to
 * "!E32WindowServer" (ws_open) and creates a full-grid EConsole window
 * (ws_window_create); gui_fb_width/height come from EConsoleScreenSize.
 *
 * Rendering model: every draw call still appends to a private BACK display
 * list (same command budget as before, so gui_pending_commands() and the
 * apps' command-budget guards keep working).  gui_present() interprets the
 * list against the console text grid: the screen is a 64x42 grid of
 * GUI_CELL_W x GUI_ROW_PITCH cells on the 1024x768 display; GC_CLEAR maps to
 * EConsoleClearScreen (bg colour), GC_RECT to chunk 2's ws_draw_fill_rect
 * (filled cells), and GC_PUTC runs coalesce into one EConsoleSetTextColors +
 * EConsoleSetCursorPosAbs + EConsoleWrite per line.
 *
 * Coordinates are PIXELS on the 1024x768 screen, as before; one character
 * cell is 16x16 px with an 18 px row pitch (64x42 text grid).  gui_text()
 * advances a full cell (16 px) per char.
 *
 * Input: gui_key() drains the PortCon lease set up by gui_init().  PS/2 in
 * this port drops extended scancodes, so ANSI escape arrows arriving on the
 * serial stream (ESC [ A/B/C/D) are decoded into GUI_KEY_UP/... values.
 * gui_mouse()/gui_mouse_full() report the pointer state; on the EWSRV path
 * there is no kernel gui_shared anymore, so until the EWSRV input-injection
 * bridge lands (plan section 5) they report zeros and apps drive by
 * keyboard.
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

/* 0x00RRGGBB colours (kept for API compatibility; the console text path
 * uses the server's palette, so rects colours apply once chunk 2 paints
 * pixels). */
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

/* Setup / teardown.  gui_init() connects to genuine EWSRV, creates the
 * console window, gets the screen size, acquires the PortCon input lease,
 * and returns 0 on success, negative on failure. */
int  gui_init(void);
void gui_shutdown(void);
int  gui_fb_width(void);
int  gui_fb_height(void);

/* Input.  gui_key() never blocks: 0 means "no key available". */
int  gui_key(void);
void gui_mouse(int* x, int* y, int* buttons);
/* Full pointer state incl. the button-change sequence counter (the old
 * gui_shared button_seq edge detector); zeros until the EWSRV input
 * bridge lands. */
void gui_mouse_full(int* x, int* y, unsigned* buttons, unsigned* seq);

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

/* Interpret the current frame over the genuine EWSRV console window. */
void gui_present(void);
int  gui_pending_commands(void);

/* Helpers. */
uint32_t gui_rgb(uint8_t r, uint8_t g, uint8_t b);
int  gui_msgbox(const char* title, const char* text);  /* waits for a key */

#endif /* QPORT_USER_GUILIB_H */
