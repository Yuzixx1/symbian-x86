# VBox / real-hardware test plan (for the human with the hardware)

Media (all rebuilt by default `make`, never stale):
- `build/symbian-grub.iso` — primary. GRUB2 combo: BIOS (`i386-pc-eltorito`)
  + UEFI (`x86_64-efi`). Boots `kernfull` on BIOS, UEFI-CD (OVMF verified),
  VirtualBox EFI (verified earlier series).
- `build/symbian-pc-full.iso`, `build/symbian-pc.iso` — classic El Torito
  reference path (QEMU-verified; VBox kept hanging on EDD — GRUB is primary).
- Test FAT disk (build it on Linux, attach as 2nd drive):
  `python3 tools/mkdisk.py seeded /tmp/hwtest.img 64`
  `for f in build/hello.e32 build/plain.e32 build/nodata.e32 build/spin.e32 build/imp.e32; do mcopy -o -i /tmp/hwtest.img@@2048S $f ::; done`
  Uppercase 8.3 names: HELLO.E32 PLAIN.E32 NODATA.E32 SPIN.E32 IMP.E32.

## VirtualBox runs (do in order, report each)
1. BIOS + GRUB-CD + FAT-HDD second drive. Expect: GRUB menu -> `EKA2>`
   (serial shows boot; VGA shows shell banner).
2. In shell: `mount`, `vol` (FAT disk as C:), then:
   `exec C:HELLO.E32` -> VGA row 22 shows `HI!` + green D, `0`, green H,
   green m. `exec C:IMP.E32` -> row 22 `IM`, row 21 col 0 yellow `P`.
   `ps` -> only ekern (+ latest dead pending reap is NORMAL).
   `exec C:SPIN.E32` (low prio spinner) -> `ps` shows SPIN, `kill <id>` ->
   `killed`, next `exec` prints `reaped 1`, `ps` clean.
3. `meminfo` before/after several execs: heap must sit ~50-57K, not grow
   ~2.3K/run (that was the old leak; fixed).
4. USB fingers: attach a USB keyboard to the VM (OHCI port, NOT xHCI —
   VirtualBox has no UHCI; our stack is UHCI+OHCI polled). In shell: `usb`
   (should enumerate), `usb poll`, type keys. Report: does the `N key(s)`
   counter move? Which controller line prints (UHCI/OHCI)? A mouse may
   enumerate with no consumer (expected, no driver yet).
5. UEFI: same ISO via VirtualBox EFI. Expect identical shell. (EFI-GRUB
   sees only the CD, never the FAT image — attach FAT as HDD.)
   NOTE (Oct 2026, verified): AHCI now works — q35 ICH9 tested end to
   end (mount C: dev 7, exec user EXEs, write/type/del round-trip). OVMF
   on QEMU-pc (`-M pc`) mounts + execs fine. For VirtualBox/real HW use
   the AHCI controller where no legacy IDE exists (drives appear as dev
   4+, tagged [AHCI] in `ide`).

## Real hardware (legacy BIOS first, then UEFI)
- BIOS: write `symbian-grub.iso` to USB stick (`dd`), boot. PS/2 is the
  safe input path; UHCI/OHCI and directly attached xHCI HID keyboards
  are supported after `usb` is run. Expect `EKA2>` on VGA + serial
  (115200 8N1).
- UEFI: same stick via UEFI boot menu (BOOTX64.EFI). Expect same.
- Storage: on-board IDE (PIO) or AHCI SATA (`ahci` verb to verify;
  ATAPI CDs skipped). `mount`/`dir`/`exec` as above.
- USB: PS/2, UHCI/OHCI, or one directly attached xHCI HID boot keyboard.
  Run `usb` to auto-probe; if absent, use `xhciports` to identify the port,
  then `xhcikbd N` (1-based port). Test typing a shell command through the
  USB keyboard; report `xhci`, `xhciports`, and `usb` output. Hubs and
  multiple xHCI devices are not supported yet.
- Report: board/ports used, where it hangs (last serial line), photos of
  VGA row 22 after `exec C:HELLO.E32`.

## Report format (paste into chat)
For each run: profile (VBox BIOS / VBox EFI / HW board+BIOS-ver), last 10
serial lines on failure, `ps`/`meminfo`/`segs` outputs, VGA row-22 contents,
USB lines. Anything red/error/fault verbatim.
