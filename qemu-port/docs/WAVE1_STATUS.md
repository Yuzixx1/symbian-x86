# WAVE1 STATUS — phase outcomes in one place

**Snapshot:** 2026-11-25 20:40 EET. Read-only synthesis of FINAL_GATE_STATUS.md, KERNEL_HOOK_FREEZE.md,
P3_FREEDM_GATE_AUDIT.md, ASSET_RECOVERY_NOTE.md, ASSET_STAGING_NOTE.md, GATE_STATUS_CONSISTENCY.md,
PLAN_OVERNIGHT_WAVE1.md, `/tmp/opencode/*.log`, `git status`/`git log`. No gate, build or QEMU run for this page.
`kernelhwsrv/` porcelain empty (READ-ONLY honored). No commits/pushes, no `make clean`.

| Phase | State | Evidence |
|---|---|---|
| P0 baseline | **NO-GO, then GO** | Strict 7/11 with 4 DOOM SKIP (wiped `/tmp` IWAD), retire 4/4, fullchain 14/14 at ~20:05; IWAD restored 28,795,076 B and recovery re-run gives 11 pass / 0 skip / 0 fail |
| P1 shim deletion | **in flight** | `fs/r1_fs_*`, `services/r1_service_host.{cpp,h}`, `variant/r1_fs_channel.cpp` still present; `tools/r1_shim_residue_sweep.py` (G12) absent; no P1 verdict in FINAL_GATE_STATUS.md |
| P3 FreeDM scaffold | **GO** | SKIP-clean exit 0 with asset absent, `mkgrubiso.py` append-only 69/0, DEFAULT_TESTS still 11, `strict.log` `OVERALL: PASS (11 pass)`; FreeDM stays SKIP pending drop-off |
| P4-CI USB/real-PC | **in flight** | `variant/qemu_pci.cpp` +169/-37, `variant/qemu_usb.cpp` +240/-2, `tools/r1_realhw_readiness_audit.py` present; USB/virtio logs 19:59–20:12 carry no verdict line, no P4 gate recorded |

**Authoritative gate log:** `/tmp/opencode/final_verify_recovery.log` — `OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run)`.
Kernel-hook freeze holds: exec 81/269/270 and the `GenuineBootLaunch()` contract untouched.

---

**SUPERSEDED 2026-11-26.** The 11-row / 10-11-pass strict gate asserted above is historical and no longer current. Current authority is **15 rows / `FINAL27_RC=0`** — 15 pass, 0 skip, 0 fail — recorded in `/tmp/opencode/final27_gate.log`. The body above is preserved byte-identical for audit history.
