# Architecture of the Symbian EKA2 x86 PC OS (QEMU port)

This document is the master architecture overview. It describes the system as it
is built and booted today from `qemu-port/`. For the exact commands to boot and
run the self-test, see the canonical usage document
[`docs/r1_boot_and_selftest.md`](r1_boot_and_selftest.md).

## 0. What this project is

This is a genuinely usable x86 PC operating system built on the **real Symbian
EKA2 kernel**, not a simulator or a reimplementation. The authoritative kernel
sources live in `kernelhwsrv/kernel/eka/` (x86 branch, `bx86gcc` exports) and are
**read-only / frozen**: no source file there is edited or rebuilt from scratch
by the port. Everything that makes the kernel boot on QEMU x86 — bootstrap,
variant ASIC, MMU glue, drivers, file system, services and applications — is
new code in `qemu-port/`.

The build (`Makefile`, `toolchain.mk`) compiles the genuine `.cia`/`.cpp`
nanokernel and kernel objects with a freestanding GCC toolchain
(`-m32 -march=i486 -ffreestanding -nostdlib`) and links them with variant code
into `build/kernfull.elf`. That ELF is the payload of the primary boot image
`build/symbian-grub.iso`.

## 1. Layer diagram

```
+--------------------------------------------------------------------------+
| User E32 programs (ring 3)                                               |
|   CALC / HELLO / SNAKE / GUISHELL / SAVER / FILEMGR / web browser ...     |
|   freestanding PE2 images: user/start.S -> app_main -> process_exit        |
|   user/runtime.c (exec0/2/4, chunks, timers, heap)  user/ipc.c  malloc.c   |
+--------------------------------------------------------------------------+
| Services (user processes, CServer2-style IPC)                            |
|   SVCMGR.E32 (ordered launch+supervision)   PORTFILESRV.E32 (!FileServer) |
|   NETSTACK.E32 (!SocketServer: ARP/IPv4/ICMP/UDP/TCP)                     |
|   DNSRES / DNSNET / DHCPC (resolver, queries, lease)                      |
+--------------------------------------------------------------------------+
| Kernel-side support (variant/, fs/)                                      |
|   E32Run loader + EUSER.DLL binding   R1 FS core + FAT32 backend         |
|   PortFile / PortCon logical channels   netbridge rings                   |
|   Interactive shell as a genuine DThread (spawned by Asic::Init3)         |
+--------------------------------------------------------------------------+
| Drivers (variant/, all polled)                                           |
|   e1000  virtio-net/blk/rng  AC97  VBE display  PS/2 kbd+mouse           |
|   USB UHCI/OHCI/EHCI/xHCI  AHCI + ATA PIO  PCI  COM1  PIT  CMOS RTC       |
+--------------------------------------------------------------------------+
| Genuine EKA2 kernel (kernelhwsrv/, READ-ONLY, compiled by qemu-port)      |
|   DProcess/DThread  NKern scheduler  NTimerQ  DFC queues  RHeapK          |
|   IPC server/session/message  chunks  CActiveScheduler/CActive (exec ABI) |
|   exec table: exec0/2/4 = int 0x21; slow execs via EpocSlowExecTable       |
|   Asic (i8259/i8254/VGA text)  K::Init1/2/3  KernelMain                   |
+--------------------------------------------------------------------------+
| Bootstrap + MMU glue (boot/, mmu/, nk/, rom/)                            |
|   GRUB 2.12 El Torito -> Multiboot1 -> ResetEntry/InitCpu -> HwInit0      |
|   4K PD + PT identity map, ROM synth, CR3-switch + #PF hooks              |
+--------------------------------------------------------------------------+
| QEMU x86 hardware: SeaBIOS/GRUB, q35 chipset, e1000, AHCI, VGA/VBE, USB   |
+--------------------------------------------------------------------------+
```

## 2. Boot chain

1. **Firmware → GRUB.** SeaBIOS loads `build/symbian-grub.iso`, a GRUB 2.12
   El Torito image (BIOS `i386-pc-eltorito` plus UEFI `x86_64-efi` entry).
   GRUB auto-selects the "Symbian EKA2 full kernel (genuine)" menu entry after
   a countdown and loads `/boot/kernfull.elf` via the Multiboot1 protocol
   (`magic 0x1BADB002`, flags `ALIGN|MEMINFO = 3`), with `PROGRAMS.IMG` as a
   multiboot module. `tools/mkgrubiso.py` writes the El Torito boot info table
   that GRUB's `cdboot` requires.
2. **Multiboot bridge → ResetEntry.** `boot/multiboot.S` is `_mb_entry`: it
   saves `EAX` (magic) and `EBX` (MBI pointer), mirrors them to a fixed
   handoff page, sets up COM1, loads the GDT and jumps to the ported
   `ResetEntry` in `boot/ported/bootmain_gas.S`. `ResetEntry` checks the magic,
   switches to the boot stack, saves the MBI pointer and calls `InitCpu` from
   `boot/ported/bootcpu_gas.S` (CPUID, CR4.PSE, genuine GDT/IDT descriptor
   bytes) before entering `_E32Startup_Body` (`nk_entry.cpp`).
3. **HwInit0 (`variant/kernfull_glue.cpp`).** This is the genuine variant entry
   called after the ROM/SuperPage addresses land and before `KernelMain`:
   - COM1 UART init;
   - `Mmu_BuildIdentityMap()` + `Mmu_Enable()`: a real **4 KiB page directory
     with shared 4 KiB page tables** covering the low 128 MiB (4 MiB fallback
     PDEs for high MMIO), `CR3` loaded, `CR0.PG|WP` and `CR4.PGE` set;
   - `BootInfo::Capture()` snapshots GRUB's module 0 into high RAM as a
     read-only ramdisk block device before the boot-time memory clear;
   - `PdInit()` installs the address-space switch and `#PF` hooks;
   - `QemuRomInit()` synthesizes the ROM header, root directory and SuperPage
     (`rom/rom_image.cpp`) with valid checksum/signature;
   - clears the kernel data region, publishes `QemuVariantInitialise()` as
     `TheAsic`, and walks `__init_array` to run driver constructors.
4. **Genuine kernel init.** `_E32Startup_Body` → `KernelMain` → `K::Init1/2/3`
   builds the kernel heap, process/thread/DFC objects and timer thread, then
   `Asic::Init1` (`variant/qemu_variant_asic.cpp`) masks the PIC, remaps HW IRQs
   to `0x30/0x38` (vectors `0x20/0x21` stay reserved as the DPL3 exec-call
   gates) and programs the i8254 PIT at 1000 Hz with `MsTickPeriod() = 1000 us`.
5. **Asic::Init3 and the shell.** Late init enables IRQ0/1/4 (PIT tick,
   PS/2 keyboard, COM1 receive), registers the display HAL group, mounts the
   FAT32 volume and the GRUB module as `X:`, installs and launches SVCMGR.E32,
   and calls `ShellSpawnFull()`. The shell runs as a genuine `DThread`
   (supervisor type, kernel process, created through `Kern::ThreadCreate`) so
   it can own Symbian container mutexes; its output is mirrored to VGA text
   (`0xB8000`) and COM1.

Alternate boot paths (custom El Torito bootsect, USB MBR, `qemu -kernel`,
UEFI/BOOTX64.EFI) are retained and documented in `README.md` §§7–19, but GRUB
is the recommended path.

## 3. Kernel layer (genuine EKA2 objects)

The full-kernel image links ~80 genuine kernel objects plus 19 nanokernel
objects compiled from `kernelhwsrv/`. These are the real EKA2 implementations,
not stubs:

- `DProcess` / `DThread`: process creation (`Kern::ProcessCreate` →
  `DProcess::Create`), thread creation (`Kern::ThreadCreate`), priority
  scheduling, logon/wait/exit status and process reaping.
- `CActiveScheduler` / `CActive`: genuine vtable dispatch is exercised through
  the CP4 PE2 fixture, which binds real EUSER ordinals and installs its own
  vtable; `Start()` (@426) waits on the genuine `int 0x20` request semaphore
  and an RTimer completion drives `RunL()` → `Stop()`.
- IPC: genuine server/session/message objects (`CServer2`, `RSessionBase`,
  `RMessage2`) with request statuses, descriptor copies, connect/receive/
  complete and server termination signalling.
- Chunks and timers: private and named global `RChunk` shared memory;
  `RTimer`/`NTimerQ` ticks at 1 kHz; `User::After` via timer executives.
- `RHeapK`, `TBitMapAllocator`, `DObjectCon` census state are live and
  inspectable through shell `meminfo`/`ps`/`threads` verbs.

**Exec table.** Ring-3 calls enter through DPL3 gates at IDT `0x20`/`0x21`.
`user/start.S` implements `exec0` (number in EAX), `exec2` (EAX/ECX/EDX) and
`exec4` (EAX/ECX/EDX/EBX/ESI), all `int $0x21`; `int $0x20` is the fast
request-wait. Fast executives are dispatched through the genuine exec table;
slow executives (e.g. `EExecHandleClose=106`, `EExecChunkCreate=107`,
`EExecThreadKill=115`, `EExecServerCreate=126`, `ProcessCreate/Loaded=152/153`)
go through `EpocSlowExecTable` with a genuine-shaped `SLOW_EXECn` frame. The
port installs a handful of new slow-exec slots (e.g. SvcMgr launch, netbridge
base/size 247/248) by patching invalid entries of that table at runtime —
`kernelhwsrv/` itself is never modified. `build/export/exec_enum.h` is the
generated enumeration of executive numbers.

**PD/CR3 switching.** Because the Direct memory model has no per-process page
tables of its own, `mmu/pd.cpp` supplies them: `PdInit()` creates a 4K PD per
process, installs the naked `PdProcessSwitch` hook via the **public**
`TScheduler::SetProcessHandler()` and installs a `#PF` handler through
`TheExcVectors[14]`. The genuine `ncsched.cia` calls the process handler on an
address-space switch (and sets per-thread `TSS.esp0` from
`NThread::ModifyAttributes`), so CR3 changes on real scheduler context
switches. Threads are tagged with `SetAddressSpace`; the `#PF` handler decodes
CR2/error, resolves the owning PID from CR3 via a side table, and reports an
EKA2 exception to the owning thread. One CPU means no ASID/PCID is needed and
a full TLB flush per switch is acceptable.

## 4. EUSER.DLL and PE2 import binding

`EUSER.DLL` (~412 KiB, checked in at `qemu-port/EUSER.DLL`) is the genuine
`bx86gcc` user DLL: its export directory has **2503 ordinals** defined by
`kernel/eka/bx86gcc/euseru.def` (10 marked `ABSENT`). The PE2 format places the
import address table at `iCodeOffset + iTextSize` (the genuine `pe_tran.cpp`
convention), with import blocks holding a DLL name and count and the IAT
holding 1-based ordinals.

`variant/e32load.cpp` implements the in-kernel loader: `E32Validate` mirrors
`E32ImageHeaderV::ValidateHeader` (header CRC, UID checksum, code/data/import/
relocation bounds, PE2 name/count/IAT terminator agreement), and `E32Run`
creates the process, copies code/data, zeroes BSS, applies TEXT/DATA/INFERRED
relocations, then binds imports. `EUSER.DLL` is loaded **once** from FAT32 into
resident kernel memory (~512 KiB buffer) and its export offsets serve the
exe's IAT slots; the old kernel trampolines (1=Exit, 2=PutC, 3=CmdLen) remain
as a fallback if the DLL is absent. The host twin `tools/e32read.py` applies
the same validation rules, and `tools/inspect_euser_relocs.py` /
`tools/inspect_imports.py` audit the genuine artifact. Most `make userland`
apps are deliberately **import-free** (`dllRefs=0`) and call executives
directly; hand-built `tools/mke32.py` images are the PE2 import fixtures.

## 5. Userland

User programs are freestanding E32 PE2 images linked by `tools/elf2e32.py`
against a small runtime, not against the Symbian C++ SDK:

- `user/start.S`: `_start` calls `app_main`, then `process_exit` (exec 115).
- `user/runtime.c`: executive wrappers for printing, command line, files,
  chunks, timers/sleep, time, random, wait/exit and console input.
- `user/ipc.c`: the C transport for `CServer2`-style IPC — server create,
  connect, receive/complete, synchronous call and descriptor copy slots. One
  outstanding request per thread; it is a transport, not an SDK-compatible API.
- `user/malloc.c`: first-fit, chunk-backed user heap (16 KiB) with alignment,
  hole reuse, coalescing and OOM-NULL.
- `user/e32base.h` / `user/euser_shim.cpp`: minimal shim for the common CBase
  patterns; genuine vtable behaviour comes from the real EUSER ordinals.

Two transitional logical channels bridge kernel services to ring 3:
**PortFile** (`variant/userfiles.cpp`) provides per-channel 64 KiB snapshots
with chunked read/write and per-process CWD, and **PortCon**
(`variant/usercon.cpp`) leases the console input stream to one program at a
time (the shell yields while leased and reclaims dead leases). The R1 file
service (`fs/r1_fs_core.cpp`, `fs/r1_fs_fat.cpp`, `fs/r1_fs_server.cpp`)
provides the core session/request state machine and a FAT32 backend with MBR
parse, 8.3 + LFN, multi-volume `C:`/`D:`/`X:` mounts and per-volume CWD.

## 6. Drivers (all variant-owned, polled I/O)

- **e1000** (`variant/qemu_nic.cpp`): Intel 82540EM (PCI 8086:100E), BAR0
  MMIO, EEPROM MAC, 8×2 KiB TX/RX rings, polled send/recv/link state.
- **virtio-net / virtio-blk / virtio-rng** (`variant/qemu_virtio_*.cpp`):
  legacy and virtio-1.0 PCI capability discovery, split virtqueues, polled
  descriptor/used-ring processing; RNG is a genuine new entropy source.
- **AC97 audio** (`variant/qemu_audio.cpp`): PCI class 04.01 mixer/PCM BARs,
  polled buffer programming, `audio` shell verb.
- **VBE display** (`variant/qemu_display.cpp`): Bochs VBE DISPI registers at
  1024×768×32 (VBE 0x118 geometry) with a VGA mode-13h fallback; GUI execs in
  `variant/qemu_gui_exec.cpp`.
- **PS/2 keyboard and mouse** (`console/kbd.cpp`, `variant/qemu_mouse.cpp`):
  IRQ1/IRQ12 through the genuine `X86IrqDispatcher`; mouse uses a separate PIC
  read-modify-write path to avoid clobbering the keyboard gate.
- **USB** (`variant/qemu_usb.cpp`, `qemu_usb_ohci.cpp`, `qemu_usb_ehci.cpp`,
  `qemu_xhci.cpp`): UHCI, OHCI, EHCI and xHCI controllers with polled HID boot
  keyboards/mice fed into the shell input ring; no USB IRQs.
- **Storage** (`variant/qemu_ata.cpp`, `qemu_ahci.cpp`, `variant/fat32.cpp`):
  ATA PIO (LBA28) and AHCI (LBA48/LBA28 DMA, q35 ICH9), all polled, with FAT32
  mount/create/write/delete/format and multi-volume shell commands.
- **netbridge** (`variant/qemu_netbridge.cpp`): two static rings in low
  identity-mapped RAM, exposed through dedicated slow executives, bridge raw
  Ethernet frames between the polled e1000 and ring-3 user processes.
- Support: PCI config scan (`qemu_pci.cpp`), BIOS/GRUB info capture
  (`bootinfo.cpp`), CMOS RTC (`qemu_variant_asic.cpp`).

Drivers register with `HwInit0`'s `__init_array` walk (after paging and ROM are
ready) or lazily from `Asic::Init3`; none depend on kernel-heap availability at
constructor time.

## 7. Services

- **SVCMGR.E32** (`user/svcmgr.cpp`): trusted service manager launched at boot
  from `Asic::Init3` via `SvcMgrExecInstall` / `SvcMgrBootLaunch`. It starts
  services in order, supervises them through process-logon/death primitives
  with bounded restart backoff, and implements an ordered shutdown.
- **PORTFILESRV.E32** (`user/portfilesrv.cpp`): genuine user process that
  registers the FS owner thread and publishes `!FileServer` with
  `FileServer::NewL` / `Publish()`; `CServer2::Start` runs the accept loop.
- **NETSTACK.E32** (`user/netstack.c`, `netstack_iat.S`, `link_netstack.ld`):
  user-space ARP/IPv4/ICMP/UDP/TCP stack and `!SocketServer` IPC service;
  sockets are new IPC function numbers, not new EUSER ordinals.
- **DNS/DHCP** (`user/dnsresolv.c`, `dnsnet.c`, `dhcpc.c`): RFC 1035 resolver,
  query client and lease client layered over the UDP socket service.
- **Shell** (`variant/kern_shell.cpp`): the interactive `EKA2>` DThread —
  program execution (`exec`), FAT commands, process/thread/`meminfo`/`pci`
  inspection, RTC `date`, and diagnostics (`netbridge`, `audio`, `reap`, …).

## 8. Key design decisions and constraints

- **`kernelhwsrv/` is frozen.** Every adaptation (exec slots, DMA backing,
  build glue) is done in `qemu-port/`; build-time repairs to genuine `.cia`
  quirks are mechanical scripts in `tools/` (`fix_cia_immediates.py`,
  `fix_mchunk_adjust.py`). Genuine `.cia` code is compiled with `-O2` because
  `-O0` prologues corrupt the hand-assembly frames.
- **Direct memory model / identity-mapped RAM.** `linear == physical`, which
  keeps the Multiboot handoff, DMA buffers and netbridge rings trivial, but
  also means the checked-in DLL and user trampolines must live in low RAM.
- **No paging initially, 4K paging now for isolation.** The original full
  kernel was identity-mapped (`CR0` without PG); the OS-CORE upgrade added a
  4K PD/PT map, U/S=0 kernel pages, per-process CR3 and per-page U/S with a
  `#PF` exception path.
- **Polled I/O.** Disk, NIC, audio, USB and virtio devices are driven by MMIO/
  I/O-port polling from the shell or driver hooks; PCI interrupt binding/DFC
  paths are future work. This avoids IRQ-timing bugs in a bring-up port.
- **Single CPU.** No SMP/APIC, no ASID/PCID; the 1 kHz PIT drives the genuine
  `NTimerQ`, and a full CR3 flush per switch is fine.
- **Freestanding toolchain, hermetic build.** No MSVC/MASM/ActivePerl/bldmake:
  MASM bootstrap files were hand-ported to GAS (`boot/ported/`), `.cia` files
  compile directly as C++ with the `__GCC32__` branch, and small shims
  (`inc/plat_priv.h`, `inc/qemu_boot.inc`) replace generated exports.
- **Verification** is QEMU-based: `python3 tools/test_stable.py` runs the
  build plus the QEMU suites; `python3 tools/r1_os_selfcheck.py` is the
  one-command boot/self-test; design history and investigations live in
  `docs/` (see [`docs/DOCS_INDEX.md`](DOCS_INDEX.md)).
