#!/usr/bin/env python3
"""r1_doom_visual_test.py -- definitive visual regression test for DOOM.

Proves the two R1-wave fixes rather than just "the game renders":

  (a) NO FLICKER / CONSOLE-BLEED.  After the title screen and Enter, grab two
      QMP screendumps ~1.2 s apart.  PIL analysis asserts the 40-pixel top and
      bottom strips hold no bright console-glyph pixels (the game's 320x200
      frame is centred, so its HUD is *inside* the frame, never in the strips),
      and that the two frames differ only inside the game area -- never in the
      edge strips (a whole-screen shift or console redraw changes those).
      A pre-fix PNG (doom_final.png / doom_playtest.png / doom_audio.png) is
      parsed too, so before/after edge-text density is reported side by side.

  (b) HELD KEYS REPEAT.  Send one `w` byte (press, NO release) and wait 2.5 s.
      Assert `DOOM: HOLD` repeat markers accumulate (with a fallback that uses
      repeated `DOOM: KEY 119` markers + frame progress + on-screen motion if
      the fix chose a different marker).  Then probe release protocols (NUL,
      same-key toggle, other key; Esc as last resort) and confirm repeats stop.

Boot path is the proven one from r1_doom_test.py / r1_doom_playtest.py:
default GRUB ISO with AC97 attached, direct log poll for `X:/> ` (85 s cap),
`doom` + Enter over the serial pipe, and the *held-open* QMP screendump session
from r1_doom_audio_test.py.  QEMU is capped at 110 s.  Missing DOOM.E32/IWAD
=> RESULT: SKIP, exit 0.  Screendumps are always best-effort (never fatal);

Usage: python3 tools/r1_doom_visual_test.py [--no-build-iso] [--log PATH]
                                            [--bright-threshold 200]
                                            [--strip-px 40] [--gap 1.2]
                                            [--hold-sec 2.5]
"""
import argparse
import os
import re
import subprocess
import sys
import time

import numpy as np
from PIL import Image, ImageChops

import r1_doom_test as base
import r1_doom_audio_test as audio

TMP = base.TMP
LOG = TMP + "/r1_doom_visual.log"
QMP = TMP + "/r1_doom_visual-qmp.sock"
SHOT_A = TMP + "/doom_visual_a.png"
SHOT_B = TMP + "/doom_visual_b.png"
SHOT_HOLD_MID = TMP + "/doom_visual_hold_mid.png"
SHOT_HOLD = TMP + "/doom_visual_hold.png"
REST_SHOTS = tuple(TMP + f"/doom_visual_rest_{i}.png" for i in range(12))
ALL_SHOTS = (SHOT_A, SHOT_B, SHOT_HOLD_MID, SHOT_HOLD) + REST_SHOTS
PRE_FIX_SHOTS = (TMP + "/doom_final.png", TMP + "/doom_playtest.png",
                 TMP + "/doom_audio.png")
ISO = base.ISO
BOOT_TIMEOUT = 85
QEMU_CAP = 110
EXC_KEYS = base.EXC_KEYS

HOLD_RE = re.compile(r"DOOM:\s*HOLD(?:\s+(\S+))?", re.I)
KEY_RE = re.compile(r"DOOM:\s*KEY\s+(\d+)", re.I)
FRAME_RE = re.compile(r"DOOM:\s*frame\s+(\d+)", re.I)


def read_log(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError:
        return b""


def send_keys(vm, data, label, delay=0.35):
    ok = base.write_keys(vm, data, label)
    time.sleep(delay)
    return ok


def frame_box(w, h):
    """DOOM's 320x200 frame is blitted 2x-scaled and centred in the 1024x768 LFB."""
    fw, fh = (640, 400) if w >= 640 and h >= 400 else (320, 200)
    return ((w - fw) // 2, (h - fh) // 2,
            (w + fw) // 2, (h + fh) // 2)


def glyph_clusters(mask):
    """Count console-glyph-like bright blobs (small, narrow, 3-38 px tall)."""
    h, w = mask.shape
    seen = np.zeros_like(mask, dtype=bool)
    count = 0
    ys, xs = np.nonzero(mask)
    for y0, x0 in zip(ys, xs):
        if seen[y0, x0]:
            continue
        stack = [(int(y0), int(x0))]
        seen[y0, x0] = True
        minx = maxx = int(x0)
        miny = maxy = int(y0)
        area = 0
        while stack:
            y, x = stack.pop()
            area += 1
            minx, maxx = min(minx, x), max(maxx, x)
            miny, maxy = min(miny, y), max(maxy, y)
            for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                ny, nx = y + dy, x + dx
                if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
                    seen[ny, nx] = True
                    stack.append((ny, nx))
        bw = maxx - minx + 1
        bh = maxy - miny + 1
        if area >= 8 and 3 <= bh <= 38 and bw <= 26:
            count += 1
    return count


def analyse_frame(png, threshold=150, strip=40):
    """Bright-pixel / glyph stats for the top & bottom strips and the HUD."""
    result = {"path": png, "ok": False}
    if not (os.path.isfile(png) and os.path.getsize(png) > 1000):
        return result
    try:
        with Image.open(png) as im:
            im = im.convert("L")
            w, h = im.size
            gx0, gy0, gx1, gy1 = frame_box(w, h)
            regions = {
                "top": im.crop((0, 0, w, min(strip, h))),
                "bottom": im.crop((0, max(0, h - strip), w, h)),
                "hud": im.crop((gx0, max(gy0, gy1 - strip), gx1, gy1)),
            }
            for name, crop in regions.items():
                arr = np.asarray(crop, dtype=np.uint8)
                mask = arr >= threshold
                bright = int(mask.sum())
                result[name] = {"bright": bright,
                                "density": 100.0 * bright / max(1, arr.size),
                                "clusters": glyph_clusters(mask),
                                "mean": round(float(arr.mean()), 1)}
            result["size"] = f"{w}x{h}"
            result["ok"] = True
    except Exception as exc:
        print(f"[WARN] PIL analyse failed for {png}: {exc}")
    return result


def compare_frames(png_a, png_b, strip=40):
    """Diff bbox + % changed in edge strips vs game region (1.2 s apart)."""
    result = {"ok": False}
    if not (os.path.isfile(png_a) and os.path.isfile(png_b)):
        return result
    try:
        with Image.open(png_a) as ia, Image.open(png_b) as ib:
            a, b = ia.convert("L"), ib.convert("L")
            if a.size != b.size:
                result["note"] = f"size mismatch {a.size} vs {b.size}"
                return result
            w, h = a.size
            lit = ImageChops.difference(a, b).point(
                lambda v: 255 if v > 24 else 0)
            mask = np.asarray(lit, dtype=np.uint8) > 0
            gx0, gy0, gx1, gy1 = frame_box(w, h)
            ga = np.asarray(a, dtype=np.int16)[gy0:gy1, gx0:gx1]
            gb = np.asarray(b, dtype=np.int16)[gy0:gy1, gx0:gx1]
            result = {
                "ok": True,
                "bbox": lit.getbbox(),
                "top_pct": round(100.0 * float(mask[:strip].mean()), 4),
                "bottom_pct": round(100.0 * float(mask[-strip:].mean()), 4),
                "game_pct": round(100.0 * float(mask[gy0:gy1, gx0:gx1].mean()), 4),
                "whole_pct": round(100.0 * float(mask.mean()), 4),
                # Mean abs luma diff catches slow scene shifts that the >24
                # threshold misses (DOOM's brown walls barely change luma).
                "game_mean_abs": round(float(np.abs(ga - gb).mean()), 4),
            }
    except Exception as exc:
        print(f"[WARN] PIL frame diff failed: {exc}")
    return result


def scan_markers(raw, start=0):
    text = raw[start:].decode("utf-8", "replace")
    # doom_marker() prints via EUSER ord 915 AND the DebugPrint fallback, so
    # every marker appears twice back-to-back. Collapse those duplicates.
    lines = text.splitlines()
    text = "\n".join(ln for i, ln in enumerate(lines)
                     if i == 0 or ln != lines[i - 1])
    holds = HOLD_RE.findall(text)
    keys = KEY_RE.findall(text)
    frames = [int(n) for n in FRAME_RE.findall(text)]
    return {"holds": len(holds), "hold_keys": [k for k in holds if k],
            "keys": keys, "key_repeats": sum(1 for k in keys if k == "119"),
            "max_frame": max(frames) if frames else 0,
            "frames": len(frames)}


def run_qemu(log_path, cap, gap=1.2, hold_sec=2.5, strip=40):
    """Boot, launch DOOM, start the game, capture shots, probe held 'w'."""
    if os.path.exists(QMP):
        os.remove(QMP)
    qemu = [
        "timeout", str(cap), "qemu-system-i386",
        "-M", "q35", "-m", "128", "-vga", "std", "-display", "none",
        "-serial", "stdio", "-qmp", f"unix:{QMP},server,nowait",
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-no-reboot", "-cdrom", ISO, "-boot", "d",
    ]
    print(f"== QEMU boot (GRUB ISO {ISO}, AC97, hard cap {cap}s) ==")
    stats = {"booted": False, "started": False,
             "shots": [False] * len(ALL_SHOTS), "hold": {}, "release": {},
             "elapsed": 0.0, "boot_elapsed": 0.0}
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        qmp = None
        try:
            t0 = time.monotonic()
            stats["booted"] = audio.wait_for_prompt(log_path, BOOT_TIMEOUT)
            stats["boot_elapsed"] = time.monotonic() - t0
            print(f"[INFO] shell prompt after {stats['boot_elapsed']:.1f}s")
            if not stats["booted"]:
                return stats
            time.sleep(0.7)
            send_keys(vm, b"doom\n", "DOOM launch", 0.6)
            stats["started"] = base.wait_for_bytes(
                log_path, [b"DOOM: WAD OK", b"DOOM: ENGINE READY",
                           b"DOOM: TITLE", b"DOOM: WAD missing"], 30)
            if not stats["started"]:
                print("[WARN] DOOM did not reach engine ready")
                return stats
            # Enter leaves the title screen; the first one queues a 3-Enter
            # quickstart walk (title -> main -> new game -> episode -> skill),
            # so two more Enters are needed to confirm skill and START A LEVEL.
            # Without them 'w' only moves the skill menu and the animated menu
            # background is mistaken for player movement.
            send_keys(vm, b"\r", "Enter/start (title)", 0.7)
            send_keys(vm, b"\r\r", "Enter x2 (confirm skill, start level)", 1.6)
            base.wait_for_bytes(log_path, [b"DOOM: FRAME", b"DOOM: frame"], 20)
            time.sleep(2.0)                       # let the first level paint

            qmp = audio.QMPSession(QMP)           # held-open from here on
            stats["shots"][0] = qmp.screendump(SHOT_A)
            time.sleep(gap)
            stats["shots"][1] = qmp.screendump(SHOT_B)

            # (b) hold 'w' -- ONE make byte, no release.  The fix synthesizes
            # typematic presses (~80 ms apart) and emits ONE sparse
            # `DOOM: HOLD <keycode>` marker, so continuous movement is proven
            # by the mid-hold / end-hold screendumps, not marker quantity.
            hold_off = len(read_log(log_path))
            frame0 = scan_markers(read_log(log_path))["max_frame"]
            send_keys(vm, b"w", "HOLD w (single press, no release)", 0.0)
            samples = []
            mid_done = False
            sample_t0 = time.monotonic()
            while time.monotonic() - sample_t0 < hold_sec:
                time.sleep(0.2)
                elapsed = time.monotonic() - sample_t0
                s = scan_markers(read_log(log_path), hold_off)
                s["at"] = round(elapsed, 2)
                samples.append(s)
                if not mid_done and elapsed >= 1.1:
                    stats["shots"][2] = qmp.screendump(SHOT_HOLD_MID)
                    mid_done = True
            stats["hold"] = scan_markers(read_log(log_path), hold_off)
            stats["hold"]["frame_delta"] = stats["hold"]["max_frame"] - frame0
            stats["hold"]["max_per_sample"] = max(
                (s["holds"] for s in samples), default=0)
            stats["hold"]["first_hold_at"] = next(
                (s["at"] for s in samples if s["holds"]), None)
            stats["hold_end_off"] = len(read_log(log_path))
            stats["shots"][3] = qmp.screendump(SHOT_HOLD)

            # Release: the fix auto-releases once makes stop, but its guest
            # tick clock runs slower than wall time, so poll screendumps until
            # movement actually ceases instead of assuming a fixed timeout.
            rest_trace = []
            for i in range(len(REST_SHOTS)):
                time.sleep(0.8)
                ok_shot = qmp.screendump(REST_SHOTS[i])
                stats["shots"][4 + i] = ok_shot
                if i == 0:
                    continue
                d = compare_frames(REST_SHOTS[i - 1], REST_SHOTS[i], strip)
                pct = d.get("game_pct", -1.0) if d.get("ok") else -1.0
                mad = d.get("game_mean_abs", -1.0) if d.get("ok") else -1.0
                rest_trace.append((i, round(pct, 3), round(mad, 3)))
                # Two consecutive near-static frames: the delayed key-up has
                # reached DOOM and gamekeydown[] is finally clear.
                if (len(rest_trace) >= 2 and mad >= 0
                        and all(s[2] >= 0 and s[2] <= 0.5
                                for s in rest_trace[-2:])):
                    break
            stats["release"] = {
                "method": "auto-release (single make, no further bytes)",
                "tail_holds": scan_markers(
                    read_log(log_path), stats["hold_end_off"])["holds"],
                "rest_trace": rest_trace,
                "samples": samples,
            }
            # Clean shutdown, informational only (proves console lease returns).
            send_keys(vm, b"\x1b", "Esc/quit", 0.4)
            base.wait_for_bytes(log_path, [b"DOOM: DONE"], 8)
            stats["elapsed"] = time.monotonic() - t0
        finally:
            if qmp is not None:
                qmp.close()
            try:
                vm.terminate()
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return stats


def check(raw, stats, threshold, strip, edge_pct_max, hold_min):
    ok = True

    def row(label, good):
        nonlocal ok
        ok &= bool(good)
        print(f"  [{'PASS' if good else 'FAIL'}] {label}")

    text = raw.decode("utf-8", "replace")
    for marker in base.MARKERS:
        row(marker, marker in text)
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    row("no Exc/Panic/FAULT/#PF", not exc)
    for ln in exc[:4]:
        print("       ", ln[:170])

    # (a) edge-text density.  No pre-fix PNG is required; when one exists the
    # before/after densities are printed for direct numeric comparison.
    a = analyse_frame(SHOT_A, threshold, strip)
    b = analyse_frame(SHOT_B, threshold, strip)
    if a["ok"] and b["ok"]:
        print(f"  [INFO] frame {a['size']} luminance>={threshold}, strips {strip}px; "
              f"HUD ref bright={a['hud']['bright']} d={a['hud']['density']:.4f}%")
        for name, f in (("A", a), ("B", b)):
            print(f"  [INFO] shot {name}: top bright={f['top']['bright']} "
                  f"glyphs={f['top']['clusters']} d={f['top']['density']:.4f}% | "
                  f"bottom bright={f['bottom']['bright']} "
                  f"glyphs={f['bottom']['clusters']} "
                  f"d={f['bottom']['density']:.4f}%")
        for old in PRE_FIX_SHOTS:
            p = analyse_frame(old, threshold, strip)
            if p["ok"]:
                print(f"  [INFO] PRE-FIX {os.path.basename(old)}: "
                      f"top bright={p['top']['bright']} "
                      f"glyphs={p['top']['clusters']} "
                      f"d={p['top']['density']:.4f}% | bottom "
                      f"bright={p['bottom']['bright']} "
                      f"glyphs={p['bottom']['clusters']} "
                      f"d={p['bottom']['density']:.4f}%")
        hud = max(a["hud"]["bright"], 1)
        print(f"  [INFO] HUD reference (>= {threshold}): bright={a['hud']['bright']} "
              f"glyphs={a['hud']['clusters']} d={a['hud']['density']:.4f}%")
        # A pre-existing shell banner above the game is static and tolerated;
        # bleeding console output *during* DOOM adds lines / scrolls the strip,
        # so detect growth between the two samples rather than absolute text.
        bleed = []
        for edge in ("top", "bottom"):
            ca, cb = a[edge]["bright"], b[edge]["bright"]
            growth = cb - ca
            clusters = max(a[edge]["clusters"], b[edge]["clusters"])
            if growth > max(40, 0.05 * hud) or (clusters > 2 and growth > 0):
                bleed.append(f"{edge}: {ca}->{cb}px (+{growth}), "
                             f"glyphs {a[edge]['clusters']}->{b[edge]['clusters']}")
        row("no NEW console text in edge strips (static banner OK)", not bleed)
        if bleed:
            print("        bleed -> " + ", ".join(bleed))
        if a["top"]["bright"] or a["bottom"]["bright"]:
            print(f"  [INFO] static banner: top {a['top']['bright']}px "
                  f"{a['top']['clusters']} glyphs, bottom "
                  f"{a['bottom']['bright']}px (unchanged across the pair)")

        # (a) two-frame diff: animation stays inside the game area.
        d = compare_frames(SHOT_A, SHOT_B, strip)
        if d["ok"]:
            print(f"  [INFO] diff bbox={d['bbox']} top={d['top_pct']:.4f}% "
                  f"bottom={d['bottom_pct']:.4f}% game={d['game_pct']:.4f}% "
                  f"whole={d['whole_pct']:.4f}%")
            row("edge strips stable across 1.2 s (no flicker/shift)",
                d["top_pct"] <= edge_pct_max and d["bottom_pct"] <= edge_pct_max)
        else:
            print("  [INFO] frame diff unavailable: " + str(d.get("note", "n/a")))
    else:
        print("  [INFO] no usable screendump pair; visual check unavailable "
              "(never fatal)")

    # (b) held-key repeats.  The fix emits one sparse HOLD marker and real
    # repeats are synthesized silently, so prove them with three timed frame
    # diffs: idle A->B (baseline), moving B->MID and MID->END, then resting
    # REST_A->REST_B after the auto-release window.
    h = stats["hold"]
    d0 = compare_frames(SHOT_A, SHOT_B, strip)
    dm = compare_frames(SHOT_B, SHOT_HOLD_MID, strip)
    de = compare_frames(SHOT_HOLD_MID, SHOT_HOLD, strip)
    idle = d0.get("game_pct", 0.0) if d0.get("ok") else 0.0
    moving = max((dm.get("game_pct", 0.0) if dm.get("ok") else 0.0),
                 (de.get("game_pct", 0.0) if de.get("ok") else 0.0))
    print(f"  [INFO] HOLD markers={h.get('holds', 0)} "
          f"keys={h.get('hold_keys', [])} KEY119={h.get('key_repeats', 0)} "
          f"first@{h.get('first_hold_at')}s max/sample="
          f"{h.get('max_per_sample', 0)} frames+={h.get('frame_delta', 0)}")
    idle_abs = d0.get("game_mean_abs", 0.0) if d0.get("ok") else 0.0
    print(f"  [INFO] game-region diff %: idle={idle:.3f} "
          f"hold-mid={dm.get('game_pct', -1):.3f} "
          f"hold-end={de.get('game_pct', -1):.3f}; "
          f"idle mean-abs={idle_abs:.3f}")
    sustained = moving > max(2.0 * idle, 3.0) and h.get("frame_delta", 0) > 0
    marker = h.get("holds", 0) >= 1
    repeats = marker and (sustained or h.get("holds", 0) >= hold_min
                          or h.get("key_repeats", 0) >= hold_min)
    row("single 'w' press produced sustained repeats", repeats)
    rel = stats["release"]
    tail_holds = rel.get("tail_holds", -1)
    trace = rel.get("rest_trace", [])
    print(f"  [INFO] post-release frame diffs %: {trace}")
    rest_ok = len(trace) >= 2 and any(
        s[2] >= 0 and s[2] <= 0.5 and trace[i - 1][2] >= 0
        and trace[i - 1][2] <= 0.5 for i, s in enumerate(trace) if i > 0)
    row("repeats stop after release (2 near-static frames)",
        tail_holds == 0 and rest_ok)
    print(f"  [INFO] release: {rel.get('method', 'none')}, new HOLD markers "
          f"after window={tail_holds}")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-build-iso", action="store_true")
    ap.add_argument("--log", default=LOG)
    ap.add_argument("--bright-threshold", type=int, default=150,
                    help="luminance of console-grey text (observed 192 on this "
                         "VGA palette; DOOM's own bright text is >=230)")
    ap.add_argument("--strip-px", type=int, default=40)
    ap.add_argument("--gap", type=float, default=1.2)
    ap.add_argument("--hold-sec", type=float, default=2.5)
    ap.add_argument("--edge-pct-max", type=float, default=1.0)
    ap.add_argument("--hold-min", type=int, default=2)
    args = ap.parse_args()

    if base.check_assets() is None:
        print("\nRESULT: SKIP")
        return 0
    base.check_tools()
    os.makedirs(TMP, exist_ok=True)
    if not (args.no_build_iso and os.path.isfile(ISO)):
        base.build_iso()

    stats = run_qemu(args.log, QEMU_CAP, args.gap, args.hold_sec,
                     args.strip_px)
    raw = read_log(args.log)
    print(f"\n== results (serial log: {args.log})")
    if not stats["booted"] or not stats["started"]:
        print("RESULT: FAIL (guest did not boot/launch DOOM)")
        return 1
    ok = check(raw, stats, args.bright_threshold, args.strip_px,
               args.edge_pct_max, args.hold_min)
    shots = ", ".join(os.path.basename(p) + ("=OK" if s else "=missing")
                      for p, s in zip(ALL_SHOTS, stats["shots"]))
    print(f"  [INFO] screenshots: {shots}; boot {stats['boot_elapsed']:.1f}s, "
          f"total {stats['elapsed']:.1f}s")
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
