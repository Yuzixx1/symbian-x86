# pack_multi.py per-image UID3 fix (ESTART -11 unblock)

Status: implemented in `genuine/pack_multi.py` only. `kernelhwsrv/` untouched,
no QEMU run, no commit/push. Diagnosis: `docs/GUARD_ESTART_BLOCK_ANALYSIS.md`.

## Problem

`pack_multi.py` stamped `UID3=0x100000bf` (genuine EFILE) on **every** PE2 image.
The E32Run single-owner guard (`variant/e32load.cpp:62-66,869-883`) claims
`0x100000bf` for EFILE; ESTART packed with the same UID is then refused with
`KErrAlreadyExists` (-11) because EFILE launched first in the boot chain.
ESHELL carried a build-time `0x10000111` placeholder for the same reason.

## Change (genuine/pack_multi.py only)

* `pack(blob, ordinals, iat_start, uid3=0x100000bf)` — new optional 4th arg;
  default is the EFILE UID so all existing callers are byte-for-byte unchanged.
* Header line 113 now calls `build_args_e32(0x1000008c, uid3, 0)[:156]`.
  **No new CRC logic**: `build_args_e32()` already computes the UID checksum
  (`uid_checksum`, +12) and header CRC (`crc32_symbian`, +20) from `uid3`.
* CLI: `--uid3 0x...` (default `0x100000bf`), passed through from `main()`.
  Help text + module docstring document ESTART/ESHELL genuine values.

## Validation (host-only, no QEMU)

`python3 -m py_compile genuine/pack_multi.py` — OK.
Packed the genuine linked `genuine/build/estart_.elf` (95 IAT slots,
`iat_start=0x6000`, IAT `0x6000..0x617c`) three ways and ran `tools/e32read.py`
(UID checksum, header CRC, PE2 imports + ordinals, full header subset):

| case | UID3 | chk (+12) | CRC (+20) | imports | e32read |
|------|------|-----------|-----------|---------|---------|
| default | `0x100000bf` | `0x55085fa7` | `0x2ea8f015` | 95 | OK |
| `--uid3 0x10272C04` | `0x10272c04` | `0x1683c9d5` | `0x0aa0ec36` | 95 | OK |
| `--uid3 0x101F7AE7` | `0x101f7ae7` | `0xb29a3b6f` | `0xaab69ef2` | 95 | OK |

* Validator script: `/tmp/opencode/verify_pack_multi_uid3.py`; outputs
  `/tmp/opencode/pack_uid3_{default,estart,eshell}.E32`.
* CLI `--uid3 0x10272C04` output is byte-identical to the direct-import path
  (25848 B, matching the staged ESTART.E32 size).
* Default path unchanged: EFILE/EWSRV/EFSRV/EFAT32 callers need no edits.

## Exact caller changes still required (other owners — NOT done here)

1. `genuine/estart_eshell/build_estart_eshell.py:277` — pass the genuine UID3:
   `blob = pack_multi(elf.read_bytes(), ords, iat_start, uid3=UID3[name])` with
   `UID3 = {"estart_": 0x10272C04, "etshell_": 0x101F7AE7,
   "etshell_dbg": 0x101F7AE7, "dbg2_": ..., "dbg3_": ...}`
   (dbg2_/dbg3_ are test helpers; give them non-colliding distinct UIDs).
2. Delete lines 278-296 (the post-pack patch that stamps `0x10000111` and
   manually fixes checksum/CRC) — it is now redundant and wrong.
3. `genuine/build_efile.py`, `genuine/ewsrv/build_ewsrv.py`,
   `genuine/ewsrv/build_client.py`, `genuine/efat32/build_fsy_dll.py`: **no
   change** — EFILE must keep `0x100000bf` (default) so the guard still dedups
   EFILE/EFSRV aliases.

## Unblock rationale

With genuine UID3s the guard's UID3 test alone distinguishes EFILE from ESTART:
`E32Validate` should print `uid3=10272c04` for ESTART and `uid3=101f7ae7` for
ESHELL; neither equals `KPortEfileUid3`, so `isEfile` is false and the single
owner slot cannot refuse them. Pair with the guard/name-fallback changes in
`docs/GUARD_ESTART_BLOCK_ANALYSIS.md` §2A for defence in depth, then check the
§3 no-regression gates and §4 discriminating markers on a rebuilt ISO.
