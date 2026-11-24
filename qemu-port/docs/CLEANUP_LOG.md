# CLEANUP_LOG — post-`d26fade` shim cleanup phase (2026-11-24)

**Scope:** running log of the human-reviewed cleanup after the guarded shim
retirement (`d26fade`, pushed). Retirement is progressing **bypass → deletion**
(2026-11-24): the two fallback lines remain commented out
(`variant/e32load.cpp:1552`, `user/svcmgr.cpp:250`), and step 2 has now
physically deleted the portfilesrv shim files/rules; rollback via snapshots
(`/tmp/opencode/shim_retire_backup_*`) or `git show d26fade:<path>`. Authority: `tools/r1_final_verify.py --strict-skip` on the current
tree; never accept `SessionPath r=-15` as IPC success; never re-add the EFILE
trampoline. `kernelhwsrv/` is READ-ONLY throughout; no commit/push/`make clean`
from this log. Sources: `REMAINING_SHIMS_AUDIT.md`, `FINAL_PROJECT_SUMMARY.md` §5,
project `AGENTS.md` "Remaining hand-written components".

## Log table

| Step | Action | Files | Status | Gate evidence |
|---|---|---|---|---|
| 1 | Authority verification: fresh `r1_final_verify.py --strict-skip` on `d26fade` before any deletion | `tools/r1_final_verify.py` (run only) | **IN PROGRESS** (2026-11-24, strict-skip run on `d26fade` not yet concluded; `CLEANUP_AUTHORITY_CHECK.md` not yet written) | Log `/tmp/opencode/final_verify_strict_regression_d26fade.log` (18:37 snapshot): selfcheck/smoke/retire/efsrv/ewsrv PASS so far; required: 11/11 PASS, 0 skip, 0 fail; prior record `FINAL_VERIFICATION_RESULT.md` (11/11 PASS) is pre-`d26fade`-cleanup authority, to be re-confirmed fresh |
| 2 | Remove dead portfilesrv build rules/files + stray 0-byte `-D__cdecl` file + stale `:portfilesrv` artifact | `user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, `services/r1_service_host.*`, `user.mk` portfilesrv rules, stray 0-byte compiler-flag file, stale `:portfilesrv` make artifact | **LANDED + VERIFIED** (2026-11-24) | Post-removal verify (`docs/POST_REMOVAL_VERIFY.md`, 2026-11-24 ~18:49, verdict: removal SAFE): `user.mk` stable 18:47:06; `portfilesrv.cpp`, `mke32_portfilesrv.py`, `:portfilesrv`, `PORTFILESRV.E32` all gone; `make userland svcmgr` exit 0 (`/tmp/opencode/post_removal_build.log`), PORTFILESRV.E32 not regenerated, SVCMGR.E32 rebuilt; smoke 10/10 PASS (`/tmp/opencode/post_removal_smoke.log`); retire 4/4 PASS (`/tmp/opencode/post_removal_retire.log`); fullchain 14/14 PASS incl. "svcmgr/portfilesrv not used" and 0 faults (`/tmp/opencode/post_removal_fullchain.log`). The 18:47:31 smoke FAIL was transient mid-deletion, superseded by post-settle PASS |
| 3 | Remove `r1_fs` kernel glue: drop `r1_fs_core.o`/`r1_fs_fat.o` kernel objects/rules (`Makefile:322-332`, `build/fat32.o` kept) and the `R1Fs::*` globals/exports (`qemu_variant_asic.cpp:14-25`, mount now direct `FatMount(4,0)`; decls out of `qemu_variant.h:10-15`) | `Makefile`, `variant/qemu_variant_asic.cpp`, `variant/qemu_variant.h` (`fs/r1_fs_*`, `variant/r1_fs_channel.*`, `variant/fat32.cpp` untouched — user-mode r1_fs copy stays) | **LANDED + VERIFIED** (2026-11-24) | `nm`: `R1Fs::` symbols 56→0, `FatMount`/`FatUse` present; `kernfull` + `isogrub` built clean; smoke PASS, retire 4/4, fullchain 14/14, fat_io PASS; `kernelhwsrv/` untouched (clean); independently reviewed CLEAN (`docs/R1FS_REMOVAL_REVIEW.md`, 2026-11-24) |
| 4 | svcmgr — apply in flight. Preserve the `SvcMgrExecInstall()` trap (`qemu_variant_asic.cpp:194`): it wraps genuine exec 81 `FsRegisterThread` (clears critical flags → 0 FAULT) and hosts the PTRINJ proof sink (exec 270 op 4, `e32load.cpp:1331`); blind removal reintroduces KERN 4 / breaks `r1_genuine_pointer_test.py` | `user/svcmgr.cpp`, `e32load.cpp:1258-1452` (`SvcMgr*` execs 268–270), `variant/qemu_variant_asic.cpp:194` | **IN FLIGHT** (2026-11-24) — apply per `docs/SVCMGR_RETIREMENT_PLAN.md`, reviewed **SAFE**; apply agent owns `e32load.cpp`/`qemu_variant_asic.cpp`/`Makefile`+`user.mk`/`user/svcmgr.cpp` | ESTART/ESHELL PASS but apply rolled back; exec-81 wrapper preserved until EFILE teardown proven without it |
| 5 | winsrv — **NOT started**. Needs `edisp` LDD/PSL port + EWSRV external-client window gate + human review (no apply path; file delete only) | `user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp` (GUI execs 249/250/251, event ring 271/272/273), ~12 GUI users + DOOM `guishost.h` fallback | **NOT STARTED** — widest blast radius, last among servers; scope: `docs/WINSRV_EDISP_SCOPE.md` | EWSRV PASS but no apply path; gate: EWSRV external-client window test + human review |
| 6 | euser_shim — **NOT started**. Separate closure: make `elf2e32.py` emit real PE2 imports, then delete the static shim | `user/euser_shim.cpp`, `user/e32base.h`/`e32cmn.h` (declarations) | **NOT STARTED** — scope: `docs/EUSER_SHIM_SCOPE.md` | EUSER PASS (genuine prebuilt `EUSER.DLL`, 2503 exports, already resident) |
| 7 | netstack — **NEVER** retire. Authored exception: ESOCK/sockets/commsdb/Bluetooth absent from this `kernelhwsrv/` snapshot; keep the "Symbian-shaped ABI" label | `user/netstack.c` + `netstack_iat.S`, `dnsresolv.c`, `ping.c`, `httpget.c`, `telnet.c`, `wget.c`, `ntpc.c`, `dhcpc.c`, `tools/mke32_netstack.py` | **KEEP — NEVER** | No genuine gate exists; explicit authored exception (audit §2, plan §3 step 7) |

## Plans (2026-11-24)

Scope/plan docs for the remaining deletion steps (4–6), so this log stays
navigable as each phase opens:

| Step | Plan doc | Status (one line) |
|---|---|---|
| 4 | `docs/SVCMGR_RETIREMENT_PLAN.md` | Reviewed **SAFE**; step-3 `r1_fs` glue removal settled (LANDED+VERIFIED) — apply **IN FLIGHT** (2026-11-24; shared files: `e32load.cpp`, `qemu_variant_asic.cpp`, `Makefile`/`user.mk`) |
| 5 | `docs/WINSRV_EDISP_SCOPE.md` | Client blockers already fixed; remaining = `edisp` LDD/PSL port + GUI migration + deletion; ends in human review (no apply path) |
| 6 | `docs/EUSER_SHIM_SCOPE.md` | Build-time only; closes after svcmgr — deletion set is `user/euser_shim.cpp` + its `user.mk` lines |

## Safe-kept (legitimate x86/PC port bridges — NOT cleanup targets)

PS/2 input (`qemu_mouse.cpp`, `qemu_variant_int.cpp`, `kern_shell.cpp` PS/2
ring), AC'97 audio (`qemu_audio*.cpp`, `audiopcm.c`, `soundplr.c`, `music.c`,
`morse.c`), storage (`qemu_ata/ahci/virtio_blk/usb*/xhci.cpp`), display/PCI/
variant/loader (`qemu_display.cpp`, `qemu_fbblit.cpp`, `qemu_nic.cpp`,
`qemu_virtio_net.cpp`, `qemu_netbridge.cpp`, `qemu_virtio_rng.cpp`,
`qemu_pci.cpp`, `qemu_assp.cpp`, `qemu_variant_rom.cpp`, `e32load.cpp`),
`kern_shell` debug console, freestanding user runtime (`runtime.c`, `ipc.c`,
`malloc.c`, …), and all `tools/r1_*` test harnesses / `tests/`. See
`REMAINING_SHIMS_AUDIT.md` §2 class B/C.

## Standing caveats

- Rollback snapshot `/tmp/opencode/shim_retire_backup_20261124_083011`
  (token `ROLLBACK-PORTFILESRV-20261124_083011`) stays valid until step 1 is
  green and the file-server cut is consolidated; generic rollback for any
  deleted file: snapshot dirs under `/tmp/opencode/shim_retire_backup_*` or
  `git show d26fade:<path>` (bypass→deletion progress, 2026-11-24).
- A disk without staged genuine EFILE/EFSRV no longer boots — genuine EFSRV is
  the sole live file server.
- No commit/push without an explicit user request; never `git add -A`.
