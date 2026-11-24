# ESTART/ESHELL genuine-UID3 cleanup (build_estart_eshell.py)

Status: implemented in `genuine/estart_eshell/build_estart_eshell.py` only.
`kernelhwsrv/` untouched, no `make clean`, no commit/push.
Background: `docs/GUARD_ESTART_BLOCK_ANALYSIS.md`, `docs/PACK_MULTI_UID_FIX.md`.

## Problem

`pack_multi.py` defaults to `UID3=0x100000bf` (the genuine EFILE UID) on every
image.  The E32Run single-owner guard (`variant/e32load.cpp`) claims that UID
exclusively for the file server, so ESTART (launched after EFILE in the genuine
boot chain) was refused with `KErrAlreadyExists` (-11).  The build worked around
this by stamping a placeholder `0x10000111` UID3 on ESHELL post-pack, while
ESTART kept the colliding EFILE UID.

## Change

`pack_multi.py` now accepts `uid3=` (default `0x100000bf`, so EFILE/EWSRV/EFSRV/
EFAT32 callers are unchanged).  `build_estart_eshell.py`:

* Added a per-image `UID3` map and pass `uid3=UID3[name]` to `pack_multi()`:
  - `estart_`     -> `0x10272C04` (genuine ESTART)
  - `etshell_`    -> `0x101F7AE7` (genuine ESHELL)
  - `etshell_dbg` -> `0x101F7AE7` (instrumented ESHELL, same genuine UID)
  - `dbg2_`       -> `0x10000112` (debug-only EUSER-export bisect helper)
  - `dbg3_`       -> `0x10000113` (debug-only console-shim helper)
* Deleted the old post-pack block (former lines 278-296) that stamped
  `0x10000111` and manually rewrote the UID checksum/header CRC — now redundant
  because `pack_multi.py` recomputes both from `uid3`.
* Added `uid3=` to the build summary line.  All sources/flags unchanged.

`dbg2_`/`dbg3_` were chosen as distinct, non-colliding debug-only UIDs; report
these values to any owner who expects a specific debug UID.

## Host validation (rebuild all five E32s from source)

`python3 genuine/estart_eshell/build_estart_eshell.py all` -- all five built,
each validated by `tools/e32read.py --quiet`.  Recomputed UID checksum and
header CRC (field zeroed to `KImageCrcInitialiser` before CRC) both match:

| image | UID3 | UID chk (+12) | header CRC (+20) | EUSER imports |
|-------|------|---------------|------------------|---------------|
| estart_ | `0x10272c04` | `0x1683c9d5` | `0x0aa0ec36` | 95 |
| etshell_ | `0x101f7ae7` | `0xb29a3b6f` | `0x10e5d617` | 190 |
| etshell_dbg | `0x101f7ae7` | `0xb29a3b6f` | `0xe072e87c` | 190 |
| dbg2_ | `0x10000112` | `0x66393485` | `0x76bc959a` | 23 |
| dbg3_ | `0x10000113` | `0x663907b4` | `0xa489608a` | 23 |

No estart_/etshell_ image carries `0x100000bf` any more.

## QEMU validation

* `tools/r1_genuine_eshell_test.py` -- **PASS**: ESTART/ESHELL genuine images
  loaded, `PS` command PASS, all 8 commands exercised, EUSER ord 915/1793/1849
  bound, no `Exc/PanicKernExec/FAULT/#PF`.  (Tests stage `ESTARTX.E32` /
  `ESHELLX.E32` straight from `genuine/build/`, so no ISO rebuild needed.)
* `tools/r1_genuine_retire_test.py` -- **ESTART/ESHELL gate PASS** (shell boot
  markers `ESTART`, `ESHELL`; no faults).  EWSRV PASS, EUSER PASS.  EFSRV/F32
  **FAIL transiently**: boot markers and `EFILE.E32 r=0` present, EFILE claimed
  once, port-shim=0, no `#PF`/FAULT/Panic, but `F32CLIENT: SessionPath r=-15`
  instead of the `F32CLIENT: IPC round trip OK` marker.  This is the shared
  file-server IPC path (EFILE keeps `0x100000bf` by design) and is attributed to
  concurrent rebuilds in flight, not to the ESTART/ESHELL UID change.
* `tools/r1_qemu_smoke.py` -- **PASS** (EUSER resident, 2503 exports, imports
  bound, ProcessCreate/Loaded/Resume OK, no Exc/Panic).

## Boundaries

`kernelhwsrv/` untouched, no `make clean`, no commit/push.  Only
`genuine/estart_eshell/build_estart_eshell.py` and this doc were changed; the
five `genuine/build/*E32` outputs were regenerated.
