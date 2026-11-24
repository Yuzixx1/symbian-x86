# Genuine F32 / EFSRV build + runtime report

## Sources compiled (read-only from kernelhwsrv/)
- **F32** (24 objs → `F32.A`): `fileserver/sfat32/*.cpp` (efat32.mmp SOURCE list,
  incl. `sl_mnt.cpp`, `fat_table32.cpp`, `sl_bpb32.cpp`, `sl_main.cpp`) plus
  `fileserver/fs_utils/{filesystem_utils,bit_vector}.cpp`.
- **EFSRV client library** (21 objs → `EFSRV.A`): `fileserver/sfsrv/cl_*.cpp`
  per `efsrv.mmp`/`efsrv.mmh` (the genuine `efsrv.dll` sources).
- **EFSRV server engine** (48 objs → `EFILE.A`): `fileserver/sfile/sf_*.cpp`
  per `efile.mmp`/`efile.mmh` plus 3 shared `cl_*.cpp`, `byte_pair.cpp`,
  `sr_rom.cpp`. NOTE: `sfsrv/` is the CLIENT; the real `!FileServer` process is
  `sfile/`'s `efile.exe` (`sf_main.cpp` publishes `!FileServer`).

## Build
`python3 genuine/f32_efsrv_build.py --rebuild` (manifest: `BUILD_MANIFEST.json`).
Flags: `gcc -m32 -Os -ffreestanding -fno-exceptions -fno-rtti -fpermissive -w
-fno-pic -fno-pie -fno-stack-protector`; defines `__GCC32__ __X86__ __EPOC32__
__GENERIC_X86__ _UNICODE` (+ `SYMBIAN_PRIVATE_EFSRV` for the server); preinclude
genuine `epoc32/include/gcc.h` + `variant/port_x86_rfile64_v1.hrh`; headers from
the cached genuine SDK export tree. `_UNICODE` is required: sfat32 uses `TDes`
where the genuine Unicode product defines it as `TDes16`.
All 93 files compiled clean (0 errors). Archives/hashes in the manifest.

## E32 validation / QEMU
- `genuine/f32_probe/F32PROBE.E32`: runs **unmodified genuine F32 byte code**
  (`fs_utils::Log2/Count1Bits`, `sfat32::TFatBootSector::Internalize/IsValid/
  FatType/FirstDataSector`) with only local EUSER descriptor primitives;
  `e32read.py --quiet` OK, PE2 import of EUSER ord 915, QEMU markers verified.
- `genuine/f32_probe/F32CLIENT.E32`: real IPC connect + genuine `TFsMessage`
  `SessionPath` (opcode 14) round trip against the running server.
- `genuine/efsrv_min/EFSRVMIN.E32`: minimal `!FileServer`-style host, staged as
  `PORTFILESRV.E32`; `e32read.py --quiet` OK, ProtServ bit patched in header.
- `python3 tools/r1_genuine_efsrv_test.py` → **PASS** (all markers, 2× ord-915
  bind, no Exc/PanicKernExec/FAULT).

## Blockers — status 2026-11-24
1. **Partially addressed.** Full `EFILE.A` now links into `EFILE.E32` (273,068 B)
   via `genuine/pack_multi.py` with PE2 import slots raised 16 → 512; it binds
   **463 EUSER ordinals** in QEMU. Still not a runnable `!FileServer` — see the
   in-flight sret-ABI / `CServer2::Start` items in `docs/GENUINE_PROGRESS.md`.
2. **RESOLVED.** `E32Run` now grants `ECapabilityProtServ` to every loaded E32, so
   `!`-prefixed protected server names publish and exec 126 no longer traps.
3. **RESOLVED.** Const client descriptors (`EDesC8/EDesC16`) are handled in
   `fs/r1_fs_server.cpp` (`KConstMaxLength == KErrBadDescriptor (-38)` is not
   fatal); genuine F32 descriptors pass through this IPC path.
4. **RESOLVED — on-disk FAT I/O.** `genuine/efsrv_min/efsrv_min.cpp` now uses
   `DiskFatBackend`; the port bridge `R1Fs::Backend` → `file_*()` →
   `PortFile`/`PortBigFile` reaches real FAT32 cluster-chain I/O
   (`AtaRead`/`AtaWrite`). Genuine `TFsMessage` opcodes, descriptors, Core and
   `r1_fs_server.cpp` are untouched. **Stub:** cluster I/O goes through the port
   `R1Fat` adapter, not full `sfat32` (PE2 slot limit); genuine F32 BPB parsing
   is still covered by `F32PROBE` only.
5. **Fallback only.** B4 is FIXED: the boot chain runs on a supervisor DThread
   from `Asic::Init3` with `svcmgr`/`portfilesrv` fallback. The FAT-I/O test still
   launches its client directly; the full server's `CServer2::Start("!FileServer")`
   error fix is **IN FLIGHT**.

## On-disk FAT verification (2026-11-24)
- `genuine/efsrv_min/efsrv_min.cpp` — minimal `!FileServer`-style host now backed
  by `DiskFatBackend` through port `R1Fs::Backend` → `file_*()` →
  `PortFile`/`PortBigFile` → FAT32 cluster-chain I/O (`AtaRead`/`AtaWrite`).
- New ring-3 client `genuine/efsrv_min/fatio_client.cpp` → `FATIOCLIENT.E32`,
  driven by `tools/r1_genuine_fat_io_test.py`.
- Evidence: opens disk-only `R1TEST\SEED.TXT`; creates/writes/closes `FATIO.TXT`;
  reopens and reads `FATIO-MARKER-42A7` byte-for-byte — all return 0, no
  `Exc`/`PanicKernExec`/`FAULT`. `FATIO.TXT` is absent pre-boot; host `mtype`
  reads the marker back from the FAT partition after boot.
- `tools/r1_genuine_fat_io_test.py` **PASS**; `tools/r1_genuine_efsrv_test.py`
  **PASS**; `tools/r1_qemu_smoke.py` **PASS**.
- Scope: genuine `TFsMessage` opcodes/descriptors, Core and `r1_fs_server.cpp`
  untouched; cluster I/O still uses the port `R1Fat` adapter rather than full
  `sfat32` (PE2 import-slot limit), and genuine F32 BPB parsing remains only in
  `F32PROBE`.
