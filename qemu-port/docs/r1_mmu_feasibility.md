# R1 M-03 MMU feasibility (read-only, 2026-11-20)

Current state
- Boot: `mmu/mmu_gas.S` builds 1024×4M identity PDEs (U/S=1 <0x80000000, 0 high); `Mmu_Enable()` loads CR3 + CR0.PG|WP + CR4.PGE once at boot (`kernfull_glue.cpp:HwInit0`). No 4K page tables.
- Per-process PD: `mmu/pd.{h,cpp}` 16-row `DProcess*`→{4K PD of 1024 copied 4M PDEs, code/data/stack runs}; `e32load.cpp:805` PdCreate after ProcessCreate, `:1047` PdDestroy in E32Reap. `pd.o` links only into `KERNFULL_OBJS` (Makefile:274), not default `make all` (BRIDGE_ELF).
- CR3 switch wired: `PdInit()` installs naked `PdProcessSwitch` via public `TScheduler::SetProcessHandler` + Vec14 stub; each DThread tagged with PD in `iNThread.iAddressSpace` (e32load.cpp:50,896); genuine `ncsched.cia:181-184` calls the handler on address-space switch, `:175-179` reloads per-thread TSS.esp0.
- #PF contained port-side: `pd.cpp` `PdPageFaultStub/C` decode CR2 + error bit2 → user: `PanicKernExec` (kills process), kernel: `Kern::Fault`; genuine `TPagingExcTrap` (ckernel.cia:331-399) compiled but unused.
- U/S only coarse 4M split; kernel heap/stacks/TSS/trampolines live below 0x80000000 and stay U/S=1 → no real isolation. `DProcess` has no portable PD slot; `execs.txt` has no MMU-control execs.

Needed for full M-03: 4K PD/PTs with per-page U/S (kernel U/S=0; user code/data/stack U/S=1) from a kernel 4K pool; CR3 reload + TSS.esp0 (already done); #PF delivering an EKA2 exception to the owning thread instead of a blanket panic; whitelist/relocate ring-3 trampolines + resident EUSER below 0x80000000.

Feasible without kernelhwsrv/? **YES** — every hook needed is already public there (`SetProcessHandler` nk_priv.h:315, `SetAddressSpace`/`ModifyAttributes` :244, `iAddressSpace` :260, `TheExcVectors`, per-thread ESP0 ncsched.cia:179); single CPU needs no ASID/PCID, full CR3 flush acceptable.

qemu-port files to change: `mmu/mmu_gas.S` (4K PD/PT build + #PF asm), `mmu/pd.{h,cpp}` (4K PT allocator, per-page U/S, fault exception delivery, fixed page pool), `variant/e32load.cpp` (tag threads created after E32Run, whitelist user pages), `variant/qemu_kernel.cpp`/`kernfull_glue.cpp` (install #PF before first user process, serial CR3/fault trace), `Makefile` (link pd.o into default image).
