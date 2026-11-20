# 4 KiB paging, per-page U/S and per-process PDs (R4/R5)

Sources read for this document: `docs/r1_mmu_4k_upgrade.md`,
`docs/r1_mmu_feasibility.md`, `docs/r1_mmu_design_v1.md`,
`mmu/mmu_gas.S`, `mmu/pd.{h,cpp}`, `variant/kernfull_glue.cpp`,
`variant/e32load.cpp`, `boot/ported/bootcpu_gas.S` and the genuine
`../kernelhwsrv/kernel/eka/nkern/x86/ncsched.cia` /
`kernel/x86/ckernel{,.cia}`. `kernelhwsrv/` is READ-ONLY; no source under it
was modified. `kh/` below abbreviates `../kernelhwsrv/kernel/eka/`.

## 1. Before the upgrade: flat 4G / 4M identity map

The original bridge path built a single 4 MiB-page identity map over the
whole 4G space: `variant/qemu_kernel.cpp:41-54` calls `QemuMmu::BuildIdentityMap`
/ `Enable` and logs `"building flat 4G identity map"`; the C++ shim is
`mmu/mmu.cpp:8-27`. `docs/r1_mmu_feasibility.md:4` records the exact
pre-upgrade state: `mmu/mmu_gas.S` built 1024x4M identity PDEs over 4G,
U/S=1 below 0x80000000 and U/S=0 high, with `Mmu_Enable()` loading CR3,
`CR0.PG|WP` and `CR4.PGE` once. That coarse split left the kernel heap,
stacks, TSS and trampolines all U/S=1, so there was no real user/kernel
boundary (`r1_mmu_feasibility.md:8`).

The upgraded image replaces that map with a real 4K directory. In the full
`kernfull` boot, `HwInit0` (`variant/kernfull_glue.cpp:93-133`) calls:

1. `QemuUart::Init()` (`:95`) — COM1 is the only output before paging.
2. `Mmu_BuildIdentityMap()` then `Mmu_Enable()` (`:101-106`) — builds the
   4K PD/PT identity map and turns paging on **before** the kernel heap,
   ROM structures or any driver constructor are touched. The serial proof
   `QEMU-HwInit0: paging ON` is emitted here.
3. `BootInfo::Capture()` and the ramdisk snapshot (`:108-112`) — before the
   boot-time `memclr` can recycle the low pages GRUB used for the module.
4. `PdInit()` (`:114`) — demotes kernel pages to supervisor, installs the
   `#PF` vector and the CR3-switch hook.
5. `QemuRomInit()`, the kernel-region clear, `X86::TheAsic` publication and
   the `__init_array` constructor walk (`:116-132`).

## 2. Boot page tables: 1024 PDEs, 32 static 4K PTs, 4M fallback

`mmu/mmu_gas.S:37-48` defines two 4K-aligned BSS objects:
`qemu_page_dir` (4096 bytes = 1024 PDEs) and `qemu_page_tables`
(`32 * 4096` = 32 pages / 128 KiB of PTEs). `Mmu_BuildIdentityMap`
(`:56-126`) zeroes both, fills each PT with 32768 identity PTEs
`P|RW|US` (`:79-97`), then builds the directory (`:98-120`):

- `PDE[0..31]`: address of the corresponding 4K PT with `P|RW|US`, **PS=0**
  (`:108-112`). These cover `0x00000000..0x07FFFFFF`, the full 128 MiB
  QEMU RAM (kernel image, heap/data at 0x1000000, E32 user code/data at
  0x2200000, EUSER BSS).
- `PDE[32..1023]`: coarse 4M identity PDE `(index<<22) | P|RW|PS|G` with
  **U/S=0** (`:99-104`, `:113-116`) for high MMIO/APIC. `G` (bit 8) is
  added only when CPUID reports PGE (`EX86Feat_PGE=0x2000`, `:100-104`).

`Mmu_Enable` (`:130-149`) loads CR3 with `qemu_page_dir`, sets `CR0.PG|WP`,
then sets `CR4.PGE` and reloads CR3 to flush non-global TLB entries;
`qemu_mmu_on=1` on success. EIP needs no far jump because the map is
identity. `Mmu_Status` (`:152-158`) exposes CR0/CR3/CR4 to serial diagnostics.
`qemu_cpu_features` was filled earlier by `InitCpu`
(`boot/ported/bootcpu_gas.S:150-200`); the bootstrap GDT/IDT are set up by
`SetupGDT`/`SetupIDT` (`:219-287`, `:397+`).

## 3. Per-process page directories (16 PDs)

Because the genuine Direct memory model has no portable page-directory slot
on `DProcess` (only the flexible memmodel adds `iPageDir/iOsAsid`),
ownership lives in a port-side side-table keyed by `DProcess*`
(`mmu/pd.h:1-14`, mirroring the `sExecs`/`ProcExt` pattern in
`variant/e32load.cpp`):

- `sProcs[16]` rows of `{DProcess*, iPdPhys, code/data/stack runs}`
  (`mmu/pd.cpp:31-49`). `sPdPages[16][1024]` is a 4K-aligned static pool;
  `KMaxPdProcs=16`, `KPdEntries=1024`, `KNumPt=32` (`:31-35`).
- `FillPd` (`:51-64`) gives each process PD the same shared PT pointers in
  PDE[0..31] and the same 4M U/S=0 fallback in PDE[32+], so CR3 loading
  works from the first instruction. `PdAvail` (`:133-139`) pre-flight,
  `PdCreate` (`:141-169`) fills a row, logs
  `PD: create proc=… PDE[0]=… PDE[8]=… (4K PTs, not 4M)` and re-asserts
  U/S=1 on the code/data/stack runs via `PdMapUser`. `PdPhys` (`:171-177`)
  returns the CR3 value; `PdDestroy` (`:179-201`) zeroes the PD unless it is
  still loaded in CR3 (E32Reap can run with a dying process's PD current).
- Pool caps: **32 PT pages + 16 PD pages = 48 pages / 192 KiB**, entirely
  static BSS. No dynamic allocation: after `PdAvail()` the pool cannot fail
  (`docs/r1_mmu_4k_upgrade.md:10-14`).
- `E32Run` calls `PdCreate` after `Kern::ProcessCreate` and before `Resume`
  (`variant/e32load.cpp:802-809`); `TagProcessAddressSpace` (`:50-64`)
  stamps every thread with `NThread::SetAddressSpace(pd)` and
  `KThreadAttAddressSpace` under the system lock (`:895-898`).

CR3 reloading is genuinely scheduler-driven: `PdInit` installs the naked
`PdProcessSwitch` through the public `TScheduler::SetProcessHandler`
(`mmu/pd.cpp:208-217`, `:308`). It reads `NThread.iAddressSpace` and does
`mov cr3, eax` (0 keeps the current PD). The genuine
`kh/nkern/x86/ncsched.cia:181-184` tests the address-space bit and calls
`iProcessHandler` with `EBX=new NThread`, `EDI=scheduler`; the same path
sets per-thread `TSS.esp0` from `iStackBase+iStackSize` at `:175-179`.
`TssInitOnce` (`variant/e32load.cpp:32-44`) publishes `X86_TSS_Ptr` because
`X86::Init1Interrupts` is not linked. One CPU means no ASID/PCID and a full
TLB flush per switch is acceptable.

## 4. U/S policy: supervisor demotion, user re-assertion

All PTEs start U/S=1 to preserve the old flat low-RAM behaviour
(`mmu_gas.S:53-55`). `PdInit()` then calls `LockKernelPages`
(`mmu/pd.cpp:119-130`) which clears bit 2 on:

- kernel text+rodata `0x100000..0x18c000` — deliberately stopping before
  the 0x18c000 trampoline page, which ring 3 calls (`sTrampExit`/
  `sTrampPutc`, `variant/e32load.cpp:71-84`);
- kernel heap/data `0x1000000..0x1200000` (2 MiB);
- the whole 32-page PT pool, the boot PD page and the TSS page.

`SetUsRange` (`:69-107`) walks 4K pages, flips `qemu_page_tables[pdi][pti]`
bit 2, and for >=128 MiB falls back to flipping the coarse 4M PDE in the
boot PD and every live process PD (`:88-101`), then reloads CR3 to flush
non-global TLB entries (`:103-107`). `PdMapUser`/`PdMapData` re-assert
U/S=1 for the E32 runs at 4K granularity (`:355-371`), while
`PdMapKernel`/`PdMapKernelRange` clear it (`:373-378`, `:352-353`).
`CountSupervisorPtes` (`:109-117`) counts supervisor PTEs for the boot log.

## 5. #PF stub and containment

`PdInit` overwrites `TheExcVectors[14]` with `PdPageFaultStub`
(`mmu/pd.cpp:307`) before `X86::Init1Interrupts` builds the IDT. The naked
stub (`:224-245`) does `cld`, saves DS/ES/EAX/EDX, reads CR2 into EAX and
the CPU-pushed error code into EDX, switches to kernel data segments and
calls `PdPageFaultC(cr2, err)` (`:238`), then `iretd`.

`PdPageFaultC` (`:273-292`) decodes the U/S bit (`err & 4`), reads CR3,
resolves the owning PID from the side-table (`PdOwnerPid`, `:249-255`),
tests whether CR2 lies in the recorded code/data/stack runs
(`PdAddrInUserRange`, `:257-271`), and prints:

```
PD: #PF pid=… cr2=0x… err=0x… user=… inUserRange=… cr3=0x…
```

User faults call `K::PanicKernExec` (kills only the current thread, process
dies, machine survives); kernel faults call `Kern::Fault("PD-KernelPF", …)`.
Full EKA2 exception delivery via `TPagingExcTrap`
(`kh/kernel/x86/ckernel.cia:331-399`) is out of scope: that class is not
linked into this image, and reaching it from the raw IDT stub would need
kernelhwsrv edits plus a live `DThread` walk in fault context
(`mmu/pd.cpp:281-289`). The genuine dispatcher that would otherwise map
vector 14 to `EExcPageFault` and deliver to `iExcTrap`/reentry is
`kh/kernel/x86/ckernel.cpp:131-189`.

## 6. Boot proof (serial log)

`docs/r1_mmu_4k_upgrade.md:33-43` records the observed line:

```
PD: boot CR3=001C8000 CR0=80010011 PDE0=001C9027 PDEhi=FB800183
    KPTE=00100023 UPTE=02000007 supPtes=000002AF
```

interpretation (`:34-40`):

- `PDE0=…27`: PT phys | P|RW|US|accessed with **PS=0** — a 4K PT pointer,
  not the old 4M `0x87` PDE.
- `PDEhi=…83`: 4M fallback with **U/S=0** (APIC/MMIO ring-0 only).
- `KPTE=…23`: kernel text page at phys 0x100000, **U/S=0**.
- `UPTE=…07`: user run page at 0x2000000, **U/S=1**.
- `supPtes=0x2AF` (687) supervisor-only PTEs in the 32 shared PTs.

`PdDump4K` (`mmu/pd.cpp:317-334`) emits this via direct UART because
`KPrintf` is not yet wired at `HwInit0` time. Further proof lines are
`PD: create proc=… (4K PTs, not 4M)` (`:164-165`) and
`PD: PdMapUser proc=… -> U/S=1 (4K)` (`:361-362`), showing the switched-to
per-process PD also uses 4K PTs.

## 7. Verification, caps and remaining gaps

Verification per `docs/r1_mmu_4k_upgrade.md:45-49`:
`make build/kernfull.elf` OK; `r1_qemu_smoke.py` 11/11 PASS;
`r1_cp4_ca_test.py` PASS (23 markers); `r1_calc_test.py` and
`r1_wbrowser_test.py` PASS; no `Exc`/`PanicKernExec` in any log.
`kernelhwsrv/` untouched; no `make clean`.

Caps and gaps (`docs/r1_mmu_4k_upgrade.md:51-59`):

- U/S is global because all process PDs share the same 32 PTs — processes
  still see each other's user pages. True per-process isolation needs
  private PT copies per PD.
- Only *known* kernel ranges are demoted; unrecognised kernel BSS (EUSER
  resident data, part of the bootstrap heap) stays U/S=1. Full opt-in
  (U/S=0 default) needs user heap/stack/DLL data addresses plumbed from
  `DMemModelProcess` into `PdCreate`.
- No demand paging and no EKA2 #PF exception delivery to the owning thread;
  user faults terminate the faulting thread.
- 16 process PDs and 128 MiB of 4K-mapped RAM are hard static limits;
  growing past 128 MiB requires enlarging the 32-PT pool (the 4M fallback
  still identity-maps the high range, supervisor-only).
