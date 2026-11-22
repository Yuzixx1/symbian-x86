#!/usr/bin/env python3
"""r1_doom_release_test.py -- measure DOOM's key-RELEASE latency in wall time.

The complaint this test targets: after letting go of a movement key DOOM keeps
walking for ~a second.  Guessing from serial markers is not enough, so this
harness measures two independent clocks for every input path:

  1. HOST WALL CLOCK.  A background thread timestamps every serial
     ``DOOM: KEY / HOLD / RELEASE / BREAK`` marker the instant it appears in the
     growing serial log.  QEMU HMP ``sendkey w <hold_ms>`` gives a precisely
     scheduled PS/2 make (now) and break (hold_ms later); the raw pipe has no
     break at all, so its timeout-driven release is measured separately.

  2. FRAME CLOCK.  A held-open QMP session takes a dense burst of PNG
     screendumps spanning the release.  PIL compares the game-region luma of
     consecutive frames; idle is ~0.03% changed pixels.  The first post-release
     frame back at the idle level is when movement actually stopped.  The
     release->stop interval is the number the test asserts on.

Phases (all at E1M1; `doom` already warps straight into the level):
  * idle pair -> baseline,
  * window path: HMP `sendkey w 800`, frames during hold + +50..+1000 ms,
  * short tap:   HMP `sendkey w 80`, frames through +800 ms,
  * serial path: one raw 'w' byte, frames through +2000 ms.

VISUAL CRITERION (player motion, not whole-screen animation).  The old
game-region motion/mad gate is still computed and printed for comparison, but
it no longer decides the verdict: it includes monster AI, weapon bob and
lighting, so it reports "motion persists" even after the player has stopped.
The verdict instead uses a small CENTER region of the 640x400 game area -- a
wall/geometry edge that only changes when the player actually moves.  Its
per-pixel luma diff COUNT (>24) is measured for consecutive frames and
calibrated against the idle baseline taken at startup.  After the
RELEASE/BREAK marker the count must return to <= 2x idle baseline within
400 ms (window) / 250 ms (tap) and STAY there for 3 consecutive samples.
That is the honest "key no longer held" proof.

A screendump failure is informational only: every PNG call is best-effort and
never fails the test.  Missing DOOM assets => RESULT: SKIP, exit 0.

Usage: python3 tools/r1_doom_release_test.py [--no-build-iso] [--log PATH]
                                             [--hold-ms 800] [--tap-ms 80]
"""
import argparse
import os
import re
import subprocess
import sys
import threading
import time

import numpy as np
from PIL import Image

import r1_doom_test as base
import r1_doom_audio_test as audio
import r1_doom_visual_test as visual

TMP = base.TMP
LOG = TMP + "/r1_doom_release.log"
QMP = TMP + "/r1_doom_release-qmp.sock"
ISO = base.ISO
BOOT_TIMEOUT = 85                # GRUB + EKA2 init under host load (proven value)
QEMU_CAP = 130                   # hard outer cap; boot ~35 s + ~20 s of probes
EXC_KEYS = base.EXC_KEYS
WKEY = "173"                     # KEY_UPARROW == 'w' in this port

# Named captures required by the test plus extra dense samples for timing.
SHOT_NAMES = (
    "idle0", "idle1", "idle2", "idle3",
    "hold_mid", "hold_end",
    "rel050", "rel100", "rel200", "rel300", "rel400", "rel600", "rel800",
    "rel1000",
    "tap_hold", "tap100", "tap200", "tap400", "tap800", "tap1200", "tap1600",
    "ser000", "ser400", "ser800", "ser1200", "ser1600", "ser2000",
    "ser2400", "ser2800",
)
SHOT = {n: TMP + "/release_%s.png" % n for n in SHOT_NAMES}

# DOOM marker family.  A BREAK marker is only emitted once the DOOM-side
# break-handling change lands; the regex tolerates it being absent.
MARK_RE = re.compile(r"DOOM:\s*(KEY|HOLD|RELEASE|BREAK|RELEASED|KEYUP)\s+(\d+)",
                     re.I)
LOOSE_RE = re.compile(r"DOOM:\s*(BREAK|RELEASE|RELEASED|KEYUP)", re.I)
IDLE_MAX_PCT = 1.0               # OLD whole-region % gate (reported only)
IDLE_MAX_MAD = 2.0               # OLD whole-region mad guard (reported only)

# --- PLAYER-MOTION criterion -------------------------------------------------
# A small central patch of the 640x400 game area.  At E1M1 start this is a
# fixed wall/geometry edge: walking forward repaints it heavily, standing
# still leaves it byte-identical (idle diff count == 0).  Monster AI, weapon
# bob and lighting live elsewhere, so this is a clean "is the player moving"
# signal.  Coordinates are (x0,y0,x1,y1) in the nominal 640x400 game area.
CENTER_REGION = (280, 180, 360, 220)
CENTER_DIFF_THRESHOLD = 24       # luma abs diff above this == changed pixel
CENTER_IDLE_FACTOR = 2           # gate = factor * idle baseline count
CENTER_MIN_COUNT = 16            # noise floor when idle baseline is 0 (0.5% of region)
CENTER_CONSECUTIVE = 3           # samples that must stay back at idle level


# ------------------------------------------------------------------ helpers
class ShotSession(audio.QMPSession):
    """Held-open QMP connection with HMP input and precisely-timed PNG dumps.

    Reuses the proven greeting/capabilities handshake from the audio test, then
    adds `sendkey()` (QMP human-monitor-command) and a screendump that polls for
    the PNG every 5 ms so capture timestamps are accurate to a few ms.
    """

    def cmd(self, command):
        if self.stream is None:
            return None
        try:
            self.stream.write((command + "\n").encode())
            self.stream.flush()
            for _ in range(12):
                line = self.stream.readline()
                if not line:
                    return None
                if b'"event"' in line:
                    continue
                if b'"return"' in line or b'"error"' in line:
                    if b'"error"' in line:
                        print("[WARN] QMP error: " + line.decode()[:140])
                    return line
        except Exception as exc:
            print("[WARN] QMP command failed: %s" % exc)
        return None

    def sendkey(self, key, hold_ms):
        """Press `key` now; QEMU schedules the break after `hold_ms`.

        Returns the monotonic issue time (the nominal press), or None.  HMP
        `sendkey` returns immediately, so screendumps can be taken during hold.
        """
        if self.stream is None:
            return None
        cmd = ('{"execute":"human-monitor-command","arguments":'
               '{"command-line":"sendkey %s %d"}}' % (key, hold_ms))
        t0 = time.monotonic()
        return t0 if self.cmd(cmd) is not None else None

    def screendump(self, png):
        """Best-effort PNG; return monotonic capture time or None, never raise."""
        if self.stream is None:
            return None
        tmp = png + ".tmp"
        try:
            if os.path.exists(tmp):
                os.remove(tmp)
            cmd = ('{"execute":"screendump","arguments":{"filename":"'
                   + tmp + '","format":"png"}}')
            self.cmd(cmd)
        except Exception as exc:
            print("[WARN] screendump command failed: %s" % exc)
            return None
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            try:
                if os.path.exists(tmp) and os.path.getsize(tmp) > 1000:
                    break
            except OSError:
                pass
            time.sleep(0.005)
        try:
            if os.path.exists(tmp) and os.path.getsize(tmp) > 1000:
                os.replace(tmp, png)
                return time.monotonic()
        except OSError as exc:
            print("[WARN] screendump promote failed: %s" % exc)
        print("[WARN] screendump PNG not produced (%s)" % os.path.basename(png))
        return None


class MarkerWatch:
    """Poll the growing serial log every 8 ms; timestamp DOOM input markers."""

    def __init__(self, path):
        self.path = path
        self.off = 0
        self.buf = b""
        self.events = []
        self._stop = threading.Event()
        self.thread = None

    def arm(self):
        self.stop()
        try:
            self.off = os.path.getsize(self.path)
        except OSError:
            self.off = 0
        self.buf = b""
        self.events = []
        self._stop.clear()
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def _add(self, kind, key):
        # doom_marker() prints through EUSER ord 915 AND the DebugPrint
        # fallback.  The two copies can be ~0.8 s apart in the log, so only the
        # FIRST occurrence of each (marker,key) in a phase is a real timestamp.
        if any(e[1] == kind and e[2] == key for e in self.events):
            return
        self.events.append((time.monotonic(), kind, key))

    def _feed(self, data):
        self.buf += data
        while True:
            i = self.buf.find(b"\n")
            if i < 0:
                break
            line = self.buf[:i].decode("utf-8", "replace").strip()
            self.buf = self.buf[i + 1:]
            m = MARK_RE.search(line)
            if m:
                self._add(m.group(1).upper(), m.group(2))
                continue
            m = LOOSE_RE.search(line)
            if m:
                self._add(m.group(1).upper(), "?")

    def _run(self):
        while not self._stop.is_set():
            try:
                with open(self.path, "rb") as f:
                    f.seek(self.off)
                    data = f.read()
                    if data:
                        self.off += len(data)
                        self._feed(data)
            except OSError:
                pass
            time.sleep(0.008)

    def stop(self):
        if self.thread is not None:
            self._stop.set()
            self.thread.join(timeout=1.0)
            self.thread = None
        return list(self.events)


def capture_at(sess, name, t_start, rel):
    """Sleep until t_start+rel, grab one PNG; return (name, t, rel) or None."""
    target = t_start + rel
    now = time.monotonic()
    if target > now:
        time.sleep(target - now)
    t = sess.screendump(SHOT[name])
    return (name, t, round((t - t_start) if t else rel, 3)) if t else None


def capture_series(sess, t_start, plan):
    out = []
    for name, rel in plan:
        try:
            item = capture_at(sess, name, t_start, rel)
        except Exception as exc:
            print("[WARN] best-effort capture %s failed: %s" % (name, exc))
            item = None
        if item:
            out.append(item)
    return out


def center_frame_diff(a, b):
    """PLAYER-MOTION diff: changed-pixel count in the CENTER wall region.

    Same luma threshold as the visual module's lit mask (>24), but restricted
    to the small central patch that only changes when the player moves.  Returns
    {"count": N, "pct": P, "mad": M, "area": A}; count is the honest metric.
    """
    try:
        with Image.open(a) as ia, Image.open(b) as ib:
            A = np.asarray(ia.convert("L"), dtype=np.int16)
            B = np.asarray(ib.convert("L"), dtype=np.int16)
        if A.shape != B.shape:
            return {}
        h, w = A.shape
        gx0, gy0, gx1, gy1 = visual.frame_box(w, h)
        gw, gh = gx1 - gx0, gy1 - gy0
        cx0, cy0, cx1, cy1 = CENTER_REGION
        # Map the nominal 640x400 coordinates onto the actual game box.
        x0 = gx0 + cx0 * gw // 640
        x1 = gx0 + cx1 * gw // 640
        y0 = gy0 + cy0 * gh // 400
        y1 = gy0 + cy1 * gh // 400
        d = np.abs(A[y0:y1, x0:x1] - B[y0:y1, x0:x1])
        lit = d > CENTER_DIFF_THRESHOLD
        cnt = int(lit.sum())
        return {"count": cnt, "pct": round(100.0 * cnt / max(1, lit.size), 4),
                "mad": round(float(d.mean()), 4), "area": int(lit.size)}
    except Exception as exc:
        print("[WARN] center frame diff failed: %s" % exc)
    return {}


def frame_diff(a, b):
    """Both metrics for consecutive frames: OLD whole-region and NEW center.

    old: pct/mad (whole game region, includes monster/weapon/lighting noise).
    new: ccount/cpct/cmad (center wall region == player movement).
    """
    out = {}
    try:
        d = visual.compare_frames(a, b)
        if d.get("ok"):
            out = {"pct": d["game_pct"], "mad": d["game_mean_abs"]}
    except Exception as exc:
        print("[WARN] frame diff failed: %s" % exc)
    c = center_frame_diff(a, b)
    out.update({"ccount": c.get("count", -1), "cpct": c.get("pct", -1.0),
                "cmad": c.get("mad", -1.0)})
    return out


def is_idle(d):
    """OLD whole-region idle test (kept only for the comparison stop time)."""
    return d.get("pct", 999) < IDLE_MAX_PCT and d.get("mad", 999) <= IDLE_MAX_MAD


def center_idle(d, gate):
    """NEW player-motion idle test: center changed-pixel count within gate."""
    return d.get("ccount", -1) >= 0 and d["ccount"] <= gate


def analyse(shots, first, t_release, gate):
    """Consecutive-frame trace plus OLD and NEW release->stop times.

    OLD (`stop_old`): the whole-region criterion kept for comparison only --
    first post-release frame whose entire remaining tail is at idle level.

    NEW (`stop`): the honest player-motion criterion.  The first post-release
    sample whose center-region changed-pixel count is <= gate AND which is
    followed by CENTER_CONSECUTIVE-1 more samples also <= gate.  A single low
    count is not proof: the guest renders slower than the screendump cadence,
    so two dumps can show the same stale framebuffer while DOOM still walks.
    """
    if not shots or first is None:
        return [], None, None
    trace = []
    for prev, cur in zip([first] + shots, shots):
        d = frame_diff(SHOT[prev[0]], SHOT[cur[0]])
        d["at"] = cur[1]
        d["rel"] = cur[2]
        d["name"] = cur[0]
        d["post"] = cur[1] >= t_release
        d["idle"] = is_idle(d)
        d["cidle"] = center_idle(d, gate)
        trace.append(d)
    # OLD: whole-region tail all idle (>=2 frames), comparison only.
    stop_old = None
    for i, d in enumerate(trace):
        tail = trace[i:]
        if d["post"] and len(tail) >= 2 and all(x["idle"] for x in tail):
            stop_old = d["at"]
            break
    # NEW: 3 consecutive center-region idle samples after release.
    stop = None
    for i, d in enumerate(trace):
        if not d["post"] or d.get("ccount", -1) < 0:
            continue
        window = trace[i:i + CENTER_CONSECUTIVE]
        if len(window) == CENTER_CONSECUTIVE and all(x["cidle"] for x in window):
            stop = d["at"]
            break
    return trace, stop, stop_old


def fmt_events(events, t0=None):
    """Compact 'K@0.012 H@0.288 R@0.832' marker string."""
    names = {"KEY": "K", "HOLD": "H", "RELEASE": "R", "BREAK": "B",
             "RELEASED": "R", "KEYUP": "B"}
    parts = []
    for t, kind, key in events:
        rel = t - t0 if t0 is not None else 0.0
        parts.append("%s@%.3f%s" % (names.get(kind, kind[0]), rel,
                                    ("/" + key) if key != "?" else ""))
    return " ".join(parts) if parts else "(none)"


def has_release(events):
    return any(k in ("RELEASE", "BREAK", "RELEASED", "KEYUP")
               for _, k, _ in events)


# ------------------------------------------------------------------ the run
def run_qemu(log_path, hold_ms, tap_ms):
    if os.path.exists(QMP):
        os.remove(QMP)
    qemu = [
        "timeout", str(QEMU_CAP), "qemu-system-i386",
        "-M", "q35", "-m", "128", "-vga", "std", "-display", "none",
        "-serial", "stdio", "-qmp", "unix:%s,server,nowait" % QMP,
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-no-reboot", "-cdrom", ISO, "-boot", "d",
    ]
    print("== QEMU boot (GRUB ISO, -vga std, cap %ss) ==" % QEMU_CAP)
    res = {"booted": False, "started": False, "phases": {}, "elapsed": 0.0,
           "shots": {n: False for n in SHOT_NAMES}}
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        sess = None
        try:
            t0 = time.monotonic()
            res["booted"] = audio.wait_for_prompt(log_path, BOOT_TIMEOUT)
            print("[INFO] X:/> prompt after %.1fs" % (time.monotonic() - t0))
            if not res["booted"]:
                return res
            time.sleep(0.7)
            base.write_keys(vm, b"doom\n", "DOOM launch")
            res["started"] = base.wait_for_bytes(
                log_path, [b"DOOM: WARP E1M1", b"DOOM: ENGINE READY",
                           b"DOOM: TITLE"], 30)
            res["started"] &= base.wait_for_bytes(
                log_path, [b"DOOM: FRAME", b"DOOM: frame"], 25)
            if not res["started"]:
                print("[WARN] DOOM never reached E1M1 frames")
                return res
            time.sleep(2.0)                      # E1M1 paints + input ring drains

            sess = ShotSession(QMP)
            if sess.stream is None:
                print("[WARN] QMP session unavailable; screendumps skipped")
            watch = MarkerWatch(log_path)

            # --- idle baseline ------------------------------------------------
            idle = capture_series(sess, time.monotonic(),
                                  [("idle0", 0.0), ("idle1", 0.25),
                                   ("idle2", 0.60), ("idle3", 1.00)])
            idle_diff = frame_diff(SHOT["idle0"], SHOT["idle1"])
            print("[INFO] idle baseline (old whole-region): pct=%.3f%% mad=%.3f" %
                  (idle_diff.get("pct", -1), idle_diff.get("mad", -1)))
            res["idle_diff"] = idle_diff
            idle_trace = [frame_diff(SHOT[a[0]], SHOT[b[0]])
                          for a, b in zip(idle, idle[1:])]
            print("[INFO] idle trace over 1s: %s" % idle_trace)
            res["idle_trace"] = idle_trace

            # Center-region idle baseline: use the worst (max) of the 3 idle
            # pairs so the gate covers startup noise, with a small noise floor
            # in case every pair is byte-identical (idle count == 0).
            idle_counts = [d.get("ccount", -1) for d in idle_trace]
            good = [c for c in idle_counts if c >= 0]
            baseline = max(good) if good else 0
            gate = max(CENTER_IDLE_FACTOR * baseline, CENTER_MIN_COUNT)
            res["center_baseline"] = baseline
            res["center_gate"] = gate
            res["center_idle_counts"] = idle_counts
            print("[INFO] PLAYER-MOTION baseline: center region %s, idle counts=%s, "
                  "baseline=%d, gate=max(%dx%d,%d)=%d changed px" %
                  (CENTER_REGION, idle_counts, baseline, CENTER_IDLE_FACTOR,
                   baseline, CENTER_MIN_COUNT, gate))

            # --- phase 1: window path, 800 ms hold ---------------------------
            watch.arm()
            t_press = sess.sendkey("w", hold_ms)
            if t_press is None:
                print("[WARN] HMP sendkey failed for window hold")
                t_press = time.monotonic()
            t_release = t_press + hold_ms / 1000.0
            hold = capture_series(sess, t_press,
                                  [("hold_mid", 0.45), ("hold_end", 0.75)])
            post = capture_series(sess, t_release,
                                  [("rel050", 0.05), ("rel100", 0.10),
                                   ("rel200", 0.20), ("rel300", 0.30),
                                   ("rel400", 0.40), ("rel600", 0.60),
                                   ("rel800", 0.80), ("rel1000", 1.00)])
            w_ev = watch.stop()
            res["phases"]["window"] = {
                "t_press": t_press, "t_release": t_release,
                "events": w_ev, "frames": hold + post,
            }
            if hold:
                htrace, _, _ = analyse(hold, idle[-1] if idle else hold[0],
                                        t_press, gate)
                res["phases"]["window"]["hold_trace"] = htrace
            trace, stop, stop_old = analyse(post, hold[-1] if hold else
                                            (idle[-1] if idle else post[0]),
                                            t_release, gate)
            res["phases"]["window"]["trace"] = trace
            res["phases"]["window"]["stop"] = stop
            res["phases"]["window"]["stop_old"] = stop_old

            # recenter between phases so the short tap still has open floor
            if sess.sendkey("s", hold_ms) is not None:
                time.sleep(hold_ms / 1000.0 + 1.4)

            # --- phase 2: window path, 80 ms tap -----------------------------
            watch.arm()
            t_tap = sess.sendkey("w", tap_ms)
            if t_tap is None:
                print("[WARN] HMP sendkey failed for short tap")
                t_tap = time.monotonic()
            t_tap_rel = t_tap + tap_ms / 1000.0
            tap = capture_series(sess, t_tap,
                                 [("tap_hold", 0.04), ("tap100", 0.10),
                                  ("tap200", 0.20), ("tap400", 0.40),
                                  ("tap800", 0.80), ("tap1200", 1.20),
                                  ("tap1600", 1.60)])
            tap_ev = watch.stop()
            trace, stop, stop_old = analyse(tap, tap[0] if tap else idle[-1],
                                            t_tap_rel, gate)
            res["phases"]["tap"] = {
                "t_press": t_tap, "t_release": t_tap_rel,
                "events": tap_ev, "frames": tap, "trace": trace, "stop": stop,
                "stop_old": stop_old,
            }

            if sess.sendkey("s", tap_ms) is not None:
                time.sleep(1.2)

            # --- phase 3: raw serial pipe (no break byte at all) -------------
            watch.arm()
            t_ser = time.monotonic()
            base.write_keys(vm, b"w", "serial w (no release byte)")
            ser = capture_series(sess, t_ser,
                                 [("ser000", 0.0), ("ser400", 0.40),
                                  ("ser800", 0.80), ("ser1200", 1.20),
                                  ("ser1600", 1.60), ("ser2000", 2.00),
                                  ("ser2400", 2.40), ("ser2800", 2.80)])
            ser_ev = watch.stop()
            s_rel_ev = next((t for t, k, _ in ser_ev
                             if k in ("RELEASE", "BREAK", "RELEASED", "KEYUP")),
                            None)
            trace, stop, stop_old = analyse(ser, ser[0] if ser else idle[-1],
                                            s_rel_ev if s_rel_ev else t_ser + 10,
                                            gate)
            res["phases"]["serial"] = {
                "t_press": t_ser, "events": ser_ev, "frames": ser,
                "trace": trace, "stop": stop, "stop_old": stop_old,
            }

            for name, ok in sess_shot_status(sess):
                res["shots"][name] = ok
            # informational clean exit
            base.write_keys(vm, b"\x1b", "Esc/quit")
            base.wait_for_bytes(log_path, [b"DOOM: DONE"], 8)
            res["elapsed"] = time.monotonic() - t0
        finally:
            if sess is not None:
                sess.close()
            try:
                vm.terminate()
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return res


def sess_shot_status(sess):
    return [(n, os.path.isfile(p) and os.path.getsize(p) > 1000)
            for n, p in SHOT.items()]


# ------------------------------------------------------------------ verdict
def check(raw, stats):
    ok = True
    phases = stats["phases"]
    baseline = stats.get("center_baseline", -1)
    gate = stats.get("center_gate", -1)
    print("[INFO] PLAYER-MOTION gate: center region %s, idle baseline=%d changed px, "
          "gate=%d px (<=2x idle, %d consecutive samples)" %
          (CENTER_REGION, baseline, gate, CENTER_CONSECUTIVE))

    def row(label, good):
        nonlocal ok
        ok &= bool(good)
        print("  [%s] %s" % ("PASS" if good else "FAIL", label))

    def lat(stop, ref):
        if stop and ref:
            return stop - ref
        return None

    def show_trace(trace, label):
        print("[INFO]   %s: frame | from_* | OLD pct/mad | NEW center count/pct/mad | idle?" %
              label)
        for d in trace:
            print("[INFO]     %-10s %+7.3fs | %7.3f%% %6.2f | %4d %7.3f%% %6.2f | %s" %
                  (d["name"], d["rel"], d.get("pct", -1), d.get("mad", -1),
                   d.get("ccount", -1), d.get("cpct", -1), d.get("cmad", -1),
                   "CENTER-IDLE" if d.get("cidle") else "-"))

    def phase(label, key, budget, rel_from):
        """Report both metrics, gate on the NEW center-region stop time."""
        p = phases.get(key, {})
        ev = p.get("events", [])
        ref = p.get(rel_from)
        new_lat = lat(p.get("stop"), ref)
        old_lat = lat(p.get("stop_old"), ref)
        print("[INFO] %s markers: %s" % (label, fmt_events(ev, p.get("t_press"))))
        print("[INFO] %s release->stop: OLD(whole)=%s  NEW(center)=%s  "
              "(target <=%.3fs)" %
              (label, ("%.3fs" % old_lat if old_lat is not None else "N/A"),
               ("%.3fs" % new_lat if new_lat is not None else "N/A"), budget))
        show_trace(p.get("trace", []), label)
        return p, ev, new_lat

    win, ev, w_lat = phase("window", "window", 0.400, "t_release")
    print("[INFO] window BREAK marker (genuine PS/2 key-up): %s" %
          ("present" if any(k == "BREAK" for _, k, _ in ev) else "ABSENT"))
    row("window: RELEASE/BREAK marker present", has_release(ev))
    row("window: PLAYER MOTION stops within 400ms (center region, 3 samples)",
        w_lat is not None and 0 <= w_lat <= 0.40)

    tap, tev, t_lat = phase("short tap", "tap", 0.250, "t_release")
    row("short tap: RELEASE/BREAK marker present", has_release(tev))
    row("short tap: PLAYER MOTION stops within 250ms (center region, 3 samples)",
        t_lat is not None and 0 <= t_lat <= 0.25)

    ser = phases.get("serial", {})
    sev = ser.get("events", [])
    s_press = ser.get("t_press")
    s_marker = next((t for t, k, _ in sev
                     if k in ("RELEASE", "BREAK", "RELEASED", "KEYUP")), None)
    s_stop = ser.get("stop")
    s_stop_old = ser.get("stop_old")
    print("[INFO] serial markers: %s" % fmt_events(sev, s_press))
    if s_marker and s_press:
        print("[INFO] serial KEY->RELEASE = %.3fs; marker->stop OLD=%s NEW=%s" %
              (s_marker - s_press,
               ("%.3fs" % (s_stop_old - s_marker)) if s_stop_old else "N/A",
               ("%.3fs" % (s_stop - s_marker)) if s_stop else "N/A"))
    else:
        print("[INFO] serial KEY->RELEASE = N/A (no release marker)")
    show_trace(ser.get("trace", []), "serial")
    row("serial: RELEASE marker present", has_release(sev))

    text = raw.decode("utf-8", "replace")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    row("no Exc/Panic/FAULT/#PF", not exc)
    for ln in exc[:5]:
        print("       " + ln[:160])
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-build-iso", action="store_true")
    ap.add_argument("--log", default=LOG)
    ap.add_argument("--hold-ms", type=int, default=800)
    ap.add_argument("--tap-ms", type=int, default=80)
    args = ap.parse_args()

    if base.check_assets() is None:
        print("\nRESULT: SKIP")
        return 0
    base.check_tools()
    os.makedirs(TMP, exist_ok=True)
    if not (args.no_build_iso and os.path.isfile(ISO)):
        base.build_iso()

    stats = run_qemu(args.log, args.hold_ms, args.tap_ms)
    raw = b""
    try:
        with open(args.log, "rb") as f:
            raw = f.read()
    except OSError:
        pass
    print("\n== results (serial log: %s)" % args.log)
    if not stats["booted"] or not stats["started"]:
        print("RESULT: FAIL (guest did not boot / reach E1M1)")
        return 1
    ok = check(raw, stats)
    print("[INFO] screenshots: " + ", ".join(
        "%s=%s" % (n, "OK" if stats["shots"][n] else "missing")
        for n in SHOT_NAMES))
    print("[INFO] boot+probes %.1fs" % stats["elapsed"])
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
