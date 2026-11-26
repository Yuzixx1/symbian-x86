# HARNESS_FIX_STATUS — selfcheck race fix (per-PID logs + flock)

**Verdict: NOT STARTED** (checked 2026-11-24, read-only).

- `tools/r1_os_selfcheck.py` (mtime 2026-11-20): logs to fixed `/tmp/opencode/r1_os_selfcheck_<name>.log`; no getpid/flock.
- `tools/r1_selftest.py` (mtime 2026-11-20): fixed `/tmp/opencode/r1_selftest.log`; no getpid/flock.
- `tools/r1_final_verify.py` (mtime 2026-11-24 03:04, content unchanged since 7f76900): fixed `final_verify_<name>.log`; no getpid/flock.
- `grep -r 'flock|fcntl|LOCK_EX'` over `qemu-port/tools/`: 0 matches. No working-tree diff vs HEAD; all 3 files py_compile OK. kernelhwsrv/ untouched.

**2026-11-26 — correction:** line 3's `NOT STARTED` status is reversed — the race fix landed in all three scripts: `tools/r1_selftest.py`/`tools/r1_os_selfcheck.py`/`tools/r1_final_verify.py` now use per-PID paths (`os.getpid()`: `r1_selftest_{os.getpid()}`, `r1_os_selfcheck_{os.getpid()}_{name}.log`) plus `fcntl.flock(... LOCK_EX)` guards, and the 13-row gate is **13/13 PASS** (`/tmp/opencode/final13_gate.log`, `OVERALL: PASS  (13 pass, 0 skip, 0 fail/not-run)`, `FINAL13_RC=0`). Body above unchanged; `kernelhwsrv/` untouched.

**2026-11-26 (later — 15-row gate) — supersedes the 13-row figure above:** the strict gate is now **15/15 PASS** (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`), the two added rows being `g12_negative` (`tools/r1_g12_negative_test.py`) and `hal_probe` (`tools/r1_genuine_hal_test.py`). The per-PID + `fcntl.flock` race fix above is unchanged; body above unchanged, `kernelhwsrv/` untouched.
