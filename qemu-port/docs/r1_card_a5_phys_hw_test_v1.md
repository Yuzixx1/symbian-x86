# Card A-5 — Physical Hardware Test Procedure

Card ID: card-A-5
Status: pending
Date: 2026-10-30

## Summary

Physical hardware smoke test of the Sybian REAL OS on a real PC. The kernel and E32 images have been validated in QEMU (cards A-1 through A-4 complete). This card documents the procedure for booting the OS on real PC hardware via USB.

## Prerequisites

1. A disposable USB flash drive (will be wiped)
2. A PC to test on (existing hardware with important data must NOT be touched)
3. The ISO image: `qemu-port/build/symbian-grub-bios.iso`

## Procedure

### Step 1: Verify the ISO
```bash
ls -la /home/yuzi/Documents/Sybian\ REAL/qemu-port/build/symbian-grub-bios.iso
ls -la /home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf
```

Both files should exist with non-zero size.

### Step 2: Write ISO to USB (HOST OS - not the target PC)
```bash
# Identify the USB device (BE CAREFUL - wrong device = data loss)
lsblk

# Write ISO to USB
sudo dd if=/home/yuzi/Documents/Sybian\ REAL/qemu-port/build/symbian-grub-bios.iso of=/dev/sdX bs=4M status=progress
sync
```
**WARNING:** Replace `/dev/sdX` with the actual USB device. Double-check with `lsblk` before running. Writing to the wrong device will destroy data.

### Step 3: Boot from USB
1. Plug the USB into the target PC
2. Reboot and press the boot menu key (usually F12, F2, F10, or Esc depending on manufacturer)
3. Select the USB drive from the boot menu
4. Select "Symbian EKA2 full kernel (genuine)" from the GRUB menu

### Step 5: Verify boot
Expected serial/console output:
- "EKA2-QEMU bridge: multiboot magic 0x2BADB002 OK"
- "Calling KernelMain()..."
- "Symbian EKA2 x86 full kernel -- interactive console"
- Shell prompt: `EKA2>`

### Step 6: Test E32 execution (optional, requires FAT disk)
If a second FAT USB is available with E32 images:
- `mount` — mount the FAT partition
- `hello.e32` — should print "hello.e32 started (user process)"
- `portfilesrv` — should print "e32run: validate r=0" and resume

## Success Criteria

- Kernel boots to interactive shell (`EKA2>` prompt)
- Mount command works (FAT partitions detected)
- E32 images load and run (if tested)

## Constraints

- Never write to the internal hard drive
- Only use disposable USB media
- Test on a spare PC if available; otherwise use the user's PC with extreme caution
- No data should be written to any internal storage device

## Next Steps

If physical hardware test passes: project milestone complete — EKA2 kernel boots on real hardware and runs E32 images.
