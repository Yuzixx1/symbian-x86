# Console data v2 — coordinator recovery note

2026-10-23. **Not the missing original card-17 report.** Original prose handoff
could not be recovered; do not invent its contents. Source and build evidence
survive at `/home/yuzi/.cache/sybian-r1/r1-console-data-build-v2/`.

Coordinator read `logs/strict_ordinal_audit.json` and preserved EKDATA MMP and
verified supplied SHA-256 values for six existing files, without rerunning audit:

| File under surviving root | SHA-256 |
| --- | --- |
| logs/strict_ordinal_audit.json | 4a760453be1186040ac99429317ccdad37ab2c07434d7796231bbef297db8907 |
| logs/generated_table_audit.json | ac548d65d74f8277329f2edb0bb973222048bb31f13acf530051a7cb1c94dff0 |
| logs/audit.json | 18c95f7a848473dbd9e7af2af09786da3b1a298cbcd8eae3c30c198fc64a1997 |
| staged/sys/bin/HAL.DLL | 4a95dbe01d201b8a0b347e0e5c4cc39f798a7d89d6e163d7f63d71d3a04cb150 |
| staged/sys/bin/EKDATA.DLL | 812af35372d2fc6a88d9671638b411b6f2694272eb15d31e1e808ce04b057e16 |
| staged/sys/bin/EKDATA.00 | 812af35372d2fc6a88d9671638b411b6f2694272eb15d31e1e808ce04b057e16 |

Historical audit records HAL exactly six correct ordinal mappings and full
484-byte InitialValue comparison; prepdef nodatasizes removes only size metadata.
EKDATA records three mismatched mappings and wrong export count, so remains
rejected. Hash verification is not independent audit of those historical claims,
nor runtime/console acceptance. No source copy/build/test repeated here.

Source baseline survives under `workspace/qemu-port/r1-console/`: HAL strict
stem hal.def, bare EKDATA stem ekdata.def, local genuine ekdatau.def, checker and
README. Card 19 supplied exact hashes. Worker must verify them before adopting
and preserve current main differences. No need to recover prose before progress.

**Coordinator waives original-prose recovery prerequisite.** Resume existing
card 19 at resolver inspection, verify surviving source against submitted hashes,
then authorized explicit `./ekdata.def` fix and single EKDATA-only v3 build.
No HAL rebuild, extra search for lost report, retry, NOSTRICTDEF, invented
ordinals, guest execution or claim of a recovered original handoff.
