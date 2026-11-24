#!/usr/bin/env python3
"""r1_final_verify.py -- one command, full verification suite, compact table.

Runs the important R1 gates in a sensible order and prints a PASS/FAIL/SKIP
table. Intended to be run ONCE after in-flight fixes land, not during normal
development.

Default order (each entry is one subprocess, logged separately):

     1. r1_os_selfcheck.py       build + isogrub + smoke + cp4 + selftest
     2. r1_qemu_smoke.py         boot / EUSER / process-create markers
     3. r1_genuine_retire_test   staged genuine-shim retirement gates
     4. r1_genuine_efsrv_test    genuine F32 + !FileServer IPC
     5. r1_genuine_ewsrv_test    genuine !E32WindowServer
     6. r1_genuine_eshell_test   genuine ESTART / ESHELL
     7. r1_genuine_fat_io_test   genuine EFSRV against real FAT32 volume
     8. r1_doom_test.py          DOOM title screen (--no-build-iso)
     9. r1_doom_audio_test.py    DOOM + AC97 PCM audio (--no-build-iso)
    10. r1_doom_playtest.py      long-run playability (--no-build-iso)
    11. r1_doom_multikey_test.py multi-key hold / release (--no-build-iso)

FAST vs LONG: 1 is the full build+boot self-check and 3 boots QEMU several
times -- both are the slowest gates. 2, 4-7 are single-boot QEMU tests. The
DOOM tests are optional-ish: they SKIP (exit 0) with a clear RESULT: SKIP when
DOOM.E32 / FREEDOOM1.WAD / EUSER.DLL assets are missing. A SKIP is reported
but never makes the overall run fail unless --strict-skip is given.

ISO STAGING POLICY (never silently reuse a stale DOOM ISO): the orchestrator
never builds userland itself, it only invokes the scripts above. Gate 1 builds
build/symbian-grub.iso via `make isogrub`; the DOOM tests boot
/tmp/opencode/r1_doom.iso. Before ANY DOOM test runs, a staging step deletes any
stale /tmp/opencode/r1_doom.iso and replaces it with a FRESH copy of the current
build/symbian-grub.iso, so a final run cannot PASS against an old artifact.
`--rebuild-iso` first runs `make isogrub` here and then stages that fresh ISO.
If the build ISO is unavailable, the stale DOOM ISO is still deleted and the
DOOM tests drop `--no-build-iso` so each builds a fresh ISO itself.

DISK-SPACE GUARD: before any log is written, the free space on the log
filesystem is checked with shutil.disk_usage. If less than 1 GiB is free,
the DEFAULT log dir is switched to
/home/yuzi/.cache/sybian-r1/final-verify-<timestamp>/ (on the roomier home
filesystem) with a printed warning. An explicitly requested --log-dir is
honored (only a warning is printed), so a caller choosing a scratch dir is
never silently overridden. Per-test logs above 200 MB are rotated to their
last 2 MB (verdict tail preserved) with a warning. Nothing fills the disk
silently.

VERDICT PARSING (conservative): the LAST explicit `OVERALL: PASS|FAIL`,
`RESULT: PASS|FAIL|SKIP`, or `[PASS]/[FAIL] r1_genuine_*_test` line in the
captured output wins (greatest character offset). Only if no explicit verdict
exists is the process exit status used. HOWEVER a nonzero exit ALWAYS forces
FAIL even if a PASS/SKIP verdict was parsed: a script that prints PASS and then
crashes is a failure. A timeout is always a failure. NOT RUN rows (from
--no-keep-going) count as failures. SKIP is non-fatal unless --strict-skip.

Usage:
    python3 tools/r1_final_verify.py                 # run the default set
    python3 tools/r1_final_verify.py --dry-run       # show commands, run none
    python3 tools/r1_final_verify.py --only doom_main doom_audio
    python3 tools/r1_final_verify.py --skip doom_playtest doom_multikey
    python3 tools/r1_final_verify.py --skip-selfcheck
    python3 tools/r1_final_verify.py --no-keep-going  # stop on first failure
    python3 tools/r1_final_verify.py --strict-skip    # make SKIP fatal
"""
import argparse
import fcntl
import os
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
from collections import namedtuple
from contextlib import contextmanager, nullcontext

PORT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(PORT, "tools")
DEFAULT_LOGDIR = "/tmp/opencode"
CACHE_ROOT = "/home/yuzi/.cache/sybian-r1"   # roomier FS for auto-fallback
MIN_FREE_BYTES = 1 * 1024 ** 3              # 1 GiB floor on the log FS
LOG_CAP_BYTES = 200 * 1024 ** 2             # rotate a per-test log above 200 MB
LOG_KEEP_BYTES = 2 * 1024 ** 2              # keep the last 2 MB (verdict tail)

BUILD_ISO = os.path.join(PORT, "build", "symbian-grub.iso")
DOOM_ISO = "/tmp/opencode/r1_doom.iso"
REBUILD_ISO_TIMEOUT = 300

# Exclusive lock around the selfcheck gate (which runs r1_selftest.py, a QEMU
# boot): only one selftest may run at a time across concurrent invocations.
LOCK = "/tmp/opencode/r1_selftest.lock"


@contextmanager
def selftest_lock():
    """flock the selftest; no-op when a parent already holds it.

    A lock-holding parent exports R1_SELFTEST_LOCK_HELD=1, so the nested
    r1_os_selfcheck.py / r1_selftest.py does not deadlock on the same file.
    """
    if os.environ.get("R1_SELFTEST_LOCK_HELD") == "1":
        yield
        return
    os.makedirs(os.path.dirname(LOCK), exist_ok=True)
    fh = open(LOCK, "a+")
    try:
        fcntl.flock(fh.fileno(), fcntl.LOCK_EX)
        os.environ["R1_SELFTEST_LOCK_HELD"] = "1"   # inherited by children
        print(f"== selftest lock acquired: {LOCK}", flush=True)
        yield
    finally:
        try:
            fcntl.flock(fh.fileno(), fcntl.LOCK_UN)
        finally:
            fh.close()

# name, script (relative to qemu-port), extra argv, timeout seconds
Test = namedtuple("Test", "name script argv timeout")

DEFAULT_TESTS = [
    Test("selfcheck",       "tools/r1_os_selfcheck.py",            [], 900),
    Test("qemu_smoke",      "tools/r1_qemu_smoke.py",              [], 150),
    Test("genuine_retire",  "tools/r1_genuine_retire_test.py",     [], 600),
    Test("genuine_efsrv",   "tools/r1_genuine_efsrv_test.py",      [], 300),
    Test("genuine_ewsrv",   "tools/r1_genuine_ewsrv_test.py",      [], 300),
    Test("genuine_eshell",  "tools/r1_genuine_eshell_test.py",     [], 300),
    Test("genuine_fat_io",  "tools/r1_genuine_fat_io_test.py",     [], 300),
    Test("doom_main",       "tools/r1_doom_test.py",               ["--no-build-iso"], 240),
    Test("doom_audio",      "tools/r1_doom_audio_test.py",         ["--no-build-iso"], 240),
    Test("doom_playtest",   "tools/r1_doom_playtest.py",           ["--no-build-iso"], 240),
    Test("doom_multikey",   "tools/r1_doom_multikey_test.py",      ["--no-build-iso"], 360),
]

# A single explicit final verdict is authoritative, chosen by greatest
# character offset so a trailing "RESULT: FAIL" beats an early "[PASS]".
OVERALL_RE = re.compile(r"^\s*OVERALL:\s*(PASS|FAIL)\b", re.MULTILINE)
RESULT_RE = re.compile(r"^\s*RESULT:\s*(PASS|FAIL|SKIP)\b", re.MULTILINE)
SUMMARY_RE = re.compile(
    r"^\s*\[(PASS|FAIL)\]\s+r1_genuine_[a-z0-9_]+_test\b", re.MULTILINE)

BAD = {"FAIL", "TIMEOUT", "ERROR", "NOT RUN"}
GOOD = {"PASS", "SKIP"}


def is_failure(verdict, strict_skip=False):
    """True when a verdict should make the whole run fail."""
    return verdict in BAD or (strict_skip and verdict == "SKIP")


def failed_names(results, strict_skip=False):
    """Names of all results whose verdict is fatal for the run."""
    return [r["name"] for r in results if is_failure(r["verdict"], strict_skip)]


def explicit_verdict(text):
    """Return (verdict, detail) from the last explicit verdict line, or None."""
    hits = []
    for label, rx in (("OVERALL", OVERALL_RE), ("RESULT", RESULT_RE),
                      ("summary line", SUMMARY_RE)):
        for m in rx.finditer(text):
            hits.append((m.start(), m.group(1), label))
    if not hits:
        return None
    _, word, label = max(hits, key=lambda h: h[0])
    return word, f"{label} line: {word}"


def free_space_bytes(path):
    """Free bytes on the filesystem that would hold `path` (may not exist)."""
    probe = os.path.abspath(path)
    while not os.path.exists(probe):
        parent = os.path.dirname(probe)
        if parent == probe:
            break
        probe = parent
    return shutil.disk_usage(probe).free


def resolve_logdir(requested, explicit, dry_run=False):
    """Pick the log dir after a pre-flight free-space check.

    `requested` is the --log-dir value (or DEFAULT_LOGDIR); `explicit` says
    whether the caller actually passed --log-dir. Returns the effective dir.
    """
    if not explicit:
        requested = DEFAULT_LOGDIR
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

    stamp = time.strftime("%Y%m%dT%H%M%S%f")   # microseconds => unique
    alt = os.path.join(CACHE_ROOT, f"final-verify-{stamp}")
    print(f"WARN: default log dir {requested} has only {free_mib:.0f} MiB "
          f"free (< 1 GiB); auto-selecting {alt} instead",
          file=sys.stderr, flush=True)
    if not dry_run:
        try:
            os.makedirs(alt, exist_ok=True)
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
    try:
        size = os.path.getsize(logpath)
    except OSError:
        return
    if size <= LOG_CAP_BYTES:
        return
    try:
        with open(logpath, "rb") as logf:
            logf.seek(size - LOG_KEEP_BYTES)
            tail = logf.read(LOG_KEEP_BYTES)
        banner = (b"[r1_final_verify.py: log exceeded 200 MB; prefix "
                  b"discarded, last 2 MB kept]\n")
        with open(logpath, "wb") as logf:
            logf.write(banner)
            logf.write(tail)
        print(f"WARN: {logpath} was {size} bytes (> 200 MB); rotated to its "
              f"last 2 MB (verdict tail preserved)", file=sys.stderr,
              flush=True)
    except OSError as exc:
        print(f"WARN: could not rotate {logpath}: {exc}", file=sys.stderr,
              flush=True)


def command_for(test, doom_build_own=False):
    argv = list(test.argv)
    if doom_build_own and test.name.startswith("doom_"):
        argv = [a for a in argv if a != "--no-build-iso"]
    return [sys.executable, os.path.join(PORT, test.script)] + argv


def _remove_stale_doom_iso():
    """Delete any stale DOOM ISO so it can never be silently reused."""
    try:
        os.remove(DOOM_ISO)
    except OSError:
        pass


def stage_doom_iso(rebuild_iso, logdir, dry_run=False):
    """Prepare a FRESH /tmp/opencode/r1_doom.iso before the DOOM tests.

    Returns (doom_build_own, ok, detail).

    doom_build_own=True means the current build ISO was unavailable; the
    stale DOOM ISO was removed and the DOOM tests must build their own ISO
    (so --no-build-iso is dropped for them). ok=False means an explicitly
    requested `make isogrub` rebuild failed (a fatal error).
    """
    if dry_run:
        print("== STAGE DOOM ISO (fresh; stale ISO never reused):")
        if rebuild_iso:
            print(f"   rebuild: {shlex.join(['make', 'isogrub'])}  (cwd {PORT})")
        print(f"   stage:   cp -f {BUILD_ISO} {DOOM_ISO}")
        present = os.path.isfile(BUILD_ISO) or rebuild_iso
        if not present:
            print("   NOTE: build ISO absent; DOOM tests build fresh "
                  "(--no-build-iso dropped)")
        return not present, True, "dry-run"

    if rebuild_iso:
        print(f"== REBUILD ISO: make isogrub (cwd {PORT})", flush=True)
        logpath = os.path.join(logdir, "final_verify_isogrub.log")
        os.makedirs(logdir, exist_ok=True)
        rc = 127
        with open(logpath, "w", errors="replace") as logf:
            try:
                proc = subprocess.Popen(
                    ["make", "isogrub"], cwd=PORT, stdout=logf,
                    stderr=subprocess.STDOUT, start_new_session=True)
            except OSError as exc:
                detail = f"could not start make isogrub: {exc}"
                print(f"ERROR: {detail}", file=sys.stderr, flush=True)
                _remove_stale_doom_iso()
                return True, False, detail
            try:
                rc = proc.wait(timeout=REBUILD_ISO_TIMEOUT)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.wait()
                rc = 124
        cap_log(logpath)
        if rc != 0:
            detail = f"make isogrub failed (rc {rc}); see {logpath}"
            print(f"ERROR: {detail}", file=sys.stderr, flush=True)
            _remove_stale_doom_iso()
            return True, False, detail

    # Drop the stale ISO first so no DOOM test can accidentally reuse it.
    _remove_stale_doom_iso()
    if os.path.isfile(BUILD_ISO):
        shutil.copyfile(BUILD_ISO, DOOM_ISO)
        print(f"== STAGED DOOM ISO: {BUILD_ISO} -> {DOOM_ISO}", flush=True)
        return False, True, "staged fresh build ISO"
    detail = f"build ISO {BUILD_ISO} absent; DOOM tests build fresh"
    print(f"== WARN {detail}", file=sys.stderr, flush=True)
    return True, True, detail


def run_test(test, logdir, doom_build_own=False):
    """Run one test in its own process group; return a result dict."""
    cmd = command_for(test, doom_build_own)
    logpath = os.path.join(logdir, f"final_verify_{test.name}.log")
    print(f"== RUN {test.name}: {shlex.join(cmd)} (timeout {test.timeout}s)",
          flush=True)
    started = time.monotonic()
    os.makedirs(logdir, exist_ok=True)
    rc = None
    timed_out = False
    error = None
    # The selfcheck contains the selftest (a QEMU boot): hold the exclusive
    # selftest lock around it so only one selftest runs at a time.  The env
    # handoff makes the lock a no-op in the nested r1_selftest.py.
    with (selftest_lock() if test.name == "selfcheck" else nullcontext()):
        with open(logpath, "w", errors="replace") as logf:
            try:
                proc = subprocess.Popen(
                    cmd, cwd=PORT, stdout=logf, stderr=subprocess.STDOUT,
                    start_new_session=True)
            except OSError as exc:
                error = f"could not start: {exc}"
            else:
                try:
                    rc = proc.wait(timeout=test.timeout)
                except subprocess.TimeoutExpired:
                    timed_out = True
                    try:
                        os.killpg(proc.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    proc.wait()
    elapsed = time.monotonic() - started

    # Rotate an oversized log before reading it, so verdict parsing never
    # slurps a multi-GB trace into memory. The tail holds the verdict lines.
    cap_log(logpath)

    if error:
        verdict, detail = "ERROR", error
    elif timed_out:
        verdict, detail = "TIMEOUT", f"exceeded {test.timeout}s (killed)"
    else:
        with open(logpath, "r", errors="replace") as logf:
            text = logf.read()
        found = explicit_verdict(text)
        if found is not None:
            verdict, detail = found
            # A process that prints a verdict and then crashes is a FAIL,
            # never a PASS/SKIP. Last-by-offset parsing is still preserved.
            if rc != 0 and verdict in ("PASS", "SKIP"):
                verdict = "FAIL"
                detail = f"{detail} (overridden: exit status {rc})"
        elif rc == 0:
            verdict, detail = "PASS", "exit status 0 (no explicit verdict)"
        else:
            verdict, detail = "FAIL", f"exit status {rc} (no explicit verdict)"
    return dict(name=test.name, verdict=verdict, detail=detail,
                elapsed=elapsed, log=logpath, rc=rc)


def print_table(results):
    headers = ("TEST", "VERDICT", "TIME", "LOG")
    rows = [(r["name"], r["verdict"], f'{r["elapsed"]:.1f}s', r["log"])
            for r in results]
    widths = [max(len(headers[i]), *(len(row[i]) for row in rows))
              for i in range(4)]
    fmt = "  ".join("{:<" + str(w) + "}" for w in widths)
    print()
    print(fmt.format(*headers))
    print(fmt.format(*("-" * w for w in widths)))
    for row in rows:
        print(fmt.format(*row))
    notes = [r for r in results if r["verdict"] != "PASS"]
    if notes:
        print()
        for r in notes:
            print(f'  note {r["name"]}: {r["verdict"]} -- {r["detail"]}')


def build_parser():
    ap = argparse.ArgumentParser(
        description="Run the full R1 verification suite in order.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="See module docstring for the default order and policies.")
    ap.add_argument("--only", action="extend", nargs="+", default=[],
                    metavar="NAME",
                    help="run only NAME(s) (repeatable); overrides defaults")
    ap.add_argument("--skip", action="extend", nargs="+", default=[],
                    metavar="NAME", help="remove NAME(s) from the set (repeatable)")
    ap.add_argument("--skip-selfcheck", action="store_true",
                    help="alias for --skip selfcheck")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the commands and exit without running anything")
    ap.add_argument("--keep-going", dest="keep_going", action="store_true",
                    default=True, help="continue after a required failure (default)")
    ap.add_argument("--no-keep-going", dest="keep_going", action="store_false",
                    help="stop after the first required failure")
    ap.add_argument("--rebuild-iso", action="store_true",
                    help="run `make isogrub` then stage a fresh DOOM ISO "
                         "(default: stage the existing build ISO without rebuilding)")
    ap.add_argument("--strict-skip", action="store_true",
                    help="treat a RESULT: SKIP verdict as a failure")
    ap.add_argument("--log-dir", default=None, metavar="DIR",
                    help=f"where per-test logs go (default {DEFAULT_LOGDIR}); "
                         f"the default is auto-moved to {CACHE_ROOT}/ if "
                         f"less than 1 GiB is free, but an explicit value "
                         f"is always honored")
    ap.add_argument("--list", action="store_true",
                    help="list the default tests in order and exit")
    return ap


def main():
    args = build_parser().parse_args()
    valid = [t.name for t in DEFAULT_TESTS]

    if args.list:
        for i, t in enumerate(DEFAULT_TESTS, 1):
            print(f"{i:2d}. {t.name:<18} {t.script} {shlex.join(t.argv)}")
        return 0

    bad_names = [n for n in args.only + args.skip if n not in valid]
    if bad_names:
        print(f"ERROR: unknown test name(s): {', '.join(bad_names)}",
              file=sys.stderr)
        print(f"valid names: {', '.join(valid)}", file=sys.stderr)
        return 2

    if args.only:
        chosen = [t for t in DEFAULT_TESTS if t.name in args.only]
    else:
        chosen = list(DEFAULT_TESTS)
    skipped = set(args.skip)
    if args.skip_selfcheck:
        skipped.add("selfcheck")
    chosen = [t for t in chosen if t.name not in skipped]

    if not chosen:
        print("ERROR: no tests selected", file=sys.stderr)
        return 2

    # Pre-flight: never start a log-writing step on a nearly-full filesystem.
    logdir = resolve_logdir(args.log_dir, args.log_dir is not None,
                            dry_run=args.dry_run)

    doom_selected = any(t.name.startswith("doom_") for t in chosen)
    doom_build_own = False
    if doom_selected:
        doom_build_own, stage_ok, stage_detail = stage_doom_iso(
            args.rebuild_iso, logdir, dry_run=args.dry_run)
        if not stage_ok:
            print(f"ERROR: ISO staging failed: {stage_detail}", file=sys.stderr)
            return 1

    if args.dry_run:
        print(f"DRY RUN: {len(chosen)} command(s), no processes started, "
              f"no logs written:")
        for i, t in enumerate(chosen, 1):
            print(f"{i:2d}. {shlex.join(command_for(t, doom_build_own))}")
        print(f"\nLogs would go to {logdir}/final_verify_<name>.log")
        return 0

    results = []
    for t in chosen:
        res = run_test(t, logdir, doom_build_own)
        results.append(res)
        print(f'-- {t.name}: {res["verdict"]} ({res["elapsed"]:.1f}s) '
              f'{res["detail"]}', flush=True)
        if is_failure(res["verdict"], args.strict_skip) and not args.keep_going:
            for rest in chosen[len(results):]:
                results.append(dict(name=rest.name, verdict="NOT RUN",
                                    detail="stopped by --no-keep-going",
                                    elapsed=0.0, log="-", rc=None))
            break

    print_table(results)
    failed = failed_names(results, args.strict_skip)
    overall = "FAIL" if failed else "PASS"
    print(f"\nOVERALL: {overall}  "
          f"({sum(1 for r in results if r['verdict'] == 'PASS')} pass, "
          f"{sum(1 for r in results if r['verdict'] == 'SKIP')} skip, "
          f"{len(failed)} fail/not-run)")
    if failed:
        print("failed: " + ", ".join(failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
