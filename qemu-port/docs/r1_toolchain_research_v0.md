# R1-TOOLCHAIN — x86 binary-route investigation v0 (blocked)

Scope: research only; no shared port build files or read-only upstream sources changed. Workspace HEAD `fa62323eea7bc96cefd666e40f5189567731bd44`; upstream `kernelhwsrv/` HEAD `0c3208650587ac0230aed8a74e9bddb5288023eb`. Builds below are **host ABI probes**, not upstream-built E32 binaries. This report feeds, but does not freeze, R1-ABI/v1.

## bx86gcc first: what is present

1. `kernelhwsrv/kernel/eka/bx86gcc/euseru.def:1-18,427-429` supplies GCC/Itanium-style EUSER names and ordinals (e.g. `CActiveScheduler::Start` `_ZN16CActiveScheduler5StartEv` @ 426 and `Install` `_ZN16CActiveScheduler7InstallEPS_` @ 428). `kernel/eka/base_e32.mrp:8` lists that directory. `kernel/eka/euser/euser.mmp:23-25` and `euser.mmh:29-55,100-123` give the EUSER target and x86-specific build inputs. `kernel/eka/include/cpudefs.h:205-217` explicitly distinguishes x86 GCC naked-function behavior; `kernel/eka/include/u32exec.h:68-105` provides GCC x86 executive-call assembly. This is source support, **not evidence of a successfully built EUSER.DLL**.
2. The EFSRV client target is separate (`kernelhwsrv/userlibandfileserver/fileserver/sfsrv/efsrv.mmp:24`; client operations in `sfsrv/cl_cli.cpp:219-245`, `sfsrv/cl_file.cpp:473-525`). This checkout has `fileserver/{bmarm,bwins,bx86,eabi}/efsrvu.def`, but **no** `fileserver/bx86gcc/efsrvu.def`; no `efsrv.lib` or `euser.lib` was found in this workspace, `/opt`, `/usr/local`, or a local `/home/yuzi` file search. Thus neither matching bx86gcc EFSRV ordinals nor an actual GCC import library have been established. A `.def` for another architecture/compiler does not identify the GCC EFSRV binary's ordinal allocation.
3. The locally available `gcc`, `g++`, `objdump`, `readelf` and `wine` were detected; `clang`, `i686-w64-mingw32-g++`, `abld`, `bldmake` and an upstream `elf2e32` executable were not found on PATH (`command -v` probe). The port's own Python `tools/elf2e32.py` and GCC `-m32` make rules (`qemu-port/user/user.mk:1-6,37-48`) are **not** the upstream application toolchain or matching EUSER/EFSRV import libraries. The presence of source and a host compiler does not reproduce an upstream MMP build.

## Concrete ABI probe, not an E32 build

Executed (scratch outputs only under `/tmp/opencode/r1-toolchain/`):

```sh
g++ -m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -c -x c++ -o /tmp/opencode/r1-toolchain/abi.o - <<< 'class RFs {}; class TDesC16 {}; class RFile {public: int Open(RFs&, TDesC16 const&, unsigned int);}; int invoke(RFile& f, RFs& fs, TDesC16 const& n) { return f.Open(fs,n,0); }'
nm /tmp/opencode/r1-toolchain/abi.o
objdump -dr /tmp/opencode/r1-toolchain/abi.o
```

Result: undefined `_ZN5RFile4OpenER3RFsRK7TDesC16j`; relocation `R_386_PC32` to that symbol. The call pushes `0`, name, fs and `this`, then caller restores 16 bytes. A second otherwise identical probe with `__attribute__((thiscall))` on `Open` (`/tmp/opencode/r1-toolchain/abi_thiscall.o`) moves `this` into ECX, pushes three arguments and does not pop their 12 bytes at the caller. `kernelhwsrv/userlibandfileserver/fileserver/bx86/efsrvu.def:141` instead describes `RFile::Open` as MSVC `__thiscall` `?Open@RFile@@QAEHAAVRFs@@ABVTDesC16@@I@Z @ 140`. **Neither the MSVC decorated name nor calling convention matches default host GCC.** The probe supports a mismatch; it does not prove exactly which historical GCC version/options the upstream platform used.

## bx86 comparison and decision

`kernelhwsrv/kernel/eka/bx86/euseru.def:1-12,1053,1657` has MSVC-decorated EUSER exports (Start @ 1656, Install @ 1052), wholly different from bx86gcc's Start @ 426 / Install @ 428. `fileserver/bx86/efsrvu.def:38,141,153-160,214-221` does provide matching-style MSVC EFSRV names (Connect @ 37, Open @ 140, Read/Write overloads). **bx86 has a more complete *source export-list pair*, not a verified feasible build:** no matching MSVC compiler, Symbian build tools, E32 artifacts or import libraries were found locally. Wine alone is not a toolchain. A GCC-built caller cannot import bx86 ordinals via a name-translation trick: calling convention, C++ layout, exception/TLS/startup and loader behavior remain distinct. Do not create a synthetic `bx86gcc/efsrvu.def` by renumbering bx86, or label a port-side substitute as upstream EFSRV.

## Required evidence before choosing a route

1. Locate/provision a **versioned, legally usable** upstream x86 SDK/build environment with EUSER and EFSRV import libraries and tools. Record compiler/linker/build-tool versions and hashes, SDK/export-header revision, machine/configuration (bx86gcc versus bx86), and the exact EFSRV export list **from the matching build**. Confirm whether this branch intended GCC EFSRV to use a different export directory or generated definitions; no inference from absent files.
2. Build unchanged small upstream-source client and genuine EUSER/EFSRV with that *same* ABI; archive build logs, imports and built DLL export maps. Verify `RFs::Connect`, `RFile::Open/Read/Write/Close`, `CActiveScheduler::Install/Start`, startup/entrypoint, relocation types and dependency order with binary inspection. A link against stubbed symbols, a rebuilt app with edited source, or an independently generated port DLL does not establish this gate.
3. Have R1-STARTUP verify thread/proc startup, TLS and heap assumptions and R1-WIRE verify F32 subsession IPC framing; have R1-PROOF-SOURCE select unchanged proof source. Integrator then resolves route and records an explicit versioned R1-ABI/v1 decision. Until these checks pass, R1-EUSER and R1-FS implementation remain gated.

Verification: `git rev-parse HEAD` in both trees, source/definition searches, tool presence probes, `g++ -m32` ABI objects, `nm` and `objdump` succeeded. No genuine DLL, import library or unmodified app was built; no QEMU/stable suite or manual hardware test was run. Failure limit: local searches do not prove that a matching SDK/artifact is unavailable elsewhere.
