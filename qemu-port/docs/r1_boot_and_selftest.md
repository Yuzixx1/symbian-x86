# Booting the EKA2 PC OS (GRUB) and running the self-test

## Prerequisites
`qemu-system-i386`, `python3`, `mkfs.fat`/`mcopy`/`mdir` (mtools), `pycdlib`, and GRUB 2.12 tools (`grub-mkrescue`/`xorriso`) for the primary ISO.

## PRIMARY ISO: `build/symbian-grub.iso`
GRUB 2.12 El Torito image. It auto-selects "Symbian EKA2 full kernel (genuine)" after a ~10s GRUB countdown, then boots the full `kernfull` kernel to the interactive `EKA2>` shell (user E32s load from there). Exact QEMU command:
```
qemu-system-i386 -M q35 -m 128 -display none -serial stdio -no-reboot -cdrom build/symbian-grub.iso -boot d -netdev user,id=n0 -device e1000,netdev=n0
```
Note the ~10s GRUB countdown before autoboot.

## Secondary / alternate ISOs in `build/`
- `symbian-pc-full.iso` — full `kernfull` kernel via the custom bootsect El Torito (all drivers), boots to `EKA2>`.
- `symbian-pc.iso` — bridge kernel (multiboot), minimal console.
- `symbian-grub-bios.iso` — GRUB BIOS variant of the GRUB image.

## Build
- Primary GRUB ISO: `make isogrub`
- User apps / E32s: `make userland`

## One-command test
`python3 tools/r1_os_selfcheck.py` — builds, makes the ISO, and runs smoke + cp4 + stress + selftest.

## Single self-test (manual)
Stage `SELFTEST.E32` + `EUSER.DLL` on the FAT disk, boot the primary ISO, then at `EKA2>` run `portfilesrv` (optional) then `selftest`. A pass looks like:
```
ST: SELFTEST START - EKA2 PC OS core services
ST: euser OK
ST: malloc OK
ST: chunk OK
ST: file OK
ST: ipc OK
ST: netbridge OK
ST: failures=0
ST: RESULT PASS - all core services operational
```

## Notes
- `kernelhwsrv/` untouched (READ-ONLY).
- Committed and pushed to `https://github.com/Yuzixx1/symbian-x86.git` (`main`).
