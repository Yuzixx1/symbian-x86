#!/usr/bin/env python3
"""r1_doom_stuck_test.py -- stress DOOM key release to reproduce the
intermittent "key gets stuck" bug the single-press release test cannot catch.

Boots the GRUB ISO, warps into E1M1, then runs five input-stress scenarios
(rapid taps, overlapping keys, overlay-toggle while held, long hold, raw
serial break), each repeated LOOPS times.  A background thread timestamps
DOOM's KEY/HOLD/RELEASE/BREAK serial markers while a held-open QMP session
takes a screendump burst spanning the final release.

Dedup: doom_marker() emits every marker TWICE -- an immediate EUSER ordinal-915
copy and a ~0.8-1.2 s delayed batched DebugPrint copy.  attribute_events()
claims the marker nearest each scheduled press/release as the immediate copy;
unclaimed events matching a delayed-copy lag are discarded, and any leftover
movement KEY/HOLD after the final release is reported as a re-assert.

Verdict per loop:
  * every KEY marker has a RELEASE/BREAK of the same key within 250 ms,
  * no KEY/HOLD marker re-asserts >150 ms after the final release,
  * center-region player motion decays to idle within 400 ms (best effort:
    INCONCLUSIVE when the hold produced no motion, e.g. against a wall),
  * no Exc/Panic/FAULT/#PF in the serial log.

Any violated check logs the full marker trace and preserves the screendumps
under TMP/stuck_FAIL_*.  Screendumps are best-effort and never fail the test.
Missing DOOM assets (or no QMP) => RESULT: SKIP, exit 0.

Usage: python3 tools/r1_doom_stuck_test.py [--no-build-iso] [--log PATH]
                                           [--loops 3]
"""
import argparse
import functools
import os
import re
import shutil
import subprocess
import sys
import threading
import time

import numpy as np  # noqa: F401  (required by r1_doom_release_test import)

import r1_doom_test as base
import r1_doom_audio_test as audio
import r1_doom_release_test as rel

TMP = base.TMP
LOG = TMP + "/r1_doom_stuck.log"
QMP = TMP + "/r1_doom_stuck-qmp.sock"
ISO = base.ISO
BOOT_TIMEOUT = 85                 # GRUB + EKA2 init under host load
QEMU_CAP = 150                    # hard outer cap (~40 s boot + ~90 s probes)
EXC_KEYS = base.EXC_KEYS
LOOPS = 3                         # task: 3-5 iterations of every scenario
RELEASE_BUDGET = 0.40             # center-motion decay window (seconds)
MATCH_WINDOW = 0.25               # KEY -> RELEASE/BREAK pairing window
REASSERT_AFTER = 0.15             # ignore hold-tail HOLD markers before this
ATTR_WINDOW = 0.26                # how far after an action its marker may land
FALLBACK_LO, FALLBACK_HI = 0.55, 1.65   # expected delayed-copy lag (seconds)
DRAIN = 1.35                      # wait for delayed marker copies to drain

MARK_RE = re.compile(r"DOOM:\s*(KEY|HOLD|RELEASE|BREAK|RELEASED|KEYUP)\s+(\d+)",
                     re.I)
REL_KINDS = ("RELEASE", "BREAK", "RELEASED", "KEYUP")
PRESS_KINDS = ("KEY", "HOLD")
# Only the six "holdable" keys use the latch/release state machine.  Other
# keys (Tab/Enter/Esc) are one-shot press+release events with no BREAK marker,
# so they must not be judged by the KEY->RELEASE pairing rule.  Values are the
# DOOM keycodes printed by the markers (ports/doom/doomkeys.h).
HOLD_KEYS = {"172", "173", "174", "175", "162", "163"}   # a,w,d,s,use,fire
BURST_OFFSETS = (0.05, 0.11, 0.18, 0.25, 0.33, 0.41)
CENTER_CONSECUTIVE = rel.CENTER_CONSECUTIVE   # 3 samples, same as release test


# ------------------------------------------------------------------ helpers
def shot_path(tag, name):
    return TMP + "/stuck_%s_%s.png" % (tag, name)


def sleep_until(t):
    now = time.monotonic()
    if t > now:
        time.sleep(t - now)


class StressMarkerWatch:
    """Poll the growing serial log every 6 ms; timestamp every DOOM marker.

    Unlike the release test's first-occurrence dedupe, this keeps ALL copies
    (immediate + delayed fallback); attribute_events() sorts that out later so
    repeated taps in one scenario are not collapsed into a single event.
    """

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
                self.events.append((time.monotonic(), m.group(1).upper(),
                                    m.group(2)))

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
            time.sleep(0.006)

    def stop(self):
        if self.thread is not None:
            self._stop.set()
            self.thread.join(timeout=1.0)
            self.thread = None
        return list(self.events)


def attribute_events(events, actions):
    """Dedupe by attributing each marker to the scheduled input action that
    caused it.

    doom_marker() emits EVERY marker twice: an immediate EUSER ordinal-915 copy
    and a ~0.8-1.2 s delayed batched DebugPrint copy.  A pure time-window
    dedupe fails on dense streams (10 taps 100 ms apart) because the previous
    same-key event is always newer than the fallback delay.  Instead, every
    scheduled press/release action claims the nearest marker of the expected
    kind within ATTR_WINDOW; that is the immediate copy.  Unclaimed markers are
    either delayed fallbacks (dropped) or a genuine un-scheduled re-assert
    (kept).  Returns (immediate, unattributed) where immediate is a list of
    (action_index, (t, kind, key)).
    """
    want = {"press": PRESS_KINDS, "release": REL_KINDS}
    immediate = []
    used = set()
    for ai in sorted(range(len(actions)), key=lambda i: actions[i][0]):
        at, role, _, _ = actions[ai]
        kinds = want.get(role, ())
        best = None
        for ei, ev in enumerate(events):
            if ei in used or ev[1] not in kinds:
                continue
            dt = ev[0] - at
            if -0.04 <= dt <= ATTR_WINDOW:
                if best is None or abs(dt) < abs(best[1][0] - at):
                    best = (ei, ev)
        if best is not None:
            used.add(best[0])
            immediate.append((ai, best[1]))
    # Unclaimed events: drop those explainable as a delayed copy of an
    # attributed marker; keep anything else (a possible re-assert).
    fallbacks = []
    unattributed = []
    bases = [ev for _, ev in immediate]
    for ei, ev in enumerate(events):
        if ei in used:
            continue
        t, kind, key = ev
        if any(bk == kind and bkey == key and FALLBACK_LO <= t - bt <= FALLBACK_HI
               for bt, bk, bkey in bases):
            fallbacks.append(ev)
        else:
            unattributed.append(ev)
    return immediate, unattributed, fallbacks


def immediate_traces(immediate, actions):
    """Flatten attributed markers into a chronological event list for logging."""
    return [ev for _, ev in sorted(immediate, key=lambda x: x[1][0])]


class Ctx:
    """Shared harness state: QMP/HMP session, guest stdin, scheduled actions."""

    def __init__(self, sess, vm):
        self.sess = sess
        self.vm = vm
        self.actions = []            # (t, role, key, label) -- for trace only

    def press(self, key, hold_ms, label=""):
        """HMP sendkey: press now, QEMU schedules the break after hold_ms."""
        t0 = None
        if self.sess is not None:
            t0 = self.sess.sendkey(key, hold_ms)
        if t0 is None:
            t0 = time.monotonic()
        tr = t0 + hold_ms / 1000.0
        self.actions.append((t0, "press", key, label))
        self.actions.append((tr, "release", key, label))
        return t0, tr

    def raw(self, data, role, key, label=""):
        """Feed a raw byte to the guest serial console (no HMP break)."""
        t = time.monotonic()
        base.write_keys(self.vm, data, label)
        self.actions.append((t, role, key, label))
        return t

    def grab(self, tag, name):
        """Best-effort screendump; return (path, monotonic_t) or None."""
        if self.sess is None:
            return None
        p = shot_path(tag, name)
        t = self.sess.screendump(p)
        return (p, t) if t is not None else None


# ------------------------------------------------------------------ scenarios
# Each returns (t_last_release, pre_path_or_None, t_motion_ref).
# t_last_release is the final key-up we assert marker hygiene from;
# t_motion_ref is when the center-motion burst starts (same except overlays).

def sc_rapid(ctx, tag):
    """(a) 10 quick 50 ms w taps; final release is the 10th break."""
    taps, tap_ms, gap = 10, 50, 0.05
    last_rel, pre = None, None
    for i in range(taps):
        _, last_rel = ctx.press("w", tap_ms, "tap%d" % (i + 1))
        if i == taps - 1:
            sleep_until(last_rel - tap_ms / 1000.0 + 0.020)
            g = ctx.grab(tag, "hold")
            pre = g[0] if g else None
        sleep_until(last_rel + gap)
    return last_rel, pre, last_rel


def sc_overlap(ctx, tag, reverse=False):
    """(b) hold w / tap d, then hold d / tap w (reverse in a later loop)."""
    hold, inner = ("d", "w") if reverse else ("w", "d")
    t0, last_rel = ctx.press(hold, 700, "hold-" + hold)
    sleep_until(t0 + 0.15)
    ctx.press(inner, 80, "inner-" + inner)      # inside the hold
    sleep_until(t0 + 0.45)
    g = ctx.grab(tag, "hold")
    pre = g[0] if g else None
    sleep_until(last_rel)
    return last_rel, pre, last_rel


def sc_menu(ctx, tag):
    """(c) hold w, toggle automap (Tab) while held, release w under the
    overlay, close it, then check motion in the restored game view.

    NOTE: vanilla Esc opens DOOM's menu, but this port binds Esc to QUIT
    (e32_platform.c sets sQuitRequested), so the overlay-toggle intent is
    exercised with the automap (key_map_toggle == KEY_TAB) instead.
    """
    t0, w_rel = ctx.press("w", 600, "held-w")
    sleep_until(t0 + 0.10)
    ctx.press("tab", 50, "overlay-on")           # automap ON
    sleep_until(t0 + 0.70)
    ctx.press("tab", 50, "overlay-off")          # automap OFF (w already up)
    sleep_until(t0 + 0.78)
    g = ctx.grab(tag, "hold")
    pre = g[0] if g else None
    return w_rel, pre, t0 + 0.78


def sc_longhold(ctx, tag):
    """(d) 2 s hold -- longer than the 900 ms typematic / 200 ms fallback."""
    _, last_rel = ctx.press("w", 2000, "long")
    sleep_until(last_rel - 1.40)                 # ~600 ms into the hold
    g = ctx.grab(tag, "hold")
    pre = g[0] if g else None
    sleep_until(last_rel)
    return last_rel, pre, last_rel


def sc_serial(ctx, tag):
    """(e) 10x raw 'w' make, 100 ms later raw 0xF7 (= 0x80|'w') break."""
    last_rel, pre = None, None
    for i in range(10):
        t_make = ctx.raw(b"w", "press", "w", "raw-make%d" % (i + 1))
        if i == 9:
            sleep_until(t_make + 0.06)
            g = ctx.grab(tag, "hold")
            pre = g[0] if g else None
            sleep_until(t_make + 0.10)
        else:
            sleep_until(t_make + 0.10)
        last_rel = ctx.raw(b"\xf7", "release", "w", "raw-break%d" % (i + 1))
        if i < 9:
            sleep_until(last_rel + 0.05)
    return last_rel, pre, last_rel


SCENARIOS = (
    ("rapid", sc_rapid),
    ("overlap", functools.partial(sc_overlap, reverse=False)),
    ("overlap_rev", functools.partial(sc_overlap, reverse=True)),
    ("menu", sc_menu),
    ("longhold", sc_longhold),
    ("serial", sc_serial),
)


# ------------------------------------------------------------------ analysis
def motion_probe(ctx, tag, t_ref, gate, pre):
    """Screendump burst after t_ref; return (trace, frames, verdict, stop)."""
    frames = []
    for i, off in enumerate(BURST_OFFSETS):
        sleep_until(t_ref + off)
        g = ctx.grab(tag, "b%d" % i)
        if g is None:
            continue
        path, t = g
        frames.append({"t": t, "rel": t - t_ref, "path": path,
                       "name": "b%d" % i})
    chain = []
    if pre and os.path.isfile(pre):
        chain.append({"t": t_ref, "path": pre, "name": "hold"})
    chain += frames
    trace = []
    for a, b in zip(chain, chain[1:]):
        d = rel.center_frame_diff(a["path"], b["path"])
        d["at"] = b["t"]
        d["rel"] = round(b["rel"], 3)
        d["name"] = b["name"]
        c = d.get("count", -1)
        d["idle"] = c >= 0 and c <= gate
        trace.append(d)

    counts = [d.get("count", -1) for d in trace]
    good = [c for c in counts if c >= 0]
    stop = None
    for i, d in enumerate(trace):
        w = trace[i:i + CENTER_CONSECUTIVE]
        if len(w) == CENTER_CONSECUTIVE and all(x.get("idle") for x in w):
            stop = round(d["rel"], 3)
            break
    if not good:
        return trace, frames, "INCONCLUSIVE(no frames)", stop
    if not any(c > gate for c in good):
        return trace, frames, "INCONCLUSIVE(no motion in hold)", stop
    if stop is None:
        return trace, frames, "FAIL(motion never stopped)", stop
    if stop > RELEASE_BUDGET + 0.02:
        return trace, frames, "FAIL(stopped late %.3fs)" % stop, stop
    return trace, frames, "PASS", stop


def marker_check(immediate, unattributed, actions, t_last_rel):
    """Return (stuck_list, notes) from attributed marker events."""
    stuck = []
    notes = []
    rels = [(ev, actions[ai]) for ai, ev in immediate
            if ev[1] in REL_KINDS and ev[2] in HOLD_KEYS]
    # (1) every emitted holdable KEY must have a RELEASE/BREAK of the same key
    #     within 250 ms (one-shot keys such as Tab have no break by design)
    for _, ev in immediate:
        kt, kind, key = ev
        if kind != "KEY" or key not in HOLD_KEYS:
            continue
        matches = [r[0][0] for r in rels if r[0][2] == key
                   and 0 <= r[0][0] - kt <= MATCH_WINDOW]
        notes.append("KEY %s @%+.3f -> %s" % (
            key, kt - t_last_rel,
            ("release @%+.3f" % (matches[0] - t_last_rel))
            if matches else "NO RELEASE"))
        if not matches:
            stuck.append("KEY %s without RELEASE/BREAK within %.0f ms" %
                         (key, MATCH_WINDOW * 1000))
    # (2) no unscheduled movement KEY/HOLD re-assert after the final release
    late = [e for e in unattributed if e[1] in PRESS_KINDS
            and e[2] in HOLD_KEYS and e[0] > t_last_rel + REASSERT_AFTER]
    if late:
        stuck.append("re-assert after final release: " +
                     ", ".join("%s %s @%+.3f" % (k, key, t - t_last_rel)
                               for t, k, key in late))
    return stuck, notes


def fmt_events(events, t0):
    names = {"KEY": "K", "HOLD": "H", "RELEASE": "R", "BREAK": "B",
             "RELEASED": "R", "KEYUP": "B"}
    return " ".join("%s@%+.3f/%s" % (names.get(k, k[0]), t - t0, key)
                    for t, k, key in events) if events else "(none)"


def preserve(tag, frames, pre):
    """On failure, copy the scenario's PNGs to stable FAIL names."""
    kept = []
    paths = ([pre] if pre and os.path.isfile(pre) else [])
    paths += [f["path"] for f in frames]
    for p in paths:
        if not os.path.isfile(p):
            continue
        dst = p.replace("/stuck_", "/stuck_FAIL_")
        try:
            shutil.copyfile(p, dst)
            kept.append(os.path.basename(dst))
        except OSError:
            pass
    return kept


# ------------------------------------------------------------------ the run
def run_qemu(log_path, loops):
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
    res = {"booted": False, "started": False, "results": [], "elapsed": 0.0}
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
            time.sleep(2.5)                      # E1M1 paints + input ring drains

            sess = rel.ShotSession(QMP)
            if sess.stream is None:
                print("[WARN] QMP session unavailable; cannot run HMP scenarios")
                return res
            ctx = Ctx(sess, vm)

            # --- idle baseline: last pair settles, min keeps the gate sensitive
            idle = []
            for i in range(4):
                g = ctx.grab("idle", "i%d" % i)
                if g:
                    idle.append(g)
                time.sleep(0.25)
            idle_counts = []
            for a, b in zip(idle, idle[1:]):
                d = rel.center_frame_diff(a[0], b[0])
                idle_counts.append(d.get("count", -1))
            good = [c for c in idle_counts if c >= 0]
            baseline = min(good) if good else 0
            gate = max(rel.CENTER_IDLE_FACTOR * baseline, rel.CENTER_MIN_COUNT)
            res["gate"] = gate
            res["baseline"] = baseline
            print("[INFO] center idle counts=%s baseline=%d gate=%d px" %
                  (idle_counts, baseline, gate))

            for si, (sname, fn) in enumerate(SCENARIOS):
                for loop in range(loops):
                    tag = "%s_%d" % (sname, loop + 1)
                    label = sname
                    action_start = len(ctx.actions)
                    watch = StressMarkerWatch(log_path)
                    watch.arm()
                    t_rel, pre, t_mref = fn(ctx, tag)
                    # capture the burst while the watch is still armed so any
                    # re-assert during observation is recorded as a marker too
                    trace, frames, mv, stop = motion_probe(ctx, tag, t_mref,
                                                           gate, pre)
                    time.sleep(0.10)
                    raw_events = watch.stop()
                    actions = ctx.actions[action_start:]
                    immediate, unattributed, fallbacks = attribute_events(
                        raw_events, actions)
                    dedup = immediate_traces(immediate, actions)
                    stuck, notes = marker_check(immediate, unattributed,
                                                actions, t_rel)
                    if mv.startswith("FAIL"):
                        stuck.append(mv)

                    result = {
                        "tag": tag, "label": label, "raw": raw_events,
                        "dedup": dedup, "notes": notes, "stuck": stuck,
                        "trace": trace, "stop": stop, "mv": mv,
                        "pre": pre, "frames": frames, "t_rel": t_rel,
                        "unattributed": unattributed,
                    }
                    res["results"].append(result)
                    status = "STUCK" if stuck else ("PASS" if mv == "PASS"
                                                   else "ok")
                    print("[%s] %-16s (raw=%d kept=%d fb=%d unattr=%d) "
                          "markers=%s" %
                          (status, tag, len(raw_events), len(dedup),
                           len(fallbacks), len(unattributed),
                           fmt_events(dedup, t_rel)))
                    print("       center: %s (stop=%s)  KEY pairs: %s" %
                          (mv, stop, "; ".join(notes) if notes else "(none)"))
                    if stuck:
                        print("       STUCK -> %s" % " | ".join(stuck))
                        kept = preserve(tag, frames, pre)
                        print("       preserved PNGs: %s" %
                              (", ".join(kept) if kept else "(none)"))
                    # small inter-loop gap; attribute_events filters any
                    # delayed fallback copies from the previous loop
                    if loop + 1 < loops:
                        time.sleep(0.4)

                # between scenarios: drain delayed copies, recenter to the
                # E1M1 start so the center-region wall edge stays a valid
                # motion probe, then drain the recenter's own markers so they
                # cannot poison the next scenario's watch.
                if si + 1 < len(SCENARIOS):
                    time.sleep(DRAIN)
                    ctx.press("s", 400, "recenter")
                    time.sleep(0.45)
                    time.sleep(1.0)

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


# ------------------------------------------------------------------ verdict
def check(raw, stats):
    ok = True
    text = raw.decode("utf-8", "replace")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    print("\n== results ==")
    print("[INFO] center gate: baseline=%d gate=%d px, %d consecutive samples" %
          (stats.get("baseline", -1), stats.get("gate", -1), CENTER_CONSECUTIVE))
    nstuck = 0
    for r in stats["results"]:
        if r["stuck"]:
            nstuck += 1
            print("  [STUCK] %s: %s" % (r["tag"], " | ".join(r["stuck"])))
            print("          full marker trace: %s" %
                  fmt_events(r["raw"], r["t_rel"]))
            print("          deduped trace:      %s" %
                  fmt_events(r["dedup"], r["t_rel"]))
            for d in r["trace"]:
                print("          frame %-6s +%.3fs ccount=%s idle=%s" %
                      (d["name"], d["rel"], d.get("count", -1), d.get("idle")))
    if exc:
        ok = False
        print("  [FAIL] Exc/Panic/FAULT/#PF detected: %d line(s)" % len(exc))
        for ln in exc[:5]:
            print("         " + ln[:160])
    if nstuck:
        ok = False
        print("  [FAIL] %d scenario loop(s) reproduced a stuck key" % nstuck)
    else:
        print("  [PASS] no scenario reproduced a stuck key (%d loops)" %
              len(stats["results"]))
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-build-iso", action="store_true")
    ap.add_argument("--log", default=LOG)
    ap.add_argument("--loops", type=int, default=LOOPS)
    args = ap.parse_args()

    if base.check_assets() is None:
        print("\nRESULT: SKIP")
        return 0
    base.check_tools()
    os.makedirs(TMP, exist_ok=True)
    if not (args.no_build_iso and os.path.isfile(ISO)):
        base.build_iso()

    stats = run_qemu(args.log, args.loops)
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
    if not stats["results"]:
        print("RESULT: SKIP (no scenarios run; QMP unavailable)")
        return 0
    ok = check(raw, stats)
    print("[INFO] boot+probes %.1fs" % stats["elapsed"])
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
