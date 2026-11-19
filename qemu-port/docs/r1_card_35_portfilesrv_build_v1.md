# Card 35 — R1 file service host: provision toolchain + build `portfilesrv.exe`

## Status
Active — implementation. Card 34 completed (EFSRV def/import library verified in existing cold build roots). Card 35 now owns toolchain provisioning and the `portfilesrv.exe` build.

## Owned paths
- `qemu-port/services/r1_service_host.{h,cpp}` (card 31 source — compile as-is, no edits)
- New `qemu-port/docs/r1_portfilesrv_build_v1.md` (build report)
- Build artifacts under unused `/home/yuzi/.cache/sybian-r1/r1-portfilesrv-build-v1` (check unused destination/quota first)

## Toolchain provisioning (authorized)
Card 35 may install/provision locally under `/home/yuzi/.cache/sybian-r1/` (home-backed, owner-approved location):
- Raptor + MinGW GCC 3.4.5
- Python 2.7 (for Raptor build scripts)
- Complete `epoc32/` include tree (Symbian headers supplying `IMPORT_C`, `__TText`, `Int64` predefs)
- `sbs`, `abld`, `petran`, `elf2e32`, `dlltool`

**Constraints:** No system-wide installs. Install only under `/home/yuzi/.cache/sybian-r1/`. Preserve all prior build roots (`r1-build-v1`, `r1-build-v1-repeat`, `r1-fixture-build-v1`, etc.). Do not modify `/tmp/opencode/stabilization/`. No upstream edits.

## Dependencies
- Card 34 completed: `efsrv.lib` verified present in both cold build roots (`r1-build-v1`, `r1-build-v1-repeat`), generated from `eabi/efsrvu.def` via Raptor's documented `eabi` fallback. Key EFSRV ordinals present/non-ABSENT: `RFs::Connect`@68, `RFile::Open`@93, `RFile::Write`@97, `RFile::Read`@259, `RFile::Seek`@263, `RFile::Close`@300. EUSER via `bx86gcc/euseru.def`. No bx86/bx86gcc ordinal mixing.
- Card 28 staged assets: `HAL.DLL`, `EKDATA.DLL`, `EKDATA.00` byte-identical under `/home/yuzi/.cache/sybian-r1/r1-console-staging-v1/sys/bin/`.
- Card 31 declarations: `r1_service_host.h/.cpp` — compile as-is.

## Task
1. Audit existing cold build roots (`/home/yuzi/.cache/sybian-r1/r1-build-v1/`, `r1-build-v1-repeat/`) for toolchain presence (`sbs`, `abld`, `petran`, `elf2e32`, `dlltool`, Python 2.7, `epoc32/`).
2. If toolchain present, reuse it. If missing, provision locally under `/home/yuzi/.cache/sybian-r1/` per above.
3. Compile and link `portfilesrv.exe` as a zero-capability, `ECapabilityProtServ` service host with static EUSER/EFSRV import binding.
4. UID3/SID/caps as documented in card 31 (`r1_file_service_host_v1.md`); fill placeholders from emitted image inspection.
5. Static EUSER/EFSRV imports bound before B3 (not via `!Loader`; dynamic `ELoadLibrary` stays `KErrNotSupported` until genuine rollback).
6. Private isolated build in `r1-portfilesrv-build-v1`; private SDK/prefix/temp; verified reused inputs; preserve all prior roots.
7. Return emitted image SHA-256, UID3/SID/VID/caps audit, imports/ordinals present/non-ABSENT, PE2/V header checks.

## Deliverable
`qemu-port/docs/r1_portfilesrv_build_v1.md` with:
- Build command, exact argv/rc/warnings
- Toolchain provisioning record (what was installed, where, hashes of key tools)
- Emitted image SHA-256 and location
- UID3/SID/VID/caps (NONE capabilities except ProtServ as documented)
- Import/ordinal audit (EUSER + EFSRV, all present non-ABSENT)
- PE2/V header, relocation bounds, zero-data verification

## Constraints
- No edits to card 31 source; compile as-is.
- No target link to kernel beyond normal user EXE link.
- No QEMU, no guest execution, no service registration, no `FsRegisterThread` call at build time (that happens at B2 runtime).
- No commits/pushes.
- Isolated worktree; preserve all prior build roots and `/tmp/opencode/stabilization/`.
- Label source evidence vs proposals.

## Exclusivity
No shared files touched beyond card 31's already-owned `services/` paths. Independent of card 34 (which is complete).

## Next after success
Card 32 (QEMU file-service regression) holds the exclusive QEMU slot and waits on this build's emitted image.
