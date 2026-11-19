# R1-ARTIFACT-AUDIT v0 — independent audit of the R1-BUILD-REPRO artifacts and deviations D1–D8

Status: **research report, NOT a frozen ABI.** Date 2026-10-21.
Port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44`, branch `main`, dirty tree
(59 status lines before this report: 35 tracked-modified, 24 untracked).
Upstream `kernelhwsrv/` HEAD `0c3208650587ac0230aed8a74e9bddb5288023eb`, `git status --short` = 0 lines.
Build-tools repo `/tmp/opencode/r1-build-repro/src/build` at `f458a4ce83f74d603362fe6b71eaa647ccc62fee` (a **commit**),
only `gethost.sh` and `talon/process.c` modified; `bintools/` and `imgtools/` are byte-identical to that commit (`git diff --quiet HEAD`).
Audit scratch: `/tmp/opencode/r1-artifact-audit/{logs,work,out}`. No E32 image was executed, no Wine prefix of another worker was touched
(a private-prefix attempt failed on the 1.6 GB tmpfs quota and was removed; no Wine run was needed afterwards).
All 4 images + `.sym` + `.lib` + `petran.exe` hashes were identical before and after the audit (`logs/hashes_before.txt` = `hashes_after.txt`).
Nothing was run under QEMU or on hardware.

## Result in one paragraph

The four built images are **structurally valid E32 PE2-import images**: all 25 header checks I mirror from
`E32ImageHeader::ValidateHeader` pass (UID checksum, `EPOC` signature, header CRC, flags, layout bounds), the export
directory/bitmap/absent marker, the EUSER/EFSRV section packing, the `.lib` thunks and every app import ordinal agree with the
upstream `.def` files and with the actual built providers. That is an observation about these bytes. It does **not** establish
runtime behaviour, official-kit equivalence or reproducibility. Two findings change how the build should be treated:
(1) **D5 is behaviour-affecting**: with the fabricated variant, `RFile64::*` in `efsrv.dll` compiles to `Panic(ENotImplemented); return KErrNotSupported` stubs;
(2) **the build is not bit-reproducible** (header timestamp, PE timestamps, and build-prefix strings embedded in `euser.dll`).
D1–D3, D6 are host-only; D4, D7, D8 need review; D5 must not be used as an official-equivalence basis.

## What "independent" means here (and does not)

- Parser: `work/e32audit.py` (sha256 `c70121006b311c58…`), written from the byte layout in `epoc32/include/platform/f32image.h`, **not** from `petran -dump`.
- It is independent of the **petran binary**. It is **not** independent of the `f32image.h` header definition or of my reading of petran's *source* rules
  (`pe_tran.cpp`, `pe_imp.cpp`, `e32image.cpp`); where I check "petran did what its source says", an error common to source and layout reading would go unseen.
- Two validator bugs of mine were found and fixed mid-audit by reading `f32image.h`: `KCodeSegIdOffset` is **12** (`f32image.h:55`, used at `:744`), not 8;
  `KImageNmdExpData` is `0x400` (`:80`), not `0x2`. All 4 images were re-run after the fix: 25 ok / 0 fail each (`logs/hdr_*.txt`).
- The Symbian CRC is `HMem::Crc32` (`h_mem.cpp:175`): reflected `0xedb88320` table, **no** pre/post inversion, header CRC field set to `KImageCrcInitialiser=0xc90fdaa2` (`f32image.h:49`) while computing; UID checksum is the CCITT pair from `e32uid.cpp`. Both reproduce the stored values.

## Artifacts (sha256, re-verified; `/tmp/opencode/r1-build-repro/epoc32/release/x86/urel/`)

| File | sha256 | Size | Role |
|---|---|---|---|
| `euser.dll` | `0c833111d1f0656ef7d75848e76d9a90459ce449babc2f636d6d80e5f65dcf60` | 0x649ac | E32 DLL, UID3 `100039e5` |
| `efsrv.dll` | `69051f33b9b7f07b16ca5352b7a8d938490ed151e2d19db05f810f09939040fe` | 0x10254 | E32 DLL, UID3 `100039e4` |
| `t_cactw.exe` | `c096c6a27d65df49d896d3f4b3fc2d5d04e53461cdab93f663c3d1405a94b133` | | E32 EXE, SID 0 |
| `fhserver.exe` | `825d43353d3008ef8372aa2b6e878c6c530a9ff9456c83a8a7bf7f4bdf627faf` | | E32 EXE, SID `00002323` |
| `epoc32/tools/petran.exe` | `65f0015bb89d569748fd067d3adbdbd35950f1e2c5da1766af3c0e8d568c72e5` | | hand-built (D7) |

Normalised hashes for future rebuild comparison (E32 `iTimeLo/Hi` zeroed, header CRC recomputed; `work/normhash.py`; `out/normalised_hashes.txt`):

```
505e7da3cb30a4b7314289a8b13267d434b920db3c21ba43522f69b67856423f  euser.dll
193f3301e54d2f0b842cb99d05fca7c304a6a9f0299d8d20466ca1290a015863  efsrv.dll
f2a89b96d92206abba1ba97a43b512d1d90a1c561b9641a63bcdeb579adde8b0  t_cactw.exe
1222bd322a4d29f6c6bcf20ac4760948d4774ff8b84aa8741f3b301840730375  fhserver.exe
```

These only say what a rebuild must match *if* the only variation is the header stamp. They do not account for the path strings below.

## 1. PE2 layout, headers, section packing — confirmed

Header facts (all independently re-derived): `iFlags` euser `0x22005101`, efsrv `0x22004101`, exes `0x22004000`
= header format V (`0x02000000`), import format PE2 (`0x20000000`), SMP-safe (`0x4000`), DLL bit on the DLLs; euser additionally code-unpaged (`0x100`) and data-unpaged (`0x1000`),
efsrv code-unpaged only. These match the `petran` arguments in the build logs (`-codepaging unpaged -datapaging unpaged` for euser; efsrv `-datapaging default`; exes `default`/`default`).
CPU `0x1000` (X86), ABI/EPT bits 0, compression 0, module version `0xa0000` (=`-version 10.0`), VID `70000001`,
SID euser `100039e5` / efsrv `100039e4` / `t_cactw` 0 / `fhserver` `00002323`, capabilities euser/efsrv `00000000_000fffff`, exes `0`,
heap `0x1000/0x100000`, stack `0x2000`, priority field `0x15e`.

Section packing rule (**resolves the BUILD-REPRO "inferred `.rdata` after `.text`" caveat**):
`pe_tran.cpp:162` states the E32 text part is `.text + .rdata + .CRT`; `pe_file.cpp:453-468` trims `.rdata` to exclude `.edata`.
Numerically, for all four images `iTextSize == al4(.text vsize) + al4(.rdata vsize) (+ al4(.CRT))` (none of the four has a `.CRT`):
euser `0x61314`, efsrv `0xf598`, `t_cactw` `0x2d54`, `fhserver` `0x3f94`. Verified in `logs/audit4.txt`.
The IAT starts at `iCodeOffset + iTextSize` (`pe_tran.cpp:49-55`), the export directory follows the IAT
(`iExportDirOffset == iCodeOffset + iTextSize + 4*(nImports+1)` for all four; EUSER `0x613dc`, EFSRV `0xf864`), and
`iCodeSize == iTextSize + IAT + al4(4*iExportDirCount)` (EUSER `0x63a30`). Exes: `iDataBase == iCodeBase + iCodeSize` (`pe_tran.cpp:187`; `t_cactw` `0x402e24`).
`iCodeBase == PE ImageBase` (`pe_tran.cpp:322`: `0x10000000` DLLs, `0x400000` exes), while the PE `.text` RVA is `0x1000`; so stored code addresses are image-base-relative with the `.text` start rebased to offset 0.
The packing rule applies to `-align-const-section`-off builds: no petran call in the four build logs passes `-align-const-section`.

Relocations: EUSER 1610, EFSRV 339, `t_cactw` 222, `fhserver` 454 code relocs, declared == counted, all type 3 (HIGHLOW), and every stored value lies within
`[iCodeBase, iCodeBase+iCodeSize)` (or, for exes, code or data/BSS). **Only range was checked**; I did *not* verify that each relocation points at the *right* target (`logs/audit5.txt`). No data relocs in any image.

Entry: DLLs at `+0x264` (`__E32Dll`, forwarding to `__E32Dll_Body` by a `jmp rel32` with a relocation), exes at `+0x260` (`__E32Startup`); both match `.sym` symbols.
The entry area is `eb 0e` followed by 14 zero bytes, i.e. the `KCodeSegIdOffset` slot (+12) is zero in the file and is the loader's to fill; the real code starts at +0x10.

## 2. Export table, bitmap, absent marker — confirmed

Rule (source): a slot equal to `iEntryPoint` is *absent* (`e32image.cpp:454`, `absentVal = iOrigHdr->iEntryPoint` for non-ELF imports); `AddExportDescription` then
emits NoHoles/FullBitmap/SparseBitmap8. Both DLLs use type 2 (**SparseBitmap8**), `iExportDescSize` `0x2d` (EUSER) / `0xc` (EFSRV), and `0x98 + align4(size+3) == iCodeOffset` (`0xc8` / `0xa8`).
I decoded the sparse bitmap myself.

| | EUSER | EFSRV |
|---|---|---|
| ordinals in image / in `.def` | 2503 / 2503 (1..2503 contiguous) | 421 / 421 (contiguous) |
| absent per bitmap | 10 | 20 |
| absent by slot==entry | 10, identical set | 20, identical set |
| absent set vs `.def` | = literal `ABSENT` tags (699,700,701,702,703,1603,1915,1937,2203,2204; RHeap items) | = 2 literal (275 `_ZTI10TParseBase`, 358 `RRawDisk::Caps`) ∪ 19 `_ZT[IV]` entries |
| present exports mapped by source rule to `nm` symbol offset | **2493 / 2493** (2451 `.text`, 42 `.rdata`) | **401 / 401** (all `.text`) |

Consequences for the ABI work:
- EUSER comes from the **primary** `bx86gcc/euseru.def`, so its 44 `_ZTV*/_ZTI*` entries are **present** (e.g. `_ZTV5CBase` @1359, `_ZTV13CPolicyServer` @2002).
- EFSRV comes from the **secondary** `eabi/efsrvu.def`, where Raptor's `sed` rule makes every `_ZT[IV]` entry `ABSENT` (`e32importlib.mk:44-54`); the 18 non-literal ones plus the 2 literal tags give 20 (union checked exactly). So **EFSRV exports no vtables/typeinfo** in this build. This is the effect of the secondary-def rule and was observed, not assumed.
- Ordinals are positional (1-based `.def` ordinal = slot index + 1). Never take ordinals from one provider's `.def` for the other.

## 3. Import libraries and dependency names — confirmed

`euser{000a0000}.lib` has 2505 members = 2503 ordinal thunks + head + tail; `efsrv{000a0000}.lib` has 423 = 421 + 2 (`logs/audit7.txt`).
All members are COFF machine `0x14c`; every thunk's `.idata$4` is an ordinal (high bit set) in `.def`; thunk ordinals == `.def` ordinals with none missing/extra;
function thunks are `jmp [__imp_…]` (`ff 25`), and EUSER has 2 data imports (`KHeapMinCellSize`, `KHeapShrinkHysRatio`). The absent ordinals are named `absent_export_N` in the lib, and the lib's absent set equals the image's absent set exactly for both (`logs/audit8_libabsent.txt`). The tail member carries the dependency string:
`euser{000a0000}[100039e5].dll` and `efsrv{000a0000}[100039e4].dll`. (My first lib pass reported 18 "name mismatches" on EFSRV; they were exactly the `absent_export_N` thunks, i.e. the ABSENT-ed `_ZT[IV]`, not errors.)
`euser.lib`/`euser{000a0000}.lib` are byte-identical (same for efsrv); `hal.lib` is an **import library only** (6 thunks, dependency `hal{000a0000}[100039e8].dll`); no `hal.dll` exists in `urel/`, and `efsrv.dll` has zero HAL references, so EFSRV does not depend on a HAL DLL. `rpipe.dll` is a real E32 DLL (UID3 `102827d5`, 25 exports, imports 24 EUSER ordinals), built as a by-product of the euser `bld.inf` pass; not otherwise examined.
`edll.lib`/`eexe.lib`/`edllstub.lib` are the startup/TLS stubs: `uc_exe_.o` exports `__E32Startup`, `uc_dll_.o` exports `__E32Dll` → `__E32Dll_Body`, `edllstub` supplies `Dll::Tls/SetTls/FreeTls/FileName` via `UserSvr::Dll*`.

Import blocks of the images (PE2 format: block = {name offset, count}, ordinals in the IAT; `pe_imp.cpp:62-84`, `f32image.h:409-415`):

| Image | Block | Count | Provider check |
|---|---|---|---|
| `t_cactw.exe` | `euser{000a0000}[100039e5].dll` | 50 | 50/50 ordinals in range and present; version == provider `iModuleVersion`, UID == provider `iUid3` |
| `fhserver.exe` | `efsrv{000a0000}[100039e4].dll` | 22 | 22/22 present (ordinals 27,52,58,65,68,70,90,93,97,105,106,108,216,259,263,292–296,300,390) |
| `fhserver.exe` | `euser{000a0000}[100039e5].dll` | 65 | 65/65 present |
| `efsrv.dll` | `euser{000a0000}[100039e5].dll` | 136 | 136/136 present |

`resolved_*.json` in `out/` give ordinal → mangled name; the `.sym` `__imp_` symbol sets match the same names (50, 22, 65). None of `fhserver`'s 22 EFSRV imports is an `RFile64` or notification symbol.
**Unverified:** how the *loader* resolves these `name{ver}[uid].dll` strings (R1-STARTUP); this audit shows only that the names/ordinals are self-consistent with the providers.

## 4. Deviations D1–D8 and extras

Legend: **HOST** = affects only how build tools run; **TARGET** = can change image bytes/semantics. Rating = sanctionable / needs review / reject-as-basis. Ratings are my recommendations; the coordinator decides.

| # | Class | What I checked | Rating |
|---|---|---|---|
| D1 `gethost.sh` | HOST | Diff is exactly 2 added lines (fallback `libc_modern` when `/lib/libc-*` does not glob; `uname -m` when `uname -i`=`unknown`). It chooses the host binary directory name only. | sanctionable |
| D2 talon `process.c` | HOST (reliability) | Diff is exactly one condition (`errno == EAGAIN \|\| errno == EINTR`). It only retries `poll()`; a failing compile still reports its own status. I did **not** reproduce the 21/90 failures. No evidence it alters output bytes. It is a source patch to a third-party tool, so it still needs explicit approval. | sanctionable with approval |
| D3 flags/gcc shim | HOST | make 3.81, bash 4.0, talon are host tools. The target compile/link lines in all four build logs call only `tools/x86bin/{g++,ar,dlltool}` (MinGW 3.4.5 wrappers) plus `petran`, never the `tools/shim/gcc` shim. | sanctionable |
| D4 `cpp`/`sed` banner shims | **TARGET-adjacent** | They only rewrite `--version`, but the real tools are GCC 16.2.1 `cpp` and sed 4.9. The `sed` path was exercised and verified by outcome (the EFSRV absent set is exactly as the rule predicts). The `cpp` path preprocesses `bld.inf`/`mmp`, so host predefines or flags could change `#if` results; I **did not audit the cpp command lines** (e.g. `-undef`). This is also a version-check bypass of the kind the brief says not to add. | needs review |
| D5 fabricated variant | **TARGET** | See §5. | **reject as official-equivalence basis** |
| D6 `kernelhwsrv_ro` symlink | HOST + path strings | Needed for the space in the real path. The symlink path is embedded in `euser.dll` (below), so it affects hashes. | sanctionable, fix a canonical prefix |
| D7 hand-built petran | tool provenance | See §6. | needs review |
| D8 manual genexec | TARGET inputs | See §7. | needs review (low risk) |
| extra: `gcc_mingw_3_4_2.h` copy | TARGET-adjacent | `epoc32/include/gcc_mingw/gcc_mingw_3_4_2.h` is byte-identical to the file in the repo's `cpptoolsplat/mingw-gcc-3.4.5/`. Whether a separate 3.4.2 header exists upstream I did not establish. | needs review |
| extra: `platform_paths.hrh` | TARGET-adjacent | Third-repo input (`deviceplatformrelease` @ `b7c838933e1ac74d3873b8722538aba029479ab2`); the copy in `dl/` and `epoc32/include/` are identical (sha256 prefix `8a8ebdc3f40804d6…`). It defines only path macros; `#define SYMBIAN_OLD_EXPORT_LOCATION` is **commented out** (line 309). | record the full hash in the recipe |
| extra: variant.cfg history | provenance | `export_eka.log`, `export_f32.log`, `petran_tools2.log` carry the `ENABLE_ABIV2_MODE` warning; later logs do not. `variant.cfg` mtime (20:34:17) is later than those logs (20:31–20:32). So EXPORT outputs were produced against an **earlier variant.cfg** that I cannot reconstruct; only the final 2-line form is on disk. | re-run EXPORT after the config is fixed |

### D1/D2 diff check
`artifacts/scratch_raptor_patches.diff` (sha256 `fd504a6009d3a700…`): `gethost.sh` +2 lines; `process.c` 1 line changed. `git -C src/build status --short` shows only those two modified files plus untracked `.pyc` and the built `linux-x86_64-libc_modern/` directory. `bintools/` and `imgtools/` unmodified.

## 5. D5: the fabricated variant — precise effect

Files (sha256 full values): `epoc32/include/variant/scratch_r1.hrh` `f49880199a1fae0a4a1aac678f525f1882ca99602e2bc20cbbd88c3bd4c825a5`
(only `#include <platform_paths.hrh>` inside a guard) and `epoc32/tools/variant/variant.cfg` `8c7bc53631c4a405cca0c62b0957723fc7132693a07b9ea595b351101733c06f`
(two lines: the hrh path and `ENABLE_ABIV2_MODE`).

How it reaches the compiler (observed on the compile lines): every compile passes `-include epoc32/include/gcc.h` and
`-DPRODUCT_INCLUDE="…/variant/scratch_r1.hrh"` (19 times in `efsrv_build2.log`); `kernel/eka/include/gcc.h:33-35` then `#include PRODUCT_INCLUDE`. The other macros on the euser line are
`__X86__ _UNICODE __SYMBIAN32__ __EPOC32__ NDEBUG __X86GCC__ __GCC32__ __DLL__ __CIA__` plus `USE_INSTRUMENTED_HEAP`/`ENABLE_HEAP_MONITORING` (from `eexe_*_heap.mmp`, not from the variant) and `SYMBIAN_FTRACE_ENABLE_UDEB` for EFSRV.
Compile flags: `-O2 -Wall -fno-rtti -fno-exceptions -fcheck-new -O1 -fomit-frame-pointer -masm=intel` (note both `-O2` and `-O1` appear; I did not determine which wins, last-wins is the usual GCC rule — unverified).

**Variant-controlled macros that are undefined in this build** (sources branch on them; none is defined in `kernelhwsrv/` or in the scratch `epoc32/include`):
`SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API` (36 uses in the F32 client tree), `SYMBIAN_F32_ENHANCED_CHANGE_NOTIFICATION` (26), `SYMBIAN_DISTINCT_LOCALE_MODEL` (10; `euser/us_exec.cpp:38,2546,2612`, `e32std.h:2498`),
`SYMBIAN_ENABLE_FAT_DIRECTORY_OPT`, others. The first is documented as a "Symbian Variability" macro in `userlibandfileserver/fileserver/group/release.txt:1813`; **where a real kit defines it I did not find, so the real default is unverified.**

**Observed consequence in `efsrv.dll` (not inference):**
`cl_file.cpp:1904 #ifdef SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API … 3441 #else … 3498 #endif`; the `#else` branch is
`EFSRV_EXPORT_C TInt RFile64::Open(…) {Panic(ENotImplemented);return (KErrNotSupported);}` (`cl_file.cpp:3442-3443`). In the built `efsrv.dll.sym`, `RFile64::Open(RFs&,const TDesC16&,TUint)` at VA `0x10006dc0`
disassembles to `push 6 (ENotImplemented); call Panic(TClientPanic); mov eax,0xfffffffb; ret`. EFSRV still **exports** the 28 `RFile64` symbols (the ordinals are source-of-truth `.def` order), so the ordinal set is the same either way, but the code behind them is a panic stub.
`cl_std.h:22` also has an `#ifndef`/`#include <f32file64.h>` guard that goes the other way. I did not check whether `RFile::Size`/`Seek` follow the 32-bit paths (`f32file.h:2264,3086`) in the built image beyond the `RFile64` stubs.
**Not examined:** the effect of the other undefined macros on `euser.dll` and on F32 behaviour (e.g. `RegionCode` takes the non-distinct-locale branch). `fhserver`'s 22 imports are unaffected by the 64-bit stubs only because none is an `RFile64` symbol.
This matters to R1-WIRE and the F32 server-side work: the client library's wire behaviour for large files in these artifacts is **not** that of a kit built with the macro defined.

Proposal: do not guess macros. The coordinator/owner picks the target variant; the sanctioned config must be a *named, hashed* `variant.cfg` + `.hrh` with every `SYMBIAN_*` choice written down, applied from EXPORT onward.

## 6. D7: hand-built petran — provenance

- Source: 20 files, exactly the `source` list of `bintools/petools/group/petran.mmp` (20 objects in `work/petran_w`, all `.err` empty); `bintools/` and `imgtools/` equal the pinned commit.
- Flags (`logs/petran_cflags.txt`): `-w -fpermissive -O1 -include new -include epoc32/include/gcc_mingw/gcc_mingw_3_4_2.h -D__SUPPORT_PE_FILES__ -D__SYMBIAN32__ -D__TOOLS__ -D__TOOLS2__ -D__EXE__ -D__SUPPORT_CPP_EXCEPTIONS__ -D_WIN32 -DWIN32 -D_WINDOWS -D__TOOLS2_WINDOWS__ -DEKA2` plus include paths. `-w` and `-fpermissive` hide diagnostics that a Raptor tools2 build would show.
- **Gap:** `logs/petran_link.log` is 0 bytes, so the **link command was not archived**. A reviewer cannot rebuild `petran.exe` from the logs alone.
- Evidence it behaves: the output of four conversions satisfies every layout rule in `pe_tran.cpp`/`pe_imp.cpp`/`e32image.cpp` that I could check numerically (§1–§3). That supports "not obviously miscompiled", not "identical to an official/Raptor-built petran".
- Invocations (from the build logs) pass no `-align-const-section`, `-compressionmethod none`, `-smpsafe`, capabilities as above.
- Header time stamp: `E32ImageFile::SetDefaultHeader` writes `timeToInt64(time(0))` (`e32image.cpp:131-133`) with constants `daysTo2000AD=730497` and `+3600` "BST" (`:104-111`). Decoding `iTime` with the standard 0 AD → 1970 offset gives a date **12 days and 1 hour after** the host mtime (minutes and seconds match exactly). I did not determine whether the constant or my decode is the cause; either way the stamp is not a trustworthy build date, and it varies every run.

## 7. D8: genexec — comparison with the upstream recipe

Upstream recipe: `kernel/eka/generic.inf:22-27` declares extension `base/genexec` with `INC_PATH = $(EPOCROOT)epoc32/include/platform` (since `SYMBIAN_OLD_EXPORT_LOCATION` is undefined) and `EXTRA_SRC_PATH = $(EXTENSION_ROOT)/kernel`;
`extension/genexec.mk` runs `perl $(EXTRA_SRC_PATH)/genexec.pl -i execs.txt -e $(XINCDIR)/exec_enum.h -u $(XINCDIR)/exec_user.h -k $(XINCKDIR)/exec_kernel.h`.
My re-run (`work/genexec/`, private prefix, with the upstream `execs.txt` sha256 `5e53fd14a7e4c706…` and `genexec.pl` sha256 `dd887d6aced8e639…`) is the same command and produced **byte-identical** outputs to the build's headers:
`exec_enum.h` `169ca7a9d94cebfa1e0eeee83f78c43f8535534c55967a70a4d844a4773a5e1e`,
`exec_user.h` `1610cda797a6b5e9bd011c02637c6c965b08a02bf66112dab5b08abc04393ef5`,
`exec_kernel.h` `b645f30862d9bef466e9ae01e205b232dccfea57e6670f87b00d949488f139d1`.
So manual == recipe in command line and output, and the generator is deterministic on this host (`genexec.pl:132` pipes through the system `cpp -I- -I. -lang-c++`, GCC 16's here; the `-I-` note is the only diagnostic).
**Unexplained:** why Raptor did not run the extension. The EXPORT pass only unzipped the 6-byte `dummy_genexec_hr.zip` placeholders (`export_eka.log:129,397`) and copied `genexec.meta/.mk` into `makefile_templates/base/`; `euser_build4.log` has 0 references to genexec. `5` "NMAKEFILE/GNUMAKEFILE/MAKEFILE keywords not supported on Linux" warnings exist; whether they relate is not established. The headers are timestamped 20:37:25, before the successful euser builds (20:37:39 and 20:37:57).

## 8. Build-path leakage and reproducibility

- `euser.dll` embeds `/tmp/opencode/r1-build-repro/kernelhwsrv_ro/kernel/eka/common/heap_hybrid.cpp` and `…/debugfunction.cpp` (`__FILE__` in heap-corruption strings; offset `0x4dc00`). The other three images contain no `/tmp/opencode` strings. So `euser.dll` bytes depend on the prefix **and on the D6 symlink name**.
- PE intermediates carry a build-time `TimeDateStamp` (`0x6ad8f873`, `…90a`, `…959`, `…988`); the converted E32 image carries its own `iTime`. A rebuild therefore differs even if code is identical. **Rebuild-twice determinism was not tested** (I was told not to redo the build); the normalised hashes above are only a baseline for that future test.
- Not covered: `udeb`, SMP, other cores, any config other than `x86_urel` / `x86_urel.test`.

## 9. Minimal reproducible recipe and proposed sanctioned configuration

This amends the BUILD-REPRO "Reproduce" section; nothing here was executed by this card.

Proposed sanctioned inputs (each needs coordinator/owner approval; items marked ◆ are the open decisions):
1. Build tools: `SymbianSource/oss.FCL.sftools.dev.build` commit `f458a4ce83f74d603362fe6b71eaa647ccc62fee`, plus **only** D1 and D2 as hashed patch file(s).
2. Host tools: Python 2.7.18 and bzip2 1.0.8 (hashes in BUILD-REPRO), make 3.81 (shipped), bash 4.0.24 + patches, MinGW 3.4.5 package hash `0c2ac07f…594c`, all in a fixed prefix.
3. ◆ A **fixed canonical build prefix** (and a non-space source path or a symlink with a fixed name) recorded in the recipe, because it is embedded in `euser.dll`.
4. ◆ A **named variant**: hashed `variant.cfg` + `.hrh` listing every `SYMBIAN_*` macro decision (§5). Run EXPORT only after it is final.
5. `platform_paths.hrh` pinned by full sha256 and source commit.
6. ◆ `petran`: one scripted build including the link line, archived log, and a stated compare against either an official `petran` or a Raptor `tools2` build (not available here).
7. ◆ D4: either supply real cpp/sed of the required versions, or have the coordinator accept GCC 16 cpp after a flag audit (`-undef` and predefined macros) of the `bld.inf`/`mmp` preprocessing.
8. ◆ D8: a root-cause for the missing extension run, or a sanctioned scripted `genexec.pl` step with the §7 command line and input hashes.
9. Order: EXPORT (eka, f32) → genexec → `euser/bld.inf` → `halservices/hal` → `efsrv` → test MMPs with `-c x86_urel.test` (`t_cactw`, `fhserver`).

Clean-rebuild requirements: start from an empty prefix with items 1–8; no hand-edits between steps; archive every log; run twice and compare against the normalised hashes (with the same prefix) after masking E32 `iTime`/CRC; confirm `git -C kernelhwsrv status --short` is empty before and after.

## 10. Executable acceptance tests (no QEMU, no E32 execution)

All use only file reads; `work/e32audit.py`, `audit2.py`…`audit8` are the scratch implementations (not committed; ask for them if wanted).
1. For each image: UID checksum, `EPOC` signature, header CRC (init `0xc90fdaa2`, no inversion), flags, `iEntryPoint+12+4 <= iCodeSize`, export-description end == `iCodeOffset`. Expect 25/25.
2. EUSER: 2503 ordinals, absent set == literal `ABSENT` tags of `bx86gcc/euseru.def`; every present export offset == `nm` symbol offset under the `.text + .rdata` packing rule (2493/2493).
3. EFSRV: 421 ordinals, absent set == literal tags ∪ `_ZT[IV]` of `eabi/efsrvu.def` (20); 401/401 present offsets match.
4. `euser{000a0000}.lib` / `efsrv{000a0000}.lib`: thunk ordinals == `.def` ordinals; absent thunks == image absent set; tail member names the right dependency string.
5. Each app import ordinal is in range and non-absent in the *built* provider, and its `{version}` and `[uid]` equal the provider's `iModuleVersion`/`iUid3` (50, 22, 65, 136 respectively).
6. Rebuild twice; normalised hashes equal (this will probably fail for `euser.dll` unless the prefix is fixed).
7. `strings -a` of each image contains no `/tmp/` prefix (will currently fail for `euser.dll`).
8. Disassemble `RFile64::Open` in `efsrv.dll.sym`: the stub (`push 6; call Panic; mov eax,-5`) disappears once the sanctioned variant defines the 64-bit macro (or is confirmed intended).
9. `git -C kernelhwsrv status --short` is empty.

## Unknowns / not done

- Runtime behaviour of every image (loader string resolution, relocation correctness per target, entry, TLS, heap). Loader resolution of `name{ver}[uid].dll` is R1-STARTUP's.
- Relocation **target correctness** (only range checked); data section contents; `.CRT`-bearing images; `-align-const-section` builds.
- Official-kit equivalence: not established for headers, variant, petran or compiler.
- Effect of undefined variant macros on `euser.dll` and on the rest of EFSRV; real-kit values for them.
- cpp command-line flags (D4); `-O1` vs `-O2` precedence; why Raptor skipped the genexec extension.
- Determinism (never rebuilt); a cold end-to-end rebuild.
- Licences (EPL source; MinGW GPL/LGPL) not cleared; GPG web of trust not established for Python/bzip2.

## Handoff

- **Card:** R1-ARTIFACT-AUDIT. **Base:** port `fa62323eea7bc96cefd666e40f5189567731bd44`, branch `main`, dirty tree (35 modified, 24 untracked before this report); upstream `0c3208650587ac0230aed8a74e9bddb5288023eb` read-only and clean; build-tools `f458a4ce83f74d603362fe6b71eaa647ccc62fee` + D1/D2.
- **Contract revision:** none issued; R1-ABI/v0 remains NOT FROZEN. Note for ABI/v0 as already recorded: file subsession close is `EFsFileSubClose`=28, 27 closes a directory.
- **Changes:** this report only (`qemu-port/docs/r1_artifact_audit_v0.md`, new, untracked). Scratch in `/tmp/opencode/r1-artifact-audit/` (not committed). No commits/pushes, no edits to source, shared docs, or another worker's products; artifact hashes unchanged.
- **Tests:** the 25-check header validation ×4, export/bitmap/def comparison ×2, lib thunk audit ×2, import-resolution ×4, genexec byte comparison ×3, D1/D2 diff check, hash before/after. Results quoted above; logs in `/tmp/opencode/r1-artifact-audit/logs` and `out`. Zero QEMU, stable-suite, hardware or E32 execution.
- **Manual step:** coordinator decides, per §4/§9: which of D1–D3, D6 become sanctioned; whether D4/D7/D8 are accepted or redone; and, with the owner, which variant macros the target kit defines (D5), notably `SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API`.
- **Limits:** see "Unknowns"; structural validity only; not an SDK; no redistribution.
- **Blocked assumptions:** runtime behaviour; official-kit/variant equivalence; determinism; R1-ABI/v1 import identity until R1-STARTUP confirms loader string resolution; F32 wire framing until R1-WIRE, which should treat these EFSRV `RFile64` stubs as non-representative.
- **Integration order:** (1) coordinator rulings on D1–D8 and the variant; (2) a clean rebuild under the sanctioned recipe, compared with the normalised hashes here; (3) R1-STARTUP (loader resolution) and R1-WIRE; (4) R1-ABI/v1 freeze by the integrator.
