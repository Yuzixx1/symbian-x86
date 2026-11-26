# KERNEL HOOK FREEZE — overnight wave 1 (P0 baseline)

**Date:** 2026-11-25. **Status: FROZEN.** Per
[PLAN_OVERNIGHT_WAVE1.md](PLAN_OVERNIGHT_WAVE1.md) "Do NOT": no kernel hook
edits — exec 81/269/270, the boot contract, and this freeze list are NO-GO
for every overnight brief. `kernelhwsrv/` is READ-ONLY (its own git repo;
`git -C kernelhwsrv status --porcelain` must stay empty).

## Frozen kernel hooks (do not edit, do not remove, do not re-slot)

| Hook | Slot | Installed by | Purpose | Reference |
|---|---|---|---|---|
| `FsRegisterThread` wrapper | exec **81** | `PortTrapExecInstall()` | Clears critical flags around genuine exec 81 (0 KERN 4 / 0 FAULT) | [variant/e32load.cpp:1447-1458](../variant/e32load.cpp) |
| `PortKillExec` → `E32Kill` | exec **269** | `PortTrapExecInstall()` | cp4 peer kill; ring-3 `exec2(269)` kill path | [variant/e32load.cpp:1380,1464](../variant/e32load.cpp) |
| Slim `PortCtlExec` ops 2/3/4 | exec **270** | `PortTrapExecInstall()` | FBO_FBINFO / FBBLIT / PTRINJ (DOOM blit + pointer-injection proof sink) | [variant/e32load.cpp:1259-1470](../variant/e32load.cpp) |

## Frozen boot contract (do not edit)

`GenuineBootLaunch()` ([variant/e32load.cpp:1541](../variant/e32load.cpp)),
spawned by Init3 on a supervisor DThread after `PortTrapExecInstall()`, must
launch the genuine chain in this exact order inside one boot:

    EUSER.DLL resident -> EFILE.E32 (!FileServer) -> ESTART.E32 ->
    EWSRV.E32 -> ESHELL.E32 (banner + C:\> prompt)

followed by the kernel marker `genuine file server up, svcmgr fallback
skipped` — no svcmgr/portfilesrv fallback, no Exc/PanicKernExec/FAULT.
Proven by `tools/r1_genuine_fullchain_test.py` (14/14).

## Rules

1. No edits to exec slots 81/269/270 or any wrapper/handler they install.
2. No edits to the `GenuineBootLaunch()` order or the fallback-suppression
   contract.
3. `kernelhwsrv/` is never written, committed or pushed.
4. Violating this freeze is an automatic NO-GO for the wave
   ([PLAN_OVERNIGHT_WAVE1.md](PLAN_OVERNIGHT_WAVE1.md), rule 6).
