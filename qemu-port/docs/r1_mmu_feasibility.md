# R1 M-03 MMU feasibility (read-only verification, 2026-11-20)

Current state
- Boot PD: `mmu/mmu_gas.S` builds 1024×4M identity PDEs over 4G; U/S=1 below 0x80000000, U/S=0 high; `Mmu_Enable()` loads CR3 + CR0.PG|WP + CR4.PGE once at boot (`kernfull_glue.cpp:HwInit0:97-102`, `qemu_kernel.cpp:43-51`). No 4K page tables.
- Per-process PD: `mmu/pd.{h,cpp}` 16-row `DProcess*`→{4K-aligned PD of 1024 copied 4M PDEs, code/data/stack runs}; `e32load.cpp:805` PdCreate after ProcessCreate, `:1047` PdDestroy in E32Reap. `pd.o` only in `KERNFULL_OBJS` (Makefile:274), not default `make all` (:399).
- CR3 switch wired: `PdInit()` installs naked `PdProcessSwitch` via public `TScheduler::SetProcessHandler` (nk_priv.h:315) + TheExcVectors[14]; threads tagged via `NThread::SetAddressSpace`/`ModifyAttributes` (e32load.cpp:50-64, nk_priv.h:245,260); genuine `ncsched.cia:181-184` calls handler on AS-switch, `:175-179` sets per-thread TSS.esp0; `TssInitOnce()` (e32load.cpp:32-44) lazily publishes TSS (no LTR).
- #PF port-side: `pd.cpp` `PdPageFaultStub/C` decode CR2+err bit2 → user:`K::PanicKernExec` (kills process), kernel:`Kern::Fault`; genuine `TPagingExcTrap` (ckernel.cia:331-399) compiled but unused.
- Isolation gap: U/S is only a coarse 4M split; kernel heap/stacks/TSS/trampolines live <0x80000000 and stay U/S=1 → no real user/kernel split. `DProcess` has no portable PD slot (kern_priv.h:1592+; only flexible `memmodel.h:162,175` adds iPageDir/iOsAsid); `execs.txt` has no MMU-control execs.

Needed for full M-03: 4K PD/PTs with per-page U/S from a kernel 4K pool; CR3 reload + TSS.esp0 (already done); #PF delivering an EKA2 exception to the owning thread instead of a blanket panic; whitelist/relocate ring-3 trampolines + resident EUSER low memory.

Feasible without kernelhwsrv/? **YES** — every required hook is already public there (SetProcessHandler nk_priv.h:315; SetAddressSpace/ModifyAttributes :245; iAddressSpace :260; TheExcVectors; per-thread ESP0 ncsched.cia:179); single CPU needs no ASID/PCID, full CR3 flush acceptable.

qemu-port files to change: `mmu/mmu_gas.S` (4K PD/PT build + #PF asm), `mmu/pd.{h,cpp}` (4K PT allocator, per-page U/S, fault delivery, fixed pool), `variant/e32load.cpp` (tag late-created threads, whitelist user pages), `variant/qemu_kernel.cpp`/`kernfull_glue.cpp` (install #PF before first user process, trace), `Makefile` (link pd.o into default image).
