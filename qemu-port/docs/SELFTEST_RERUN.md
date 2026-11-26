# SELFTEST_RERUN — 2026-11-24 ~21:26

Command: `python3 tools/r1_os_selfcheck.py` (quiet-slot rerun of the 21:13 selfcheck failure)

**OVERALL: blocked — slot NOT quiet, selfcheck NOT run.**

Busy evidence (ps @ ~21:26): PID 1194626 `r1_final_verify.py --strict-skip` (54s in, holds
/tmp/opencode/final_verify.lock; a NEW 21:25 run, not the finished 21:13 one), PID 1194857
qemu-system-i386 active, PID 1194653 selfcheck + PID 1194834 qemu_smoke in flight, PID 1194975
flock fullchain/retire chain; 21:13 gate runner (1138576) still alive.

Race-fix check: tools/r1_os_selfcheck.py mtime 2026-11-20 21:44 — unchanged; LOGDIR is still
fixed `/tmp/opencode` with shared `r1_os_selfcheck_{name}.log` names, no per-PID paths. Fix NOT landed.

Verdict: inconclusive — rerun deferred until slot is free. kernelhwsrv/ untouched.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.
