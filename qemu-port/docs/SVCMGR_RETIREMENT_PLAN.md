# SVCMGR_RETIREMENT_PLAN — retire svcmgr without reintroducing KERN 4 (2026-11-24)

**Status: reviewed SAFE — apply-ready after r1_fs glue removal settles** (per
`docs/SVCMGR_PLAN_REVIEW.md`, verdict SAFE, 3 minor fixes folded in below).
Shared files with the in-flight r1_fs glue removal: `e32load.cpp`,
`qemu_variant_asic.cpp`, `qemu_variant.h`, `Makefile`/`user.mk`.

**Scope:** read-only plan. No source edits except this report. No QEMU run, no
commit/push, no `make clean`. `kernelhwsrv/` untouched. Companion to
`docs/REMAINING_SHIMS_AUDIT.md` §3 step 2 / §4 trap warning.

## 1. Dependency map (symbol → file:line → caller)

| Symbol / slot | Defined | Declared | Called from | Role |
|---|---|---|---|---|
| `SvcMgrExecInstall()` | `variant/e32load.cpp:1430-1452` | `variant/qemu_variant.h:407` | `variant/qemu_variant_asic.cpp:194` (`Asic::Init3`) — **only call site** | Boot-time patcher: wraps exec 81, installs execs 268/269/270 |
| `PortFsRegisterThread` (exec 81 wrapper) | `e32load.cpp:1415-1428` (patch at `:1437-1442`) | — | genuine EFILE.E32 via `FsRegisterThread` (sexec.cpp:2228); `user/svcmgr.cpp:210` | Clears `KThreadFlagSystemPermanent|SystemCritical` + process flags after genuine registration → prevents `K::ESystemThreadPanic` (KERN 4) when EFILE thread exits |
| `SvcMgrLaunchExec` = exec 268 | `e32load.cpp:1275-1313` | — | `user/svcmgr.cpp:238`, `user/desktop.c:402`, `user/applaunch.c:143`, `user/guimenu.c:238`, `ports/lua/luaboot.c:24` | `FatReadPath`+`E32Run` launcher |
| `SvcMgrKillExec` = exec 269 | `e32load.cpp:1316-1317` | — | `user/svcmgr.cpp:240`, `user/killproc.c:26` (portcon) | `E32Kill` |
| `SvcMgrCtlExec` = exec 270 | `e32load.cpp:1320-1339` | — | `user/svcmgr.cpp:241-242` (ops 0/1); **shared:** ops 2/3 = `FBO_FBINFO`/`FBO_FBBLIT` → `fbblit_info_exec`/`fbblit_blit_exec` (`qemu_fbblit.cpp:71,95`) used by `ports/doom/e32_platform.c:136,245` and `user/fbblittest.c`; **op 4 = PTRINJ sink** called from `PortEwsrvPosHook` asm (`e32load.cpp:1359-1379`), logged `PTRBRIDGE: TRawEvent::Pos` | Mixed: svcmgr ops 0/1, fbblit ops 2/3 (DOOM gate), PTRINJ op 4 (pointer-test gate) |
| `SvcMgrBootLaunch()` | `e32load.cpp:1455-1467` | `qemu_variant.h:408` | `e32load.cpp:1552` — **already commented out** (`// RETIRED by r1_shim_retire_apply`) | Loads `\SVCMGR.E32`; dead code |
| `sSvcImg[512K]` buffer | `e32load.cpp:1265` | — | `SvcMgrLaunchExec:1304`, `SvcMgrBootLaunch:1460`, **`GenuineBootTry:1504`** | Shared load buffer — **keep** |
| `PortEwsrvPosHook` / `PointerInjectionActive` / `IsEwsrvProcess` | `e32load.cpp:1356-1403`, bound at `:989-1000` | `e32load.cpp:723` | EWSRV PE2 IAT rebind at load; `int 0x21` exec 270 op 4 | PTRINJ proof; independent of svcmgr but **shares exec 270 op 4** |
| `GenuineBootLaunch` / `GenuineBootSpawn` / `kGenuineBootList` | `e32load.cpp:1488-1552`; `qemu_variant_asic.cpp:64-84,195` | declared locally at `qemu_variant_asic.cpp:47` (`qemu_variant.h:421` is `GenuinePointerBridge`) | `Asic::Init3` via supervisor DThread | **Genuine replacement** for svcmgr boot/supervision: EFILE/EFSRV → ESTART/E32STRT → EWSRV → ESHELL, FAT-probed, one-file-server guard (`PortFileServerPublished()` at `:1529`) |
| `user/svcmgr.cpp` (SVCMGR.E32) | `user/user.mk:7,36-37,356-366` | — | only via `SvcMgrBootLaunch()` (retired) or manual shell run | Orchestrator + FileServer host mode; `kServices[]` portfilesrv row already commented (`svcmgr.cpp:250`) |

**Exec-slot ownership:** 268 = svcmgr-only; 269 = svcmgr-only; 270 = **shared**
(fbblit 2/3 for DOOM + PTRINJ 4 + svcmgr 0/1); 81 = genuine, wrapped by the trap.

## 2. Trap analysis — what must move out of `SvcMgrExecInstall()`

`SvcMgrExecInstall()` (asic:194) does two inseparable things today:

1. **Exec-81 `FsRegisterThread` wrapper** (`PortFsRegisterThread`). Removing it
   reintroduces `K::ESystemThreadPanic` (KERN 4) when the genuine EFILE thread
   exits → **must be preserved**. It is a port-shim fact (EFILE teardown), not a
   svcmgr feature. Move it into a standalone installer, e.g.
   `PortFsRegisterWrapInstall()`, still called from `Asic::Init3`.
2. **Exec 270 hosting.** Only ops 0/1 (dead/reap) are svcmgr's. Ops 2/3 (fbblit,
   DOOM `e32_platform.c`) and op 4 (PTRINJ sink, `r1_genuine_pointer_test.py`
   asserts the `PTRBRIDGE:` lines) must keep a ring-0 home. Keep exec 270
   installed by a slim `PortCtlExec` that dispatches **only** ops 2, 3, 4.

Execs 268/269 have no remaining production caller once svcmgr is gone
(`desktop.c`/`applaunch.c`/`guimenu.c`/`luaboot.c`/`killproc.c` are manually
launched shell utilities, not in any gate — see risks).

**Genuine replacement for svcmgr's role:** `GenuineBootLaunch()` on the
`GenuineBootSpawn()` supervisor DThread already runs the genuine chain
(EFILE/EFSRV→ESTART→EWSRV→ESHELL) with the single-file-server guard; genuine
estart/domainsrv provide ordered startup + supervision. svcmgr adds nothing
on the production path.

## 3. Ordered edit steps (minimal set, exact strings)

Baseline gates **before** any edit: `python3 tools/r1_os_selfcheck.py`,
`r1_genuine_retire_test.py` (expect 4/4), `r1_genuine_fullchain_test.py`
(14/14), `r1_genuine_pointer_test.py` (PASS), `r1_final_verify.py
--strict-skip` (11/11). Snapshot first:
`cp -a qemu-port /tmp/opencode/svcmgr_retire_backup_<ts>` (record token).

1. `variant/e32load.cpp` — split the installer. Replace the body of
   `SvcMgrExecInstall()` (`:1430-1452`) with `void PortTrapExecInstall()` that
   keeps **only** the exec-81 wrapper block (`:1437-1442`) and installs
   **only slot 270** pointing at a new slim ctl. The new
   `PortTrapExecInstall()` **must keep the `tab[0] < 271u` table-size guard**
   around the slot-270 install (slot 270 still needs it):
   ```c
   if (tab[0] < 271u) return;            // guard: table must hold slot 270
   static TInt PortCtlExec(TInt aOp, TInt aArg)
       {
       if (aOp == (TInt)FBO_FBINFO) return fbblit_info_exec(aArg);
       if (aOp == (TInt)FBO_FBBLIT) return fbblit_blit_exec(aArg);
       if (aOp == 4) { KPrintf("PTRBRIDGE: TRawEvent::Pos x=%d y=%d\r\n", ...); return KErrNone; }
       return KErrArgument;
       }
   tab[270] = (TAny*)PortCtlExec;
   ```
   Also delete `SvcDesc()` (`e32load.cpp:1267-1272`) — it becomes an unused
   `static` once `SvcMgrLaunchExec` is gone (`-Wunused-function` warning,
   non-fatal, but delete it to keep the build clean).
   Delete `SvcMgrLaunchExec` (`:1274-1313`), `SvcMgrKillExec` (`:1315-1317`),
   svcmgr ops 0/1 in `SvcMgrCtlExec` (delete `:1319-1339`), and
   `SvcMgrBootLaunch` (`:1454-1467`). Keep `sSvcImg` (`:1265`, used by
   `GenuineBootTry:1504`), `PortEwsrvPosHook`, `PointerInjectionActive`,
   `PortFsRegisterThread`. Delete the dead commented line at `:1552`.
   Update the header comment at `:1258-1263`.
2. `variant/qemu_variant.h:407-408` — replace declarations
   `void SvcMgrExecInstall();` / `void SvcMgrBootLaunch();` with
   `void PortTrapExecInstall();`.
3. `variant/qemu_variant_asic.cpp:194` — replace exact string
   `    SvcMgrExecInstall();` with `    PortTrapExecInstall();` and refresh the
   comments at `:48` and `:186-193`.
4. `user/user.mk` — remove `build/user/SVCMGR.E32` from `userland:` (`:7`),
   the `.PHONY: svcmgr` + `svcmgr:` lines (`:36-37`), and rules `:356-366`.
   (mkgrubiso.py:106 is a comment only; refresh if it stages SVCMGR.E32.)
5. Delete `user/svcmgr.cpp` **last**, only after all gates pass on the tree
   without the execs (step 1-4). Its `kServices[]` is already empty of live
   rows, so the image is inert either way.

Net edit count: **4 files** (e32load.cpp, qemu_variant.h, qemu_variant_asic.cpp,
user.mk) + 1 deletion (user/svcmgr.cpp).

## 4. Rollback

- Snapshot: `/tmp/opencode/svcmgr_retire_backup_<ts>` (pre-edit `cp -a`), token
  recorded in the session log; restore = copy back the 4 edited files + svcmgr.cpp.
- Git: `git show d26fade:variant/e32load.cpp`, `git show
  d26fade:variant/qemu_variant.h`, `git show
  d26fade:variant/qemu_variant_asic.cpp`, `git show d26fade:user/user.mk`,
  `git show d26fade:user/svcmgr.cpp` (all exist at d26fade; e32load.cpp:1552
  and svcmgr.cpp:250 already carry the RETIRED comments there).
- Prior snapshot `/tmp/opencode/shim_retire_backup_20261124_083011`
  (token `ROLLBACK-PORTFILESRV-20261124_083011`) remains valid for the
  file-server cut.

## 5. Gate evidence required

| Gate | Before | After |
|---|---|---|
| `r1_os_selfcheck.py` | PASS | PASS |
| `r1_genuine_retire_test.py` | 4/4 | 4/4 (ESTART/ESHELL/EFSRV/EUSER) |
| `r1_genuine_fullchain_test.py` | 14/14 | 14/14 |
| `r1_genuine_pointer_test.py` | PASS (`PTRBRIDGE:` Pos line, no FAULT) | PASS — proves exec 270 op 4 survived |
| `r1_final_verify.py --strict-skip` | 11/11 | 11/11 — proves DOOM fbblit ops 2/3 + KERN-4-free EFILE teardown survived |

Explicit negative checks after: no `KERN 4` / `ESystemThreadPanic` in any log
(exec-81 wrapper live), `PTRBRIDGE: TRawEvent::Pos` present (op 4 live), DOOM
framebuffer renders (ops 2/3 live). Never accept `SessionPath r=-15` as IPC
success.

## 6. Risks

- **Exec 270 is shared** — deleting the whole slot breaks DOOM (fbblit 2/3) and
  the pointer test (op 4). Only ops 0/1 die with svcmgr.
- **KERN 4 trap** — the exec-81 wrapper must move, not vanish; verify
  `FsRegister: cleared system-permanent/critical flags` still prints.
- **Accepted regression — execs 268/269 consumers.** Deleting execs 268/269
  turns **six non-gate harnesses soft-FAIL**: `r1_desktop_test.py:186,233`,
  `r1_guimenu_test.py:172,217`, `r1_applaunch_test.py:167-173`,
  `r1_lua_test.py:202`, `r1_ported_software_test.py:64`,
  `r1_desktop_handoff_test.py:231` (exec 268), plus
  `r1_cp4_ca_stress_test.py:177` (exec 269). These harnesses are **outside the
  authoritative 11/11 strict gate** (`r1_final_verify.py`); desktop/guimenu/
  applaunch print a fallback, so they fail soft. **The shell is unaffected:**
  `kern_shell.cpp` calls `E32Kill`/`E32Reap`/`E32ProcessDead` directly
  (`:67,:1344,:1508,:2440,:2461,:2493`), never execs 268/269. Accepted as a
  known regression for the 11/11 gate; optionally retire/re-point those
  utilities in the same pass. Source utilities (`desktop.c`, `applaunch.c`,
  `guimenu.c`, `luaboot.c`, `killproc.c`) lose their launch/kill path and fail
  cleanly (`r<0`) if manually run.
- `SVCMGR.E32` may still be staged on the ISO by `mkgrubiso.py`; harmless once
  `SvcMgrBootLaunch` is gone, but confirm no boot path reads `\SVCMGR.E32`.
- Never re-add the EFILE trampoline (EFILE single-owner decision); the exec-81
  flag-clear is the sanctioned shim and stays.
- `kernelhwsrv/` read-only; no commit/push/`make clean` performed by this plan.

## Supersession (2026-11-26) — the gate is 15 rows, not 11

The `11/11` strict-gate token used throughout this plan (§5 gate row, §6 risk 3 at
`:144`, "the authoritative 11/11 strict gate") is historical. The authoritative gate
`python3 tools/r1_final_verify.py --strict-skip` now carries **15** rows in
`DEFAULT_TESTS` (`tools/r1_final_verify.py:134-150`) and its last full run is
`/tmp/opencode/final27_gate.log`: `OVERALL: PASS (15 pass, 0 skip, 0 fail/not-run)`,
`FINAL27_RC=0`. The accepted-regression analysis for execs 268/269 still holds — those
harnesses are outside the gate at either row count — but "11/11" in this document now
means the retired baseline, not current authority. Nothing above is rewritten.
