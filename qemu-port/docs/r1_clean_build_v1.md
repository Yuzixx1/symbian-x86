# R1-CLEAN-BUILD v1 — one clean scratch build with `port-x86-rfile64-v1`

Status: **research result, not frozen, no ABI freeze, no official-kit-equivalence claim.**
The one authorized clean attempt was made at the owner-approved root
`/home/yuzi/.cache/sybian-r1/r1-build-v1` (home-backed; supersedes the `/tmp` path).
All stages up to and including `t_cactw`/`fhserver` built with `errors: 0`. No E32 image was
executed, no QEMU or hardware, no commits, no second cold build. Scratch is not redistributed.

## 0. History preserved: the first attempt was blocked by quota (superseded)

The first launch stopped before allocating anything, as the card required.

| Measurement (2026-10-22, before the amendment) | Value |
|---|---|
| `/tmp` (tmpfs) size / used / available | 7871 MB / 5185 MB / 2686 MB |
| user quota on `/tmp` | 5185 MB used of **6297 MB** (soft = hard), so about **1112 MB** free |
| estimate for a fresh private attempt | about 2.75 GB (Wine prefix 1588 MB, tools 182, build repo 522 + 361 .git, inputs 38, epoc32/logs about 60) |
| shortfall | about 1.6 GB; nothing deleted, moved or created under `/tmp/opencode` |

Owner amendment: use the home-backed root, keep the Wine prefix, TEMP/TMP and intermediates under it, preserve
everything under `/tmp/opencode`. Realised footprint of this build: **2723 MB** at the new root
(wineprefix 1588, src 881, tools 181, inputs 38, epoc32 23). Old `/tmp` usage went 5185 → 5190 MB
(+5 MB, Wine-server sockets and other desktop activity; see §9).

## 1. Provenance

| Item | Value |
|---|---|
| Port repo | `fa62323eea7bc96cefd666e40f5189567731bd44`, branch `main`, dirty (66 `git status --short` lines at the end: other cards' untracked files plus this report; none edited by this card) |
| Upstream `kernelhwsrv/` | `0c3208650587ac0230aed8a74e9bddb5288023eb`, `status --short` = **0 lines** before, between stages (run_stage.sh) and after |
| Build repo (cloned fresh into the new root) | `f458a4ce83f74d603362fe6b71eaa647ccc62fee`; only tracked modifications `sbsv2/raptor/bin/gethost.sh`, `sbsv2/raptor/util/talon/process.c` = D1/D2 applied from `scratch_raptor_patches.diff`; live `git diff` sha256 `fd504a6009d3a70039bece577b0f2a3082987d56b415f4209a93ba50f289e50d` (equals the patch file; checked before and after every stage) |
| Root / env | `R=/home/yuzi/.cache/sybian-r1/r1-build-v1`, `EPOCROOT=$R/`, `TMPDIR=TMP=TEMP=$R/tmp`, `WINEPREFIX=$R/wineprefix` (fresh `wineboot`, own prefix). `env.sh` sha256 `2feaba56ec8b7d006629c5afc69ce87bc647c44b9d7646317e6dba2e72c4a615`; it contains no `/tmp/opencode` and no banner shims |
| Source symlink (D6) | `$R/kernelhwsrv_ro -> /home/yuzi/Documents/Sybian REAL/kernelhwsrv` |
| Host tools | gcc/cpp 16.2.1 (`/usr/bin/cpp` real), sed 4.9 (real), perl 5.42.3, wine-11.0 (Staging) |
| Host build tools (copied binaries, not rebuilt) | Python 2.7.18 (`python2.7` sha256 `a19750e5cf60ddc0e0c56b6af395864906338da3e44a1e9a8c1c4c944e7589cd`, `sys.prefix` = new root, `bz2` imports), make 3.81 (`33c10ef85671e2624c3d81e1f598cf96f6165cc6957a169f30e4deca84abc7bc`), bash 4.0.24 host-dir binary (`4f0341e5c906f45a321289d73691f8114e0a664f30bcb685bce9ef6a06630973`), `FindBin.pm` (`a135b2fb3a94d062276239e93884374b0a19b7a62dbc8b97e8b73817beb572d9`). `make` and `bash` still contain **old `/tmp/opencode/r1-build-repro/...` strings** as compiled-in default dirs (share/locale, include, lib); they were not used at run time as far as observed, **not proven** |
| Host utilities (rebuilt in the new root) | talon, ransleep, tmkdir, sbs_descramble built from the patched sources with the D3 gcc shim (log `logs/raptor_util.log`, rc 0); 0 old-path strings in `talon` |
| MinGW 3.4.5 | `mingw-symbian-001.zip` sha256 `0c2ac07f9a3cad87e49d04814c37c0dca71143caac0777409f611e8daa07594c`, unzipped in `tools/mingw` (tree identical to the old one by `diff -rq` and mode list) |
| Pinned headers | `platform_paths.hrh` `8a8ebdc3f40804d630def1af4c0c3a68568a79d13eb08d0661d9ba252dd96f31`; `gcc_mingw_3_4_2.h` `9c004e2ad0aea7d866c22ac9310e39b911174345bd00ef649edc969d044a12a6` |
| Variant (installed, hash-verified before and after every stage) | `epoc32/include/variant/port_x86_rfile64_v1.hrh` `500889884b3063c1b0712c000d2c62f55e70774cbfda41b51b82f343be277106`; `epoc32/tools/variant/variant.cfg` `2c505b4bb43e4b74e362589ce78f866053e4e9d8283688e47cca321c58e08cd0` (contents as in `r1_build_config_v0.md` §3) |
| Inputs copied read-only from earlier work | `dl/*` (hash-verified), `scratch_raptor_patches.diff`, the two component patches, `petran_cflags.txt` (reference only). **Generated old headers/epoc32 were not reused**; `epoc32/` started empty |
| `epoc32/tools` perl helpers | The EUSER flow calls `prepdef.pl` and friends; the old tree had 25 of them. They were installed from the build repo's own `sbsv1/abld/group/bld.inf` and `bintools/elftools/group/bld.inf` export mappings by `harness/install_perl_tools.py` (byte copies, list in `logs/install_perl_tools.log`, hashes in `logs/perl_tools.sha256`). The old tree also had `_secure_E32Env.pm` with no mapping in either inf; it was **not** installed and nothing failed without it |

## 2. Harness corrections made first (authorized scratch changes; no upstream edits)

| Correction | Where |
|---|---|
| Linker/compile status propagated immediately; exit 7 on a nonzero link, exit 1 on any compile failure or empty object | `harness/petran_build.sh` |
| Fresh empty output dir required (exit 5 if non-empty), so no stale objects/exe | `harness/petran_build.sh`, `run_stage.sh` (refuses an existing stage log) |
| Full compile and link argv, stdout, stderr and rc archived per file | `petran_out/log/argv.txt`, `*.out`, `*.err`, `compile_summary.txt` |
| Patch **contents** (live `git diff`), variant files, headers and tool versions pinned by hash; real tools verified, shim detection | `harness/preflight.sh` (31 checks pre, passes also post) |
| Config/patch/`kernelhwsrv` cleanliness checked before and after **every** sbs stage, abort codes 10/11/12 | `harness/run_stage.sh` |
| Missing dirs/manifests created | `harness/MANIFEST.sha256` (script hashes), `logs/*.sha256` |
| `check_rfile64.sh` now reads the **whole** function (to the next symbol) and requires 0 Panic calls and at least one IPC call; new `rfile64_inventory.py` lists every RFile64 symbol with its pushed Panic codes | `harness/` |

Script hashes (sha256, full):

- `preflight.sh` `d8f90190794b275a8c2dee28e76908229722b9a4c7f63b1e8976135eeaed7865`
- `run_stage.sh` `3e66fe9c165123d0f942aa3e8e99c554e2cf26deddd5ab87a85bf2f9cb455595`
- `petran_build.sh` `e4181df909851fa8335b2a52422a78322feec613047fa165459a739e9badd527`
- `check_rfile64.sh` `e5ebb237c568e4cac6aff275a0928dd133c6623fcdf1fea82544ec79e20d6fa1`
- `rfile64_inventory.py` `e02353e458d2987695460a697af5e87f1ec78de5730f145663f305a4ced8564a`
- `install_perl_tools.py` `495cc74f9f87203464358845c808ba38d50d24d50992bf943d659331ec0c5e44`

## 3. What ran, in order (all with `sbs --toolcheck=off -d`, real `/usr/bin/cpp` and `/usr/bin/sed`, no banner shims)

| # | Stage | rc | Result |
|---|---|---|---|
| 0 | `preflight.sh pre` | 0 | 31 ok |
| 1 | `export_eka` (kernel/eka/bld.inf, `-c x86_urel EXPORT`) | 0 | 16 live cpp lines |
| 2 | `export_f32` (fileserver/group/bld.inf EXPORT) | 0 | `f32file64.h` 6406 B and `f32file64.inl` 25283 B exported |
| 3 | genexec (D8, explicit upstream `genexec.pl`) | 0 | the three 6-byte dummy headers (all sha256 `693cc160…`) replaced by 9426 / 40746 / 37337-byte generated headers; `cc1: note: obsolete option -I-` only |
| 4 | petran (D7 reconstructed, local experimental tool) | 0 | 20/20 compile rc 0 with **0 diagnostic bytes**, link rc 0, exe 677646 B sha256 `b200bf0878679e6c7d6c20e5a78cb2bb93f16d5b21790a2b66c7dd04a53ec155` |
| 5 | `euser` | 0 (after a harness fix, §4) | 102 compiles, 0 errors, 12 sbs warnings |
| 6 | `hal` | 0 | `hal.lib` produced; no warnings |
| 7 | `efsrv` (`-p efsrv.mmp`) | 0 | 21 compiles (matches the 21 sources in `efsrv.mmp`), 1 warning (missing dependency `epoc32/tools/checklib`, also seen before) |
| 8 | `t_cactw` (`-c x86_urel.test`) | 0 | 3 warnings |
| 9 | `fhserver` (`-c x86_urel.test`) | 0 | 3 warnings |
| 10 | `preflight.sh post` | 0 | passes |

Final `git -C kernelhwsrv status --short`: **0 lines**; patch hash unchanged; variant hashes unchanged.

## 4. First reproducible blocker (harness, resolved): `dlltool.exe: CreateProcess`

- The first `euser` stage failed with 8 errors, all `dlltool.exe: CreateProcess` (dlltool could not spawn `as.exe`):
  `Z:\home\yuzi\.cache\sybian-r1\r1-build-v1\tools\mingw\bin\dlltool.exe: CreateProcess`.
  Logs kept in `logs/attempt1_dlltool_blocker/`.
- Reproduced by hand outside Raptor: same failure with the old prefix, with or without `TMPDIR`, with a `WINEPATH`
  to the mingw bin, and from a path symlinked without the dot directory. Passing `--as <path>` worked, and launching
  `dlltool.exe` as `M:\bin\dlltool.exe` (a `m:` drive symlink to `tools/mingw` inside the private prefix) worked.
  The **old tree's** dlltool at `/tmp/opencode/r1-build-repro/tools/mingw/bin` worked on the same input.
- **Root cause not isolated.** The `.cache` dot-directory in the Z: path is only a suspicion. The `M:` mapping is a
  scratch harness workaround, applied to the `dlltool` wrapper only (`tools/x86bin/dlltool`, sha256 `4b285fc50b6bf9e2d50a99d54251059325d8c0d1bb51b97741b7df4eaa13db65`).
  No upstream or tool bytes were changed.
- **Disclosure about "one attempt":** after the fix, I ran the `euser` stage a second time (`euser_resume`, rc 0), saw it
  had reused partial outputs from the failed stage, quarantined `epoc32/build/euser` and `epoc32/release` plus its logs
  to `quarantine_partial_euser/`, and ran `euser` once more from a clean state (rc 0). The EUSER/EFSRV/test results
  below come from that clean rerun. EXPORT, genexec and petran were **not** repeated. I treat this as one attempt with
  one harness fix; the coordinator may decide otherwise. No whole-tree cold rebuild was done twice.

## 5. Captured live metadata commands (closes the A1 gap of BUILD-CONFIG)

37 `Preprocessing command line` entries were captured from the `-d` logs (`logs/live_cpp_commands.txt`):
export_eka 16, export_f32 4, euser 9, hal 2, efsrv 2, t_cactw 2 (the `.test` variant), fhserver 2.

- Every one starts with `/usr/bin/cpp -undef -nostdinc -MD -MF<dep> -MT'$(PARSETARGET)'`, then
  `-include $R/epoc32/include/variant/port_x86_rfile64_v1.hrh`, then
  `-DSBSV2=_____SBSV2 -DX86GCC=_____X86GCC -DGCC32=_____GCC32 -DEPOC32=_____EPOC32 -DX86=_____X86 -DGENERIC_X86=_____GENERIC_X86 -D__GNUC__=3`.
  0 of 37 lack any of those elements.
- Include order observed: `-I .`, `-I $R/epoc32/include/variant`, `-I $R/epoc32/include`, then one or two component directories.
  This **matches the reconstruction in BUILD-CONFIG §6.1**, and the `.test` commands (t_cactw, fhserver) use the same macros and variant.
- Macro tokens: only the 7 above, 37 times each; no other `-D`.
- Compile side: 126 logged `g++` compile commands (EUSER 102, EFSRV 21, t_cactw 1, fhserver 2) all carry
  `-DPRODUCT_INCLUDE="$R/epoc32/include/variant/port_x86_rfile64_v1.hrh"` (0 exceptions). `SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API`
  is **not** on any command line; it reaches the compile only through the variant header, as designed. Other `-D`:
  `__X86__ _UNICODE __SYMBIAN32__ __EPOC32__ NDEBUG __X86GCC__ __GCC32__` (all 126), `__DLL__` 112, `__EXPORT_MEMCPY__` 90, `__CIA__` 10,
  `SYMBIAN_FTRACE_ENABLE_UDEB` 21 (efsrv.mmp), `__EXE__` 3, `USE_INSTRUMENTED_HEAP` 2, `ENABLE_HEAP_MONITORING` 1.
- Selected graph vs variant: `fileserver/group/bld.inf` exported `f32file64.h`/`.inl` (variant on); 21 EFSRV translation units and the 2 consumers
  `handshare.cpp`/`handshareint.cpp` list `f32file64.h` in their `.d` files.
- Not verified: that these `cpp` calls are identical to what a Windows-hosted Raptor would run (only this host).

## 6. RFile64: from panic stubs to real bodies (full-body inspection of the new `efsrv.dll.sym`)

`harness/check_rfile64.sh` → rc 0 on the new image, rc 1 on the old image (negative control, 4 × NOT a full real body).

| Function | New VA | Size | Calls | Panic | What the body does |
|---|---|---|---|---|---|
| `RFile64::Open(RFs&,const TDesC&,TUint)` | `0x10006e00` | 80 B | 1 | none | `mode \| 0x40000` (`EFileBigFile`), opcode `0x1e`, then `call RFile::CreateSubSession(RSessionBase const&,int,TIpcArgs const&)` at `0x100106f0` |
| `RFile64::Create` | `0x10006e50` | 80 B | 1 | none | same shape, opcode `0x1f` |
| `RFile64::Replace` | `0x10006ea0` | 80 B | 1 | none | same shape, opcode `0x20` |
| `RFile64::Temp` | `0x10006ef0` | 80 B | 1 | none | same shape, opcode `0x21` |

Source cross-check: `cl_file.cpp:1955,2004,2058,2098` is `CreateSubSession(aFs,EFsFile{Open,Create,Replace,Temp},TIpcArgs(...,aFileMode|EFileBigFile,...))`; `f32file.h:1175` `EFileBigFile = 0x00040000`;
`F32plugin.h:74` `EFsFileOpen` is `-- 30`, so 30..33 = `0x1e..0x21`. Old (v0) bodies were 32 bytes, 1 `Panic`, code 6 (`ENotImplemented`).
(My earlier "`TIpcArgs`/`SendReceive` call" heuristic was a symbol-name match; the true IPC call is `CreateSubSession`. The checker's `ipc_calls` count matched that name, so treat it as a rough counter.)

Whole-class inventory (`logs/rfile64_inventory.txt`, 29 text symbols = 28 exports + new private `CFileMan::DoCopy(RFile64 const&,RFile64&,int&)`):
- **0** functions still carry the code-6 stub panic (old: 20 of 28 had only code 6).
- 18 functions have no Panic at all; 10 (the `…Ex…` Read/Write variants, `Lock`, `UnLock`) have only code `0x13`, which is index 19 of `TClientPanic`
  = `EPosNegative` (counted from the enum in `cl_std.h`; the source comment says `//20`, so the comment is off by one); they are the `__ASSERT_ALWAYS(aPos>=0,Panic(EPosNegative))` guards (`cl_file.cpp:2890`).
- `CFileMan::DoCopy(RFile64…)` has one Panic, code 12 = `EFManBadValueFromObserver`.
- This is **static code inspection only**; behaviour of the real implementations against a real file server is unverified.

## 7. Exports, imports, headers, hashes, paths

**Ordinals / absence** (`petran -dump e`, compared with the def files; no equality with old artifacts is claimed):

| Image | Def source | Ordinals in def / image | ABSENT def / image | Notes |
|---|---|---|---|---|
| `euser.dll` | `kernel/eka/bx86gcc/euseru.def` | 2503 / 2503, sets equal | 10 / 10, sets equal | 2451 present names have `.sym` address − `0x10001000` = image slot; the other 42 differ by a constant 1984 (`0x7c0`), the same pattern as in BUILD-REPRO (inference not re-proven here) |
| `efsrv.dll` | `fileserver/eabi/efsrvu.def` + the Raptor sed (`_ZT[IV]` → ABSENT) | 421 / 421, sets equal | 20 / 20, sets equal | all 401 present names match the `.sym` address with delta 0; ordinals 68, 93, 97, 300 present |

**Imports** (`petran -dump i`; dependency names as built, not frozen): `t_cactw.exe` 50 from `euser{000a0000}[100039e5].dll`;
`fhserver.exe` 22 from `efsrv{000a0000}[100039e4].dll` + 65 from `euser{000a0000}[100039e5].dll`; `efsrv.dll` 136 from `euser{000a0000}[100039e5].dll`.
All imported ordinals exist and are non-ABSENT in the new `euser.dll`/`efsrv.dll`. Whether the loader accepts these decorated strings is R1-STARTUP.

**Headers** (`petran -v`): `euser.dll` UIDs `10000079 1000008d 100039e5`, SID `100039e5`; `efsrv.dll` `… 100039e4`, SID `100039e4`, header format 2,
caps `000fffff`, min/max heap `0x1000`/`0x100000`, stack `0x2000`; `rpipe.dll` `… 102827d5`; `t_cactw.exe` UID1 `1000007a`, UID3 `0`; `fhserver.exe` UID3/SID `00002323`; `SMP Safe : True`, `Debuggable : False`.

**Hashes** (new root; `logs/artifact_hashes.sha256`, `logs/all_release_hashes.sha256`):

| File | sha256 |
|---|---|
| `euser.dll` (412104 B) | `4851fec4b6439bf5263e6f10f7f3010828f84df55df050c710f57077f2dd017b` |
| `efsrv.dll` (71088 B) | `332d97274a151bb17a18112e728684b98c8462fe8723bfadb1de0d471e372b6d` |
| `t_cactw.exe` (12892 B) | `f1d53fbaaac080650b21c657cef1f982c973a07925f6c192fdb08520de73fd5e` |
| `fhserver.exe` (17820 B) | `717e040762d7be2a2ef6469e1249e2df4168f05e357276474de6f33ca4e4ccf1` |
| `rpipe.dll` | `cb907021ad3ad9851251d71bec625281a04f2862818b22d5dfcc453cc5b469a9` |
| `efsrv.dll.sym` / `euser.dll.sym` | `5396babad8309d5415bfcc0b8acd650223fa665bb7e8dd97a64b18515d943900` / `f4baef839c9c6a60e5dcf2d2ce54743c6f850b966dca88276c20684e01087e45` |
| `petran.exe` (677646 B) | `b200bf0878679e6c7d6c20e5a78cb2bb93f16d5b21790a2b66c7dd04a53ec155` |
| `exec_enum.h` / `exec_user.h` / `exec_kernel.h` | `169ca7a9d94cebfa1e0eeee83f78c43f8535534c55967a70a4d844a4773a5e1e` / `1610cda797a6b5e9bd011c02637c6c965b08a02bf66112dab5b08abc04393ef5` / `b645f30862d9bef466e9ae01e205b232dccfea57e6670f87b00d949488f139d1` |

Old artifacts (`/tmp/opencode/r1-build-repro/...`) were re-hashed and are unchanged (`0c833111…`, `69051f33…`, `c096c6a2…`, `825d4335…`).
These new hashes differ from the old ones as expected (new variant, new root, new timestamps); **no equality is required or claimed**.
Total release files 18 (images, maps, syms, libs); the full list is in `logs/all_release_hashes.sha256`.

**Embedded paths:** `euser.dll` has 2 strings, `/home/yuzi/.cache/sybian-r1/r1-build-v1/kernelhwsrv_ro/kernel/eka/common/{heap_hybrid,debugfunction}.cpp`
(0 `/tmp`); `efsrv.dll`, `t_cactw.exe`, `fhserver.exe`, `rpipe.dll`: 0 `/tmp`, 0 `/home`, 0 `.cache`, 0 `sybian`. The same two-string pattern as the old `/tmp` build, now with the new root.

## 8. Expected change confirmed, other statements

- The BUILD-CONFIG prediction held for the items I could check statically: RFile64 stubs gone, `CFileMan::DoCopy` private signature swapped to `RFile64` (a 29th text symbol, not exported),
  `f32file64.*` exported, 28 `RFile64` exports and the ordinal set intact (421 / 2503). Class `sizeof` equality was **not** re-measured here.
- `euser.dll`: 12 sbs warnings, 420 `warning:` lines in the stage output (mostly pre-existing `.cia` "control reaches end of non-void function" notes); not triaged.

## 9. Storage and temporary files

- Everything lives under `/home/yuzi/.cache/sybian-r1/r1-build-v1`; `$R/tmp` (the TMPDIR) ended at **0 bytes**; I created a short-lived `/tmp/bash40.<pid>` copy while moving the bash binary, and removed it (verified absent).
- Unavoidable `/tmp` use: the Wine server socket directory `/tmp/.wine-1000/server-*` (0 bytes of data, created by Wine; other workers' Wine also uses it). No build data goes there.
- Free space after the build: `/home` about 260.8 GB; `/tmp` quota 5190 of 6297 MB used (other activity included).
- Nothing under `/tmp/opencode` was modified, deleted or cleaned (the old tree was only read, and the old `efsrv.dll.sym` was disassembled as a negative control).

## 10. Unknowns and blocked assumptions

- No E32 image was loaded or run; runtime behaviour of any image and the real RFile64 IPC wire format (R1-WIRE) are unverified. Loader string resolution is R1-STARTUP; R1-ABI/v1 import identity is not frozen.
- Raptor ran with `--toolcheck=off` and host GCC 16.2.1 `cpp`; equivalence with the MinGW `cpp` was compared for content earlier but the mmp parser was not run with the MinGW `cpp`. CR/blank-line handling for the MinGW variant remains untested.
- Determinism: only one cold build exists (plus the EUSER stage rerun, §4); a rebuild-twice comparison was **not** run, and PE/E32 headers carry timestamps.
- `dlltool` spawn failure root cause not isolated (§4); the M: drive workaround is scratch-only.
- `petran` is a reconstructed local experimental tool: link command was inferred earlier from its outcome; equality with an official/Raptor `tools2` petran is unproven. Object hashes of the new `petran` were not compared to the earlier ones in this card.
- `make` and `bash` binaries were copied, not rebuilt, and carry old-path default strings.
- `udeb`, SMP configurations, the other `SYMBIAN_*` macros (still off), licences (EPL source, GPL/LGPL MinGW) are unchanged from BUILD-CONFIG §12. `sbs` warnings were not triaged.
- The sed comment/enum off-by-one in `cl_std.h` (`//20` vs index 19) is noted but not investigated further.

## 11. Acceptance checks (re-runnable from the new root)

1. `bash harness/preflight.sh $R "<port repo>" pre` (before) and `… post` (after) → rc 0 (run: both).
2. `bash harness/check_rfile64.sh epoc32/release/x86/urel/efsrv.dll.sym` → rc 0; same script on an old image → rc 1 (run: both).
3. `python3 harness/rfile64_inventory.py epoc32/release/x86/urel/efsrv.dll.sym` → rc 0 (no code-6 panic) (run).
4. `epoc32/tools/petran -dump e` on `euser.dll`/`efsrv.dll` → 2503/421 ordinals, 10/20 ABSENT, sets equal to the defs (run, §7).
5. Every `-d` metadata `cpp` line contains the variant `-include` and the seven macros (37 of 37; run).
6. `git -C "<port repo>/kernelhwsrv" status --short` → 0 lines (run, also between every stage).
7. Old artifact hashes unchanged (run).
8. **Not run:** rebuild-twice determinism; runtime/loader tests.

## 12. Handoff

- **Card:** R1-CLEAN-BUILD. **Base commit:** port `fa62323eea7bc96cefd666e40f5189567731bd44` (branch `main`, dirty, not edited by this card); upstream `kernelhwsrv` `0c3208650587ac0230aed8a74e9bddb5288023eb` clean; build repo `f458a4ce83f74d603362fe6b71eaa647ccc62fee` + D1/D2.
- **Contract revision:** none issued; R1-ABI/v0 not frozen.
- **Changes:** this report only (`qemu-port/docs/r1_clean_build_v1.md`). Scratch at `/home/yuzi/.cache/sybian-r1/r1-build-v1` (not committed, not redistributed). `/tmp/opencode` untouched.
- **Tests:** §11. No QEMU, stable-suite or hardware tests; no E32 execution.
- **Manual step:** coordinator to (1) decide whether the `euser` stage rerun in §4 counts as part of the one attempt, (2) decide whether to authorise a rebuild-twice determinism run, (3) decide who investigates the `dlltool` spawn failure, (4) route the new images and hashes to R1-STARTUP.
- **Limits:** §10. Research scratch only; official-kit equivalence not claimed.
- **Blocked assumptions:** runtime behaviour, loader acceptance of decorated import names, true IPC framing, determinism.
- **Integration order:** this report → coordinator review of §4 and §10 → R1-STARTUP consumes the new hashes and import names → R1-WIRE for RFile64 framing → R1-ABI/v1 only after both.
