# HARNESS_FIX_STATUS2 — selfcheck harness race fix

**Verdict: LANDED** (uncommitted working-tree changes; `git diff --stat`: +169/-49 across the 3 files).

Evidence:
- `tools/r1_selftest.py` (mtime 2026-11-24 21:27): `selftest_lock()` — exclusive `fcntl.flock(LOCK_EX)` on `/tmp/opencode/r1_selftest.lock`, sets `R1_SELFTEST_LOCK_HELD=1` for children; per-PID rundir `/tmp/opencode/r1_selftest_<pid>`; `TERMINATOR = "terminating on signal"` + grace deadline at lines 149-158.
- `tools/r1_os_selfcheck.py`: same `selftest_lock()` (lines 29-50), wraps the `selftest` step (line 100), inherits lock via env handoff.
- `tools/r1_final_verify.py` (mtime 2026-11-24 03:04): same lock around the `selfcheck` gate, env handoff prevents nested deadlock.
- `python3 -m py_compile` on all 3: OK. `git status kernelhwsrv/`: clean (untouched).
