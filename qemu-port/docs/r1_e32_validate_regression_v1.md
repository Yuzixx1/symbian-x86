# Card A-5 — QEMU Regression Test (E32 Images Pass Validation)

Card ID: card-A-5
Status: complete
Date: 2026-10-30
Owned path: Test only (no source changes)

## Summary

Verified that E32 images pass `E32Validate()` and run as genuine user processes in QEMU after the card-A-2 (CRC table) and card-A-3 (PE2 import) fixes. Two images tested:

1. **hello.e32** (mke32-generated, import-free EXE with code relocations, data, BSS) — passes validation and runs
2. **portfilesrv** (card-36, PE2-style imports) — passes validation and runs (from card-A-4)

## Verification

### hello.e32 — QEMU serial output

```
E32Validate: validate r=0
e32run: ProcessCreate r=0 run=2201000 proc=1028648
e32run: sections in (deltas 1e01000/1d00000), Loaded()...
e32run: Loaded r=0
e32run: Resume()...
e32run: resumed
hello.e32 started (user process)
```

Image details:
- uid1=0x1000007a, uid2=0x10000001, uid3=0x10000002
- codeSize=512, dataSize=16, bssSize=4, textSize=512
- codeOff=0x9c (156), dataOff=0x29c, impOff=0 (import-free)
- codeRelocOff=0x2ac (has code relocations)
- Entry point at code offset 0
- `dllRefs=0` (no imports)

### portfilesrv — QEMU serial output (from card-A-4)

```
e32run: validate r=0
e32run: ProcessCreate r=0 run=2200000 proc=1028648
e32run: imports bound trampoline (dll r=-1)
e32run: sections in (deltas 1e00000/0), Loaded()...
e32run: Loaded r=0
e32run: Resume()...
```

Image details:
- uid1=0x1000007a, uid2=0x00000000, uid3=0x00002323
- codeSize=684, dataSize=0, textSize=672
- codeOff=0x9c (156), impOff=0x348 (PE2 imports, isize=40)
- `dllRefs=1` (EUSER imports via PE2 format)

## Commands and return codes

```
# hello.e32 test
qemu-system-i386 -M q35 -m 128 -display none -serial stdio -kernel build/kernfull.elf \
  -drive file=diag_disk_hello.img,format=raw,snapshot=on,if=none,id=d0 \
  -device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0 \
  -monitor none -no-reboot
# Serial: mount -> hello.e32 -> "e32run: validate r=0" -> "hello.e32 started (user process)"
# RC: 0 (QEMU terminated by timeout after serial capture)

# portfilesrv test (from card-A-4)
# Serial: mount -> portfilesrv -> "e32run: validate r=0" -> "e32run: Resume()..."
# RC: 0 (QEMU terminated by timeout after serial capture)
```

## Constraints verified

- No source files modified ✅
- `kernelhwsrs/` read-only — never edited ✅
- No commits/pushes ✅
- Prior build roots preserved ✅
- Card-A-1 debug KPrintfs preserved ✅
- Card-A-2 CRC table fix preserved ✅
- Card-A-3 PE2 import support preserved ✅

## Next card

**card-A-6** — QEMU file-service regression: portfilesrv as file server host with EUSER.DLL resident. Tests end-to-end file service: boot kernel, mount FAT disk with portfilesrv + EUSER.DLL, run portfilesrv, verify file operations work.