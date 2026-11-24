/* e32_platform.c -- doomgeneric platform callbacks for the EKA2 x86 port.
 *
 * Video: DG_ScreenBuffer is the engine's 640x400 RGBA8888 frame (the engine
 * already converts the WAD PLAYPAL palette in I_SetPalette/cmap_to_fb).  A
 * single bulk 32bpp blit through the genuine kernel exec 270 op 3 (the
 * fbblit capability, logical execs 274/275) paints the whole frame to the
 * Bochs VBE LFB in kernel context.
 *
 * Input: PS/2/serial keys are polled with con_tryread() under the PortCon
 * lease and mapped to DOOM keycodes.  The kernel PS/2 ISR forwards a key-up as
 * 0x80|ascii and suppresses hardware typematic repeats in graphics mode, so
 * as soon as ONE break byte is observed the port enters DIRECT mode: a make
 * emits press=1, a break emits press=0, 1:1.  Vanilla DOOM latches a held key
 * in gamekeydown[] on the single keydown event (g_game.c G_Responder), so a
 * make/break pair is the entire input protocol -- no typematic emulation, no
 * delays, no hold state machine.  Until a break byte is seen we may be on a
 * serial pipe that only sends makes; a small time-based fallback (software
 * repeat + a ~200 ms synthetic release) then supplies the missing key-up.
 * The fallback is strictly for headless tests and is discarded permanently on
 * the first real break.
 *
 * Timing: exec 68 (EExecTimeNow) microseconds, rebased to the 2000 epoch.
 */
#include "doomgeneric.h"
#include "doomkeys.h"
#include "e32_shim.h"
#include "runtime.h"

#include "qemu_fbblit.h"      /* variant/qemu_fbblit.h: exec 270 ops 2/3 */

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
static int sQuitRequested;       /* Esc seen: let app_main exit cleanly */

/* --- input mode ------------------------------------------------------------
 * Set the first time a high-bit break byte (0x80|ascii) arrives from the
 * console.  From then on keyboard input is authoritative make/break pairs and
 * the time-based fallback is dead.  This is the single switch between the two
 * input paths -- there is no per-key state machine in DIRECT mode. */
static int sHaveBreakBytes;

/* --- deferred synthetic key-ups (FALLBACK / macro only) --------------------
 * Queued macro keys (quickstart / skill walk) and one-shot keys seen BEFORE
 * any break byte have no real key-up, so their press=0 is deferred a couple of
 * tics: I_GetEvent() loops on DG_GetKey() until it sees press=0, so a coincident
 * keydown+keyup would otherwise land in one tic.  Give every DOOM keycode its
 * own slot so one key's pending release can never be clobbered by another.
 * DIRECT-mode one-shot keys use their real break byte instead: no sPend entry
 * is armed for them. */
struct pend_key {
    unsigned char active;         /* a synthetic keyup is waiting for this key */
    unsigned char skip;           /* DG_GetKey calls left before delivering it */
};
#define PEND_KEY_COUNT 256        /* DOOM keycodes are all <= 0xff */
static struct pend_key sPend[PEND_KEY_COUNT];

/* --- time-based FALLBACK hold (only while sHaveBreakBytes == 0) -------------
 * A serial pipe sends makes only; synthesize the release after a quiet gap and
 * emulate typematic so a held key actually moves the player.  The first real
 * break byte resets all of this (see reset_hold_states()). */
#define HOLD_RELEASE_MS 200u      /* fallback release after the last make/repeat */
#define HOLD_INITIAL_MS 60u       /* first software repeat */
#define HOLD_REPEAT_MS  80u       /* typematic repeat interval */
#define HOLD_MAX_MS     900u      /* stop repeating after 900 ms of hold */

#define HOLD_IDLE 0u              /* no fallback hold active */
#define HOLD_DOWN 1u              /* press emitted, key latched in gamekeydown */

/* Every key DOOM reads continuously in gamekeydown[] gets its own fallback
 * slot.  WASD and space/'e' are mapped to these keycodes in map_raw(), so they
 * share the corresponding slot; the strafe/run modifiers are included too. */
#define HOLD_KEY_COUNT 10
static const unsigned char sHoldKeys[HOLD_KEY_COUNT] = {
    KEY_UPARROW, KEY_DOWNARROW, KEY_LEFTARROW, KEY_RIGHTARROW,
    KEY_FIRE, KEY_USE,
    KEY_STRAFE_L, KEY_STRAFE_R,
    KEY_RSHIFT,                   /* run / speed */
    KEY_RALT                      /* strafe modifier (== KEY_LALT) */
};

struct hold_key {
    unsigned char state;          /* HOLD_IDLE / HOLD_DOWN */
    unsigned char marked;         /* first-repeat HOLD marker already sent */
    unsigned last_make;           /* last make or software repeat */
    unsigned next_repeat;         /* next software-generated press time */
    unsigned start;               /* first make byte of this hold */
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
static unsigned sKeyCount;       /* sparse KEY / HOLD press markers */
static unsigned sBreakCount;     /* sparse BREAK markers */
static unsigned sStaleCount;     /* sparse STALE markers (unmapped break) */
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
    sHaveBreakBytes = 0;
    for (i = 0; i < PEND_KEY_COUNT; ++i)
        {
        sPend[i].active = 0;
        sPend[i].skip = 0;
        }
    sQuitRequested = 0;
    sPeek = -1;
    sEscAwait = 0;
    sEscAt = 0;
    sKeyQHead = 0;
    sKeyQLen = 0;
    sKeyCount = 0;
    sBreakCount = 0;
    sStaleCount = 0;
    /* -warp (user/doome32.c) starts straight into the level, so there is no
     * title/menu walk to fast-forward.  Keep the flag set so the first real
     * Enter in the game (or a menu the user opens with Esc) is passed through
     * untouched instead of queuing three extra Enters. */
    sEnterSeen = 1;
    for (i = 0; i < HOLD_KEY_COUNT; ++i)
        {
        sHold[i].state = HOLD_IDLE;
        sHold[i].marked = 0;
        sHold[i].last_make = 0;
        sHold[i].next_repeat = 0;
        sHold[i].start = 0;
        }

    doom_marker("DOOM: INIT");
    doom_marker("DOOM: ENGINE READY");
    doom_marker("DOOM: CONTROLS: Enter=select Esc=quit arrows/WASD=move space=fire 1-5=skill");
    (void)con_acquire();
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
    if (r != 0 && sFrames == 1)
        doom_marker_n("DOOM: fbblit error ", (unsigned)r);
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

/* Emit sparse press diagnostics.  A held (holdable) key uses HOLD so the
 * stress harness does not demand a release within 250 ms of a long hold's
 * initial make; one-shot taps use KEY.  Early presses are all logged to prove
 * input is live, then at most one in twenty. */
static void mark_press(unsigned k, int holdable)
{
    ++sKeyCount;
    if (sKeyCount <= 6u || (sKeyCount % 20u) == 0u)
        doom_marker_n(holdable ? "DOOM: HOLD " : "DOOM: KEY ", k);
}

/* --- FALLBACK hold helpers (never used once sHaveBreakBytes is set) --------- */
static int hold_index(unsigned k)
{
    int i;
    for (i = 0; i < HOLD_KEY_COUNT; ++i)
        if (sHoldKeys[i] == (unsigned char)k) return i;
    return -1;
}

static int ms_since(unsigned now, unsigned then) { return (int)(now - then); }

/* Clear every fallback hold.  Called exactly once, on the first break byte:
 * DIRECT mode owns all keys from then on, so a stale fallback timer must never
 * fire a second (bogus) release for a key that has already been released. */
static void reset_hold_states(void)
{
    unsigned i;
    for (i = 0; i < HOLD_KEY_COUNT; ++i)
        {
        sHold[i].state = HOLD_IDLE;
        sHold[i].marked = 0;
        }
}

/* Latch a key down for the serial fallback.  Returns 1 for a fresh press,
 * 2 for a repeated make while down. */
static int hold_note_press(unsigned k, unsigned now)
{
    int i = hold_index(k);
    struct hold_key *h;
    if (i < 0) return 0;
    h = &sHold[i];

    if (h->state == HOLD_DOWN)
        {
        h->last_make = now;       /* refresh the synthetic release timer */
        return 2;
        }
    h->state = HOLD_DOWN;
    h->marked = 0;
    h->start = h->last_make = now;
    h->next_repeat = now + HOLD_INITIAL_MS;
    return 1;
}

/* Software typematic + quiet-gap release for serial-only input.  One event per
 * call keeps I_GetEvent()'s press->keyup loop sane. */
static int hold_tick(int *pressed, unsigned char *key)
{
    unsigned now = DG_GetTicksMs();
    int i;

    for (i = 0; i < HOLD_KEY_COUNT; ++i)
        {
        struct hold_key *h = &sHold[i];
        if (h->state != HOLD_DOWN) continue;

        if (ms_since(now, h->last_make) >= (int)HOLD_RELEASE_MS)
            {
            h->state = HOLD_IDLE;
            doom_marker_n("DOOM: RELEASE ", sHoldKeys[i]);
            *pressed = 0;
            *key = sHoldKeys[i];
            return 1;
            }
        if (ms_since(now, h->next_repeat) >= 0 &&
            ms_since(now, h->start) < (int)HOLD_MAX_MS)
            {
            h->last_make = now;
            h->next_repeat = now + HOLD_REPEAT_MS;
            if (!h->marked)
                {
                h->marked = 1;
                doom_marker_n("DOOM: HOLD ", sHoldKeys[i]);
                }
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

    /* Deliver any deferred synthetic key-up (queued macro / fallback one-shot)
     * before reading fresh input.  Deliver at most one per call: I_GetEvent()
     * breaks out of its loop on the first press=0. */
        {
        int pi, any_pending = 0;
        for (pi = 0; pi < PEND_KEY_COUNT; ++pi)
            {
            if (!sPend[pi].active) continue;
            if (sPend[pi].skip > 0)
                {
                --sPend[pi].skip;
                any_pending = 1;
                continue;
                }
            sPend[pi].active = 0;
            *pressed = 0;
            *key = (unsigned char)pi;
            return 1;
            }
        if (any_pending) return 0;    /* keep the tap latched this tic */
        }

    /* Serial-only typematic / synthetic release.  Permanently disabled after
     * the first genuine break byte switches us to DIRECT mode. */
    if (!sHaveBreakBytes && hold_tick(pressed, key)) return 1;

    for (;;)
        {
        int c = keyq_pop();           /* macro events take priority */
        int queued = c >= 0;
        if (!queued) c = next_raw();
        if (c < 0) return 0;          /* input ring empty */

        if (!queued && (c & 0x80))
            {
            /* DIRECT mode: the kernel PS/2 ISR forwards a key-up as 0x80|ascii.
             * Decode with the SAME map_raw() as make bytes and emit press=0 at
             * once -- no debounce, no timer, no state.  Seeing any break byte
             * also retires the serial fallback for good.  A break can never be
             * part of an ESC [ X arrow sequence. */
            int bk;
            if (!sHaveBreakBytes)
                {
                sHaveBreakBytes = 1;
                reset_hold_states();
                }
            sEscAwait = 0;
            bk = map_raw(c & 0x7f);
            if (!bk)
                {
                ++sStaleCount;
                if (sStaleCount <= 8u || (sStaleCount % 20u) == 0u)
                    doom_marker_n("DOOM: STALE ", c & 0x7f);
                continue;             /* unmapped break: drop */
                }
            /* A RELEASE marker pairs EVERY emitted hold press (KEY markers
             * are sparse); BREAK is the sparse explicit key-up marker. */
            if (hold_index((unsigned)bk) >= 0)
                doom_marker_n("DOOM: RELEASE ", (unsigned)bk);
            ++sBreakCount;
            if (sBreakCount <= 8u)
                doom_marker_n("DOOM: BREAK ", (unsigned)bk);
            *pressed = 0;
            *key = (unsigned char)bk;
            return 1;
            }

        if (!queued)
            {
            /* Skill shortcut: consume the digit, feed the menu-walk events. */
            if (!sEscAwait && c >= '1' && c <= '5')
                {
                queue_skill(c - '0');
                mark_press((unsigned)c, 0);
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

        if (!queued && sHaveBreakBytes)
            {
            /* DIRECT mode: make -> press=1, break -> press=0, nothing else.
             * gamekeydown[] stays latched between the two events, so a held
             * key needs no repeats and multiple keys hold independently. */
            int holdable = (hold_index((unsigned)k) >= 0);
            *pressed = 1;
            *key = (unsigned char)k;
            mark_press((unsigned)k, holdable);
            return 1;
            }

        if (!queued && hold_index((unsigned)k) >= 0)
            {
            /* FALLBACK only: latch this holdable key; no synthetic per-byte
             * release, so movement/fire do not get 1-tic gaps. */
            int r = hold_note_press((unsigned)k, DG_GetTicksMs());
            if (r < 0) continue;
            if (r == 2)
                doom_marker_n("DOOM: HOLD ", (unsigned)k);
            *pressed = 1;
            *key = (unsigned char)k;
            mark_press((unsigned)k, 1);
            return 1;
            }

        /* One-shot key (fallback raw or queued macro): arm THIS key's own
         * deferred release.  DIRECT-mode one-shots never reach here -- their
         * real break byte already produced the matching press=0. */
        sPend[k & 0xff].active = 1;
        sPend[k & 0xff].skip = 2;     /* hold ~2 tics so taps register in game */
        *pressed = 1;
        *key = (unsigned char)k;
        mark_press((unsigned)k, 0);
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
