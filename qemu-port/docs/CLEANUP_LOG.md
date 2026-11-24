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

**Update 2026-11-24 (later):** step 4 (svcmgr) + step 3 (`r1_fs` glue) LANDED
and were committed + pushed as **`dd22ae2`** (HEAD = origin/main = `dd22ae2`,
push `e29a072..dd22ae2`; apply-agent gates all PASS, 0 KERN-4). Steps 5 (winsrv,
edisp port) and 6 (euser_shim closure) are IN FLIGHT. See the rows below and the
Commits table.

## Log table

| Step | Action | Files | Status | Gate evidence |
|---|---|---|---|---|
| 1 | Authority verification: fresh `r1_final_verify.py --strict-skip` on `d26fade` before any deletion | `tools/r1_final_verify.py` (run only) | **IN PROGRESS** (2026-11-24, strict-skip run on `d26fade` not yet concluded; `CLEANUP_AUTHORITY_CHECK.md` not yet written) | Log `/tmp/opencode/final_verify_strict_regression_d26fade.log` (18:37 snapshot): selfcheck/smoke/retire/efsrv/ewsrv PASS so far; required: 11/11 PASS, 0 skip, 0 fail; prior record `FINAL_VERIFICATION_RESULT.md` (11/11 PASS) is pre-`d26fade`-cleanup authority, to be re-confirmed fresh |
| 2 | Remove dead portfilesrv build rules/files + stray 0-byte `-D__cdecl` file + stale `:portfilesrv` artifact | `user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, `services/r1_service_host.*`, `user.mk` portfilesrv rules, stray 0-byte compiler-flag file, stale `:portfilesrv` make artifact | **LANDED + VERIFIED** (2026-11-24) | Post-removal verify (`docs/POST_REMOVAL_VERIFY.md`, 2026-11-24 ~18:49, verdict: removal SAFE): `user.mk` stable 18:47:06; `portfilesrv.cpp`, `mke32_portfilesrv.py`, `:portfilesrv`, `PORTFILESRV.E32` all gone; `make userland svcmgr` exit 0 (`/tmp/opencode/post_removal_build.log`), PORTFILESRV.E32 not regenerated, SVCMGR.E32 rebuilt; smoke 10/10 PASS (`/tmp/opencode/post_removal_smoke.log`); retire 4/4 PASS (`/tmp/opencode/post_removal_retire.log`); fullchain 14/14 PASS incl. "svcmgr/portfilesrv not used" and 0 faults (`/tmp/opencode/post_removal_fullchain.log`). The 18:47:31 smoke FAIL was transient mid-deletion, superseded by post-settle PASS |
| 3 | Remove `r1_fs` kernel glue: drop `r1_fs_core.o`/`r1_fs_fat.o` kernel objects/rules (`Makefile:322-332`, `build/fat32.o` kept) and the `R1Fs::*` globals/exports (`qemu_variant_asic.cpp:14-25`, mount now direct `FatMount(4,0)`; decls out of `qemu_variant.h:10-15`) | `Makefile`, `variant/qemu_variant_asic.cpp`, `variant/qemu_variant.h` (`fs/r1_fs_*`, `variant/r1_fs_channel.*`, `variant/fat32.cpp` untouched — user-mode r1_fs copy stays) | **LANDED + VERIFIED** (2026-11-24) — **committed `dd22ae2`** (pushed; HEAD=origin/main=`dd22ae2`) | `nm`: `R1Fs::` symbols 56→0, `FatMount`/`FatUse` present; `kernfull` + `isogrub` built clean; smoke PASS, retire 4/4, fullchain 14/14, fat_io PASS; `kernelhwsrv/` untouched (clean); independently reviewed CLEAN (`docs/R1FS_REMOVAL_REVIEW.md`, 2026-11-24) |
| 4 | svcmgr — retired. The `SvcMgrExecInstall()` trap was preserved as `PortTrapExecInstall()`: it still wraps genuine exec 81 `FsRegisterThread` (clears critical flags → 0 KERN 4 / 0 FAULT) and hosts the PTRINJ proof sink (slim exec 270 op 4, fbblit ops 2/3 kept for DOOM); blind removal would have reintroduced KERN 4 / broken `r1_genuine_pointer_test.py` | `user/svcmgr.cpp` (deleted), `e32load.cpp` (`SvcMgr*` execs 268–270 launch/kill/ctl-0/1/boot gone, `PortTrapExecInstall()` installed), `variant/qemu_variant_asic.cpp`, `variant/qemu_variant.h`, `user/user.mk` | **LANDED + COMMITTED `dd22ae2`** (2026-11-24, pushed; HEAD=origin/main=`dd22ae2`) — apply per `docs/SVCMGR_RETIREMENT_PLAN.md` (reviewed **SAFE**); apply agent gates all PASS: retire 4/4, fullchain 14/14, `r1_final_verify.py --strict-skip` 11/11, pointer/PTRINJ PASS, **0 KERN-4** | Post-apply gates all PASS, 0 KERN-4 / 0 FAULT; push `e29a072..dd22ae2`; rollback via `/tmp/opencode/svcmgr_retire_backup_20261124_185621/` or `git show e29a072:<path>` |
| 5 | winsrv — edisp port in flight. Needs `edisp` LDD/PSL port + EWSRV external-client window gate + human review (no apply path; file delete only) | `user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp` (GUI execs 249/250/251, event ring 271/272/273), ~12 GUI users + DOOM `guishost.h` fallback | **IN FLIGHT** (2026-11-24) — edisp port in flight (`genuine/ewsrv/build_edisp.py`, `genuine/ewsrv/edisp_glue.cpp` untracked); EWSRV client blockers fixed and EWCLIENT gate PASS (`ewsrv_final_test2.out`), but EDISP.DLL factorization + winsrv migration not started; widest blast radius, last among servers; scope: `docs/WINSRV_EDISP_SCOPE.md` | EWSRV PASS but no apply path; gate: EWSRV external-client window test + human review |
| 6 | euser_shim — closure: real PE2 imports in `elf2e32.py` IAT slots, then delete the static shim | `user/euser_shim.cpp` (deleted), new `user/ca_glue.cpp` (`web_euser_printf` via PE2 IAT slot, `__cxa_pure_virtual`, `User::WaitForExit`), `user/ca_test.c` now `-DWEB_USE_EUSER`, `user.mk` euser_shim rules gone | **LANDED (uncommitted)** (2026-11-24) — closure landed uncommitted on top of `dd22ae2`; **commit pending** (pre-authorized cleanup commit once strict gate passes). **VERIFYING**: `r1_final_verify.py --strict-skip` running in background by the closure agent | Builds OK; smoke PASS, retire 4/4, fullchain 14/14, web browser test PASS; EUSER PASS (genuine prebuilt `EUSER.DLL`, 2503 exports, already resident); strict 11/11 gate in flight |
| 7 | netstack — **NEVER** retire. Authored exception: ESOCK/sockets/commsdb/Bluetooth absent from this `kernelhwsrv/` snapshot; keep the "Symbian-shaped ABI" label | `user/netstack.c` + `netstack_iat.S`, `dnsresolv.c`, `ping.c`, `httpget.c`, `telnet.c`, `wget.c`, `ntpc.c`, `dhcpc.c`, `tools/mke32_netstack.py` | **KEEP — NEVER** | No genuine gate exists; explicit authored exception (audit §2, plan §3 step 7) |

## Plans (2026-11-24)

Scope/plan docs for the remaining deletion steps (4–6), so this log stays
navigable as each phase opens:

| Step | Plan doc | Status (one line) |
|---|---|---|
| 4 | `docs/SVCMGR_RETIREMENT_PLAN.md` | Reviewed **SAFE**; **LANDED + COMMITTED `dd22ae2`** (2026-11-24, pushed) — apply-agent gates all PASS, 0 KERN-4; `SvcMgrExecInstall()` trap preserved as `PortTrapExecInstall()` |
| 5 | `docs/WINSRV_EDISP_SCOPE.md` | **IN FLIGHT** (2026-11-24): edisp port in flight (`build_edisp.py`/`edisp_glue.cpp`); client blockers already fixed, EWCLIENT gate PASS; remaining = EDISP.DLL factorization + GUI migration + deletion; ends in human review (no apply path) |
| 6 | `docs/EUSER_SHIM_SCOPE.md` | **LANDED (uncommitted)** (2026-11-24): `user/euser_shim.cpp` deleted, `user/ca_glue.cpp` added (PE2 IAT `web_euser_printf`/`__cxa_pure_virtual`/`User::WaitForExit`), `user/ca_test.c` `-DWEB_USE_EUSER`, `user.mk` shim rules gone; builds OK, smoke/retire 4/4/fullchain 14/14/web test PASS; strict `--strict-skip` VERIFYING in background; commit pending (pre-authorized once strict passes) |

## Commits (2026-11-24)

| Commit | Contents | State |
|---|---|---|
| `e29a072` | Cleanup deletions + docs: removed dead portfilesrv files/artifacts (`user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, stray flag file, `:portfilesrv`) + cleanup-phase docs | committed + pushed (2026-11-24) |
| `dd22ae2` | svcmgr retirement (`user/svcmgr.cpp` deleted; `SvcMgr*` execs 268/269 + ctl ops 0/1 removed from `e32load.cpp`; `PortTrapExecInstall()` keeps the exec-81 `FsRegisterThread` wrapper + slim exec 270 ops 2/3/4) + `r1_fs` kernel glue removal (`Makefile`, `qemu_variant.h`, `qemu_variant_asic.cpp`, `user/user.mk`); 49+/547− across 6 files | **committed + pushed** (2026-11-24); HEAD = origin/main = `dd22ae2`; push `e29a072..dd22ae2`; apply-agent gates all PASS, 0 KERN-4 |

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
