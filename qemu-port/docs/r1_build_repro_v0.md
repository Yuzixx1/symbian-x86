# R1-BUILD-REPRO v0 — scratch reproduction of the upstream x86 build route

Status: **research report, NOT a frozen ABI.** Date 2026-10-21.
Port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44` (many uncommitted files).
Upstream `kernelhwsrv/` HEAD `0c3208650587ac0230aed8a74e9bddb5288023eb`,
**unmodified** (`git status --short` = 0 lines before and after).
Scratch prefix: `/tmp/opencode/r1-build-repro/` (≈2.9 GB, never committed).
Nothing was run under QEMU or on hardware. No E32 image was executed anywhere.

## Result in one paragraph

The documented upstream route (Raptor 2.15.3 `-c x86_urel`, MinGW GCC 3.4.5
under Wine, `bx86gcc`-first def resolution) **produced genuine E32 images** for
`euser.dll`, `efsrv.dll`, `t_cactw.exe` and `fhserver.exe`, plus `.lib` import
libraries. It required **three source/tool workarounds and several scratch
shims** (listed under "Deviations"); none touch `kernelhwsrv/`, but two are
source patches to the Raptor clone and the card said to ask first. They are
reported, not hidden. Everything about runtime behaviour remains unverified.

## Pinned inputs (all in scratch; none redistributed)

| Item | Pin | Verification |
|---|---|---|
| Build tools repo `SymbianSource/oss.FCL.sftools.dev.build` | commit `f458a4ce83f74d603362fe6b71eaa647ccc62fee` (GitHub API: this is the **commit**; its tree is `968aa9bac548662b49af4e2b873b61873ba6f680`) | `git rev-parse HEAD` in clone |
| MinGW 3.4.5 package `mingw-symbian-001.zip` | sha256 `0c2ac07f9a3cad87e49d04814c37c0dca71143caac0777409f611e8daa07594c` | hash recorded; `g++ -dumpversion` = 3.4.5 under Wine |
| Python 2.7.18 | sha256 `da3080e3b488f648a3d7a4560ddee895284c3380b11d6de75edb986526b9a814` | GPG signature good (key C01E1CAD…D4FF, Benjamin Peterson; key fetched from keyserver.ubuntu.com, **web of trust not established**) |
| bzip2 1.0.8 (for Python `bz2`) | sha256 `ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269` | GPG signature good (Mark Wielaard, 12768A96…D99A78) |
| GNU make 3.81 | `f3e69023771e23908f5d5592954d8271d3d6af09693cecfd29cee6fde8550dc8` | shipped inside the build repo (`sbsv2/raptor/util/ext`) |
| bash 4.0 + patches | `9793d394…c50518`, patches `8802c48d…d91e` | shipped inside the build repo |
| `platform_paths.hrh` | sha256 `8a8ebdc3f40804d630def1af4c0c3a68568a79d13eb08d0661d9ba252dd96f31`, from `oss.FCL.sf.os.deviceplatformrelease` commit `b7c838933e1ac74d3873b8722538aba029479ab2`, path `symbianosbld/cedarutils/` | hash only |
| Perl `FindBin.pm` (host perl 5.42.3 lacks it) | sha256 `a135b2fb3a94d062276239e93884374b0a19b7a62dbc8b97e8b73817beb572d9`, from the perl5 tag matching the host version | hash only |

Correction to earlier reports: `r1_sdk_route_research_v0.md` and earlier text
call `f458a4ce…` a "tree"; it is the commit SHA.

Host: gcc 16.2.1, perl 5.42.3, python3 3.14, wine-11.0 (Staging, wow64 prefix;
`WINEARCH=win32` is rejected, a win64 prefix runs the 32-bit PE tools fine).

## Stages and outcomes

| # | Stage | Outcome |
|---|---|---|
| 1 | Real `g++`/`dlltool`/`ld`/`ar` under isolated Wine prefix (probe DLL; **probe only, not an artifact**) | OK. Log `logs/stage1_linker_probe.log` |
| 2 | Python 2.7.18 built into scratch | OK after adding bzip2 in scratch (`bz2` needed by Raptor's log writer) |
| 3 | Raptor host tools: make 3.81, bash 4.0.24, talon, ransleep | OK, with deviations D1–D4 |
| 4 | `sbs --version` | `sbs version 2.15.3` |
| 5 | EXPORT pass for `kernel/eka` and `fileserver/group` into scratch `epoc32` | OK, 0 errors, 296 headers; needs D5, D6 |
| 6 | `petran.exe` | Built by hand, **not** via Raptor `tools2` (D7). Runs; `-v`/`-dump` work |
| 7 | `prepdef.pl`/`gendef.pl`/`def2dll.pl` | Copied from the build repo; `perl -c` OK with FindBin supplied |
| 8 | Generated `exec_user.h`/`exec_enum.h`/`kernel/exec_kernel.h` | Needed `genexec.pl` (D8) |
| 9 | HAL | `halservices/hal/bld.inf` OK → `hal.lib` |
| 10 | EUSER (`euser.dll`, `euser.lib`, `edll.lib`, `edllstub.lib`, `eexe*.lib`, `rpipe.dll`) | OK, 0 errors |
| 11 | EFSRV (`efsrv.dll`, `efsrv.lib`) | OK, 0 errors |
| 12 | `t_cactw.exe` (`e32test`) and `fhserver.exe` (`f32test`), `-c x86_urel.test` | OK, 0 errors |

## Artifacts (scratch paths; sha256)

`epoc32/release/x86/urel/`:

- `euser.dll` `0c833111d1f0656ef7d75848e76d9a90459ce449babc2f636d6d80e5f65dcf60`
- `efsrv.dll` `69051f33b9b7f07b16ca5352b7a8d938490ed151e2d19db05f810f09939040fe`
- `t_cactw.exe` `c096c6a27d65df49d896d3f4b3fc2d5d04e53461cdab93f663c3d1405a94b133`
- `fhserver.exe` `825d43353d3008ef8372aa2b6e878c6c530a9ff9456c83a8a7bf7f4bdf627faf`
- `epoc32/tools/petran.exe` `65f0015bb89d569748fd067d3adbdbd35950f1e2c5da1766af3c0e8d568c72e5`
- `epoc32/release/x86/lib/euser.lib` `55b3c739…a353446`, `efsrv.lib` `0d840f4c…e1d19db0`
  (existence and hash only; **import-library contents were not inspected**)

Every `*.dll`/`*.exe` has a sibling `*.sym` that is the **PE intermediate**
(`file`: "PE32 executable for MS Windows 4.00 (DLL), Intel i386"). The file
without `.sym` is the **converted E32 image** (`file`: "Psion Series 5
binary"). Do not confuse them. Size of `euser.dll` (412076) equals the
E32 header `File Size 0x649ac`.

Redistribution: these are build products of EPL source and GPL/LGPL MinGW
tooling. They stay in scratch. Licence clearance is still the owner's call.

## Inspected E32 facts (tool: `petran.exe -v` / `-dump`)

`euser.dll`: "EPOC Dll for X86 CPU"; header format 2; not compressed; UIDs
`10000079 1000008d 100039e5`; SID `100039e5`; VID `70000001`; capabilities
`00000000 000fffff` (the mmp's `+all`); version 10.0; code size `0x63a30`; code
link address `0x10000000`; relocation offset `0x63af8`, `0x64a` relocs; entry
point at code +`0x264`; 2503 export slots; min/max heap 4 KiB/1 MiB, stack 8 KiB;
"No Exception Descriptor"; "Imports are PE-style without redundant ordinal lists".

`t_cactw.exe`: "EPOC Exe for X86 CPU"; UIDs `1000007a 0 0`; SID 0; VID
`70000001`; **capabilities none**; code link `0x00400000`, data link
`0x00402e24`; entry at code +`0x260`; 222 relocs; 1 DLL reference; imports **50
ordinals from `euser{000a0000}[100039e5].dll`** (list: `artifacts/t_cactw_euser_imports.txt`).

`fhserver.exe`: imports **22 from `efsrv{000a0000}[100039e4].dll`** and **65 from
`euser{000a0000}[100039e5].dll`**. EFSRV set includes ordinals 68
(`RFs::Connect(int)`), 93 (`RFile::Open`), 97 (`RFile::Write(TDesC8 const&)`),
300 (`RFile::Close`), 259 (`RFile::Read(int,TDes8&)`), 263 (`RFile::Seek`).

Import names carry the `{000a0000}` module version (10.0) and the owner UID in
brackets. A loader that matches DLLs by bare name will not match these strings;
this is a design input for R1-STARTUP, not something this card verified.

## Ordinal consistency checks (what was actually compared)

All checks compare three independent things: the def file Raptor used, the
exports `petran -dump e` reports in the E32 image, and the symbol addresses
`nm` reports in the matching PE `.sym`.

- **EUSER** used `kernel/eka/bx86gcc/euseru.def` (bx86gcc first; not the
  unrelated bx86 MSVC set). 2503 def ordinals, 2503 image slots, 10 `ABSENT` on
  both sides (same set). 2451 match by simple `symbol address − 0x10001000`.
  The other 42 (40 `_ZT*`, `KHeapMinCellSize`, `KHeapShrinkHysRatio`) differ by a
  constant `0x7c0`. **That is an inference, not a measured fact:** `.text` is
  `0x4b840` bytes at VMA `0x10001000` and `.rdata` sits at VMA `0x1004d000`, so
  the PE leaves a gap that the E32 image closes by packing `.rdata` directly after
  `.text`. With that model all 2493 present exports match, 0 mismatches. I have
  not read the petran source to confirm the packing rule.
- **EFSRV**: Raptor generated `efsrv.def.x86` from
  `fileserver/eabi/efsrvu.def` (no `bx86gcc` EFSRV def exists; a `bx86` MSVC
  one does and was correctly ignored). Same 421 ordinals and names; the only
  differences are 18 `_ZTI`/`_ZTV` entries rewritten to `ABSENT` (eabi had 2
  ABSENT, the x86 def has 20). All 401 present exports match the image;
  `RFs::Connect`@68, `RFile::Open`@93, `Write`@97, `Close`@300 verified.
- **t_cactw** imports: all 50 ordinals exist (non-ABSENT) in the built
  `euser.dll`. Names under the bx86gcc def: `CActiveScheduler::Start`@426,
  `Install`@428, etc. The same ordinals read through the **eabi** EUSER def give a
  different name in 1 of 50 cases, which re-confirms the two spaces must never
  be mixed. (Earlier finding: 362 of 2496 shared names have different ordinals.)
- **fhserver** imports: 22/22 resolve in the built EFSRV def and 65/65 in EUSER.

Not checked: that any ordinal's *behaviour* is what the port provides; that
`hal.lib` is static or import (not classified; no `hal.dll` appeared in `urel`).

## Deviations from the card (read these)

The card says: no upstream source edits, and "do not patch source to make a
build pass without coordinator review". `kernelhwsrv/` was not touched. But the
following were done in scratch without prior review:

- **D1 (source patch, Raptor clone)** `sbsv2/raptor/bin/gethost.sh`: upstream
  computes the host dir from `/lib/libc-*` and `uname -i`, both unusable on this
  distro (literal `libc-*`, arch `unknown`). Patched to `libc_modern` /
  `uname -m`. Diff: `artifacts/scratch_raptor_patches.diff`.
- **D2 (source patch, Raptor clone)** `util/talon/process.c`: `poll()` returning
  `EINTR` made talon treat the recipe as "failed to run shell" for 21 of 90
  compiles. Added `|| errno == EINTR`. Without it, results were nondeterministic.
  This is a genuine upstream bug exposed by modern glibc/kernel timing.
- **D3 (compiler flags, no source edit)** make 3.81 needed
  `-D__alloca=alloca -D__stat=stat`; talon/ransleep needed a `gcc` shim adding
  `-std=gnu89 -w -fcommon -include ctype.h …`; bash 4.0 needed `-std=gnu89`.
- **D4 (banner shims)** Raptor demands `cpp … [345].x` and `GNU sed version [3-9]`.
  Wrapper scripts rewrite only the `--version` banner (they print both the fake
  and the real version) and otherwise `exec` the real host cpp 16.2.1 / sed 4.9.
  **So preprocessing of bld.inf/mmp is done by GCC 16's cpp, not by 3.x/4.x.**
- **D5 (fabricated config)** `epoc32/tools/variant/variant.cfg` plus
  `include/variant/scratch_r1.hrh` (includes only `platform_paths.hrh`) are
  **invented scratch stand-ins** for a real OS variant. No feature macros, no
  `SYMBIAN_*` settings from a real kit. The first EXPORT pass used Raptor's
  `nohrh` variant to bootstrap. Any behaviour that depends on variant macros
  is **not** represented in these binaries.
- **D6** `kernelhwsrv_ro` is a scratch **symlink** to the port's `kernelhwsrv/`
  because the real path contains a space and Raptor's preprocessor splits it.
- **D7 (`petran` not built the Raptor way)** Raptor's `tools2` config rejected
  the host compiler and 32-bit glibc dev files are absent, so I compiled the 20
  `petran` sources by hand with MinGW 3.4.5 using flags I assembled from
  `gcc.xml`/`petran.mmp` (`logs/petran_cflags.txt`, incl. `-include new`, which
  I added to get past a `throw()` mismatch with the 3.4.5 `<new>`). The tool
  works but is **not** identical to a Raptor-built one.
- **D8 (header generation)** `exec_user.h` etc. were 6-byte dummies from
  `dummy_genexec_hr.zip` after EXPORT. I ran `kernel/genexec.pl` (host perl) by
  hand to produce real ones. Upstream's `genexec.mk` extension does this
  inside an abld flow that Raptor did not trigger here. Needs a coordinator
  decision on how the official flow should do it.
- `cpptoolsplat/mingw-gcc-3.4.2` header `gcc_mingw_3_4_2.h` was copied from the
  3.4.5 directory of the same repo into `epoc32/include/gcc_mingw/`.

None of D1–D8 modifies `kernelhwsrv/` or the port repo.

## First blockers hit, in order (for anyone repeating this)

1. `bz2` import error in Python → needs bzip2 headers (built in scratch).
2. Raptor `HOSTPLATFORM_DIR` = `linux-unknown-/lib/libc-*` → D1.
3. make 3.81 link error `__stat`/`__alloca` → D3.
4. talon compile errors from implicit declarations → D3.
5. `bin/bash` missing under the host dir → built bash 4.0.24.
6. toolcheck rejects cpp/sed versions → D4.
7. `variant.cfg` missing → D5.
8. space in path breaks cpp → D6.
9. `SYMBIAN_OS_LAYER_PLATFORM_EXPORT_PATH` undefined → the macro lives in
   `platform_paths.hrh`, which is **not** in the build repo or `kernelhwsrv`; it is
   in the separate `deviceplatformrelease` repo. This is a real, previously
   unlisted dependency.
10. `Exec::` undeclared → D8.
11. talon `EINTR` flake → D2.
12. EFSRV link needs `hal.lib` → build `halservices/hal` first.
13. `*.test` MMPs need the `.test` variant (`-c x86_urel.test`).

## Reproduce

```
source /tmp/opencode/r1-build-repro/env.sh      # sets EPOCROOT, SBS_HOME, shims, WINEPREFIX
cd $R
sbs=$SBS_HOME/bin/sbs; K=$R/kernelhwsrv_ro
bash $sbs -b $K/kernel/eka/bld.inf                      -c x86_urel EXPORT
bash $sbs -b $K/userlibandfileserver/fileserver/group/bld.inf -c x86_urel EXPORT
# genexec (D8):
perl $K/kernel/eka/kernel/genexec.pl -i $K/kernel/eka/kernel/execs.txt \
  -e $R/epoc32/include/platform/exec_enum.h -u $R/epoc32/include/platform/exec_user.h \
  -k $R/epoc32/include/platform/kernel/exec_kernel.h
bash $sbs -b $K/kernel/eka/euser/bld.inf   -c x86_urel
bash $sbs -b $K/halservices/hal/bld.inf    -c x86_urel
bash $sbs -b $K/userlibandfileserver/fileserver/group/bld.inf -c x86_urel -p efsrv.mmp
bash $sbs -b $K/kerneltest/e32test/group/bld.inf -c x86_urel.test -p t_cactw.mmp
bash $sbs -b $K/kerneltest/f32test/group/bld.inf -c x86_urel.test -p fhserver.mmp
```

This sequence presumes the already-provisioned tools in scratch. A cold
rebuild also needs D1–D7, so this report is **not** a one-command recipe yet.

## Acceptance tests proposed (executable, no QEMU)

1. `wine petran.exe -dump e euser.dll | grep -c Ordinal` = 2503; 10 lines `ABSENT`.
2. `wine petran.exe -dump e efsrv.dll` shows ordinals 68/93/97/300 non-ABSENT.
3. `wine petran.exe -dump i t_cactw.exe` lists exactly the 50 ordinals in
   `artifacts/t_cactw_euser_imports.txt`, all non-ABSENT in the built EUSER.
4. `wine petran.exe -v` on each artifact: UIDs/SID/capabilities/heap/stack as above.
5. `git -C kernelhwsrv status --short` is empty after the whole build.
6. Rebuilding twice yields identical hashes (**not yet tested**; PE and E32
   headers carry timestamps, so expect differences unless normalised).

## Unknowns / not done

- No artifact was loaded or run; relocation, entry, TLS and heap behaviour at
  runtime are unverified. (That is R1-STARTUP / a future loader card.)
- Import-library (`euser.lib`) contents, thunk format and how `edll.lib`/`eexe.lib`
  entry stubs call `_E32Dll`/`_E32Startup` were not examined.
- `hal.lib` kind, `rpipe.dll`, `eexe_*` variants not examined.
- `udeb`, SMP, and any non-`urel` configuration not attempted.
- Determinism not tested. Cold rebuild from scratch not repeated end to end.
- Build used GCC 16 cpp for metadata (D4) and a made-up variant header (D5);
  bit-identity with an official kit cannot be claimed.
- Licences (EPL, GPL/LGPL MinGW) not cleared; GPG trust for Python/bzip2 keys
  rests on keyserver retrieval only.

## Blocked assumptions

The ordinal/def route (bx86gcc-first, eabi fallback with `_ZT[IV]`→`ABSENT`) is
now **demonstrated at build level for EUSER, EFSRV, and two real programs**, but
remains **unverified at run level**. Do not freeze R1-ABI/v1 import identity
(`euser{000a0000}[100039e5].dll` style) until R1-STARTUP confirms how the
loader resolves those strings.

## Handoff

- **Card:** R1-BUILD-REPRO. **Base:** port `fa62323e…` + uncommitted files;
  upstream `0c320865…` read-only; branch as checked out (unchanged).
- **Contract revision:** none issued; R1-ABI/v0 remains not frozen.
- **Changes:** this report only (`qemu-port/docs/r1_build_repro_v0.md`).
  Scratch artifacts under `/tmp/opencode/r1-build-repro/` (not committed).
- **Tests:** build/inspection commands above; zero QEMU, stable-suite or
  hardware runs.
- **Manual step:** coordinator to review deviations D1–D8 (especially D2
  talon patch and D5 fabricated variant) and decide which become sanctioned.
- **Limits:** see "Unknowns". Not a promised SDK; no redistribution.
- **Blocked assumptions:** runtime behaviour; official-variant equivalence;
  determinism; licence clearance.
- **Integration order:** after this, R1-STARTUP (consume artifact hashes),
  then any loader/EUSER/EFS implementation; R1-ABI/v1 only after those.
