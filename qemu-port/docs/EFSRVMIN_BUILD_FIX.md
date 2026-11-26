# EFSRVMIN build_server.sh fixes

Two fixes to `genuine/efsrv_min/build_server.sh`:

1. Dropped `build/user/euser_shim.o` from the `ld` line; that object does not exist (shim retired, `user/user.mk:204`).
2. Compile `fs/r1_fs_core.cpp` + `fs/r1_fs_server.cpp` into `$D` instead of linking nonexistent `build/user/r1_fs_*.o`; `build/user/%.o` only reaches `user/`, and no `user.mk` rule remains for `fs/` since the svcmgr retirement.

Evidence the objects are genuinely needed: `efsrv_min.cpp:300` calls `R1Fs::FileServer::NewL(*backend)`, and `fs/r1_fs_server.cpp:15` holds `R1Fs::Core&` as session state (`fs/r1_fs_core.h:21` declares `class Core`).

`bash -n genuine/efsrv_min/build_server.sh` passes. No successful link is claimed: no `/tmp/opencode/efsrvmin_build.log` exists to cite.

## 2026-11-25 build verdict

FIX INCOMPLETE (history; superseded below) — script compiles but does not link. Build exits 1 with a linker error: undefined `CleanupStack::iStack`, referenced from `r1_fs_server.o` in `R1Fs::FileServer::NewSessionL`. `EFSRVMIN.E32` is therefore NOT produced; the 16,460-byte artifact still in place is stale from Nov 24, and its companion `.elf` was deleted when the link failed. Note the earlier gcc failure was environmental, not source: `LD_LIBRARY_PATH` was leaking Mistral-Vibe's `libstdc++` into the cross-compiler.

## 2026-11-25 SUCCESS

Adding `CBase* CleanupStack::iStack = nullptr;` at `genuine/efsrv_min/efsrv_min.cpp:49` resolves the undefined reference. Rebuild with `env -u LD_LIBRARY_PATH` exited 0; log `/tmp/opencode/efsrvmin_build2.log` shows `EFSRVMIN OK: genuine/efsrv_min/EFSRVMIN.E32` and `FATIOCLIENT OK: genuine/efsrv_min/FATIOCLIENT.E32`, with zero undefined refs and caps0 fixed up `0x0 -> 0x100` (header CRC recomputed).

Fresh artifacts: `EFSRVMIN.E32` 16,396 B at 21:37:03 (the stale 16,460 B Nov-24 image is replaced), alongside `FATIOCLIENT.E32` 6,412 B.
## 2026-11-25 no-prefix rebuild

The `env -u LD_LIBRARY_PATH` prefix is no longer needed. All four genuine build scripts now `unset LD_LIBRARY_PATH` themselves immediately after `set -e`: `build_server.sh:7`, `build_dm.sh:9`, `build_probe.sh:5`, `build_hal.sh:6`. A plain `./build_server.sh` exited 0, printing `EFSRVMIN OK` and `FATIOCLIENT OK`; `EFSRVMIN.E32` rebuilt at 21:40:39, 16,396 B, md5 `97cd6bd2...`, caps0 `0x0 -> 0x100` with the header CRC patched. Log `/tmp/opencode/efsrvmin_build3.log` records zero errors.
