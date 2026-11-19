# R1-SDK-ROUTE — upstream x86 build/SDK route v0 (route identified, not reproduced)

Scope: research only; feeds, does not freeze, R1-ABI/v1. Port base `fa62323eea7bc96cefd666e40f5189567731bd44`; read-only `kernelhwsrv/` `0c3208650587ac0230aed8a74e9bddb5288023eb`. Nothing in the repo besides this report changed. No binary was redistributed or committed; downloaded artifacts live only under `/tmp/opencode/r1-sdk-route/`. **No EUSER/EFSRV DLL, import library or E32 image was built.** Date of external fetches: 2026-10-21.

## Result in one paragraph

This checkout contains no build tools (`kernelhwsrv/` is a source-only repo: no Raptor, no `abld`, no `epoc32`, no PE/E32 post-linker). The sibling SymbianSource repo `oss.FCL.sftools.dev.build` (Raptor `sbsv2`, legacy `sbsv1`, tools source, and a packaged MinGW GCC 3.4.5 for Symbian) **does** define an x86 GCC build, and it answers the earlier "where is bx86gcc EFSRV?" question: **there is none by design**. Raptor resolves x86 `.def` files from `bx86gcc` first and falls back to `eabi`, rewriting the EABI def for x86. So the candidate matching pair is EUSER from `bx86gcc/euseru.def` and EFSRV from `eabi/efsrvu.def` (transformed), both imported through a `dlltool`-generated i386 import library. This is a **documented upstream route, not a verified one**: no step has been executed here.

## Evidence

### What the workspace has (and lacks)
- `kernelhwsrv/` is a shallow clone of `github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv` (`README.md` layout section). Contains `bld.inf`/`.mmp`/`.mmh` and `.def` files only. `kernel/eka/base_e32.mrp:8` lists `kernel\eka\bx86gcc`.
- `sdk/` and `sdk.zip` at the workspace root are **this port's own freestanding C SDK** (`sdk/README.md:3-5,41-52`: stock `gcc -m32`, `elf2e32.py`, "Current SDK apps are import-free; this port's E32 support is not the complete Symbian ABI/toolchain"). They contain no `euser.lib`/`efsrv.lib`, no `.def`, no upstream tool. It is not the route; do not confuse the two SDKs.
- Host scan (`find / -xdev` for `euser.lib|efsrv.lib|*.dso|elf2e32*|bldmake*|abld*|sbs|epoc32|*symbian*sdk*`): nothing relevant. Present: `gcc 16.2.1`, `perl 5.42.3`, `python3 3.14.8`, `make`, `wine 11.0`. **Absent: Python 2** (Raptor 2.15.x is Python 2 code, e.g. `has_key`, `type(x)==list` in `raptor_meta.py:2458-2465`), `FindBin.pm` (Perl core module missing in this distro install; `prepdef.pl` failed on `use FindBin`), any `epoc32` tree.

### Upstream x86 GCC route (external repo, master `f458a4ce83f74d603362fe6b71eaa647ccc62fee`, 9782 tree entries, pushed 2016-11-06)
Repo: `https://github.com/SymbianSource/oss.FCL.sftools.dev.build` (GitHub `license: null`; source headers say EPL v1.0; `sbsv2/raptor/license.txt` says EPL v1.0).
- `sbsv2/raptor/notes/x86_initial.txt`: Raptor "supports a basic x86 OS build on Windows given ... an appropriate MinGW GCC toolchain"; **alpha quality**; build EXE/DLL/IMPLIB/LIB; `SBS_GCCX86BIN` must point at MinGW `bin`; configs `sbs -c x86_urel|x86_udeb`; checks GCC **3.4.5** and requires "a custom GCC toolchain that differs from vanilla MinGW releases"; output `epoc32/release/x86/{lib,urel,udeb}` (optional `release_x86gcc` variant). Component must have empty/`X86`/`DEFAULT`/`BASEDEFAULT` `PRJ_PLATFORMS` (matches `kernelhwsrv` `bld.inf` `PRJ_PLATFORMS BASEDEFAULT` and `#if defined(GENERIC_X86)` in `kernel/eka/euser/bld.inf:32`).
- `sbsv2/raptor/lib/config/gcc_x86.xml`: macros `X86GCC GCC32` / `__X86GCC__ __GCC32__`, `-masm=intel -fno-rtti -fno-exceptions -fcheck-new`, `-nostdlib`, `-Wl,--dll`, `--entry`, preinclude `epoc32/include/gcc.h` (= `kernel/eka/include/gcc.h`, comment "Currently only tested with GCC 3.4.4"), `DLLTOOL`, `AR`, linker `g++`. `x86.xml`: `BASE_ARCH=x86`, `POSTLINKER=petran`, `IMPLIBTOOL=dlltool`, `PLATMACROS.CONFIG="X86 GENERIC_X86"`, `IMPLIB_EXTENSION=lib`.
- `sbsv2/raptor/python/raptor_meta.py:43-50`: `PlatformDefaultDefFileDir['X86'] = ['bx86gcc','eabi']`; `resolveDefFile` (`:2445-2521`) uses `../bx86gcc/<target>u.def` and, **if that file does not exist**, `../eabi/<target>u.def` flagged `RESOLVED_DEFFILE_SECONDARY`.
- `sbsv2/raptor/lib/flm/e32importlib.mk` (`importlibtarget_prepfile_x86`, `importlibtargetversioned_x86`): for a secondary (EABI) def, `sed` turns `_ZT[IV]...NONAME` into `... ABSENT` and `St9__va_list` into `Pc`, then `prepdef` and `dlltool -m i386 --input-def ... --dllname ... --output-lib <name>.lib`. Post-link is `petran` (PE -> E32) with `-sid -uid* -capability -stack -heap ...` (`e32postlink.mk`, `e32postlink_x86`).
- Legacy `sbsv1/abld/platform/cl_x86gcc.pm` (also `cl_x86.pm`) is the older `abld` equivalent: `dlltool -m i386`, `prepdef.pl`/`gendef.pl`, link with `g++ -nostdlib --dll --entry _E32Dll|_E32Startup`; `sbsv1/abld/e32util/checkgcc.pm`.
- Toolchain package: `cpptoolsplat/mingw-gcc-3.4.5/` — `mingw-symbian-001.zip` (20,097,486 bytes; **sha256 `0c2ac07f9a3cad87e49d04814c37c0dca71143caac0777409f611e8daa07594c`**, 943 files, git blob `9958a52998e7e6474f39da4d283604a84157e79e`) exported to `/epoc32/gcc_mingw`. Its `readme.txt` says it is made from MinGW `binutils-2.17.50-20060824-1`, `gcc-core/g++ 3.4.5-20060117-1`, `mingw-runtime-3.14`, `w32api-3.11`. `COPYING` and `COPYING.LIB` (GPL/LGPL) ship inside. Windows PE executables; `wine bin/g++.exe --version` prints `g++.exe (GCC) 3.4.5 (mingw special)`.
- Other tools needed as **source only** in that repo (no prebuilt `petran`, `elf2e32`, `checklib`, `rcomp` binaries found in the tree listing): `bintools/petools/group/petran.mmp` (+ `imgtools/imglib` sources), `sbsv1/abld/e32util/{prepdef,gendef,efreeze}.pl`, `bintools/elftools/def2dll.pl`, `bintools/checklib`, `bintools/rcomp`. `sbsv2/cpp-raptor` carries a prebuilt `linux-i386` `cc1` for GCC 4.4.0 for the preprocessor, which is not the x86 target compiler.

### The EUSER/EFSRV pair this implies
Computed from the workspace `.def` files:
- EUSER x86gcc: `kernel/eka/bx86gcc/euseru.def`, 2503 names, no duplicate ordinals; `CActiveScheduler::Start` @ 426, `Install` @ 428 (the same names are 426/428 in `kernel/eka/eabi/euseru.def`).
- EUSER bx86gcc vs eabi are **not** interchangeable: 2496 shared names, **362 with different ordinals** (e.g. `__e32_memory_barrier` 913 vs 2438), 7 only in bx86gcc, 50 only in eabi. Never substitute one for the other.
- EFSRV: `bx86gcc/efsrvu.def` does not exist; the Raptor fallback is `userlibandfileserver/fileserver/eabi/efsrvu.def` (421 names, ordinals 1..421; `RFs::Connect(int)` `_ZN3RFs7ConnectEi` @ 68, `RFile::Open` `_ZN5RFile4OpenER3RFsRK7TDesC16j` @ 93, `RFile::Write(TDesC8 const&)` @ 97, `RFile::Close()` @ 300, `RFs::Close()` @ 390). The Raptor `sed` step marks all 19 `_ZTI/_ZTV` lines `ABSENT` (one already was); with the separate pre-existing `RRawDisk::Caps` `ABSENT` at ordinal 358, the transformed def has 20 `ABSENT` lines (checked by running the `sed` on a scratch copy). `St9__va_list` does not occur in the EFSRV def, so that rewrite is a no-op for EFSRV; EUSER uses its own bx86gcc def and is not rewritten.
- The older bx86 (MSVC) EFSRV (`bx86/efsrvu.def`, `Open` @ 140) is an unrelated ordinal space; the earlier R1-TOOLCHAIN conclusion not to renumber bx86 stands.

### ABI probe with the real upstream compiler (Wine, scratch only)
`wine zipx/bin/g++.exe -x c++ -masm=intel -fno-rtti -fno-exceptions -fcheck-new -O2 -c abi.cpp` on `int invoke(RFile&, RFs&, const TDesC16&) { return f.Open(fs,n,0); }` produced: undefined symbol `__ZN5RFile4OpenER3RFsRK7TDesC16j` (Itanium-style name with the MinGW leading `_`), caller stores `this`, `fs`, `n`, `0` into `(%esp)..0xc(%esp)` (cdecl-style outgoing area, **`this` on the stack, not ECX**), reloc `DISP32`. Predefs: `_WIN32`, `__MINGW32__`, `__i386__`, `__USER_LABEL_PREFIX__ _`, `__WCHAR_TYPE__ short unsigned int`, `__WCHAR_MAX__ 65535`. Consequences (all **unverified until a real EUSER/EFSRV is linked**):
1. The EABI-derived EFSRV names (`_ZN5RFile4Open...`) do correspond to what this GCC emits, consistent with the Raptor fallback design (and consistent with, but not proof of, matching host-GCC behavior from R1-TOOLCHAIN).
2. Class member calls use plain stack `this`, so the loader/ABI story is "cdecl-like C++ with Itanium names", not MSVC `thiscall`.
3. The images are **PE-derived**: a `petran` conversion yields the E32 import table; entry symbols are `_E32Startup` (EXE) / `_E32Dll` (DLL) per `cl_x86gcc.pm`, `prepdef ... E32Startup`. The port's current loader/`elf2e32.py` ELF flow is not this format; R1-STARTUP must check import-by-ordinal, relocation kind and entry-point handling against real `petran` output.

## Blockers and exact acquisition list
To reproduce, the integrator must provision all of:
1. Raptor `sbsv2` at a recorded revision from `oss.FCL.sftools.dev.build` (suggested pin: tree `f458a4ce83f74d603362fe6b71eaa647ccc62fee`, release notes show 2.15.3). It needs **Python 2.7** (not available on this host; the upstream Linux package supplied its own) and GNU make. Unknown whether Raptor 2.15 can run on this Linux host without the supplied Python/binaries; the x86 note says "on Windows".
2. The packaged MinGW GCC 3.4.5 (hash above) with `SBS_GCCX86BIN` set; runs only under Wine here (verified: `g++`, `nm`, `objdump`, `-E -dM` ran). Whether `ld`/`dlltool` work for full DLL links under Wine is **not tested**.
3. Host-built tools from source (not yet built): `petran` (needs `imgtools/imglib` + a host C++ build, `__SUPPORT_PE_FILES__`), `prepdef`/`gendef`/`efreeze` Perl (need working `FindBin`, `Defutl.pm` etc. from `sbsv1/abld/e32util`), `checklib`, `rcomp`, `bmconv` if resources are needed.
4. A staged `epoc32` tree: exported headers (`kernelhwsrv` `PRJ_EXPORTS`, `gcc.h`, `u32exec.h`, generated `genexec` stubs — `kernel/eka/extension/genexec.{meta,mk}` and `kernel/eka/kernel/genexec.pl`) and the other layers' headers that EUSER/EFSRV `#include` (the `OS_LAYER_SYSTEMINCLUDE_SYMBIAN` roots). Unknown how many sibling `sf.os.*`/`sf.mw.*` repos are needed beyond `kernelhwsrv`; `kernelhwsrv/package_map.xml` / `build.config.xml` were not mined for dependencies.
5. The EUSER and EFSRV build themselves: `kernel/eka/euser/euser.mmp`+`euser.mmh` (x86 sources: `epoc/x86/*`, `common/x86/*`, `us_gcc.cpp`, `gcchelp.cpp` under `GCC32`) and `userlibandfileserver/fileserver/sfsrv/efsrv.mmp`+`efsrv.mmh` (needs `euser.lib hal.lib`; `halservices/hal` has only `bx86/halu.def` MSVC style and `eabi/halu.def`, so HAL also falls back to EABI). EFSRV builds only if `SMP` is **not** defined (`fileserver/group/bld.inf:53`); `euser_lib`/`rpipe` likewise. The relevant GCC x86 build history (`kernel/eka/release.txt` ~754, 1462, 1532, 2282, 2300, 2922, 5653, 6006) shows the platform existed, but includes known failures on `x86gcc.fmm`; those are about kernel/e32test, not proof EFSRV builds.

## Legal/usability (not a legal opinion)
- Sources: EPL v1.0 file headers (kernelhwsrv and the build repo). The repository has no top-level LICENSE; GitHub reports no license. Owner should confirm before any redistribution.
- The MinGW package is GPL/LGPL (`COPYING`, `COPYING.LIB` inside) built from mingw.org components; any redistribution of binaries needs a separate decision. Compiled output linking `mingw-runtime`/`w32api` was not examined; the Symbian use is `-nostdlib`.
- Built EUSER/EFSRV DLLs and import libraries would be derived artifacts; do not check them into this repo or `sdk.zip` without that decision. This report copies none of them.

## Unknowns that still gate ABI/v1
- Whether the Raptor x86 route actually produces a working EFSRV/EUSER pair from this commit (alpha-quality note; GCC 3.4.5 vs the repo's later C++ usage; `release.txt` only claims x86gcc builds in 2008-2010).
- Whether the EFSRV import ordinals from the EABI def are the ones the *built* `efsrv.dll` exports (frozen EABI numbering in a PE x86 build; Raptor generates the `.lib` from the def and checks via `FIX_EXPORTS_USING_EXP_FILE`). Needs a `petran`/`objdump` export-map dump.
- E32 import-section layout/ordinal encoding for PE-derived images, relocation types, `_E32Startup` behavior, TLS/heap setup (R1-STARTUP) and F32 message framing (R1-WIRE) — all unverified.
- Whether any single unchanged proof app (R1-PROOF-SOURCE) builds under this route.

## Executable acceptance tests (when the route is provisioned)
1. `sha256sum` the Raptor tree, MinGW zip (expect hash above) and every built tool; record versions.
2. `sbs -c x86_urel -b kernelhwsrv/kernel/eka/euser/bld.inf` and `-b .../fileserver/group/bld.inf` (not SMP). Save logs; EUSER and EFSRV DLLs and `.lib` files must exist.
3. `objdump -p`/`petran`/`pe_dump` on `euser.dll`/`efsrv.dll`: export-ordinal map must contain `CActiveScheduler::Install/Start` and `RFs::Connect`/`RFile::Open/Read/Write/Close` at the ordinals recorded here (or the discrepancy is the finding).
4. Build `t_cactw` and a file app unchanged with the same configuration; dump imports (ordinals, DLL names, UIDs, capability) and verify entry symbol and relocations.
5. Only then load into QEMU on a unique disposable FAT32 image; separate card, no manual hardware claim.

## Handoff
- Card / base / branch: R1-SDK-ROUTE; port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44` (working tree dirty by other work); upstream `0c3208650587ac0230aed8a74e9bddb5288023eb`.
- Contract revision: R1-ABI v0 (not frozen); this report adds input, no freeze.
- Changes: only `qemu-port/docs/r1_sdk_route_research_v0.md` (new).
- Tests: static source/def analysis; Wine-run MinGW 3.4.5 compile probe plus `nm`/`objdump`; Python def-ordinal comparison; GitHub tree and raw-file reads. No build of EUSER/EFSRV, no QEMU, no stable suite.
- Manual step: none for the owner; the integrator must decide whether to provision the Raptor/Python 2/MinGW stack.
- Limits: route documented, not executed; one external repo snapshot; Wine and ld/dlltool behavior not exercised; no legal clearance.
- Blocked assumptions: matching EFSRV ordinals, PE-derived E32 import format, startup, wire framing, proof-app choice (see above).
- Integration order: this -> a follow-up "reproduce the route" card (only if provisioned) -> R1-STARTUP/R1-WIRE/R1-PROOF-SOURCE reviews -> integrator ABI/v1 -> R1-EUSER/R1-FS/R1-FAT.
