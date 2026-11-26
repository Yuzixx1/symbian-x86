# QEMU slot serialization: P4 strict wait is EXPECTED (not a failure)

Read-only checks, 2026-11-25 20:58 EET (18:58Z).

- `/tmp/opencode/p4_strict.log` is 0 bytes and `/tmp/opencode/p4_strict.done` is absent: pid 71984 (`flock -w 14400 .sybian/qemu-slot.lock env -u LD_LIBRARY_PATH python3 -u tools/r1_final_verify.py --strict-skip`) is still waiting to enter the slot.
- The lock holder is the P1 chain: pid 70236 (`flock ... bash -c`), 70237 (bash), 71642 (`tools/r1_final_verify.py --strict-skip --log-dir /tmp/opencode/p1_step2`), all shown by `fuser -v .sybian/qemu-slot.lock`.
- P1 strict state per `/tmp/opencode/p1_step2/strict.log`: selfcheck, qemu_smoke, genuine_retire, genuine_efsrv and genuine_ewsrv all PASS; it is currently running genuine_eshell. So P1 has already cleared genuine_efsrv, not merely reached it.
- Verdict: this is EXPECTED serialization under the wave rules (one QEMU slot, strict runs exclusive), not a P4 failure. P4's verdict becomes readable only after 70236 releases the lock and the runner appends `STRICT_EXIT=` and touches `p4_strict.done`. No P4 content predates that point; an empty log carries no signal either way.
