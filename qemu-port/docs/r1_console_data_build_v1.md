# R1-CONSOLE-DATA-BUILD v1 — build completed, exports rejected

2026-10-23. Card 13. **Both recipes returned rc0, but emitted exports FAIL the
genuine contracts. Diagnostic artifacts only; not console bring-up inputs.**
Owned path: this report only. No submitted source/MMP edits, upstream patches,
NOSTRICTDEF, target retry, guest execution, QEMU, hardware, commit or push.
QEMU-ready: **none**.

## 1. Result

One isolated serial incremental sequence built HAL (four translation units) and
EKDATA (one). Each reported **0 errors, 2 warnings**: missing resolved DEFFILE
and known missing `checklib`. Compiler/link stderr was empty. No compile/link
failure occurred, so no first-error retry or workaround was attempted.

However, Raptor looked for `haluu.def` and `ekdatau.def` instead of the supplied
genuine definitions, then linked auto-exports. HAL ordinal 1 is the ignore-export
stub, not HAL::Get; EKDATA ordinals 1–3 are Settings/ConvTable/FuncTable, not
ConvTable/FuncTable/Settings. **Successful link is not compatible output.**

Basic CRC/structure/import audits pass. Both DLLs have **zero initialized data
and zero BSS**. Compiled HAL arrays contain exactly eight immutable valid
attributes and zero implementation pointers. This establishes emission only,
not HAL execution, key translation, console acceptance or SDK equivalence.

## 2. Provenance, allocation and preservation

- Read latest Common/card 13, reconciliation, board, roadmap and submitted
  console README. Port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44` plus current
  dirty main inputs; no old-HEAD-only source assumption.
- Detached report/helper worktree `/tmp/opencode/r1-console-data-build` at that
  HEAD. Actual build source copied from current main `qemu-port/r1-console`.
- Upstream `kernelhwsrv` HEAD `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean
  before/after. Private `workspace/kernelhwsrv` is a read-only-use reference
  symlink to main upstream; no upstream files copied or modified. This preserves
  MMP relative layout while keeping generated objects private.
- New root **`/home/yuzi/.cache/sybian-r1/r1-console-data-build-v1`** (`$R`)
  absent including as symlink before provisioning. Home free 269,685,735,424 bytes;
  provisioning required >4 GiB. `quota -s` reported tmpfs quotas only: busy tmpfs
  5453/6297 MiB, other 567/6297 MiB; no reported home quota. SDK/prefix/build/tmp
  home-backed. A later report write in detached tmpfs worktree failed EDQUOT
  (left an empty scratch report); this final report was written in owned main
  home-backed path. No deletion/move of prior roots to recover space.
- Reuse origin `/home/yuzi/.cache/sybian-r1/r1-build-v1`. **7,287 regular copied
  SDK/tool/Raptor files** source/copy SHA-verified, originals rehashed unchanged
  after build. No old `epoc32/build` or target objects copied. Reused reviewed
  SDK headers/import/startup libraries/release inputs, tools and Raptor subtree.
- Build-tools source identity checked: HEAD
  `f458a4ce83f74d603362fe6b71eaa647ccc62fee`, reviewed D1/D2 live diff hash
  `fd504a6009d3a70039bece577b0f2a3082987d56b415f4209a93ba50f289e50d`.
- Fresh private Wine prefix (`wineboot -u` rc0), private SDK/tmp/tool copies;
  M: maps to this root's `tools/mingw`. Env and 12 x86 tool wrappers plus petran
  wrapper changed old-root strings to new-root only. Two Raptor Python/make
  links repointed privately. No tool-byte/source patch, banner shim, prefix
  sharing or mid-build mapping change. Reviewed hybrid-toolchain qualifications
  remain; not official-kit equivalence or a new cold SDK build.
- All nine submitted files copied unchanged to
  `$R/workspace/qemu-port/r1-console`, hashes in `logs/source_hashes.sha256`.
  Only private `hal/generated/` added. Main source/README untouched.
- Complete copied inputs: `logs/reused_inputs.json`; wrapper/link substitutions:
  `logs/provision.json`; end-state host/Wine/git/quota/disk:
  `logs/host_snapshot.json`. Provision helper retained in detached worktree as
  `provision_console.py`; previous fixture, clean, repeat and other roots preserved.

### Live-verified configuration pins

Named configuration **port-x86-rfile64-v1**; actual target `x86_urel`.

| Input | SHA-256 |
|---|---|
| EUSER | `4851fec4b6439bf5263e6f10f7f3010828f84df55df050c710f57077f2dd017b` |
| EFSRV (copied, not imported here) | `332d97274a151bb17a18112e728684b98c8462fe8723bfadb1de0d471e372b6d` |
| petran.exe | `b200bf0878679e6c7d6c20e5a78cb2bb93f16d5b21790a2b66c7dd04a53ec155` |
| platform_paths.hrh | `8a8ebdc3f40804d630def1af4c0c3a68568a79d13eb08d0661d9ba252dd96f31` |
| port_x86_rfile64_v1.hrh | `500889884b3063c1b0712c000d2c62f55e70774cbfda41b51b82f343be277106` |
| variant.cfg | `2c505b4bb43e4b74e362589ce78f866053e4e9d8283688e47cca321c58e08cd0` |

Full per-file hashes include compiler, import/startup libraries, Python and make.

## 3. Exact attempt and inspection evidence

Private `harness/attempt.py` archives argv/cwd/rc/stdout/stderr under
`logs/<step>.{cmd.json,out,err}` and exits on the first nonzero step. Generator
steps `generate_config`, `generate_values`; build steps `build_hal`,
`build_keymap`. Raptor XML retains expanded compiler/link/Petran commands and
all recipe statuses **attempt 1**. No target failure or retry.

With U=`<main>/kernelhwsrv/halservices/hal`, cwd private `hal/`:

```sh
perl "$U/halcfg.pl" -x "$U/inc/hal_data.h" config.hcf generated/config.cpp
perl "$U/halcfg.pl" "$U/inc/hal_data.h" values.hda generated/values.cpp
```

Both genuine generator calls rc0. Source `$R/env.sh`; for C=private hal then
keymap, M=hal.mmp then ekdata.mmp, exactly one call each:

```sh
bash "$SBS_HOME/bin/sbs" --toolcheck=off -d -j 1 \
  -f "$R/logs/build_<component>.xml" -b "$C/bld.inf" -c x86_urel -p "$M"
```

HAL stdout 7 seconds, keymap 2 seconds. `-O2 -Wall -fno-rtti -fno-exceptions
-fcheck-new`, `__X86__`, genuine gcc.h, pinned PRODUCT_INCLUDE. Genuine edll.lib /
edllstub.lib, `-nostdlib`, `--entry=__E32Dll`, copied genuine euser.lib. No compiler
warnings. Missing checklib neither fabricated nor suppressed.

Host Petran `-v`, `-dump i`, `-dump e` on both outputs all rc0; objdump `-p` and
`-tC` on both PE `.dll.sym` completed. Dumps/argv/status retained in logs.
A later batch inspecting reused EUSER timed out at shell's 120-second limit
after its header dump, after HAL/EKDATA inspection had completed. No target
rebuild. Provider ordinal audit uses the **prior fixture card's EUSER export
dump**, after verifying identical provider hash; explicitly inherited export
evidence, not a fresh provider-export inspection. New IAT checks use actual bytes.

Submitted host `check_source.py --upstream <main>/kernelhwsrv --port
<main>/qemu-port` rc0 (constants/generator/key rows/bounds/duplicates). Its printed
UNBUILT describes that noncompiling checker only. Initial omitted-argument
invocation rc2 was host-only. Independent audit corrected its initial hardcoded
array-length assumption to 121 including final non-comma row; no source/build
change. Results `logs/audit.json`, `generated_table_audit.json`.

## 4. Actual security and structure

| Field | HAL | EKDATA |
|---|---|---|
| Bytes | 10,952 (`0x2ac8`) | 2,784 (`0xae0`) |
| UID3 / SID | `100039e8` | `100039e0` |
| UID checksum | `ea0570db` | `ea05f972` |
| Header CRC | `39c53b90` | `ff9744f4` |
| Flags | `22000001` | `22000101` |
| Code offset / size | `0x9c / 0x2900` | `0x9c / 0x9ac` |
| Code base / entry | `0x10000000 / 0x264` | `0x10000000 / 0x264` |
| Initialized data / BSS | **0 / 0** | **0 / 0** |
| Data base / data reloc offset | 0 / 0 | 0 / 0 |
| Exports count / file offset | 4 / `0x298c` | 4 / `0xa38` |
| Import offset / DLL count | `0x299c` / 1 | `0xa48` / 1 |
| Code reloc offset / entries | `0x29c8` / 112 | `0xa74` / 46 |
| Reloc payload / kinds | `0xf8`, all `0x3000` | `0x64`, all `0x3000` |
| Code paging | default | unpaged |

Both UID1 DLL `10000079`, UID2 `1000008d`, VID `70000001`, X86, version 10.0,
uncompressed V/PE2, not debuggable/SMP-safe. Petran prints capabilities high-first
`00000000 000fffff`; raw little-endian capability array is **[000fffff,00000000]**.
All 20 capability bits, not a privilege grant to the calling process.

`harness/audit.py` independently checks UID CRC16 pair, header CRC32 (zero
initial CRC, c90fdaa2 header-word initializer), imports/names/IAT/terminator,
provider ordinals, relocation block extent/count/kind/unique in-code targets
and export bounds. Streams end at EOF. Structural checks pass; bounded audit
is not a full loader validator. Header rules/layout:
`kernelhwsrv/userlibandfileserver/fileserver/inc/f32image.h:161-218,437,650-666`.

**No writable-data loader extension is required for these actual two images.**
Their const pointer arrays are in code and require ordinary relocation, not
per-process writable DLL state. Genuine HAL property/heap paths still require
EUSER services when called. Corrected strict-export builds must be re-audited,
especially HAL data export; no blanket future zero-data claim and no library
changed to force zero data.

## 5. Compiled generated values

Compiled PE symbol offsets were used to compare every InitialValue, Properties
and Implementation entry against generator output, not merely source text.
121 entries, all implementation pointers zero; exactly these properties are
**EValid=1, not ESettable**. All other properties and initial values zero.

| Index | Attribute | Value |
|---|---|---|
| 8 | ECPU | 2 (x86) |
| 10 | ECPUABI | 6 (x86) |
| 14 | ESystemTickPeriod | 15625 |
| 18 | EMemoryPageSize | 4096 |
| 69 | EKeyboardIndex | **0** |
| 84 | EDisplayMode | 0 |
| 91 | EDisplayNumberOfScreens | 1 |
| 92 | ENanoTickPeriod | 1000 |

Full 121-row compiled ledger `logs/generated_table_audit.json`; generated files
retained in private `hal/generated`. This proves emission, not runtime return.
Immutable KeyboardIndex selects InitialValue in genuine
`halservices/hal/src/hal_main.cpp:186-203`; P&S code remains linked (`:26-67`).

EKDATA ordinary-key const tables compiled without diagnostics. Submitted
`keymap.cpp:73-148` defines fixed/symbol/letter/modifier/conversion/state data;
`:153-178` defines three export APIs. Host source checks pass. No translation,
raw PS/2, event, display or genuine EKTRAN runtime behavior confirmed.

## 6. Actual imports and incompatible exports

Each imports only **euser{000a0000}[100039e5].dll**, version 10.0:

| Image | Ordinal / API |
|---|---|
| HAL | 644 User::Free; 646 User::Alloc; 650 User::Panic; 819 TPtr8 ctor; 1213 UserSvr::HalFunction(4 args); 2005 HalFunction(5 args); 1356 TVersion ctor; 1438 RProperty::Get; 1444 RProperty::Set; 1981 RProperty::Define; 1473 TBufBase8 ctor |
| EKDATA | 644 User::Free; 646 User::Alloc (genuine startup references) |

All **13/13 image-specific imports** present/nonzero/non-ABSENT in pinned
EUSER, checked against `kernel/eka/bx86gcc/euseru.def`; full symbol/export-offset
ledger `logs/audit.json`. Immutable profile does not remove static P&S binding.

Actual E32 export offsets matched to PE ordinal/name tables:

| DLL / ordinal | Actual (code-relative offset) | Genuine requirement |
|---|---|---|
| HAL 1 | __ignore_this_export (`0`) | HAL::Get(attribute,value&) |
| HAL 2 | HAL::Get(attribute,value&) (`0x6c0`) | HAL::Set(attribute,value) |
| HAL 3 | HAL::Set(attribute,value) (`0x590`) | HAL::GetAll |
| HAL 4 | HAL::GetAll (`0x6f0`) | HAL::Get(device,attribute,value&) |
| HAL 5 / 6 | absent, out of table | HAL::Set(device,attribute,value) / InitialValue data |
| EKDATA 1 | KeyDataSettings (`0x370`) | KeyDataConvTable |
| EKDATA 2 | KeyDataConvTable (`0x3e0`) | KeyDataFuncTable |
| EKDATA 3 | KeyDataFuncTable (`0x3a0`) | KeyDataSettings |
| EKDATA 4 | __ignore_this_export (`0`) | no fourth export |

Genuine requirements: `halservices/hal/eabi/halu.def:2-7` and
`kernel/eka/eabi/ekdata.def:2-4`. Zero here is code-start ignore stub, **not an
absent-hole conclusion**. PE name tables disambiguate it.

### Root cause and minimal review proposal — not applied

Copied Raptor `python/raptor_meta.py:2468-2497` strips .def then appends **u**
unless NOSTRICTDEF; `:2277-2285` warns and empties missing resolved defs.
HAL MMP:12 supplies `halu.def`, so search is **haluu.def**; EKDATA MMP:11
supplies `ekdata.def`, so search is **ekdatau.def**. Warnings in XML and direct
auto-export link corroborate this (HAL XML:456,571-577). No genuine frozen list
reached link. Do not waive this as merely the old checklib warning.

Propose source-owner/coordinator review, not implementation here:

1. HAL strict stem `.../eabi/hal.def` resolves existing genuine `halu.def`.
   Necessary one-line recipe correction, **not proven sufficient** for a later
   strict link. No NOSTRICTDEF.
2. EKDATA needs a port-owned byte-identical staged copy of genuine ekdata.def
   named **ekdatau.def**, with MMP local DEFFILE stem ekdata.def, or explicitly
   reviewed equivalent staging. Do not modify upstream, invent ordinals or
   preserve auto-exports.
3. Next authorized strict HAL build must force and audit all six symbols.
   Device overloads are in current PE symbols but not auto-exported. Genuine
   `hal_int.h:27-33` empties EXPORT_D on X86; InitialValue is linked const data
   but not auto-exported. Genuine eabi def says **DATA 428**, while generated
   array is 121*4=484 bytes: flag for owner review, do not silently rewrite
   metadata or claim correct emitted data export today.

No source fix applied or retry performed. Correct recipe/strict-export review
must precede considering these components usable.

## 7. Artifacts, aliases and coordinator handoff

| Rejected diagnostic image | SHA-256 |
|---|---|
| `epoc32/release/x86/urel/hal.dll` | `6ce17e2abbfd0784646501d537ff36c70438cb06d760b9ca7b494812475669a9` |
| `epoc32/release/x86/urel/ekdata.dll` | `d7141cc730f5060bed7ddd905175d7d00629da54fc84318649790952973af539` |

`$R/staged/sys/bin/HAL.DLL` matches HAL. **EKDATA.DLL and EKDATA.00 are actual
byte-identical staged copies**, not symlinks, same EKDATA hash above; inert and
never loaded. Aliases do not repair wrong ordinals. Naming/FS selection remains
future integration work.

`logs/final_hashes.sha256` inventories submitted/generated files, helpers/logs,
release images/maps/symbols and aliases. `audit.json` preservation section:
7,287 original files unchanged, private wrapper root substitutions only,
submitted source unchanged, upstream clean. Existing roots untouched by builds.

- **Card/base/config:** R1-CONSOLE-DATA-BUILD, §2 identities, no ABI freeze.
- **Changed repo path:** this new report only; source-owner README unmodified.
- **Commands/results:** one HAL+EKDATA sequence rc0, genuine generator/source
  checker rc0; structure/import/table PASS, **genuine export contract FAIL**.
- **Blocker/fix:** strict DEFFILE resolution/staging proposal §6, no compile/link
  error or authorized corrective retry.
- **Writable DLL data:** none in these outputs; recheck after export correction.
- **Runtime/manual:** no guest/QEMU/hardware, no manual procedure edits; HAL,
  translation, security, input/display and cumulative service memory unproved.
- **Next owner:** coordinator/source owner reviews exact defs, recipe and HAL
  data-export metadata, then separately authorizes corrected isolated build.
  These hashes are diagnostic-only, not console acceptance evidence.
