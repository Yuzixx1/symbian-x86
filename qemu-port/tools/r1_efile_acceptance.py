#!/usr/bin/env python3
"""r1_efile_acceptance.py -- strict acceptance checker for the final
EFILE/EFSRV fix (docs/EFILE_CONVERGENCE.md section 2).

This is a *verifier*, not a fixer.  It either

  * boots the port once (staging genuine EFILE.E32 + F32CLIENT.E32 so that
    GenuineBootLaunch() auto-launches the genuine server, then driving the
    F32CLIENT probe from the serial shell), capturing the raw serial log to
    /tmp/opencode/efile_acceptance.log; or
  * with --log FILE, analyses an existing raw serial log and runs NO QEMU.

Every criterion from the convergence doc is counted independently from ONE
complete raw log and printed as (criterion, expected, actual, PASS/FAIL).
The exit status is 0 only if ALL criteria pass; otherwise it is 1 and the
failed criteria are listed.  Counts are never fabricated and a missing or
unreadable log can never produce a PASS.

Hard rules:
  * NO source edits, NO rebuild.  kernelhwsrv/ is READ-ONLY.  A scratch FAT
    disk image and the serial log are written only under the --log-dir path
    (default /tmp/opencode).
  * No QEMU is launched in --dry-run or --log mode.
  * Disk-space guard: before any log is written, free space on the log
    filesystem is checked.  If less than 1 GiB is free, the DEFAULT log dir
    is switched to /home/yuzi/.cache/sybian-r1/efile-acceptance-<timestamp>/
    with a warning; an explicit --log-dir is honored with a warning instead.
    Any log above 200 MB is rotated to its last 2 MB before parsing, so a
    multi-GB serial trace can never fill tmpfs or be slurped into memory.

Usage:
    python3 tools/r1_efile_acceptance.py --dry-run
    python3 tools/r1_efile_acceptance.py                 # boot once, then check
    python3 tools/r1_efile_acceptance.py --log /tmp/opencode/efile_acceptance.log
    python3 tools/r1_efile_acceptance.py --log-dir /tmp/opencode --log my.log
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path

PORT = Path(__file__).resolve().parent.parent
KERNEL = PORT / "build/kernfull.elf"
GENUINE_BUILD = PORT / "genuine/build"
EUSER_DLL = Path("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll")
F32CLIENT = PORT / "genuine/f32_probe/F32CLIENT.E32"

LOGDIR = Path("/tmp/opencode")
DEFAULT_LOG = LOGDIR / "efile_acceptance.log"
CACHE_ROOT = Path("/home/yuzi/.cache/sybian-r1")  # roomier FS for auto-fallback
MIN_FREE_BYTES = 1 * 1024 ** 3             # 1 GiB floor on the log FS
LOG_CAP_BYTES = 200 * 1024 ** 2            # rotate a log above 200 MB
LOG_KEEP_BYTES = 2 * 1024 ** 2             # keep the last 2 MB (verdict tail)
PROMPT = b"C:/> "
BOOT_MARKER = b"KERN spawning genuine boot DThread"
IPC_MARKER = b"F32CLIENT: IPC round trip OK"

# Guest-fault keys.  Kept identical to r1_genuine_retire_test.py / the
# convergence doc ("Exc / PanicKernExec / FAULT == 0").
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512

# (key, label, needle, lo, hi) -- hi=None means ">= lo".  Counts come from ONE
# complete raw serial log; harness stdout repeats markers and overcounts.
CRITERIA = [
    ("start",      "EFILE: genuine file server starting", "EFILE: genuine file server starting", 1, 1),
    ("registered", "File server thread registered",       "File server thread registered",       1, 1),
    ("returned",   "E32Main returned",                     "E32Main returned",                    0, 0),
    ("pf",         "#PF",                                  "#PF",                                 0, 0),
    ("portshim",   "EFILE: port-shim",                     "EFILE: port-shim",                    0, 0),
    ("ipc",        "F32CLIENT: IPC round trip OK",         "F32CLIENT: IPC round trip OK",        1, None),
    ("sessionpath","SessionPath r=-15",                    "SessionPath r=-15",                   0, 0),
    ("exc",        "Exc/PanicKernExec/FAULT",              None,                                  0, 0),
    ("boot",       "GenuineBoot: EFILE.E32 r=0",           "GenuineBoot: EFILE.E32 r=0",          1, 1),
]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def count_of(text, c):
    """Return the observed count for one criterion from a raw log string."""
    key, _label, needle, _lo, _hi = c
    if key == "exc":
        return sum(1 for ln in text.splitlines()
                   if any(k in ln for k in EXC_KEYS))
    return text.count(needle)


def expected_str(lo, hi):
    if hi is None:
        return f">= {lo}"
    if lo == hi:
        return f"== {lo}"
    return f"{lo}..{hi}"


def verdict_for(c, actual):
    _key, _label, _needle, lo, hi = c
    return actual >= lo and (hi is None or actual <= hi)


def print_table(rows, note=""):
    """rows = list of (label, expected, actual_text, verdict_text)."""
    width = max(len("criterion"), *(len(r[0]) for r in rows))
    print(f"  {'criterion'.ljust(width)}  {'expected':>9}  {'actual':>7}  verdict")
    print(f"  {'-' * width}  {'-' * 9}  {'-' * 7}  -------")
    for label, exp, actual, verdict in rows:
        # Defensive: a None expected/actual cell must never crash the verdict
        # table (cf. r1_efile_clean_prove.py TypeError: NoneType.__format__).
        # str() is a no-op for the normal string cells, so output/semantics are
        # unchanged; it only prevents the format crash on a None value.
        print(f"  {label.ljust(width)}  {str(exp):>9}  {str(actual):>7}  {verdict}")
    if note:
        print(f"  ({note})")


# ------------------------------------------------------- disk-space guard ---
def free_space_bytes(path):
    """Free bytes on the filesystem that would hold `path` (may not exist)."""
    probe = os.path.abspath(os.fspath(path))
    while not os.path.exists(probe):
        parent = os.path.dirname(probe)
        if parent == probe:
            break
        probe = parent
    return shutil.disk_usage(probe).free


def resolve_logdir(requested, explicit, dry_run=False):
    """Pick the log dir after a pre-flight free-space check.

    `requested` is the --log-dir value (or None); `explicit` says whether the
    caller actually passed --log-dir. Returns the effective dir as a Path.
    Mirrors r1_final_verify.resolve_logdir().
    """
    if not explicit or requested is None:
        requested = LOGDIR
    requested = Path(requested)
    try:
        free = free_space_bytes(requested)
    except OSError as exc:
        print(f"WARN: could not check free space for {requested}: {exc}; "
              f"using it as-is", file=sys.stderr, flush=True)
        return requested

    if free >= MIN_FREE_BYTES:
        return requested

    free_mib = free / (1024 ** 2)
    if explicit:
        # A caller-chosen directory is never silently replaced.
        print(f"WARN: log dir {requested} has only {free_mib:.0f} MiB free "
              f"(< 1 GiB); honoring explicit --log-dir, disk may fill",
              file=sys.stderr, flush=True)
        return requested

    # strftime %f is literal on Linux, so append a nanosecond fraction to make
    # the directory genuinely unique (even for same-second runs).
    stamp = time.strftime("%Y%m%dT%H%M%S") + f"-{time.time_ns() % 1_000_000_000:09d}"
    alt = CACHE_ROOT / f"efile-acceptance-{stamp}"
    print(f"WARN: default log dir {requested} has only {free_mib:.0f} MiB "
          f"free (< 1 GiB); auto-selecting {alt} instead",
          file=sys.stderr, flush=True)
    if not dry_run:
        try:
            alt.mkdir(parents=True, exist_ok=True)
        except OSError as exc:
            print(f"WARN: could not create {alt}: {exc}; "
                  f"falling back to {requested}", file=sys.stderr, flush=True)
            return requested
        try:
            alt_free = free_space_bytes(alt)
            if alt_free < MIN_FREE_BYTES:
                print(f"WARN: fallback dir {alt} also has only "
                      f"{alt_free / (1024 ** 2):.0f} MiB free",
                      file=sys.stderr, flush=True)
        except OSError:
            pass
    return alt


def cap_log(logpath):
    """Rotate an oversized log to its last 2 MB, keeping the verdict tail."""
    logpath = Path(logpath)
    try:
        size = logpath.stat().st_size
    except OSError:
        return
    if size <= LOG_CAP_BYTES:
        return
    try:
        with logpath.open("rb") as logf:
            logf.seek(size - LOG_KEEP_BYTES)
            tail = logf.read(LOG_KEEP_BYTES)
        banner = (b"[r1_efile_acceptance.py: log exceeded 200 MB; prefix "
                  b"discarded, last 2 MB kept]\n")
        with logpath.open("wb") as logf:
            logf.write(banner)
            logf.write(tail)
        print(f"WARN: {logpath} was {size} bytes (> 200 MB); rotated to its "
              f"last 2 MB (verdict tail preserved)", file=sys.stderr,
              flush=True)
    except OSError as exc:
        print(f"WARN: could not rotate {logpath}: {exc}", file=sys.stderr,
              flush=True)


# --------------------------------------------------------------- runtime ---
def wait_for(path, needle, pos=0, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(path, "rb") as f:
            f.seek(pos)
            data = f.read()
        i = data.find(needle)
        if i >= 0:
            return pos + i + len(needle)
        time.sleep(0.2)
    raise RuntimeError(f"serial log missing {needle!r} (see {path})")


def stage_disk(logdir):
    """Stage genuine boot artifacts; NO rebuild, scratch disk only in logdir."""
    disk, fs = logdir / "efile_acceptance.img", logdir / "efile_acceptance_fs.img"
    for p in (disk, fs):
        if p.exists():
            p.unlink()
    disk.write_bytes(b"\0" * (MB * 1024 * 1024))
    fs.write_bytes(b"\0" * FS_SIZE)
    sh("mkfs.fat", "-F", "32", fs, check=True, stdout=subprocess.DEVNULL)
    server = GENUINE_BUILD / "EFILE.E32"
    fsy = GENUINE_BUILD / "EFAT32.FSY"
    files = [(EUSER_DLL, "EUSER.DLL"), (server, "EFILE.E32"),
             (fsy, "EFAT32.FSY"), (F32CLIENT, "F32CLIENT.E32")]
    for src, name in files:
        if not Path(src).is_file():
            sys.exit(f"missing staged artifact {src}")
        sh("mcopy", "-i", fs, src, f"::{name}", check=True)
    data = fs.read_bytes()
    with disk.open("r+b") as f:
        f.seek(OFFSET)
        f.write(data)
        mbr = bytearray(512)
        mbr[446:462] = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                        + LBA.to_bytes(4, "little") + SECTORS.to_bytes(4, "little"))
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)
    return disk


def run_qemu(log, logdir):
    """Boot once, drive F32CLIENT, return nothing (log file is the artifact)."""
    disk = stage_disk(logdir)
    if log.exists():
        log.unlink()
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-netdev", "user,id=n0", "-device", "e1000,netdev=n0",
        "-audiodev", "none,id=a0", "-device", "AC97,audiodev=a0",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-kernel", str(KERNEL),
        "-drive", f"file={disk},format=raw,snapshot=on,if=none,id=d0",
        "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
        "-monitor", "none", "-no-reboot",
    ]
    print(f"== booting QEMU for EFILE acceptance; log -> {log}", flush=True)
    with log.open("wb") as lf:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=lf,
                              stderr=subprocess.STDOUT)
        try:
            wait_for(log, BOOT_MARKER, 0, 90)
            pos = wait_for(log, PROMPT, 0, 90)
            # Genuine EFILE is auto-launched by GenuineBootLaunch() during
            # Init3; only the probe client is launched from the shell.
            vm.stdin.write(b"f32client\n")
            vm.stdin.flush()
            try:
                wait_for(log, IPC_MARKER, pos, 60)
            except RuntimeError as exc:
                print(f"  [WARN] {exc}", flush=True)
        except RuntimeError as exc:
            print(f"  [WARN] boot: {exc}", flush=True)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=5)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()


# ------------------------------------------------------------------- main ---
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true",
                    help="print criteria + planned command; no QEMU, no log analysis")
    ap.add_argument("--log", metavar="FILE",
                    help="analyse an EXISTING raw serial log (FILE); no QEMU is run")
    ap.add_argument("--log-dir", default=None, metavar="DIR",
                    help=f"scratch disk + default log directory (default {LOGDIR}); "
                         f"the default is auto-moved to {CACHE_ROOT}/ if less "
                         f"than 1 GiB is free, but an explicit value is always honored")
    args = ap.parse_args()

    # Pre-flight: never write a log on a nearly-full filesystem.  An explicit
    # --log-dir is honored with a warning; the default is auto-redirected to
    # the roomier cache filesystem.
    logdir = resolve_logdir(args.log_dir, args.log_dir is not None,
                            dry_run=args.dry_run)
    log = Path(args.log) if args.log else logdir / DEFAULT_LOG.name

    print("r1_efile_acceptance -- strict EFILE/EFSRV acceptance "
          "(docs/EFILE_CONVERGENCE.md section 2)", flush=True)
    print(f"log: {log}  log-dir: {logdir}  dry-run={args.dry_run}  "
          f"analyse-only={bool(args.log)}", flush=True)

    if args.dry_run:
        rows = [(label, expected_str(lo, hi), "-", "SKIP (dry-run)")
                for _k, label, _n, lo, hi in CRITERIA]
        print_table(rows, "no QEMU and no log analysis: no verdict issued")
        print("planned: stage genuine EFILE.E32 + F32CLIENT.E32, boot once, "
              "drive f32client, capture raw log")
        print("DRY RUN OK (exit 0 = plan printed; no PASS claimed)")
        return 0

    if args.log:
        if not log.is_file():
            print(f"ERROR: log file not found: {log}", file=sys.stderr)
            return 1
        cap_log(log)  # rotate an oversized log before slurping it into memory
        text = log.read_text(errors="replace")
    else:
        missing = [str(p) for p in (KERNEL, EUSER_DLL, F32CLIENT,
                                    GENUINE_BUILD / "EFILE.E32",
                                    GENUINE_BUILD / "EFAT32.FSY") if not p.is_file()]
        if missing:
            print("ERROR: missing artifacts (no rebuild will be done): "
                  + ", ".join(missing), file=sys.stderr)
            return 1
        for tool in ("qemu-system-i386", "mkfs.fat", "mcopy"):
            if not shutil.which(tool):
                print(f"ERROR: missing required tool {tool}", file=sys.stderr)
                return 1
        logdir.mkdir(parents=True, exist_ok=True)
        run_qemu(log, logdir)
        if not log.is_file():
            print(f"ERROR: QEMU produced no log at {log}", file=sys.stderr)
            return 1
        cap_log(log)  # rotate an oversized log before slurping it into memory
        text = log.read_text(errors="replace")

    rows, failed = [], []
    for c in CRITERIA:
        key, label, _needle, lo, hi = c
        actual = count_of(text, c)
        ok = verdict_for(c, actual)
        rows.append((label, expected_str(lo, hi), str(actual),
                     "PASS" if ok else "FAIL"))
        if not ok:
            failed.append(f"{label} (expected {expected_str(lo, hi)}, got {actual})")
    print_table(rows, "counts from ONE complete raw serial log")

    passed = len(CRITERIA) - len(failed)
    if failed:
        print(f"RESULT: FAIL ({passed}/{len(CRITERIA)} criteria pass)", flush=True)
        print("Failed criteria:")
        for f in failed:
            print("  - " + f)
        return 1
    print(f"RESULT: PASS ({passed}/{len(CRITERIA)} criteria pass)", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
