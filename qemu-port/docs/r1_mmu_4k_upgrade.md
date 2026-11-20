# OS-CORE upgrade — 4 KiB paging + per-page U/S (2026-11-20)

## Design chosen (shipped, passing)
- `mmu/mmu_gas.S`: `qemu_page_dir` is now a 4K directory. `PDE[0..31]` point at
  32 STATIC 4K-aligned identity page tables (`qemu_page_tables`, 128 KiB BSS)
  covering 0x00000000..0x07FFFFFF — all 128 MiB of QEMU RAM (kernel image,
  kernel heap/data at 0x1000000, E32 user code/data at 0x2200000, EUSER BSS).
  `PDE[32..1023]` remain coarse 4M identity PDEs `P|RW|PS|G` with U/S=0 for
  high MMIO/APIC. `Mmu_Enable()` unchanged (CR3 + CR0.PG|WP + CR4.PGE).
- `mmu/pd.{h,cpp}`: 16-row `DProcess*`→PD side-table. Each process PD is a 4K
  page from a static 4K-aligned pool (`sPdPages`, 16 pages) whose PDE[0..31]
  point at the SHARED PTs; PDE[32+] are the 4M fallback. Pool caps: **32 PT
  pages (128 MiB) + 16 PD pages = 48 pages / 192 KiB** — no dynamic allocation,
  cannot fail after `PdAvail()`. Per-page U/S is global across processes.
- U/S policy: PTEs start U/S=1 (old flat low-RAM behaviour → zero regression).
  `PdInit()` → `LockKernelPages()` clears U/S on known kernel-only pages:
  kernel text+rodata 0x100000..0x18c000 (the 0x18c000 trampoline page is
  excluded — ring 3 calls `sTrampExit`/`sTrampPutc`), kernel heap/data
  0x1000000..0x1200000, the PT pool, boot PD page and TSS page. EUSER.DLL code
  in kernel BSS (0x2f3908..) stays U/S=1. `PdMapUser`/`PdMapData` re-assert
  U/S=1 on E32Run code/data/stack; `PdMapKernel`/`PdMapKernelRange` clear it.
  Ranges ≥128 MiB fall back to flipping U/S on the coarse 4M PDE.
- `#PF` (Vec14) decodes CR2+error, resolves pid from CR3 via the side-table,
  prints `PD: #PF pid=... cr2=... err=... user=... inUserRange=... cr3=...`,
  then kills only the current thread (`K::PanicKernExec`) for user faults and
  keeps `Kern::Fault` for kernel faults. Full EKA2 trap delivery (genuine
  `TPagingExcTrap`, ckernel.cia:331-399) is out of scope: that class is not
  linked into this image and reaching it from the raw IDT stub would require
  kernelhwsrv edits (read-only) plus a live `DThread` walk in fault context.
- `PdProcessSwitch` (via public `TScheduler::SetProcessHandler`) still reloads
  CR3 from `NThread.iAddressSpace`, now a real 4K PD phys from `PdCreate`.

## Serial proof (r1_qemu_smoke.log)
- `PD: boot CR3=001C8000 CR0=80010011 PDE0=001C9027 PDEhi=FB800183
  KPTE=00100023 UPTE=02000007 supPtes=000002AF`
  - `PDE0=...27`: PT phys | P|RW|US|accessed, **PS=0 → 4K, not 4M 0x87**.
  - `PDEhi=...83`: 4M fallback, **U/S=0** (APIC/MMIO ring-0 only).
  - `KPTE=...23`: kernel text page phys 0x100000, **U/S=0**.
  - `UPTE=...07`: user run page phys 0x2000000, **U/S=1**.
  - `supPtes=0x2AF` (687) supervisor-only PTEs.
- `PD: create proc=... CR3=39a000 PDE[0]=1c9007 PDE[8]=1d1007 (4K PTs, not 4M)`
  proves the switched-to per-process PD also uses 4K PTs.
- `PD: PdMapUser proc=... v=2201000 ... -> U/S=1 (4K)` etc.

## Verification
`make build/kernfull.elf` OK; `r1_qemu_smoke.py` 11/11 PASS;
`r1_cp4_ca_test.py` PASS (23 markers); `r1_calc_test.py` PASS;
`r1_wbrowser_test.py` PASS. No `Exc`/`PanicKernExec` in any log.
`kernelhwsrv/` untouched; no `make clean`; no other workers' files touched.

## What remains
- U/S is global, not per-process (shared PTs): processes still see each other's
  user pages; true per-process isolation needs private PT copies per PD.
- Only *known* kernel regions are demoted; unrecognised kernel BSS pages
  (including EUSER resident data and part of the bootstrap heap) stay U/S=1.
  Full opt-in (U/S=0 default) needs the user heap/stack/DLL data addresses
  plumbed from `DMemModelProcess` into `PdCreate`.
- No demand paging / no EKA2 #PF exception delivery to the owning thread.
- Grow >128 MiB RAM requires enlarging the 32-PT static pool.
