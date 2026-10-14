# Symbian EKA2 on QEMU i386 — port record (Direct model, paging off)

Status (2026-10-14): full Kern boots to an interactive shell; ring-3 user
programs, FAT32 storage, genuine kernel IPC, and an app SDK all work and are
regressed. `make` (default) rebuilds kernels + all boot ISOs; primary image
is `build/symbian-grub.iso`; `python3 tools/test_stable.py` runs build + 12
QEMU suites. What is done vs missing: `docs/full_os_checklist.md`. Manual
test order: `docs/manual_test_order.md`. Sections below are the build log in
phase order; early-phase notes are historical.

## 1. Toolchain: MASM `.asm` + Symbian `.cia` on modern Linux

Host: x86_64 Linux, `gcc 16.2.1 / as+ld 2.46.1 / qemu-system-i386 10.2.2`.
Flags: `-m32 -march=i486 -ffreestanding -nostdlib -D__GCC32__` (see `toolchain.mk`).
No `glibc-devel.i686`, no NASM, no Windows/MSVC, no ActivePerl/bldmake.

### 1a. `.cia` files — compile directly as C++ (no conversion)

`.cia` = C++ with inline `asm("…")` strings, not a separate language.
Every x86 `.cia` already carries a GCC branch:

- `nkern/x86/vectors.cia:21`, `ncsched.cia` — `#ifdef __GCC32__` selects
  `asm("push …"); asm("jmp %a0" : :"i"(&fn))` over `__asm push/__asm jmp`.
- `cpudefs.h:209` — `__NAKED__` is **empty** on `__GCC32__`/x86 (GCC has no
  naked-x86); MSVC path uses `__declspec(naked)`.
- `x86hlp_gcc.inl` provides the GCC helpers (`_allmul`, `_alloca`, …).

Makefile rule for Phase 3 (invoked from §7 on; pattern):

    build/%.o: $(EKA)/%.cia
        $(CXX) $(CXXFLAGS) $(KERNEL_INCLUDES) -x c++ -c $< -o $@

One known gap, already designed for: `x86.h:35` does `#include <plat_priv.h>`,
which bldmake *generated* during export. Phase 3 added a minimal hand-written
`inc/plat_priv.h` shim (page-size constants only) — no logic, ~20 lines.

### 1b. `.asm` files — manual GAS port, originals stay read-only

`kernel/x86/bootcpu.asm` (711 lines), `bootmain.asm` (782), `bootutil.asm`
(~1800+) are pure MASM32: `cseg segment use32`, `PROC NEAR/ENDP`, `INCLUDE`,
`IFDEF/IFNDEF`, `ADR/GETEIP/PRINT/BOOTCALL` macros from `bootmacr.inc`,
`bootcpu.inc` (`INCLUDE config.inc` — the x86pc variant config that was
**never in this repo**; cf. `rombuild/bootx86.bat` → `\os\boardsupport…\x86pc`).

Decision: **hand-port to GAS `.S` (Intel syntax via `.intel_syntax`), file by
file, in `boot/ported/` during Phase 3.** Rejected alternatives:

| Option | Verdict |
|---|---|
| MSVC+MASM / JWASM binary | Rejected: non-hermetic, unreviewable blobs |
| `sed` MASM→NASM auto-translate | Rejected: `PROC`/segments/`ADR` EIP tricks don't map 1:1; silent misassembles |
| NASM rewrite | Rejected: extra dep, still a manual port, GAS already installed |

Porting dictionary (applied in Phase 3, documented here so review is mechanical):

- `cseg segment use32 … / cseg ends` → `.code32` + `.section .text.boot,"ax"`
- `X PROC NEAR / ENDP` → `.globl X; .type X,@function; X: …; ret`
- `INCLUDE x` → `.include "x"` (genuine `bootcpu.inc` values reused, not redefined)
- `IFDEF/IFNDEF/ENDIF` → `.ifdef/.ifndef/.endif`
- `ADR reg,label` → `lea label, reg` (see `inc/qemu_boot.inc`)
- `GETEIP eax` → `call 1f; 1: pop eax`
- `PRINT*/PRTLN/MEMDUMP` → COM1 `outb` helpers (off unless `CFG_DebugBootRom`)
- `BOOTCALL n` → indirect call via superpage boot-table (same ABI, GAS syntax)

## 2. Memory model: Direct (`memmodel/epoc/direct/x86`)

Chosen per Phase 1 recommendation. Reasons:

- Identity-maps RAM (`linear==physical`), so the QEMU bridge (`load @1M`)
  and the bootstrap's `RamPhysicalToLinear` no-ops stay trivial.
- No `SPageInfo` array, no page-table-info self-mapping, no ASID — cf. the
  ~100 lines of `PageInfoMap` setup in `bootmain.asm:618` that Direct skips.
- Matches the bootloader constraint already in-tree
  (`bootcpu.inc:292`: bootloader **must** use Direct).
- `direct/x86/xinit.cpp:1` is tiny (block size, stack/guard sizes, ram-drive
  bounds) — the only per-model C++ we must satisfy in Phase 3.

Pinned slice (`make check-slice` verifies all 37 files):

- boot MASM: `kernel/x86/{bootmain,bootcpu,bootutil}.asm`
- nanokernel: `nkern/{nkern,sched,nk_timer,nklib,dfcs}.cpp`,
  `nkern/x86/{ncsched,ncthrd,ncutils,ncutilf,ncmonitor,vectors,nk_entry}.cia`,
  matching `.cpp` + `ncglob.cpp`, `nctimer.cpp`
- kernel: `kernel/x86/{cinit,k_entry,ckernel,cutils,cexec}.cpp`
- memmodel: `memmodel/epoc/direct/{mchunk,mglobals,minit,mkernel,mutils}.cpp`
  + `direct/x86/{xinit,xkernel,xglobals,xutils}.cpp`
- common: `common/x86/{atomics,cmem}.cia`

## 3. Standalone build (no bldmake)

```
qemu-port/
  Makefile            # relative paths only (workspace has a space); targets below
  toolchain.mk        # CC/CXX/AS/LD/QEMU + freestanding -m32 flags
  boot/multiboot.S    # §4 bridge stub (infrastructure, not a kernel)
  boot/link.ld        # ENTRY(_mb_entry), BASE 0x100000, .multiboot first
  boot/ported/        # Phase 3: GAS ports of the 3 MASM files (populated, see §7)
  inc/qemu_config.h   # C-side variant config (COM1, PIC, PIT, RAM map)
  inc/qemu_boot.inc   # asm-side variant config (replaces missing config.inc)
  inc/plat_priv.h     # Phase 3: minimal export shim (present, page-size constants)
  tools/check_env.sh  # Env probe used by `make check-env`
  build/              # generated (multiboot.o, multiboot.elf)
```

Targets: `check-env` (toolchain probe) · `all` (default: kernels + every
ISO incl. `isogrub`) · `userland` (all 11 E32 programs) · `kernfull.elf`
· `qemu*` boot helpers · `clean`. See the Makefile for the full list.

## 4. Multiboot bridge: QEMU → `ResetEntry` contract

Multiboot1 (§3.1.1): 12-byte header in first 8 KiB, 4-byte aligned —
`0x1BADB002, flags=ALIGN|MEMINFO (0x3), checksum=-(magic+flags)`.
Verified in image: `objdump -s -j .multiboot` shows `02b0ad1b 03000000 …`
at file offset `0x1000`, VMA `0x100000` (`make headers`).

QEMU entry state (already proven by `make qemu` serial log): `EAX=2BADB002`,
`EBX=info*`, 32-bit segments, IRQ off, paging off. `boot/multiboot.S`
saves `EAX/EBX` into `qemu_handoff` **and** mirrors them to the fixed page
`0x9000` (`QEMU_HANDOFF_ADDR`, magic `QEMU_HANDOFF_MAGIC='QEK2'`), inits COM1
`0x3F8` 115200-8N1, prints the banner, and halts. Phase 3 replaces the halt
with: load QEMU GDT → `EDI=handoff superpage slot, ESP=boot stack top,
ESI=ROM-header linear` → `JMP` ported `ResetEntry` (`bootmain.asm:60`),
which then runs the genuine `InitCpu → MapContiguous → EnableMmu →
_E32Startup_Body → KernelMain → A::Init1` chain unmodified.

QEMU invocation (pinned): `qemu-system-i386 -kernel build/kernfull.elf
-m 128 -display none -serial stdio -no-reboot` for direct boots; primary
test path is `build/symbian-grub.iso` (BIOS/UEFI, see §18).

## 5. Verification (all green, run from `qemu-port/`)

- `make` → kernels + all ISOs rebuilt.
- `python3 tools/test_stable.py` → build + 12 QEMU regressions
  (relocs, args, DLL, data-relocs, userland, IPC, CLI, disk-full, crash,
  concurrency, interactive, SDK). Logs: `/tmp/opencode/stabilization/`.
- Early-phase probes (`check-env`, `headers`, serial smoke boots) remain for
  bring-up forensics; transcripts live in `docs/history/`.

## 6. Phase 3 handoff (done — see §7 for the record)

Planned: ported bootstrap GAS files, QEMU ASSP (`assp.h:class Asic`),
`plat_priv.h` shim, first genuine `.cia` compile and `ResetEntry` link.
All landed; §7 is the record.

## 7. Phase 3 record — ported bootstrap + minimal QEMU ASSP (DONE)

Ports (all GAS, `.code32`, i486, freestanding):

- `boot/ported/bootcpu_gas.S` — faithful port of `bootcpu.asm` subset:
  `InitCpu` (EFLAGS.ID→CPUID.01→CR4.PSE), `SetupGDT` (genuine descriptor
  bytes R0CS `00cf9b00`/R0DS `00cf9300`/R3CS `00cbfb00`/R3DS `00cbf300`),
  `SetupIDT`/`InstallIDTEntry` (256 entries, 32 CPU stubs→`VectorCommon`
  with COM1 fault report). GDT/IDT in `.bss`, not superpage (Phase 4).
- `boot/ported/bootmain_gas.S` — `ResetEntry` skeleton in genuine order
  (`bootmain.asm:60`): magic check → `QemuInitialiseHardware` → boot-stack
  switch → `InitCpu` → kernel entry. Entry regs saved to `.bss` before the
  stack switch (stack-saved regs would be orphaned — fixed during bring-up).
- `variant/qemu_assp.{h,cpp}` — `QemuAsic` with genuine `assp.h` method
  names (`Init1/Init3/DebugOutput/Idle/MsTickPeriod/NanoWaitCalibration`),
  `QemuIrq::RemapPic(0x20,0x28)` (ICW1-4 `0x11/0x20,0x28/0x04,0x02/0x01`),
  `QemuPit::Setup(1000Hz)` + latch-poll tick proof, COM1 `puts/puthex`.
- `variant/qemu_kernel.cpp` — `QemuKernelMain(magic,info)` milestone entry
  standing in for `_E32Startup_Body/KernelMain` until the ROM image exists.
- `boot/multiboot.S` — now `call ResetEntry` with `EAX=magic/EBX=info`
  (replaces Phase-2 halt); `make qemu-capture` saves `build/serial.log`.

Serial capture (docs/history/phase3_serial_capture.txt, QEMU i386, `-m 128`):

    EKA2-QEMU bridge: 32-bit protected mode, paging off, COM1 ok
    EKA2-QEMU bridge: multiboot magic 2BADB002 OK
    EKA2-QEMU bridge: jumping to ported ResetEntry...
    EKA2-QEMU ResetEntry: multiboot handoff OK (128M RAM, ROM-in-RAM deferred)
    EKA2-QEMU ResetEntry: calling ported InitCpu (GDT/IDT)...
    EKA2-QEMU ResetEntry: InitCpu done, cpu_features=0x0781ABFD
    EKA2-QEMU kernel side: entered QemuKernelMain (standing in for KernelMain)
    EKA2-QEMU kernel side: multiboot magic=0x2BADB002 info=0x00009500
    EKA2-QEMU Asic::Init1: remapping i8259 PIC master=0x20 slave=0x28...
    EKA2-QEMU Asic::Init1: PIC remapped (IRQ0->0x20, no CPU-trap clash)
    EKA2-QEMU Asic::Init1: programming i8254 PIT 1000Hz...
    EKA2-QEMU Asic::Init1: PIT ch0 latched count=0x00000485 (ticking)
    EKA2-QEMU Asic::Init1: MsTickPeriod=1ms NanoWaitCal=1000
    EKA2-QEMU kernel side: PIT 100ms poll test...
    EKA2-QEMU kernel side: PIT poll done spins=0x01312D01 (counter advanced, IRQs still masked)
    EKA2-QEMU Asic::Init3: late init (nothing QEMU-specific)
    EKA2-QEMU MILESTONE: ResetEntry->InitCpu->Asic::Init1 reached, COM1+PIC+PIT OK
    EKA2-QEMU next: ROM image + EnableMmu + genuine KernelMain (Phase 4)

Scope honesty: GDT/IDT/PIC/PIT/COM1 init above is ported genuine logic and
really executes on QEMU. Full `EnableMmu`/`MapContiguous`/ROM-image
`KernelMain` (processes, NTimerQ-IRQ, superpage) is Phase 4 — it needs the
ROM header + page tables that `rombuild` used to generate.

## 8. Phase 4 record — ROM synth, MMU enablement, KernelMain (DONE)

New components:

- `rom/rom_image.{h,cpp}` — in-memory rombuild stand-in. Compatible-prefix
  layouts (genuine field order through `TRomHeader::iCheckSum@168` /
  `SSuperPageBase::iRamBase@72`; trailing genuine fields deferred and
  documented). Word-sum checksum (`target 0x51EB0001`), QEMU magic/signature,
  `ValidateRom` (magic+checksum+layout) + `ValidateSuperPage`
  (signature+RAM+pageDir-align+cpuId+ramBase).
- `mmu/mmu_gas.S` — ported `EnableMmu`/`MapSingleSection` essence: 32x 4M
  identity PDEs (`P|W|PS`, +`G` when PGE) over 0–128M, `CR3` load,
  `CR0.PG|WP`, `CR4.PGE` + TLB reload. No temp-identity dance (load==link).
- `mmu/mmu.cpp` — `BuildIdentityMap/Enable/Status/TouchTest` wrappers.
- `variant/irq_gas.S` + `variant/irq.cpp` — IDT `0x20` (genuine-empty slot)
  rebound to `Irq0Stub` (pushal+segs → `Irq0Handler_C` tick++ → PIC EOI →
  `iret`); `Takeover()` installs gate, unmasks IRQ0, `sti`.
- `variant/kernel_main.cpp` — genuine-named `KernelMain()`: re-validates
  ROM/SuperPage, confirms `CR0.PG`, takes over IRQ0, waits 5 PIT ticks.
- `variant/qemu_kernel.cpp` — flow is now ROM synth → validate → MMU
  build/enable → touch test → `KernelMain()`.

Serial capture (docs/history/phase4_serial_capture.txt):

    EKA2-QEMU ROM synth: TRomHeader+SuperPage built (rombuild stand-in)
    EKA2-QEMU ROM synth: ROM check OK SuperPage check OK
    EKA2-QEMU ROM: base=0x00100000 kernData=0x00200000 kernLimit=0x00400000
    EKA2-QEMU SuperPage: ram=0x08000000 pageDir=0x0010A000 cpuId=0x0781ABFD
    EKA2-QEMU MMU: building 32x4M identity map (0-128M)...
    EKA2-QEMU MMU: enabling paging (CR3 load, CR0.PG|WP, CR4.PGE)...
    EKA2-QEMU MMU: paging on, CR0=0x80010011 (PG=1)
    EKA2-QEMU MMU: touch test through paging OK
    EKA2-QEMU: transitioning QemuKernelMain -> KernelMain...
    EKA2-QEMU KernelMain: entered (genuine entry point name)
    EKA2-QEMU KernelMain: ROM validation OK (magic+checksum+layout)
    EKA2-QEMU KernelMain: SuperPage validation OK (signature+RAM+pageDir+cpuId)
    EKA2-QEMU KernelMain: paging live CR0=0x80010011 (PG=1) CR3=0x0010A000
    EKA2-QEMU KernelMain: unmasking IRQ0, enabling interrupts...
    EKA2-QEMU KernelMain: IRQs unmasked, waiting for 5 PIT ticks...
    EKA2-QEMU KernelMain: PIT ticks seen=0x00000005 (ticking, timer path live)
    EKA2-QEMU MILESTONE: paging + ROM/SuperPage + KernelMain + IRQ PIT ticks OK

Scope honesty: paging validates for real (`CR0.PG=1` read back post-enable,
touch test through the page tables, deterministic across runs). The 5 ticks
are counted by a real IRQ0 ISR through the remapped PIC with `sti` — i.e.
the exact interrupt source genuine `NTimerQ`/scheduler consume. Compiling
the full genuine `NKern`/`Kern` object set against these structures is the
defined next step, not claimed here.

## 9. Phase 5 record — nanokernel scheduler + thread switching (DONE)

Genuine sources integrated by name/layout (not yet compiled as objects —
they need the full bldmake export tree; the harness below is
structure-compatible with them):

- `nk/nk_sched.h` — `NThreadState` values (`nk_priv.h:48`), `KNumPriorities=64`
  (`nkern.h:72`), `SThreadStack` EXACT field order (`nk_plat.h:59`),
  `TScheduler::{Reschedule,YieldTo,TimesliceTick,iRescheduleNeededFlag,
  iCurrentThread}` (`nk_priv.h:304`), `NTimerQ::{Init1,Tick}` (`nk_priv.h:431`),
  `NKern::{TickCount,YieldTimeslice}` (`nkern.h:945`).
- `nk/nk_ctx_gas.S` — `NThread_Switch`/`NThread_InitStack`, saving the genuine
  register set (`fs/gs/ebp/edi/esi/ebx/CR0`, cf. `ncsched.cia` Reschedule);
  FPU/`iCoprocessorState` deferred. Single address space (no TSS.esp0/CR3
  switch — documented).
- `nk/nk_sched.cpp` — 64-level ready bitmap, quantum 10ms with
  tick-requested preemption (`TimesliceTick` sets the flag; the switch happens
  at the yield boundary — as genuine does at unlock/yield). IRQ0 calls
  `NTimerQ::Tick` at 1000Hz. Threads A+B (pri 10, 8K stacks) round-robin.
- `variant/irq.cpp` — ISR now drives `NTimerQ_TickIrq` too; `variant/
  kernel_main.cpp` starts the scheduler after IRQ takeover.

Serial capture (docs/history/phase5_serial_capture.txt, ticks advance ~50/iter proving
live IRQ0 during thread execution):

    EKA2-QEMU NKern: scheduler init (64 pri, quantum 10ms)...
    EKA2-QEMU NKern: threads ready A(pri10) B(pri10), IRQs live, start...
    EKA2-QEMU NKern Thread A: iter=0x00000000 tick=0x00000001 yield->B
    EKA2-QEMU NKern Thread B: iter=0x00000000 tick=0x00000033 worked, yield->A
    EKA2-QEMU NKern Thread A: iter=0x00000001 tick=0x00000033 yield->B
    EKA2-QEMU NKern Thread B: iter=0x00000001 tick=0x00000065 worked, yield->A
    EKA2-QEMU NKern Thread A: iter=0x00000002 tick=0x00000065 yield->B
    EKA2-QEMU NKern Thread B: iter=0x00000002 tick=0x00000097 worked, yield->A
    EKA2-QEMU NKern Thread A: done, switch->main
    EKA2-QEMU NKern: back in main thread, iters=0x00000003 A_switches=0x00000004 B_switches=0x00000003
    EKA2-QEMU MILESTONE: thread switch A->B->A OK under paging

Scope honesty: the switch is a real register-level context switch
(`NThread_Switch` saves/restores the `SThreadStack`-exact frame and resumes a
different stack/`EIP`), driven by `YieldTimeslice` yields plus timer-requested
preemption flags — all under paging with IRQs unmasked. Full genuine
`sched.cpp`/`ncsched.cia` object compilation (needs exported `plat_priv.h` +
variant DFC/wait objects) remains the defined next step.

## 10. Phase 6 record — VGA console, PS/2 keyboard, shell thread (DONE)

- `console/vga.{h,cpp}` — 80x25 cells at phys `0xB8000` (identity-mapped),
  per-write color attributes, scroll, CRT-cursor via `0x3D4/0x3D5`, `\n\r\b`
  handling; every byte mirrored to COM1 so serial captures stay complete.
- `console/kbd.{h,cpp}` + `Irq1Stub` (`variant/irq_gas.S`) — IDT `0x21` gate,
  IRQ1 unmask, 8042 drain loop, Set-1 map (letters/digits/punct, Shift via
  `0x2A/0x36`, releases ignored, `0xE0` extended skipped), 256B ISR ring,
  `SerialPoll` (LSR.DR→RBR) so headless `-serial stdio` is interactive.
- `console/shell.{h,cpp}` — shell `NThread` (id 3, pri 10, 16K stack) + idle
  `NThread` (id 4, pri 0, `sti;hlt` loop); line editor (echo/Backspace/ESC);
  `help version uptime threads reboot` (`threads` walks the new
  `TScheduler::Register/ThreadAt` table; `reboot` = 8042 `0x64:0xFE`).
  `TScheduler::Reschedule` now round-robins highest-ready != current.
- Verified live: serial typing (all 5 commands), PS/2 `sendkey h e l p ret`
  via monitor socket (IRQ1 path), `reboot` double-banner, Backspace/ESC/unknown.

Serial capture (docs/history/phase6_serial_capture.txt):

    Symbian EKA2 x86 bare-metal port -- interactive console
    VGA 80x25 + COM1 mirror, PS/2 IRQ1 + serial input
    EKA2> help
    commands: help version uptime threads reboot
    EKA2> version
    Symbian EKA2 microkernel, x86 bare-metal research port (QEMU i386)
      cpu_features=0x0781ABFD CR0=0x80010011 tick=1ms direct-mmap phases 1-6
    EKA2> uptime
    uptime: 5911 ms (5 s)
    EKA2> threads
    id pri state   switches stackTop
    0  0   READY  0x00000000 0x0010A1E0
    1  10   READY  0x00000004 0x00111280
    2  10   READY  0x00000003 0x0010F280
    3  10   READY  0x00000001 0x001164A0
    4  0   READY  0x00000000 0x001124A0
    EKA2>

Notes: piping input the instant QEMU starts can drop the first byte in the
emulated 16550 before guest COM1 init (observed once as `help`->`elp`);
delayed or human typing is exact. VGA output was validated via the COM1
mirror and writes to the mapped framebuffer (headless host has no screen).

## 11. Phase 7 record — meminfo, bootable ISO, docs (DONE)

- `meminfo` (`console/shell.cpp`): SuperPage RAM base/size, Multiboot
  `mem_lower/mem_upper` (captured from the real firmware MBI into
  `variant/bootinfo.*`), live `CR3/CR0.PG/CR4.PSE/PGE` + `cpu_features`,
  kernel heap bounds, direct-map layout. Output:
  `RAM base=0x0 size=0x08000000 (128M)`, `lower=639K upper=129920K`,
  `CR3=0x0010C000 PG=on PSE=on PGE=on`, `kernData/Limit/initHeap`.
- `make iso` → `build/symbian-pc.iso` (88 KiB, `SYMBIAN_EKA2`): GRUB-free
  El Torito (own `boot/bootsect.S` + `pycdlib`, no xorriso). Boots via
  `qemu-system-i386 -cdrom build/symbian-pc.iso`; bare-metal boot is
  untested (QEMU/OVMF only verified — see the checklist).
- `docs/getting-started.md`: prerequisites, build, run, shell, layout,
  earned porting notes, non-goals.
- Serial capture (docs/history/phase7_serial_capture.txt, 69 lines): full CD boot
  (bootsect → PM → KernelMain → scheduler → threads → `EKA2>`) plus
  `help/version/uptime/threads/meminfo` transcripts.
- Decisive fixes this phase: PHDR-correct ELF loading (raw linear copy put
  code 0x1000 too high); missing `CR0.PE` switch before PM code; own debug
  marker clobbering `EAX` (magic); `movw` leaving MBI high-half residue;
  real-mode string instructions replaced by discrete moves everywhere.

## 12. Phase 11 record — full Kern personality boots (DONE)

- `make build/kernfull.elf`: 80 genuine kernel objects + 19 kern-flagged
  nanokernel objects (build/knk, no `__STANDALONE_NANOKERNEL__`) + new
  variant (`qemu_variant_{rom,asic,int}.cpp`, `kern_shell.cpp`,
  `kernfull_glue.cpp`). Links with zero undefined refs; default image
  (`make`) untouched.
- Genuine `KernelMain` runs: variant ROM/SuperPage synth, `M::Init1/A::Init1`
  (PIC/PIT/IDT/TSS), kernel heap + process + initial/supervisor threads,
  DFC queues, timer thread, `K::StartKernelServer`; shell as genuine
  `NThread` (prio 10) with `help/version/uptime/meminfo/reboot`; 1kHz ticks
  verified (+4004ms/4s), PS/2 via `sendkey`, 8042 reboot reboots.
- Transcripts: `docs/history/phase11_full_kernel_shell.txt` (-kernel),
  `docs/history/phase11_full_kernel_cd.txt` (CD).
- Upstream-gap repairs (build glue only, genuine tree clean): (1) kernel
  `.cia` need `-O2` (frame-pointer omission; `-O0` prologues corrupt hand-asm
  frames); `cexec.cia` "impossible constraint" worked around by
  `tools/fix_cia_immediates.py` (direct function addresses, asserted 2
  sites). (2) Direct-model `DMemModelChunk::Adjust` never tracks `iSize`,
  starving the RMessageK pool (`tools/fix_mchunk_adjust.py` adds the
  moving-model bookkeeping; x86/Direct never shipped, untested upstream).
  (3) PIC remapped to 0x30/0x38 (IDT 0x20/0x21 stay reserved as the DPL3
  exec-call gates; cf. §20 exec-gates). (4) Bootstrap
  GDT carries a real TSS for LTR. (5) Dummy ROM extension (zero extensions
  panic `RArray(0)`).

## 13. Phase 12 record — memory numbers + streaming ISO loader (DONE)

- `meminfo` (full kernel): heap base/max/total, RAM bitmap free blocks,
  object counts, msgpool free — all genuine (`RHeapK`, `TBitMapAllocator`,
  `DObjectCon`). Paging stays off by design (Direct model: linear==physical;
  `CR0=0x19`). (The PG=1 transcript in §9 is the separate nanokernel harness
  proving paged boot; the full kernel here is identity-mapped.)
  (`RHeapK::Available` faults by design; not used.)
- `boot/bootsect.S` is now a streaming loader: 16KB EDD-v1 bounce +
  per-chunk PT_LOAD streaming (table parsed from chunk 0) + table BSS zero.
  No staging limit (killed the VGA/BIOS-overwrite bug at >512K and the
  SeaBIOS-hanging EDD-v3 experiment). Both ISOs boot from it.
- `make isofull` → `build/symbian-pc-full.iso` (827392 bytes, 376 sectors):
  full kernel on CD, interactive (`help`, `meminfo` verified from disc).
- Storage boundary: ROM root is a valid empty `TRomDir`; no FS/drivers/user
  binaries (needs an E32 image toolchain: petran/rombuild equivalent).

## 14. Drivers batch — serial IRQ, DThread shell, threads/ps/date/pci (DONE)

- COM1 receive via IRQ4 (16550A RDA + OUT2 gate, ISR drains RBR into the
  shared input ring). Proven: all serial input arrives with the poll
  fallback compiled out; fallback kept for robustness.
- Shell promoted from raw `NThread` to genuine `DThread` (supervisor type,
  kernel process, via sanctioned `Kern::ThreadCreate`): container mutexes
  need DThread context (`EMutexWaitNotDThread` otherwise).
- New commands (all genuine state): `threads` (name/prio/nanokernel state
  for all 7: Null/Supervisor/DfcThread0-1/DShBuf/Timer/Shell), `ps`
  (ekern.exe), `date` (CMOS RTC → YYYY-MM-DD hh:mm:ss), `pci` (config-space
  scan: PIIX host/ISA/IDE/PM-bridge, VGA, e1000).
- Shell output discipline: genuine `Kern::Printf` ends every call with
  CR/LF, so prompt/echo/date-padding use a raw COM1+VGA emitter (fixes the
  vertical-text bug).

## 15. Boot media — hybrid ISO for CD, USB, VirtualBox, real hardware (DONE)

- Both ISOs are hybrid: El Torito (CD) + MBR partition (USB-HDD). `make`
  (default goal) always rebuilds the PC ISOs plus `build/symbian-grub.iso`
  (§18, the primary test image), so test media never goes stale.
- `boot/mbr.S` (+`boot/mbr.ld`): USB path loads the 4096B El Torito image
  via 8x1-sector EDD reads straight to 0x7C00 (self-relocated to 0x6000, so
  the load never eats its own tail). `tools/mkiso.py` writes sector 0
  (active 0x96 partition + 0x55AA) and patches the boot-image LBA.
- Sector-unit discipline (SeaBIOS/QEMU forensics): CD EDD uses 2048B
  units, HDD EDD 512B units — kernel LBA (QHBA), transfer count, and LBA
  advance all scale ×4 off-CD (selected by drive number < 0xE0); boot-image
  LBA likewise. `disk_fail` prints LBA+count evidence cells.
- Defensive BIOS: A20 via 0x92 *and* 8042-KBC fallback; CS normalized at
  MBR entry; segments re-pinned after every INT13 (SeaBIOS trashes them);
  no string ops / no stack across BIOS calls; 16-bit address hygiene
  (`xorl` before every `movw`-loaded 32-bit base).
- Verified matrix (QEMU): CD + USB-HDD × {nanokernel, full kernel} all reach
  `EKA2>` with working shell; q35 CD too. VirtualBox GRUB boot verified
  later (see §18); physical hardware untested.

## 16. Storage — ATA PIO + FAT32 read/write/format (DONE)

- `variant/qemu_ata.cpp`: primary/secondary IDE, IDENTIFY (ATA vs ATAPI,
  LBA28 sectors, model strings), polled PIO sector R/W with timeouts,
  cache flush. `ide` lists devices + MBR partitions.
- `variant/qemu_ahci.cpp` (this session): AHCI SATA for QEMU-q35 ICH9,
  VirtualBox AHCI, real HW IDE/compat mode. GHC AE + BIOS/OS handoff +
  HBA reset, per-port start (CLB/FB, FRE/ST, DET/SIG), IDENTIFY, LBA48/
  LBA28 DMA R/W + flush, all polled. Ports present as Fat devices 4..7
  (`ide` tags `[AHCI]`); `ahci` verb for controller/port info. Hard-won
  notes: command-header DW0 packs CFL|W|C|PRDTL (CTBA is DW2 — a shifted
  word hangs CI forever); PRD flags_size is entry bytes 12..15; never
  COMRESET a working link (sets ERR_STAT, which QEMU needs clear to
  complete); slot 1 used (slot 0 left clear of BIOS-era residue).
  Verified on q35: mount C: + `exec` user EXEs + write/type/del round-trip
  byte-verified vs Linux mcopy. ATAPI skipped (like legacy).
- `variant/qemu_xhci.cpp`: xHCI PCI detect (0C.03.30), controller reset,
  DCBAA/scratchpads, command/event rings, root-port reset, slot/address,
  EP0 control requests and a polled interrupt-IN HID boot keyboard.
  `usb` auto-probes an attached xHCI keyboard when UHCI/OHCI are absent;
  `xhciports` shows connected ports and `xhcikbd N` can select a root port.
  Verified with QEMU `qemu-xhci` and `nec-usb-xhci`; the checked-in
  `tools/test_xhci_keyboard.py` injects `version` through QEMU's USB
  keyboard and confirms the shell receives it (8 HID key events, no fault).
  Current scope: one directly connected keyboard; no hubs, hotplug,
  multi-device or xHCI mouse support. Real hardware/VirtualBox untested.
- `variant/fat32.cpp`: MBR parse, BPB mount, cluster chains, 8.3 + LFN
  reads, file/dir create/write/delete, per-volume CWD with `cd`/`cd ..`,
  whole-disk `format` (aligned 0x0C partition, BPB+FSInfo+twinned FATs+root,
  volume label, RTC date stamps, FSInfo free-count maintenance). No FAT
  cache: all FAT updates write every copy through. Shell: `ide mount vol
  drive dir cd type write del mkdir format` (+ quotes for spaced names,
  multi-line `write`, `\n`→CRLF fixup on `type`).
- Multi-volume: up to 8 mounted volumes (`C:`, `D:`, ...) across drives and
  partitions, each with its own CWD. `mount` (no args) mounts every FAT32
  partition found + lists; `vol` lists (`*` = active); `drive X` or bare
  `X:` switches; file args take `X:name` prefixes; prompt shows `C:/dir>`.
  Failed mounts leave the active volume untouched; `format` drops that
  disk's stale volumes and re-mounts fresh (letters reused, never leaked).
- Verified both directions: our reader vs Linux-written FAT32 (8.3 + LFN);
  our writer vs Linux (`mcopy` byte-exact incl. 1266B multi-cluster;
  `fsck.vfat` clean). Multi-drive verified live: 2×64MB disks formatted to
  `C:`+`D:`, cross-drive write/type byte-exact from the host, subdir ops,
  `vol`/`drive`, format-drop semantics. Edge cases: overwrite-truncate,
  existing-mkdir, non-empty-dir delete refused, missing names, explicit
  `mount 0 0`, bad drive letters.
- `tools/mkdisk.py`: `blank` (zeroed, format it in the shell) and `seeded`
  (Linux FAT32 + test files) raw images; `make qemu-hd` boots with one
  attached as dev 0. VirtualBox: attach the raw image as an IDE/SATA disk.
- Honest limits: ATA PIO is LBA28-only (≤128GiB), polled, one shared sector
  buffer (shell-only use); AHCI path does LBA48/LBA28 DMA. MBR only, no
  disk-management utility (by request).

### Flashing / testing

- CD: burn the `.iso` as an image (not as a file).
- USB (Linux): `sudo dd if=build/symbian-pc-full.iso of=/dev/sdX bs=4M
  status=progress conv=fsync` (whole stick, not a partition; CHECK sdX).
  Windows: Rufus/balenaEtcher in DD/image mode.
- VirtualBox: new VM, type Other/Unknown, 128MB+, attach ISO to IDE CD or
  the image to SATA as existing disk, enable IO-APIC off, serial port to
  host pipe for logs.
- Real hardware needs: 486+, EDD BIOS, legacy/CSM boot *or* UEFI (x64,
  Secure Boot off), ~64MB RAM, COM1 for logs (115200 8N1), PS/2 or serial
  for input.
- Report: boot banner text, where it stops, `EKA2>` transcript, board/BIOS.

## 17. UEFI boot — BOOTX64.EFI loader, dual BIOS+UEFI ISOs (DONE)

- `boot/efi/loader.c` (+`efi.h`, `trans64.S`): freestanding X64 PE32+
  (`-mabi=ms`, `-fshort-wchar`), built with system gcc + `ld -mi386pep`,
  junk sections stripped (firmware rejects `.comment`-at-RVA-0 etc.).
  Loads `EKA2/EKERN.ELF` from its own FAT volume via SimpleFileSystem
  (each ISO carries its own kernel copy — no ISO9660 driver needed),
  places PT_LOADs at linked addresses <4G, synthesizes the Multiboot1 info
  block, exits boot services, drops long→protected mode (PG/PAE/LME off)
  and jumps to `_mb_entry` with the BIOS-path contract. Kernel can't tell.
- `tools/mkfat.py`: minimal FAT16 writer (EFI/BOOT/BOOTX64.EFI +
  EKA2/EKERN.ELF); per-ISO payload. `tools/mkiso.py`: second El Torito
  entry (platform 0xEF) + MBR ESP partition (type 0xEF) sharing the file.
- Key forensics: EDK2 rejects PEs with junk sections; ConOut calls
  between map and exit invalidate the MapKey (direct serial used instead);
  32-bit relays need HIGHLOW-applied addresses (image stays <4G).
- Verified under OVMF: UEFI CD + UEFI HDD × {nano, full} all reach `EKA2>`
  with working shell/ticks. Legacy matrix re-verified after the mkiso
  changes. Secure Boot unsigned → disable it (documented for testers).

## 18. GRUB is the primary boot path (DONE)

- `build/symbian-grub.iso` (`make isogrub`, now in default `make`) is a
  GRUB2 combo: BIOS El Torito (`i386-pc-eltorito`, prefix `(cd)/boot/grub`,
  modules + Rock Ridge lowercase names on disc) + UEFI El Torito (FAT with
  GRUB `x86_64-efi` core that `search`es the CD for the shared config).
  Both menus offer full kernel + nanokernel. Kernels needed zero changes
  (they already carry a valid Multiboot1 header, flags=3); `set
  gfxpayload=text` avoids a video-mode failure at `boot`.
- Why: our hand-written bootsect's EDD streaming hangs VirtualBox's BIOS
  on the 2nd read (no error, no return), and GRUB's `cdboot` replacement
  needed the El Torito **boot info table** (`bi_pvd/bi_file/bi_length` at
  image offset 8 — what xorriso `--boot-info-table` writes, pycdlib
  doesn't). Without it: instant `no boot info` on every BIOS. `mkgrubiso.py`
  does a two-pass master to patch it (size-stable, like `mkiso.py`).
- Verified: BIOS GRUB → full kernel → `EKA2>` (VGA screenshot) and UEFI
  GRUB → full kernel → `EKA2>` (OVMF + VirtualBox). The hand-written
  bootsect path is retained for QEMU/real-hardware reference but is no
  longer the recommendation for VirtualBox.

## 19. USB HID — UHCI + OHCI + xHCI, polled keyboards (DONE within bounds)

- `variant/qemu_usb.cpp`: UHCI backend (PCI 0c03/00, I/O BAR, HCRESET,
  1ms frame schedule, TD/QH engine) + shared front-end (control-transfer
  enumeration, HID boot-protocol parse, usage→ASCII with shift, release
  tracking into the shell ring). `variant/qemu_usb_ohci.cpp`: OHCI backend
  (MMIO BAR, HCCA + periodic ED, CLF control engine, CC-field polling).
  No IRQs anywhere: `UsbPoll()` runs in the shell loop.
- `usb` enumerates all controllers/ports (`uhci`/`ohci` tags, endpoint
  info, `N key(s) from USB HID` counter); `usb poll` dumps one raw
  interrupt-IN (bring-up debug). Up to 4 devices; keyboards feed the
  shell, mice enumerate (no GUI consumer yet).
- Verified (QEMU, end-to-end keystrokes): UHCI + `usb-kbd` → `1 key(s)
  from USB HID`; OHCI + `usb-kbd` → `xyz` typed on the USB path with PS/2
  input disabled in-guest, proving USB delivery independent of PS/2.
  Forensics: QEMU's HID only reports on press/release *edges* (NAKs while a
  key is held — correct, not a stall); UHCI NAK keeps Active set (fast
  idle path); OHCI idle = CC NotAccessed. `usb only` was a diagnostic
  since removed: it proved input paths are independent but starved the
  COM1 RX ISR (which drains into the PS/2 ring), so serial bytes rotted.
- Notes: `usb` enumerates all controllers/ports (`uhci`/`ohci` tags,
  endpoint info, `N key(s) from USB HID` counter); `usb poll` dumps one
  raw interrupt-IN (bring-up debug). Up to 4 devices; keyboards feed the
  shell, mice enumerate (no GUI consumer yet). Polled only — no USB IRQs.

## 20. User-mode system — loader, processes, files, IPC, SDK (RUNNING)

- `tools/mke32.py`: builds a minimal valid E32 EXE (220B: 156B
  `E32ImageHeaderV` NoHoles + 64B x86 code) for `ECpuX86`, import-free,
  uncompressed, codeBase `0x400000` (user side, Direct model). Header CRC
  + UID checksum use the genuine algorithms (`Mem::Crc32`/`TCheckedUid`
  from `kernelhwsrv/.../euser/us_func.cpp`).
- `tools/e32read.py`: host validator mirroring genuine
  `E32ImageHeaderV::ValidateHeader` (`f32image.h`) — exit 0 valid,
  exit 1 corrupt (CRC/uid/sig/entry/regions/export-tail all checked).
- In-kernel twin `variant/e32load.cpp` (`E32Validate`, same genuine CRC
  tables, freestanding, no genuine-header dependency) + `FatRead`
  (raw cluster-chain read into a 64K BSS buffer) + shell `exec <file>`.
- Verified live (QEMU + seeded FAT32): `exec HELLO.E32` prints uids /
  entry / bases / sizes; `exec HELLO.TXT` rejected; corrupt `BAD.E32`
  prints `corrupt` — host and kernel verdicts agree on all three.
- STAGE 3a-RUN (this session): `exec` now RUNS import-free EXEs through
  the genuine path (`E32Run`: `Kern::ProcessCreate` → loader copy to the
  allocated run address → `DProcess::Loaded` → `Resume`). Verified full
  lifecycle in one log: create r=0 at run=0x2200000, `Loaded r=0`, user
  thread executes at CPL3 (regs prove it), draws `HI! ` to VGA row 22
  (memory-read confirmed), exits, shell resumes, `ps` lists
  `HELLO[10000002] id=8`. Demo code is position-independent (relative +
  absolute constants only) since `AllocRegion` ignores the linked base.
- Port gaps fixed along the way (all variant-side, genuine untouched):
  `X86::Init1Interrupts` never runs here, so (a) nothing ever LTR'd —
  first ring-3 preemption triple-faulted (#TS); fixed lazily with the
  bootstrap TSS (SS0=0x10, dedicated stack, `X86_TSS_Ptr` published so
  reschedule ESP0 updates land live); (b) the bootstrap GDT's TSS
  descriptor misplaces base_mid (genuine `SetTssDescriptor` puts it at
  bits 0-7) — fixed to match genuine bit-for-bit incl. AVL. (Also:
  genuine `__lidt` LTRs 0x28 as a side effect — a redundant reload in
  our code faulted and was removed.)
- Debugging notes: `TBuf8` needs its ctor (placement-new after `AllocZ`,
  else `Copy` panics); unknown header priority maps to `EPriorityForeground`
  (a 0/garbage header must not kill exec); a spinning user thread legitimately
  starves lower-priority threads (genuine priority scheduling — use `kill`).
- Honest limits: pilot `EUSER.DLL` serves ordinals 1–3 (Exit/PutC/CmdLen;
  generated, not genuine EUSER — the only real DLL file), command lines only
  (no environment block), no capabilities enforcement, exit via ThreadKill
  exec. Teardown is repaired (reaper + heap-routed free, §20 teardown notes
  below); steady state holds with zero faults. Full DLL loading still out.
- Data/BSS/relocs (this session): data copy + BSS zero + reloc apply
  (TEXT/DATA/INFERRED per genuine `svRelocateSection`) all in `E32Run`;
  green-`D` proves fixups in-thread; BSS zero proven by direct reads.
- Initialized-data relocation regression: `tools/mke32.py --data-reloc`
  emits an EXE containing a pointer in `.data` to another `.data` word.
  `E32Validate` checks data-relocation sites against initialized data,
  and `E32Run` applies them to the process data run address. Run
  `python3 tools/test_e32_relocs.py` for malformed-section rejection and
  `python3 tools/test_data_reloc.py` for a QEMU/FAT32/ring-3 readback.
  This does not add DLL file loading or general import resolution.

### DOS-style shell commands

The shell accepts command names without regard to case. `HELP` shows a short
grouped command list; `HELP COPY` (or another command) prints usage and a
description. `DIR /W` displays a compact listing; `CLS` clears the screen.
`COPY [/Y] source dest` copies a file, refusing to replace an existing file
unless `/Y` is supplied. On overwrite, the old file's chain remains attached
until the replacement is ready; a disk-full failure preserves its contents.
`REN old new` changes a same-directory 8.3 file entry without copying data,
refusing collisions and LFN-backed entries. `MD`/`RD` and `ERASE` are aliases
for `MKDIR`/`RMDIR` and `DEL`. File copying is limited to 64 KiB and FAT32's
current 8.3 output names. Paths may use mounted drive prefixes (`C:FILE.TXT`);
`DIR [/W] [C:\\folder]` temporarily visits a directory without changing the
prompt, including nested 8.3 paths. `DIR *.TXT`, `DIR C:\\D\\*.E32`, and `?`
wildcards filter names; listings show padded names, byte sizes, and file/dir
totals, with no match reported as not found. Missing or bad arguments now
print the same `HELP <command>` text as `COMMAND /?`. `CD`, `TYPE`, `WRITE`, `COPY`, `REN`,
`DEL`, `MKDIR`, `RMDIR`, and `EXEC` accept nested paths too; `REN` still
refuses cross-directory moves. `COMMAND /?` shows per-command usage without
running it (including destructive commands such as `FORMAT`). `DATE` shows
the RTC clock, and `DATE YYYY-MM-DD HH:MM:SS` sets it (2000–2099, validated,
read back). `EXITS` lists recent user-process deaths with names and reasons,
surviving reaping. File commands
still require 8.3 names for new entries; LFN files can be read by name. Run
`python3 tools/test_cli.py` and
`python3 tools/test_cli_diskfull.py` for disposable QEMU/FAT32 regressions.
- BSS TRANSIENT SOLVED (was: demo read 0xF000FF53): demo bug — the BSS
  check did `mov eax,[bss]` (A1, correct: EAX=content=0) followed by a
  stray `mov eax,[eax]` that dereferenced address 0 (BIOS-area garbage
  0xF000FF53, low nibble 3). Removed the second load; demo now draws
  '0'. Loader zeroing was correct all along. Same garbage value also
  explained a stale-EBX red herring in heap debugging.
- §20 exec-gates, exit, heap, pilot imports (this session):
  IDT 0x20/0x21 installed DPL3 (`SetupExecGates`, genuine
  `__X86Vector20/21` already linked); user exit = slow exec
  `EExecThreadKill=115` with a genuine-shaped SLOW_EXEC4 frame
  (arg order: arg4..arg1 then filler ret slot then ebx/esi — two
  earlier layouts shifted registers and killed with handle 0).
  `kill <id>` (E32Kill: Open under container lock, release, then Die;
  holding the lock across Die faults 88) reaps spinners; `exec` closes
  its own handle after Resume (no handle leak on our side) and the demo
  closes its chunk+mutex via `EExecHandleClose=106` before exiting.
  Heap proven live: private 4KB chunk via genuine `EExecChunkCreate=107`
  (TChunkCreate per genuine `RChunk::Create` conversion), write+readback
  two dwords via `EExecChunkBase=1`, green-`H`; `EExecMutexCreate=111`
  green-`m`. Private-chunk errno reporting drew -46 once: root cause was
  a NULL struct pointer (a `mov ebx,[esp+16]` that loaded 0 in every live
  test though GDB showed the slot holding P — sidestepped by recomputing
  `EBX=ESP+20`; cause unknown, possibly a QEMU-gdbstub observation
  artifact; the NULL read BIOS-area garbage with ECache bits set, hence
  PermissionDenied). Pilot imports: EUSER ordinals 1=Exit / 2=PutC
  served from kernel trampolines (Direct model: all RAM XN-free);
  ELF-style import section + IAT validated like genuine
  (`ValidateImports` bounds discipline) and resolved positionally;
  `build/imp.e32` draws via imported PutC and exits via imported Exit
  with zero faults; bad-DLL/bad-slot/bad-count rejected identically by
  kernel and host twins. `exec` verified from `symbian-grub.iso` (CD
  boot + FAT disk) as well as `-kernel`, on BIOS and UEFI (OVMF/pc and
  q35 with AHCI).
- §20 teardown REPAIRED (this session): two composed fixes.  (a) Post-mortem code-seg reaper (`E32Reap`, variant-side, runs on every
  `exec` + `reap` verb): genuine F32 owns a seg handle per load and drops
  it after process death; our loader bypassed F32, leaving the kernel
  creator ref forever, so the seg (+RAM region, +attach-open on data
  processes) pinned dead processes as handle-less zombies. The reaper
  tracks (proc, seg) per exec and CheckedCloses segs of threadless,
  exited, seg-detached processes (safe: `RemoveCodeSeg` already ran).
  Cascade verified live: `reaped 1`, zombie gone from `ps`, seg gone
  from new `segs` census, RAM block recovered. (b) Global `operator
  delete` routed to the kernel heap: our glue `free()` was a no-op while
  `DBase::new` uses `Kern::AllocZ`, so every deleted DObject leaked its
  storage (~2.3K/exec: process+thread+seg+names) while censuses looked
  clean. Range-checked (bump pool never reaches `Kern::Free`; pre-heap
  guarded by `K::Allocator`; verified by `heaptest` alloc/free symmetry
  + new `segs`/`pzomb`/`reap`/`heaptest` verbs). Steady state now:
  repeated execs hold heap ~50.9K/RAM 24063/procs 2 (ekern + latest dead
  pending next-exec reap), zero faults; kill path verified end to end
  (kill spinner -> dead -> reaped -> clean).
  Root-cause trail kept: attach-open (`SetAttachProcess` in
  `DoCreateRam`), seg refcount audit, alloc/free size matching.
- Real DLL step 1 (this session): `tools/mke32.py --dll` builds a pilot
  `EUSER.DLL` (DLL uid, `KImageDll` flag, 2-entry export dir of code
  offsets for Exit/PutC). Kernel `E32Validate` accepts DLL headers and
  checks every export target against code bounds; `E32Run` loads
  `EUSER.DLL` from the EXE's drive once (resident, kernel heap, relocs
  applied) and resolves EXE ordinals from its export table. Missing DLL
  falls back to the old trampolines. `python3 tools/test_dll.py` proves
  DLL-bound imports end to end in QEMU with no fault.
- Program command lines (this session): `exec PROG args...` joins the tail
  into a genuine `HBuf` stored as `DProcess::iCommandLine` (same mechanism
  F32/ESTART use); `args <id>` prints it kernel-side. EUSER ord 3 (CmdLen)
  exposes the genuine `EExecProcessCommandLineLength` exec to user code in
  cdecl form; `tools/mke32.py --args` + `tools/test_args.py` prove the
  round-trip shell -> process -> user thread (11 chars in, full text in
  `args`, 0 when absent). Command lines are stored as UTF-16 bytes in
  the kernel's byte-oriented HBuf; 11 characters correctly read back as
  11 through the length executive. Compiled C programs (`make userland`,
  `tools/elf2e32.py`, `tools/test_userland.py`) retrieve the text
  user-side via the genuine `ProcessCommandLine` TDes8 copy and print
  via `DebugPrint`; `TYPE.E32` reads `C:\...` files through a genuine
  `PortFile` logical channel (open snapshot + chunked read). `CP.E32`
  copies files user-to-user (open-write, chunked append, commit, now
  streaming to the full 64 KiB); shell `TYPE` confirms the bytes.

## 21. Userland maturity — files, directories, heap, IPC, console (RUNNING)

- Programs (`make userland`, 11 E32): `ECHO` (verbatim command tail),
  `TYPE` (file print + EOF), `CP` (streaming copy), `FILECHK` (independent
  handles, deliberate no-close proving auto-cleanup), `IPCSRV`/`IPCCLI`
  (genuine IPC fixtures), `CWD` (inherited + mutable working directory),
  `CRASH` (deliberate UD2 fault), `MALLOC` (allocator self-test),
  `INTERACT` (leased console conversation), `SLEEP` (repeat-sleep probe).
- `PortFile` (`variant/userfiles.cpp`): per-channel 64 KiB snapshots over
  genuine channel executives; handles ID-tracked to creator thread+process;
  overlong paths rejected, never truncated. Per-process CWD is inherited at
  exec and mutable via validated `chdir`/`getcwd` that leave volume globals
  untouched (`CWD.E32`, `test_userland.py`).
- User heap (`user/malloc.c`): first-fit over 16 KiB, alignment, hole reuse,
  OOM-NULL, coalescing (`MALLOC.E32`). Sleep/time/random from genuine
  executives: fresh-RTimer sleeps, `TimeNow`, `MathRandom` (`SLEEP.E32`;
  note: thread-embedded `User::After` hangs on reuse here, so the runtime
  never uses it).
- Genuine kernel IPC (`user/ipc.c`, `docs/symbian_services.md`): servers,
  sessions, integer + 8/16-bit descriptor messages, receive cancel,
  disconnect/shutdown/restart, pending-client `KErrServerTerminated`
  (`test_ipc.py`, `test_conc.py`). One outstanding request per thread; the C
  bridge is transport, not SDK compatibility.
- Console input (`variant/usercon.cpp`, transitional `PortCon` lease, not a
  console server): one program owns the stream, the shell yields while leased
  and reclaims dead leases; `read_line` echoes with backspace + timeout
  (`INTERACT.E32`, `test_interact.py`).
- Exit status: 16-entry ring (name/id/type/reason, orphan path marked) +
  shell `EXITS` verb; `CRASH.E32` proves `Exc 06` attribution with shell
  survival (`test_crash.py`); `kill` refuses the shell's own process.
- Clock: `DATE [YYYY-MM-DD HH:MM:SS]` sets CMOS via the genuine Asic setter
  with calendar validation + readback (`test_cli.py`).
- SDK (`../sdk/`): `newapp.py` scaffolder, space-safe `buildapp.py`, five
  verified example apps, API/limits/IPC docs (`test_sdk.py`).

## 22. Verification & remaining work

- `python3 tools/test_stable.py`: build + 12 suites (relocs, args, DLL,
  data-relocs, userland, IPC, CLI, disk-full, crash, concurrency,
  interactive, SDK). Manual order: `docs/manual_test_order.md`.
- Still out (see `docs/full_os_checklist.md`): per-process memory protection,
  full DLL loading, file/console system servers, active-scheduler dispatch,
  blocking `wait`, networking of any kind, GUI (last).
