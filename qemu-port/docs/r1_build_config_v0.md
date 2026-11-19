# R1-BUILD-CONFIG v0 — named port variant, metadata-preprocessing audit, petran provenance, clean-build recipe

Status: **research proposal, not frozen, not an official-kit-equivalence claim.**
Nothing here was committed; no target was rebuilt; no E32 image was executed; no
QEMU or hardware run. All scratch is under `/tmp/opencode/r1-build-config/` and
is not redistributed. The scripts that matter are embedded in §11 so this report
is self-contained.

## 1. Result in one paragraph

A named variant **`port-x86-rfile64-v1`** (one macro:
`SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API`, plus `platform_paths.hrh`) replaces the
rejected D5 `scratch_r1` variant. Compiling the same 121 EUSER/EFSRV translation
units from the existing build logs with and without the macro shows that the macro
changes **6 EFSRV TUs and 0 EUSER TUs**; `RFile64::{Open,Create,Replace,Temp}` stop
being `Panic(ENotImplemented)` stubs, and no class size or export set I measured
changes. D7 is closed by a 20-object / one-link script that reproduces the
archived `petran.exe` **byte for byte except the PE `TimeDateStamp` and `CheckSum`**.
D4 is **partly** closed: the metadata `cpp` command line is reconstructed from
Raptor source (not captured live), host `cpp` and the real MinGW 3.4.5 `cpp` agree
on content for 24 of 24 files, and I propose replacing the banner-rewriting shims by
`--toolcheck=off` plus a recorded preflight. That policy is **untested end to end**.
Several items remain unverified (§12).

## 2. Revisions and provenance

| Item | Value |
|---|---|
| Port repo | `fa62323eea7bc96cefd666e40f5189567731bd44`, branch `main`, **dirty**: 62 `git status --short` lines when this report was written (untracked docs/files of other cards included; none of them touched by this card) |
| Upstream `kernelhwsrv/` | `0c3208650587ac0230aed8a74e9bddb5288023eb`, `status --short` = 0 lines (read-only, unchanged) |
| Build tools repo | `SymbianSource/oss.FCL.sftools.dev.build` commit `f458a4ce83f74d603362fe6b71eaa647ccc62fee`; tracked modifications are exactly `sbsv2/raptor/bin/gethost.sh` and `sbsv2/raptor/util/talon/process.c` (D1/D2), 39 untracked entries (build products/tools) |
| Contract revision | none issued. R1-ABI/v0 stays not frozen |
| Existing images | unchanged (sha256 prefixes `0c833111` euser, `69051f33` efsrv, `c096c6a2` t_cactw, `825d4335` fhserver) |

## 3. The named variant

Name: **`port-x86-rfile64-v1`**. It is a *port* configuration chosen by the owner
for this project. It is not a Symbian-published variant and I make no claim that
it matches any product kit.

`epoc32/include/variant/port_x86_rfile64_v1.hrh` (sha256 `500889884b3063c1b0712c000d2c62f55e70774cbfda41b51b82f343be277106`):

```
// Named port variant: port-x86-rfile64-v1 (R1-BUILD-CONFIG v0 proposal; not an official Symbian kit variant)
#ifndef PORT_X86_RFILE64_V1_HRH
#define PORT_X86_RFILE64_V1_HRH
#include <platform_paths.hrh>
#define SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API
#endif
```

`epoc32/tools/variant/variant.cfg` (sha256 `2c505b4bb43e4b74e362589ce78f866053e4e9d8283688e47cca321c58e08cd0`):

```
epoc32/include/variant/port_x86_rfile64_v1.hrh
ENABLE_ABIV2_MODE
```

Evidence for the shape:
- `raptor_meta.py:60-80` reads `variant.cfg`; a line containing `.hrh` becomes
  `VARIANT_HRH = EPOCROOT + that path`; `ENABLE_ABIV2_MODE` is the other line the
  upstream `variant.cfg` carries (upstream `deviceplatformrelease@b7c83893…`
  `foundation_system/sf_config/config/inc/variant.cfg` has the same two lines
  with a Windows path).
- `kernel/eka/include/gcc.h:33-35` does `#include PRODUCT_INCLUDE`; Raptor sets
  `-DPRODUCT_INCLUDE="<VARIANT_HRH>"`; the metadata `cpp` gets the same file via
  `-include` (§6). So one header drives both bld.inf/mmp preprocessing and C++.
- The old D5 header differed only in lacking the `#define` (it just included
  `platform_paths.hrh`, sha256 prefix `f4988019`), which is why the old EFSRV has
  the stubs.

**Why only one macro.** The card says not to blindly enable unrelated features.
The audit table (§4) lists every `SYMBIAN_*`/related macro read by the EUSER and
EFSRV closure with evidence. Only the 64-bit macro is selected. The reference file
`Symbian_OS_future.hrh` (sha256 `1011d0e0…621220`, from `deviceplatformrelease@b7c83893…`)
*does* define it at line 1834 (with `@publishedAll`, "supports PREQ1725", lines
1822-1834), which agrees with the owner's choice, but I did **not** take the other
~150 macros of that file.

`SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API` also controls the **EXPORT** step:
`userlibandfileserver/fileserver/group/bld.inf:31-34` exports `f32file64.h` and
`f32file64.inl` only when it is defined (confirmed by running the metadata `cpp`:
v0 gives 21 content lines for that bld.inf, v1 gives 23). **EXPORT must therefore
run only after the variant files are in place** (§9). A tree exported with the old
variant lacks those two headers; `f32file.h:2465` then fails to find
`<f32file64.h>`. The existing scratch `epoc32/include` has no `f32file64.h`.
Do not reuse it.

## 4. Variability-macro audit (EUSER + EFSRV closure)

Method (all read-only):
1. Collected the dependency closure from the 165 `.d` files and the 121 logged
   compile commands (102 EUSER, 19 EFSRV; every one of the 102 EUSER `.o.d` files
   maps to a logged command): 314 source/header files.
2. Listed every `SYMBIAN_*`, `__SYMBIAN*`, and every other conditional identifier
   that is never `#define`d in the files read (137 identifiers; most are other
   compilers or platforms — ARMCC, CW32, BSD, dla.h internals).
3. Intersected those 137 with every `#define` in the reference kit headers
   fetched from `deviceplatformrelease@b7c83893…` (`Symbian_OS_future`,
   `Symbian_OS_tb101sf`, `Symbian_OS_tb102sf`, `bldcodeline/bldprivate/bldpublic/
   defaultcaps`, western `bldregional`): exactly **5** macros.
4. For each candidate, compiled all 121 TUs to assembly (`g++ -S`, same flags as the
   logged commands, only `-DPRODUCT_INCLUDE` swapped) under seven variants and
   compared the asm hashes. Variants were `v0` (no macro), `v1` (64-bit only),
   and `v1` plus one of each other macro (`v1s`, `v1d`, `v1n`, `v1t`, `v1L`), plus `v1all`
   (64-bit, split headers, distinct locale). Zero compile failures.

| Macro | In reference kit files? | Where used (examples) | Effect measured vs `v1` | Selected |
|---|---|---|---|---|
| `SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API` | future, tb101sf, tb102sf | `bld.inf:31`, `f32file.h:2264,2465,3086,3098,3248,3329`, `cl_file.cpp:1543,1904` + 5 more `.cpp` | vs `v0`: **6 EFSRV TUs change, 0 EUSER** (`cl_cdir`, `cl_cli`, `cl_entry`, `cl_file`, `cl_ftext`, `cl_fman`) | **ENABLE** (owner) |
| `SYMBIAN_ENABLE_SPLIT_HEADERS` | future, tb101sf, tb102sf | 13 headers, e.g. `e32cmn.h:2783,6767`, `f32file.h:3337` | 0 TUs change; it changes which headers are included, not codegen in this set | no (owner may decide) |
| `SYMBIAN_DISTINCT_LOCALE_MODEL` | future, tb101sf, tb102sf | `us_exec.cpp` (9 sites), `e32std.h:2498` | 1 TU (`us_exec.cpp`). Undefined: `TExtendedLocale::LoadLocaleAspect` returns `KErrNotSupported` (`us_exec.cpp:3223-3227`), region code 0 (`:2546-2553`) | no — behavioural, owner decision |
| `SYMBIAN_F32_ENHANCED_CHANGE_NOTIFICATION` | **tb102sf only** (not future) | `cl_notification.cpp:20,443`, `bld.inf`, `f32file.h` | 1 TU. Undefined: `TFsNotification::*` methods `Panic(ENotificationPanic)` (`cl_notification.cpp:445-480`) | no — owner decision |
| `SYMBIAN_FIX_TDESC_CONSTRUCTORS` | **none** | `e32des8.h:97,307`, `e32des16.h` | **29 TUs** (18 EUSER + 11 EFSRV): adds inline copy constructors to `TDesC/TDes` | **no** — touches every descriptor user and is not in any reference file |
| `__LEAVE_EQUALS_THROW__` | future, tb101sf, tb102sf | `ub_act.cpp:919`, `uc_utl.cpp`… | **0 TUs**: `e32def.h:29-31` `#undef`s it unless `__SUPPORT_CPP_EXCEPTIONS__`, which the x86 compile lines do not define (and they pass `-fno-exceptions`) | no (no effect) |
| `SYMBIAN_OLD_EXPORT_LOCATION` | commented in `platform_paths.hrh:309` | export-path macros, 5 sites | n/a (selects old export dirs) | no — leave as pinned |
| `SYMBIAN_FTRACE_ENABLE_UDEB` / `_UREL` | — | set by the `macro` line of `efsrv.mmp` (appears in the mmp preprocessing output and on the compile line) | n/a | unchanged (comes from the mmp) |
| `SYMBIAN_TRACE_SYSTEM_INCLUDE` | — | `#define`d in `group/f32.mmh:27` | n/a | unchanged |
| `__SYMBIAN_KERNEL_HYBRID_HEAP__` | — | `#define`d in `e32cmn.h:2837` | n/a | unchanged |
| `SYMBIAN_PRIVATE_EFSRV`, `SYMBIAN_ENABLE_PUBLIC_PLATFORM_HEADER_SPLIT`, `__SYMBIAN_STDCPP_SUPPORT__`, `SYMBIAN_INCLUDE_EXECUTABLE_TRACE` | none | 2 / 1 / comments / 0 uses in the closure | **not measured** | no (effect unverified) |

Closure stability: re-running the dependency scan under `v1all` reads 272 files and
**0 files** that the old closure did not read.

Limits of this audit: (a) the reference-kit files are from one
`deviceplatformrelease` commit; a real product kit may differ; (b) only the
EUSER/EFSRV closure plus the two test programs were checked — other components
(kernel, drivers, file systems) were not; (c) "no asm change" is not a proof of
no effect on headers installed by EXPORT; (d) I did not check whether
`F32_ENHANCED_CHANGE_NOTIFICATION` changes `efsrvu.def` ordinals consumed elsewhere.

## 5. Expected change from the panic stubs to real implementations

Measured on assembly (compiled with the logged EFSRV flags, `-O2`, `-S`):

- **`RFile64::Open/Create/Replace/Temp`**: `v0` is `push ebp; mov ebp,esp; sub esp,8;
  mov [esp],6; call Panic(TClientPanic); leave; mov eax,-5; ret` (matches the
  shipped `efsrv.dll` at VA `0x10006dc0`); `v1` builds an IPC argument block and
  calls the server (`mov ecx,30` etc.). The `#else` stub bodies are at
  `cl_file.cpp:3444-3450`.
- `cl_file.cpp`: 32 functions change, none added or removed. `cl_fman.cpp`: 24 change,
  one private function swaps signature (`CFileMan::DoCopy(RFile const&,RFile&,int&)` →
  `DoCopy(RFile64 const&,RFile64&,int&)`); it is private and not in `efsrvu.def`.
  `cl_cdir`: 13 changed; `cl_cli`: 4; `cl_entry`: 5; `cl_ftext`: 7.
  Calls to `RFile::Open/Create` inside EFSRV become calls to `RFile64::Open/Create`.
- **Unchanged (measured):** `sizeof(TEntry)=552`, `TVolumeInfo=576`, `RFile=8`,
  `CFileMan=3328`, `TFileText=284`, `CDir=8` are equal under `v0` and `v1`
  (`RFile64` is 8 under `v1`). The 28 `RFile64` exports exist in the def file
  regardless of the variant (`efsrvu.def` has 28 `RFile64` lines; the current stub
  build already exports 28 `T` symbols), so the **ordinal set does not move**.
  `DoCopy` does not appear in `efsrvu.def`.
- **EUSER:** 0 of 102 TUs change.
- **Consumers:** `t_cactw.cpp` and `handshareint.cpp` are unchanged; `fhserver`'s
  `handshare.cpp` changes. So a rebuilt `fhserver.exe` will differ, `t_cactw.exe` is
  expected not to.
- Not measured: link-level differences in the final `efsrv.dll` (section layout,
  VA of each function); runtime behaviour of the real implementations.

## 6. Metadata preprocessing audit (D4)

### 6.1 What Raptor actually runs (reconstructed from source, **not captured live**)

`raptor_meta.py`: `MetaDataFile.getContent` (466-492), `PreProcessor.call` (300),
`__constructPreProcessorCall` (393-408), `preparePreProcessorMacros` (502-520),
`preparePreProcessorIncludePaths` (527-560). Command shape:

```
$(GNUCPP) -undef -nostdinc <-MD -MF<dep> -MT'$(PARSETARGET)'>      # dep options when deps are generated
          -include <VARIANT_HRH>
          -DSBSV2=_____SBSV2 -DX86GCC=_____X86GCC -DGCC32=_____GCC32 -DEPOC32=_____EPOC32
          -DX86=_____X86 -DGENERIC_X86=_____GENERIC_X86 -D__GNUC__=3
          -I . -I <dir of VARIANT_HRH> -I <EPOCROOT>/epoc32/include -I <bld.inf dir>
          <file>
```

- Macro list = `SBSV2` + `PLATMACROS` = `gcc_x86.xml:6` (`X86GCC GCC32`) + `EPOC32` +
  `x86.xml:50` (`X86 GENERIC_X86`) (`x86.xml:48-52`). `SYSTEMINCLUDE` for x86 is empty
  (`x86.xml:94`). The `_____` prefix is undone by the regex at `raptor_meta.py:483-489`.
- stderr handling (`:332-349`): lines containing `warning:` etc. are warnings, any
  other stderr is an error and aborts. The preprocessor must therefore be silent on
  stderr. Both tested `cpp`s were (0 bytes).
- `GNUCPP` = `$(SBS_GNUCPP)`, default `$(SBS_USRBIN)/cpp` on Linux
  (`locations.xml:22`), `$(MINGWBINDIR)/cpp.exe` on Windows (`:58`).
- Reconstruction caveat: I did not run `sbs -d` (which would print
  `Preprocessing command line …`, `raptor_meta.py:303`, because a target rebuild is
  out of scope). Include-path order and the exact macro set for the `.test`
  variant are **unverified** against a live log (acceptance test A1).

### 6.2 Host `cpp` vs the real 3.4.5 `cpp`

Both run on 12 metadata files (bld.inf: `kernel/eka`, `kernel/eka/euser`,
`halservices/hal`, `fileserver/group`; mmp: `edll`, `eexe`, `eexe_instrumented_heap`,
`eexe_monitor_heap`, `edllstub`, `euser`, `efsrv`, `t_cactw`), under both `v0` and
`v1all` = 24 comparisons. Host: `/usr/bin/cpp` GCC 16.2.1. Reference: MinGW 3.4.5
`cpp.exe` (`mingw-symbian-001.zip`) run under Wine 11.0.

| Observation | Result |
|---|---|
| exit codes | 0 and 0 for all 24; stderr 0 bytes |
| content lines (linemarkers `# n "file"` removed, CR stripped, trailing blanks stripped) | **identical in 24/24** after dropping blank lines |
| blank lines | differ in count (e.g. 15 host vs 22 mingw for `fileserver/group/bld.inf`); after Raptor's own post-processing the raw lists differ, so a strict list compare gives 0/24 equal. `__getSection` skips blank lines (`raptor_meta.py:1151`), so this is harmless there. **Not checked for the mmp parser.** |
| carriage returns | MinGW-under-Wine output contains CR (139 in `euser.mmp`); host 0. Irrelevant for the real Windows path (`universal_newlines=True`, `raptor_meta.py:317-322`), but on Linux with the MinGW binary the CRs would reach the parser — unverified |
| linemarker file sequence | equal in 24/24 |
| linemarker line numbers | the first marker is `# 0 "file"` (host) vs `# 1 "file"` (mingw); only affects line numbers in diagnostics (`getPreProcessorCommentDetail`, `:219-234`) |
| predefines under `-undef -nostdinc` | host leaves 8 (`__STDC_HOSTED__`, `__STDC_EMBED_*`, `__STDC_UTF_16__` …), MinGW leaves 1 (`__STDC_HOSTED__`). `__STDC` has **0** uses in the `kernelhwsrv` mmp/mmh/bld.inf/hrh files and in `platform_paths.hrh` |
| `-MD -MF -MT` | accepted by MinGW 3.4.5 `cpp` (rc 0, dep file written) |

### 6.3 Why the version check fails and what the fake banner did

`locations.xml:90` requires `cpp(.exe)? .* [345]\..*` (passed by the real MinGW
`cpp.exe`: `cpp.exe (GCC) 3.4.5 (mingw special)`; **failed** by host `cpp (GCC) 16.2.1`).
`locations.xml:95` requires `GNU sed version [3-9].*`, which **no modern sed can
satisfy**, because modern sed prints `sed (GNU sed) 4.9`. So `GNUSED` is a regex
limitation, not a version problem. The previous shims (`hostshim/cpp`, `hostshim/sed`)
only rewrote the `--version` banner and otherwise `exec`ed the host tool. Verified by
reading them.

The only `sed` expression executed by the x86 EUSER/EFSRV flow is
`e32importlib.mk:54` (`_ZT[IV]…NONAME` → `ABSENT`, `St9__va_list` → `Pc`). With real
sed 4.9 it produces byte-identical output (`cmp`) to the intermediate
`efsrv.def.x86` the earlier build used, with 20 `ABSENT` lines. Other `GNUSED` uses
(`gccxml.flm`, `resource.flm`, `win32.flm`) are not on this route; I did not check
that no other flm in the x86 route uses it. (The 20 `ABSENT` lines exist only after the
transform; the raw def has 2.)

### 6.4 Proposed policy (for review, replaces the banner shims)

1. Do not install any `cpp`/`sed` wrapper. Set `SBS_GNUCPP=/usr/bin/cpp`,
   `SBS_GNUSED=/usr/bin/sed` (real tools).
2. Run `sbs` with the documented `--toolcheck=off` (`raptor_cli.py:149-155`,
   `raptor.py:1326`; Raptor prints "Not Checking Tool Versions").
3. Replace the lost check by `preflight.sh` (§11), which records the real
   `cpp`/`sed` versions and exits non-zero on any pinned-input mismatch.
4. Alternative with no policy exception for `cpp`: point `SBS_GNUCPP` at a wrapper that
   execs MinGW `cpp.exe` under Wine; this *passes* the real check. It still
   needs `toolcheck=off` for sed, and adds CR output, so I do not recommend it.

Both statements "host cpp is content-equivalent" and "`--toolcheck=off` does not
change other behaviour" are tested only as stated above, not by a Raptor run.

## 7. `petran` provenance and script (D7)

- Source: build repo `bintools/petools/group/petran.mmp` (20 sources, one
  `macro __SUPPORT_PE_FILES__`, `userinclude` `imglib/compress`, `imglib/inc`).
  Sources live in `imgtools/imglib/*` and `bintools/petools/pefile`, **not** in
  `kernelhwsrv`.
- Why not Raptor `tools2`: the old attempt (`petran_tools2.log`) failed first on
  `tool 'COMPILER' … did not return version 'gcc version (3.[456])|(4.[0-9])'`
  (line 111), then on `gnu/stubs-32.h` missing (32-bit glibc headers) for every file.
  Not retried here.
- Flags: the archived set `logs/petran_cflags.txt` (26 tokens): `-fpermissive -O1
  -include new -include epoc32/include/gcc_mingw/gcc_mingw_3_4_2.h`, `-D` set
  (`__SUPPORT_PE_FILES__ __SYMBIAN32__ __TOOLS__ __TOOLS2__ __EXE__
  __SUPPORT_CPP_EXCEPTIONS__ _WIN32 WIN32 _WINDOWS __TOOLS2_WINDOWS__ EKA2`) and 8 `-I`.
  The archive also had `-w`; the script now **defaults to warnings on**.
  `gcc_mingw_3_4_2.h` is byte-identical to `cpptoolsplat/mingw-gcc-3.4.5/gcc_mingw_3_4_2.h`
  (sha256 `9c004e2ad0aea7d866c22ac9310e39b911174345bd00ef649edc969d044a12a6`).
- Link: the original link command was never archived (`petran_link.log` is 0 bytes).
  The script's link is `g++ -o petran.exe <20 objects, C-locale sorted>`, unstripped, no
  extra libs. **This is reconstructed by outcome, not recovered.** The output's imports
  are `KERNEL32.dll` and `msvcrt.dll`, linker 2.56, Windows CUI.
- Result of running the script (`petran_build.sh`, §11):
  - all 20 objects **byte-identical** to `work/petran_w/*.o` of the original hand build;
  - warnings on: **0 bytes** of compiler diagnostics for all 20 and for the link;
  - `petran.exe` is 677646 bytes, same as the archived one, and equal after
    masking the PE `TimeDateStamp` (offset `pe+8`) and `CheckSum` (`pe+88`):
    **0 differing bytes**; the unmasked hashes differ (archived `65f0015b…c72e5`;
    the rebuilt exe differs in the timestamp/checksum fields, so its hash varies per link).
  - Later reruns of the final script also produced the executable (rc 0).
- One early rerun appeared to fail (rc 2, no `petran.exe`). It coincided with my own
  `bash -x` debugging, and I could not reproduce it afterwards; the cause was **not**
  identified. The script now fails closed (exit 3 if no exe, exit 4 if not 20 objects).
  Treat that as an open reliability question.
- Case issue found while scripting: the mmp says `pagedCompress.cpp`, the file on disk
  is `pagedcompress.cpp`. Compiling with the mmp spelling changes exactly one byte
  (the COFF `.file` record, `C` vs `c`). A case-sensitive Raptor `tools2` build would
  not find the file under the mmp spelling at all. The script uses the on-disk name.
- Still **not** claimed: equality with an official `petran`, or with a Raptor `tools2`
  build (whose flags differ: `-m32 -s -pipe` plus ~40 `-f` options, a 4.x/host
  compiler). The `iTime` decode anomaly (12 d + 1 h off the file mtime) from the
  artifact audit remains unexplained and was not investigated here.

## 8. Path convention and determinism

- The only build-path leakage found in images: `euser.dll` has 2 strings
  `…/kernelhwsrv_ro/kernel/eka/common/heap_hybrid.cpp` and `…/debugfunction.cpp`;
  `efsrv.dll`, `t_cactw.exe`, `fhserver.exe` have 0 `/tmp/opencode` and 0
  `/home/` / `Sybian` strings.
- Raptor output dirs are `c_<md5 of the lowercase absolute bld.inf path>`
  (`raptor_meta.py:1169-1188`), so they change with the source path.
- **Convention:** one fixed, absolute, space-free `BUILD_ROOT` (owner to choose; a
  research value is `/tmp/opencode/r1-build-repro` as used so far) with
  `$BUILD_ROOT/kernelhwsrv_ro` a symlink to the port's `kernelhwsrv/` (D6; the real
  path contains a space, which Raptor's preprocessor splits). Never build from the
  real path. Any change of `BUILD_ROOT` is expected to change `euser.dll` bytes; this
  was **not** tested.
- Determinism: PE headers and `ar` members carry timestamps (the import libraries
  show distinct mtimes). A rebuild-twice test was not run. Compare with the audit's
  normalised hashes (`/tmp/opencode/r1-artifact-audit/out/normalised_hashes.txt`).

## 9. Exact future clean-build recipe (proposal, **never run end to end**)

Pieces that were each run separately: preflight, the metadata-cpp comparison, the
per-TU compile matrix, `petran_build.sh`, the sed expression. The `sbs` steps below
are the BUILD-REPRO ones with the new variant and `--toolcheck=off`; they were **not**
re-run here.

```
# 0  Prepare (fresh BUILD_ROOT; do not reuse an old epoc32/)
export R=<BUILD_ROOT> P="<port repo>"          # P contains a space: always quote
source $R/env.sh                                # sets EPOCROOT=$R/, SBS_HOME, PATH shims, PERL5LIB, WINEPREFIX
export SBS_GNUCPP=/usr/bin/cpp SBS_GNUSED=/usr/bin/sed   # replaces the hostshim banner wrappers
ln -sfn "$P/kernelhwsrv" $R/kernelhwsrv_ro      # D6
K=$R/kernelhwsrv_ro; sbs="bash $SBS_HOME/bin/sbs --toolcheck=off -d"

# 1  Preflight: pinned inputs, patch hashes, tool versions, sed-expression result (must exit 0)
bash preflight.sh $R "$P"

# 2  Toolchain inputs already pinned by BUILD-REPRO: Python 2.7.18, bzip2 1.0.8, make 3.81,
#    bash 4.0.24, MinGW 3.4.5 zip, D1+D2 patch (scratch_raptor_patches.diff), D3 host flags.

# 3  Install the pinned header and the NAMED VARIANT, then lock them
mkdir -p $R/epoc32/include/variant $R/epoc32/tools/variant
cp $R/dl/platform_paths.hrh               $R/epoc32/include/platform_paths.hrh
cp port_x86_rfile64_v1.hrh                $R/epoc32/include/variant/
cp variant.cfg                            $R/epoc32/tools/variant/variant.cfg
cp $R/src/build/cpptoolsplat/mingw-gcc-3.4.5/gcc_mingw_3_4_2.h  $R/epoc32/include/gcc_mingw/   # petran flag set
sha256sum -c variant.sha256                 # two lines, listed in §11

# 4  EXPORT — only now, only once, with the final variant
$sbs -b $K/kernel/eka/bld.inf                                  -c x86_urel EXPORT
$sbs -b $K/userlibandfileserver/fileserver/group/bld.inf       -c x86_urel EXPORT
test -f $R/epoc32/include/f32file64.h && test -f $R/epoc32/include/f32file64.inl

# 5  genexec (D8, explicit generated-header step; command identical to the upstream recipe)
perl $K/kernel/eka/kernel/genexec.pl -i $K/kernel/eka/kernel/execs.txt \
  -e $R/epoc32/include/platform/exec_enum.h -u $R/epoc32/include/platform/exec_user.h \
  -k $R/epoc32/include/platform/kernel/exec_kernel.h

# 6  petran (D7 scripted); the output is then installed where the old tree had it
bash petran_build.sh $R/src/build $R/epoc32/include $R/tools/mingw $R/petran_out $R/wineprefix
cp $R/petran_out/petran.exe $R/epoc32/tools/petran.exe

# 7  Build (order as BUILD-REPRO)
$sbs -b $K/kernel/eka/euser/bld.inf                              -c x86_urel
$sbs -b $K/halservices/hal/bld.inf                               -c x86_urel
$sbs -b $K/userlibandfileserver/fileserver/group/bld.inf         -c x86_urel -p efsrv.mmp
$sbs -b $K/kerneltest/e32test/group/bld.inf                      -c x86_urel.test -p t_cactw.mmp
$sbs -b $K/kerneltest/f32test/group/bld.inf                      -c x86_urel.test -p fhserver.mmp

# 8  Acceptance (§10) and: git -C "$P/kernelhwsrv" status --short   # must be empty
```

Notes: each `sbs` stdout/stderr should be redirected to a per-stage log under
`$R/logs/`; `-d` is what makes Raptor print the real `Preprocessing command line …`
(`raptor_meta.py:303`) for acceptance test A1. Step 4 order matters (variant first).
Step 6 can run before step 4 because it needs no EXPORT, but the petran script reads
`epoc32/include/gcc_mingw/…` which step 3 installs.

## 10. Acceptance tests (executable; none requires QEMU or E32 execution)

| # | Test | Status |
|---|---|---|
| A1 | After a step-7 build, extract every `Preprocessing command line` from the `-d` log; each must contain `-undef -nostdinc`, `-include …/port_x86_rfile64_v1.hrh`, `-DSBSV2=_____SBSV2`, `-DX86GCC=_____X86GCC`, `-DGCC32=_____GCC32`, `-D__GNUC__=3`, and the include order of §6.1. Compare against the reconstruction in §6.1 and amend it. | **not run** |
| A2 | `bash check_rfile64.sh $R/epoc32/release/x86/urel/efsrv.dll.sym` → exit 0 (all four real). | On the **old** image it prints `PANIC STUB` ×4 and exits 1 (run). Textual positive control: in `-S` output for `v1`, 0 `Panic` calls in the first 24 lines of each of the four functions (v0: 2 each). Not yet run on a rebuilt binary. |
| A3 | `petran -dump e efsrv.dll` still shows the same 28 `RFile64` exports and the same ordinal map as the old `efsrv.dll` (compare against `/tmp/opencode/r1-build-repro/logs/efsrv_ord_off.txt`). | not run |
| A4 | `sha256sum -c` of the variant files (list in §11) and `preflight.sh` exit 0. | preflight run: all pass |
| A5 | `petran_build.sh` twice into different dirs → 20 object hashes equal and exe equal after masking `pe+8` and `pe+88`. | run, repeated, passes (one early unexplained failed rerun, §7) |
| A6 | Rebuild twice from a clean `BUILD_ROOT` and compare normalised hashes (`normhash.py` of the artifact audit). | **not run** |
| A7 | `f32file64.h`/`.inl` exist in `epoc32/include` after EXPORT (step 4). | not run (derived from the metadata `cpp` result: bld.inf content 21 vs 23 lines) |
| A8 | `git -C "$P/kernelhwsrv" status --short` empty. | run: 0 lines |

## 11. Files (embedded; sha256 of the exact scratch copies)

`petran_build.sh` (`da73d74ae767f7036c71130a98379e25e0a2526f64c481a34ef12b411f8e195b`):

```bash
#!/bin/bash
# Proposed scripted petran build (R1-BUILD-CONFIG, D7 closure). Scratch/proposal only.
# Usage: petran_build.sh <build-repo-src-root> <epoc32-include-root> <mingw-root> <outdir> [wine-prefix]
# Reads sources only; writes only under <outdir>. No source edits.
set -u -o pipefail
SRC=$1; INC=$2; MINGW=$3; OUT=$4; export WINEPREFIX=${5:-${WINEPREFIX:-}}
export WINEDEBUG=-all
GPP="wine $MINGW/bin/g++.exe"
mkdir -p "$OUT/obj" "$OUT/log"
IM=$SRC/imgtools/imglib; PE=$SRC/bintools/petools/pefile
# source list = petran.mmp SOURCE lines, in mmp order; pagedCompress.cpp is spelled pagedcompress.cpp on disk (the spelling lands in the COFF .file record)
SOURCES="
$IM/e32uid/e32uid.cpp
$IM/host/h_file.cpp $IM/host/h_mem.cpp $IM/host/h_utl.cpp
$PE/pe_file.cpp $PE/pe_imp.cpp $PE/pe_reloc.cpp $PE/pe_tran.cpp $PE/pe_utl.cpp
$IM/e32image/e32image.cpp $IM/e32image/tr_main.cpp $IM/e32image/imgdump.cpp
$IM/e32image/deflate/decode.cpp $IM/e32image/deflate/encode.cpp $IM/e32image/deflate/deflate.cpp
$IM/e32image/deflate/inflate.cpp $IM/e32image/deflate/panic.cpp $IM/e32image/deflate/compress.cpp
$IM/compress/byte_pair.cpp $IM/compress/pagedcompress.cpp"
# flags: archived hand set (logs/petran_cflags.txt); warnings ON by default (the archived set had -w; objects are byte-identical either way)
W=""; [ "${QUIET:-0}" = 1 ] && W="-w"
FLAGS="$W -fpermissive -O1 -include new -include $INC/gcc_mingw/gcc_mingw_3_4_2.h \
-D__SUPPORT_PE_FILES__ -D__SYMBIAN32__ -D__TOOLS__ -D__TOOLS2__ -D__EXE__ -D__SUPPORT_CPP_EXCEPTIONS__ \
-D_WIN32 -DWIN32 -D_WINDOWS -D__TOOLS2_WINDOWS__ -DEKA2 \
-I$IM/inc -I$IM/compress -I$PE -I$IM/e32image -I$IM/e32uid -I$IM/host -I$INC -I$INC/platform"
rc_all=0
for s in $SOURCES; do
  b=$(basename "$s" .cpp | tr 'A-Z' 'a-z')
  $GPP $FLAGS -c "$s" -o "$OUT/obj/$b.o" > "$OUT/log/$b.out" 2> "$OUT/log/$b.err"; rc=$?
  echo "$b rc=$rc err_bytes=$(wc -c < "$OUT/log/$b.err")" | tee -a "$OUT/log/compile_summary.txt"
  [ $rc -ne 0 ] && rc_all=1
done
[ $rc_all -ne 0 ] && { echo "COMPILE FAILED"; exit 1; }
# link: unstripped, objects in lexicographic order (matches archived binary; see report)
( cd "$OUT/obj" && $GPP -o ../petran.exe $(ls *.o | LC_ALL=C sort) ) > "$OUT/log/link.out" 2> "$OUT/log/link.err"
echo "link rc=$? err_bytes=$(wc -c < "$OUT/log/link.err")" | tee -a "$OUT/log/compile_summary.txt"
[ -s "$OUT/petran.exe" ] || { echo "LINK PRODUCED NO petran.exe"; exit 3; }
[ "$(ls "$OUT"/obj/*.o | wc -l)" = 20 ] || { echo "EXPECTED 20 OBJECTS"; exit 4; }
sha256sum "$OUT"/obj/*.o "$OUT/petran.exe" > "$OUT/log/sha256.txt"
```

`check_rfile64.sh` (`1d09fa15772cb1af79062a16ca1aee33994f1d1c9f8f31c500fc164f4ad97efe`):

```bash
#!/bin/bash
# Acceptance test: RFile64::{Open,Create,Replace,Temp} in a built efsrv are real implementations, not Panic stubs.
# Usage: check_rfile64.sh <efsrv.dll.sym>   (the PE-form .sym next to efsrv.dll; host binutils, static only)
# Exit 0 = all four are real, 1 = a Panic stub or missing symbol or no disassembly, 2 = usage error
SYM=$1
[ -r "$SYM" ] || { echo "usage: $0 efsrv.dll.sym"; exit 2; }
fail=0
for fn in __ZN7RFile644OpenER3RFsRK7TDesC16j __ZN7RFile646CreateER3RFsRK7TDesC16j \
          __ZN7RFile647ReplaceER3RFsRK7TDesC16j __ZN7RFile644TempER3RFsRK7TDesC16R4TBufILi256EEj; do
  addr=$(nm "$SYM" | awk -v f="$fn" '$3==f{print $1}')
  [ -n "$addr" ] || { echo "$fn: NOT FOUND"; fail=1; continue; }
  body=$(objdump -d -M intel --start-address=0x$addr --stop-address=$((0x$addr + 24)) "$SYM" 2>/dev/null | grep -c '^ *[0-9a-f]*:')
  [ "$body" -gt 0 ] || { echo "$fn @0x$addr: NO DISASSEMBLY (fail closed)"; fail=1; continue; }
  # stub: the first call within 24 bytes targets Panic, and the body returns -5 (KErrNotSupported)
  if objdump -d -M intel --start-address=0x$addr --stop-address=$((0x$addr + 24)) "$SYM" | grep -q 'call.*Panic12TClientPanic'; then
    echo "$fn @0x$addr: PANIC STUB"; fail=1
  else
    echo "$fn @0x$addr: real implementation (no Panic call in first 24 bytes)"
  fi
done
exit $fail
```

`preflight.sh` (`f03791c9f0f4c4f668214dd0abff2988e5a6e4429f7458dd4a99653d0c507d2d`):

```bash
#!/bin/bash
# Pre-flight for the proposed toolcheck policy (replaces the banner-rewriting cpp/sed shims).
# Usage: preflight.sh <BUILD_ROOT> <PORT_REPO>      Exit 0 only if every check passes.
R=$1; P=$2; fail=0; export WINEDEBUG=-all
chk(){ if [ "$2" = "$3" ]; then echo "ok   $1: $2"; else echo "FAIL $1: got '$2' want '$3'"; fail=1; fi; }
sha(){ sha256sum "$1" | cut -d' ' -f1; }
chk build-repo-HEAD "$(git -C $R/src/build rev-parse HEAD)" f458a4ce83f74d603362fe6b71eaa647ccc62fee
chk build-repo-tracked-mods "$(git -C $R/src/build status --short | grep -v '^??' | awk '{print $2}' | sort | tr '\n' ' ')" "sbsv2/raptor/bin/gethost.sh sbsv2/raptor/util/talon/process.c "
chk raptor-patch-diff "$(sha $R/artifacts/scratch_raptor_patches.diff)" fd504a6009d3a70039bece577b0f2a3082987d56b415f4209a93ba50f289e50d
chk kernelhwsrv-HEAD "$(git -C "$P/kernelhwsrv" rev-parse HEAD)" 0c3208650587ac0230aed8a74e9bddb5288023eb
chk kernelhwsrv-clean "$(git -C "$P/kernelhwsrv" status --short | wc -l)" 0
chk mingw-zip "$(sha $R/dl/mingw-symbian-001.zip)" 0c2ac07f9a3cad87e49d04814c37c0dca71143caac0777409f611e8daa07594c
chk platform_paths.hrh "$(sha $R/dl/platform_paths.hrh)" 8a8ebdc3f40804d630def1af4c0c3a68568a79d13eb08d0661d9ba252dd96f31
chk compiler "$(wine $R/tools/mingw/bin/g++.exe -dumpversion 2>/dev/null | tr -d '\r')" 3.4.5
chk python "$($R/tools/py27/bin/python --version 2>&1)" "Python 2.7.18"
chk make "$($R/tools/sbshost/bin/make --version 2>&1 | head -1)" "GNU Make 3.81"
echo "info metadata-cpp : $(cpp --version | head -1)   (policy: host cpp, equivalence 24/24 on tested files; see report)"
echo "info sed          : $(sed --version | head -1)"
# the one sed expression Raptor runs on the EFSRV def (e32importlib.mk:54) must give the expected result
D="$P/kernelhwsrv/userlibandfileserver/fileserver/eabi/efsrvu.def"
out=$(sed -r 's%(^\s*_ZT[I|V].*NONAME).*$%\1 ABSENT%;s%St9__va_list%Pc%' < "$D")
chk sed-expr-ABSENT-count "$(echo "$out" | grep -c ABSENT)" 20
echo "info sed-expr-output-sha256 : $(echo "$out" | sha256sum | cut -c1-16)"
exit $fail
```

Pinned inputs used by the above:

| Input | sha256 |
|---|---|
| `mingw-symbian-001.zip` | `0c2ac07f9a3cad87e49d04814c37c0dca71143caac0777409f611e8daa07594c` |
| `platform_paths.hrh` (`deviceplatformrelease@b7c838933e1ac74d3873b8722538aba029479ab2`, `symbianosbld/cedarutils/`) | `8a8ebdc3f40804d630def1af4c0c3a68568a79d13eb08d0661d9ba252dd96f31` |
| `Symbian_OS_future.hrh` (same commit, reference only, not installed) | `1011d0e00d1af6488916ade7a7f1f321dc8759091b6f2c6b525a069bfd621220` |
| `Symbian_OS_tb101sf.hrh` / `Symbian_OS_tb102sf.hrh` (reference only) | `61ad1dc71167b504aeb75e48c4b7ef09f088e5814d4386666b2cb041598f56e9` / `d2aeb785337ef938c5c8586cabb9b2784cd2bedf08941f0f3498ba29b70cadb8` |
| `scratch_raptor_patches.diff` (D1+D2) | `fd504a6009d3a70039bece577b0f2a3082987d56b415f4209a93ba50f289e50d` |
| `scratch_gethost.patch` / `scratch_talon_eintr.patch` | `6c81a2f7456e95037173012a62ac1158f872d1c2aadab915884e82bd1936660a` / `3bdd995ffa1979966645baeb964b824f7e85928f7dc78394070cc1c7bc9ab38d` |
| `port_x86_rfile64_v1.hrh` / `variant.cfg` | `500889884b3063c1b0712c000d2c62f55e70774cbfda41b51b82f343be277106` / `2c505b4bb43e4b74e362589ce78f866053e4e9d8283688e47cca321c58e08cd0` |
| `gcc_mingw_3_4_2.h` | `9c004e2ad0aea7d866c22ac9310e39b911174345bd00ef649edc969d044a12a6` |
| archived `petran.exe` | `65f0015bb89d569748fd067d3adbdbd35950f1e2c5da1766af3c0e8d568c72e5` |
| `e32uid.o` (petran, reproduced) | `9357896d10cc813a31dc42d1e85faae17b11779ecb714358b80f3275260d955e` |

The preflight script, as written, checks the patch diff, MinGW zip, `platform_paths.hrh`,
the build-repo and `kernelhwsrv` revisions, compiler/Python/make versions and the sed
expression. It does **not** yet check the variant files or `gcc_mingw_3_4_2.h`; the
`variant.sha256` list in step 3 does not exist yet and must be created from the two
variant hashes above (small gap, noted).

## 12. Unknowns and blocked assumptions

- No `sbs` run with the new variant exists. Everything about step 4-7 with the new
  variant and `--toolcheck=off` is **unverified** (only the per-TU compile matrix,
  the metadata `cpp` comparison and `petran_build.sh` were run).
- The metadata `cpp` command is reconstructed from source, not captured (A1). The
  `.test` variant's macro set was not checked.
- MinGW `cpp` CR output and blank-line differences were not run through the mmp
  parser.
- Which other `SYMBIAN_*` macros the target kit should define is an **owner decision**:
  `DISTINCT_LOCALE_MODEL` (locale behaviour), `F32_ENHANCED_CHANGE_NOTIFICATION`,
  `ENABLE_SPLIT_HEADERS`; my recommendation is to leave them off until a consumer needs them.
  `FIX_TDESC_CONSTRUCTORS` should stay off.
- Final `efsrv.dll` link layout and the real `RFile64` IPC behaviour are unverified; wire
  framing is R1-WIRE, loader string resolution is R1-STARTUP. R1-ABI/v1 import identity
  must not be frozen from this.
- `petran`: link command reconstructed by outcome, one early unexplained failed rerun,
  no comparison with an official or `tools2`-built tool.
- Official-kit equivalence, determinism, `udeb`, SMP, licences (EPL source,
  GPL/LGPL MinGW) remain open. Disk: `/tmp` is tmpfs with a user quota; a
  full clean rebuild needs roughly the 2.9 GB the previous scratch used.

## 13. Handoff

- **Card:** R1-BUILD-CONFIG. **Base:** port `fa62323e…` (dirty, 62 status lines, none
  edited by this card); upstream `kernelhwsrv` `0c320865…` clean; build repo
  `f458a4ce…` + D1/D2. Branch as checked out; no commits or pushes.
- **Contract revision:** none issued; R1-ABI/v0 remains not frozen. Proposes variant
  name `port-x86-rfile64-v1` for the integrator's review.
- **Changes:** this report only (`qemu-port/docs/r1_build_config_v0.md`). Scratch in
  `/tmp/opencode/r1-build-config/` (not committed). Existing images and the
  BUILD-REPRO tree were not modified.
- **Tests:** §10 (what was and was not run). No QEMU, stable-suite or hardware runs, no
  E32 execution.
- **Manual step:** coordinator/owner to (1) approve or amend the variant (one macro),
  (2) decide the D4 policy (`--toolcheck=off` + preflight vs. MinGW-cpp wrapper),
  (3) choose the fixed `BUILD_ROOT`, (4) decide whether the other three macros
  are wanted, (5) authorise one clean rebuild to run A1-A3 and A6.
- **Limits:** §12. Not a promised SDK; no redistribution.
- **Blocked assumptions:** official-kit equivalence; live `sbs` behaviour with the
  new variant; runtime behaviour of the built images.
- **Integration order:** this report → coordinator review of the variant manifest →
  (authorised) clean rebuild → R1-STARTUP consumes new hashes → R1-ABI/v1 only
  after R1-STARTUP and R1-WIRE.

