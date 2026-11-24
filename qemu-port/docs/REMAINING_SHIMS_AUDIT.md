# REMAINING_SHIMS_AUDIT — what is still hand-written in the production path (2026-11-24)

**Scope:** read-only inventory. No edits except this report, no QEMU, no commit/push.
`kernelhwsrv/` untouched. Sources: `docs/SHIM_RETIREMENT_RESULT.md`,
`SHIM_RETIRE_READINESS.md`, `SHIM_RETIREMENT_PLAN.md`, `SHIM_RETIRE_APPLY_REVIEW.md`,
`GENUINE_USERSPACE.md` §2, `GENUINE_PROGRESS.md` §1, and current
`variant/e32load.cpp`, `user/svcmgr.cpp`, `variant/qemu_variant_asic.cpp`, `Makefile`.

## 1. What the guarded apply actually changed

Only **two lines**, both bypasses, not deletions:
- `variant/e32load.cpp:1552` — `SvcMgrBootLaunch();` fallback commented out
  (`// RETIRED by r1_shim_retire_apply`). `portfilesrv + r1_fs` retire gate EFSRV/F32 PASS.
- `user/svcmgr.cpp:250` — `\PORTFILESRV.E32` row in `kServices[]` commented out
  (same shared edit set as `r1_fs_*`; that standalone apply correctly REFUSEd rc 2).
- `svcmgr` was applied then **rolled back** (out of scope): `variant/qemu_variant_asic.cpp:194`
  `SvcMgrExecInstall();` is **still live**.
- `winsrv` was **never applied** — no apply path; removal is a file delete + human review.
- Genuine `kGenuineBootList[]` (EFILE/EFSRV/ESTART/E32STRT/EWSRV/ESHELL),
  `GenuineBootSpawn()` and `ShellSpawnFull()` are untouched.

So "retired" = boot path bypassed. The shim **files and their kernel objects are still
compiled and linked**: `Makefile:326` still adds `r1_fs_core.o`/`r1_fs_fat.o`, and
`variant/qemu_variant_asic.cpp:151-164` still constructs `R1Fs::FatBackend`/`R1Fs::Core`
(`GenuineBootLaunch()` still reads `PortFileServerPublished()` at `e32load.cpp:1529`).

## 2. Inventory

Legend: **A** = obsolete hand-written Symbian replacement (retire); **B** = legitimate
x86/PC port bridge (keep); **C** = test/research-only (not production).

| Item | Path(s) | Role in production path | Genuine replacement | Gate status | Need to retire? |
|---|---|---|---|---|---|
| **A portfilesrv** | `user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, `services/r1_service_host.*` | Ring-3 owner of hand-rolled `!FileServer`; boot row + fallback now commented out; image/objects still present | genuine `efile.exe`/`efsrv.dll`/`efat32.fsy` from `kernelhwsrv/userlibandfileserver/fileserver/` | EFSRV/F32 **PASS** (retire applied) | Yes — delete files/tool/flat image once bypass proven stable |
| **A r1_fs_\*** | `fs/r1_fs_core.{cpp,h}`, `r1_fs_backend.h`, `r1_fs_server.{cpp,h}`, `r1_fs_fat.{cpp,h}`, `r1_fs_channel*.{cpp,h}`, `variant/r1_fs_channel.{cpp,h}`, `variant/fat32.cpp` (kernel FAT glue) | Private `R1Fs` wire opcodes (FileOpen=30…), `CServer2` server, FAT32 adapter; `r1_fs_core.o`/`r1_fs_fat.o` linked unconditionally (`Makefile:326`) and used by `qemu_variant_asic.cpp` mount | genuine `RFs`/`RFile` ordinals + `TFsMessage` server; `R1Fs::Backend` kept only as unit-test seam | EFSRV/F32 **PASS** | Yes — drop objects/glue from kernel after file-server cut; keep `Backend` for tests |
| **A svcmgr** | `user/svcmgr.cpp`; `e32load.cpp:1258-1452` (`SvcMgrLaunch/Kill/CtlExec`, `SvcMgrExecInstall`, `SvcMgrBootLaunch`); `qemu_variant_asic.cpp:194` | Still installed at boot: repurposes genuine slow execs 268/269/270; **also wraps genuine FsRegisterThread exec 81** and hosts PTRINJ proof sink (op 4 at `e32load.cpp:1331`) | genuine `e32strt.exe`/`estart`, `eshell.exe`, `domainsrv.exe` | ESTART/ESHELL **PASS**, but apply **rolled back** | **Yes** — next retirement, with care (see §4) |
| **A winsrv** | `user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp` (GUI execs 249/250/251, event ring 271/272/273) | Hand-written PE2 compositor / display-list transport; still serves GUI apps and the DOOM `guishost.h` fallback | genuine `kernel/eka/ewsrv/ewsrv.exe` + QEMU `edisp` LDD/PSL | EWSRV **PASS**, but **no apply path** (file removal + human review) | Yes — after `edisp` port + external-client window gate |
| **A euser static shim** | `user/euser_shim.cpp`, `user/e32base.h`/`e32cmn.h` (declarations) | Static members linked into import-free user E32s; genuine `EUSER.DLL` already resident (2503 exports) | genuine `EUSER.DLL` ordinals via real PE2 imports | EUSER **PASS** | Yes — separate closure: make `elf2e32.py` emit PE2 imports, then delete shim |
| **A netstack** | `user/netstack.c`+`netstack_iat.S`, `dnsresolv.c`, `ping.c`, `httpget.c`, `telnet.c`, `wget.c`, `ntpc.c`, `dhcpc.c`, `tools/mke32_netstack.py` | Authored IPv4/IPv6 stack over `ipc.c` | **none** — ESOCK/commsdb/Bluetooth absent from this `kernelhwsrv/` snapshot | no genuine gate | **No** — explicit authored exception; keep "Symbian-shaped ABI" label (plan §3 step 7) |
| **B input** | `variant/qemu_mouse.cpp`, `variant/qemu_variant_int.cpp`, `kern_shell.cpp` PS/2 ring, `qemu_gui_exec.cpp` button bridge | Real 8042/PS/2 keyboard+mouse IRQ handling on QEMU | N/A — board-level hardware driver, exactly what a new ASSP/variant supplies | smoke/mouse/desktop PASS | No keep — legitimate port bridge |
| **B audio** | `variant/qemu_audio.cpp`, `qemu_audio_exec.cpp`, `user/audiopcm.c`, `soundplr.c`, `music.c`, `morse.c` | AC'97 PCM-OUT BDL/DMA engine + exec-240; PC audio hardware | N/A — no Symbian audio server in tree | DOOM audio/audio PCM PASS | No keep — port bridge |
| **B storage** | `variant/qemu_ata.cpp`, `qemu_ahci.cpp`, `qemu_virtio_blk.cpp`, `qemu_usb*.cpp`, `qemu_xhci.cpp` | ATA PIO / AHCI / virtio-blk / USB block devices for `FatReadPath` and boot media | N/A — board drivers | FAT-IO / smoke PASS | No keep — port bridge (do not confuse with `fat32.cpp` above) |
| **B display/network/misc** | `qemu_display.cpp`, `qemu_fbblit.cpp`, `qemu_nic.cpp`, `qemu_virtio_net.cpp`, `qemu_netbridge.cpp`, `qemu_virtio_rng.cpp`, `qemu_pci.cpp`, `qemu_assp.cpp`, `qemu_variant_rom.cpp`, `e32load.cpp` loader | VBE framebuffer, blit exec, guest NIC/link layer, PCI/ASSP/variant, PE2 loader | N/A — kernel/variant port layer | all gates PASS | No keep — legitimate x86/PC glue |
| **B kern_shell** | `variant/kern_shell.cpp`, `ShellSpawnFull()` | Debug console NThread from `Asic::Init3`; VGA teletype + line editor | N/A — **not** a Symbian server fork; plan §3 allows it to remain as a debug aid | smoke/selfcheck PASS | No keep |
| **B user runtime** | `user/runtime.c`, `ipc.c`, `malloc.c`, `sleep.c`, `chunk*.c`, `interact.c`, shell/test apps | Freestanding C runtime + IPC bridge for ported apps | N/A — generic port runtime | n/a | No keep |
| **C r1_g_\*** | `variant/r1_g_launch.cpp`, `r1_g_objects.cpp`, `r1_g_bootstrap.cpp` | Explicit opt-in genuine-G research provisioning; `R1_G_ASSET_DIR` empty by default (`Makefile:334-347`) | genuine E32 launch research | n/a | Not in production; leave off |
| **C r1_fs channel** | `variant/r1_fs_channel.cpp`, `fs/r1_fs_channel*.h` | Opt-in `R1_FS_CHANNEL=0`, excluded from default kernfull (`Makefile:349-363`) | n/a | n/a | Not in production |
| **C gate/test harness** | `tools/r1_*.py`, `tests/` | Verification tools (retire, fullchain, DOOM, per-subsystem) | n/a | n/a | Keep — do not delete with shims |

## 3. Recommended retirement order (dependency-first)

1. **Consolidate the applied file-server cut** (done at boot; still to do: remove dead
   files/objects and the `qemu_variant_asic.cpp` `R1Fs::*`) — only after a clean
   `r1_final_verify.py --strict-skip` on the current tree.
2. **svcmgr** — re-run the guarded apply/review; edit `qemu_variant_asic.cpp:194`,
   then remove `SvcMgr*Exec` handlers and execs 268–270 with the file. Preserve the
   exec-81 wrapper until EFILE teardown is proven without it (see risks).
3. **winsrv** — port `edisp` LDD/PSL, pass EWSRV external-client window gate, then
   hand-delete `winsrv.c`/`guishost.h`/`qemu_windowsrv.*`/GUI execs and fix the
   ~12 GUI users + DOOM `guishost.h` fallback. **Human review; no apply path.**
4. **EUSER static shim** — separate closure via real PE2 imports.
5. **netstack** — never retire; ESOCK absent.

## 4. Risks and caveats

- The two applied edits are **bypasses, not deletions**: shim files and kernel objects
  remain linked; a disk without staged genuine EFILE/EFSRV no longer boots. Keep a
  rollback snapshot (`/tmp/opencode/shim_retire_backup_20261124_083011`, token
  `ROLLBACK-PORTFILESRV-20261124_083011`) until the full gate is green.
- `SvcMgrExecInstall()` also patches the genuine **exec 81 `FsRegisterThread`**
  wrapper that clears system-permanent/critical flags so EFILE teardown gives
  0 FAULT (`K::ESystemThreadPanic=4`), and exec 270 op 4 is the PTRINJ proof sink.
  Blindly removing it reintroduces KERN 4 / breaks `r1_genuine_pointer_test.py`.
- Gate snapshots conflict: `FINAL_GATE_STATUS.md` (03:26) still shows EFSRV FAIL
  (`SessionPath r=-15`); `GENUINE_PROGRESS.md` and current AGENTS.md report all green.
  `SHIM_RETIREMENT_RESULT.md` records `r1_final_verify.py --strict-skip` 10 PASS /
  1 FAIL with `cp4_stress` proven **pre-existing** (A/B stash). Use a fresh
  `--strict-skip` run as the authority; never accept `SessionPath r=-15` as IPC success.
- Winsrv removal has the widest blast radius (GUI apps + DOOM); do it last among
  servers. After step 5 the console may stay as a debug aid; it is not a fork.
- Never re-add the EFILE trampoline (EFILE single-owner decision). `kernelhwsrv/`
  is read-only throughout; no commit/push/make clean was performed here.
