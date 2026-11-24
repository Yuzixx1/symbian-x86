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
push `e29a072..dd22ae2`; apply-agent gates all PASS, 0 KERN-4). Step 6 (euser_shim closure) has since
LANDED and was committed + pushed as **`0f9ec6a`** (HEAD = origin/main =
`0f9ec6a`, push `dd22ae2..0f9ec6a`); strict gate was in flight at commit time
and a `cp4_stress` regression was found post-commit (fix in flight). Step 5
(winsrv, edisp port) remains IN FLIGHT. See the rows below and the
Commits table.

## Log table

| Step | Action | Files | Status | Gate evidence |
|---|---|---|---|---|
| 1 | Authority verification: fresh `r1_final_verify.py --strict-skip` on `d26fade` before any deletion | `tools/r1_final_verify.py` (run only) | **IN PROGRESS** (2026-11-24, strict-skip run on `d26fade` not yet concluded; `CLEANUP_AUTHORITY_CHECK.md` not yet written) | Log `/tmp/opencode/final_verify_strict_regression_d26fade.log` (18:37 snapshot): selfcheck/smoke/retire/efsrv/ewsrv PASS so far; required: 11/11 PASS, 0 skip, 0 fail; prior record `FINAL_VERIFICATION_RESULT.md` (11/11 PASS) is pre-`d26fade`-cleanup authority, to be re-confirmed fresh |
| 2 | Remove dead portfilesrv build rules/files + stray 0-byte `-D__cdecl` file + stale `:portfilesrv` artifact | `user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, `services/r1_service_host.*`, `user.mk` portfilesrv rules, stray 0-byte compiler-flag file, stale `:portfilesrv` make artifact | **LANDED + VERIFIED** (2026-11-24) | Post-removal verify (`docs/POST_REMOVAL_VERIFY.md`, 2026-11-24 ~18:49, verdict: removal SAFE): `user.mk` stable 18:47:06; `portfilesrv.cpp`, `mke32_portfilesrv.py`, `:portfilesrv`, `PORTFILESRV.E32` all gone; `make userland svcmgr` exit 0 (`/tmp/opencode/post_removal_build.log`), PORTFILESRV.E32 not regenerated, SVCMGR.E32 rebuilt; smoke 10/10 PASS (`/tmp/opencode/post_removal_smoke.log`); retire 4/4 PASS (`/tmp/opencode/post_removal_retire.log`); fullchain 14/14 PASS incl. "svcmgr/portfilesrv not used" and 0 faults (`/tmp/opencode/post_removal_fullchain.log`). The 18:47:31 smoke FAIL was transient mid-deletion, superseded by post-settle PASS |
| 3 | Remove `r1_fs` kernel glue: drop `r1_fs_core.o`/`r1_fs_fat.o` kernel objects/rules (`Makefile:322-332`, `build/fat32.o` kept) and the `R1Fs::*` globals/exports (`qemu_variant_asic.cpp:14-25`, mount now direct `FatMount(4,0)`; decls out of `qemu_variant.h:10-15`) | `Makefile`, `variant/qemu_variant_asic.cpp`, `variant/qemu_variant.h` (`fs/r1_fs_*`, `variant/r1_fs_channel.*`, `variant/fat32.cpp` untouched — user-mode r1_fs copy stays) | **LANDED + VERIFIED** (2026-11-24) — **committed `dd22ae2`** (pushed; HEAD=origin/main=`dd22ae2`) | `nm`: `R1Fs::` symbols 56→0, `FatMount`/`FatUse` present; `kernfull` + `isogrub` built clean; smoke PASS, retire 4/4, fullchain 14/14, fat_io PASS; `kernelhwsrv/` untouched (clean); independently reviewed CLEAN (`docs/R1FS_REMOVAL_REVIEW.md`, 2026-11-24) |
| 4 | svcmgr — retired. The `SvcMgrExecInstall()` trap was preserved as `PortTrapExecInstall()`: it still wraps genuine exec 81 `FsRegisterThread` (clears critical flags → 0 KERN 4 / 0 FAULT) and hosts the PTRINJ proof sink (slim exec 270 op 4, fbblit ops 2/3 kept for DOOM); blind removal would have reintroduced KERN 4 / broken `r1_genuine_pointer_test.py` | `user/svcmgr.cpp` (deleted), `e32load.cpp` (`SvcMgr*` execs 268–270 launch/kill/ctl-0/1/boot gone, `PortTrapExecInstall()` installed), `variant/qemu_variant_asic.cpp`, `variant/qemu_variant.h`, `user/user.mk` | **LANDED + COMMITTED `dd22ae2`** (2026-11-24, pushed; HEAD=origin/main=`dd22ae2`) — apply per `docs/SVCMGR_RETIREMENT_PLAN.md` (reviewed **SAFE**); apply agent gates all PASS: retire 4/4, fullchain 14/14, `r1_final_verify.py --strict-skip` 11/11, pointer/PTRINJ PASS, **0 KERN-4** | Post-apply gates all PASS, 0 KERN-4 / 0 FAULT; push `e29a072..dd22ae2`; rollback via `/tmp/opencode/svcmgr_retire_backup_20261124_185621/` or `git show e29a072:<path>` |
| 5 | winsrv — edisp port in flight. Needs `edisp` LDD/PSL port + EWSRV external-client window gate + human review (no apply path; file delete only) | `user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp` (GUI execs 249/250/251, event ring 271/272/273), ~12 GUI users + DOOM `guishost.h` fallback | **IN FLIGHT** (2026-11-24) — **chunk 1 landed:** `user/ws_e32_client.c` ports wsclient `open`/`window_create`/`close`/`set_title`/`set_size`/`screen_size` onto genuine EWSRV via PE2 ordinals 211/1547/120/915 (`pack_multi` IAT at 0x8000); `WSE32TEST.E32` builds + validates (33136 B, IAT words verified); `WSDL`/`wsclient` still build unchanged; draw/input stubs remain (next chunks); blocker: event ring needs EWSRV input injection (in flight). Earlier: edisp port in flight (`genuine/ewsrv/build_edisp.py`, `genuine/ewsrv/edisp_glue.cpp` untracked), EWSRV client blockers fixed, EWCLIENT gate PASS (`ewsrv_final_test2.out`); widest blast radius, last among servers; scope: `docs/WINSRV_EDISP_SCOPE.md`. **Chunk A LANDED (2026-11-24):** `ws_e32_client` wsclient port on genuine EWSRV ordinals 211/1547/120/915 (`WSE32TEST.E32` validates); SHOT migrated to `shot_e32.c` on genuine RConsole/EWSRV (`SHOTE32.E32` 53888 B, 20 imports, e32read PASS, readback harness ready; `user.mk` lines reported not applied). Chunks B (guilib + GUIHELLO/DTEDIT/DTCALC) and C (SAVER/GCBLITTEST) IN FLIGHT; D blocked on EWSRV input injection (in flight); E = DOOM fallback + deletion last. Discovery: this tree's genuine ws client is `RConsole`/`CWsSession` (`e32twin.h`), not `RWsSession`. **winsrv migration chunks LANDED (2026-11-24, see "winsrv migration" subsection):** chunk A `wsclient`→`ws_e32_client` on ordinals 211/1547/120/915 + WSDL swap build-verified; chunk 2 draw (`ws_draw_fill_rect`, text-window glyph ops 21/6/10) + D0 `ws_text` + D1 `ws_get_event` (EConsoleRead op 11, ord 1548 async) landed; chunk B guilib + GUIHELLO/DTEDIT/DTCALC runtime-verified (GUIHELLO 13/13 markers, screendump, keys); chunk C SAVER/GCBLITTEST build-verified; D2 GUIMENU, D3 BREAKOUT, D4 DESKTOP (20:22), D5 GUISHELL migrated; old `shot.c` migrated, `ws_e32_draw_test.c` cleaned. EWSRV rebuilt with `-fshort-wchar` (UTF-16 literals), runtime-verified `Connect r=0` for hand clients. Slot-269 `E32Kill` fix landed; strict 11/11 captured 20:41. **Remaining:** fresh strict verify on full tree (blocked by QEMU), then winsrv deletion via `tools/r1_winsrv_delete.py` (in progress), DOOM 251-fallback removal last | EWSRV PASS but no apply path; gate: EWSRV external-client window test + human review |
| 6 | euser_shim — closure: real PE2 imports in `elf2e32.py` IAT slots, then delete the static shim | `user/euser_shim.cpp` (deleted), new `user/ca_glue.cpp` (`web_euser_printf` via PE2 IAT slot, `__cxa_pure_virtual`, `User::WaitForExit`), `user/ca_test.c` now `-DWEB_USE_EUSER`, `user.mk` euser_shim rules gone | **LANDED + COMMITTED `0f9ec6a`** (2026-11-24, pushed; HEAD=origin/main=`0f9ec6a`; push `dd22ae2..0f9ec6a`) — strict `--strict-skip` gate was still in flight at commit time; post-commit `cp4_stress` regression found, fix in flight | Builds OK; smoke PASS, retire 4/4, fullchain 14/14, web browser test PASS; EUSER PASS (genuine prebuilt `EUSER.DLL`, 2503 exports, already resident; `ca_test` on genuine PE2 imports) |
| 7 | netstack — **NEVER** retire. Authored exception: ESOCK/sockets/commsdb/Bluetooth absent from this `kernelhwsrv/` snapshot; keep the "Symbian-shaped ABI" label | `user/netstack.c` + `netstack_iat.S`, `dnsresolv.c`, `ping.c`, `httpget.c`, `telnet.c`, `wget.c`, `ntpc.c`, `dhcpc.c`, `tools/mke32_netstack.py` | **KEEP — NEVER** | No genuine gate exists; explicit authored exception (audit §2, plan §3 step 7) |

## Plans (2026-11-24)

Scope/plan docs for the remaining deletion steps (4–6), so this log stays
navigable as each phase opens:

| Step | Plan doc | Status (one line) |
|---|---|---|
| 4 | `docs/SVCMGR_RETIREMENT_PLAN.md` | Reviewed **SAFE**; **LANDED + COMMITTED `dd22ae2`** (2026-11-24, pushed) — apply-agent gates all PASS, 0 KERN-4; `SvcMgrExecInstall()` trap preserved as `PortTrapExecInstall()` |
| 5 | `docs/WINSRV_EDISP_SCOPE.md` | **IN FLIGHT** (2026-11-24): edisp port in flight (`build_edisp.py`/`edisp_glue.cpp`); client blockers already fixed, EWCLIENT gate PASS; **chunk 1:** `ws_e32_client.c` wsclient open/window_create/close/set_title/set_size/screen_size on genuine EWSRV PE2 ordinals 211/1547/120/915 (pack_multi IAT 0x8000), WSE32TEST.E32 33136 B validated, WSDL/wsclient unchanged; remaining = draw/input stubs, event-ring input injection, EDISP.DLL factorization + deletion; ends in human review (no apply path). **Chunk A LANDED (2026-11-24):** `ws_e32_client` on genuine EWSRV ordinals 211/1547/120/915, `WSE32TEST.E32` validates; SHOT on `shot_e32.c` via genuine `RConsole`/`CWsSession` (`e32twin.h` — not `RWsSession`; `SHOTE32.E32` 53888 B, 20 imports, e32read PASS, readback harness ready; `user.mk` lines reported not applied). Chunks B (guilib+GUIHELLO/DTEDIT/DTCALC) and C (SAVER/GCBLITTEST) in flight; D blocked on EWSRV input injection (in flight); E = DOOM fallback + deletion last. **Migration chunks LANDED (2026-11-24):** A `ws_e32_client` wsclient port (ords 211/1547/120/915) + WSDL swap build-verified; chunk 2 draw (`ws_draw_fill_rect`, glyph ops 21/6/10) + D0 `ws_text` + D1 `ws_get_event` (EConsoleRead op 11, ord 1548 async) landed; B guilib+GUIHELLO/DTEDIT/DTCALC runtime-verified (GUIHELLO 13/13, screendump, keys); C SAVER/GCBLITTEST build-verified; D2 GUIMENU/D3 BREAKOUT/D4 DESKTOP (20:22)/D5 GUISHELL migrated; old `shot.c` migrated, `ws_e32_draw_test.c` cleaned. EWSRV `-fshort-wchar` rebuild runtime-verified `Connect r=0`; slot-269 `E32Kill` fix landed; strict 11/11 at 20:41. Remaining: fresh strict verify on full tree (QEMU-blocked), winsrv deletion via `tools/r1_winsrv_delete.py` (in progress), DOOM 251-fallback removal last |
| 6 | `docs/EUSER_SHIM_SCOPE.md` | **LANDED + COMMITTED `0f9ec6a`** (2026-11-24, pushed): `user/euser_shim.cpp` deleted, `user/ca_glue.cpp` added (PE2 IAT `web_euser_printf`/`__cxa_pure_virtual`/`User::WaitForExit`), `user/ca_test.c` `-DWEB_USE_EUSER`, `user.mk` shim rules gone; builds OK, smoke/retire 4/4/fullchain 14/14/web test PASS; strict gate in flight at commit time; `cp4_stress` regression found post-commit, fix in flight |

## winsrv migration (2026-11-24, step 5 chunks)

Migration of the hand-written winsrv clients onto genuine EWSRV, in chunks
(agent-report verified facts):

- **Chunk A — LANDED, build-verified:** `wsclient` → `ws_e32_client` on genuine
  EWSRV ordinals 211/1547/120/915; WSDL swap build-verified.
- **Chunk 2 — LANDED:** draw (`ws_draw_fill_rect`, text-window glyph ops
  21/6/10); D0 `ws_text`; D1 `ws_get_event` (EConsoleRead op 11, ord 1548 async).
- **Chunk B — LANDED, runtime-verified:** guilib + GUIHELLO/DTEDIT/DTCALC
  (GUIHELLO 13/13 markers, screendump, keys).
- **Chunk C — LANDED, build-verified:** SAVER/GCBLITTEST.
- **Chunks D2/D3/D4/D5 — LANDED:** GUIMENU, BREAKOUT, DESKTOP (20:22),
  GUISHELL migrated; old `shot.c` migrated; `ws_e32_draw_test.c` cleaned.
- **EWSRV rebuild:** `-fshort-wchar` (UTF-16 name literals); runtime-verified
  `Connect r=0` for hand clients.
- **Slot-269 `E32Kill` fix landed;** strict `r1_final_verify.py --strict-skip`
  11/11 PASS captured 20:41.
- **Remaining:** fresh strict verify on full tree (blocked by QEMU), then
  winsrv deletion via `tools/r1_winsrv_delete.py` (being implemented), DOOM
  251-fallback removal last.

## Commits (2026-11-24)

| Commit | Contents | State |
|---|---|---|
| `e29a072` | Cleanup deletions + docs: removed dead portfilesrv files/artifacts (`user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, stray flag file, `:portfilesrv`) + cleanup-phase docs | committed + pushed (2026-11-24); push `d26fade..e29a072` |
| `dd22ae2` | svcmgr retirement (`user/svcmgr.cpp` deleted; `SvcMgr*` execs 268/269 + ctl ops 0/1 removed from `e32load.cpp`; `PortTrapExecInstall()` keeps the exec-81 `FsRegisterThread` wrapper + slim exec 270 ops 2/3/4) + `r1_fs` kernel glue removal (`Makefile`, `qemu_variant.h`, `qemu_variant_asic.cpp`, `user/user.mk`); 49+/547− across 6 files | **committed + pushed** (2026-11-24); push `e29a072..dd22ae2`; apply-agent gates all PASS, 0 KERN-4 |
| `0f9ec6a` | Close euser_shim: `ca_test` moved onto genuine EUSER PE2 imports (`user/ca_glue.cpp` IAT slots: `web_euser_printf`, `__cxa_pure_virtual`, `User::WaitForExit`; `user/euser_shim.cpp` deleted; `user.mk` shim rules gone) | **committed + pushed** (2026-11-24); HEAD = origin/main = `0f9ec6a`; push `dd22ae2..0f9ec6a`; strict gate was in flight at commit time; post-commit `cp4_stress` regression found, fix in flight |

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
