# Card A-8 — Rebuild GRUB ISOs with Current Kernel Fixes

Card ID: card-A-8
Status: complete
Date: 2026-10-30
Owned path: Build artifacts only (no source changes)

## Summary

Rebuilt both GRUB-based bootable ISOs (`symbian-grub.iso` and `symbian-grub-bios.iso`) with the current kernel that includes card-A-2 (CRC table fix) and card-A-3 (PE2 import support). GRUB ISOs use GRUB's own disk drivers instead of the hand-written bootsect EDD path, which avoids a VirtualBox BIOS hang on the 2nd EDD read.

## Changes

### Build artifacts (no source modified)

| File | Size | Date | Boot methods |
|------|------|------|-------------|
| `build/symbian-grub.iso` | 8,640,512 | Oct 30 | BIOS El Torito + UEFI El Torito |
| `build/symbian-grub-bios.iso` | 8,640,512 | Oct 30 | BIOS El Torito only |

Both ISOs contain:
- `build/kernfull.elf` — full kernel with CRC table fix + PE2 import support + debug KPrintfs
- `build/multiboot.elf` — nanokernel
- GRUB 2.12 menu with serial console output

## Verification

### QEMU boot test (symbian-grub.iso)

```
GRUB version 2.12
[GRUB menu: Symbian EKA2 full kernel (genuine) / nanokernel]
Booting `Symbian EKA2 full kernel (genuine)'
Booting full kernel...
Multiboot ok, booting...
KEKA2-QEMU bridge: 32-bit protected mode, paging off, COM1 ok
EKA2-QEMU bridge: multiboot magic 2BADB002 OK
EKA2-QEMU bridge: jumping to ported ResetEntry...
EKA2-QEMU ResetEntry: multiboot handoff OK (128M RAM, ROM-in-RAM deferred)
EKA2-QEMU ResetEntry: calling ported InitCpu (GDT/IDT)...
InitCpu done, cpu_features=0x0781ABFD
...
Symbian EKA2 x86 full kernel -- interactive console
sched by TheScheduler, IRQs via genuine vectors + variant Bind
EKA2>
```

- GRUB menu displays with serial + console output
- Autocountdown 10s boots full kernel by default
- Multiboot handoff OK
- Kernel reaches interactive shell at `EKA2>`
- All card-A-2/3 fixes included in kernel

## Physical hardware test procedure (card-A-9)

1. Write ISO to USB: `dd if=symbian-grub-bios.iso of=/dev/sdX bs=4M status=progress`
   - Use BIOS-only ISO for legacy BIOS PCs, combo ISO for UEFI-capable PCs
   - **Be careful: /dev/sdX must be the USB device, NOT your internal hard drive**
2. Boot target PC from USB (use one-time boot menu: F12/F2/Esc/F9/F10)
3. GRUB menu appears → select "Symbian EKA2 full kernel (genuine)"
4. Kernel loads → `EKA2>` shell prompt on COM1/VGA
5. Plug in second FAT32 USB with `portfilesrv` or `hello.e32`
6. `mount` → `portfilesrv` / `hello.e32`

## Constraints verified

- No source files modified ✅
- `kernelhwsrs/` read-only — never edited ✅
- No commits/pushes ✅
- Prior build roots preserved ✅
- Card-A-1/2/3/4/5/6/7 fixes all included in ISOs ✅

## Next card

**card-A-9** — Physical PC smoke test: write GRUB ISO to USB, boot real hardware, verify kernel shell and E32 execution. Requires physical PC access (not possible in this sandbox).