# Genuine userspace: architecture and retirement plan

**Status:** design / policy document. It records a *project rule* from the user,
maps every current qemu-port userspace shim to its genuine Symbian counterpart
in the read-only `kernelhwsrv/` tree, and gives an ordered plan to retire each
shim. It does not change any source. `kernelhwsrv/` is read-only in this port;
all work described here happens in `qemu-port/`.

---

## 1. The rule, and why it exists

> **Everything above the kernel must be actual Symbian code. If the kernel
> works, the rest should too. Changing userspace breaks compatibility with the
> other Symbian architecture ports.**

Symbian EKA2 was built as a portable microkernel plus a *singleton* set of
user-side servers and libraries. The architecture split is visible in the
source tree itself:

- `kernelhwsrv/kernel/eka/` contains the EKA2 kernel and its per-architecture
  slices: `bx86`, `bx86gcc`, `bmarm`, `bwins`, `eabi`, `common`, `nkern`,
  `drivers`, and the two user-side components `euser/` and `ewsrv/`.
- `kernelhwsrv/halservices/hal/` and `kernelhwsrv/bsptemplate/asspandvariant/`
  are the *only* places a new board or CPU is expected to differ: HAL data and
  the ASSP/variant (GPIO, IIC, power, DMA, interrupts). `bsptemplate` is literally
  a template for a new port — the "template_assp" and "template_variant"
  directories are what you copy, not the file server or window server.
- `kernelhwsrv/userlibandfileserver/` holds the shared, arch-independent
  services: the file server (F32), the startup process, the text shell and the
  domain manager. They are compiled from the same `.mmp` sources for every
  board (`PRJ_PLATFORMS BASEUSERDEFAULT`, `GENERIC_MARM`, `WINS`, `GENERIC_X86`).

Consequently the **port layer** for a new machine is: the kernel/HAL/variant,
the board drivers, and the boot media. The user servers (F32, EWSRV, EUSER,
ESTART, ESHELL, DOMAINSRV) are inherited unchanged. That is why "if the kernel
works the rest should too" is a real invariant and not a slogan: EKA2 user
code talks to the kernel only through the published executive/IPC ABI, and
that ABI is what `kernelhwsrv/` fixes per architecture. When we write our own
`winsrv.c`, `portfilesrv.cpp`, `netstack.c` or `svcmgr.cpp`, we are not merely
adding a convenience layer — we are **forking the Symbian user ABI**. That fork
is invisible to *this* port (we control both ends) but it is invisible to the
*other* ports too: they cannot consume our servers, and we cannot consume
their E32 images without our shims reimplementing the same R-class contracts.
Every shim is therefore technical debt against portability, and the goal of
this document is to pay it down in dependency order.

The single exception is networking (`ESOCK`), which is genuinely **absent**
from this `kernelhwsrv/` snapshot. That is the one place where this port must
author code, and §2 and §6 treat it explicitly as authored code behind a
Symbian-shaped ABI rather than pretend it is "genuine".

---

## 2. Subsystem map: genuine source → current shim → retirement

Legend: **genuine** = lives in read-only `kernelhwsrv/`; **shim** = a file in
`qemu-port/` that replaces it today; **plan** = how we retire the shim.

### 2.1 File server / F32

| | |
|---|---|
| Genuine | `kernelhwsrv/userlibandfileserver/fileserver/` — `sfsrv/` (`efsrv.mmp` → `efsrv.dll`, the `RFs`/`RFile` client library: `cl_*.cpp`), `sfile/` (`efile.mmp` → `efile.exe`, the server engine: `sf_*.cpp`), `sfat32/` (`efat32.mmp` → `efat32.fsy`, the FAT32 file system driver: `sl_*.cpp`, `fat_table32.cpp`), plus `sfat/`, `srofs/`, `srom/`, `fs_utils/`, `estart/`, `etshell/`. |
| Shim | `user/portfilesrv.cpp` (ring-3 owner of `!FileServer`, uses `CServer2::Start` but an injected `FatBackend`), `fs/r1_fs_core.cpp`, `fs/r1_fs_fat.cpp`, `fs/r1_fs_server.cpp`, `fs/r1_fs_channel*.cpp`, `fs/r1_fs_backend.h`, and the kernel-side FAT glue `variant/fat32.cpp` (`FatReadPath` / `FatWritePath` / `FatMount`) plus `variant/r1_fs_channel.cpp`. The wire opcodes in `fs/r1_fs_core.h` (`FileOpen=30`, `FileRead=34`, `FileWrite=35`, …) mirror F32 function numbers but are our own frame format. |
| Plan | Three-step: (a) adopt the genuine `efsrv.dll` **client** by binding its 421 ordinals from `bx86gcc/efsrv*.def` so applications link to the real `RFs`/`RFile`; (b) build the genuine `efile.exe` server + `efat32.fsy` driver with `-m32` and run them as the `!FileServer` process, replacing `portfilesrv.cpp`; (c) replace the kernel-side `FatReadPath` calls with a ring-3 FAT adapter that presents the `efat32.fsy` PSL interface over a device handle. The `R1Fs::Backend` contract then becomes a *test seam*, not the production path. |

### 2.2 Window server

| | |
|---|---|
| Genuine | `kernelhwsrv/kernel/eka/ewsrv/` — `ewsrv.mmp` → `ewsrv.exe` (`ws_main.cpp`, `ws_win.cpp`, `ws_dat.cpp`, `ky_tran.cpp`, `co_cli.cpp`, `co_twin.cpp`, …), linked against `econs.lib`, `ektran.lib`, `hal.lib`, `efsrv.lib`, `domaincli.lib`. Its `bld.inf` also builds `../drivers/edisp/` (`edisp`, `edisp_lib`, `edisp_vt100.mmp`). |
| Shim — **RETIRED** | `user/winsrv.c` (ring-3 PE2 compositor), `user/guishost.h` (display-list ABI), `user/wsclient.c` + `user/wsclient.h`, `variant/qemu_windowsrv.cpp` + `variant/qemu_windowsrv.h` (kernel event ring, slow execs 271/272/273) and the GUI execs in `variant/qemu_gui_exec.cpp` (249/250/251) were a hand-written `CWsScreen`-equivalent transport, not the EKA2 `RWindow`/`CWsScreenDevice` ABI. **All seven files are deleted** in commit `94ce9c9`, and `ee8490e` removed the stale build wiring they left behind. None exists in the tree today and none appears in a build input, so execs 249/250/251 and 271/272/273 have no shim implementation behind them. |
| Plan — **DONE** | The `edisp` LDD/PSL family was ported to the QEMU framebuffer as the kernel half, and the genuine `ewsrv.exe` now runs unmodified as the ring-3 window server. GUI applications reach it through `user/ws_e32_client.c` (migrated off the winsrv execs in `97f224b`), so no client E32 changed interface when the shim was removed. Verified in FINAL26: `genuine_ewsrv` PASS in 16.8s, `EWSRV.E32` startup markers green with 212 PE2 EUSER imports bound, `EWCLIENT.E32` `Create`/`Init`/`SetTitle`/`Write` all `r=0` (232 imports bound), screendump renders a window. |

### 2.3 EUSER — already genuine at runtime

| | |
|---|---|
| Genuine | `kernelhwsrv/kernel/eka/euser/` (`euser.mmp` + `euser.mmh`, ~50 sources in `cbase/`, `maths/`, `unicode/`, `epoc/`). The scratch x86 build produced the genuine `euser.dll` (412,104 B; UIDs `10000079 / 1000008d / 100039e5`; **2503 ordinals** from `kernel/eka/bx86gcc/euseru.def`). |
| Shim — **RETIRED** | `user/e32base.h` and `user/e32cmn.h` remain as *declaration-only* headers (CBase, descriptors, CServer2/CSession2, TRequestStatus). The implementation shim `user/euser_shim.cpp` is **deleted** (commit `0f9ec6a`) and is absent from the tree and from every build input. Every user E32 now carries real PE2 import descriptors bound against the tracked root `qemu-port/EUSER.DLL` (`tools/elf2e32.py`, `tools/mke32*.py`). |
| Plan — **DONE** | The DLL itself is **already genuine** — do not replace it, and it is not a shim: the tracked root `qemu-port/EUSER.DLL` is the runtime library. The *static shim* is retired: `elf2e32.py` now emits real PE2 import descriptors so every user E32 binds the genuine ordinals (e.g. `RDebug::Printf`@915, `UserSvr::FsRegisterThread`@1953, `CActiveScheduler::Start`@426) instead of linking `euser_shim.cpp`, and that file no longer exists. `e32base.h` stays a thin header exposing genuine layouts only. Note the honest caveat from `r1_e32_validate_filesvc_regression_v1.md`: one relocation in the genuine DLL exceeds a strict section bound, so the loader keeps a tolerance path — that is a loader fix, not a reason to fork EUSER. |

### 2.4 Sockets / networking — **ESOCK ABSENT**

| | |
|---|---|
| Genuine | **Not present in this `kernelhwsrv/` tree.** There is no `esock`, `esockets`, `commsdb` or Bluetooth source directory here. The only network-adjacent assets are HAL/HW drivers and `kerneltest/e32test/ethernet`. |
| Shim | `user/netstack.c` + `user/netstack_iat.S` + `tools/mke32_netstack.py`: a compact IPv4/IPv6/ICMP stack over the genuine `ipc.c` bridge, with a software loopback link layer because the port has no guest NIC. `user/dnsresolv.c`, `dnsnet.c`, `ping.c`, `httpget.c`, `telnet.c`, `wget.c`, `ntpc.c`, `dhcpc.c` are clients of this one service. |
| Plan | **Keep it, but label it honestly as authored port code.** This is the one subsystem where there is nothing genuine to compile, so the rule's preservation mechanism is ABI shape: keep the `RSocket`-style server name, request framing and error codes so a future ESOCK-bearing tree can replace the image without touching clients. Route the link layer through the genuine NIC drivers (`variant/qemu_nic.cpp`, `qemu_virtio_net.cpp`, `qemu_netbridge.cpp`) rather than loopback when a guest NIC is available. Do not claim this file is "Symbian source" in any compatibility statement. |

### 2.5 Startup, ESTART, domain manager, text shell

| | |
|---|---|
| Genuine | `fileserver/estart/` (`estart.mmp` → `e32strt.exe`, TCB-capable startup process), `fileserver/etshell/` (`eshell.mmp` → `eshell.exe`), `userlibandfileserver/domainmgr/` (`domainsrv.mmp` → `domainsrv.exe`, `domaincli.mmp` → `domaincli.dll`, `domainpolicy.mmp`; defs `bx86/domaincliu.def`, `domainpolicyu.def`). |
| Shim | `user/svcmgr.cpp` (service manager launched from disk) + `variant/e32load.cpp` `SvcMgrExecInstall()` (repurposes genuine test-only slow-exec slots 268/269/270 for launch/kill/reap) and `SvcMgrBootLaunch()` (reads `\SVCMGR.E32` via `FatReadPath`). |
| Plan | Compile `e32strt.exe`, `domainsrv.exe` and `eshell.exe` with the same route as §3 and launch them from the boot image instead of `SVCMGR.E32`; use the genuine domain-policy DLL for power/shutdown. `svcmgr.cpp` shrinks to a boot image manifest / loader, then disappears. Keep execs 268–270 only as long as any shim service still needs supervised launch. |

### 2.6 HAL

| | |
|---|---|
| Genuine | `kernelhwsrv/halservices/hal/` — `inc/hal.h`, `inc/hal_data.h`, `src/userhal.cpp`, `src/hal_main.cpp`, and the per-architecture export defs `bx86/halu.def`, `bmarm/halu.def`, `bwins/halu.def`, `eabi/halu.def`. The board-specific data comes from `bsptemplate/asspandvariant/template_variant/hal/{config.hcf,values.hda,hal.mmp}`. |
| Plan | This component is present and already reached by the port through the `HalFunction` executive (exec 79; EUSER ordinals 1213/2005). Adopt `halu.def` and the genuine HAL data layout directly for the QEMU variant, i.e. implement the HAL function table in `variant/qemu_variant*.cpp` rather than in a user shim. No separate user-side HAL shim exists to retire. |

---

## 3. Build strategy

The genuine components are built from the **same sources and headers** as the
other Symbian ports; only the host toolchain differs.

1. **Toolchain.** `qemu-port/toolchain.mk` already selects a 32-bit IA32 target:
   `-m32 -march=i486 -mtune=generic`, `-ffreestanding -nostdlib -fno-builtin`,
   `-fno-pic -fno-pie -fno-exceptions -fno-rtti -fno-threadsafe-statics`,
   `-mno-mmx -mno-sse`. This replaces `bldmake`/ActivePerl/MSVC+MASM with a
   Linux GCC/binutils route; the *format* produced is still PE2 E32, so the
   in-kernel loader (`variant/e32load.cpp`) and `tools/e32read.py` accept it.
2. **Headers.** Include paths point at `kernelhwsrv/kernel/eka/include`,
   `.../include/kernel/x86`, the component `inc/` directories (`fileserver/inc`,
   `ewsrv/../include`, `halservices/hal/inc`, `domainmgr/inc`) and `qport/inc`
   for the one generated header we must supply (`plat_priv.h`). We do **not**
   copy or edit genuine headers into the port tree.
3. **Exports.** The authoritative export ordinals come from the per-architecture
   `.def` files in `kernelhwsrv` (97 `bx86*.def` files are present — e.g.
   `kernel/eka/bx86gcc/euseru.def`, `halservices/hal/bx86/halu.def`,
   `domainmgr/bx86/domaincliu.def`, `fileserver/bx86/efsrv*.def`). Generate
   import descriptors/IATs from those defs with `tools/elf2e32.py` /
   `tools/mke32*.py`, exactly as the genuine SDK would, instead of hand-picking
   ordinals in C.
4. **Packing.** Emit PE2 E32 images (UID1/UID2/UID3 from the `.mmp`, e.g.
   `ewsrv` `0x1000008c 0x100000bf`, `efat32.fsy` `0 0x100000d6`, `e32strt.exe`
   `0 0x10272C04`) and validate with `tools/e32read.py` before putting them on
   the FAT32 disk image (`tools/mkfat.py`, `tools/mkdisk.py`, `tools/mkgrubiso.py`).
   The scratch-build recipe in `docs/r1_build_repro_v1.md` and
   `docs/r1_clean_build_v1.md` is the reference for the genuine build inputs.
5. **What stays port-layer.** Only the architecture-specific layer is authored
   here: `variant/qemu_*` (display, mouse, audio, NIC/netbridge, virtio, USB,
   display-channel backend), `variant/kernel_main.cpp`, the kernel exec-number
   tables/glue, `variant/e32load.cpp` boot launch, the boot image manifest, and
   the host-side tool scripts. These are the EKA2 equivalent of copying
   `bsptemplate/asspandvariant/template_variant` for a new board — exactly the
   layer the rule permits to change per port. The *user* servers must come from
   `kernelhwsrv/`.

A useful acceptance criterion for step 3–4: an E32 produced here should differ
from an SDK-produced E32 only in build paths/timestamps/relocation ripple, as
audited in `r1_artifact_audit_v0.md` — not in ABI shape.

---

## 4. Retirement plan (ordered) and gates

Ordering is by dependency: a server can only be retired after the library its
clients bind is genuine, and the window server depends on both the file server
(WAD/bitmap loading) and the HAL/display channel.

| Step | Change | Gate that must pass before proceeding |
|---|---|---|
| 0 — baseline | Freeze current behaviour; no source changes. | `python3 tools/r1_os_selfcheck.py` (**selfcheck**), `python3 tools/r1_doom_test.py` (**doom**, graphics + WAD from FAT), `python3 tools/r1_window_system_test.py`, `python3 tools/r1_winsrv_test.py`. Record hashes/markers. |
| 1 — EUSER closure — **DONE** | `elf2e32.py` emits real PE2 imports to the genuine `EUSER.DLL`; `e32base.h` carries declarations only; `euser_shim.cpp` deleted in `0f9ec6a`. | PASS: `tools/r1_cp4_ca_test.py` binds the `CActiveScheduler`/`CServer2` ordinals (430/428/426/425, `RDebug::Printf`@915) against the tracked `EUSER.DLL`; FINAL26 `selfcheck` PASS (233.9s). |
| 2 — HAL adoption | Implement `halservices` `halu.def` exports in the QEMU variant; remove ad-hoc HAL execs from user shims. | selfcheck; `e32test` HAL checks (`kerneltest/e32test/hcr`) if ported. |
| 3 — File server | Build `efsrv.dll`, `efile.exe`, `efat32.fsy`; bind client ordinals; run genuine `!FileServer`, then retire `portfilesrv.cpp` and `fs/r1_fs_*` from the production path. | **efsrv file test**: `tools/r1_fcopy_test.py`, `tools/r1_largefile_test.py`, `tests/r1_file_regression/`, plus genuine `kerneltest/f32test/server` cases; doom must still load its WAD from FAT32. Keep `R1Fs::Backend` only as unit-test seam. |
| 4 — Startup / domains / shell | Launch `e32strt.exe`, `domainsrv.exe`, `eshell.exe` from the boot image; reduce then remove `svcmgr.cpp` and execs 268–270. | selfcheck + efsrv file test + doom; shutdown/power path via `domainpolicy.dll`. |
| 5 — Window server | Build genuine `ewsrv.exe` + ported `edisp` driver; route the framebuffer through the display channel; delete `winsrv.c`, `guishost.h` and the ad-hoc GUI execs. | **ewsrv window test**: `kerneltest/e32test/window` (`t_wsimp`, `t_wwins`, `t_wjpwin`) plus `tools/r1_window_system_test.py`, `tools/r1_winsrv_test.py`, `tools/r1_desktop_test.py`; doom graphical frame must be unchanged. |
| 6 — Networking (authored) | Keep `netstack.c`; move its link layer onto the genuine NIC drivers; freeze its IPC contract so a future ESOCK tree can drop in. | `tools/r1_ping_test.py`, `r1_httpget_test.py`, `r1_telnet_test.py`, `r1_dnsres_test.py`. This gate never becomes a "genuine" gate — it is an ABI-stability gate. |

After step 5 the only authored user image is the network stack (step 6). At
that point the invariant in §1 holds for this tree: the kernel/variant is the
only per-port code, and everything else is compiled from `kernelhwsrv/`.

---

## 5. Honest gaps — components absent from this source tree

The rule can only be honoured for code that exists. The following are **not**
in this `kernelhwsrv/` snapshot and are therefore either shimmed or deferred:

- **ESOCK / RSocket / comms database (`CCommsDatabase`) / Bluetooth** — absent.
  `netstack.c` is authored port code (§2.4). Bluetooth and the comms DB have no
  replacement at all; features that need them are out of scope, not "compatible".
- **UI framework (UIKON, EIKON, BAFL, app framework)** — absent. GUI programs
  here use the hand-written `guilib.c` / `desktop.c`, which no longer sit on a
  shim window server: both render through `user/ws_e32_client.c` onto the
  **genuine EWSRV** session (FINAL26 `genuine_ewsrv` PASS). The gap is the
  toolkit, not the server; this is the largest compatibility gap after ESOCK.
- **GDI / graphics resource server (`CFbsScreenDevice`, font/server bitmap
  server)** — absent beyond the allocator (`CBitMapAllocator`) exported by
  EUSER. Only the EUSER graphics-function ordinals (1211, 1213, 1216, 1225…)
  are available; there is no `CWsScreenDevice` to compile.
- **Multimedia / camera / telephony servers** — absent. Audio is a direct
  slow-exec bridge (`variant/qemu_audio*.cpp`, exec 240 family), not an EKA2
  multimedia server.
- **`edisp` LDD source** is referenced by `ewsrv/bld.inf` but the QEMU-specific
  implementation must be written in the port (step 5); the vt100/x86 variants
  in `kernelhwsrv` are the model.
- **Build-time generated headers** (`plat_priv.h`, variant config) are normally
  produced by `bldmake` export; `qemu-port/inc/` supplies minimal shims. These
  are *header shims only* — they expose genuine layouts, they are not a
  userspace fork.

Handling policy: authored replacements must (a) live behind a stable,
Symbian-shaped IPC/exec ABI, (b) be documented as authored in this file and in
the relevant subsystem doc, and (c) be removable without changing any client
E32 when genuine source becomes available. Documentation must never describe an
authored shim as "genuine Symbian code"; EUSER.DLL, by contrast, *is* genuine
and is already the runtime library.

---

## 6. Verification harness

`python3 tools/r1_genuine_retire_test.py` is the gate that tracks each
retirement step above. It reports **BUILT / RUNNING / PASS / FAIL / SKIP
independently per subsystem**:

- **EFSRV/F32 — PASS (FINAL26, 14.7s).** `genuine/build/` holds the genuine
  `EFSRV.DLL` client, `EFILE.E32` server and `EFAT32.FSY` driver, so the row is
  no longer SKIP. It boots QEMU, `GenuineBootLaunch()` brings up the genuine
  `!FileServer` (markers `GenuineBoot: file server up, svcmgr fallback skipped`,
  `EFILE: genuine file server starting`) and drives a user client over the real
  RFileServer session protocol (`F32CLIENT: IPC round trip OK`), asserting
  `EFILE: port-shim count == 0`. A static check always runs: the wire
  opcodes in `fs/r1_fs_core.h` must still equal the genuine `TFsMessage`
  ordinals in `kernelhwsrv/.../F32plugin.h` (13/13), and `user/ipc.c` must still
  be able to connect to `!FileServer` by name — that is what makes our IPC bridge
  able to talk to the genuine server unchanged.
- **EWSRV — PASS (FINAL26, 16.8s).** `genuine/build/EWSRV.E32` exists; the row
  boots it, checks the genuine startup markers, confirms 212 PE2 EUSER imports
  bound, and takes a QMP screendump proving a window renders.
- **ESTART/ESHELL — PASS (FINAL26, 19.9s).** `genuine/build/estart_.E32` and
  `etshell_.E32` exist; the row boots the genuine shell and checks its boot
  markers (`ESTART`, `ESHELL`).
- **EUSER** — the prebuilt genuine `euser.dll` (UIDs `10000079/1000008d/
  100039e5`, 2503 exports) must be present and load resident in every QEMU
  boot (residency is also covered by `tools/r1_qemu_smoke.py`).

Policy encoded in the harness: a not-yet-built component is **SKIP** and never
fails the run; only a component that is actually run hard-fails, on guest
`Exc`/`PanicKernExec`/`FAULT` or missing markers. `--dry-run` does the BUILT
and static checks only (no QEMU). Exit code is 0 unless a run component
failed, so the test can gate CI incrementally as each genuine server lands.

**Boot-time auto-launch — RESOLVED (2026-11-24).** The G-01 change to
`variant/e32load.cpp` / `qemu_variant_asic.cpp` was reworked: `Asic::Init3()`
now spawns a **supervisor DThread** (`GenuineBootSpawn`, priority 12) which
calls `GenuineBootLaunch()`; calling `E32Run()` directly from the Init3
nanokernel start thread had faulted `EMutexWaitNotDThread=98` (`KERN 0x62`).
`E32Run()` now grants `ECapabilityProtServ` on every loaded E32 so `!`-prefixed
server names can publish, and raises PE2 import slots to 512. The genuine chain
is probed in order — EUSER → EFILE/EFSRV → ESTART → EWSRV → ESHELL — with
`svcmgr`/`portfilesrv` as fallback when no genuine file server is found. Full
markers now verify with **0 faults**; `r1_qemu_smoke.py` PASS and the
retire gate reports efsrv/ewsrv/estart/euser all PASS. B7 (the `KERN 0x62`
boot fault) and B4 are **FIXED**; see `GENUINE_PROGRESS.md` §2–§3 and
`BOOT_CHAIN_VERIFICATION.md`. Because the harness stages the genuine names,
components are "run" at boot, so a guest fault still correctly fails a gate.
The former EFILE blocker is **CLOSED** (FINAL26 `genuine_efsrv` PASS, 14.7s):
`CServer2::Start("!FileServer")` returned **KErrBadName (-28)** from
`User::ValidateName` in EUSER's `RServer2::CreateGlobal` because the genuine
build encoded `_LIT` literals as UTF-32 (Linux gcc 32-bit `wchar_t`) while
prebuilt EUSER reads 16-bit chars. **Fix landed:** `-fshort-wchar` alongside
`-freg-struct-return` in `tools/genuine_build.py`, `genuine/f32_efsrv_build.py`,
`genuine/build_efile.py` and `genuine/efat32/build_fsy_dll.py`; EFILE now
publishes `!FileServer`, registers its server thread and reports port-shim 0.
The earlier `KERN 0x4 EBadHandle` sret mismatch stays superseded:
`variant/e32load.cpp` wraps exec 81 to clear critical/permanent flags (0 FAULT);
`docs/EFILE_START_ANALYSIS.md` ranked cause #1 (EUSER translation of a successful
create) stays superseded too — exec 126/127 were never reached.

**On-disk FAT I/O VERIFIED (2026-11-24, B5 milestone).** The minimal server
`genuine/efsrv_min/efsrv_min.cpp` now uses `DiskFatBackend`; the port bridge
`R1Fs::Backend` → `file_*()` → `PortFile`/`PortBigFile` reaches real FAT32
cluster-chain I/O (`AtaRead`/`AtaWrite`). Run `python3 tools/r1_genuine_fat_io_test.py`
(`FATIOCLIENT.E32` from `genuine/efsrv_min/fatio_client.cpp`): disk-only
`R1TEST\SEED.TXT` opens, `FATIO.TXT` is created/written/closed, reopened and
`FATIO-MARKER-42A7` read byte-for-byte; all r0, no `Exc`/`PanicKernExec`/`FAULT`.
Genuine `TFsMessage` opcodes/descriptors/Core and `r1_fs_server.cpp` are
untouched; cluster I/O uses the port `R1Fat` adapter, not full `sfat32` (PE2
slot limit), and F32 BPB parsing remains in `F32PROBE`. See
`docs/GENUINE_PROGRESS.md` §2/§3 and `genuine/F32_EFSRV_REPORT.md`. The EFSRV
retire gate is **PASS**, not SKIP (FINAL26, 14.7s). The earlier "0 shims
retired" note is obsolete: `user/portfilesrv.cpp`, `user/svcmgr.cpp` and
`variant/r1_fs_channel.cpp` are deleted from the tree, and the surviving
`fs/r1_fs_*` sources plus `variant/fat32.cpp` carry no production wiring — they
are reached only by `tests/r1_fs_core/` and `genuine/efsrv_min/`, i.e. the
`R1Fs::Backend` test seam.

---

## 7. References

- `docs/ARCHITECTURE.md` §4 — EUSER.DLL and PE2 import binding.
- `docs/BUILD_GUIDE.md`, `docs/r1_build_repro_v1.md`, `docs/r1_clean_build_v1.md`,
  `docs/r1_artifact_audit_v0.md` — genuine x86 build route and audit.
- `docs/r1_service_boundary_v1.md`, `docs/r1_startup_research_v0.md` —
  `!FileServer` bootstrap phases and service-host boundary.
- `docs/WINDOW_SERVER.md`, `docs/NETWORKING_STACK.md`, `docs/TEXT_MODE.md` —
  current shim subsystem descriptions.
- `docs/r1_boot_and_selftest.md`, `docs/TESTING_QA.md` — how to run the gates.
- `kernelhwsrv/build.config.xml`, `kernelhwsrv/package_definition.xml`,
  `kernelhwsrv/bsptemplate/asspandvariant/` — the genuine per-architecture port
  boundary that motivates the rule.

## 8. Supersession — 2026-11-26 (FINAL27, `FINAL27_RC=0`, 15/15)

Line 263 ("**Fix landed:** `-fshort-wchar` … `build_fsy_dll.py`") is confirmed,
not pending — the FSY side is no longer in flight. `genuine/build/EFAT32.FSY`
(264,720 B), `genuine/build/EFSRV.DLL` (263,580 B) and
`genuine/build/EFILE.E32` (273,044 B) are staged; `-fshort-wchar` is present at
`genuine/efat32/build_fsy_dll.py:55`. FINAL27 shows `genuine_efsrv` **PASS
15.1s**, `genuine_fat_io` **PASS 14.7s** (`FATIO-MARKER-42A7` round trip) and
`genuine_retire` **PASS 44.4s** (efsrv row PASS, not SKIP). Gate of record:
`OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`, log
`/tmp/opencode/final27_gate.log`, HEAD `ee8490e`, no commits. Historical text
above is left unedited.
