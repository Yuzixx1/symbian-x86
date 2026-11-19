# Card 36 — R1 file service host: petran E32 conversion of `portfilesrv.exe`

Status: **complete.** PEi-i386 → E32 conversion performed and verified. No QEMU,
no guest execution, no service registration, no commits/pushes. Date: 2026-10-28.
Port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44`; upstream `kernelhwsrs`
`0c3208650587ac0230aed8a74e9bddb5288023eb`, clean.

Owned path: new `qemu-port/docs/r1_portfilesrv_e32_v1.md` only. All work under
`/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/` (isolated conversion root,
242 GB free on `/home`).

## Dependencies (source evidence)

| Item | Path | Hash |
| --- | --- | --- |
| Card 35 PEi-i386 input | `/home/yuzi/.cache/sybian-r1/r1-portfilesrv-build-v1/bin/portfilesrv.exe` | `e3ca495061b11656d2cb592fe4dd5ece486217ccf4855065111352eba0f75c88` (6240 bytes) |
| petran.exe (rebuilt, D7) | `/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/obj/petran.exe` | `34a3407ff4c0421c466fcfa8520612cbc8944023930430215e71d7da5b2a26e3` |
| petran source | `r1-build-v1/src/build/imgtools/imglib/e32image/` + `bintools/petools/pefile/` | — |
| MinGW GCC 3.4.5 | `r1-build-v1/dl/mingw-symbian-001.zip` | `0c2ac07f9a3cad87e49d04814c37c0dca71143caac0777409f611e8daa07594c` |
| Wine | host system | wine-11.0 (Staging) |

petran.exe was **rebuilt from source** using the D7 workaround from
`r1_build_repro_v0.md` (hand-compile of 20 sources with MinGW 3.4.5 under Wine,
flags: `-fpermissive -w -D__SUPPORT_PE_FILES__ -D__SYMBIAN32__ -D__TOOLS__ …`,
preincludes `-include new -include gcc_mingw_3_4_2.h`, plus epoc32 include
paths). The pre-existing petran binary (sha `b200bf08…`) failed with `line 698`
(`RETURN_FAILURE(KErrNotSupported)` in `f32image.h:698` — UID1 validation
against `KExecutableImageUidValue=0x1000007a`) on this PE input; the rebuilt
binary succeeds. All 20 source files compiled rc=0; link rc=0.

## Conversion

Input (card 35 output, copied as-is, not modified):
- `r1-portfilesrv-e32-v1/input/portfilesrv.exe` — sha256
  `e3ca495061b11656d2cb592fe4dd5ece486217ccf4855065111352eba0f75c88`

Command:
```
wine obj/petran.exe -v -nocompress \
  -uid1 0x1000007a -uid2 0 -uid3 0x2323 \
  -vid 0x70000001 -capability "ProtServ" \
  input/portfilesrv.exe output/portfilesrv
```

`rc=0`. Produced E32 image:
- `r1-portfilesrv-e32-v1/output/portfilesrv` (884 bytes)
- sha256 `d91ebcf28b659e84ab69bbbad9ff08828e72bfdbc2e307966a0e6c9bce6809d0`
- `file`: "Psion Series 5 executable" ✅

No `.sym` sibling produced (petran default behaviour); the PE intermediate is
the input `portfilesrv.exe`, preserved unchanged in `input/`.

## E32 header facts (`petran -v`)

| Field | Value |
| --- | --- |
| Format | EPOC Exe for X86 CPU, header format 2 |
| Compression | not compressed (`KFormatNotCompressed`) |
| Secure ID (SID) | `00000000` |
| Vendor ID (VID) | `70000001` (matches card-28 staged console assets) |
| Capabilities | `00000000 00000100` (ECapabilityProtServ only) |
| UIDs | `1000007a 00000000 00002323` |
| Checksum / header CRC | `edbb8f20` |
| File size | `0x00000374` (884 bytes) |
| Code size | `0x000002ac` |
| Data size | `0x00000000` (no writable data — consistent with first G profile) |
| Min/max heap | `0x1000` / `0x100000` |
| Stack size | `0x2000` |
| Code link address | `0x00400000` |
| Data link address | `0x00000000` |
| Entry point | code offset `0x9c` |
| Import format | PE-style without redundant ordinal lists (`KImageImpFmt_PE2`) |
| DLL ref table count | 1 |
| Exports | 0 (EXE, no exports) |

`petran -dump i` confirmed:
- 2 imports from `euser{000a0000}[100039e5].dll`: ordinals **1221** and **1953**
  (both present/non-ABSENT). Ordinal 1953 = `UserSvr::FsRegisterThread()`
  (referenced by `B2RegisterFsThread`); ordinal 1221 = `_memcpy` (C runtime).
- No EFSRV imports: `portfilesrv.exe` source (`r1_service_host.cpp`) references
  only EUSER (`UserSvr::FsRegisterThread` via `e32svr.h`); no EFSRV symbols are
  directly called. `efsrv.lib` was linked at card-35 link time for the static
  import closure (as required), but the linker omits unreferenced imports — this
  is correct behaviour. EFSRV ordinals (68/93/97/259/263/300) are verified
  present in the built `efsrv.dll` from card 34 but are not imported by this
  host EXE.

`petran -dump h` confirmed: format 2, not compressed, entry point in code
section, relocation bounds valid (1 code reloc at offset 0x9c), no data/BSS
section.

## Import/ordinal audit confirmation

| DLL | Ordinals | Status |
| --- | --- | --- |
| `euser{000a0000}[100039e5].dll` | 1221, 1953 | present, non-ABSENT ✅ |
| `efsrv{000a0000}[100039e4].dll` | — | not imported (no direct EFSRV calls in source); EFSRV ordinals 68/93/97/259/263/300 verified present in built `efsrv.dll` per card 34 |

This matches the card 35 audit: EUSER ordinals 1221/1953 present; static
EUSER/EFSRV import binding at link time; dynamic `ELoadLibrary` stays
`KErrNotSupported` until genuine rollback (card 31/B4).

## D7 workaround

Applied: petran rebuilt from source because the pre-existing binary failed on
this PE input (`line 698` = `RETURN_FAILURE(KErrNotSupported)` at
`f32image.h:698`, UID1 != `KExecutableImageUidValue`). The rebuilt binary
compiles all 20 sources rc=0 and converts successfully. No changes to
`kernelhwsrs/` or the port repo.

## Deliverable scope

- Conversion command, exact argv/rc/warnings: documented above ✅
- petran.exe source path and hash: `obj/petran.exe`, `34a3407f…26e3` ✅
- Input PEi-i386 hash: `e3ca4950…75c88` (card 35 output, unchanged) ✅
- Emitted E32 image SHA-256 and location:
  `d91ebcf28b659e84ab69bbbad9ff08828e72bfdbc2e307966a0e6c9bce6809d0`,
  `r1-portfilesrv-e32-v1/output/portfilesrv` ✅
- E32 header facts: UID3/SID/VID/caps, format, compression, entry, link
  addresses, export/import slots/ABSENT: documented above ✅
- Import audit confirmation (EUSER + EFSRV): documented above ✅
- D7 workaround applied: documented above ✅

## Constraints verified

- No edits to card 31/35 source ✅
- No QEMU, no guest execution, no service registration, no `FsRegisterThread`
  call ✅
- No commits/pushes ✅
- Prior build roots and `/tmp/opencode/stabilization/` preserved untouched ✅
- No system-wide installs; all under `/home/yuzi/.cache/sybian-r1/` ✅
- No leftover processes ✅

## Next

Card 32 (QEMU file-service regression) holds the exclusive QEMU slot and can
now proceed with this E32 image. Card 33 (physical PC smoke test) waits on
card 32.
