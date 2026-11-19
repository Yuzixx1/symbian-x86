Card ID: card-A-1
Tier: strong
Owned paths: variant/e32load.cpp
Dependencies: (none)
Exclusivity: (none)
Constraints:
- Do not modify proof apps under apps/proof/
- Do not run hardware tests
- Do not change the QEMU lock file
Deliverables:
- variant/e32load.cpp (with debug prints only — do not remove yet)
- qemu-port/docs/r1_e32_validate_diag_v1.md (diagnosis report)
Effort estimate: 2-4 hours

## Task

The kernel's E32Validate() in variant/e32load.cpp rejects card 36's E32 portfilesrv image
with "validate r=-20" (KErrCorrupt). The image is at:
  /home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv
(sha256 d91ebcf28b659e84ab69bbbad9ff08828e72bfdbc2e307966a0e6c9bce6809d0, 884 bytes).
It passes petran -v but fails kernel E32Validate.

Steps:
1. Add temporary KPrintf debug statements before EACH return KErrCorrupt / return KErrNotSupported
   in E32Validate() (lines ~277-429 of variant/e32load.cpp). Print the line number and which
   check failed.
2. Rebuild build/kernfull.elf: make build/kernfull.elf
3. Boot in QEMU with portfilesrv on a disposable FAT disk, capture serial output.
4. STOP. Do not remove debug prints. Do not fix the check yet.

## Deliverable

Create qemu-port/docs/r1_e32_validate_diag_v1.md with:
- The exact check that fails (line number + condition name)
- The serial log from QEMU showing the failure
- Values being compared (expected vs actual) for the failing check
- Whether it's UID checksum (offset 12), header CRC (offset 20), import section format, relocation bounds, or entry point validation

## Constraints
- No edits to card 31/35/36 source
- No edits to qemu_variant.h, kern_shell.cpp, or any other variant file
- No QEMU regression test yet (that's a later card)
- No commits/pushes
- Preserve all prior build roots