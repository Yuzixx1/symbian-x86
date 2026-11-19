# Card 32 — R1 file-service QEMU regression (exclusive QEMU slot)

## Status
Offered, not launched. Holds the exclusive QEMU/stable-suite slot.

## Owned paths
- `qemu-port/tools/test_stable.py` (wire file-service regression; do not weaken existing 15 checks)
- `qemu-port/tests/r1_file_regression/` (new — regression driver + manual step)
- `qemu-port/docs/manual_test_order.md` (add manual step)

## Dependencies
- Card 36 complete: E32 `portfilesrv` image at `/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv` (SHA-256 `d91ebcf28b659e84ab69bbbad9ff08828e72bfdbc2e307966a0e6c9bce6809d0`, 884 bytes, `Psion Series 5 executable`, UIDs `1000007a 00000000 00002323`, SID `00000000`, VID `70000001`, Caps ProtServ-only `00000000 00000100`)
- Card 30 approved: `t_cactw.exe` (active scheduling) and `fhserver.exe` (file I/O) as R1-B proof apps, built at build level in `/tmp/opencode/r1-build-repro/`
- Card 31 declarations: `r1_service_host.h/.cpp` (service host source, not rebuilt here)
- Card 28 staged console assets (HAL/EKDATA) not required for file-service regression

## Task
1. Wire a file-service regression into `tools/test_stable.py`:
   - Registered `!FileServer` via `portfilesrv` E32 image (B2 `FsRegisterThread` once)
   - Fixture launch: `t_cactw` scheduler proof and `fhserver` file proof
   - Operations: create/write/flush/close/reopen, EOF/no-hole, both share orders
   - Deliberate negatives: `FSCLIENT/19`, `FSCLIENT/20`, `Panic/-8`
   - Second-boot persistence on disposable image (same media, reopen, byte/size compare)
2. Run in the exclusive QEMU slot, serially (no concurrent QEMU suites)
3. Existing 15 checks must remain PASS; G skip/pass honest per asset availability
4. Add manual test step to `manual_test_order.md`
5. Stop on first real failure with preserved transcript; no test weakening

## Deliverable
- `qemu-port/tools/test_stable.py` changes (file-service regression wired)
- `qemu-port/tests/r1_file_regression/` driver + README
- `qemu-port/docs/manual_test_order.md` manual step added
- `qemu-port/docs/r1_file_regression_v1.md` results transcript with exact commands/timings

## Constraints
- No QEMU slot sharing; no concurrent QEMU suites
- No commits/pushes
- Preserve all prior build roots and logs
- Label source evidence vs proposals

## Exclusivity
Holds the exclusive QEMU/stable-suite slot. Card 33 (physical PC smoke test) waits on this.

## Next after success
Card 33 — your physical PC smoke test on the reference Pentium 4-era PC (legacy BIOS, PS/2, VGA-only, no serial capture; boot, launch, visible output, persistence, shell recovery; archive boot mode/media/controller/visible error/last successful action).
