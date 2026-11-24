/*
 * guihello.c -- demonstration program for the reusable PE2 GUI toolkit.
 *
 * Opens a titled window with three focusable buttons, a live status panel and
 * a mouse readout through guilib.c, which now runs on the GENUINE EWSRV
 * client path (user/ws_e32_client.c): a real EConsole window on
 * "!E32WindowServer", screen geometry from EConsoleScreenSize, text drawn
 * with EConsoleWrite.  No winsrv execs 249/250/251 remain in this file.
 * Arrow keys (ANSI ESC [ A/B/C/D on the serial console) move the focus ring,
 * Enter "presses" a button and Esc quits.  A bounded auto-exit keeps the
 * end-to-end QEMU test short.  Serial markers use the genuine EUSER.DLL
 * ordinal 915 import; IAT slot guihello_printf_slot lives at 0x408010 (the
 * ws_e32_client slots occupy 0x408000-0x40800c).
 */
#include "runtime.h"
#include "guilib.h"

extern unsigned int guihello_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&guihello_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);
    else print(text);
}

/* ---- tiny formatters (no libc) ---- */
static unsigned append(char* p, unsigned k, const char* s)
{
    while (*s) p[k++] = *s++;
    return k;
}
static unsigned dec(char* p, unsigned k, unsigned v)
{
    char t[12];
    int n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10u); v /= 10u; }
    while (n) p[k++] = t[--n];
    return k;
}
static const char* key_name(int k)
{
    switch (k) {
    case GUI_KEY_UP:    return "UP";
    case GUI_KEY_DOWN:  return "DOWN";
    case GUI_KEY_LEFT:  return "LEFT";
    case GUI_KEY_RIGHT: return "RIGHT";
    case GUI_KEY_ENTER: return "ENTER";
    case GUI_KEY_ESC:   return "ESC";
    case GUI_KEY_BACKSPACE: return "BACKSPACE";
    default: return k >= 32 && k <= 126 ? "CHAR" : "NONE";
    }
}

static const char* button_name(int i)
{
    static const char* names[3] = { "OPEN", "EDIT", "QUIT" };
    return names[i];
}

#define BUTTON_COUNT  3
#define FRAME_TICKS   600
#define FRAME_MS      40

int app_main(void)
{
    char line[160];
    unsigned k;
    int rc, key, focus = 0, presses = 0, tick, mx = 0, my = 0, mb = 0;
    int W, H, win;

    marker("GH: GUI HELLO START");

    rc = gui_init();
    if (rc != 0) {
        k = append(line, 0, "GH: FAIL gui_init rc=");
        if (rc < 0) line[k++] = '-';
        k = dec(line, k, (unsigned)(rc < 0 ? -rc : rc));
        line[k] = 0;
        marker(line);
        return 1;
    }
    W = gui_fb_width();
    H = gui_fb_height();

    k = append(line, 0, "GH: shared ready geometry=");
    k = dec(line, k, (unsigned)W);
    k = append(line, k, "x");
    k = dec(line, k, (unsigned)H);
    line[k] = 0;
    marker(line);

    for (tick = 0; tick < FRAME_TICKS; ++tick) {
        key = gui_key();
        if (key == GUI_KEY_UP || key == GUI_KEY_LEFT) {
            focus = (focus + BUTTON_COUNT - 1) % BUTTON_COUNT;
        } else if (key == GUI_KEY_DOWN || key == GUI_KEY_RIGHT) {
            focus = (focus + 1) % BUTTON_COUNT;
        } else if (key == GUI_KEY_ENTER) {
            ++presses;
        } else if (key == GUI_KEY_ESC) {
            marker("GH: ESC received");
            break;
        }
        if (key) {
            k = append(line, 0, "GH: key ");
            k = append(line, k, key_name(key));
            k = append(line, k, " focus=");
            k = append(line, k, button_name(focus));
            line[k] = 0;
            marker(line);
        }

        gui_mouse(&mx, &my, &mb);
        if (tick == 0) {
            k = append(line, 0, "GH: mouse buttons=");
            k = dec(line, k, (unsigned)mb);
            k = append(line, k, " (1=L 2=R 4=M)");
            line[k] = 0;
            marker(line);
        }

        /* ---- draw one complete frame into the toolkit's back buffer ---- */
        gui_clear(GUI_NAVY);
        gui_rect(8, 8, W - 16, 32, GUI_TITLEBAR);
        gui_text(16, 14, "EKA2 GUI TOOLKIT", GUI_WHITE, GUI_TITLEBAR);
        gui_hline(8, 40, W - 16, GUI_LIGHTGRAY);
        win = gui_window(16, 56, 640, 380, "GUIHELLO DEMO", 1, 1);
        (void)win;

        gui_button(0, 40,  200, 176, 44, "OPEN", focus == 0);
        gui_button(1, 240, 200, 176, 44, "EDIT", focus == 1);
        gui_button(2, 440, 200, 176, 44, "QUIT", focus == 2);

        k = append(line, 0, "KEY ");
        k = append(line, k, key_name(key));
        line[k] = 0;
        gui_text(40, 280, line, GUI_YELLOW, GUI_BLUE);

        k = append(line, 0, "FOCUS:");
        k = append(line, k, button_name(focus));
        k = append(line, k, " N=");
        k = dec(line, k, (unsigned)presses);
        line[k] = 0;
        gui_text(40, 302, line, GUI_WHITE, GUI_BLUE);

        k = append(line, 0, "MOUSE ");
        k = dec(line, k, (unsigned)mx);
        k = append(line, k, ",");
        k = dec(line, k, (unsigned)my);
        line[k] = 0;
        gui_text(40, 324, line, GUI_WHITE, GUI_BLUE);

        gui_text(40, 360, "ARROWS move ENTER press ESC quit",
                 GUI_LIGHTGRAY, GUI_BLUE);

        /* animated "live" pip on the title strip */
        gui_rect(W - 120, 14, 12, 12,
                 (tick & 8u) ? GUI_GREEN : GUI_YELLOW);
        gui_text(W - 102, 14, "LIVE", GUI_WHITE, GUI_TITLEBAR);

        gui_present();
        if (tick == 0) {
            k = append(line, 0, "GH: frame 1 drawn cmds=");
            k = dec(line, k, (unsigned)gui_pending_commands());
            line[k] = 0;
            marker(line);
        }
        sleep_ms(FRAME_MS);
    }

    k = append(line, 0, "GH: mouse x=");
    k = dec(line, k, (unsigned)mx);
    k = append(line, k, " y=");
    k = dec(line, k, (unsigned)my);
    k = append(line, k, " buttons=");
    k = dec(line, k, (unsigned)mb);
    line[k] = 0;
    marker(line);

    k = append(line, 0, "GH: exit presses=");
    k = dec(line, k, (unsigned)presses);
    line[k] = 0;
    marker(line);

    marker("GH: DONE");
    /* Repaint the last frame once more so the final LFB image stays a clean
       GUI (serial markers are mirrored onto the VGA text surface by the
       kernel shell after the program exits). */
    gui_present();
    gui_shutdown();
    return 0;
}
