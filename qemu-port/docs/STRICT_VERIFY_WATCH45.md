# STRICT_VERIFY_WATCH45 — watch of in-flight run (2026-11-24 21:22 EET)
Log: /tmp/opencode/final_verify_1913.log (1.5 KB, <50 MB). PID 1137836 ALIVE (9m10s), now on doom_audio (test 9/11); lock held.
Verdicts so far: selfcheck FAIL (248s, guest selftest markers never appeared, qemu exit 124) | qemu_smoke PASS | genuine_retire PASS | genuine_efsrv PASS | genuine_ewsrv PASS | genuine_eshell PASS | genuine_fat_io PASS | doom_main PASS (80.8s).
Run-level OVERALL / FV_EXIT: not yet printed (still in flight). Fault-scan: no guest Exc/Panic/KERN/#PF in any current-run sublog (only benign "[PASS] no Exc/Panic/FAULT" lines + one Python BrokenPipe artifact in doom_main).

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.
