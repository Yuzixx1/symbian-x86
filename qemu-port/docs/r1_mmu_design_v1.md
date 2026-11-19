# R1 M-03 design v1 — MMU + user memory isolation (host-side design, no kernel build)

Scope: design only. `kernelhwsrv/` is READ-ON-LY; no `make`/QEMU run; no source files changed.

## Done criteria (from deleted `full_os_checklist.md`, recovered at 1e67072 §3/M-03)
MMU on; page tables per process; user/kernel split enforced; existing E32/TYPE/CP/IPC
tests still pass; a bad user pointer faults **the process, not the kernel**.

## Current state (verified)
- `mmu/mmu_gas.S`: `qemu_page_dir` (4K BSS, 1024 PDEs), `Mmu_BuildIdentityMap()`
  writes 32 × 4 MiB identity PDEs `P|W|PS`(+`G`) covering 0–128 MiB, all U/S=1,
  no PTs; `Mmu_Enable()` loads CR3, sets `CR0.PG|WP`, `CR4.PGE`. One flat AS.
- `mmu/mmu.cpp` / `variant/qemu_kernel.cpp`: BuildIdentityMap → Enable → TouchTest
  happens once at boot before `KernelMain()`; never revisited.
- `rom/rom_image.cpp`: `SuperPage.iPageDir = (TUint)qemu_page_dir` (boot-info only).
- `nk/nk_sched.cpp` + `nk/nk_ctx_gas.S`: `NThread_Switch` saves CR0/fs/gs/EIP in the
  genuine `SThreadStack` order but **no CR3, no TSS.esp0 switch** (documented
  "single shared address space"). User DThreads actually run on the genuine
  scheduler (ProcessCreate/Resume from `variant/e32load.cpp`), not this harness.
- `variant/e32load.cpp: TssInitOnce()` lazily publishes one bootstrap `qemu_tss`
  with fixed `iSs0=0x10`, `iEsp0=sTssStack+4096`, `X86_TSS_Ptr=tss` (no LTR;
  `__lidt` already loaded TR 0x28). Same ESP0 for every process today.
- Genuine reference exists but is NOT built by this port:
  `kernelhwsrv/.../memmodel/epoc/multiple/x86/xmmu.cpp` (`X86Mmu::Init1`,
  `NewPageDirectory`, `InitPageDirectory`, `ApplyPagePermissions`, 4K pages,
  1024 ASIDs, separate global PD), `xkernel.cpp` (`TIpcExcTrap::ExcLocation`
  decodes `EX86VectorPageFault`), `x86/mmboot.h` (user local 0x00400000,
  user shared end 0x80000000, ROM 0x80000000, user global 0x90000000–0xA0000000,
  PD base 0xC1000000). `DProcess` has `iCodeSeg`/`iDataBssRunAddress`/
  `iDataBssStackChunk` (`kern_priv.h:1706-1708`) but no portable PD slot in the
  base class; the flexible memmodel adds `TPhysAddr iPageDir` + `iOsAsid`
  (`memmodel/epoc/flexible/memmodel.h:162,175`).

## Target design
1. **Per-process PD, 4K pages.** Keep `qemu_page_dir` as the single kernel/boot PD;
   add `pd.cpp` to build one 4 KiB PD + on-demand 4 KiB PTs per process. Replace the
   32 identity 4M PDEs with 4K PTEs so U/S can be set per page (kernel and user both
   currently live in low RAM, so a pure address-range rule is insufficient: kernel
   RAM/dll/trampoline pages must stay U/S=0 even below the split).
2. **Split + U/S.** Coarse policy: linear < 0x80000000 = user range (matches E32 run
   addresses); ≥ 0x80000000 = kernel/ROM/global. Set U/S=1 only on pages mapped for
   the owning process (code, data/bss/stack, heap, resident EUSER code), U/S=0 for
   kernel heap/stack/TSS/IDT/PD/PT and trampolines. Keep `CR0.WP=1`.
3. **ASID-free switching.** Single CPU: no PCID/ASID allocation; on every context
   switch reload CR3 from the incoming process's PD (full non-global TLB flush is
   acceptable), and write per-process `TSS.iEsp0` before entering ring 3. Extend
   `NThread_Switch` (or the genuine switch path) with a `SwitchAddressSpace(DProcess*)`
   hook that does both; re-run `TssInitOnce()` as the one-time TSS owner.
4. **#PF containment.** `#PF` vector (14) decodes CR2 + error code. If CR2 is in the
   user range and the fault is not-present/user-mode → deliver an EKA2 exception to
   the **owning DThread only** (existing `DThread::IpcExcHandler`/`K::Exc` path;
   `Die(EExitKill, KErrFault...)`), then reschedule; if CR2 is in the kernel range,
   or the access is supervisor-originated on a kernel page → `Kern::Panic`/`FAULT:`
   (kernel bug, never kill the whole machine for a user page).
5. **Ownership side-table (read-only constraint).** Cannot edit `kern_priv.h`, so
   store `DProcess* → {PD phys, PT list, ESP0}` in a port-side table (same pattern as
   `sExecs`/`ProcExt` in `e32load.cpp`). Seed at `E32Run` after `ProcessCreate`,
   tear down in `E32Reap` when the process dies; `iCodeSeg`/`iDataBssRunAddress`
   supply the code/data mappings to install.

## Allocation strategy
- Page-aligned chunks: `Kern::AllocZ(4096)` / existing hybrid heap with a 4096-alignment
  wrapper; reject sub-page fragments. A dedicated fixed page pool (carved from the
  same low RAM as the current boot heap) is the fallback if heap fragmentation bites.
  M-05 is only PARTIAL (16 KiB static `user/malloc.c` pool), M-04 chunks are DONE;
  page-table allocation must come from the **kernel** heap/pool, not the user heap.
  (Correction to the premise: M-05 is not "done".)

## Bootstrap sequence
1. Boot: build boot PD identity map with 4K PTEs, kernel pages U/S=0; enable paging
   (unchanged `QemuKernelMain` order), install #PF handler (currently absent).
2. `KernelMain()` / first `ProcessCreate`: create process PD = copy of kernel PDE/
   PTE entries (global kernel mappings), map user code/data/stack with U/S=1.
3. First thread resume: `SwitchAddressSpace(proc)` → CR3 reload + TSS.esp0.
4. Each later context switch: same hook; kill path frees PD/PTs and drops side-table row.

## QEMU test plan (host-side; no code run here)
- `tools/test_crash.py` style: user E32 reads a kernel address (e.g. 0xC1000000) →
  #PF → `exit: type=... prog=CRASH.E32`, shell/sibling processes survive.
- Kernel-side intentional bad access to a user page marked supervisor → `FAULT:` panic
  with serial trace (negative control; requires a test hook, not shipped).
- Valid user code/data/stack/heap access + existing TYPE/CP/IPC/CHUNK regressions stay
  green; `ps`, `kill`, reaper still correct across ≥2 concurrent processes.
- Verify serial `CR3=` changes across `exec` and that `Exc` names the owning program.

## Concrete next steps (order)
1. `mmu/mmu_gas.S`: add 4K `Mmu_BuildKernelPd()` + `Mmu_Switch(TUint aPdPhys)` and a
   `#PF` asm stub; keep `Mmu_Enable()` as-is.
2. New `mmu/pd.cpp`: process PD/PT allocator + side-table API (`PdCreate/PdMapUser/
   PdSwitch/PdDestroy`) using `Kern::AllocZ(4096)`; wire into Makefile source list.
3. `variant/e32load.cpp`: call `PdCreate/PdMapUser` for code/data from
   `pinfo.iCodeRunAddress` / `iDataRunAddress`; call `PdDestroy` in `E32Reap`.
4. `nk/nk_ctx_gas.S` + `nk/nk_sched.h`: add CR3 reload hook and per-thread ESP0 arg
   (genuine switch path hook if user DThreads are scheduled there instead).
5. `variant/qemu_kernel.cpp`: install #PF handler before first user process; add
   serial CR3/fault diagnostics. Update README §8 + `test_stable.py`/manual order.
