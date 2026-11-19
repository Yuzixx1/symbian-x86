# Project roadmap — decisions and milestone gates

Planning decisions recorded October 2026. This is a **plan**, not a claim that
the features below work. For current implementation status use
`full_os_checklist.md`; for commands to test today's build use
`manual_test_order.md`. Parallel AI work cards and handoff rules live in
`parallel_work.md`; its coordinator is the owner's interface for collecting
other AI handoffs, resolving technical blockers, assigning waves and asking
the owner only project-policy decisions. Do not edit the read-only
`kernelhwsrv/` checkout.

## Destination and scope

- Keep the genuine Symbian EKA2 nanokernel and Kern as the native **x86** OS.
  Build a usable Symbian-style system around them. Preserve a path toward
  historical software compatibility, rather than claiming it today.
- First native compatibility proof: upstream-source **unchanged x86 E32**
  program(s), built with a matching upstream toolchain, run as ordinary
  unprivileged processes using genuine EUSER/CBase. The proof covers both
  active scheduling and RFs/RFile read **and write**. Prefer one program;
  **two independent unchanged upstream apps** (one for each behavior) are
  acceptable if no suitable single program exists. This is the **R1-B
  unchanged-app compatibility gate**, not a prerequisite for every functional
  checkpoint. **R1-A** may use a clearly labelled port-authored file fixture
  built with the matching upstream x86 toolchain and genuine EUSER/EFSRV;
  the port's freestanding SDK alone does not meet that checkpoint. R1-A is
  not evidence of unchanged historical file-app compatibility.
- R1 console scope now follows the owner's **upstream econs** choice: audit
  and provide its loader and E32WindowServer dependencies rather than silently
  replacing it with a private console DLL. This is not blanket GUI scope.
  The named port variant enables `SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API`;
  exported panic stubs in the old scratch EFSRV are not acceptance support.
  Size limits remain separately tested claims, not a consequence of this macro.
- Current local research configuration is `port-x86-rfile64-v1`: the file API
  macro is enabled; unrelated optional locale/notification/header features
  remain off. H0 (genuine heap's existing DL fallback, no heap patch) is the
  provisional bring-up contract, not a runtime result. See the latest
  `r1_research_reconciliation_v0.md` section for build permissions and tests.
- Named-config EUSER/EFSRV/t_cactw/fhserver now have a completed clean target
  build with disclosed EUSER harness recovery; coordinator verified four image
  hashes. No E32 runtime or two-build determinism proof yet. Source-reviewed
  file-contract corrections are adopted for the next candidate, including
  source-following SetSize position behavior and unspecified growth contents.
  An explicitly labelled custom serial-diagnostic R1-A fixture may precede
  genuine console bring-up; unchanged t_cactw and the selected upstream-console
  acceptance still require genuine econs/ewsrv. This does not satisfy R1-B.
- First historical platform profile: **S60 5th**; Symbian^3 is a later,
  separate profile. Most historical phone apps are ARM binaries: an x86
  kernel and an E32 loader do not by themselves execute their instructions.
  The ARM execution track below is additional to x86 system services.
- Pre-GUI usability priorities are console quality, developer tools and
  multitasking. GUI starts only after core memory, file, console and startup
  services are usable. Networking follows file/console services; it is not
  implied by same-machine IPC. No disk-management utility is planned.
- Do not publish releases yet. Historic binaries/firmware and the port/SDK
  license require a provenance and licensing review before distribution;
  do not bundle unverified third-party artifacts. `kernelhwsrv/` keeps its
  upstream terms; this review is not a legal conclusion.

## Verification vocabulary

- **Planned:** design or investigation only.
- **QEMU-ready:** a new regression in `tools/test_*.py` is wired into
  `tools/test_stable.py`, the whole suite passes, and a step is added to
  `manual_test_order.md`. Default `make` still rebuilds all ISOs.
- **Hardware-verified / milestone complete:** QEMU-ready **and** the project
  owner reports the milestone's smoke test on the reference physical PC.
  If PC access is delayed, call it QEMU-ready, *not* complete. Archive the
  boot mode, media, controller, visible error and last successful action.
- VirtualBox remains a useful independent check, not a substitute for the
  agreed physical-PC gate. No new untested hardware behavior is assumed.

The first hardware reference is a Pentium 4–era Intel-board PC with legacy
BIOS and PS/2 input. IDE/PATA is **suspected**, not confirmed; SATA, CPU
features, RAM, USB boot support and a serial port are unknown. Start with a
conservative single-core, non-PAE 32-bit x86 baseline (no assumed NX or APIC).
The owner can try USB boot or a **dedicated** test disk; confirm boot support,
controller and which disk is disposable before writing any media. No serial
capture is available now, so the smoke procedure must be observable on VGA,
including actionable fault diagnostics. Never overwrite the system disk as
part of a test recipe. `hw_tests.md` records the hands-on results when known.

## Milestone sequence

Each numbered gate includes its own negative/fault tests, clean reruns and
manual step; names below are planning IDs, not implemented features.

### R0 — Establish the baseline and hardware route

Keep the existing QEMU stable suite green; inventory Pentium 4 CPU/board,
RAM, boot options, disk controller and a safe test disk when convenient.
Prepare a VGA-only boot/diagnostic checklist. Reconcile stale counts/statuses
in `full_os_checklist.md` as features are actually verified. Do not call the
hardware route proven before a real boot and input/storage test.

### R1 — Native EUSER/CBase, full file service and scheduling (next public proof)

Build genuine upstream EUSER/CBase components in the **port** with an x86
import/ordinal ABI, process startup, TLS/error conventions as needed, and a
working active scheduler. **Before the proof app**, build a pragmatic, general
RFs/RFile-compatible file server over FAT: sessions, paths and directories,
open/create/read/write/seek/close, errors, multiple clients, sharing rules,
durable close/flush, and crash/restart behavior. Test the complete ordinary
file workflow, not a one-file, read-only adapter. Use upstream behavior where
it matters at the app boundary; targeted port shims are acceptable and must
be documented. This service is not claimed to be the genuine upstream F32
implementation. Pilot EUSER.DLL and the freestanding C IPC bridge remain
scaffolding, not proof of compatibility. Harden partial loader-failure
cleanup, DLL import validation, timer/request accounting and server-death
paths before trusting the demo. The file server may be started explicitly
for R1 tests; service supervision belongs to R3.

R1 has **explicit internal checkpoints**, each with its own QEMU regression
and manual step, but none is the complete R1 gate:

1. Inventory the upstream x86 toolchain, EUSER dependencies, RFs protocol,
   error codes, sharing semantics and file API. When app-visible semantics
   conflict with today's shell/FAT behavior, follow the upstream RFs contract
   at the application boundary and document the port's transition. The
   research candidate is **bx86gcc first**, *not* a frozen toolchain: its
   EUSER ordinals differ from bx86, and a matching EFSRV import set is not
   now built experimentally but **not runtime-verified**.
   `r1_build_repro_v0.md` records EUSER/EFSRV and two apps, with unapproved
   scratch-tool patches, a fabricated variant and other deviations still
   requiring audit. `r1_sdk_route_research_v0.md` identifies Raptor's
   documented bx86gcc-first, transformed-eabi-fallback route for EFSRV;
   reproducing it cleanly and validating its artifacts remain required.
   See `r1_abi_contract_v0.md` and
   `r1_research_reconciliation_v0.md`; resolve that mismatch before
   implementation, never splice ordinal tables from both targets. The latter
   corrects v0's file-close opcode (28, not directory-close 27) and records
   other wire/startup/proof limits; neither report freezes an ABI.
2. Bring up the general RFs/RFile service and test multi-client read/write,
   reopening, seeks, directory paths and persistence on disposable media.
   Remove the **64 KiB** transport ceiling. Aim toward FAT32's practical
   per-file ceiling (just under 4 GiB); use fast boundary/offset tests in the
   stable suite and **optional manual large-image tests** for size claims.
   A small-file test alone cannot certify the maximum. Scope seek/size types
   against the actual upstream API rather than assuming 64-bit offsets.
3. Bring up genuine EUSER/CBase and an active-scheduler app; test imports,
   cancel/completion order, cleanup and process isolation expectations.
4. **R1-A functional checkpoint:** build a new, explicitly port-authored
   RFs/RFile fixture with the matching upstream x86 toolchain, genuine
   EUSER/EFSRV and zero capabilities. Demonstrate the full file assertions
   below, plus genuine active scheduling (prefer unchanged `t_cactw` if
   feasible; it remains an unbuilt candidate). Record any custom scheduler
   fixture separately, not as unchanged-app evidence.
5. **R1-B unchanged-app compatibility gate:** build selected proof app(s) from
   **upstream source and a matching
   toolchain**, recording revisions and build inputs. Run unchanged x86 E32
   file read/write and active-object proofs; if they are separate binaries,
   both must run unprivileged on the same integrated service baseline.

Crash-during-write integrity at R1 is **best effort**: partial file contents,
loss of a write, or even FAT metadata damage are possible. Test failure
injection on disposable images, document exactly what survives, and provide
an offline backup/repair/reformat recovery procedure before recommending
real data. Never run destructive stress/power-loss tests on the reference
PC's system disk. `flush` means the requested I/O was issued; it is not an
atomic-write or power-loss guarantee.

**Shared R1-A functional acceptance:** the custom upstream-toolchain file
fixture runs unprivileged with genuine EUSER/CBase and EFSRV. It creates,
writes, flushes/closes, reopens, verifies bytes and EOF, and reports a
missing-file error through RFs/RFile. A fresh boot verifies persisted bytes.
Genuine active scheduling, multi-client errors, import failures,
cancel/completion order and server failure/restart also need regression
coverage and manual steps. The owner checks boot, launch, visible output,
persistence and shell recovery on the reference PC for hardware verification.
An automated pass is only **R1-A QEMU-ready**, not milestone complete.

**R1-B compatibility gate:** unchanged upstream-built x86 proof app(s) run with real EUSER/CBase
without special kernel privilege. The file app creates/writes, closes,
reopens, verifies bytes and EOF, and reports a missing-file error through
RFs/RFile; the same app **or** a second unchanged upstream app demonstrates
asynchronous callback scheduling. A fresh boot verifies persisted bytes.
QEMU tests also cover multi-client errors, import errors, cancel/completion
order and server failure/restart; the owner checks boot, launch, visible
output, persistence and shell recovery on the reference PC. A privileged
demonstration or an RFs read-only stub is bring-up only.
Large-file limits must be reported separately as *tested to size X*, not
declared near-4-GiB-capable merely from the design target or a small smoke
test. The R1 milestone does not imply power-loss-safe storage. R1-A does not
close R1-B or the overall R1 compatibility milestone. Do not silently remove
the unchanged-app requirement or assume R1-A authorizes R2 implementation;
revisit sequencing explicitly if R1-B remains blocked.

### R2 — Full memory redesign, preserving documented source APIs

After R1, design explicit kernel/user virtual ranges, per-process page tables,
code/data/stack mappings, shared/private chunks, user-pointer validation and
fault containment. Support non-PAE x86 first and retain the documented C SDK
source APIs; apps may need recompiling and any binary ABI break must be
versioned/documented. Replace the fixed 16 KiB user heap with tested
chunk-backed growth, add guard pages, and address DMA mappings deliberately.
Bring up paging incrementally behind QEMU fault/mapping tests rather than
shipping a single untestable flip of all memory paths.

**Gate:** shell plus R1 program, current samples, file/IPC/chunk workflows and
concurrency tests pass with paging on; bad user pointers and stack overflow
terminate the owning process without killing the shell. Repeat core userland
and crash checks on the reference PC. Paging is not claimed safe merely
because it boots.

### R3 — Supervised services, console and multitasking

Introduce an ordered startup/supervisor with bounded restart policy and
visible failure reporting (storage → R1 file server → console → other
services). Migrate remaining PortFile clients to the R1 service gradually;
expand it with notifications and higher limits as separate tested work. Do
not label it upstream F32. Replace the console debug transport and exclusive
lease with DOS-like text-console sessions, focused input, binary-safe output,
editing and scrollback. Make shell launches **foreground by default**, with
explicit background jobs, exit/wait status and a `shutdown` that stops
services, flushes FAT, displays safe-to-power-off and halts. Add
developer-facing build/install/debug workflows. Avoid promising a full
POSIX shell.

**Gate:** concurrent apps can use files and the console while another app
exits or a service crashes; bounded restart or explicit failure works without
silently losing data. Exercise jobs, editing, shutdown and a fresh SDK-built
app in QEMU and by VGA/PS2 on the reference PC. Larger files and long-name
creation are future filesystem gates unless covered by the R1 server tests.

### R4 — Loader/service breadth and network foundations

Expand DLL dependency resolution, imports, unloading, header/security and
process-environment support against actual upstream contracts. Grow storage
reliability, large-file support and long-name creation with corruption and
power-loss tests before calling the file service mature. Only after stable
file/console services, bring up a NIC, RX/TX, DHCP/DNS and user sockets with
QEMU network tests. Do not imply TCP/IP or third-party binary compatibility
from IPC or a successful pilot DLL.

### R5 — ARM binary translation (independent research track)

Research E32 machine types, ARM process ABI, call boundary, DLL dependencies
and S60 5th service expectations alongside the native milestones. The chosen
initial implementation is a **built-in experimental kernel component** with
dynamic translation and a correctness-first code cache. First cover ARMv6
ARM-state **and Thumb/interworking** user instructions and relevant VFP;
NEON belongs to a **separate ARMv7 extension**, not baseline ARMv6. Use
instruction/ABI differential tests and a small controlled headless ARM app
before claiming broad compatibility. No whole-phone emulation is assumed.

Kernel panics are tolerated only for trusted bring-up fixtures. Before any
third-party binary is run, R2's isolation and translation fault containment
must ensure malformed ARM code cannot take down the OS; keep untrusted
translation off by default until this gate. A working native EUSER app does
not demonstrate ARM execution; a translated toy program does not demonstrate
S60 5th compatibility. Later gates cover S60 5th services and a legally
obtained, headless historical app, then Symbian^3 as another profile.

### R6 — GUI only after core-service gates

After native memory, startup, RFs/console and a supported app/runtime path
have usable gates, design window/event/input focus, graphics and the S60 UI
compatibility surface. Scope and hardware graphics requirements remain open;
do not count a framebuffer demo as a completed window server.

## Standing risks and open decisions

1. Genuine EUSER/CBase build dependencies and the upstream x86 ABI/toolchain
   may change R1 effort. `r1_abi_contract_v0.md` is **not frozen**: bx86gcc
   EUSER and bx86 EFSRV import ordinals cannot be mixed. Raptor's matching
   candidate route produced experimental genuine-source E32 DLLs with
   build deviations; clean reproduction and runtime verification remain due.
   Unchanged file proof sources remain unselected for R1-B; owner-authorized
   R1-A uses a custom authentic-toolchain fixture instead. Freeze R1-A's
   build/startup/wire contract before service implementation; do not mistake
   that checkpoint for R1-B compatibility.
2. R1 intentionally tolerates FAT corruption on interrupted writes; its
   recovery procedure is a prerequisite to real-media testing, not a promise
   of power-loss durability. Do not claim a file-size limit not exercised by
   the separately recorded large-image tests.
3. A built-in translator has a larger kernel attack/crash surface than a user
   process. Isolation must be a gate before arbitrary historical software;
   revisit placement if it cannot be contained.
4. An upstream EUSER user app can validate x86 ABI without validating ARM,
   S60 5th UI or Symbian^3. Maintain separate evidence for each claim.
5. The Pentium 4 boot path, IDE/PATA guess and lack of serial output may force
   VGA diagnostics or a different safe test medium. Do not equate VBox with
   physical-PC results.
6. Review the port/SDK license, historical binaries and firmware/ROM
   provenance before any public artifact distribution; decide upstream
   submodule representation separately, without editing its sources.

When a milestone completes, update this roadmap, the implementation
checklist, QEMU regression list and manual/hardware results together.
