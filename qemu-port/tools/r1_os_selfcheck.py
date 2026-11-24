#!/usr/bin/env python3
"""r1_os_selfcheck.py — one-command full OS verification for the EKA2 PC port.

Runs, in order and with no shell wrappers of its own:

  1. make build/kernfull.elf userland   (fail fast if this breaks)
  2. make isogrub                       (builds build/symbian-grub.iso)
  3. python3 tools/r1_qemu_smoke.py
  4. python3 tools/r1_cp4_ca_test.py
  5. python3 tools/r1_cp4_ca_stress_test.py
  6. python3 tools/r1_selftest.py

Every step is reported as [PASS] or [FAIL] with a reason; the final line is
OVERALL: PASS / OVERALL: FAIL.  The exit status is 0 only when every step
passed.  Usage: python3 tools/r1_os_selfcheck.py
"""
import fcntl
import os
import re
import shlex
import signal
import subprocess
import sys
import time
from contextlib import contextmanager, nullcontext

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LOGDIR = "/tmp/opencode"
# Exclusive lock around the selftest step (it boots QEMU): only one selftest
# may run at a time, even across concurrent selfcheck/final-verify runs.
LOCK = "/tmp/opencode/r1_selftest.lock"


@contextmanager
def selftest_lock():
    """flock the selftest; no-op when a parent already holds it.

    A lock-holding parent exports R1_SELFTEST_LOCK_HELD=1, so the nested
    r1_selftest.py (and this step, if re-entered under it) does not deadlock
    on the same lock file.
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

BANNER = "EKA2 PC OS — single-command selfcheck"

# (name, argv, timeout_seconds, fail_fast)
STEPS = [
    ("build", ["make", "build/kernfull.elf", "userland"], 120, True),
    ("isogrub", ["make", "isogrub"], 120, False),
    ("qemu_smoke", ["python3", "tools/r1_qemu_smoke.py"], 70, False),
    ("cp4_ca", ["python3", "tools/r1_cp4_ca_test.py"], 70, False),
    ("cp4_stress", ["python3", "tools/r1_cp4_ca_stress_test.py"], 90, False),
    ("selftest", ["python3", "tools/r1_selftest.py"], 110, False),
]

# The four QEMU scripts all end with "RESULT: PASS" / "RESULT: FAIL", except
# the stress test, which prints a bare "PASS: ..." / "FAIL: ..." line.
PASS_LINE = re.compile(r"^\s*(?:RESULT:\s*)?PASS\b", re.MULTILINE)
FAIL_LINE = re.compile(r"^\s*(?:RESULT:\s*)?FAIL\b", re.MULTILINE)


def verdict(text):
    """Return (passed, detail) inferred from a sub-test's own final verdict."""
    has_pass = bool(PASS_LINE.search(text))
    has_fail = bool(FAIL_LINE.search(text))
    if has_fail:
        return False, "script reported FAIL"
    if has_pass:
        return True, "script reported PASS"
    return False, "no PASS verdict in output"


def tail(text, n=15):
    lines = [ln for ln in text.splitlines() if ln.strip()]
    if not lines:
        return "(no output)"
    return " | ".join(lines[-n:])


def run_step(name, cmd, timeout):
    """Run one command in its own process group; return (ok, reason, elapsed)."""
    print(f"== RUN {name}: {shlex.join(cmd)}  (timeout {timeout}s)", flush=True)
    started = time.monotonic()
    # The selftest boots QEMU; serialize it with the exclusive selftest lock
    # so concurrent selfchecks cannot truncate each other's artifacts.
    with (selftest_lock() if name == "selftest" else nullcontext()):
        try:
            proc = subprocess.Popen(
                cmd, cwd=ROOT,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, errors="replace", start_new_session=True,
            )
        except OSError as exc:
            return False, f"could not start command: {exc}", time.monotonic() - started

        timed_out = False
        try:
            out, _ = proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            # Kill the whole process group so no QEMU/timeout grandchildren survive.
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except ProcessLookupError:
                pass
            out, _ = proc.communicate()
    elapsed = time.monotonic() - started
    rc = proc.returncode

    # Keep the full output for post-mortem debugging.  Per-PID filename so
    # two concurrent selfchecks never overwrite each other's step logs.
    logpath = os.path.join(LOGDIR, f"r1_os_selfcheck_{os.getpid()}_{name}.log")
    try:
        with open(logpath, "w", errors="replace") as fh:
            fh.write(out)
    except OSError:
        logpath = "(unwritable)"

    if timed_out:
        return False, f"timed out after {timeout}s; tail: {tail(out)}", elapsed

    if name in ("build", "isogrub"):
        ok = rc == 0
        reason = ("exit 0" if ok else f"exit {rc}; tail: {tail(out)}")
    else:
        ok = (rc == 0) and verdict(out)[0]
        vdetail = verdict(out)[1]
        reason = (f"exit 0, {vdetail}" if ok
                  else f"exit {rc}, {vdetail}; tail: {tail(out)}")
    return ok, reason, elapsed


def main():
    print(BANNER, flush=True)
    print(f"workspace: {ROOT}", flush=True)

    results = []       # (name, ok, reason, elapsed) in execution order
    skipped = []       # (name, reason)
    failures = []      # (name, reason)
    abort = False
    started_all = time.monotonic()

    for name, cmd, timeout, fail_fast in STEPS:
        if abort:
            skipped.append((name, "skipped: build failed (fail-fast)"))
            print(f"[SKIP] {name} (build failed, fail-fast)", flush=True)
            continue

        ok, reason, elapsed = run_step(name, cmd, timeout)
        results.append((name, ok, reason, elapsed))
        print(f"[{'PASS' if ok else 'FAIL'}] {name} ({elapsed:.1f}s): {reason}",
              flush=True)
        if ok:
            continue

        failures.append((name, reason))
        if fail_fast:
            print("!! build failed — aborting remaining steps (fail-fast)",
                  flush=True)
            abort = True

    total = time.monotonic() - started_all

    print("\n== summary ==")
    if failures:
        print(f"OVERALL: FAIL — {len(failures)} step(s) failed:")
        for name, reason in failures:
            print(f"  - {name}: {reason}")
        for name, reason in skipped:
            print(f"  - {name}: {reason}")
    else:
        print("OVERALL: PASS")
    print(f"total elapsed: {total:.1f}s")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
