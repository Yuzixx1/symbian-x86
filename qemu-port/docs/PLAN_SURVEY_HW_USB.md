# PLAN SURVEY — real-hardware boot + USB readiness of the x86/PC glue

Scope: read-only survey of `qemu-port/` (boot chain, `variant/` drivers, `console/`,
`inc/qemu_config.h`, `tools/mkiso.py` / `tools/mkgrubiso.py`, Makefile ISO/QEMU
targets). `kernelhwsrv/` was not modified. Nothing committed or pushed; no QEMU
runs; `make clean` not run. Goal: what it would take to boot the ISO on a
physical PC and use USB devices, with QEMU-only CI remaining primary.

## Current capabilities

- **Boot paths (3, all El Torito hybrid ISOs):**
  - `tools/mkiso.py` — GRUB-free standalone ISO: hand-written `boot/bootsect.S`
    (A20 via 0x92 + 8042 fallback, E820, EDD streaming) loads `build/multiboot.elf`;
    `isofull` variant carries `kernfull.elf`. Also ships `build/efi/BOOTX64.EFI`.
  - `tools/mkgrubiso.py` (`make isogrub`) — GRUB2 combo ISO (BIOS El Torito
    i386-pc + UEFI x86_64-efi in one image). Kernels carry a valid Multiboot1
    header, so GRUB's `multiboot` command honors the exact `_mb_entry` contract
    (EAX=0x2BADB002, EBX=MBI, PM, paging off, A20 on). `/programs.img` rides as a
    Multiboot module -> RAM disk dev 8 (X:). This is the most firmware-agnostic
    path: GRUB's own disk drivers replace the hand-written EDD path that hangs
    VirtualBox BIOS on the 2nd read (noted in `mkgrubiso.py`).
  - `boot/multiboot.S` bridge validates the Multiboot1 info and hands off to the
    ported `ResetEntry` via a magic page at 0x9000.
- **PCI:** `variant/qemu_pci.cpp` — generic 0xCF8/0xCFC config cycles, bus 0 scan,
  `PciFindClass`, `PciList` shell command. Real-portable, but **bus 0 only, no
  PCI-PCI bridge walk, no PCIe ECAM (0x MMCONFIG)**.
- **Storage:** `qemu_ata.cpp` legacy IDE PIO (0x1F0/0x170), `qemu_ahci.cpp` SATA
  ports 0-3, `qemu_virtio_blk.cpp` (legacy 1001 + modern 1040..107F, cap-list
  walk), plus multiboot-module RAM disk. FAT32 mounted from AHCI dev 4 part 0
  (partlba hard-coded 2048). **No NVMe.**
- **Network:** `qemu_nic.cpp` e1000 (8086:100E) and `qemu_virtio_net.cpp`
  (1AF4:1000/1041) — both polled descriptor rings, no IRQ/MSI-X.
- **Audio:** `qemu_audio.cpp` AC'97 (PCI 04.01, 82801AA) PCM-OUT DMA, unified
  BDL engine + 8-slot queue, exec 240 for userland. Works on any ICH AC'97.
- **USB:** front-end `qemu_usb.cpp` (UHCI 8086:7020, polled TD/QH) with three
  backends: `qemu_usb_ohci.cpp`, `qemu_usb_ehci.cpp` (ICH9 8086:293A), and
  `qemu_xhci.cpp` (root-port only). All polled, identity-mapped DMA. HID boot
  keyboard + mouse only.
- **Input:** PS/2 keyboard `console/kbd.cpp` (IRQ1, 8042 Set-1) and PS/2 mouse
  `variant/qemu_mouse.cpp` (IRQ12 via 8042 aux, VBE crosshair). USB HID feeds
  the same shell ring via `QemuKbdPushUsb`.
- **Display:** `qemu_display.cpp` Bochs VBE DISPI (0x1CE/0x1CF) 1024x768x32 LFB
  (mode 0x118), fallback VGA mode 13h; text console at 0xB8000. LFB base read
  from VGA PCI BAR (default 0xE0000000). No real-mode thunk needed.
- **Interrupts:** i8259 PIC only, vectors 0x30+, 16-slot dispatcher
  (`qemu_variant_int.cpp`); bound: IRQ0 PIT tick, IRQ1 kbd, IRQ4 COM1, IRQ12
  mouse. **No local APIC, no IO-APIC, no MSI/MSI-X, no ACPI anywhere.**
- **QEMU CI stays primary:** `make qemu-cdrom/qemu-cdfull` boot the ISOs under
  `qemu-system-i386` with `-m 128`; ram size hard-coded 128M in
  `inc/qemu_config.h` (`QEMU_RAM_SIZE`).

## Gaps for real hardware

- **No ACPI at all** — no table parsing, no power management/shutdown, no SMP,
  no PCIe ECAM. PIC-only INTx works on legacy boards via BIOS PIRQ routing but
  is fragile on modern boards; no IO-APIC means >16 IRQs / shared INTx is the
  only option.
- **PCI enumeration is bus-0-only** — real machines put devices behind
  PCI-PCI bridges (bus > 0); NIC/audio/USB controllers on such buses are
  invisible. Needs recursive bridge walk + MMCONFIG for PCIe.
- **Boot medium risk:** the standalone `bootsect.S` EDD path is known to hang
  at least one real BIOS (VirtualBox). The GRUB ISO is the safer real-hardware
  path (GRUB loads the kernel itself); USB-stick `dd` of the ISO is an
  unverified boot path (no isohybrid, per `docs/hw_worksheet.md`). CD-R burn
  is the low-risk first medium.
- **Hard-coded QEMU assumptions:** 128M RAM size, identity map assumptions,
  `QEMU_VGA_HOLE` 0xA0000-0xFFFFF, COM1-only debug, PIT 1ms tick, CMOS RTC.
  All fine on real x86, but the memory map is assumed, not probed beyond E820.
- **Storage on real HW:** IDE/AHCI PIO works on most boards; SATA mode must be
  IDE-compatible/AHCI; no NVMe means modern boards with only NVMe storage
  cannot host C:. Kernel hard-codes partition LBA 2048.
- **Display:** Bochs VBE DISPI works on any VBE2+ BIOS card; mode 0x118
  (1024x768) is near-universal. Text mode is safe everywhere.

## Gaps for USB devices

- **HID-only.** No USB mass storage, no hubs, no hotplug, no composite devices,
  no USB mouse on xHCI path (keyboard only there). xHCI covers direct root-port
  connections only (no route strings / downstream hubs).
- **All USB controllers polled** — fine for HID, but every backend re-implements
  its own PCI scan on bus 0; on real HW the xHCI controller may sit behind a
  bridge and be missed entirely (same bus-0 limitation).
- **No IRQ-driven USB** — `UsbPoll()` runs in the shell loop; no Interrupt::Bind
  for USB INTR. Acceptable for input; a real USB stack needs interrupt/ISR work.
- **No USB boot support in-kernel** — booting *from* a USB stick relies on BIOS
  El Torito/USB-HDD emulation, not on our drivers.

## Risk areas with frozen kernel hooks

- `kernelhwsrv/` is a separate read-only repo: genuine `Interrupt` class,
  `X86::Init1Interrupts` (IDT/TSS), `Asic::Init1/Init3`, `NTimerQ::Init1`,
  `ResetEntry` (`bootmain_gas.S`) are frozen. Any APIC/MSI/ACPI work must go in
  `variant/` (as the PIC dispatcher already does) or require unlocking
  kernelhwsrv.
- **Boot contract** (`boot/multiboot.S` + `_mb_entry` + 0x9000 handoff page) is
  load-bearing for both QEMU `-kernel` and GRUB `multiboot`; changes risk
  breaking the primary CI path.
- **`HwInit0` `__init_array` walk** — every driver self-registers via static
  constructors; adding real-HW drivers must stay idempotent and must not extend
  boot time on the QEMU path.
- **`Asic::Init3` mount order** (AHCI dev 4 part 0 as C:, module dev 8 as X:)
  and `partlba=2048` are baked into `tools/mkgrubiso.py`/MBR layout — changing
  disk geometry touches the ISO tooling too.
- **PIC shadow-mask pitfall** (documented in `qemu_mouse.cpp`): `Interrupt::Enable`
  re-masks live IRQs; new IRQ bindings must use `QemuIrq::Enable` direct RMW.
- `QEMU_RAM_SIZE`/heap sizing (`CFG_MinimiseKernelHeap`) tuned for `-m 128`;
  real PCs with more RAM are fine, but anything assuming 128M upper bound is a
  hook to respect.

## Feasibility verdict

**Boot on a physical PC: feasible with low risk via the GRUB combo ISO
(`build/symbian-grub.iso`) burned to CD-R** — GRUB handles loading, the
multiboot contract is unchanged, and text-mode console + PS/2 keyboard + IDE/AHCI
+ e1000 + AC'97 + VBE are all standard PC hardware the drivers already speak.
Main real-HW risks are bus-0-only PCI enumeration (devices behind bridges are
missed) and no ACPI/APIC (modern boards, power management, >16 IRQs). **USB
devices: feasible for HID keyboards/mice today** (UHCI/OHCI/EHCI/xHCI backends
exist and are polled); **USB storage/hubs/hotplug are not implemented** and
would be new variant-owned drivers. QEMU-only CI remains the primary gate; real
HW is a secondary smoke path (see `docs/hw_worksheet.md` for the non-destructive
policy).

## First 3 concrete steps

1. Burn `build/symbian-grub.iso` to a CD-R (per `docs/hw_worksheet.md` policy:
   build PC only, never near the reference PC's data disk) and boot the
   Pentium-4-era reference machine; record PASS/FAIL/HANG per worksheet fields
   for GRUB menu, multiboot handoff, shell prompt, `pci` list.
2. Add a recursive PCI bridge walk (bus > 0) plus PCIe ECAM (MMCONFIG) fallback
   in `variant/qemu_pci.cpp` — pure variant-side change, no kernelhwsrv touch —
   so controllers behind bridges on real boards become visible to all drivers.
3. Add a polled USB mass-storage class driver on top of the existing UHCI/OHCI/
   EHCI/xHCI front-end in `variant/qemu_usb.cpp` (BOT/CBW + SCSI read10), so a
   USB stick can be mounted as a FAT volume — the single biggest USB gap.
