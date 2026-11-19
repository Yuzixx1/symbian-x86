# Card A-2 — E32Validate() Header CRC Fix

Card ID: card-A-2
Status: complete
Date: 2026-10-30
Owned path: qemu-port/variant/e32load.cpp (+ qemu-port/tools/mke32.py host-side twin)

## Summary

Fixed a genuine bug in the kernel's CRC-32 table used by `E32Validate()`'s header
CRC check (line 304–305 of `variant/e32load.cpp`). Table entry at index 205 was
`0xE5D5C06D` (wrong) instead of `0xE5D5BE0D` (correct standard CRC-32 value).
This single wrong entry caused the header CRC recomputation to produce an
incorrect value, making `E32Validate()` reject otherwise-valid E32 images with
`KErrCorrupt`.

## Root cause

`gCrc32Tab[205]` in `qemu-port/variant/e32load.cpp` contained `0xE5D5C06D` instead
of the standard reflected CRC-32 value `0xE5D5BE0D`. Every CRC-32 computation
that passes through byte index 205 of the table (which is common for typical
header data) produces a wrong result.

The same stale value existed in the host-side twin `qemu-port/tools/mke32.py`
(line 88), which is used to build test E32 images.

## Changes

### qemu-port/variant/e32load.cpp (line 122)
```
-	0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5C06D,0x7CDCEFB7,0x0BDBDF21,
+	0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
```

### qemu-port/tools/mke32.py (line 88)
```
-    0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5C06D,0x7CDCEFB7,0x0BDBDF21,
+    0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
```

## Verification

1. **CRC table matches standard**: All 256 entries of `gCrc32Tab` now match the
   standard reflected CRC-32 table (polynomial 0xEDB88320). Zero differences.

2. **crc205_standard.e32 now passes header CRC**: Previously failed at header CRC
   mismatch with the old table; now passes header CRC (fails at a different
   fixture-specific relocation check, which is expected for a negative test case).

3. **mke32-generated images pass validation**: Freshly built `test_hello.e32` and
   `test_imp.e32` (built with the fixed `mke32.py`) both pass `e32read.py`
   validation (exit code 0).

4. **QEMU kernel rebuild**: `make build/kernfull.elf` — RC: 0, `BUILT: build/kernfull.elf`.

5. **QEMU serial log for portfilesrv**: With the fixed table, `E32Validate` now
   computes `0xedbb8f20` (previously `0x24a4db8c` with the old buggy table).
   This matches the CRC value petran -v reports (`0xedbb8f20`). However, the
   portfilesrv image on disk stores `0xeddbfb20` at offset 20 — petran computed
   `0xedbb8f20` but wrote a different value to disk. This is a petran-side image
   issue, not a kernel bug. The kernel correctly rejects images whose stored
   header CRC does not match recomputation.

```
E32Validate: crc fail computed=0xedbb8f20 hdr=0xeddbfb20
e32run: validate r=-20
portfilesrv (corrupt)
```

## Source evidence vs proposals

**Source evidence (verified):**
- Kernel's old `gCrc32Tab[205]` = `0xE5D5C06D` ≠ standard `0xE5D5BE0D`
- Petran's table (`kernelhwsrs/.../h_mem.cpp`) has `0xE5D5BE0D` at index 205 (correct)
- f32image.h `ValidateHeader()` computes CRC identically: field=`KImageCrcInitialiser`, init=0
- petran -v reports header CRC = `0xedbb8f20` (matches fixed-table computation)
- Portfilesrv stored header CRC = `0xeddbfb20` (≠ both old and fixed table computations)

**Proposals (NOT implemented):**
- None needed. The CRC table fix is the complete fix for the kernel's validation
  logic. Portfilesrv's stored CRC discrepancy is a petran/image issue outside
  kernel scope.

## Commands and return codes

```
make build/kernfull.elf
# RC: 0 — BUILT: build/kernfull.elf

python3 tools/e32read.py /tmp/opencode/test_hello.e32 --quiet
# RC: 0 (valid E32 image built with fixed mke32.py)

python3 tools/e32read.py /home/yuzi/.cache/sybian-r1/r1-format-compat-v1/host-tests-3/crc205_standard.e32 --quiet
# Header CRC now passes (different failure: relocation fixture)
```

## Constraints verified

- Only `qemu-port/variant/e32load.cpp` and `qemu-port/tools/mke32.py` edited ✅
- `kernelhwsrs/` read-only — never edited ✅
- No commits/pushes ✅
- All prior build roots under `/home/yuzi/.cache/sybian-r1/` preserved ✅
- Card-A-1 debug KPrintfs preserved in e32load.cpp ✅
- No QEMU regression test yet (later card) ✅

## Next card

**card-A-3** — Address portfilesrv's stored header CRC mismatch (petran wrote
`0xeddbfb20` but computed `0xedbb8f20`). Options: regenerate the E32 image with
correct CRC, or investigate why petran's disk write diverges from its computation.