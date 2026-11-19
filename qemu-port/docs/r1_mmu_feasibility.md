# R1 M-03 MMU + user isolation — feasibility (read-only, 2026-11-19)

Current state (verified)
- Boot MMU `mmu/mmu_gas.S`: `qemu_page_dir` 4K BSS; `Mmu_BuildIdentityMap()`=32×4M identity PDEs 0x83/0x183 → **U/S=0**, no PTs; `Mmu_Enable()` loads CR3, CR0.PG|WP + CR4.PGE. Enabled once in `variant/qemu_kernel.cpp:41-54` before `KernelMain()`; one flat AS.
- Per-process PD is port-side bookkeeping only: `mmu/pd.{h,cpp}` = 16-row `DProcess*`→{4K PD, code/data/stack runs} side-table (PD=32×4M identity PDEs 0x87, U/S=1 but unused); wired in `e32load.cpp:785` (`PdCreate`)/`:1026` (`PdDestroy`), but `pd.o` links only into `make kernfull` (Makefile:260,271), not default.
- **No CR3 switch / per-proc TSS.esp0**: `%cr3` only in `mmu_gas.S` (boot/status) + console read; no `PdSwitch`. `nk/nk_ctx_gas.S:NThread_Switch` saves CR0/fs/gs/regs/EIP only; genuine `nkern/x86/ncsched.cia:178-179` writes TSS.iEsp0 but same fixed `sTssStack` value via `e32load.cpp:TssInitOnce()`. CR3 reload exists only in unbuilt `memmodel/epoc/multiple/x86/xsched.cia:36-37` (`KERN_MEMMODEL=direct`).
- **No #PF containment**: `boot/ported/bootcpu_gas.S` Vec14→`VectorCommon` prints "bootstrap FAULT vector=0xE" + `hlt` (whole box; no CR2/error decode). Genuine `TPagingExcTrap` (`kernel/x86/ckernel.cia:331-399`) is compiled in but never wired to a #PF vector; `X86::Init1Interrupts` is bypassed.
- **No U/S enforcement**: all RAM U/S=0; multiple-model split (`multiple/x86/mmboot.h`: 0x00400000 user, 0x80000000 ROM, 0x90000000–0xA0000000 user-global, PD 0xC1000000) not applied. `DProcess` (`kern_priv.h:1706-1708`) has `iCodeSeg`/`iDataBssRunAddress`/`iDataBssStackChunk` but no portable PD slot (only flexible model adds `iPageDir`/`iOsAsid`) → hence side-table. `execs.txt` has no MMU-control execs (only chunk/memory-info queries).

Verdict
- PD allocation/ownership is already port-side; full enforcement is **not a small patch** but **feasible without kernelhwsrv edits**: intercept the real ring-3 reschedule path (machine-coded in read-only `ncsched.cia`), override IDT Vec14, and rebuild 4K PTs from qemu-port.
- Single CPU ⇒ no ASID/PCID needed (full CR3-flush OK); today a bad user pointer halts the whole box.

Work remaining (all qemu-port/)
1. `mmu/pd.cpp`: 4K PTs with U/S=1 user / U/S=0 kernel, copy kernel mappings, add `PdMapUser`/`PdSwitch`.
2. Switch path (`nk_ctx_gas.S` or genuine-reschedule hook): reload CR3 from `PdPhys(incoming DProcess)` + set per-process `TSS.iEsp0`.
3. `boot/ported/bootcpu_gas.S`: dedicated Vec14 stub (CR2 + error code) → kill owning DThread on user CR2, `Kern::Panic` on kernel CR2.
4. `variant/qemu_kernel.cpp`/`kernfull_glue.cpp`: install #PF before first user process, link `pd.o` into default make, add CR3/fault trace; `e32load.cpp` seeding/teardown already partly done.
