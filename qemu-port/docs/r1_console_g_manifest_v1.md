# R1-CONSOLE-G-MANIFEST v1 (card 28)

## Scope
Host/build only. Wire accepted HAL/EKDATA v3 hashes into `tools/stage_r1_g.py`
so asset staging copies byte-identical `HAL.DLL`, `EKDATA.DLL` and `EKDATA.00`
from the verified card-26 staging directory. Default builds remain asset-free;
no redistribution permission. No rebuild/link/QEMU/console-closure claim.

## Hash wiring
`tools/stage_r1_g.py` `PINS` now includes the accepted card-24/26 console assets:

- `HAL.DLL` = `4a95dbe01d201b8a0b347e0e5c4cc39f798a7d89d6e163d7f63d71d3a04cb150`
  (size 10960, UID3/SID `0x100039e8`, six ordinals, no data/BSS/reloc)
- `EKDATA.DLL` = `2e11e7287325a7829a28b7cb510c5605390e57b1953a0b16704b8210c4aced0f`
  (size 2780, UID3/SID `0x100039e0`, three ordinals, no data/BSS/reloc)
- `EKDATA.00` = same hash as `EKDATA.DLL` (byte-identical alias)

Asset staging source: `/home/yuzi/.cache/sybian-r1/r1-console-staging-v1/sys/bin/`
(verified card-26 output). Staging verifies each pinned hash before writing;
any mismatch aborts with `hash mismatch: <name>`. Generated `r1_g_assets.inc`
and `provenance.json` record hashes and sources; `verify()` regenerates the
canonical source from decoded pinned bytes and rejects any noncanonical edit.

## Sysbin placement
- `sys/bin/HAL.DLL`
- `sys/bin/EKDATA.DLL`
- `sys/bin/EKDATA.00`

## Immutable image / zero-data requirements (for future G manifest)
- HAL.DLL and EKDATA.DLL/EKDATA.00 are immutable: `data_size=0`, `bss=0`,
  `data_reloc=0`; no writable DLL data.
- HAL exports six ordinals; EKDATA exports three ordinals.
- HAL UID1 `0x10000079`, UID2 `0x1000008d`, UID3/SID `0x100039e8`,
  VENDORID `0x70000001`, caps0 `0x000fffff`, caps1 `0`.
- EKDATA UID1 `0x10000079`, UID2 `0x1000008d`, UID3/SID `0x100039e0`.
- Every HAL attribute outside the eight valid ones (ECPU, ECPUABI,
  ESystemTickPeriod, ENanoTickPeriod, EMemoryPageSize, EKeyboardIndex,
  EDisplayMode, EDisplayNumberOfScreens) is zero; HAL InitialValue is 484 bytes
  with the adopted profile. EKDATA carries no data/bss.
- Default builds require no console assets and rebuild all existing ISOs.

## Evidence
Verified against card 24/26 `hal_ekdata_evidence.json` (status PASS) before
writing. No mismatch; no HAL/EKDATA logic changes.