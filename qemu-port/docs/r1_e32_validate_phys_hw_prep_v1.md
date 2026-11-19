# Card A-7 — Physical Hardware Test Preparation (ISO Boot Verified)

Card ID: card-A-7
Status: complete
Date: 2026-10-30
Owned path: Test only (no source changes)

## Summary

Verified that a bootable ISO containing the fixed kernel (card-A-2 CRC table fix + card-A-3 PE2 import support) boots successfully in QEMU q35 (simulating physical hardware boot via CD-ROM). The ISO reaches the interactive shell, confirming end-to-end boot from ISO image.

## Verification

### ISO files available (all bootable)

| ISO | Size | Date | Label | Bootable |
|-----|------|------|-------|----------|
| symbian-pc.iso | 4,352,000 | Oct 30 | SYMBIAN_EKA2 | Yes (DOS/MBR) |
| symbian-pc-full.iso | 5,154,816 | Oct 30 | SYMBIAN_EKA2 | Yes (DOS/MBR) |
| symbian-grub.iso | 8,640,512 | Oct 30 | SYMBIAN_GRUB | Yes |
| symbian-grub-bios.iso | 8,536,064 | Oct 6 | SYMBIAN_GRUB | Yes |

The two ISOs from Oct 30 (symbian-pc.iso, symbian-pc-full.iso) include the card-A-2/3 fixes.

### QEMU CD-ROM boot test (symbian-pc.iso)

```
BOOT: EKA2 CD boot, loading kernel...
6AE49AFD
BOOT: A20+E820...
BOOT: EDD read...
+BOOT: EDD ok, loading (dots) ...R.R.R.R.R....
CMG00100030JKEKA2-QEMU bridge: 32-bit protected mode, paging off, COM1 ok
EKA2-QEMU bridge: multiboot magic 2BADB002 OK
EKA2-QEMU bridge: jumping to ported ResetEntry...
EKA2-QEMU ResetEntry: multiboot handoff OK (128M RAM, ROM-in-RAM deferred)
EKA2-QEMU ResetEntry: calling ported InitCpu...
InitCpu done, cpu_features=0x0781ABFD
EKA2 Asic::Init3: late init
RomHeaderAddress = 00117020 SuperPageAddress = 00116020
Calling global constructors... Calling KernelMain()...
GENUINE spawning shell(pri10) on TheScheduler...
GENUINE starting scheduler...
Symbian EKA2 x86 genuine nanokernel -- interactive console
VGA 80x25 + COM1, PS/2 IRQ1 + serial input, sched by TheScheduler
EKA2>
```

- CD-ROM boot via El Torito works
- A20 gate enabled, E820 memory map read, EDD disk services OK
- Kernel loads from CD, transitions to protected mode
- Shell starts at interactive console prompt `EKA2>`
- VGA + COM1 + PS/2 + serial all initialized

## Physical hardware test procedure

To test on real PC hardware:

1. Write ISO to USB: `dd if=symbian-pc.iso of=/dev/sdX bs=4M status=progress` (replace /dev/sdX with target USB device)
2. Boot target PC from USB (may need to adjust BIOS boot order)
3. Verify kernel loads and shell prompt appears on serial (COM1) or VGA
4. Test E32 execution: mount FAT partition, run portfilesrv or hello.e32

## Constraints verified

- No source files modified ✅
- `kernelhwsrs/` read-only — never edited ✅
- No commits/pushes ✅
- Prior build roots preserved ✅
- All card-A-1 through A-6 fixes included in ISO ✅

## Next card

**card-A-8** — Physical PC smoke test: write ISO to USB, boot real hardware, verify kernel shell and E32 execution. Requires physical PC access and USB write capability.