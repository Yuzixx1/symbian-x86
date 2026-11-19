# Card 36 — R1 file service host: petran E32 conversion of `portfilesrv.exe`

## Status
Active, blocked on this card only. Card 35 complete (PEi-i386 built, verified). Card 34 complete (EFSRV verified). Card 32 (QEMU file-service regression) waits on this.

## Owned paths
New `qemu-port/docs/r1_portfilesrv_e32_v1.md` (conversion report) only. Do not edit `r1_service_host.{h,cpp}` or card 31/35 source. All work under `/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/` (check unused destination/quota first).

## Dependencies
- Card 35 output: `/home/yuzi/.cache/sybian-r1/r1-portfilesrv-build-v1/bin/portfilesrv.exe` (PEi-i386, SHA-256 `e3ca495061b11656d2cb592fe4dd5ece486217ccf4855065111352eba0f75c88`, 6240 bytes)
- Card 34 output: `efsrv.lib` / `efsrv.dll` verified in existing cold build roots; ordinals 68/93/97/259/263/300 present/non-ABSENT
- Existing petran.exe: `/tmp/opencode/r1-build-repro/epoc32/tools/petran.exe` (SHA-256 `65f0015bb89d569748fd067d3adbdbd35950f1e2c5da1766af3c0e8d568c72e5`), `-v`/`-dump` confirmed working
- Existing env.sh: `/tmp/opencode/r1-build-repro/env.sh` (sets EPOCROOT, SBS_HOME, shims, WINEPREFIX)
- Do not rebuild petran from source unless the existing binary fails; D7 workaround documented in `r1_build_repro_v0.md` (compile 20 petran sources by hand with MinGW 3.4.5, flags in `logs/petran_cflags.txt`, add `-include new`)

## Task
1. Source `/tmp/opencode/r1-build-repro/env.sh` (sets EPOCROOT, SBS_HOME, WINEPREFIX, shims).
2. Verify petran.exe runs: `wine petran.exe -v` (check version/header dump works).
3. Copy card 35's `portfilesrv.exe` into isolated conversion root (do not modify card 35 output).
4. Run petran E32 conversion on `portfilesrv.exe` → produces E32 image (sibling `.sym` is the PE intermediate; the file without `.sym` is the converted E32 image, identified by `file` as "Psion Series 5 binary").
5. Verify E32 image with `wine petran.exe -v` and `wine petran.exe -dump`:
   - UID3/SID/VID/capabilities
   - Format header 2, not compressed
   - Entry point, code/data link addresses
   - Import/export slot counts and ABSENT status
   - Heap/stack sizes
6. Record emitted E32 image SHA-256 and location.
7. Verify imports against card 35 audit: `euser{000a0000}[100039e5].dll` ordinals 1221/1953 present; EFSRV ordinals 68/93/97/259/263/300 present/non-ABSENT.

## Deliverable
`qemu-port/docs/r1_portfilesrv_e32_v1.md` with:
- Conversion command, exact argv/rc/warnings
- petran.exe source path and hash
- Input PEi-i386 hash (card 35's `e3ca4950…`)
- Emitted E32 image SHA-256 and location
- E32 header facts: UID3/SID/VID/caps, format, compression, entry, link addresses, export/import slots/ABSENT
- Import audit confirmation (EUSER + EFSRV ordinals present/non-ABSENT)
- Any D7 workaround applied (only if petran.exe needed rebuilding)

## Constraints
- No edits to card 31/35 source.
- No QEMU, no guest execution, no service registration, no `FsRegisterThread` call.
- No commits/pushes.
- Preserve all prior build roots and `/tmp/opencode/stabilization/`.
- Label source evidence vs proposals.
- No system-wide installs; all under `/home/yuzi/.cache/sybian-r1/`.

## Exclusivity
No shared files touched. Blocks nothing except card 32; independent of all other cards.

## Next after success
Card 32 (QEMU file-service regression) holds the exclusive QEMU slot and waits on this E32 image. Card 33 (your physical PC smoke test) waits on card 32.
