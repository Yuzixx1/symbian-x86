# STALE_OBJECT_REBUILD — 2026-11-24

## Verdict
**CONFIRMED and REPAIRED.** cp4_stress's 4/4 FAIL was caused solely by a
stale `build/qemu_variant_asic.o` (mtime 08:31:59, built during the svcmgr
apply/rollback window). Its mtime was newer than its source, so `make` never
rebuilt it. Force-rebuilding that one object + relinking fixed it; no
retirement edit was at fault, and no test/source logic was changed.

## Root cause evidence (docs/CP4_STRESS_INDEPENDENT_CHECK.md)
- `variant/qemu_variant_asic.cpp` byte-identical in all trees: sha256 `ef3878be…`.
- Stale object sha256 `e441e39e…`; freshly-built (correct) object `c6b6c4c3…`.
- Pristine tree 4/4 PASS; pristine + edits 3/3 PASS; stale object seeded into
  fresh tree reproduces the FAIL and replacing just that object restores PASS.

## mtimes — BEFORE
| file | mtime | sha256 (prefix) |
| --- | --- | --- |
| `variant/qemu_variant_asic.cpp` | 2026-11-23 22:47:17 | `ef3878be` |
| `build/qemu_variant_asic.o`     | 2026-11-24 08:31:59 | `e441e39e` (stale) |
| `build/kernfull.elf`            | 2026-11-24 09:03:02 | `84ebc0f0` |

## Commands run (no `make clean`)
```
cd qemu-port
touch variant/qemu_variant_asic.cpp
make build/kernfull.elf
make isogrub
python3 tools/r1_cp4_ca_stress_test.py          # x4 serial trials
python3 tools/r1_final_verify.py --strict-skip
python3 tools/r1_genuine_retire_test.py
python3 tools/r1_genuine_fullchain_test.py
```

## mtimes / hashes — AFTER
| file | mtime | sha256 (prefix) |
| --- | --- | --- |
| `variant/qemu_variant_asic.cpp` | 2026-11-24 09:08:17.707 | `ef3878be` (unchanged content) |
| `build/qemu_variant_asic.o`     | 2026-11-24 09:08:17.720 | `c6b6c4c3` (correct) |
| `build/kernfull.elf`            | 2026-11-24 09:08:17.749 | `e190801f` (relinked) |

New `.o` mtime > source mtime: **YES**. ISO rebuilt:
`build/symbian-grub.iso` (75,882,496 bytes, GRUB-ISO-OK, 100 programs).

## Per-test results
- **cp4_stress** (`tools/r1_cp4_ca_stress_test.py`, 4 serial runs): **4/4 PASS**
  (`PASS: 3 cycles x 4 clients, 10 clean completions, peer kill recorded,
  no faults, exit ring, channels=0 segs=1`). The brief's
  `r1_cp4_stress_test.py` name does not exist; selfcheck invokes this exact
  script as step `cp4_stress` (`r1_os_selfcheck.py:36`).
- **r1_final_verify.py --strict-skip**: **11/11 PASS** (0 skip, 0 fail),
  OVERALL PASS. Includes selfcheck (incl. cp4_stress), qemu_smoke,
  genuine_retire/efsrv/ewsrv/eshell/fat_io, doom_main/audio/playtest/multikey.
- **r1_genuine_retire_test.py**: **4/4 PASS** (efsrv, ewsrv, estart, euser;
  0 skipped, 0 failed).
- **r1_genuine_fullchain_test.py**: **14/14 PASS** (0 faults, no fallback).

## Boundaries
`kernelhwsrv/` clean (no edits); no commit/push; no `make clean`; build from
`kernelhwsrv/` only; no test or source logic modified.
