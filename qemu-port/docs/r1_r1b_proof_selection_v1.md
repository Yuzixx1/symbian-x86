# R1-B proof selection — unchanged upstream x86 proof app(s) (card 30)

Status: **research proposal, NOT a selection, nothing built or run as part of this card.**
Date: 2026-10-28. Port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44`; upstream
`kernelhwsrv/` HEAD `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean
(`git status --short` empty). Owned path: this document only. No shared files
touched. No QEMU, no target build, no guest execution. No build is authorized
yet — the owner must approve the selected app(s) first.

## Inputs read

- `qemu-port/docs/r1_file_proof_search_v0.md` — six external packages searched
  (not exhaustive), `kernelhwsrv/` re-checked mechanically; no unchanged
  zero-capability file app found that is simpler than FHServer **and** covers
  the R1 file assertions; `t_cactw.exe` recommended as provisional scheduler
  proof, FHServer as later file checkpoint.
- `qemu-port/docs/r1_abi_contract_v0.md` — app-visible RFs/RFile contract,
  export-set warning (never mix bx86 and bx86gcc ordinals), candidates not
  selected (eshell too large; t_file.mmp needs TCB/DISKADMIN/ALLFILES;
  bigfilewriter.cpp is Windows host code).
- `qemu-port/docs/r1_toolchain_research_v0.md` — local route blocked: no
  bx86gcc `efsrvu.def`, no `euser.lib`/`efsrv.lib` found locally; matching
  upstream x86 SDK not provisioned.
- `qemu-port/docs/r1_build_repro_v0.md` — documented upstream route
  (Raptor 2.15.3 `-c x86_urel`, MinGW GCC 3.4.5 under Wine, sbs 2.15.3)
  **produced genuine E32 images** at build level for `euser.dll`, `efsrv.dll`,
  `t_cactw.exe`, `fhserver.exe` plus `.lib` import libraries, in scratch
  `/tmp/opencode/r1-build-repro/` (never committed, never executed anywhere).

## Selected candidates (pending owner approval)

Two separate apps, per the roadmap's allowance, both from the same upstream
revision, both **capability none / NONE** (unprivileged), both built at build
level in scratch and their imports verified against the same pair of built
EUSER/EFSRV images — so **no bx86/bx86gcc ordinal mixing** (EUSER used
`kernel/eka/bx86gcc/euseru.def`; EFSRV used `fileserver/eabi/efsrvu.def`;
the unrelated bx86 MSVC def was correctly ignored; ordinals were never taken
from mixed def sets).

| Role | App | MMP (upstream path) | Sources | What it demonstrates |
| --- | --- | --- | --- | --- |
| Active scheduling | `t_cactw.exe` | `kerneltest/e32test/group/t_cactw.mmp` | `kerneltest/e32test/active/t_cactw.cpp` | `CActiveScheduler::Install`/`Start`/`Stop`, `CActiveScheduler::Add`, `CActive::SetActive`, `CActiveSchedulerWait` start/stop/wait directives |
| File read/write | `fhserver.exe` | `kerneltest/f32test/group/fhserver.mmp` | `kerneltest/f32test/fileshare/handshare.cpp`, `handshareint.cpp` | `RFs::Connect`, `RFile::Open`/`Read`/`Write`/`Close`/`Seek`, `RFs::Delete`, `RFile::Rename` via EFSRV imports; subsession-handle adoption/transfer |

### Exact revisions and build inputs

- Upstream source tree: `kernelhwsrv/` at `0c3208650587ac0230aed8a74e9bddb5288023eb` (read-only, unmodified).
- Build tools repo `SymbianSource/oss.FCL.sftools.dev.build`:
  commit `f458a4ce83f74d603362fe6b71eaa647ccc62fee` (tree
  `968aa9bac548662b49af4e2b873b61873ba6f680`); `sbs version 2.15.3`.
- Compiler: MinGW 3.4.5 package `mingw-symbian-001.zip`, sha256
  `0c2ac07f9a3cad87e49d04814c37c0dca71143caac0777409f611e8daa07594c`;
  `g++ -dumpversion` = 3.4.5 under Wine.
- Variant: `-c x86_urel` for DLLs, `-c x86_urel.test` for the two `.test` MMPs.
- Build order (from r1_build_repro): EXPORT passes for `kernel/eka` and
  `fileserver/group`; `genexec` headers; `euser`; `hal`; `efsrv` (`-p efsrv.mmp`);
  `t_cactw` (`-p t_cactw.mmp`); `fhserver` (`-p fhserver.mmp`).

### Verified E32 facts (petran `-v`/`-dump`, build-level only)

- `t_cactw.exe`: "EPOC Exe for X86 CPU"; UIDs `1000007a 0 0`; SID 0; VID
  `70000001`; **capabilities none**; code link `0x00400000`; entry code +`0x260`;
  222 relocs; 1 DLL reference; **imports 50 ordinals from**
  `euser{000a0000}[100039e5].dll` (list:
  `artifacts/t_cactw_euser_imports.txt` under the scratch tree); bx86gcc names:
  `CActiveScheduler::Start`@426, `Install`@428, etc.
- `fhserver.exe`: imports **22 ordinals from** `efsrv{000a0000}[100039e4].dll`
  and **65 from** `euser{000a0000}[100039e5].dll`. EFSRV set includes ordinal 68
  (`RFs::Connect(int)`), 93 (`RFile::Open`), 97 (`RFile::Write(TDesC8 const&)`),
  300 (`RFile::Close`), 259 (`RFile::Read(int,TDes8&)`), 263 (`RFile::Seek`).
- `euser.dll` sha256 `0c833111d1f0656ef7d75848e76d9a90459ce449babc2f636d6d80e5f65dcf60`;
  `efsrv.dll` sha256 `69051f33b9b7f07b16ca5352b7a8d938490ed151e2d19db05f810f09939040fe`;
  `t_cactw.exe` sha256 `c096c6a27d65df49d896d3f4b3fc2d5d04e53461cdab93f663c3d1405a94b133`;
  `fhserver.exe` sha256 `825d43353d3008ef8372aa2b6e878c6c530a9ff9456c83a8a7bf7f4bdf627faf`;
  `petran.exe` sha256 `65f0015bb89d569748fd067d3adbdbd35950f1e2c5da1766af3c0e8d568c72e5`;
  `euser.lib` `55b3c739…a353446`, `efsrv.lib` `0d840f4c…e1d19db0` (existence/hash
  only; import-library contents not inspected).

### MMP capability confirmation (read from upstream source)

- `t_cactw.mmp`: `capability none`, `VENDORID 0x70000001`, `SMPSAFE`,
  `LIBRARY euser.lib`.
- `fhserver.mmp`: `capability NONE`, `uid 0x00000000 0x00002323`,
  `VENDORID 0x70000001`, `SMPSAFE`, `LIBRARY euser.lib efsrv.lib`.

## Build plan (plan only — execute only after owner approval)

1. Provision the pinned tools under scratch (build repo commit, MinGW 3.4.5,
   Python 2.7.18, GNU make 3.81, bash 4.0, talon, ransleep, petran sources)
   and the `env.sh` from r1_build_repro (`EPOCROOT`, `SBS_HOME`, shims,
   `WINEPREFIX`).
2. Run the build sequence with `kernelhwsrv_ro` = scratch symlink to the port's
   read-only `kernelhwsrv/` (D6 workaround for the space in the path):
   ```
   sbs -b $K/kernel/eka/bld.inf                      -c x86_urel EXPORT
   sbs -b $K/userlibandfileserver/fileserver/group/bld.inf -c x86_urel EXPORT
   perl $K/kernel/eka/kernel/genexec.pl ...          # exec_enum/exec_user/exec_kernel headers (D8)
   sbs -b $K/kernel/eka/euser/bld.inf   -c x86_urel
   sbs -b $K/halservices/hal/bld.inf    -c x86_urel
   sbs -b $K/userlibandfileserver/fileserver/group/bld.inf -c x86_urel -p efsrv.mmp
   sbs -b $K/kerneltest/e32test/group/bld.inf -c x86_urel.test -p t_cactw.mmp
   sbs -b $K/kerneltest/f32test/group/bld.inf -c x86_urel.test -p fhserver.mmp
   ```
3. Inspect (do not execute): `petran.exe -v` UIDs/SID/caps/heap/stack; `petran
   -dump e` export-slot counts (euser 2503, efsrv 421) and ABSENT lines; `petran
   -dump i` imports for both EXEs against the built defs; confirm no ordinal
   taken from the bx86 MSVC def set.
4. Record package HEAD, compiler, EUSER/EFSRV import metadata and hashes; run
   twice to check determinism (PE/E32 headers carry timestamps — normalization
   may be needed).

## Open items / not selected

- `eshell` (`kernelhwsrv/userlibandfileserver/fileserver/etshell/`): real RFs/RFile
  file utility but a large interactive shell needing console/HAL/other services
  outside R1 — rejected for R1-B.
- `t_file.mmp` (`kerneltest/f32test/group/`): builds with TCB/DISKADMIN/ALLFILES
  — not unprivileged; pruning would violate unchanged-source proof — rejected.
- `kerneltest/f32test/tools/bigfilewriter.cpp`: Windows host code despite its
  `.mmp` — rejected as E32 proof.
- `T_RscPanic.exe` (ossrv): zero-capability file consumer but links `bafl.lib`,
  needs ROM fixtures on Z:, uses RTest console + `__UHEAP_MARK`/JIT panic thread,
  weaker assertions (no byte compare, no EOF, no missing-file assert) — second-tier
  later option only.
- Local toolchain is blocked (r1_toolchain_research): no local bx86gcc EFSRV def or
  import library; the demonstrated build depends on scratch-provisioned tools with
  deviations D1–D8 (Raptor source patches D1/D2, fabricated variant header D5,
  genexec handling D8) that need coordinator review before the route is sanctioned.
- `t_cactw` uses the `RTest` console (`Getch`); a console/no-console route is an
  open R1-STARTUP/R1-ABI concern, not settled here.
- fhserver's exact assertion coverage (create/write/close/reopen/read, EOF,
  missing-file) should be source-confirmed against `handshare.cpp`/`t_handshare.cpp`
  before freezing; the import list proves file calls exist, not the assertion set.

## Decision requested

Owner to approve (a) `t_cactw.exe` for the active-scheduling proof and
`fhserver.exe` for the file read/write proof, or (b) a single-app alternative,
or (c) deferral pending a cleaner candidate from the not-searched repos listed
in r1_file_proof_search section 2. **No target build, QEMU run, or guest
execution is authorized by this document.** The R1-ABI/v1 import identity
(`euser{000a0000}[100039e5].dll` / `efsrv{000a0000}[100039e4].dll` versioned
strings) remains unfrozen pending R1-STARTUP loader confirmation.
