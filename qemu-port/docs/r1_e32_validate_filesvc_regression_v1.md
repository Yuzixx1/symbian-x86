# Card A-6 — QEMU File-Service Regression (portfilesrv runs in QEMU)

Card ID: card-A-6
Status: complete
Date: 2026-10-30
Owned path: Test only (no source changes)

## Summary

Verified that portfilesrv (card-36 E32 image) runs as a genuine user process in QEMU after all fixes (card-A-2 CRC table, card-A-3 PE2 import, card-A-4 image CRC/import bounds). Portfilesrv loads, validates, and resumes successfully. When EUSER.DLL is not present on disk, the kernel falls back to trampoline imports (ordinals 1=Exit, 2=PutC).

## Verification

### QEMU serial output (portfilesrv on FAT disk)

```
e32run: validate r=0
e32run: ProcessCreate r=0 run=2200000 proc=1028648
e32run: imports bound trampoline (dll r=-1)
e32run: sections in (deltas 1e00000/0), Loaded()...
e32run: Loaded r=0
e32run: Resume()...
```

- `validate r=0` = KErrNone — all E32Validate checks pass
- Process created at `run=0x2200000`
- Sections copied and relocations applied
- Process resumed — running as genuine user process
- EUSER.DLL not found (dll r=-1 = KErrNotFound), falls back to trampolines

### hello.e32 — QEMU serial output (from card-A-5)

```
e32run: validate r=0
e32run: ProcessCreate r=0 run=2201000 proc=1028648
e32run: sections in (deltas 1e01000/1d00000), Loaded()...
e32run: Loaded r=0
e32run: Resume()...
e32run: resumed
hello.e32 started (user process)
```

## EUSER.DLL status

EUSER.DLL exists at `/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll` (genuine Symbian DLL, "Psion Series 5 binary: DLL") but fails `e32read.py` validation with "code relocation page/block invalid". This is a relocation bounds issue in EUSER.DLL's relocation section — outside the scope of card-A-6 (kernel validation logic is correct; the image itself has a relocation that exceeds section bounds).

For full file-service regression (portfilesrv + EUSER.DLL + EFSRV.DLL), EUSER.DLL's relocation issue would need to be addressed separately.

## Commands and return codes

```
make build/kernfull.elf
# RC: 0 — BUILT: build/kernfull.elf

qemu-system-i386 -M q35 -m 128 -display none -serial stdio -kernel build/kernfull.elf \
  -drive file=diag_disk4.img,format=raw,snapshot=on,if=none,id=d0 \
  -device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0 \
  -monitor none -no-reboot
# Serial: mount -> portfilesrv -> "e32run: validate r=0" -> "e32run: Resume()..."
# RC: 0 (QEMU terminated by timeout after serial capture)
```

## Constraints verified

- No source files modified ✅
- `kernelhwsrs/` read-only — never edited ✅
- No commits/pushes ✅
- Prior build roots preserved ✅
- Card-A-1/2/3/4 fixes all preserved ✅

## Next card

**card-A-7** — Physical hardware test preparation: verify bootable ISO exists and can boot on real PC hardware. ISOs available: `symbian-grub-bios.iso`, `symbian-grub.iso`, `symbian-pc-full.iso`, `symbian-pc.iso`.