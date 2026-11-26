# PLAN_SURVEY_SHIMS — remaining hand-written shims survey (planning input)

Read-only survey of `qemu-port/` as of HEAD `ee8490e`. No edits outside this
file; `kernelhwsrv/` untouched; no commit/push; no `make clean`; no QEMU runs.
Supersedes `docs/REMAINING_SHIMS_AUDIT.md` (2026-11-24), which predates commits
`d26fade` (portfilesrv/r1_fs retire), `dd22ae2` (svcmgr retire + r1_fs kernel glue
drop), `0f9ec6a` (euser_shim close), `94ce9c9`/`ee8490e` (winsrv/GUI delete).

## Inventory table

| Item | Path(s) | Still in tree? | Compiled/linked in default build? | Notes |
|---|---|---|---|---|
| svcmgr | `user/svcmgr.cpp`, `SvcMgr*` handlers in `variant/e32load.cpp` | **DELETED** (`dd22ae2`) | No | Only stale comments remain (`user/user.mk:1297,1314`, `qemu_fbblit.h`) |
| portfilesrv | `user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py` | **DELETED** (`d26fade`/`e29a072`) | No | Stale artifacts remain: `PORTFILES.RV.E32` (repo root), `build/user/PORTFILESRV.EXE` |
| winsrv/GUI | `user/winsrv.c`, `user/guishost.h`, `wsclient.*`, `variant/qemu_windowsrv.*`, `variant/qemu_gui_exec.cpp` | **DELETED** (`94ce9c9`) | No | GUI apps migrated to genuine EWSRV (`97f224b`) |
| euser_shim | `user/euser_shim.cpp` | **DELETED** (`0f9ec6a`) | No | ca_test on genuine EUSER PE2 imports; stale comments in `user.mk:204` |
| r1_fs sources | `fs/r1_fs_*` (13 files: core/server/fat/channel/eka/wire/backend/mmp) | **Yes — orphaned** | No (kernel glue dropped `dd22ae2`; `user.mk:354,362` rules have **no dependent target**) | Referenced by host tests `tests/r1_fs_core`, `tests/r1_fs_channel`, static opcode check in `tools/r1_genuine_retire_test.py:48`, plan file list in `tools/r1_shim_retire_plan.py:100-104` |
| r1_fs channel (kernel) | `variant/r1_fs_channel.{cpp,h}`, Makefile opt-in block (`Makefile:341-351`) | Yes | **No** — `R1_FS_CHANNEL ?= 0`, excluded from default kernfull | Includes `fs/r1_fs_channel.h`; stale `build/r1_fs_channel.o` |
| file-server shim host | `services/r1_service_host.{cpp,h}`, `r1_service_state.h` | Yes | **No — fully orphaned** (no includers outside `services/`; one comment ref in `genuine/efsrv_min/efsrv_min.cpp:10`) | Was portfilesrv's ring-3 host |
| DOOM libc shim | `ports/doom/e32_shim.c` (1111 ln), `e32_shim.h` | Yes | **YES** — `build/user/doom_e32_shim.o` in `DOOM_ALL_OBJS` (`user.mk:1616`) | Freestanding libc + PortBigFile I/O for doome32.elf; port-shaped but hand-written |
| Lua libc shim | `ports/lua/e32_shim.c` (844 ln), `e32_shim.h` | Yes | **YES** — `build/lua/e32_shim.o` into `LUA.E32` (`user.mk:1536-1551`) | No test in the 11-gate covers LUA.E32 |
| EWSRV shims | `genuine/ewsrv/ewsrv_shims.cpp` (82 ln), `stubs.S`, `patched/co_cli.cpp` | Yes | **YES** — in `build_ewsrv.py:72`, `build_client.py:54`, and `SHOTE32.E32` (`user.mk:1425`) | Glue so genuine EWSRV builds/runs on x86; legitimate port bridge |
| HAL shim header | `genuine/hal/shim/kernel/hal_int.h` | Yes | Part of genuine HAL build | Port bridge |
| Stale build artifacts | `build/user/PORTFILESRV.EXE`, `build/user/r1_fs_{core,server}.o`, `build/r1_fs_{core,fat,channel}.o`, `build/lua/e32_shim.o`, `PORTFILES.RV.E32` | Yes | No (orphan rules never fire) | Cosmetic until a clean build |

## Live vs bypassed

**Bypassed/retired (boot path no longer uses them):** portfilesrv + r1_fs
(`d26fade`), svcmgr + exec 268 launch (`dd22ae2`), winsrv GUI execs 249-251/271-273
(`94ce9c9`), euser_shim static link (`0f9ec6a`). Genuine chain
EUSER → EFILE/EFSRV → ESTART → EWSRV → ESHELL is the sole boot path
(`GenuineBootLaunch()`, `variant/e32load.cpp:1540`).

**Still LIVE kernel hooks** — installed by `PortTrapExecInstall()`
(`variant/e32load.cpp:1447`, called from `variant/qemu_variant_asic.cpp:171`;
renamed from `SvcMgrExecInstall`):

- **exec 81 slot: `PortFsRegisterThread` wrapper** (`e32load.cpp:1363,1457`) —
  wraps genuine `FsRegisterThread` so EFILE teardown doesn't fault
  `K::ESystemThreadPanic=4`. LIVE and load-bearing.
- **exec 269: `PortKillExec`** (`e32load.cpp:1462-1464`) — peer-kill path asserted
  by `cp4_stress` via killproc's `exec2(269,...)`. LIVE.
- **exec 270 op 2/3/4: `PortCtlExec`** (`e32load.cpp:1270,1469`) — FBO_FBINFO /
  FBO_FBBLIT (`qemu_fbblit.cpp`) plus **op 4 PTRINJ proof sink** (`e32load.cpp:1277`),
  fed by the `\PTRINJ.TXT`-gated pointer bridge in `qemu_display.cpp`. LIVE.
- `PortFileServerPublished()` race guard still read at `e32load.cpp:797` and
  `:1545` (defensive; genuine EFSRV wins the boot race now).

**LIVE user-space shims still linked:** `ports/doom/e32_shim.c`,
`ports/lua/e32_shim.c`, `genuine/ewsrv/ewsrv_shims.cpp` (+HAL shim header).

**Dangling refs:** `user/applaunch.c:143`, `user/desktop.c:496`, `user/guimenu.c:303`
still call `exec2(268)` whose slot is retired — they hit the explicit
"exec 268 unavailable" fallback (`applaunch.c:161-162`); apps are in the default
`userland` target.

## Dependency risks

1. **`fs/r1_fs_core.h` is a gate input, not just a shim file:**
   `tools/r1_genuine_retire_test.py:46-48,127` statically diffs shim opcodes in
   `fs/r1_fs_core.h` against genuine `TFsMessage` in `kernelhwsrv/.../F32plugin.h`.
   Deleting `fs/r1_fs_core.h` breaks the `genuine_retire` test → breaks the 11/11
   gate. Port the check (or drop it deliberately) before deleting the file.
2. **exec-81 wrapper / KERN 4:** removing `PortFsRegisterThread` reintroduces
   `ESystemThreadPanic=4` on EFILE teardown (AGENTS.md EFILE decision; do not
   re-add trampoline either). Keep until teardown is proven clean without it.
3. **exec 269** is asserted by `r1_cp4_ca_stress_test.py`; **op 4 PTRINJ** by
   `tools/r1_genuine_pointer_test.py`. Removing either breaks a gate.
4. **`variant/r1_fs_channel.*` includes `fs/r1_fs_channel*.h`** — delete the
   Makefile opt-in block and `variant/` files together with (or after) `fs/`.
5. **DOOM shim is in the doom gate path:** `doom_main/audio/playtest/multikey`
   (4 of the 11) link `doom_e32_shim.o`. Only removable if DOOM moves fully onto
   `runtime.c`/genuine EUSER — not currently planned.
6. Stale artifacts (`PORTFILES.RV.E32`, `build/**` shim objects) are inert but
   confuse audits; they regenerate only if orphaned rules fire, which they don't.
7. Stale comments referencing portfilesrv/svcmgr in `user/user.mk` (lines 51, 61,
   204, 403, 623, 1297, 1314) — cosmetic, safe to fix with any deletion pass.

## Gate coverage

- **11/11 authoritative gate:** `tools/r1_final_verify.py` → selfcheck, qemu_smoke,
  genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io,
  doom_main, doom_audio, doom_playtest, doom_multikey (`--strict-skip`; logs
  `/tmp/opencode/final_verify_<name>.log`).
- **Retire 4/4:** `tools/r1_genuine_retire_test.py` subsystems efsrv/ewsrv/estart/
  euser (order at `:703`); includes the `fs/r1_fs_core.h` static check above.
- **Fullchain 14/14:** `tools/r1_genuine_fullchain_test.py` — includes a
  "svcmgr/portfilesrv not used" row.
- **Pointer/PTRINJ:** `tools/r1_genuine_pointer_test.py` (op-4 sink + `\PTRINJ.TXT`).
- **Per-subsystem:** `r1_qemu_smoke.py`, `r1_genuine_{efsrv,ewsrv,eshell,fat_io}_test.py`,
  `r1_doom_*_test.py`, `r1_cp4_ca_stress_test.py` (exec 269).
- No gate covers `LUA.E32`/`ports/lua/e32_shim.c`; no gate pins
  `services/r1_service_host.*` or `variant/r1_fs_channel.*` (opt-in, off).

## Recommended deletion order

Safe order that keeps `r1_final_verify.py --strict-skip` at 11/11; re-run the
strict gate after each step:

1. **Stale artifacts** — `PORTFILES.RV.E32`, `build/user/PORTFILESRV.EXE`,
   `build/**/r1_fs_*.o`. Zero build wiring; purely cosmetic. (No `make clean`.)
2. **Orphaned build rules + host** — drop `user.mk:354-362` r1_fs rules and
   `services/r1_service_host.*`; nothing depends on them.
3. **Gate-check port, then `fs/`** — first port/replace the opcode static check
   in `tools/r1_genuine_retire_test.py` off `fs/r1_fs_core.h` (e.g. compare
   `user/ipc.c` PortFile ops directly), verify `genuine_retire` still 4/4, then
   delete `fs/r1_fs_*` and `tests/r1_fs_{core,channel}`.
4. **Opt-in channel** — remove `Makefile:341-351` block + `variant/r1_fs_channel.*`
   (depends on step 3 headers).
5. **Lua shim** — `ports/lua/e32_shim.c` only with `LUA.E32` wiring (ungated;
   either gate it first or remove the app).
6. **Stale comments** in `user/user.mk` mentioning portfilesrv/svcmgr.
7. **DOOM shim last** — only when DOOM no longer needs `doom_e32_shim.o`
   (4/11 gate dependency).
8. **Never in this pass:** kernel traps — exec-81 `PortFsRegisterThread`
   wrapper, exec 269 `PortKillExec`, exec 270 ops 2/3/4 incl. PTRINJ sink —
   they are live port bridges pinned by the pointer/cp4 gates and the EFILE
   KERN-4 constraint. `genuine/ewsrv/ewsrv_shims.cpp` + HAL shim stay as
   legitimate x86 glue. netstack stays (ESOCK absent — authored exception).

---

**2026-11-26 — supersession footer (authoritative gate is now 15 rows):** every gate count in this file that reads **11/11**, **11-gate**, **11 gates**, **13/13** or **13-row** is superseded and is not current evidence. The 11-gate dependency analysis and the deletion ordering that protects 11/11 apply unchanged to the 15-row gate. The authoritative strict gate is `python3 tools/r1_final_verify.py --strict-skip`, whose `DEFAULT_TESTS` list holds **15 rows** (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`, `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`, `doom_playtest`, `doom_multikey`, `hal_probe`), and it is green at **15 pass, 0 skip, 0 fail** with `FINAL27_RC=0` (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`). Where this file enumerates the gate's composition — selfcheck, qemu_smoke, genuine_retire and so on — read that enumeration as the 15 rows above rather than as a literal 11- or 13-item list. Standing plan decisions, control phrases (GO/NO-GO, PASS/SKIP/FAIL) and every other statement in this file are unchanged by this note; only the row count and the gate verdict are restated here.
