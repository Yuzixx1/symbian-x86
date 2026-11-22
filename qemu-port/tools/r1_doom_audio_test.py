#!/usr/bin/env python3
"""r1_doom_audio_test.py -- DOOM + AC97 PCM audio integration test.

Extends the proven r1_doom_test.py staging/boot path with the audio hardware
this R1 wave adds. It boots the GRUB ISO with an AC97 device attached
(`-audiodev none,id=a0 -device AC97,audiodev=a0`), launches DOOM, presses Enter
to leave the title screen and start the game, then asserts the *whole* audio
chain in the serial log:

  * guest AC97 boot banner with the QEMU codec vendor ID 8384:7600,
  * DOOM's ``SND:`` markers (INIT, sfx, play),
  * the kernel's ``AU:`` PCM-OUT play markers,
  * ``DOOM: TITLE`` / ``DOOM: FRAME`` (the game really rendered),
  * no Exc / PanicKernExec / FAULT / #PF anywhere.

`-audiodev none` means "no host-side audio sink": QEMU still emulates the AC97
PCI device and guest DMA normally, but does not open a real speaker, so the
test is fully headless. A best-effort PNG is grabbed through a *held-open* QMP
connection (see QMPSession); a flaky screendump never fails the test.

Missing DOOM.E32 / IWAD / EUSER.DLL assets yield ``RESULT: SKIP`` with exit 0.

Usage: python3 tools/r1_doom_audio_test.py [--no-build-iso] [--log PATH]
                                           [--png PATH]
"""
import argparse
import os
import socket
import subprocess
import sys
import time

import r1_doom_test as base

TMP = base.TMP
LOG = TMP + "/r1_doom_audio.log"
PNG = TMP + "/doom_audio.png"
QMP = TMP + "/r1_doom_audio-qmp.sock"
ISO = base.ISO
QEMU_CAP = 110                         # hard outer cap; boot is ~33 s typical
BOOT_TIMEOUT = 85                      # GRUB menu + EKA2 init under host load
PROMPTS = base.PROMPTS
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF")


def wait_for_prompt(log_path, timeout):
    """Direct poll for the shell prompt -- the proven reliable boot gate.

    ``base.wait_for_bytes()`` missed ``X:/> `` under heavy host load (~9.5-10)
    and the previous version of this harness declared the prompt "detected too
    late" and never launched DOOM.  A plain poll of the whole log with
    ``open(..., 'rb').read()`` found the prompt at 33-34 s in r1_doom_playtest,
    so use exactly that probe here.  Never raises.
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


class QMPSession:
    """One QMP connection opened once and held open for the whole boot.

    The greeting + qmp_capabilities handshake happen a single time; screendump
    reuses the same socket/file. Every public method is best-effort and never
    raises, because an audio test must not be gated on a graphics screenshot.
    """

    def __init__(self, sock_path, wait=20):
        self.sock = None
        self.stream = None
        deadline = time.monotonic() + wait
        while time.monotonic() < deadline and not os.path.exists(sock_path):
            time.sleep(0.25)
        if not os.path.exists(sock_path):
            return
        try:
            self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.sock.settimeout(5)
            self.sock.connect(sock_path)
            self.stream = self.sock.makefile("rwb")
            if not self.stream.readline():          # QMP greeting
                raise RuntimeError("empty QMP greeting")
            self.stream.write(b'{"execute":"qmp_capabilities"}\n')
            self.stream.flush()
            self.stream.readline()                  # capabilities reply
        except Exception as exc:
            print(f"[WARN] QMP handshake failed: {exc}")
            self.close()

    def screendump(self, png):
        """Write a PNG via the held-open stream; return True on success."""
        if self.stream is None:
            return False
        tmp = png + ".tmp"
        try:
            if os.path.exists(tmp):
                os.remove(tmp)
            cmd = ('{"execute":"screendump","arguments":{"filename":"'
                   + tmp + '","format":"png"}}\n')
            self.stream.write(cmd.encode())
            self.stream.flush()
            for _ in range(10):                     # consume the reply line(s)
                line = self.stream.readline()
                if not line or b"return" in line or b"error" in line.lower():
                    if line and b"error" in line.lower():
                        print(f"[WARN] QMP screendump error: "
                              f"{line[:160]!r}")
                    break
        except Exception as exc:
            print(f"[WARN] QMP screendump command failed: {exc}")
            return False
        for _ in range(20):                         # PNG write is asynchronous
            if os.path.exists(tmp) and os.path.getsize(tmp) > 1000:
                break
            time.sleep(0.15)
        if os.path.exists(tmp) and os.path.getsize(tmp) > 1000:
            os.replace(tmp, png)
            print(f"[INFO] screendump {png}: {os.path.getsize(png)} bytes")
            return True
        print(f"[WARN] screendump PNG not produced ({png})")
        return False

    def close(self):
        for obj in (self.stream, self.sock):
            if obj is not None:
                try:
                    obj.close()
                except Exception:
                    pass
        self.stream = self.sock = None


def run_qemu(log_path, png_path):
    """Boot once with AC97 attached, launch DOOM, Enter, grab a screenshot."""
    if os.path.exists(QMP):
        os.remove(QMP)
    qemu = [
        "timeout", str(QEMU_CAP), "qemu-system-i386",
        "-M", "q35", "-m", "128", "-vga", "std", "-display", "none",
        "-serial", "stdio", "-qmp", f"unix:{QMP},server,nowait",
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-no-reboot", "-cdrom", ISO, "-boot", "d",
    ]
    print(f"== QEMU boot (GRUB ISO {ISO}, AC97 attached, cap {QEMU_CAP}s) ==")
    shot = False
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        qmp = None
        try:
            boot_t0 = time.monotonic()
            booted = wait_for_prompt(log_path, BOOT_TIMEOUT)
            print(f"[INFO] shell prompt after "
                  f"{time.monotonic() - boot_t0:.1f}s (booted={booted})")
            if not booted:
                print("[WARN] shell prompt never appeared; DOOM never launched")
                return shot
            # Settle the newly-printed prompt, then launch DOOM.  The shell
            # matches executable names case-insensitively, so `doom` resolves
            # to DOOM.E32 (same command the base/proven tests use).
            time.sleep(1.0)
            base.write_keys(vm, b"doom\n", "DOOM launch")
            started = base.wait_for_bytes(
                log_path,
                [b"DOOM: WAD OK", b"DOOM: ENGINE READY", b"DOOM: TITLE"],
                30)
            print(f"[INFO] DOOM launch markers seen: {started}")
            time.sleep(2.0)                        # let the title screen paint
            # Enter dismisses the title screen and starts the actual game;
            # SFX/music init and the first PCM DMA happen on entry, so this is
            # essential for real SND:/AU: markers.
            base.write_keys(vm, b"\r", "Enter/start game")
            # A few gameplay keys actually fire weapons so SFX are requested;
            # I_UpdateSound self-pumps them into exec 240 PCM-op play calls.
            time.sleep(1.5)
            base.write_keys(vm, b"\x20 \x20", "fire weapon (SFX)")
            base.wait_for_bytes(
                log_path,
                [b"DOOM: FRAME", b"SND: play", b"SND: music", b"AU: pcm play"],
                22)
            time.sleep(3.0)                        # generous: let tics drain
            qmp = QMPSession(QMP)                  # held open from here on
            shot = qmp.screendump(png_path)        # best-effort; never fatal
            time.sleep(0.5)
        finally:
            if qmp is not None:
                qmp.close()
            try:
                vm.terminate()
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return shot


def _has(lines, prefix, *words):
    """True if any line contains `prefix` (case-insensitive) and all words."""
    for line in lines:
        low = line.lower()
        if prefix.lower() in low and all(w.lower() in low for w in words):
            return True
    return False


def check_log(text):
    lines = text.splitlines()
    ok = True

    def row(label, good):
        nonlocal ok
        ok &= bool(good)
        print(f"  [{'PASS' if good else 'FAIL'}] {label}")

    # Guest AC97 enumeration banner plus the QEMU Sigmatel codec vendor ID.
    row("AC97 boot banner (vendor 8384:7600)",
        _has(lines, "audio:", "8384:7600"))
    # DOOM sound backend markers: init, a registered SFX and a play/flush.
    row("SND: INIT", _has(lines, "SND:", "init"))
    row("SND: sfx", _has(lines, "SND:", "sfx"))
    row("SND: play", _has(lines, "SND:", "play"))
    # Music starts when a level actually begins; report it, but don't fail a
    # short title-screen run that never reached a MUS lump.
    music = _has(lines, "SND:", "music")
    print(f"  [{'PASS' if music else 'INFO'}] SND: music"
          + (" (MUS lump rendered)" if music else " (no MUS lump played)"))
    # Kernel PCM-OUT DMA markers emitted by the new AU: path.
    row("AU: pcm play", _has(lines, "AU:", "pcm", "play"))
    # The game itself must have rendered a title and at least one frame.
    row("DOOM: TITLE", "DOOM: TITLE" in text)
    row("DOOM: FRAME", "DOOM: FRAME" in text)
    exc = [ln for ln in lines if any(k in ln for k in EXC_KEYS)]
    row("no Exc/PanicKernExec/FAULT/#PF", not exc)
    for ln in exc[:8]:
        print("      ", ln[:200])
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-build-iso", action="store_true")
    ap.add_argument("--log", default=LOG)
    ap.add_argument("--png", default=PNG)
    args = ap.parse_args()

    assets = base.check_assets()
    if assets is None:
        print("\nRESULT: SKIP")
        return 0
    base.check_tools()
    os.makedirs(TMP, exist_ok=True)
    if not (args.no_build_iso and os.path.isfile(ISO)):
        base.build_iso()

    shot = run_qemu(args.log, args.png)
    with open(args.log, "r", errors="replace") as f:
        text = f.read()
    print(f"\n== results (serial log: {args.log})")
    ok = check_log(text)
    if shot:
        print(f"  [PASS] screendump {args.png} (verify in-game frame manually)")
    else:
        print(f"  [INFO] screendump unavailable ({args.png}) -- not a failure")
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
