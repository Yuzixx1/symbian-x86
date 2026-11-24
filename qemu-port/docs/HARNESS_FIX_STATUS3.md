# HARNESS_FIX_STATUS3 — selfcheck race fix: LANDED

- All 3 tools touched at 21:27:53–21:28:22 (fix agent ran ~21:27): `r1_selftest.py`, `r1_os_selfcheck.py`, `r1_final_verify.py`; `git status` shows all three modified (+169/−49 lines).
- Fix present in all 3: `fcntl.flock` LOCK_EX on `/tmp/opencode/r1_selftest.lock` (lock file exists, created 21:28), inherited/no-op when a parent holds it; `os.killpg(..., SIGKILL)` cleanup; `r1_selftest.py` detects QEMU's own `terminating on signal` line with a grace deadline.
- `python3 -m py_compile` on all 3: OK.
- `kernelhwsrv/`: untouched (git status clean). No QEMU run.
