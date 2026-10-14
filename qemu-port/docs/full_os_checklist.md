# Full-OS Checklist — Symbian-on-x86 (SymbianSource basis)

Basis: https://github.com/SymbianSource, read-only checkout `oss.FCL.sf.os.kernelhwsrv`.
Port lives in `qemu-port/`. This file is the single source of truth for
"what is done vs half-built vs missing" and is meant to be re-read by future
sessions before writing code. Status words:

- DONE: implemented + QEMU-regression covered.
- PARTIAL: works within documented bounds, real gaps remain.
- TODO: not started or fixture-only.

Ground rules (do not regress):
- G0. `kernelhwsrv/` stays read-only. All port work in `qemu-port/`.
- G1. Default `make` rebuilds all ISOs so test media never goes stale.
- G2. FAT32, whole-disk `format`, multi-drive; no disk-management utility.
- G3. GUI is last. Hardware drivers beyond the port wait for user HW tests.
- G4. Every feature lands with a QEMU regression in `tools/test_*.py` wired
  into `tools/test_stable.py`, plus a manual step in `docs/manual_test_order.md`.

Primary boot image: `build/symbian-grub.iso`. Test runner: `python3 tools/test_stable.py`.

---

## 1. Boot & ROM (PARTIAL)

- B-01 Multiboot/BIOS+UEFI via GRUB ISO — DONE. `build/symbian-grub.iso`,
  `tools/mkgrubiso.py`. Verify: boot ISO in QEMU/OVMF, shell appears.
- B-02 ROM synthesis, SuperPage, variant entry — DONE. `variant/qemu_variant_rom.cpp`.
- B-03 KernelMain + scheduler start, serial/VGA early console — DONE.
- B-04 VirtualBox EFI/GRUB + physical HW boot matrix — TODO. See `docs/hw_tests.md`.
  Done = boot + shell + `mount` + `dir` on each target, logs archived.
- B-05 Boot robustness (bad multiboot magic, low RAM, missing drives) — TODO.
  Done = clean fault messages, no hangs; negative QEMU cases.

## 2. CPU / interrupts / timers (PARTIAL)

- C-01 GDT/IDT, ring-3 exec gates 0x20/0x21, TSS — DONE.
- C-02 PIT tick, IRQ dispatcher, PS/2 + polled USB keyboards — DONE
  (`test_xhci_keyboard.py` covers one direct-root-port xHCI keyboard).
- C-03 Full IRQ sharing/edge cases, APIC/SMP — TODO. Single-core QEMU only.
- C-04 RTC read — DONE (`QemuRtcSeconds`); `DATE [YYYY-MM-DD HH:MM:SS]` sets
  the CMOS clock through the genuine Asic setter with full calendar validation
  and sticky readback (`test_cli.py`). Timezone support still TODO.
- C-05 Panic/fault reporting — PARTIAL. Kernel-heap `FAULT:` path DONE;
  genuine `Exc 06` trap info prints on a user UD2 and the reaper exit line now
  names the program (`prog=CRASH.E32`, including the already-destroyed orphan
  path); `CRASH.E32` proves shell survival (`tools/test_crash.py`). Full
  per-thread attribution (exception descriptor, panic propagation to `ps`) TODO.

## 3. Memory management (biggest gap) (PARTIAL)

- M-01 Direct model, identity-mapped RAM, paging OFF — DONE (deliberate).
- M-02 Kernel heap + heap-routed global new/delete — DONE (`kernfull_glue.cpp`).
- M-03 Per-process address spaces / paging / U/S isolation — TODO. Today ring-3
  runs with no protected AS; any user pointer reaches all RAM. Done = MMU on,
  page tables per process, user/kernel split enforced, existing E32/TYPE/CP/IPC
  tests still pass, bad user pointer faults the process not the kernel.
- M-04 Chunks (private/shared, `ChunkCreate`) — PARTIAL (used by loader for code/data;
  no general user `RChunk` API/aliasing/pinning tests).
- M-05 User heap + `malloc/free` — PARTIAL. Freestanding first-fit allocator
  over a 16 KiB static pool (`user/malloc.c`: alignment, reuse, OOM-NULL,
  coalescing, `MALLOC.E32` fixture in `test_userland.py`). Single-threaded,
  fixed size, no chunk-backed growth — that plus MMU isolation still TODO.
- M-06 Stack guard / stack overflow detection — TODO.
- M-07 DMA-coherent allocators for drivers — TODO (drivers use ad-hoc buffers).

## 4. Scheduler / threads / processes (PARTIAL)

- P-01 Genuine EKA2 NKern preemptive scheduler — DONE (kernel + shell + user threads).
- P-02 `exec` returns immediately; multiple user processes alive — DONE.
- P-03 Priority mapping + starvation note — PARTIAL. Unknown header priority
  promotes to foreground; spinning high-priority thread starves lower ones
  (documented genuine behavior). Done = priority policy documented + `ps`
  shows priorities + starvation regression (two long-runners + responsive shell).
- P-04 Exit path via ThreadKill; `kill <id>` refuses own process — DONE.
- P-05 Reaper (`E32Reap`): drops creator seg ref, clears ProcExt+sExecs — PARTIAL.
  Gaps: only tracks processes remembered at resume; partial-Create/Loaded-failure
  cleanup leaks; shared-seg double close possible. Fixed this round: exit lines
  name the program (both paths), both tables gated before creation, `kill`
  refuses own process by pointer, `pzomb` locked. Done = failure-injection tests
  (bad image mid-load leaves no seg/proc residue in `segs`/`ps`).
- P-06 Reliable wait/exit-status API — PARTIAL. 16-entry exit ring survives
  reaping (name/id/type/reason, orphan path marked); shell `EXITS` verb with
  `test_crash.py`/`test_conc.py` coverage. Blocking `wait <id>` still TODO.
- P-07 TLS, thread exceptions, `RThread::Exit` veneer, priorities API — TODO.
- P-08 Concurrent-workload regression — DONE (`tools/test_conc.py`): IPC server
  blocked in receive while shell + clients run, diagnostics responsive under
  load, `kill` of a session-owning server recovers via restart, exit ring records it.

## 5. Loader / E32 / DLLs (PARTIAL)

- L-01 `mke32`/`e32read` host twins + in-kernel `E32Validate` (CRC/UID) — DONE.
- L-02 Code/data copy, BSS zero, TEXT/DATA/INFERRED relocs — DONE
  (`test_e32_relocs.py`, `test_data_reloc.py`).
- L-03 ELF→E32 (`elf2e32.py`: R_386_32/same-region PC32, rejects rest) — DONE.
- L-04 DLL-as-EXE rejection, EUSER-from-EXE-drive, strict resident checks — DONE.
- L-05 Pilot `EUSER.DLL`: ordinals 1–3 (Exit/PutC/CmdLen) — PARTIAL. Generated
  pilot, NOT genuine EUSER. Missing-exports fail fast; missing-DLL+2-imports
  falls back to trampolines. Done = genuine EUSER/CBase built from source and
  loader binds its real exports (see S-04).
- L-06 General DLL loading (deps, relocs in DLLs, unloading, versioning) — TODO.
- L-07 Compression, capabilities enforcement, UID capability checks — TODO.
- L-08 Heap/stack sizes from header enforced + OOM behavior — TODO.
- L-09 `ESTART` / process environment block (only command lines today) — TODO.

## 6. Kernel IPC (PARTIAL — transport real, framework thin)

- I-01 Server/session/message executives via C bridge — DONE (`user/ipc.c`).
- I-02 Connect incl. version + `EConnect`, integer request/reply — DONE.
- I-03 8-bit descriptors (binary, chunked, offsets, overflow/type errors) — DONE.
- I-04 16-bit descriptors (129-char, embedded zero, offset, 8-on-16 reject) — DONE.
- I-05 Pending-receive cancel + reuse; server-death wakes client with
  `KErrServerTerminated` — DONE. (`tools/test_ipc.py`.)
- I-06 One-outstanding-request-per-thread only — PARTIAL. Surplus signals are
  preserved as spurious wakeups, not counted like `User::WaitForRequest`.
  Done = concurrent async test (2+ outstanding requests per thread, completions
  in either order, no lost wakeup over 1000 iterations).
- I-07 Active-object scheduler in userland — TODO. Needed before any real
  service loop.
- I-08 Upstream `CServer2`/`CSession2` + genuine EUSER/CBase — TODO (see S-04).
  The C bridge is transport, not SDK compatibility.
- I-09 Security policies on session create, session sharing/transfer — TODO.
- I-10 Chunk/large-data transfer, message cancellation protocol (beyond receive
  cancel), client-death cleanup races — TODO.

## 7. File service (PARTIAL — PortFile transitional)

- F-01 `PortFile` logical device over genuine channels — DONE (open snapshot,
  256B chunked read, open-write/append/commit, 64 KiB cap).
- F-02 Thread+process ID ownership, no pointer UAF; overlong paths rejected — DONE.
- F-03 Per-process CWD: inherited + mutable validated chdir/getcwd — DONE
  (`CWD.E32`, shell isolation, bad-dir rejection in `test_userland.py`).
- F-04 Streaming `CP` to full 64 KiB; oversize preserves destination — DONE.
- F-05 `TYPE`/`FILECHK` (independent handles, auto-close, census zero) — DONE.
- F-06 Upstream F32 + `RFs`/`RFile`/`RDir` session protocol — TODO. PortFile must
  be replaced by a real file server with a port backend. Done = RFs connect,
  open/read/write/rename/delete/mkdir/dir through server sessions, async ops,
  file notifications, sharing modes.
- F-07 Long filenames on create, timestamps, attributes, read-only flags — TODO
  (LFN read works; create is 8.3-only).
- F-08 Cross-drive relative CWD (today: other-drive root), per-drive CWDs — TODO.
- F-09 fsck/recovery, power-loss atomicity (FAT is non-journaled, shared-buffer) — TODO.
- F-10 `TYPE` binary safety (today DebugPrint text transport) — TODO.

## 8. Console & shell (PARTIAL)

- H-01 DOS-like shell, `HELP`/`HELP <cmd>`, aliases, nested paths, wildcards,
  `COPY /Y`, same-dir `REN`, disk-full preservation — DONE (`test_cli.py`,
  `test_cli_diskfull.py`).
- H-02 Early console = `DebugPrint` — PARTIAL. `PortCon` lease gives one user
  program at a time the keyboard/serial stream (shell yields while leased,
  reclaims on owner death); runtime offers `read_line` (echo, backspace,
  timeout) + `INTERACT.E32` fixture (`test_interact.py`). No console server.
  Done = proper console sessions with focus, binary-safe output, scroll/CLS/ANSI.
- H-03 Shell FAT mutex coverage incl. multiline writes — DONE; lock-ordering
  (FAT lock across `E32Run`) is a known inversion — TODO to narrow.
- H-04 Shell job control (background/foreground, `wait`, exit-status history),
  redirection/pipes, batch files — TODO.
- H-05 `ps`/`args`/`kill`/`reap`/`segs`/`pzomb` diagnostics — PARTIAL (see P-05).

## 9. Drivers (PARTIAL)

- D-01 PCI scan — DONE.
- D-02 ATA PIO (LBA28 ≤128GiB, MBR, shared buffer) — DONE.
- D-03 AHCI DMA read/write/flush (ports as Fat devs 4–7) — DONE.
- D-04 UHCI/OHCI keyboards + one direct-root-port xHCI keyboard, polled — DONE.
- D-05 Full USB (hubs, hotplug, mice, mass storage), USB storage as block dev — TODO.
- D-06 NIC driver (QEMU e1000/virtio-net/pcnet) — TODO (blocks all networking).
- D-07 Audio, power management, full RTC/clock — TODO.
- D-08 Driver fault isolation + unload/reload — TODO.

## 10. Storage formats (PARTIAL)

- T-01 FAT32 R/W/format whole-disk, multi-volume + per-volume CWD — DONE.
- T-02 LBA48/>128GiB, GPT, fragmentation/perf, `fsck` — TODO.

## 11. Networking (TODO — nothing yet)

- N-01 NIC bring-up + RX/TX + link status — TODO.
- N-02 Symbian TCP/IP stack port + ESOCK (`RSocketServ`/`RSocket`) — TODO.
- N-03 DHCP/DNS/address config — TODO.
- N-04 End-to-end tests: ping, DHCP lease, TCP echo, socket API — TODO.
- IPC work is same-machine only and does not imply networking.

## 12. Security (TODO)

- X-01 Capability enforcement, platsec diagnostics, process isolation via MMU — TODO.
- X-02 File permissions, secure time, crypto services — TODO.

## 13. C/C++ runtime (PARTIAL)

- R-01 Freestanding runtime (`runtime.h`: cmdline, print, files, chdir) — DONE.
- R-02 `ipc.h` bridge (sync + desc16) — DONE.
- R-04 Sleep/time/random from genuine executives — DONE (`sleep_ms` via fresh
  RTimer per call, `thread_yield`, `time_now` via TimeNow + libgcc division,
  `random32` via MathRandom; `SLEEP.E32` repeats in `test_userland.py`).
  Known port issue: thread-embedded `User::After` completes once then hangs on
  reuse, so the runtime never uses it; blocking `wait` on timers still TODO.
- R-03 Full libc/ESTLIB, C++ exceptions/RTTI, genuine EUSER/CBase — TODO.

## 17. Application SDK (DONE v0)

- SDK-01 `sdk/` tree: `tools/newapp.py` scaffolder, `tools/buildapp.py
  space-safe builder, 5 verified examples (hello/args/fileview/prompt/asleep).
- SDK-02 Docs: getting-started, full `runtime.h`/`ipc.h` API reference,
  honest limits, IPC guide with upstream template pointers.
- SDK-03 `tools/test_sdk.py` in the stable suite: scaffold + build all
  examples + boot and run SDK-built binaries on the OS.
- SDK-04 Client/server example app, C++ support, on-device SDK install — TODO.

## 14. System servers & startup order (TODO)

- S-01 Current: kernel shell spawns everything ad hoc; no service manager.
- S-02 Needed: ordered startup (storage → F32 → console → loader → IPC services),
  restart-on-death supervision, shutdown protocol.
- S-03 Loader + console services against upstream contracts — TODO.
- S-04 Genuine EUSER/CBase build + import ABI — TODO (unblocks CServer2/CSession2,
  active scheduler, real apps).

## 15. GUI (TODO, explicitly last)

- W-01 Window server, events/focus, fonts/bitmaps/composition, input routing — TODO.
- No GUI work until sections 3–8, 13–14 are DONE.

## 16. Testing & release (PARTIAL)

- Q-01 QEMU q35/AHCI regressions green (`test_stable.py` 9/9) — DONE.
- Q-02 Hardware matrix (`docs/hw_tests.md`) — TODO.
- Q-03 Stress/soak (repeat exec/IPC/FS 1000×, OOM, disk-full, power-loss) — TODO.
- Q-04 Negative/fuzz coverage (corrupt E32, bad descriptors, racy kill/reap) — TODO.
- Q-05 Docs kept current (`manual_test_order.md`, `symbian_services.md`,
  `usermode_plan.md`, README §20+) — PARTIAL, ongoing.

---

## Suggested build order (dependencies first)

1. M-03/M-05 (MMU + user heap) — unblocks isolation, malloc, real apps.
2. P-06/P-08 (wait/status + concurrency regression) + P-05 hardening.
3. S-04 → I-08 → I-07 (EUSER/CBase → CServer2 → active scheduler).
4. S-02 + F-06 (service manager + F32/RFs) → retire PortFile.
5. H-02 (console server with input) → user programs become interactive.
6. D-06 → N-01..N-04 (NIC → stack → sockets).
7. L-06..L-09 (full loader) once MMU + F32 exist.
8. W-01 (GUI) last.

Rule: each step must extend `test_stable.py` + `manual_test_order.md` and keep
`kernelhwsrv/` clean. If a step cannot be tested in QEMU, it is not done.
