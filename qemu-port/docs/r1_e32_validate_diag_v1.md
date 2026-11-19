# Card A-1 — E32Validate() Diagnosis Report

Card ID: card-A-1
Status: complete
Date: 2026-10-30
Owned path: qemu-port/variant/e32load.cpp only

## Summary

The kernel's `E32Validate()` rejects the card-36 portfilesrv E32 image with
`validate r=-20` (KErrCorrupt). The failing check is the **header CRC check**
at **line 304–305** of `variant/e32load.cpp`.

The image passes `petran -v` but fails kernel `E32Validate()` because the
recomputed header CRC does not match the CRC stored in the image header.

---

## The exact check that fails

**File:** `qemu-port/variant/e32load.cpp`
**Function:** `E32Validate()`
**Lines:** 304–305

```cpp
    if (Crc32(tmp, codeOff) != hdrCrc)
        { KPrintf("E32Validate: crc fail computed=0x%08x hdr=0x%08x\r\n", Crc32(tmp, codeOff), hdrCrc); return KErrCorrupt; }
```

**Condition name:** header CRC check (offset 20 in the E32 header)
**Result:** KErrCorrupt (r = -20)

The header CRC is recomputed over a 156-byte copy of the image header with the
CRC-init constant (`0xC90FDAA2`) patched into bytes 20–23 of the copy, then
compared against the `hdrCrc` field read from offset 20 of the original buffer.

---

## Values being compared (expected vs actual)

From QEMU serial output:

| Value | Actual | Expected (from header) | Match? |
|-------|--------|------------------------|--------|
| UID checksum (offset 12) | `0x57ef902b` | `0x57ef902b` | ✅ PASS |
| Signature (offset 16) | `0x434f5045` ("EPOC") | `0x434f5045` ("EPOC") | ✅ PASS |
| **Header CRC (offset 20)** | **`0x24a4db8c`** | **`0xeddbfb20`** | ❌ **FAIL** |

The recomputed CRC (`0x24a4db8c`) does not equal the CRC stored in the image
header (`0xeddbfb20`).

---

## Classification

This is the **header CRC** check (offset 20), NOT:

- ❌ UID checksum (offset 12) — PASSES (computed == header)
- ❌ Import section format — not reached (CRC fails first)
- ❌ Relocation bounds — not reached (CRC fails first)
- ❌ Entry point validation — not reached (CRC fails first)

The CRC check is the 6th check in `E32Validate()` (after flags format, codeOff,
cpu, UID checksum, and signature), and it is the first one that fails.

---

## QEMU serial log showing the failure

Boot command:
```
qemu-system-i386 -M q35 -m 128 -display none -serial stdio -kernel build/kernfull.elf \
  -drive file=diag_disk.img,format=raw,snapshot=on,if=none,id=d0 \
  -device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0 \
  -monitor none -no-reboot
```

Serial commands sent via stdin: `mount`, then `portfilesrv`.

Relevant serial output (from `/tmp/opencode/qemu_final_output.txt`):

```
E32Validate: ENTER aLen=884 aBuf=2277a0
E32Validate: passed null check
E32Validate: first4=7a000010
E32Validate: read uids uid1=1000007a uid2=00000000 uid3=00002323
E32Validate: read hdr2 uidChk=57ef902b sig=434f5045 hdrCrc=eddbfb20 modver=00010000 comp=00000000
E32Validate: read mid flags=22000000 codeSize=684 dataSize=0 heapMin=4096 heapMax=1048576 stackSize=8192 bssSize=0
E32Validate: read tail entry=00000000 codeBase=00400000 dataBase=00000000 dllRefs=1 expOff=00000000 expCount=0 textSize=672 codeOff=0000009c dataOff=00000000 impOff=00000348 crel=00000000 drel=00000000 cpu=4096 pri=350
E32Validate: hdr1 aLen=884 uid1=1000007a uid2=00000000 uid3=00002323 uidChk=57ef902b sig=434f5045 hdrCrc=eddbfb20 modver=00010000 comp=00000000
E32Validate: hdr2 flags=22000000 codeSize=684 dataSize=0 heapMin=4096 heapMax=1048576 stackSize=8192 bssSize=0 entry=00000000 codeBase=00400000 dataBase=00000000
E32Validate: hdr3 dllRefs=1 expOff=00000000 expCount=0 textSize=672 codeOff=0000009c dataOff=00000000 impOff=00000348 codeRelocOff=00000000 dataRelocOff=0x00000000 cpu=4096 pri=350
E32Validate: crc fail computed=0x24a4db8c hdr=0xeddbfb20
e32run: validate r=-20
portfilesrv (corrupt)
```

The line `E32Validate: crc fail computed=0x24a4db8c hdr=0xeddbfb20` is the
diagnostic print added at line 305, confirming the header CRC mismatch.

---

## Header field values (from QEMU serial output)

| Field | Offset | Value |
|-------|--------|-------|
| UID1 | 0 | 0x1000007a |
| UID2 | 4 | 0x00000000 |
| UID3 | 8 | 0x00002323 |
| UID checksum | 12 | 0x57ef902b |
| Signature | 16 | 0x434f5045 ("EPOC") |
| Header CRC | 20 | 0xeddbfb20 |
| Module version | 24 | 0x00010000 |
| Compression | 28 | 0x00000000 |
| Flags | 44 | 0x22000000 |
| Code size | 48 | 684 |
| Data size | 52 | 0 |
| Heap min | 56 | 4096 |
| Heap max | 60 | 1048576 |
| Stack size | 64 | 8192 |
| BSS size | 68 | 0 |
| Entry point | 72 | 0x00000000 |
| Code base | 76 | 0x00400000 |
| Data base | 80 | 0x00000000 |
| DLL refs | 84 | 1 |
| Export off | 88 | 0x00000000 |
| Export count | 92 | 0 |
| Text size | 96 | 672 |
| Code off | 100 | 0x0000009c (156) |
| Data off | 104 | 0x00000000 |
| Import off | 108 | 0x00000348 |
| Code reloc off | 112 | 0x00000000 |
| Data reloc off | 116 | 0x00000000 |
| CPU | 122 | 4096 (0x1000 = x86) |
| Priority | 120 | 350 |
| File length | — | 884 |

---

## Source evidence vs proposals

**Source evidence (verified by QEMU serial output + independent Python recomputation):**
- `E32Validate()` in `variant/e32load.cpp` line 304–305: header CRC check fails
- Computed CRC `0x24a4db8c` != header CRC `0xeddbfb20`
- All prior checks pass: flags format, codeOff, CPU, UID checksum, signature
- Returns `KErrCorrupt` (-20), which propagates as `validate r=-20` in `e32run`

**Proposals (for card-A-2, NOT implemented here):**
- The header CRC may have been computed by `petran` over a different header
  layout or with a different CRC-init placement than the kernel expects
- The fix (card-A-2) will reconcile the kernel's CRC computation with
  `petran`'s output, without modifying `kernelhwsrs/` or card 31/35/36 source

---

## Commands and return codes

```
make build/kernfull.elf
# RC: 0 — BUILT: build/kernfull.elf

qemu-system-i386 -M q35 -m 128 -display none -serial stdio -kernel build/kernfull.elf \
  -drive file=diag_disk.img,format=raw,snapshot=on,if=none,id=d0 \
  -device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0 \
  -monitor none -no-reboot
# Serial: mount -> portfilesrv -> "E32Validate: crc fail computed=0x24a4db8c hdr=0xeddbfb20"
#         -> "e32run: validate r=-20" -> "portfilesrv (corrupt)"
# RC: 0 (QEMU terminated by timeout after serial capture complete)
```

---

## Constraints verified

- Only `qemu-port/variant/e32load.cpp` was edited ✅
- No edits to `qemu_variant.h`, `kern_shell.cpp`, or other variant files ✅
- No edits to card 31/35/36 source ✅
- `kernelhwsrs/` is read-only — never edited ✅
- No commits/pushes ✅
- All prior build roots under `/home/yuzi/.cache/sybian-r1/` preserved ✅
- Debug prints left in place (not removed) ✅
- The failing check was NOT fixed (per STOP instruction) ✅

---

## Next card

**card-A-2** — Fix the failing header CRC check in `E32Validate()`.