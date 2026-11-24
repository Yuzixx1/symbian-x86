# Boot-chain verification (READ-ONLY, post-landing)

Date: 2026-11-24. Workspace: `qemu-port/`. No source/test files modified;
`kernelhwsrv/` untouched; nothing committed/pushed; `make clean` not run.
QEMU tests were run **serially** (no lock holder: `.sybian/coordinator/state.json`
`qemu_lock = null`); the coordinator QEMU lock file was not changed.

## 1. Change inventory (`git status`)

Modified (unstaged): `.sybian/coordinator/state.json`, `docs/DOCS_INDEX.md`,
`tools/e32read.py`, `variant/e32load.cpp`, `variant/qemu_display.cpp`,
`variant/qemu_variant.h`, `variant/qemu_variant_asic.cpp`.
Untracked (pre-existing): `AGENTS.md`, `.sybian/qemu-slot.lock`,
`docs/GENUINE_{BUILD_REVIEW,PROGRESS,USERSPACE}.md`, `genuine/`,
`tools/genuine_build.py`, `tools/r1_genuine_{efsrv,eshell,ewsrv,retire}_test.py`,
archive tarball.

Relevant boot-chain code:

* `variant/qemu_variant_asic.cpp::Init3`: `SvcMgrExecInstall()` then
  `GenuineBootSpawn()` (was a direct `SvcMgrBootLaunch()`), followed by
  `ShellSpawnFull()`. The spawn creates an `EThreadSupervisor` DThread
  (`iInitialThreadPriority = 12`, 0x8000 supervisor stack, named
  `GenuineBoot`) which calls `GenuineBootLaunch()`. Reason: `E32Run()` takes
  kernel heap/container mutexes, which fault (`EMutexWaitNotDThread=98`) from
  Init3's nanokernel start thread.
* `variant/e32load.cpp::GenuineBootLaunch` (lines ~1231-1305): probes the FAT
  boot volume (defaults to `C:`) in genuine order:
  `\EFILE.E32`, `\EFSRV.E32` (one file-server slot only), `\ESTART.E32`,
  `\E32STRT.E32`, `\EWSRV.E32` (calls `display_ensure_graphics()` first),
  `\ESHELL.E32`. Each hit is loaded via the same `FatReadPath` + `E32Run`
  path as the shell's `RunImage`; logs `GenuineBoot: <name> r=.. pid=..`.
* **Fallback condition:** only `iFileServer` entries decide it. If neither
  `\EFILE.E32` nor `\EFSRV.E32` loads with `KErrNone`, `GenuineBootLaunch()`
  calls `SvcMgrBootLaunch()` (trusted execs installed by `SvcMgrExecInstall()`,
  then `SVCMGR.E32` → `\PORTFILESRV.E32`). If a genuine file server did load,
  it logs `genuine file server up, svcmgr fallback skipped` and returns.
  Absent EWSRV/ESHELL leave the hand-written console shell + GUI execs.
  Detection is FAT-presence based, so shim-only disks take the same path as
  before (probe logs + fallback).
* Supporting changes: `E32Run()` grants `ECapabilityProtServ` on every loaded
  E32 process so genuine `!`-prefixed servers can publish; PE2 import slots
  raised 16 → 512 (`e32read.cpp`, `TE32Info`, `E32Validate`); `E32Run()` maps
  the VBE LFB into user mode for genuine EWSRV's direct framebuffer writes.

## 2. Test results (all run serially)

| Test | Result | Evidence |
|---|---|---|
| `tools/r1_os_selfcheck.py` | **PASS** | `OVERALL: PASS`, 236.2s; build, isogrub, qemu_smoke, cp4_ca, cp4_stress, selftest all PASS |
| `tools/r1_qemu_smoke.py` | **PASS** | exit 0, `RESULT: PASS`; EUSER 2503 exports, imports bound, no Exc/PanicKernExec |
| `tools/r1_cp4_ca_test.py` | **PASS** | exit 0, `RESULT: PASS`; CP4 CActive ordinals 887..626 round trip OK |
| `tools/r1_cp4_ca_stress_test.py` | **PASS** | via selfcheck (33.6s) |
| `tools/r1_selftest.py` | **PASS** | via selfcheck (91.1s) |
| `tools/r1_genuine_efsrv_test.py` | **PASS** | exit 0; genuine F32 BPB/FAT probes + `!FileServer` IPC round trip, no Exc/Panic/FAULT |
| `tools/r1_genuine_ewsrv_test.py` | **FAIL (harness only)** | exit 1; all 8 functional markers PASS (incl. `WINSERVER IPC server started`, 424 PE2 imports, no faults); only `[FAIL] screendump` |
| `tools/r1_genuine_eshell_test.py` | **PASS** | exit 0; ESTART/ESHELL load, `C:\>` prompt, HELP commands, `E32Main returned 0`, no Exc/Panic/FAULT/#PF |
| `tools/r1_genuine_retire_test.py` | **PASS** | `RESULT: PASS` (3 passed, 1 skipped, 0 failed): ewsrv, estart, euser PASS; efsrv SKIP (EFSRV.DLL/EFAT32.FSY not built) |

EWSRV FAIL root cause: the new boot chain now auto-launches `\EWSRV.E32` at
boot, so this test's manual `ewsrv` shell launch produces a *second* EWSRV
instance. Its sequential `wait_for()` trips on the duplicated marker
(`QEMU drive failed: missing b'EWSRV: WINSERVER IPC server started' ...`) and
skips `qmp_screendump()`, leaving `shot_ok = False`; the stale PNG from a prior
run is still on disk. The post-run full-log marker scan passes every genuine
marker, and the retire test's independent EWSRV QMP screendump check PASSes
(`screendump renders a window`). This is a test-expectation issue exposed by
the new boot chain, **not** a kernel/chain functional regression.

Skipped (deliberately, cost): long game/DOOM/network/visual matrices and other
non-cheap `r1_*` scripts (e.g. Doom playtests, large-file, netstack, GUI/hangman
matrices). The selfcheck already covers the core smoke/CP4/stress/selftest set.

## 3. ISO verification and boot

`build/symbian-grub.iso` exists (75,874,304 B). After the selfcheck's
`make` + `make isogrub`, ISO mtime (00:48:16) was newer than the kernel build
(00:48:15). A later `make build/kernfull.elf` invoked by `cp4_stress` relinked
`kernfull.elf` (00:50:07), so its mtime is now newer than the ISO; SHA-256 of
the ISO's `/BOOT/KERNFULL.ELF;1` and of the current `build/kernfull.elf` are
**identical** (`9bd12daf26b576648ddfdbec1fe3d201438e752cd96a1cb829b866c597e7bc0d`,
1,017,148 B), so the ISO is not stale content-wise.

Booted once with the standard helper `tools/boot_grub.sh` (exit 124 = expected
interactive timeout). Serial log `/tmp/opencode/verify_iso_boot.log`;
fault-marker count (`Exc `, `PanicKernExec`, `FAULT`, `#PF`) = **0**.

Boot marker order (ISO / shim-only disk):

```
Booting `Symbian EKA2 full kernel (genuine)'
KEKA2-QEMU bridge: 32-bit protected mode, paging off, COM1 ok
X: mounted (programs from GRUB module)
SvcMgr: launch execs installed (268/269/270)
KERN spawning genuine boot DThread...
GenuineBoot: probing boot volume for the genuine startup chain
SvcMgr: SVCMGR.E32 not found r=-1
Symbian EKA2 x86 full kernel -- interactive console
C:/>
```

Genuine-chain marker order observed on the staged disks (genuine EWSRV test):
`KERN spawning genuine boot DThread` → `GenuineBoot: probing ...` →
`EWSRV: genuine window server starting` → `E32Main entered` →
`WINSERVER IPC server started` → `EWSRV-EDISP: LFB=fd000000 1024x768 bpp=32`
→ `CScreenDriver::New() ok` → `entering active scheduler (window server READY)`
→ `GenuineBoot: EWSRV.E32 r=0 pid=9` → `SvcMgr: SVCMGR.E32 not found r=-1`
(fallback: no genuine file server staged on that disk).
On the ESTART/ESHELL disk: `GenuineBoot: ESTART.E32 r=0 pid=9` →
`GenuineBoot: ESHELL.E32 r=0 pid=11` → same fallback.

## 4. Regressions / findings

1. **No functional regression found** in the boot-chain changes: every core
   selfcheck, smoke, CP4, genuine EFSRV/ESTART/ESHELL and retire-gate test
   passes with zero guest faults.
2. `r1_genuine_ewsrv_test.py` now reports FAIL (screendump) because the boot
   chain auto-launches EWSRV, duplicating the test's manual launch. Functional
   markers all pass; harness needs to tolerate boot-time auto-launch. Report
   only — not fixed here.
3. No single test staged the complete `\EFILE/\EFSRV → \ESTART → \EWSRV →
   \ESHELL` set on one disk, so the *full* in-order chain in one boot was not
   directly observed; per-component boot markers were verified across boots.
   EFSRV retire gate is SKIP (genuine `EFSRV.DLL`/`EFAT32.FSY` not built),
   though the F32/EFSRV IPC proof test passes.
4. `kernfull.elf` mtime is now newer than the ISO because a later `make`
   relinked it; embedded kernel bytes are identical (hash above). Re-run
   `make isogrub` if a strict mtime ordering is required.
