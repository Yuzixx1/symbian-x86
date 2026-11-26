# SVCMGR APPLY STATUS — read-only snapshot (2026-11-24 ~18:58)

Monitor-only snapshot of the in-flight svcmgr retirement apply. No edits made by
this monitor; `kernelhwsrv/` untouched; no QEMU run. Apply agent owns:
`variant/e32load.cpp`, `variant/qemu_variant_asic.cpp`, `variant/qemu_variant.h`,
`user/user.mk`, deletion of `user/svcmgr.cpp`.

## Step table

| Step | Status | Evidence |
|---|---|---|
| Snapshot of 5 target files | DONE | `/tmp/opencode/svcmgr_retire_backup_20261124_185621/` (MANIFEST.sha256 + copies of e32load.cpp, qemu_variant_asic.cpp, qemu_variant.h, user.mk, svcmgr.cpp, 18:56) |
| Edits: e32load.cpp (remove SvcMgrExecInstall) | NOT STARTED | Live file == snapshot sha256; `SvcMgrExecInstall()` still defined at `variant/e32load.cpp:1430` (mtime 08:45, unmodified) |
| Edits: qemu_variant_asic.cpp (drop call) | NOT STARTED | Live == snapshot; `SvcMgrExecInstall();` still live at `qemu_variant_asic.cpp:172` |
| Edits: qemu_variant.h | NOT STARTED | Live == snapshot (mtime 18:49, matches backup) |
| Edits: user/user.mk (drop SVCMGR.E32) | NOT STARTED | Live == snapshot; svcmgr targets still present (lines 36-37, 358-364) |
| Deletion: user/svcmgr.cpp | NOT STARTED | File still exists (14059 bytes, mtime 08:45); not in `git status` deletions |
| Build after edits | PENDING | No post-18:56 build logs attributable to the apply |
| Gates (smoke/retire/fullchain/strict 11/11) | BASELINE GREEN (pre-apply) | See below |

## Gate evidence (newest logs, pre-apply baseline)

- `final_verify_strict_20261124.log` — **OVERALL: PASS (11 pass, 0 skip, 0 fail)**;
  per-test logs `/tmp/opencode/final_verify_*.log` (18:50-18:57) all `RESULT: PASS`
  (selfcheck, qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv,
  genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey).
- Retire 4/4: `final_verify_genuine_retire.log` (18:52) — efsrv/ewsrv/estart/euser
  all PASS, `RESULT: PASS (4 passed, 0 skipped, 0 failed)`.
- Fullchain 14/14: `post_removal_fullchain.log` (18:51) — `RESULT: PASS (14/14 rows PASS)`,
  "svcmgr/portfilesrv not used" PASS, 0 fault lines.
- Smoke: `final_verify_qemu_smoke.log` (18:51) RESULT: PASS, no Exc/PanicKernExec.
- EFSRV/EWSRV/ESHELL individual: all PASS (18:52-18:53 logs).
- Pointer test: last evidence `genuine_pointer.log` + `genuine_pointer_PTRINJ.TXT`
  (02:30, PTRINJ `80 40`); no newer pointer run.
- Faults/rollback: only stale hits — `r1_retire_full.log` (2026-11-23 22:41) shows
  KERN 0x62 FAULTs in an old exploratory run, superseded by 18:52 4/4 PASS.
  All 18:5x gate logs report zero Exc/PanicKernExec/FAULT. No rollback in progress.
- Transient: `final_verify_selfcheck.log` (18:50) recorded a FAIL, superseded by
  selfcheck PASS inside the 11/11 strict run.

## Overall verdict

**IN PROGRESS** — snapshot taken (18:56), zero of the five target files edited yet
(all live files hash-identical to the snapshot), `SvcMgrExecInstall()` still live,
`user/svcmgr.cpp` still present. Baseline gates are green (strict 11/11 PASS,
retire 4/4, fullchain 14/14), so the apply has a clean rollback point.

## Blockers

None observed. Apply agent had not begun edits at snapshot time; gates currently
passing on the un-applied tree.

**2026-11-26 correction:** the step table rows above (lines 13-17) reading svcmgr as pending/current are superseded — the svcmgr retirement has landed (`97f224b`; `user/svcmgr.cpp` deleted, `SvcMgrExecInstall` absent tree-wide), and the load-bearing KERN-4 guard is `PortTrapExecInstall()` at `variant/e32load.cpp:1447`, which wires `PortFsRegisterThread` into slot 81 via `tab[4 + 2 * 81]` (`variant/e32load.cpp:1457`) — not svcmgr; the 13-row gate is now 13/13 PASS (`/tmp/opencode/final13_gate.log`, `OVERALL: PASS (13 pass, 0 skip, 0 fail/not-run)`, `FINAL13_RC=0`).

**2026-11-26 (later — 15-row gate) — supersedes the 13-row figure above:** the gate is now **15/15 PASS** (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`), the two added rows being `g12_negative` (`tools/r1_g12_negative_test.py`) and `hal_probe` (`tools/r1_genuine_hal_test.py`). The svcmgr-retired verdict and the `PortTrapExecInstall()` / `variant/e32load.cpp:1447` + `tab[4 + 2 * 81]` (line 1457) note are unchanged; step table above unchanged, `kernelhwsrv/` untouched.
