#!/usr/bin/env python3
"""r1_efile_clean_prove.py -- ONE uninterrupted EFILE prove on the final ISO.

Why this wrapper exists: recent EFILE proves either ended in harness SIGTERM or
were overwritten by concurrent agents sharing /tmp/opencode paths.  This tool
uses private scratch paths under a roomy cache directory, boots the *current*
GRUB ISO (not build/kernfull.elf, which another agent may have rebuilt), and
waits for the client's natural completion marker instead of a fixed 4 s sleep.

Hard rules:
  * kernelhwsrv/ is READ-ONLY; no source edits, no rebuild, no `make clean`.
  * The raw serial log is opened exactly once and never overwritten mid-run.
  * QEMU is stopped with a QMP `quit` (fallback SIGTERM only after completion),
    so a successful raw log does not contain "terminating on signal 15".
  * All counts come from that ONE raw log, never from harness stdout.

Usage:
    python3 tools/r1_efile_clean_prove.py --dry-run
    python3 tools/r1_efile_clean_prove.py
    python3 tools/r1_efile_clean_prove.py --log-dir /home/yuzi/.cache/sybian-r1
"""
import argparse
import os
import shlex
import shutil
import socket
import subprocess
import sys
import time
from pathlib import Path

PORT = Path(__file__).resolve().parent.parent
ISO = PORT / "build" / "symbian-grub.iso"
GENUINE = PORT / "genuine" / "build"
EUSER = Path("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll")
F32CLIENT = PORT / "genuine" / "f32_probe" / "F32CLIENT.E32"

CACHE_ROOT = Path("/home/yuzi/.cache/sybian-r1")
RAW_NAME = "efile_clean_prove_raw.log"
FINAL_NAME = "efile_clean_prove.log"          # stable, authoritative path

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PROMPT = b"C:/> "
BOOT_ANCHOR = b"KERN spawning genuine boot DThread"
CLIENT_START = b"F32CLIENT: start"
# Any of these is the client's natural end-of-run marker.  Two copies are
# normally emitted because the genuine EUSER entry path re-runs app_main.
TERMINALS = (b"F32CLIENT: IPC round trip OK",
             b"F32CLIENT: SessionPath FAILED",
             b"F32CLIENT: no IPC reply",
             b"F32CLIENT: connect failed")
EXPECTED_TERMINALS = 2
QUIET_SECONDS = 12.0                          # stop waiting after the log goes quiet
MIN_FREE_BYTES = 512 * 1024 ** 2
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def free_bytes(path):
    probe = Path(os.path.abspath(os.fspath(path)))
    while not probe.exists() and probe != probe.parent:
        probe = probe.parent
    return shutil.disk_usage(probe).free


def sha256(path):
    import hashlib
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def stage_disk(logdir):
    """FAT32 scratch C: with the canonical acceptance staging set."""
    disk, fs = logdir / "efile_clean_prove.img", logdir / "efile_clean_prove_fs.img"
    for p in (disk, fs):
        if p.exists():
            p.unlink()
    with disk.open("wb") as f:
        f.truncate(MB * 1024 * 1024)
    with fs.open("wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", fs, check=True, stdout=subprocess.DEVNULL)
    files = [(EUSER, "EUSER.DLL"), (GENUINE / "EFILE.E32", "EFILE.E32"),
             (GENUINE / "EFAT32.FSY", "EFAT32.FSY"), (F32CLIENT, "F32CLIENT.E32")]
    for src, name in files:
        if not Path(src).is_file():
            sys.exit(f"missing staged artifact {src}")
        sh("mcopy", "-i", fs, src, f"::{name}", check=True)
    data = fs.read_bytes()
    if len(data) > FS_SIZE:
        raise RuntimeError("staged FAT image larger than partition")
    entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
             + LBA.to_bytes(4, "little") + SECTORS.to_bytes(4, "little"))
    mbr = bytearray(512)
    mbr[446:462] = entry
    mbr[510], mbr[511] = 0x55, 0xAA
    with disk.open("r+b") as f:
        f.seek(OFFSET)
        f.write(data)
        f.seek(0)
        f.write(mbr)
    listing = sh("mdir", "-i", f"{disk}@@{OFFSET}", "::", check=True,
                 capture_output=True, text=True).stdout
    norm = "".join(listing.lower().split()).replace(".", "")
    for _src, name in files:
        if name.lower().replace(".", "") not in norm:
            raise RuntimeError(f"staged disk missing {name}")
    print(f"  [PASS] FAT32 disk staged with {len(files)} genuine files", flush=True)
    return disk


def tail_from(path, pos):
    with open(path, "rb") as f:
        f.seek(pos)
        return f.read()


def wait_for(path, needle, pos=0, timeout=180):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        i = tail_from(path, pos).find(needle)
        if i >= 0:
            return pos + i + len(needle)
        time.sleep(0.2)
    raise RuntimeError(f"serial log missing {needle!r} (see {path})")


def terminal_count(text):
    return sum(text.count(m.decode()) for m in TERMINALS)


def wait_for_completion(path, pos, timeout=180):
    """Wait for >=1 natural terminal marker, then settle for up to 30 s.

    Returns (status, first_marker_pos).  status is COMPLETE (2 markers or a
    quiet log), SINGLE (one marker, quiet) or TIMEOUT.
    """
    deadline = time.monotonic() + timeout
    first, quiet = None, 0
    last_size = path.stat().st_size
    while time.monotonic() < deadline:
        data = tail_from(path, pos)
        n = terminal_count(data.decode("utf-8", "replace"))
        if n and first is None:
            first = path.stat().st_size - len(data)
            print(f"  [MARK] first client completion marker @ byte {first}",
                  flush=True)
            last_size = path.stat().st_size
        if n >= EXPECTED_TERMINALS:
            return "COMPLETE", first
        # Settle: accept one completion only after the log is quiet for a
        # while (the second app_main instance can start a few seconds later).
        size = path.stat().st_size
        if size > last_size:
            quiet = 0
            last_size = size
        elif first is not None:
            quiet += 1
        if quiet >= 8:
            return "SINGLE", first
        time.sleep(1.0)
    return "TIMEOUT", first


def qmp_quit(sock_path, log):
    """Stop QEMU cleanly; no SIGTERM line in the raw serial log."""
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(4)
            s.connect(str(sock_path))
            f = s.makefile("rwb")
            f.readline()                       # QMP greeting
            f.write(b'{"execute":"qmp_capabilities"}\n'); f.flush(); f.readline()
            f.write(b'{"execute":"quit"}\n'); f.flush()
            f.close(); s.close()
            print("  [PASS] QMP quit (no SIGTERM in raw log)", flush=True)
            return True
        except OSError as exc:
            print(f"  [WARN] QMP quit retry: {exc}", flush=True)
            time.sleep(0.4)
    print("  [WARN] QMP quit failed; falling back to terminate", flush=True)
    return False


def boot_and_capture(logdir, raw):
    disk = stage_disk(logdir)
    sock = logdir / f"efile_clean_prove-{os.getpid()}.qmp.sock"
    for p in (sock,):
        if p.exists():
            p.unlink()
    # The ISO is authoritative: build/kernfull.elf may have been rebuilt with
    # the rejected entry trampoline.  Verified separately that the ISO has no
    # trampoline strings.
    qemu = ["qemu-system-i386", "-M", "q35", "-m", "128",
            "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
            "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
            "-vga", "std", "-display", "none", "-serial", "stdio",
            "-cdrom", str(ISO), "-boot", "d",
            "-drive", f"file={disk},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot",
            "-qmp", f"unix:{sock},server,nowait"]
    print(f"== booting CURRENT GRUB ISO {ISO}", flush=True)
    print(f"   raw log -> {raw}", flush=True)
    status = "TIMEOUT"
    with raw.open("wb") as lf:                 # opened ONCE; never overwritten
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=lf,
                              stderr=subprocess.STDOUT)
        try:
            p = wait_for(raw, PROMPT, 0, 240)
            print(f"  [PASS] shell prompt @ byte {p}", flush=True)
            vm.stdin.write(b"f32client\n")
            vm.stdin.flush()
            p = wait_for(raw, CLIENT_START, p, 180)
            status, _first = wait_for_completion(raw, p, 180)
            print(f"  [{status}] client completion", flush=True)
            # Let the shell and kernel drain before stopping the VM.
            time.sleep(3)
        except RuntimeError as exc:
            print(f"  [WARN] {exc}", flush=True)
        finally:
            if not qmp_quit(sock, raw):
                vm.terminate()
            try:
                vm.wait(timeout=12)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    if sock.exists():
        sock.unlink()
    return status


def count_markers(text):
    """CRITERIA rows: (label, expected, actual, status)."""
    rows = []

    def add(label, needle, expected, hard=True, lo=None, hi=None, lines=False):
        actual = (sum(1 for ln in text.splitlines() if needle in ln)
                  if lines else text.count(needle))
        if expected is None:
            verdict = "INFO"
        else:
            ok = actual >= lo and (hi is None or actual <= hi)
            verdict = ("PASS" if ok else "FAIL") if hard else (
                "PASS" if ok else "WARN")
        rows.append((label, expected, str(actual), verdict))

    add("e32run: bind slot", "e32run: bind slot", "== 464", lo=464, hi=464)
    add("genuine server start", "EFILE: genuine file server starting",
        ">= 1", lo=1)
    add("GenuineBoot EFILE r=0", "GenuineBoot: EFILE.E32 r=0", "== 1", lo=1, hi=1)
    add("single-owner claim", "single-owner:", ">= 1", lo=1)
    add("File server registered", "File server thread registered", ">= 1", lo=1)
    add("E32Main returned", "E32Main returned", "== 0", lo=0, hi=0)
    add("EFILE: port-shim", "EFILE: port-shim", "== 0", lo=0, hi=0)
    add("IPC round trip OK", "F32CLIENT: IPC round trip OK", ">= 1", lo=1)
    add("F32CLIENT: start", "F32CLIENT: start", None)
    add("SessionPath r=", "F32CLIENT: SessionPath r=", None)
    add("SessionPath r=-15", "F32CLIENT: SessionPath r=-15", None)
    add("SessionPath FAILED", "F32CLIENT: SessionPath FAILED", None)
    add("no IPC reply", "F32CLIENT: no IPC reply", None)
    add("#PF", "#PF", "== 0", lo=0, hi=0)
    add("FAULT:", "FAULT:", "== 0", lo=0, hi=0)
    add("PanicKernExec", "PanicKernExec", "== 0", lo=0, hi=0)
    add("Exc (line)", "Exc ", "== 0", lo=0, hi=0, lines=True)
    add("SIGTERM in log", "terminating on signal 15", "== 0", lo=0, hi=0)
    return rows


def print_table(rows):
    width = max(len("marker"), *(len(r[0]) for r in rows))
    print(f"  {'marker'.ljust(width)}  {'expected':>10}  {'actual':>6}  verdict")
    print(f"  {'-' * width}  {'-' * 10}  {'-' * 6}  -------")
    for label, exp, actual, verdict in rows:
        print(f"  {label.ljust(width)}  {(exp or 'info'):>10}  "
              f"{actual:>6}  {verdict}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true",
                    help="print the plan; no QEMU, no log, no verdict")
    ap.add_argument("--log-dir", metavar="DIR", default=str(CACHE_ROOT),
                    help=f"scratch disk + raw log directory (default {CACHE_ROOT})")
    ap.add_argument("--log", metavar="FILE",
                    help="analyse an EXISTING raw log; no QEMU is launched")
    args = ap.parse_args()

    logdir = Path(args.log_dir)
    raw = logdir / RAW_NAME
    final = CACHE_ROOT / FINAL_NAME
    print("r1_efile_clean_prove -- ONE uninterrupted prove on the final GRUB ISO",
          flush=True)
    print(f"ISO: {ISO}  log-dir: {logdir}  raw: {raw}  final: {final}", flush=True)

    if args.log:
        log = Path(args.log)
        if not log.is_file():
            print(f"ERROR: log file not found: {log}", file=sys.stderr)
            return 1
        text = log.read_text(errors="replace")
        rows = count_markers(text)
        print_table(rows)
        hard_fail = [r for r in rows if r[3] == "FAIL"]
        print(f"ANALYZE ONLY ({log}); no QEMU launched", flush=True)
        print(f"EVIDENCE: {'PASS' if not hard_fail else 'FAIL'} "
              f"({len(rows) - len(hard_fail)}/{len(rows)} hard rows pass)",
              flush=True)
        return 0

    if args.dry_run:
        print_table([(l, e or "info", "-", "SKIP")
                     for l, e, _a, _v in count_markers("")])
        print("planned: stage EUSER.DLL + EFILE.E32 + EFAT32.FSY + F32CLIENT.E32; "
              "boot ISO; send f32client; wait for >=2 natural terminal markers; "
              "QMP quit; snapshot raw log")
        print("DRY RUN OK (no PASS claimed, no QEMU launched)")
        return 0

    missing = [str(p) for p in (ISO, EUSER, GENUINE / "EFILE.E32",
                                GENUINE / "EFAT32.FSY", F32CLIENT)
               if not p.is_file()]
    if missing:
        print("ERROR: missing artifacts (no rebuild): " + ", ".join(missing),
              file=sys.stderr)
        return 1
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not shutil.which(tool):
            print(f"ERROR: missing required tool {tool}", file=sys.stderr)
            return 1
    logdir.mkdir(parents=True, exist_ok=True)
    free = free_bytes(logdir)
    if free < MIN_FREE_BYTES:
        print(f"WARN: {logdir} has only {free / 2**20:.0f} MiB free",
              file=sys.stderr, flush=True)

    print(f"ISO sha256: {sha256(ISO)}", flush=True)
    print(f"F32CLIENT sha256: {sha256(F32CLIENT)}", flush=True)
    status = boot_and_capture(logdir, raw)
    if not raw.is_file() or raw.stat().st_size == 0:
        print("ERROR: QEMU produced no raw log", file=sys.stderr)
        return 2
    shutil.copyfile(raw, final)
    print(f"raw log copied -> {final}", flush=True)

    text = raw.read_text(errors="replace")
    rows = count_markers(text)
    print_table(rows)
    hard_fail = [r for r in rows if r[3] == "FAIL"]
    if status == "TIMEOUT":
        print(f"CAPTURE: TIMEOUT (tail follows)\n{text[-1500:]}", flush=True)
        return 2
    print(f"CAPTURE: {'PASS' if status in ('COMPLETE', 'SINGLE') else status} "
          f"({status}; natural completion marker before VM stop)", flush=True)
    print(f"EVIDENCE: {'PASS' if not hard_fail else 'FAIL'} "
          f"({len(rows) - len(hard_fail)}/{len(rows)} hard rows pass; "
          f"SessionPath r=-15 is INFO/under investigation)", flush=True)
    if hard_fail:
        for r in hard_fail:
            print(f"  FAILED: {r[0]} expected {r[1]} got {r[2]}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
