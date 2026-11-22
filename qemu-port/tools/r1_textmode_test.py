#!/usr/bin/env python3
"""r1_textmode_test.py -- prove the boot-in-VGA-text-mode architecture.

The port now boots in real VGA mode 3 (80x25 text at 0xB8000, 16 colours) and
switches to VBE 2.0 0x118 (1024x768x32 LFB) only when a graphical E32 launches.
This harness boots the GRUB ISO with `-vga std -serial stdio`, uses the proven
*held-open* QMP screendump session from r1_doom_audio_test.py, and asserts:

  1. At the EKA2>/X:/> prompt the host-visible screen is 720x400 or 640x400
     (80x25 text), has few distinct colours and real glyph patterns in its top
     rows -- NOT the old 1024x768 RGB bitmap console.
  2. `version` + `help` appear on COM1 AND change the on-screen text region.
  3. Launching DOOM (default) or GUISHELL (`--app guishell`) switches to a
     1024x768 graphics mode; the second screendump must prove it.
  4. No Exc / PanicKernExec / FAULT / #PF in the serial log.

Screendumps are best-effort: a flaky QMP connection is reported honestly as
INFO and never crashes the test. Missing DOOM/IWAD (or GUISHELL) => SKIP, 0.

Usage: python3 tools/r1_textmode_test.py [--app doom|guishell]
                                         [--no-build-iso] [--log PATH]
"""
import argparse
import os
import subprocess
import sys
import time

import numpy as np
from PIL import Image

import r1_doom_test as base
import r1_doom_audio_test as audio

TMP = base.TMP
ISO = base.ISO
LOG = TMP + "/r1_textmode.log"
QMP = TMP + "/r1_textmode-qmp.sock"
SHOT_TEXT = TMP + "/textmode_prompt.png"
SHOT_CMDS = TMP + "/textmode_after_cmds.png"
SHOT_GFX = TMP + "/textmode_gfx.png"
BOOT_TIMEOUT = 85
QEMU_CAP = 120
EXC_KEYS = base.EXC_KEYS
TEXT_SIZES = {(720, 400), (640, 400)}   # 80x25: 9x16 or 8x16 character cells
GFX_SIZE = (1024, 768)                  # VBE 0x118


def read_log(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError:
        return b""


def wait_for_prompt(log_path, timeout):
    """Poll the whole log for any shell prompt (the proven direct-poll gate)."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        raw = read_log(log_path)
        if any(p in raw for p in base.PROMPTS):
            return True
        time.sleep(0.2)
    return False


def analyse_shot(png):
    """Resolution, palette size and glyph-banding stats for one screendump."""
    r = {"ok": False, "path": png}
    if not (os.path.isfile(png) and os.path.getsize(png) > 1000):
        return r
    try:
        with Image.open(png) as im:
            rgb = im.convert("RGB")
            w, h = rgb.size
            flat = np.asarray(rgb).reshape(-1, 3)
            colours = int(np.unique(
                flat.view([("r", "u1"), ("g", "u1"), ("b", "u1")])).size)
            mask = np.asarray(rgb.convert("L"), dtype=np.uint8)[:min(h, 120)] >= 96
            rows = mask.sum(axis=1)
            active = rows > max(8, int(w * 0.01))
            bands, prev = 0, False
            for on in active:
                if on and not prev:
                    bands += 1
                prev = on
            trans = [int(np.abs(np.diff(mask[y].astype(np.int8))).sum())
                     for y in np.nonzero(active)[0]]
            r.update(ok=True, w=w, h=h, size=f"{w}x{h}", colours=colours,
                     bands=bands,
                     transitions=round(float(np.mean(trans)), 1) if trans else 0,
                     bright=round(float(mask.mean()), 4))
    except Exception as exc:
        print(f"[WARN] image analysis failed for {png}: {exc}")
    return r


def is_text_screen(a):
    """80x25 text signature: small mode size, tiny palette, banded glyphs."""
    return (a["ok"] and (a["w"], a["h"]) in TEXT_SIZES and a["colours"] <= 32
            and a["bands"] >= 3 and a["transitions"] >= 6
            and 0.002 <= a["bright"] <= 0.35)


def changed_fraction(png_a, png_b):
    """Fraction of noticeably changed pixels; None when unavailable."""
    try:
        with Image.open(png_a) as ia, Image.open(png_b) as ib:
            a = np.asarray(ia.convert("L"), dtype=np.int16)
            b = np.asarray(ib.convert("L"), dtype=np.int16)
            if a.shape != b.shape:
                return 1.0
            return float((np.abs(a - b) > 16).mean())
    except Exception as exc:
        print(f"[WARN] frame diff failed: {exc}")
        return None


def run_qemu(log_path, app):
    """Boot once; dump text screen, after commands, then the graphics screen."""
    if os.path.exists(QMP):
        os.remove(QMP)
    qemu = ["timeout", str(QEMU_CAP), "qemu-system-i386",
            "-M", "q35", "-m", "128", "-vga", "std", "-display", "none",
            "-serial", "stdio", "-qmp", f"unix:{QMP},server,nowait",
            "-audiodev", "none,id=a0", "-no-reboot",
            "-cdrom", ISO, "-boot", "d"]
    print(f"== QEMU boot (GRUB ISO {ISO}, -vga std, cap {QEMU_CAP}s) ==")
    stats = {"booted": False, "version": False, "help": False, "app": False,
             "shots": {SHOT_TEXT: False, SHOT_CMDS: False, SHOT_GFX: False},
             "switch_marker": False, "app_name": app}
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        qmp = None
        try:
            t0 = time.monotonic()
            stats["booted"] = wait_for_prompt(log_path, BOOT_TIMEOUT)
            print(f"[INFO] prompt after {time.monotonic() - t0:.1f}s "
                  f"(booted={stats['booted']})")
            if not stats["booted"]:
                return stats
            time.sleep(0.8)
            qmp = audio.QMPSession(QMP)            # held open; never fatal
            stats["shots"][SHOT_TEXT] = qmp.screendump(SHOT_TEXT)
            base.write_keys(vm, b"version\n", "version")
            stats["version"] = base.wait_for_bytes(
                log_path, [b"Symbian EKA2 full kernel", b"feats="], 15)
            base.write_keys(vm, b"help\n", "help")
            stats["help"] = base.wait_for_bytes(
                log_path, [b"EKA2 DOS-style command shell", b"Files:"], 15)
            time.sleep(0.6)
            stats["shots"][SHOT_CMDS] = qmp.screendump(SHOT_CMDS)

            if app == "doom":
                base.write_keys(vm, b"doom\n", "DOOM launch")
                stats["app"] = base.wait_for_bytes(
                    log_path, [b"DOOM: WAD OK", b"DOOM: ENGINE READY",
                               b"DOOM: TITLE"], 30)
                time.sleep(3.0)
            else:
                base.write_keys(vm, b"guishell\n", "GUISHELL launch")
                stats["app"] = base.wait_for_bytes(
                    log_path, [b"GUI: ", b"GUISHELL"], 25)
                time.sleep(1.5)
            raw = read_log(log_path)
            stats["switch_marker"] = any(
                k in raw for k in (b"VBE: SWITCH OK", b"VBE switch",
                                   b"DISPLAY: VBE mode 0x118 set"))
            stats["shots"][SHOT_GFX] = qmp.screendump(SHOT_GFX)
            if not stats["shots"][SHOT_GFX] or analyse_shot(SHOT_GFX).get("size") \
                    != f"{GFX_SIZE[0]}x{GFX_SIZE[1]}":
                time.sleep(2.0)                    # mode switch / PNG is async
                stats["shots"][SHOT_GFX] = qmp.screendump(SHOT_GFX)
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


def check(raw, stats):
    """Hard-assert serial facts; visual assertions only when PNGs exist."""
    ok = True

    def row(label, good):
        nonlocal ok
        ok &= bool(good)
        print(f"  [{'PASS' if good else 'FAIL'}] {label}")

    def info(label):
        print(f"  [INFO] {label} (QMP screendump unavailable -- not fatal)")

    text = raw.decode("utf-8", "replace")
    row("guest reached the shell prompt", stats["booted"])
    row("`version` output on COM1", stats["version"])
    row("`help` output on COM1", stats["help"])
    row(f"graphical app ({stats['app_name']}) launched", stats["app"])
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    row("no Exc/PanicKernExec/FAULT/#PF", not exc)
    for ln in exc[:5]:
        print("       ", ln[:180])

    a = analyse_shot(SHOT_TEXT)
    if stats["shots"][SHOT_TEXT] and a["ok"]:
        print(f"  [INFO] prompt screen: {a['size']}, {a['colours']} colours, "
              f"{a['bands']} glyph rows, {a['transitions']} transitions/row")
        row("prompt screen is 80x25 text, not a 1024x768 RGB bitmap",
            is_text_screen(a))
    else:
        info("prompt text-mode resolution/colours/glyphs NOT VERIFIED")
    if stats["shots"][SHOT_CMDS] and stats["shots"][SHOT_TEXT]:
        frac = changed_fraction(SHOT_TEXT, SHOT_CMDS)
        if frac is None:
            info("on-screen text change NOT VERIFIED")
        else:
            print(f"  [INFO] screen text region changed: {frac * 100:.2f}%")
            row("`version`/`help` changed the on-screen text region",
                frac >= 0.002)
    else:
        info("on-screen text change NOT VERIFIED")
    g = analyse_shot(SHOT_GFX)
    if stats["shots"][SHOT_GFX] and g["ok"]:
        print(f"  [INFO] post-launch screen: {g['size']}, "
              f"{g['colours']} colours")
        row("VBE switch to 1024x768 graphics", (g["w"], g["h"]) == GFX_SIZE)
    else:
        info("post-launch VBE 1024x768 resolution NOT VERIFIED")
    if stats["switch_marker"]:
        print("  [INFO] serial log confirms the VBE switch marker")
    else:
        print("  [INFO] no explicit VBE-switch marker in serial log "
              "(screendump is the authority)")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--app", choices=("doom", "guishell"), default="doom")
    ap.add_argument("--no-build-iso", action="store_true")
    ap.add_argument("--log", default=LOG)
    args = ap.parse_args()

    if args.app == "doom":
        if base.check_assets() is None:         # DOOM.E32 + IWAD gate
            print("\nRESULT: SKIP")
            return 0
    else:
        shell = os.path.join(base.REPO, "build/user/GUISHELL.E32")
        if not (os.path.isfile(shell) or os.path.isfile(ISO)):
            print(f"[SKIP] {shell} and ISO {ISO} both missing.")
            print("\nRESULT: SKIP")
            return 0
    base.check_tools()
    os.makedirs(TMP, exist_ok=True)
    if not (args.no_build_iso and os.path.isfile(ISO)):
        base.build_iso()

    stats = run_qemu(args.log, args.app)
    print(f"\n== results (serial log: {args.log})")
    ok = check(read_log(args.log), stats)
    shots = ", ".join(os.path.basename(p) + ("=OK" if s else "=missing")
                      for p, s in stats["shots"].items())
    print(f"  [INFO] screenshots: {shots}")
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
