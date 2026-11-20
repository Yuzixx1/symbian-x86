# Booting the EKA2 PC OS and running the self-test

## Prerequisites
`qemu-system-i386`, `python3`, `mkfs.fat`/`mcopy`/`mdir` (mtools), `pycdlib`.

## The three ISOs in `build/`
- `symbian-pc-full.iso` — full `kernfull` kernel, all drivers (e1000, virtio-*, AC97, VBE, PS/2, USB, AHCI); boots to interactive `EKA2>` shell + runs user E32s.
- `symbian-pc.iso` — bridge kernel (multiboot), minimal console.
- `symbian-grub.iso` — GRUB El Torito variant (pre-existing).

## Boot each from CD (full kernel: type `portfilesrv` at `EKA2>` before user E32s load)
```
qemu-system-i386 -cdrom build/symbian-pc-full.iso -m 128 -display none -serial stdio -no-reboot -boot d
qemu-system-i386 -cdrom build/symbian-pc.iso -m 128 -display none -serial stdio -no-reboot -boot d
qemu-system-i386 -cdrom build/symbian-grub.iso -m 128 -display none -serial stdio -no-reboot -boot d
```

## Run the single self-test
All-in-one (build + ISO + smoke + cp4 + stress + selftest): `python3 tools/r1_os_selfcheck.py`
OR manual: build userland, stage `SELFTEST.E32` + `EUSER.DLL` on the FAT disk, boot the ISO, type `portfilesrv` then `selftest`, look for `ST: RESULT PASS`.

## What a pass looks like
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
- Committed as `08f4a76`, pushed to `https://github.com/Yuzixx1/symbian-x86.git` (`main`).
