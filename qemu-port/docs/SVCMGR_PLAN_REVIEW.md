# SVCMGR_PLAN_REVIEW — read-only review of docs/SVCMGR_RETIREMENT_PLAN.md (2026-11-24)

**Verdict: SAFE** (no blockers; 3 minor fixes recommended before apply).
Reviewed against live sources in `qemu-port/` (read-only; `kernelhwsrv/` untouched;
no QEMU run, no commit/push, no `make clean`).

## 1. File:line claims — VERIFIED

| Plan claim | Reality | Status |
|---|---|---|
| `SvcMgrExecInstall()` `e32load.cpp:1430-1452`, sole call `qemu_variant_asic.cpp:194` | exact match; only call site | OK |
| exec-81 wrapper patch at `:1437-1442`, `PortFsRegisterThread` `:1415-1428` | exact match | OK |
| exec 268 `SvcMgrLaunchExec :1275-1313`, exec 269 `SvcMgrKillExec :1316-1317`, exec 270 `SvcMgrCtlExec :1320-1339` | exact match | OK |
| ops 0/1 = svcmgr dead/reap, 2/3 = fbblit, 4 = PTRINJ | `FBO_FBINFO=2`, `FBO_FBBLIT=3` (`qemu_fbblit.h:34-35`); op 4 emits `PTRBRIDGE: TRawEvent::Pos` (`:1331-1337`), consumed by `r1_genuine_pointer_test.py:224,306,314` | OK |
| `sSvcImg` `:1265` shared with `GenuineBootTry:1504` | exact match — keep | OK |
| `SvcMgrBootLaunch` `:1455-1467`, call already commented `:1552` | exact match | OK |
| header decls `qemu_variant.h:407-408`; asic comments `:48,:186-193` | exact match | OK |
| `user.mk` svcmgr at `:7,36-37,356-366` | exact match (comment 356-357, rules 358-366) | OK |
| `mkgrubiso.py:106` is comment-only | confirmed; staging enumerates `build/user/*.E32` (`mkgrubiso.py:120`), so dropping SVCMGR.E32 from `userland:` removes it from the ISO | OK |
| rollback `git show d26fade:<path>` for all 5 files | verified: all 5 paths exist at `d26fade` (`git cat-file -e`) | OK |

## 2. Exec-270 preservation — VERIFIED

- Slim `PortCtlExec` keeping ops 2/3/4 preserves both gate consumers: DOOM
  (`ports/doom/e32_platform.c:30` uses exec 270 ops 2/3) and the pointer test
  (op 4 `PTRBRIDGE:` lines, asserted by `r1_genuine_pointer_test.py`).
- Ops 0/1 (`E32ProcessDead`/`E32Reap`) have **no** caller outside
  `user/svcmgr.cpp` — safe to drop.
- `kern_shell.cpp` uses **direct kernel calls**, not execs: `E32Kill` `:1344,:2440`,
  `E32Reap` `:1508,:2461`, `E32ProcessDead` `:67,:2493`. Deleting execs 268/269
  does **not** break the shell's command table or its `RunImage`/`E32Run` path.

## 3. Issues

| # | Severity | Location | Issue / required fix |
|---|---|---|---|
| 1 | LOW | plan §3 step 1 | Keep the `tab[0] < 271u` table-size guard in the new `PortTrapExecInstall()` — slot 270 still needs it; the plan's snippet omits it. |
| 2 | LOW | `e32load.cpp:1267-1272` | `SvcDesc()` becomes an unused `static` after `SvcMgrLaunchExec` is deleted → `-Wunused-function` warning (no `-Werror` found in the build, so non-fatal). Delete `SvcDesc` too, or note the warning is expected. |
| 3 | MEDIUM | plan §6 risk 3 | Plan says 268/269 users are "not in any gate" — true for the authoritative 11-test gate (`r1_final_verify.py:91-103`), but six **non-gate** harnesses assert exec 268/269: `r1_desktop_test.py:186,233`, `r1_guimenu_test.py:172,217`, `r1_applaunch_test.py:167-173`, `r1_lua_test.py:202`, `r1_ported_software_test.py:64`, `r1_desktop_handoff_test.py:231`, plus `r1_cp4_ca_stress_test.py:177` (exec 269). Deleting 268/269 turns these FAIL/degraded (desktop/guimenu/applaunch have a printed fallback, so they fail soft). State this as an accepted regression, or retire those utilities in the same pass. Not a blocker for the 11/11 gate. |
| 4 | LOW | plan §1 row 19 | Doc nit: `GenuineBootLaunch` is declared locally at `qemu_variant_asic.cpp:47`, not `qemu_variant.h:421` (that is `GenuinePointerBridge`). |
| 5 | INFO | `mkgrubiso.py:120` + build dir | A stale `build/user/SVCMGR.E32` from a previous build (no `make clean` allowed) could still be staged onto the ISO; harmless once `SvcMgrBootLaunch` is deleted since no boot path reads `\SVCMGR.E32`. Confirmed. |

## 4. Gate coverage & rollback — VERIFIED

- Plan requires before **and** after: `r1_os_selfcheck.py`,
  `r1_genuine_retire_test.py` 4/4, `r1_genuine_fullchain_test.py` 14/14,
  `r1_genuine_pointer_test.py` PASS, `r1_final_verify.py --strict-skip` 11/11 —
  matches the authoritative gate in AGENTS.md plus the pointer gate. Negative
  checks (no KERN 4, `PTRBRIDGE:` present, DOOM fbblit renders, never accept
  `SessionPath r=-15`) are correct.
- Rollback is complete and verified: snapshot token + `git show d26fade:` for all
  5 files (existence checked); prior `shim_retire_backup_20261124_083011` noted.
- `user.mk`: `euser_shim.o` is also linked into `ca_test.elf` (`:205`), so the
  plan correctly leaves the euser_shim rules alone — only svcmgr rules go.
  `make userland` / `make isogrub` stay intact.

## 5. Open decisions

1. Exec 268/269 consumers (desktop/applaunch/guimenu/luaboot/killproc + 6 non-gate
   harnesses): accept soft-fail regression, or retire/re-point them in the same
   pass? Plan default (leave files, fail cleanly) is acceptable for the gate.
2. Naming: `PortTrapExecInstall()` / `PortCtlExec` are fine; keep the
   "never re-add the EFILE trampoline" caveat — the exec-81 flag-clear is the
   sanctioned shim and the plan preserves it.
