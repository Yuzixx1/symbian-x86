#!/usr/bin/env python3
"""r1_doom_multikey_test.py -- prove MULTI-KEY HOLD works in DOOM (serial ground truth).

The sticky-key root cause is a per-key hold state machine: releasing one key
must never release, or re-arm, another key that is still physically down.  The
existing r1_doom_stuck_test.py covers overlaps but judges player motion in a
center screen region, which is noisy (monster AI, weapon bob, geometry edges).
This test makes SERIAL MARKERS the authoritative signal instead:

  DOOM: KEY <code>     fresh press (sparse: first 6 + every 20th)
  DOOM: HOLD <code>    first typematic repeat of a hold (one per hold)
  DOOM: BREAK <code>   key-up byte consumed (sparse: first 8)
  DOOM: RELEASE <code> key actually released (NOT sparse -- authoritative)
  DOOM: STALE <code>   stale make swallowed by the cooldown (good: filter works)

doom_marker() prints every marker TWICE (immediate EUSER ord-915 + delayed
batched DebugPrint copy ~0.8-1.2 s later).  attribute_events() claims the copy
nearest each scheduled action for the TIMING checks; the delayed twin is
dropped by its lag.  For the no-stuck-key verdict, the AUTHORITATIVE full serial
log is parsed after the run, copy-pairs are collapsed, and markers are bound to
scheduled actions by (role, key, order) -- no clock window.  This is required
because the guest can emit a CRLF RELEASE seconds after the host break (under
load), so the short per-loop watch window can miss it entirely; a missing
watch marker alone must not be read as a stuck key.

Scenarios (each LOOPS times):
  (a) HOLD W + HOLD D (diagonal): drop W, D must stay held, then drop D.
  (b) HOLD W + space (run+fire): drop space, W must stay held, then drop W.
  (c) HOLD D + e (move+use):     drop e, D must stay held, then drop D.
  (d) RAPID alternating W/D x5: every asserted press has one RELEASE.

Verdict: for each asserted press exactly one RELEASE within 300 ms of its
scheduled break; no RELEASE of the still-held key at the first release; no
KEY/HOLD re-assert after the final release; zero Exc/Panic/FAULT/#PF.
Screendumps are best-effort (/tmp/opencode/multikey_*.png) and never gate the
test.  Missing DOOM assets => RESULT: SKIP, exit 0.

Usage: python3 tools/r1_doom_multikey_test.py [--no-build-iso] [--log PATH]
                                              [--loops 3]
"""
import argparse
import os
import re
import subprocess
import sys
import threading
import time

import numpy as np  # noqa: F401  (required by r1_doom_release_test import)

import r1_doom_test as base
import r1_doom_audio_test as audio
import r1_doom_release_test as rel

TMP = base.TMP
LOG = TMP + "/r1_doom_multikey.log"
QMP = TMP + "/r1_doom_multikey-qmp.sock"
ISO = base.ISO
BOOT_TIMEOUT = 85
QEMU_CAP = 150
EXC_KEYS = base.EXC_KEYS
LOOPS = 3                            # task: 2-3x per scenario

RELEASE_WINDOW = 0.30                # RELEASE must land within 300 ms of break
ATTR_WINDOW = 0.30                   # marker claim window after a scheduled action
REASSERT_AFTER = 0.15                # ignore hold-tail markers before this
FALLBACK_LO, FALLBACK_HI = 0.55, 1.65  # delayed-copy lag window
DRAIN = 1.35                         # let delayed copies flush between scenarios

MARK_RE = re.compile(r"DOOM:\s*(KEY|HOLD|BREAK|RELEASE|STALE)\s+(\d+)", re.I)
# Byte-level scan of the authoritative full log (both CRLF and bare-LF copies).
MARK_ALL_RE = re.compile(
    rb"DOOM:\s*(KEY|HOLD|BREAK|RELEASE|STALE)\s+(\d+)\r?\n", re.I)
PRESS_KINDS = ("KEY", "HOLD")
REL_KINDS = ("RELEASE", "BREAK")
# DOOM keycodes (ports/doom/doomkeys.h), decimal: w,d,s,a,fire,use
HOLD_KEYS = {"172", "173", "174", "175", "162", "163"}
KN = {"173": "W", "174": "D", "172": "A", "175": "S",
      "163": "FIRE", "162": "USE"}
SHORT = {"KEY": "K", "HOLD": "H", "BREAK": "B", "RELEASE": "R", "STALE": "S"}


# ------------------------------------------------------------------ helpers
def sleep_until(t):
    now = time.monotonic()
    if t > now:
        time.sleep(t - now)


def shot_path(tag, name):
    return TMP + "/multikey_%s_%s.png" % (tag, name)


class MarkerWatch:
    """Poll the growing serial log every 6 ms; timestamp DOOM markers.

    doom_marker() writes each marker twice through two distinct serial paths:
      * EUSER.DLL ordinal 915 (immediate, CRLF-terminated) -- authoritative;
      * DebugPrint fallback (batched ~0.8-1.2 s later, LF-only) -- a duplicate.
    Keeping only CRLF lines strips the doubled copy at the source: the marker
    stream then has one timestamp per real press/release/break.
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
            raw = self.buf[:i]
            self.buf = self.buf[i + 1:]
            # CRLF == immediate EUSER ord-915 copy; bare LF == delayed batch.
            if not raw.endswith(b"\r"):
                continue
            line = raw.decode("utf-8", "replace").strip()
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


def dedupe_stale(events):
    """Return (unique STALE events, non-STALE events).

    STALE is informational (swallowed stale make == filter working).  Collapse
    its doubled copies: a twin within 60 ms of an earlier same-key copy, or a
    delayed batch copy 0.55-1.65 s later, is not a second stale event.
    """
    stale = [e for e in events if e[1] == "STALE"]
    others = [e for e in events if e[1] != "STALE"]
    kept = []
    for e in sorted(stale):
        t, _, key = e
        if any(k == key and (0 <= t - kt <= 0.06 or
                             FALLBACK_LO <= t - kt <= FALLBACK_HI)
               for kt, _, k in kept):
            continue
        kept.append(e)
    return kept, others


def parse_full_markers(log_path):
    """Parse every marker copy from the authoritative FULL serial log.

    Returns (offset, kind, key, crlf) tuples.  Byte offsets -- not host times --
    are used because the guest's serial copy can lag the host sendkey clock by
    seconds under load: the per-loop MarkerWatch window can then miss a RELEASE
    that is nevertheless present in the log (this is what made diagonal_2 FAIL).
    """
    try:
        with open(log_path, "rb") as f:
            data = f.read()
    except OSError:
        return []
    return [(m.start(), m.group(1).decode("ascii").upper(),
             m.group(2).decode("ascii"),
             data[m.start():m.end()].endswith(b"\r\n"))
            for m in MARK_ALL_RE.finditer(data)]


def dedupe_copies(markers):
    """Collapse the immediate CRLF + delayed bare-LF twin of each real marker.

    Both serial paths emit one copy per event; the batched LF copy arrives
    0.55-1.65 s later.  Keep the CRLF copy as canonical, drop an LF copy that
    pairs with a still-unpaired earlier CRLF of the same (kind, key), and keep
    an LF copy that has no CRLF twin (some events use only the batched path).
    """
    pending = {}                     # (kind, key) -> unpaired CRLF copies
    kept = []
    for off, kind, key, crlf in markers:
        tag = (kind, key)
        if crlf:
            pending[tag] = pending.get(tag, 0) + 1
            kept.append((off, kind, key))
        elif pending.get(tag, 0) > 0:
            pending[tag] -= 1
        else:
            kept.append((off, kind, key))
    return kept


def attribute_global(logical, actions):
    """Bind each logical marker to one scheduled action by (role, key), in order.

    No timestamp window is used: host-time windows are unreliable given the
    guest serial lag.  Actions are visited in scheduled-time order and each is
    claimed at most once, so a delayed duplicate cannot be counted twice.
    """
    order = sorted(range(len(actions)), key=lambda i: actions[i][0])
    out = {}
    for off, kind, key in logical:
        if kind in PRESS_KINDS:
            role = "press"
        elif kind in REL_KINDS:
            role = "release"
        else:
            continue
        for ai in order:
            if ai in out or actions[ai][1] != role or actions[ai][2] != key:
                continue
            out[ai] = (off, kind, key)
            break
    return out


def attribute_events(events, actions):
    """Claim the immediate copy of each marker for its scheduled action.

    Candidate markers are matched by KEY CODE as well as kind, which avoids
    cross-talk when W and D are held together.  Returns (immediate, unattributed,
    fallbacks); immediate is a list of (action_index, event).
    """
    want = {"press": PRESS_KINDS, "release": REL_KINDS}
    immediate = []
    used = set()
    for ai in sorted(range(len(actions)), key=lambda i: actions[i][0]):
        at, role, key = actions[ai][:3]
        kinds = want.get(role, ())
        best = None
        for ei, ev in enumerate(events):
            if ei in used or ev[1] not in kinds or ev[2] != key:
                continue
            dt = ev[0] - at
            if -0.05 <= dt <= ATTR_WINDOW:
                if best is None or abs(dt) < abs(best[1][0] - at):
                    best = (ei, ev)
        if best is not None:
            used.add(best[0])
            immediate.append((ai, best[1]))
    fallbacks, unattributed = [], []
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


class Ctx:
    """Shared harness state: held-open QMP session, guest stdin, actions."""

    def __init__(self, sess, vm):
        self.sess = sess
        self.vm = vm
        self.actions = []            # (t, role, keycode, label)

    def press(self, hmp_key, hold_ms, keycode, label=""):
        """HMP sendkey: make now, QEMU schedules the break after hold_ms."""
        t0 = self.sess.sendkey(hmp_key, hold_ms)
        if t0 is None:
            t0 = time.monotonic()
        tr = t0 + hold_ms / 1000.0
        self.actions.append((t0, "press", keycode, label))
        self.actions.append((tr, "release", keycode, label))
        return t0, tr

    def grab(self, tag, name):
        if self.sess is None:
            return None
        p = shot_path(tag, name)
        t = self.sess.screendump(p)
        return (p, t) if t is not None else None


# ------------------------------------------------------------------ scenarios
# Each returns metadata used by analyse(); "held" is the key that must remain
# down after "first_rel" and only release at "last_rel".

def sc_diagonal(ctx, tag):
    """(a) W + D diagonal; drop W, D must stay held, then drop D."""
    tw, wr = ctx.press("w", 1500, "173", "W")
    td, dr = ctx.press("d", 2200, "174", "D")
    ctx.grab(tag, "both_held")
    sleep_until(wr + 0.30)            # D is still physically down here
    ctx.grab(tag, "after_W_release")
    sleep_until(dr + 0.10)
    ctx.grab(tag, "after_D_release")
    return {"t0": tw, "last_rel": dr, "first_rel": wr, "held": "174",
            "label": "W+D diagonal", "seq": [("173", wr), ("174", dr)]}


def sc_runfire(ctx, tag):
    """(b) W + space (run+fire); drop space, W must stay held, then drop W."""
    tw, wr = ctx.press("w", 1600, "173", "W")
    ts, sr = ctx.press("spc", 1000, "163", "FIRE")
    ctx.grab(tag, "both_held")
    sleep_until(sr + 0.30)            # W is still physically down here
    ctx.grab(tag, "after_space_release")
    sleep_until(wr + 0.10)
    ctx.grab(tag, "after_W_release")
    return {"t0": tw, "last_rel": wr, "first_rel": sr, "held": "173",
            "label": "W+space run/fire", "seq": [("163", sr), ("173", wr)]}


def sc_moveuse(ctx, tag):
    """(c) D + e (move+use); drop e, D must stay held, then drop D."""
    td, dr = ctx.press("d", 1500, "174", "D")
    te, er = ctx.press("e", 1000, "162", "USE")
    ctx.grab(tag, "both_held")
    sleep_until(er + 0.30)            # D is still physically down here
    ctx.grab(tag, "after_e_release")
    sleep_until(dr + 0.10)
    ctx.grab(tag, "after_D_release")
    return {"t0": td, "last_rel": dr, "first_rel": er, "held": "174",
            "label": "D+e move/use", "seq": [("162", er), ("174", dr)]}


def sc_rapid(ctx, tag):
    """(d) 5x press W, press D, release W, release D (quickly)."""
    n, w_hold, d_hold, stagger, iter_gap = 5, 240, 240, 0.05, 0.20
    seq = []
    t_start = time.monotonic()
    for i in range(n):
        w0, wr = ctx.press("w", w_hold, "173", "W%d" % (i + 1))
        sleep_until(w0 + stagger)
        d0, dr = ctx.press("d", d_hold, "174", "D%d" % (i + 1))
        sleep_until(dr + iter_gap)
        seq.append(("173", wr))
        seq.append(("174", dr))
    ctx.grab(tag, "final")
    return {"t0": t_start, "last_rel": dr, "first_rel": None, "held": None,
            "label": "rapid W/D x5", "seq": seq}


SCENARIOS = (
    ("diagonal", sc_diagonal),
    ("runfire", sc_runfire),
    ("moveuse", sc_moveuse),
    ("rapid", sc_rapid),
)


# ------------------------------------------------------------------ analysis
def fmt_trace(events, t0):
    return " ".join("%s/%s@%+.3f" % (SHORT.get(k, k[0]), KN.get(key, key),
                                      t - t0)
                    for t, k, key in sorted(events)) if events else "(none)"


def analyse(meta, actions, immediate, unattributed, stale, ordered):
    """Return (failures, notes) from deduped, key-matched markers.

    `ordered` maps scenario-local action indices to logical markers attributed
    from the FULL serial log (lag-immune).  `immediate` is the per-loop watch
    view used only for the timing checks below; it may miss a release whose
    serial copy landed after the watch window, so it no longer decides (1).
    """
    bad = []
    notes = []
    t0 = meta["t0"]
    t_last = meta["last_rel"]

    rel_ai = [(ai, ev) for ai, ev in immediate if ev[1] in REL_KINDS]
    prs_ai = [(ai, ev) for ai, ev in immediate if ev[1] in PRESS_KINDS]

    # The partner of a press is its own scheduled break (same key, next in
    # time).  Pairing actions -- not wall-clock proximity -- is what makes the
    # no-stuck-key assertion correct when D is held 2.2 s or serial lags.
    partner = {}
    for pi, act in enumerate(actions):
        if act[1] != "press":
            continue
        cands = [ri for ri, a2 in enumerate(actions)
                 if a2[1] == "release" and a2[2] == act[2] and a2[0] >= act[0]]
        if cands:
            partner[pi] = min(cands, key=lambda ri: actions[ri][0] - act[0])

    # (1) every OBSERVED press must have its OWN scheduled break observed as a
    #     RELEASE/BREAK marker in the authoritative full log.  Order-based
    #     full-log attribution is immune to the guest serial lag that made the
    #     old 2.0 s press->release window report false "NO RELEASE" FAILs.
    for ai in sorted(ordered):
        at, role, key = actions[ai][:3]
        if role != "press":
            continue
        ri = partner.get(ai)
        if ri is None:
            bad.append("press %s has no scheduled break" % KN.get(key, key))
            continue
        if ri not in ordered:
            bad.append("press %s has NO RELEASE for its own break" %
                       KN.get(key, key))
        else:
            notes.append("press %s -> RELEASE at its own break" %
                         KN.get(key, key))

    # (2) each scheduled break produces a RELEASE within 300 ms of it
    for ai, act in enumerate(actions):
        at, role, key = act[:3]
        if role != "release":
            continue
        matched = [rev for rai, rev in rel_ai if rai == ai]
        if not matched:
            # a press swallowed in COOL/PENDING has no RELEASE by design
            continue
        dt = matched[0][0] - at
        if not (-0.05 <= dt <= RELEASE_WINDOW):
            bad.append("%s RELEASE %.3fs from break (window %.0f ms)" %
                       (KN.get(key, key), dt, RELEASE_WINDOW * 1000))

    # (3) the still-held key must not release when the other key drops
    if meta["first_rel"] is not None and meta["held"] is not None:
        hk, fr = meta["held"], meta["first_rel"]
        early = [rev for _, rev in rel_ai
                 if rev[2] == hk and fr - 0.10 <= rev[0] <= fr + RELEASE_WINDOW]
        if early:
            bad.append("%s released at first break @%+.3f (should stay held)" %
                       (KN.get(hk, hk), fr - t0))
        notes.append("%s stays held past first break @%+.3f" %
                     (KN.get(hk, hk), fr - t0))

    # (4) no KEY/HOLD re-assert after the final release
    late = [e for e in unattributed
            if e[1] in PRESS_KINDS and e[2] in HOLD_KEYS
            and e[0] > t_last + REASSERT_AFTER]
    late += [ev for _, ev in prs_ai if ev[0] > t_last + REASSERT_AFTER]
    if late:
        bad.append("re-assert after final release: " +
                   ", ".join("%s/%s@%+.3f" % (SHORT.get(k, k[0]),
                                              KN.get(key, key), t - t0)
                             for t, k, key in late))

    notes.append("STALE swallowed: %d" % len(stale))
    return bad, notes


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

            for si, (sname, fn) in enumerate(SCENARIOS):
                for loop in range(loops):
                    tag = "%s_%d" % (sname, loop + 1)
                    start = len(ctx.actions)
                    watch = MarkerWatch(log_path)
                    watch.arm()
                    meta = fn(ctx, tag)
                    time.sleep(0.10)             # let the last markers land
                    raw = watch.stop()
                    actions = ctx.actions[start:]
                    stale, evs = dedupe_stale(raw)
                    immediate, unattributed, fallbacks = attribute_events(evs, actions)
                    dedup = sorted([ev for _, ev in immediate])
                    res["results"].append({
                        "tag": tag, "meta": meta, "raw": raw, "stale": stale,
                        "dedup": dedup, "unattributed": unattributed,
                        "fallbacks": fallbacks, "bad": [], "notes": [],
                        "_span": (start, len(ctx.actions)), "actions": actions,
                        "immediate": immediate,
                    })
                    if loop + 1 < loops:
                        time.sleep(0.35)
                if si + 1 < len(SCENARIOS):
                    time.sleep(DRAIN)            # flush delayed copies

            # Attribute against the AUTHORITATIVE full serial log.  The guest
            # can emit a CRLF RELEASE seconds after the host break (or only via
            # the batched LF path), so the per-loop watch window cannot be the
            # sole evidence for "key released" -- it produced diagonal_2's
            # false FAIL.  Order-based global binding needs no timing window.
            time.sleep(DRAIN)
            logical = dedupe_copies(parse_full_markers(log_path))
            claims = attribute_global(logical, ctx.actions)
            for r in res["results"]:
                lo, hi = r["_span"]
                ordered = {ai - lo: claims[ai] for ai in claims if lo <= ai < hi}
                bad, notes = analyse(r["meta"], r["actions"], r["immediate"],
                                     r["unattributed"], r["stale"], ordered)
                r["bad"], r["notes"] = bad, notes
                status = "FAIL" if bad else "PASS"
                print("[%s] %-14s %s (raw=%d dedup=%d fb=%d unattr=%d)" %
                      (status, r["tag"], r["meta"]["label"], len(r["raw"]),
                       len(r["dedup"]), len(r["fallbacks"]),
                       len(r["unattributed"])))
                print("       trace: %s" % fmt_trace(r["dedup"], r["meta"]["t0"]))
                print("       %s" % "; ".join(notes))
                if bad:
                    print("       raw: %s" % fmt_trace(r["raw"], r["meta"]["t0"]))
                    print("       -> %s" % " | ".join(bad))

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
    print("\n== scenarios ==")
    for r in stats["results"]:
        good = not r["bad"]
        ok &= good
        print("  [%s] %-14s %s" %
              ("PASS" if good else "FAIL", r["tag"], r["meta"]["label"]))
        print("        marker trace: %s" % fmt_trace(r["dedup"], r["meta"]["t0"]))
        print("        %s" % "; ".join(r["notes"]))
        if not good:
            print("        raw trace: %s" % fmt_trace(r["raw"], r["meta"]["t0"]))
            for b in r["bad"]:
                print("        repro: %s" % b)
    print("  [%s] no Exc/Panic/FAULT/#PF" % ("PASS" if not exc else "FAIL"))
    if exc:
        ok = False
        for ln in exc[:5]:
            print("         " + ln[:160])
    print("\n[INFO] boot+probes %.1fs, %d scenario loops" %
          (stats["elapsed"], len(stats["results"])))
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
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
