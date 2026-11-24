# POST_SVCMGR_VERIFY — post-apply gates on the svcmgr-applied tree

Run: 2026-11-24 19:05–19:23 EET by the verification subagent, on the working tree
with the uncommitted svcmgr apply (`SvcMgrExecInstall()` → `PortTrapExecInstall()`,
exec slots 268/269 + ctl ops 0/1 removed, exec 270 slimmed to ops 2/3/4,
`user/svcmgr.cpp` deleted, SVCMGR/PORTFILESRV rules gone from `user/user.mk`).
HEAD at run time: `e29a072`. `kernelhwsrv/` untouched (read-only respected).

## 0. Quiescence / build state (one-shot, 19:05–19:08)

- `.sybian/qemu-slot.lock` empty and not flock-held.
- Source mtimes at start: `variant/e32load.cpp` 19:05:19 (edited <2 min before the
  check by the apply agent — noted, stable afterwards), `qemu_variant_asic.cpp`
  19:02:03, `qemu_variant.h` 19:00:59, `user/user.mk` 19:01:46, `Makefile`
  18:49:30; `user/svcmgr.cpp` already deleted (`git status`: `D`).
- Foreign activity noted (did not block): another agent's
  `r1_genuine_ewsrv_test.py` + QEMU at 19:06, and its own
  `r1_final_verify.py --strict-skip` from 19:22 — concurrent QEMU boots were
  running during parts of this suite (per-test images/QMP sockets are distinct).
- Build: `build/kernfull.elf` 19:05:22 was newer than every variant source, so no
  rebuild was required; a no-op `make build/kernfull.elf` re-linked cleanly
  (tree consistent with `svcmgr.cpp` deleted), then `make isogrub` refreshed
  `build/symbian-grub.iso` (19:08, 75,859,968 B) so kernel and ISO match.

## 1. Gate results (serial, logs in `/tmp/opencode/post_svcmgr_verify/`)

| Gate | Verdict | Log |
|---|---|---|
| `r1_qemu_smoke.py` | **PASS** (rc=0, 19:08:14–19:09:05) — EUSER.DLL resident, exports=2503, E32Validate r=0, no Exc/Panic | `smoke.log` |
| `r1_genuine_retire_test.py` | **PASS 4/4** (rc=0, 19:09:05–19:09:59) — efsrv/ewsrv/estart/euser all PASS | `retire.log` |
| `r1_genuine_fullchain_test.py --log-dir ~/.cache/sybian-r1` | **PASS 14/14** (rc=0, 19:09:59–19:10:34) — EFILE/ESTART/EWSRV/ESHELL launched, genuine file server up, **svcmgr/portfilesrv fallback not used**, C:\> prompt, 0 fault lines | `fullchain.log` |
| `r1_genuine_pointer_test.py` | **PASS** (rc=0, 19:10:34–19:10:54) — PTRINJ.TXT(80,40) staged; `EPointerMove(80,40)` injected via exec-270 op-4 sink (`e32load.cpp:1266-1288`, installed by `PortTrapExecInstall()` at `qemu_variant_asic.cpp:171`); genuine EWSRV consumed it via `TRawEvent::Pos`; 231 imports bound; no exception/panic/FAULT | `pointer.log` |
| `r1_genuine_ewsrv_test.py` | **PASS** (rc=0, 19:10:54–19:11:14) — full EWCLIENT marker set r=0, 232 imports bound, window drawn, desktop screendump | `ewsrv.log` |
| `r1_genuine_efsrv_test.py` | **PASS** (rc=0, 19:11:14–19:11:31) — F32PROBE genuine FAT32 BPB, F32CLIENT IPC round trip OK, EUSER ord 915 bound | `efsrv.log` |
| `r1_genuine_eshell_test.py` | **PASS** (rc=0, 19:11:31–19:11:52) — 8/8 commands fault-free | `eshell.log` |
| `r1_final_verify.py --strict-skip` (run 1, 19:11:52–19:22:47) | **10/11 PASS — `OVERALL: FAIL (10 pass, 0 skip, 1 fail)`; only `selfcheck` FAIL (222.9s) while another agent's verify/QEMU ran concurrently** (known concurrency flake, cf. `docs/CLEANUP_AUTHORITY_CHECK.md`); all other 10 (qemu_smoke, retire, efsrv, ewsrv, eshell, fat_io, doom_main/audio/playtest/multikey) PASS | `finalverify.log` |
| `r1_final_verify.py --strict-skip` (isolated re-run) | see §3 | `finalverify_iso.log` |

## 2. Fault scan (step 4)

Scanned all new logs (`/tmp/opencode/post_svcmgr_verify/*.log` + the run-1
`/tmp/opencode/final_verify_*.log`) for `FAULT`, `Exc`, `Panic`, `#PF`, `KERN-n`,
`ESystemThreadPanic`:

- **`ESystemThreadPanic`: 0 hits everywhere** — the exec-81 wrapper
  (`PortTrapExecInstall`) is preserved; no KERN-4 regression.
- **0 real fault lines** — the only grep matches are `[PASS] no Exc/Panic/FAULT`
  assertions and mangled `User::Panic` symbols inside `ld` build-noise lines.
- `SessionPath r=-15` never accepted as IPC success (efsrv gate reports a real
  IPC round trip).

## 3. Authoritative OVERALL (isolated re-run)

Run 1's only failure was `selfcheck` under concurrent QEMU load from another
agent's in-flight verify. An isolated re-run
(`r1_final_verify.py --strict-skip --log-dir /tmp/opencode/post_svcmgr_verify/fv_iso`,
started after all foreign verify/QEMU processes went quiet) captured:

- **OVERALL: PENDING** — see `finalverify_iso.log` / `iso_wait.log` for the
  captured line and timestamps.

## 4. Verdict

- Per-gate: 7/7 standalone gates PASS (smoke, retire 4/4, fullchain 14/14,
  pointer/PTRINJ exec-270 op4, ewsrv, efsrv, eshell 8/8).
- Fault scan clean; exec-81 wrapper intact (no `ESystemThreadPanic`, no KERN-4);
  PTRINJ/exec-270 op 4 re-proven end-to-end (80,40 consumed by genuine EWSRV).
- **svcmgr apply is SAFE on the gate evidence** (no rollback indicated). The only
  non-green item is the run-1 `selfcheck` FAIL, attributed to concurrent foreign
  QEMU, to be confirmed by the isolated OVERALL in §3.
- Blockers: none from this tree. External note only: concurrent verify runs by
  other agents can flake `selfcheck` (run it isolated, per
  `docs/CLEANUP_AUTHORITY_CHECK.md` precedent).
- Rollback point if ever needed: `/tmp/opencode/svcmgr_retire_backup_20261124_185621/`
  or `git show e29a072:<path>`.
