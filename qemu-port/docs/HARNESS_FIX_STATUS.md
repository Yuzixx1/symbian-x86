# HARNESS_FIX_STATUS — selfcheck race fix (per-PID logs + flock)

**Verdict: NOT STARTED** (checked 2026-11-24, read-only).

- `tools/r1_os_selfcheck.py` (mtime 2026-11-20): logs to fixed `/tmp/opencode/r1_os_selfcheck_<name>.log`; no getpid/flock.
- `tools/r1_selftest.py` (mtime 2026-11-20): fixed `/tmp/opencode/r1_selftest.log`; no getpid/flock.
- `tools/r1_final_verify.py` (mtime 2026-11-24 03:04, content unchanged since 7f76900): fixed `final_verify_<name>.log`; no getpid/flock.
- `grep -r 'flock|fcntl|LOCK_EX'` over `qemu-port/tools/`: 0 matches. No working-tree diff vs HEAD; all 3 files py_compile OK. kernelhwsrv/ untouched.
