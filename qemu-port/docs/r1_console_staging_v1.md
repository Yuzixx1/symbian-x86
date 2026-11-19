# R1-CONSOLE-STAGING v1 (card 26)

## Scope
Host/build only. No rebuild, link, QEMU run, or console-closure claim. Private
local asset staging with no redistribution.

## Asset source and verification
Accepted evidence: `/home/yuzi/.cache/sybian-r1/r1-console-recipe-closure-v1/logs/hal_ekdata_evidence.json`
(card 24, status PASS). Staged assets verified byte-identical:
- `HAL.DLL` SHA-256 `4a95dbe01d201b8a0b347e0e5c4cc39f798a7d89d6e163d7f63d71d3a04cb150`, size 10960.
- `EKDATA.DLL` and `EKDATA.00` SHA-256
  `2e11e7287325a7829a28b7cb510c5605390e57b1953a0b16704b8210c4aced0f`, size 2780
  (aliases byte-identical).

Private asset directory (no redistribution):
`/home/yuzi/.cache/sybian-r1/r1-console-staging-v1/sys/bin/` containing
`HAL.DLL`, `EKDATA.DLL`, `EKDATA.00`. Default builds remain asset-free.

## Exact sysbin placement
- `sys/bin/HAL.DLL`
- `sys/bin/EKDATA.DLL`
- `sys/bin/EKDATA.00`

## Immutable image requirements
- HAL.DLL and EKDATA images are immutable: `data_size=0`, `bss=0`,
  `data_reloc=0`; no writable DLL data.
- HAL exports six ordinals (HAL::Get/Set/GetAll/Get/Set + HalInternal::InitialValue).
- EKDATA exports three ordinals (KeyDataConvTable/KeyDataFuncTable/KeyDataSettings).
- UID/SID/capabilities:
  - HAL: UID1 `0x10000079`, UID2 `0x1000008d`, UID3/SID `0x100039e8`,
    VENDORID `0x70000001`, caps0 `0x000fffff`, caps1 `0`.
  - EKDATA: UID1 `0x10000079`, UID2 `0x1000008d`, UID3/SID `0x100039e0`,
    caps as declared by the upstream def.
- Zero-data requirement: every HAL attribute outside the eight valid ones
  (ECPU, ECPUABI, ESystemTickPeriod, ENanoTickPeriod, EMemoryPageSize,
  EKeyboardIndex, EDisplayMode, EDisplayNumberOfScreens) is zero; HAL
  InitialValue is 484 bytes with the adopted profile
  (ECPU=2 x86, ECPUABI=6, ESystemTickPeriod=15625, ENanoTickPeriod=1000,
  EMemoryPageSize=4096). EKDATA carries no data/bss.

## Future G manifest
The future G manifest must reference the exact hashes and sizes above, the
immutable-image and zero-data requirements, and the sysbin paths. No source
rewrite or NOSTRICTDEF. Manual/hardware remains pending.
