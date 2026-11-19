# Card 34 — R1-B toolchain blocker: bx86gcc EFSRV def/import library

Status: **research report; no target build, no QEMU, no guest execution, no upstream edits, no commits/pushes.**
Date: 2026-10-28. Port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44`; upstream `kernelhwsrs` `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean. Owned path: this document only.

## Problem (from card 30 evidence)

Card 30 built `t_cactw.exe` and `fhserver.exe` at build level via Raptor 2.15.3 / MinGW GCC 3.4.5 under Wine / sbs 2.15.3, but reported that the **local bx86gcc EFSRV def/import library is missing**. Import ordinals were verified against `bx86gcc/euseru.def` for EUSER and `eabi/efsrvu.def` for EFSRV, but the actual import-library artifacts for EFSRV were not confirmed present in the local toolchain. Deviations D1–D8 from `r1_build_repro_v0.md` (Raptor source patches D1/D2, fabricated variant header D5, genexec D8) still require audit.

## Task

Audit `/home/yuzi/.cache/sybian-r1/r1-build-v1/` and `r1-build-v1-repeat/` for EFSRV import library / def files; audit pinned providers under `/home/yuzi/.cache/sybian-r1/` for `eabi/efsrvu.def` and corresponding `.lib`; if missing, determine whether new local provisioning is needed or the clean-build artifacts are sufficient; verify no bx86/bx86gcc ordinal mixing; record exact missing/present inputs and recommended build path for card 35 onward.

## Findings — present inputs (source evidence)

Both cold build roots were rebuilt successfully (all stages rc 0) and are preserved under `/home/yuzi/.cache/sybian-r1/`.

### `r1-build-v1` (root `/home/yuzi/.cache/sybian-r1/r1-build-v1`)

| Item | Path | Present | sha256 |
| --- | --- | --- | --- |
| EFSRV import library | `epoc32/release/x86/lib/efsrv.lib` | yes | `a73e7011a1b101f55702d34d828817b84f626a5095b3d040647a6744ba4103b5` |
| EFSRV versioned import lib | `epoc32/release/x86/lib/efsrv{000a0000}.lib` | yes | same as above (identical bytes) |
| EUSER import library | `epoc32/release/x86/lib/euser.lib` | yes | `9db82a37119d7373d44b2138c45700e93b5e6c25bf56d262d4b73fb36aac9eee` |
| EUSER versioned import lib | `epoc32/release/x86/lib/euser{000a0000}.lib` | yes | same as above |
| EFSRV DLL | `epoc32/release/x86/urel/efsrv.dll` | yes | `332d97274a15…` (first 12 hex per r1_build_repeat §4) |
| EUSER DLL | `epoc32/release/x86/urel/euser.dll` | yes | `4851fec4b643…` (first 12 hex) |
| Generated EFSRV def (x86) | `epoc32/build/fileserver/c_a0fe57e26c413907/efsrv_dll/x86/urel/efsrv.def.x86` | yes | `76fdab4ce483994c240c3247bf01aea36b938b2525e9f99ae56eef06bf84f5fa` |
| EFSRV lib.exp | `epoc32/build/fileserver/c_a0fe57e26c413907/efsrv_dll/x86/urel/efsrv.lib.exp` | yes | `da485f041013377072ea1dbf7c76d3b0924847233eb0a2e02212c684a7581056` |
| Upstream source symlink | `kernelhwsrv_ro -> /home/yuzi/Documents/Sybian REAL/kernelhwsrv` | yes | read-only reference |

### `r1-build-v1-repeat` (root `/home/yuzi/.cache/sybian-r1/r1-build-v1-repeat`)

| Item | Path | Present | sha256 |
| --- | --- | --- | --- |
| EFSRV import library | `epoc32/release/x86/lib/efsrv.lib` | yes | `a32f2bdfd9b69d339d3babe58b91f1870141f85131353370d3951ec0ed0901d6` |
| EUSER import library | `epoc32/release/x86/lib/euser.lib` | yes | `9fb37d286020ac3d1b88d25ccbf8c2d1fb7a0135d54156c342d24212d24a8b05` |
| Generated EFSRV def (x86) | `epoc32/build/fileserver/c_b629023a8626e4f4/efsrv_dll/x86/urel/efsrv.def.x86` | yes | `76fdab4ce483994c240c3247bf01aea36b938b2525e9f99ae56eef06bf84f5fa` (identical to v1 — def content is root-path independent) |
| EFSRV lib.exp | `epoc32/build/fileserver/c_b629023a8626e4f4/efsrv_dll/x86/urel/efsrv.lib.exp` | yes | `206d62e7900dd674e7a11840ecbdf4da559b991430d08ed2eb9c7da329859504` (differs from v1 — root-derived names) |

### Def-content verification (r1-build-v1 `efsrv.def.x86`, 423 lines)

The generated def uses **Itanium/GCC mangled names** (not MSVC decorated names), confirming it was produced from the EABI def via the Raptor `eabi` fallback, transformed by `sed` (`_ZT[IV]...NONAME` → `... ABSENT`), then fed to `dlltool -m i386`:

- `RFs::Connect(int)` `_ZN3RFs7ConnectEi` @ **68** NONAME
- `RFile::Open(RFs&, TDesC16 const&, unsigned int)` `_ZN5RFile4OpenER3RFsRK7TDesC16j` @ **93** NONAME
- `RFile::Close()` `_ZN5RFile5CloseEv` @ **300** NONAME
- `RFile::Write(TDesC8 const&)` @ 97, `RFile::Read(int,TDes8&)` @ 259, `RFile::Seek` @ 263 (per r1_sdk_route_research §26)

These match the ordinals recorded in r1_build_repro §100 and r1_sdk_route_research §26-27. 421 ordinals total, 20 `ABSENT` lines (19 `_ZTI`/`_ZTV` + 1 pre-existing `RRawDisk::Caps` at ordinal 358).

## Findings — missing inputs

| Item | Searched | Present? |
| --- | --- | --- |
| `bx86gcc/efsrvu.def` (GCC x86 EFSRV def) | upstream `kernelhwsrv/userlibandfileserver/fileserver/` (dirs: `bmarm,bwins,bx86,eabi` — **no `bx86gcc`**), both build roots, entire `/home/yuzi/.cache/sybian-r1/` | **no — does not exist anywhere** |
| Source `eabi/efsrvu.def` | upstream `kernelhwsrv/userlibandfileserver/fileserver/eabi/efsrvu.def` | **yes** (upstream source, read-only) |
| Any other provider directory | `/home/yuzi/.cache/sybian-r1/` (all dirs listed) | **none beyond the two build roots** |

### The "missing bx86gcc EFSRV def" is by design, not a blocker

Per `r1_sdk_route_research_v0.md` (§7, §26-31), Raptor resolves x86 `.def` files via `PlatformDefaultDefFileDir['X86'] = ['bx86gcc','eabi']` (`raptor_meta.py:43-50`, `resolveDefFile`): it uses `../bx86gcc/<target>u.def` if it exists, otherwise `../eabi/<target>u.def` flagged `RESOLVED_DEFFILE_SECONDARY`. **Upstream provides no `bx86gcc/efsrvu.def`; EFSRV intentionally falls back to `eabi/efsrvu.def`.** EUSER has its own `bx86gcc/euseru.def` (used directly, no fallback).

The bx86 (MSVC) `efsrvu.def` (`Open` @ 140, etc.) is an unrelated ordinal space and must **never** be spliced into a GCC build (r1_toolchain_research §25; r1_abi_contract §8).

Conclusion: the import library `efsrv.lib` exists, was built successfully (rc 0), and links against the EABI-derived def. The card-34 "missing def/import library" premise is incorrect for the import library (it exists) and correct-but-by-design for the `bx86gcc` def (upstream doesn't ship one). **No new local provisioning is needed for linking proof apps.** The clean-build artifacts are sufficient.

## D1–D8 audit status (from r1_build_repro_v0 §31-33, r1_build_repeat_v1 §28-30)

All eight deviations were exercised in both cold builds; none touched `kernelhwsrv/` or the port repo.

| ID | Deviation | Status | Disposition |
| --- | --- | --- | --- |
| D1 | Raptor `gethost.sh` patch (host dir from `libc_modern`/`uname -m`) | Applied, both builds rc 0 | **Accepted** — distro incompatibility; needs coordinator review to sanction |
| D2 | talon `process.c` `poll()` `EINTR` retry | Applied, both builds rc 0 | **Accepted** — genuine upstream bug exposed by modern glibc/kernel; needs coordinator review |
| D3 | make/talon/bash flags (`-D__alloca=alloca`, `-std=gnu89`, `-fcommon`, etc.) | Applied, both builds rc 0 | **Accepted** — compiler-flag shims, no source edit |
| D4 | cpp/sed banner shims (GCC 16 cpp used for metadata, not 3.x/4.x) | Applied, both builds rc 0 | **Accepted** — banner-only wrappers; means preprocessing uses host cpp 16.2.1, not upstream 3.x/4.x |
| D5 | Fabricated `variant.cfg` + `include/variant/scratch_r1.hrh` (invented OS variant stand-in) | Applied, both builds rc 0 | **Needs coordinator decision** — no feature macros/SYMBIAN_* settings from a real kit; behavior depending on variant macros not represented |
| D6 | `kernelhwsrv_ro` symlink (space-in-path workaround for cpp) | Applied, both builds rc 0 | **Accepted** — path workaround only, read-only reference |
| D7 | `petran` compiled by hand (not via Raptor `tools2`) | Applied, both builds rc 0 | **Accepted** — tool works but is not identical to a Raptor-built one |
| D8 | `genexec.pl` run by hand to produce real exec headers | Applied, both builds rc 0 | **Accepted** — coordinator decision needed on official flow (D8 in r1_build_repro) |

The repeat build (r1_build_repeat_v1) confirmed: fresh Wine prefix, fresh `epoc32`, same reviewed v1 inputs, all stages rc 0 on first attempt. Raw hashes differ from v1 (explained by root-path length, E32 header timestamps/CRCs, root-derived names in import libs/maps/.sym); after the documented normalization masks, exports/imports/headers/RFile64 bodies are identical (0 residual for most, explained ripple for euser/t_cactw/fhserver).

## Ordinal-mixing verification

- EUSER def: `bx86gcc/euseru.def` (GCC/Itanium names; `CActiveScheduler::Start` @ 426, `Install` @ 428). Used directly, no fallback.
- EFSRV def: `eabi/efsrvu.def` via Raptor fallback → generated `efsrv.def.x86` (Itanium names; `RFs::Connect` @ 68, `RFile::Open` @ 93, `Close` @ 300, `Write` @ 97, `Read` @ 259, `Seek` @ 263).
- bx86 MSVC defs (`bx86/euseru.def`, `bx86/efsrvu.def`) were **not** used; ordinal spaces differ (e.g. 362 of 2496 shared EUSER names have different ordinals between bx86gcc and eabi).
- **No bx86/bx86gcc ordinal mixing:** confirmed by comparing def ordinals, `petran -dump e` export slots, and `petran -dump i` imports against the matching built defs (all present/non-ABSENT, per r1_build_repro §100-101 and r1_build_repeat §6).

## Recommended build path for card 35 onward

For linking unchanged proof apps `t_cactw.exe` and `fhserver.exe` (card 30 selection, pending owner approval):

1. **Use the existing clean-build artifacts as the import-library source.** Both roots contain `epoc32/release/x86/lib/euser.lib` and `efsrv.lib` (plus versioned `{000a0000}` copies), built rc 0 from unchanged upstream sources. Copy (do not regenerate) these `.lib` files into the card-35 build environment.
2. **Use the matching built DLLs** (`euser.dll`, `efsrv.dll`) for export-map inspection (`petran -dump e`) and import verification (`petran -dump i` on the proof EXEs), per r1_build_repro acceptance tests §217-223.
3. **Def files:** EUSER via `bx86gcc/euseru.def` (upstream source, read-only); EFSRV via generated `efsrv.def.x86` (build artifact) or the upstream `eabi/efsrvu.def` with the documented Raptor `sed` transform. Do **not** create a synthetic `bx86gcc/efsrvu.def`.
4. **Toolchain:** Raptor `sbsv2` at build-repo commit `f458a4ce83f74d603362fe6b71eaa647ccc62fee`, MinGW GCC 3.4.5 (`mingw-symbian-001.zip`, sha256 `0c2ac07f9a3cad87e49d04814c37c0dca71143caac0777409f611e8daa07594c`) under Wine, Python 2.7.18, GNU make 3.81, bash 4.0, with D1–D4 shims and D6 symlink. D5 variant stand-in and D7/D8 handling require coordinator decisions (D5, D8).
5. **Build commands** (from r1_build_repro §195): EXPORT passes for `kernel/eka` and `fileserver/group`; genexec headers; `euser`; `hal`; `efsrv` (`-p efsrv.mmp`); `t_cactw` (`-p t_cactw.mmp`); `fhserver` (`-p fhserver.mmp`), all `-c x86_urel` / `-c x86_urel.test`.
6. **No target build, no QEMU, no guest execution** at this stage; R1-STARTUP/R1-WIRE remain gated on ABI/v1.

## Coordinator decisions needed

1. Sanction deviations D1 (Raptor `gethost.sh` patch) and D2 (talon `EINTR` patch) as permanent toolchain shims.
2. Decide on D5 (fabricated variant header): accept the stand-in variant or provision a real OS variant with `SYMBIAN_*` settings.
3. Decide on D8 (genexec flow): official Raptor `genexec.mk`/abld flow vs hand-run `genexec.pl`.
4. Confirm that using the EABI-derived `efsrv.def.x86` (Raptor fallback) rather than a nonexistent `bx86gcc/efsrvu.def` is the sanctioned EFSRR def route — i.e., that the "missing bx86gcc EFSRV def" is resolved as by-design, not as a provisioning task.
5. Confirm reuse of clean-build import libraries (`efsrv.lib`, `euser.lib`) for card-35 proof-app linking, with hash verification against the values above.

## Deliverable scope

- Present/missing EFSRV def/import library inputs and hashes: recorded above.
- D1–D8 audit status: accepted/needs-coordinator-decision as above.
- Recommended build path for `t_cactw`/`fhserver` linking: reuse clean-build artifacts; no new provisioning.
- Coordinator decisions: D1/D2 sanctioning, D5 variant, D8 genexec, EFSRR def route confirmation.

Source evidence vs proposals: all hashes and paths are from direct filesystem inspection of the two preserved build roots and upstream source; the "by-design def fallback" conclusion is sourced from `r1_sdk_route_research_v0.md` (Raptor `resolveDefFile` behavior) and verified against the generated `efsrv.def.x86` artifact. No upstream files edited; no commits/pushes.
