#ifndef QPORT_WSCLIENT_H
#define QPORT_WSCLIENT_H

/* ---------------------------------------------------------------------------
 * wsclient.h -- CLIENT side library for the R1 window server.
 *
 * A client E32 owns a *window*, not the screen.  It discovers the kernel's
 * struct ws_shared through slow execs WS_BASE / WS_SIZE, creates one or more
 * windows with WS_OP(WSO_CREATE_WINDOW), and draws into each window's private
 * content buffer that lives inside the shared region at WS_CONTENT_OFFSET(wid).
 * Low RAM is identity mapped and user-accessible here, so the content buffer is
 * written directly -- the LFB itself is never touched by a client.  A dirty
 * flag tells the long-lived WINSRV compositor to blit the window into the
 * desktop on its next frame.  Input arrives as a bounded event ring; see
 * docs/WINDOW_SERVER.md.
 *
 * The authoritative kernel<->user ABI is variant/qemu_windowsrv.h, included
 * below.  This header only adds the small client-side API and convenience
 * aliases; it does not redefine any ABI structure.
 * ------------------------------------------------------------------------ */

#if !defined(__cplusplus)
typedef unsigned int       uint32_t;
typedef unsigned short     uint16_t;
typedef unsigned char      uint8_t;
#endif

#include "../variant/qemu_windowsrv.h"

/* Friendly aliases for the prompt's WS_BASE / WS_SIZE / WS_OP naming. */
#ifndef WS_BASE
#define WS_BASE QE_EXEC_WS_BASE
#endif
#ifndef WS_SIZE
#define WS_SIZE QE_EXEC_WS_SIZE
#endif
#ifndef WS_OP
#define WS_OP   QE_EXEC_WS_CTL
#endif

/* ---- client-side geometry / palette ------------------------------------- */
#define WS_CELL_W        16      /* 8x8 glyph blown up 2x (matches guilib) */
#define WS_CELL_H        16
#define WS_ROW_PITCH     18

#define WSC_BLACK        0x00000000u
#define WSC_NAVY         0x00000080u
#define WSC_BLUE         0x000060A0u
#define WSC_TITLEBAR     0x000080C0u
#define WSC_CYAN         0x0000C0C0u
#define WSC_GREEN        0x0000FF00u
#define WSC_YELLOW       0x00FFFF00u
#define WSC_WHITE        0x00FFFFFFu
#define WSC_GRAY         0x00808080u
#define WSC_LIGHTGRAY    0x00C0C0C0u
#define WSC_RED          0x00FF3020u

/* ---- public client API -------------------------------------------------- */
int  ws_open(void);                 /* discover + validate shared control blk */
int  ws_window_create(int x, int y, int w, int h, const char* title);
void ws_fill(int wid, int x, int y, int w, int h, uint32_t color);
void ws_text(int wid, int x, int y, const char* s, uint32_t fg);
void ws_button(int wid, int x, int y, int w, int h,
               const char* label, int hover);
void ws_present(int wid);            /* bump dirty + ask server to composite  */
int  ws_get_event(struct ws_event* e);   /* 1 = event copied, 0 = none        */
void ws_close(void);

/* Helpers. */
uint32_t ws_rgb(uint8_t r, uint8_t g, uint8_t b);
int      ws_window_count(void);
unsigned char* ws_content(int wid);      /* private pixel buffer base         */
int      ws_window_geom(int wid, int* x, int* y, int* w, int* h);
int      ws_server_ready(void);           /* server_pid published?             */

/* ---- E32/EWSRV migration additions (docs/WINSRV_MIGRATION_PLAN.md) --------
 * Implemented genuinely by ws_e32_client.c over the CWsSession opcodes of the
 * genuine EWSRV (EConsoleSetTitle / EConsoleSetSize / EConsoleScreenSize).
 * The old exec-based wsclient.c answers these with -1: the R1 shim window
 * server has no set-title / set-size / screen-size op.  On the E32 path w/h
 * are character cells (the genuine console window is sized in cells, exactly
 * like RConsole::Init in ewsrv/co_cli.cpp). */
int  ws_set_title(int wid, const char* title);
int  ws_set_size(int wid, int w, int h);
int  ws_screen_size(int* w, int* h);

#endif /* QPORT_WSCLIENT_H */
