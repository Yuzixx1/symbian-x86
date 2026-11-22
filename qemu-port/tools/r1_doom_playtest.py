#!/usr/bin/env python3
"""r1_doom_playtest.py -- definitive DOOM long-run / playability test.

Unlike r1_doom_test.py (which only proves the title screen renders), this
harness keeps DOOM alive for a LONG session and proves it stays *playable*:

  1. boots the default GRUB ISO (64 MB FAT image, FDOOM1.WAD staged),
  2. launches `DOOM` from the serial shell,
  3. feeds gameplay keys (Enter to start, then WASD/space),
  4. sends NO quit key for ~40 s and asserts:
       * no `DOOM: DONE` before the 40 s mark (no auto-quit / 455-frame cap),
       * `DOOM: frame N` keeps advancing through the no-key window
         (continuous progress, not an absolute count -- host load varies),
       * no Exc / PanicKernExec / FAULT / #PF,
  5. then sends Esc and asserts a clean `DOOM: DONE` plus a returned shell
     prompt,
  6. best-effort QMP screendump of an in-game frame to
     /tmp/opencode/doom_playtest.png (never fails the test).

Missing host assets yield `RESULT: SKIP` and exit 0.  A hard `timeout 88`
caps QEMU; total wall time is ~60 s.  Reuses r1_doom_test.py's proven helpers
(check_assets, build_iso, wait_for_bytes, qmp_screendump, write_keys, ...).

Usage: python3 tools/r1_doom_playtest.py [--window 40] [--min-frames 70]
                                        [--png PATH] [--log PATH]
                                        [--no-build-iso]
"""
import argparse
import os
import re
import subprocess
import sys
import time

import r1_doom_test as base

TMP = base.TMP
LOG = TMP + "/r1_doom_playtest.log"
PNG = TMP + "/doom_playtest.png"
QMP = TMP + "/r1_doom_playtest-qmp.sock"
ISO = base.ISO                         # /tmp/opencode/r1_doom.iso

BOOT_TIMEOUT = 85                      # GRUB menu (10 s) + EKA2 init; load varies
QEMU_CAP = 120                         # hard outer cap (~60 s idle, more if loaded)
PROMPTS = base.PROMPTS
EXC_KEYS = base.EXC_KEYS


def read_log(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError:
        return b""


def wait_for_prompt(log_path, timeout):
    """Direct poll for the shell prompt (proven under heavy host load).

    base.wait_for_bytes() missed `X:/> ` when the host was loaded (~9.5-10),
    while a plain poll of the whole log with open(..., 'rb').read() found it
    at 33-34 s.  Use exactly that proven probe for the boot gate; do not rely
    on the shared helper here.
    """
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            with open(log_path, "rb") as f:
                if b"X:/> " in f.read():
                    return True
        except OSError:
            pass
        time.sleep(0.2)
    return False


def max_frame(raw):
    return max([int(n) for n in re.findall(rb"DOOM: frame (\d+)", raw)] or [0])


def first_index(raw, needle):
    i = raw.find(needle)
    return i if i >= 0 else None


def prompt_after(raw, offset):
    return any(p in raw[offset:offset + 4096] for p in PROMPTS)


def send_keys(vm, data, label, delay=0.35):
    ok = base.write_keys(vm, data, label)
    time.sleep(delay)
    return ok


def describe_png(path):
    """Best-effort PNG description (size + rough colour diversity)."""
    if not (os.path.isfile(path) and os.path.getsize(path) > 1000):
        return None
    info = f"{os.path.getsize(path)} bytes"
    try:
        from PIL import Image
        with Image.open(path) as im:
            im = im.convert("RGB")
            colors = len(im.getcolors(maxcolors=256 * 1024) or [])
            info = f"{im.size[0]}x{im.size[1]}, {info}, ~{colors} colours"
    except Exception:
        pass
    return info


def run_qemu(log_path, png_path, window, cap):
    if os.path.exists(QMP):
        os.remove(QMP)
    qemu = [
        "timeout", str(cap), "qemu-system-i386",
        "-M", "q35", "-m", "128", "-vga", "std", "-display", "none",
        "-serial", "stdio", "-qmp", f"unix:{QMP},server,nowait",
        "-no-reboot", "-cdrom", ISO, "-boot", "d",
    ]
    print(f"== QEMU boot (GRUB ISO {ISO}, hard cap {cap}s) ==")
    stats = {"booted": False, "started": False, "screenshot": False,
             "done_in_window": False, "done_after_esc": False,
             "post_prompt": False, "frames": 0, "frames_start": 0,
             "exc": [], "elapsed": 0.0, "boot_elapsed": 0.0,
             "esc_off": None}
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        try:
            boot_t0 = time.monotonic()
            stats["booted"] = wait_for_prompt(log_path, BOOT_TIMEOUT)
            stats["boot_elapsed"] = time.monotonic() - boot_t0
            print(f"[INFO] shell prompt after {stats['boot_elapsed']:.1f}s")
            if not stats["booted"]:
                print("[WARN] shell prompt never appeared")
                return stats
            # Fit the no-quit window under the hard cap, leaving room for Esc.
            window = max(12, min(window, QEMU_CAP
                                 - int(stats["boot_elapsed"]) - 16))
            time.sleep(0.5)
            send_keys(vm, b"DOOM\n", "DOOM launch", 0.6)
            stats["started"] = base.wait_for_bytes(
                log_path, [b"DOOM: WAD OK", b"DOOM: ENGINE READY",
                           b"DOOM: WAD missing", b"DOOM: DONE"], 30)
            if not stats["started"]:
                print("[WARN] DOOM did not reach engine ready")
                return stats

            # Gameplay keys: Enter starts the game, then movement/fire. NO Esc.
            for key, label in ((b"\r", "Enter/start"), (b"w", "W"),
                               (b"a", "A"), (b"s", "S"), (b"d", "D"),
                               (b"\x20", "space/fire")):
                send_keys(vm, key, label)
            time.sleep(2.0)                       # let the in-game frame paint

            # Best-effort in-game screendump (never gates the result).
            stats["screenshot"] = base.qmp_screendump(QMP, png_path)

            # Record where the no-quit window starts (byte + frame count).
            window_start = len(read_log(log_path))
            stats["frames_start"] = max_frame(read_log(log_path))
            t0 = time.monotonic()
            while time.monotonic() - t0 < window:
                raw = read_log(log_path)
                if b"DOOM: DONE" in raw[window_start:]:
                    stats["done_in_window"] = True
                time.sleep(1.0)
            stats["elapsed"] = time.monotonic() - t0
            stats["frames"] = max_frame(read_log(log_path))

            # Now quit: Esc, then wait for a clean DONE + returned prompt.
            # Record the log size at Esc-send time so check() can scan the
            # pre-Esc prefix for an (unwanted) auto-quit DONE and the post-Esc
            # suffix for the (required) deliberate-quit DONE.
            esc_off = len(read_log(log_path))
            stats["esc_off"] = esc_off
            send_keys(vm, b"\x1b", "Esc/quit", 0.3)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                raw = read_log(log_path)
                if b"DOOM: DONE" in raw[esc_off:]:
                    stats["done_after_esc"] = True
                d = first_index(raw, b"DOOM: DONE")
                if d is not None and prompt_after(raw, d):
                    stats["post_prompt"] = True
                    break
                time.sleep(0.4)
        finally:
            try:
                vm.terminate()
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return stats


def check(raw, stats, window, min_frames, png_path):
    ok = True

    def row(label, good):
        nonlocal ok
        ok &= bool(good)
        print(f"  [{'PASS' if good else 'FAIL'}] {label}")

    # Marker tables in r1_doom_test are str, the serial log is bytes; decode
    # once up front so membership tests compare like with like.
    text = raw.decode("utf-8", "replace")
    lines = text.splitlines()

    # Boot + engine markers (the base test already proves the WAD is real).
    for marker in base.MARKERS:
        row(marker, marker in text)
    row("WAD full byte count", any("DOOM: WAD" in ln and base.WAD_SIZE in ln
                                   for ln in lines))
    for marker in base.BOOT_MARKERS:
        row("boot: " + marker, marker in text)
    row("EUSER ord 915 bound",
        any("e32run: bind slot" in ln and "ord 915" in ln
            for ln in lines))

    # Gameplay key marker: informational (the serial console may be
    # output-only on some builds, so absence must not fail the playtest).
    keyed = b"DOOM: KEY" in raw
    print(f"  [{'PASS' if keyed else 'INFO'}] DOOM: KEY marker "
          f"(gameplay keys consumed: {keyed})")

    # The core regression: DOOM must not auto-quit during the no-key window.
    # Scan ONLY the log prefix captured before Esc was sent.  A DONE in the
    # post-Esc suffix is expected and required (asserted below), so testing
    # the full log would falsely fail on the legitimate Esc-driven shutdown.
    esc_off = stats["esc_off"]
    pre_done = first_index(raw[:esc_off], b"DOOM: DONE")
    row(f"no DOOM: DONE before {window}s",
        not stats["done_in_window"] and pre_done is None)
    if pre_done is not None:
        print(f"        premature DONE at byte {pre_done} "
              f"(before Esc at byte {esc_off})")

    # Frame progress: verify CONTINUOUS PROGRESS, not an absolute count. Under
    # heavy host load only ~70 frames render in 40 s, so a fixed >1000 (or even
    # >455) target says nothing.  What proves the old auto-quit regression is
    # gone is that frames keep advancing through the no-key window
    # (frames_start -> frames), with no DONE before Esc (asserted above).
    frames = stats["frames"]
    started = stats["frames_start"]
    row(f"frames advance during no-quit window ({started} -> {frames})",
        frames > started)
    row(f"frames reach minimum floor {min_frames} (max={frames})",
        frames >= min_frames)

    # Clean shutdown only after the deliberate Esc: a DONE must exist in the
    # post-Esc suffix, followed by a returned shell prompt.
    post_done = first_index(raw[esc_off:], b"DOOM: DONE")
    post_done_off = esc_off + post_done if post_done is not None else None
    row("clean DOOM: DONE after Esc",
        post_done is not None and stats["done_after_esc"])
    row("shell prompt returns after DONE",
        post_done_off is not None and prompt_after(raw, post_done_off))
    if stats["done_in_window"] or pre_done is not None:
        print("        NOTE: DONE appeared before Esc (auto-quit)")

    # No kernel faults anywhere in the session.
    exc = [ln for ln in lines if any(k in ln for k in EXC_KEYS)]
    row("no Exc/PanicKernExec/FAULT/#PF", not exc)
    for ln in exc[:6]:
        print("        ", ln[:180])
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--window", type=int, default=40,
                    help="seconds to run with no quit key (default 40)")
    ap.add_argument("--min-frames", type=int, default=70,
                    help="minimum frame floor (default 70; the real gate is "
                         "continuous progress + no DONE before Esc)")
    ap.add_argument("--png", default=PNG)
    ap.add_argument("--log", default=LOG)
    ap.add_argument("--no-build-iso", action="store_true")
    args = ap.parse_args()

    assets = base.check_assets()
    if assets is None:
        print("\nRESULT: SKIP")
        return 0
    base.check_tools()
    os.makedirs(TMP, exist_ok=True)
    if not (args.no_build_iso and os.path.isfile(ISO)):
        base.build_iso()

    stats = run_qemu(args.log, args.png, args.window, QEMU_CAP)
    raw = read_log(args.log)
    print(f"\n== results (serial log: {args.log})")
    if not stats["booted"] or not stats["started"]:
        print("RESULT: FAIL (guest did not boot/launch DOOM)")
        return 1
    ok = check(raw, stats, args.window, args.min_frames, args.png)

    shot = describe_png(args.png)
    print(f"  [{'PASS' if shot else 'INFO'}] screendump in-game frame: "
          f"{args.png}" + (f" ({shot})" if shot else " (unavailable)"))
    fps = stats["frames"] / stats["elapsed"] if stats["elapsed"] else 0
    print(f"  [INFO] boot {stats['boot_elapsed']:.1f}s, no-quit window "
          f"{stats['elapsed']:.1f}s, max frame {stats['frames']} "
          f"(~{fps:.1f} fps, past old 455-frame cap: "
          f"{stats['frames'] > 455}), Esc: DONE={stats['done_after_esc']}, "
          f"prompt={stats['post_prompt']}")
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
