/* e32_platform.c -- doomgeneric platform callbacks for the EKA2 x86 port.
 *
 * Video: DG_ScreenBuffer is the engine's 640x400 RGBA8888 frame (the engine
 * already converts the WAD PLAYPAL palette in I_SetPalette/cmap_to_fb).  A
 * single bulk 32bpp blit through the genuine kernel exec 270 op 3 (the
 * fbblit capability, logical execs 274/275) paints the whole frame to the
 * Bochs VBE LFB in kernel context.  If that exec is unavailable, a coarse
 * exec-251 GC_RECT display list is used as a fallback.
 *
 * Input: PS/2/serial keys are polled with con_tryread() under the PortCon
 * lease and mapped to DOOM keycodes.  Menu taps get a paired synthetic
 * release; continuous keys (move/fire/use) use a per-key hold table so a
 * held key keeps gamekeydown[] latched until a timed release, with typematic
 * repeats generated here because kernel break scancodes are discarded.
 *
 * Timing: exec 68 (EExecTimeNow) microseconds, rebased to the 2000 epoch.
 */
#include "doomgeneric.h"
#include "doomkeys.h"
#include "e32_shim.h"
#include "runtime.h"

#include "qemu_fbblit.h"      /* variant/qemu_fbblit.h: exec 270 ops 2/3 */
#include "guishost.h"         /* user/guishost.h: GC_RECT fallback list */

#include <stdio.h>
#include <string.h>

extern int exec2(unsigned number, unsigned a1, unsigned a2);

#define DOOM_FB_W 640u
#define DOOM_FB_H 400u

/* 8.3 name staged on the FAT volume; doome32.c locates it and builds argv.
 * The default points at the drive where the ISO's RAMDISK stages the WAD;
 * find_iwad() overrides this with the candidate that actually opened. */
const char *doom_iwad_path = "X:/FDOOM1.WAD";

static unsigned sXOff, sYOff;
static unsigned sFrames;
static int sPaletteMarked;
static int sKeyRaw;
static int sKeyPending;          /* pending synthetic key-up */
static int sHoldSkip;            /* DG_GetKey calls to defer the key-up */
static int sQuitRequested;       /* Esc seen: let app_main exit cleanly */

/* --- per-key hold / typematic state ----------------------------------------
 * The kernel PS/2 ISR throws away break (key-up) scancodes and serial only
 * delivers make bytes, so userland never sees a genuine key release.  QEMU's
 * 8042 may emit typematic make bytes while a physical key is held; a serial
 * client does not.  We therefore emulate both repeat and release with a
 * per-key state machine:
 *
 *   IDLE --make--> DOWN: emit press=1, latch in gamekeydown[].
 *   DOWN: software typematic presses every 80 ms (first at 280 ms) up to
 *         HOLD_MAX_MS; genuine QEMU make bytes, when present, replace these.
 *   DOWN --quiet HOLD_RELEASE_MS--> COOL: emit one press=0 so the latch clears
 *         promptly, then ignore stale makes for HOLD_COOLDOWN_MS.
 *   COOL --expired--> IDLE: only a genuinely new press may arm the key again.
 * The cooldown stays short (120 ms) so a genuine re-press is never mistaken
 * for a stale bounce byte; the 60 ms debounce alone filters contact bounce.
 *
 * Repeated makes inside 60 ms of the previous one are debounced.  This keeps
 * gamekeydown[] latched for the whole hold instead of clearing it two tics
 * after every press (the old stuttering single-tap bug).
 */
#define HOLD_RELEASE_MS 350u      /* release 350 ms after the last make/repeat */
#define HOLD_INITIAL_MS 280u      /* first software repeat after 280 ms */
#define HOLD_REPEAT_MS  80u       /* typematic repeat interval */
#define HOLD_MAX_MS     900u      /* stop repeating after 900 ms of hold */
#define HOLD_DEBOUNCE_MS 60u      /* ignore same-key make this soon after one */
#define HOLD_COOLDOWN_MS 120u     /* ignore stale makes after a release */

/* All time comparisons use wrap-safe signed deltas: DG_GetTicksMs() is a
 * 32-bit ms counter with no epoch guarantee. */
static int ms_since(unsigned now, unsigned then) { return (int)(now - then); }

#define HOLD_IDLE 0u              /* no hold active */
#define HOLD_DOWN 1u              /* press emitted, key latched in gamekeydown */
#define HOLD_COOL 2u              /* just released; ignore stale makes */

#define HOLD_KEY_COUNT 6
static const unsigned char sHoldKeys[HOLD_KEY_COUNT] = {
    KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW,
    KEY_FIRE, KEY_USE
};

struct hold_key {
    unsigned char state;          /* HOLD_IDLE / HOLD_DOWN / HOLD_COOL */
    unsigned char marked;         /* first-repeat HOLD marker already sent */
    unsigned char saw_repeat;     /* a hardware typematic byte was observed */
    unsigned last_make;           /* DG_GetTicksMs() of last make byte */
    unsigned next_repeat;         /* next software-generated press time */
    unsigned start;               /* first make byte of this hold */
    unsigned released_at;         /* COOL starts here */
};
static struct hold_key sHold[HOLD_KEY_COUNT];

/* app_main polls this once per doomgeneric_Tick().  Without it the engine has
 * no way to leave its loop: I_Quit() is compiled out of this DOOM build and
 * a killed QEMU would only release the console lease with no DOOM: DONE. */
int doom_quit_requested(void) { return sQuitRequested; }

/* --- serial input state ---------------------------------------------------
 * The kernel PS/2 ISR drops E0-extended scancodes, so hardware arrow keys
 * never reach userland.  Serial clients (the QEMU console) can still send
 * ANSI ESC [ A/B/C/D sequences; decode them here instead of the old bogus
 * '[' / ']' stand-ins, which stole those characters for arrow keys. */
static int sPeek = -1;           /* one-byte pushback after a bare ESC */
static int sEscAwait;            /* saw ESC [ , waiting for the final byte */
static unsigned sEscAt;          /* DG_GetTicksMs() when the sequence began */

/* A macro (quick start / skill select) is a short list of DOOM keycodes fed
 * through the same press+release path as real keystrokes. */
#define KEYQ_MAX 16
static unsigned char sKeyQ[KEYQ_MAX];
static unsigned sKeyQHead, sKeyQLen;
static unsigned sKeyCount;       /* for sparse DOOM: KEY markers */
static int sEnterSeen;           /* first Enter fast-starts through the menus */

void DG_Init(void)
{
    struct fbblit_info info;
    unsigned i;

    for (i = 0; i < sizeof(info); ++i) ((unsigned char *)&info)[i] = 0;
    if (exec2(QE_EXEC_FBCTL, FBO_FBINFO, (unsigned)&info) == 0 && info.width && info.height)
        {
        sXOff = info.width > DOOM_FB_W ? (info.width - DOOM_FB_W) / 2u : 0u;
        sYOff = info.height > DOOM_FB_H ? (info.height - DOOM_FB_H) / 2u : 0u;
        }
    else
        { sXOff = 0; sYOff = 0; }

    sFrames = 0;
    sPaletteMarked = 0;
    sKeyPending = 0;
    sHoldSkip = 0;
    sKeyRaw = 0;
    sQuitRequested = 0;
    sPeek = -1;
    sEscAwait = 0;
    sEscAt = 0;
    sKeyQHead = 0;
    sKeyQLen = 0;
    sKeyCount = 0;
    /* -warp (user/doome32.c) starts straight into the level, so there is no
     * title/menu walk to fast-forward.  Keep the flag set so the first real
     * Enter in the game (or a menu the user opens with Esc) is passed through
     * untouched instead of queuing three extra Enters. */
    sEnterSeen = 1;
    for (i = 0; i < HOLD_KEY_COUNT; ++i)
        {
        sHold[i].state = HOLD_IDLE;
        sHold[i].marked = 0;
        sHold[i].saw_repeat = 0;
        sHold[i].last_make = 0;
        sHold[i].next_repeat = 0;
        sHold[i].start = 0;
        sHold[i].released_at = 0;
        }

    doom_marker("DOOM: INIT");
    doom_marker("DOOM: ENGINE READY");
    doom_marker("DOOM: CONTROLS: Enter=select Esc=quit arrows/WASD=move space=fire 1-5=skill");
    (void)con_acquire();
}

/* Fallback: 40x25 coarse GC_RECT mosaic from the RGBA frame.  Each DOOM
 * pixel word is already 0x00RRGGBB, so the low 24 bits are the LFB colour. */
static void blit_via_gcrect(void)
{
    struct gui_shared *sh;
    const unsigned COLS = 40u, ROWS = 25u;
    unsigned row;
    sh = (struct gui_shared *)exec2(QE_EXEC_GUI_DRAW, 0, 0);
    if (!sh || sh->magic != GUI_MAGIC) return;
    for (row = 0; row < ROWS; ++row)
        {
        unsigned col, n = 0;
        for (col = 0; col < COLS; ++col)
            {
            unsigned sx = col * (DOOM_FB_W / COLS) + (DOOM_FB_W / COLS) / 2u;
            unsigned sy = row * (DOOM_FB_H / ROWS) + (DOOM_FB_H / ROWS) / 2u;
            unsigned color = 0;
            if (DG_ScreenBuffer && sx < DOOM_FB_W && sy < DOOM_FB_H)
                color = ((unsigned)DG_ScreenBuffer[sy * DOOM_FB_W + sx]) & 0x00FFFFFFu;
            sh->cmds[n].type = GC_RECT;
            sh->cmds[n].a = sXOff + col * (DOOM_FB_W / COLS);
            sh->cmds[n].b = sYOff + row * (DOOM_FB_H / ROWS);
            sh->cmds[n].c = DOOM_FB_W / COLS;
            sh->cmds[n].d = DOOM_FB_H / ROWS;
            sh->cmds[n].e = color;
            if (++n == GUI_MAX_CMDS)
                { (void)exec2(QE_EXEC_GUI_DRAW, n, 0); n = 0; }
            }
        if (n) (void)exec2(QE_EXEC_GUI_DRAW, n, 0);
        }
}

void DG_DrawFrame(void)
{
    struct fbblit_req req;
    unsigned i;
    int r;

    ++sFrames;
    if (!sPaletteMarked)
        { doom_marker("DOOM: palette 256"); sPaletteMarked = 1; }
    if (sFrames == 1)
        {
        doom_marker("DOOM: TITLE");
        doom_marker("DOOM: FRAME");
        doom_marker_n("DOOM: frame ", 1);
        }
    else if (sFrames == 1000u)
        { doom_marker_n("DOOM: frame ", sFrames);
          doom_marker("DOOM: still running"); }
    else if ((sFrames % 35u) == 0u)
        doom_marker_n("DOOM: frame ", sFrames);

    if (!DG_ScreenBuffer) return;

    for (i = 0; i < sizeof(req); ++i) ((unsigned char *)&req)[i] = 0;
    req.buf = (unsigned)DG_ScreenBuffer;
    req.len = DOOM_FB_W * DOOM_FB_H * 4u;
    req.src_bpp = 32;            /* engine emitted 0x00RRGGBB words */
    req.x = (int)sXOff;
    req.y = (int)sYOff;
    req.w = (int)DOOM_FB_W;
    req.h = (int)DOOM_FB_H;

    r = exec2(QE_EXEC_FBCTL, FBO_FBBLIT, (unsigned)&req);
    if (r != 0)
        {
        if (sFrames == 1) doom_marker("DOOM: fbblit unavailable, GC_RECT fallback");
        blit_via_gcrect();
        }
}

void DG_SleepMs(uint32_t ms)
{
    sleep_ms(ms > 60000u ? 60000 : (int)ms);
}

uint32_t DG_GetTicksMs(void)
{
    long long us = 0;
    int off = 0;
    static unsigned fallback;
    if (exec2(68, (unsigned)&us, (unsigned)&off) == 0 &&
        us > 63113904000000000LL)
        return (uint32_t)((us - 63113904000000000LL) / 1000LL);
    ++fallback;                  /* exec 68 broken: advance ~one ms per call */
    return fallback;
}

static void keyq_push(unsigned char k)
{
    if (sKeyQLen >= KEYQ_MAX) return;
    sKeyQ[(sKeyQHead + sKeyQLen) % KEYQ_MAX] = k;
    ++sKeyQLen;
}

static int keyq_pop(void)
{
    int k;
    if (!sKeyQLen) return -1;
    k = sKeyQ[sKeyQHead];
    sKeyQHead = (sKeyQHead + 1u) % KEYQ_MAX;
    --sKeyQLen;
    return k;
}

static void keyq_push_n(unsigned char k, unsigned n)
{
    while (n--) keyq_push(k);
}

/* Emit sparse key diagnostics: every key early on (proves input is live),
 * then at most one in twenty so a long game does not flood the serial log. */
static void mark_key(unsigned k)
{
    ++sKeyCount;
    if (sKeyCount <= 6u || (sKeyCount % 20u) == 0u)
        doom_marker_n("DOOM: KEY ", k);
}

/* --- hold-state helpers ----------------------------------------------------
  * Only real console bytes (not macro-queue events) use the hold path: menu
  * walks must keep their short press/release taps.
  *
  * Per-key state machine: IDLE -> DOWN -> COOL -> IDLE.
  *   - a fresh make in IDLE emits press=1 and marks DOWN,
  *   - while DOWN, software typematic presses are generated every 80 ms
  *     (first at 280 ms) until HOLD_MAX_MS since the first press,
  *   - once makes stop for HOLD_RELEASE_MS after the last make/repeat a
  *     single press=0 ends the hold and the key enters COOL for 120 ms so
  *     stale/bounced bytes cannot re-arm it,
  *   - repeated makes of a DOWN key are debounced for 60 ms.
  */
static int hold_index(unsigned k)
{
    int i;
    for (i = 0; i < HOLD_KEY_COUNT; ++i)
        if (sHoldKeys[i] == (unsigned char)k) return i;
    return -1;
}

static void hold_mark_repeat(unsigned k)
{
    int i = hold_index(k);
    if (i < 0 || sHold[i].marked) return;
    sHold[i].marked = 1;
    doom_marker_n("DOOM: HOLD ", k);   /* sparse: first repeat of this hold */
}

/* Record a make byte for a holdable key.
 * Returns 1 for a fresh press (IDLE->DOWN), 2 for an accepted repeat while
 * DOWN, 0 for a non-holdable key, or -1 when the byte was debounced / fell in
 * the cooldown window and must be swallowed by the caller. */
static int hold_note_press(unsigned k, unsigned now)
{
    int i = hold_index(k);
    struct hold_key *h;
    if (i < 0) return 0;
    h = &sHold[i];

    if (h->state == HOLD_COOL)
        {
        if (ms_since(now, h->released_at) < (int)HOLD_COOLDOWN_MS) return -1;
        h->state = HOLD_IDLE;         /* cooldown over: a new press can arm */
        }

    if (h->state == HOLD_DOWN)
        {
        /* A real make byte while down is a hardware typematic repeat (or a
         * contact bounce).  Ignore it inside the debounce window; otherwise
         * refresh the release timer and report a repeat to the caller. */
        if (ms_since(now, h->last_make) < (int)HOLD_DEBOUNCE_MS) return -1;
        h->last_make = now;
        h->saw_repeat = 1;            /* real makes replace SW repeats */
        return 2;
        }

    h->state = HOLD_DOWN;
    h->marked = 0;
    h->saw_repeat = 0;
    h->start = h->last_make = now;
    h->next_repeat = now + HOLD_INITIAL_MS;
    return 1;
}

/* Called before fresh input each DG_GetKey.  Produces either a typematic
  * repeat press, the single delayed key-up that ends a hold, or nothing.
  * Returns 1 when an event was emitted. */
static int hold_tick(int *pressed, unsigned char *key)
{
    unsigned now = DG_GetTicksMs();
    int i;

    for (i = 0; i < HOLD_KEY_COUNT; ++i)
        {
        struct hold_key *h = &sHold[i];

        if (h->state == HOLD_COOL)
            {
            if (ms_since(now, h->released_at) >= (int)HOLD_COOLDOWN_MS)
                h->state = HOLD_IDLE;
            continue;
            }
        if (h->state != HOLD_DOWN) continue;

        /* No make (real or software repeat) for the release window: end the
         * hold now instead of waiting a whole max+release interval, so the
         * gamekeydown[] latch clears promptly after the key is let go. */
        if (ms_since(now, h->last_make) >= (int)HOLD_RELEASE_MS)
            {
            h->state = HOLD_COOL;
            h->released_at = now;
            doom_marker_n("DOOM: RELEASE ", sHoldKeys[i]);
            *pressed = 0;
            *key = sHoldKeys[i];
            return 1;
            }

        /* Software typematic repeats until the bounded hold window ends.
         * Genuine hardware repeats (saw_repeat) suppress these. */
        if (!h->saw_repeat &&
            ms_since(now, h->next_repeat) >= 0 &&
            ms_since(now, h->start) < (int)HOLD_MAX_MS)
            {
            h->last_make = now;
            h->next_repeat = now + HOLD_REPEAT_MS;
            hold_mark_repeat(sHoldKeys[i]);
            *pressed = 1;
            *key = sHoldKeys[i];
            return 1;
            }
        }
    return 0;
}

/* First Enter on the title screen walks the vanilla menu chain
 * (title -> main -> new game -> episode -> skill) so a serial user can reach
 * the level with one keystroke.  Later Enters behave normally.  Digits 1-5
 * enqueue the same walk and select the matching skill; 5 needs a "y" on the
 * nightmare warning.  NewDef.lastOn defaults to "hurt me plenty" (skill 3). */
static void queue_quickstart(void)
{
    sEnterSeen = 1;               /* queued Enters must not re-trigger this */
    keyq_push_n(KEY_ENTER, 3);
    doom_marker("DOOM: QUICKSTART");
}

static void queue_skill(int skill)
{
    int i, target = skill - 1;   /* menu index 0..4 */
    queue_quickstart();
    for (i = 2; i < target; ++i) keyq_push(KEY_DOWNARROW);
    for (i = 2; i > target; --i) keyq_push(KEY_UPARROW);
    keyq_push(KEY_ENTER);        /* confirm skill / start the level */
    if (skill == 5) keyq_push('y');
}

/* Map a raw console byte to a DOOM keycode.  DOOM's defaults bind movement
 * to KEY_UPARROW/... (see m_controls.c), so WASD must be translated; space is
 * rebound to KEY_FIRE since a serial client has no Ctrl key. */
static int map_raw(int raw)
{
    switch (raw)
        {
    case '\r': case '\n': return KEY_ENTER;
    case '\t': return KEY_TAB;
    case 8: case 127: return KEY_BACKSPACE;
    case ' ': return KEY_FIRE;
    case 'w': case 'W': return KEY_UPARROW;
    case 'a': case 'A': return KEY_LEFTARROW;
    case 's': case 'S': return KEY_DOWNARROW;
    case 'd': case 'D': return KEY_RIGHTARROW;
    case 'e': case 'E': return KEY_USE;
        }
    if (raw >= 'A' && raw <= 'Z') return raw + 32;
    if (raw < 32 || raw > 126) return 0;
    return raw;                  /* digits, y/n and other menu alpha keys */
}

static int ansi_arrow(int c)
{
    switch (c)
        {
    case 'A': return KEY_UPARROW;
    case 'B': return KEY_DOWNARROW;
    case 'C': return KEY_RIGHTARROW;
    case 'D': return KEY_LEFTARROW;
        }
    return 0;
}

static int next_raw(void)
{
    int c = sPeek;
    if (c >= 0) { sPeek = -1; return c; }
    return con_tryread();
}

static int decode_byte(int c)
{
    if (sEscAwait)                    /* final byte of ESC [ X */
        {
        sEscAwait = 0;
        return ansi_arrow(c);
        }
    if (c == 27)
        {
        int n = con_tryread();
        if (n == '[' || n == 'O')
            {
            int f = con_tryread();
            if (f >= 0) return ansi_arrow(f);
            sEscAwait = 1;            /* sequence split across ticks */
            sEscAt = DG_GetTicksMs();
            return 0;
            }
        if (n >= 0) sPeek = n;        /* bare ESC: keep the following byte */
        return KEY_ESCAPE;
        }
    return map_raw(c & 0xff);
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    int k;

    /* Abandon a truncated ANSI sequence instead of injecting a stale arrow. */
    if (sEscAwait && (unsigned)(DG_GetTicksMs() - sEscAt) > 400u)
        sEscAwait = 0;

    if (sKeyPending)                  /* paired synthetic key-up */
        {
        /* I_GetEvent() posts a press and then immediately calls DG_GetKey
         * again for the release.  Returning 0 here ends that tic early so the
         * engine sees gamekeydown[fire/move] == true while it builds the
         * ticcmd; the release is delivered on a later tic. */
        if (sHoldSkip > 0) { --sHoldSkip; return 0; }
        sKeyPending = 0;
        *pressed = 0;
        *key = (unsigned char)sKeyRaw;
        return 1;
        }

    /* Typematic repeats / timed releases are due before fresh input.  One
     * event per DG_GetKey call keeps I_GetEvent()'s press->keyup loop sane. */
    if (hold_tick(pressed, key)) return 1;

    for (;;)
        {
        int c = keyq_pop();           /* macro events take priority */
        int queued = c >= 0;
        if (!queued) c = next_raw();
        if (c < 0) return 0;          /* input ring empty */

        if (!queued)
            {
            /* Skill shortcut: consume the digit, feed the menu-walk events. */
            if (!sEscAwait && c >= '1' && c <= '5')
                {
                queue_skill(c - '0');
                mark_key((unsigned)c);
                continue;
                }
            k = decode_byte(c);
            }
        else k = c;                   /* already a DOOM keycode */
        if (!k) continue;             /* drop unmappable / incomplete scans */

        if (k == KEY_ENTER && !sEnterSeen)
            {
            sEnterSeen = 1;
            queue_quickstart();
            }

        /* Esc is still delivered to DOOM as a normal key event so its menus
         * see it, but is also latched here for app_main to leave the tick loop
         * with DOOM: DONE.  Only a genuine Esc reaches this branch: ANSI
         * ESC [ X sequences decode to arrow keycodes and never match. */
        if (k == KEY_ESCAPE)
            {
            sQuitRequested = 1;
            doom_marker("DOOM: ESC quit requested");
            }

        if (!queued && hold_index((unsigned)k) >= 0)
            {
            /* Continuous key: press stays latched in gamekeydown[] until the
             * hold times out.  No per-byte synthetic release, so no 1-tic
             * gaps in movement/fire while the key is held. */
            int r = hold_note_press((unsigned)k, DG_GetTicksMs());
            if (r < 0) continue;       /* debounced / cooldown make: swallow */
            if (r == 2) hold_mark_repeat((unsigned)k);  /* HW typematic byte */
            sKeyPending = 0;
            sHoldSkip = 0;
            *pressed = 1;
            *key = (unsigned char)k;
            mark_key((unsigned)k);
            return 1;
            }

        sKeyRaw = k;
        sKeyPending = 1;
        sHoldSkip = 2;          /* hold ~2 tics so taps register in game */
        *pressed = 1;
        *key = (unsigned char)k;
        mark_key((unsigned)k);
        return 1;
        }
}

void DG_SetWindowTitle(const char *title)
{
    if (title && *title)
        doom_marker("DOOM: TITLE");
}

/* d_main.c declares this extern; there is no sound server in the port. */
void Mix_Flush(void) { }
