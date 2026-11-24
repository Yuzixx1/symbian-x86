# DANGLING_REFS_SWEEP — post-deletion reference sweep (2026-11-24)

Scope: prove no build/test/doc reference to the deleted portfilesrv files
(`user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, `build/user/PORTFILESRV.E32`,
`:portfilesrv`) or the removed kernel r1_fs glue (`build/r1_fs_core.o`,
`build/r1_fs_fat.o`, `R1FsCore`, `R1FsSession`) breaks the build.
Swept: Makefile, toolchain.mk, user/, tools/, variant/, services/, tests/, fs/,
boot/ console/ disk/ inc/ loader/ mmu/ nk/ ports/ rom/ r1-console/, genuine/
code (docs/, .sybian/, build/ artifacts excluded). Deleted files confirmed absent
on disk. kernelhwsrv/ untouched.

## Make dry-runs (read-only)

| Command | Exit | Dangling-ref hits |
|---|---|---|
| `make -n userland` | 0 | 0 |
| `make -n kernfull` / `make -n build/kernfull.elf` | 0 | 0 |
| `make -n isogrub` | 0 | 0 |

(`make -n kernfull.elf` exits 2 "No rule to make target" — pre-existing target
naming; the real targets are `kernfull` / `build/kernfull.elf`, both clean.)
No dry-run line references portfilesrv/PORTFILESRV/mke32_portfilesrv/
r1_fs_core.o/r1_fs_fat.o/R1FsCore/R1FsSession.

## Reference table

| Ref | Location | Class | Action needed? |
|---|---|---|---|
| `portfilesrv.cpp` / `mke32_portfilesrv.py` / `:portfilesrv` paths | `tools/r1_shim_retire_plan.py:82-84,167-169` | comment/plan-data strings (retire plan tool, not a build dep) | No |
| `SvcMgrBootLaunch ... \PORTFILESRV.E32` | `variant/e32load.cpp:1478,1552`; `variant/qemu_variant_asic.cpp:169`; `user/svcmgr.cpp:250` | comment/prose (retired markers) | No |
| `PORTFILESRV.E32` service row | `user/svcmgr.cpp:250` | commented-out code (bypass, by design) | No |
| `portfilesrv` mentions | `user/user.mk:56,66,262,426,...`; `user/*.c` headers | comment/prose | No |
| `"PORTFILESRV.E32"` import-string literals | `user/filemgr.c:45`, `user/fcopy.c:171` | runtime string in PE2 import list, not a file dep | No |
| `build/user/r1_fs_core.o`, `build/user/r1_fs_server.o` rules | `user/user.mk:353,358,361,373`; `genuine/efsrv_min/build_server.sh:16` | live build rules, but sources `fs/r1_fs_core.cpp` / `fs/r1_fs_server.cpp` still exist (user-mode copy intentionally kept) | No |
| Kernel `r1_fs_core.o`/`r1_fs_fat.o` removal note | `Makefile:322-324` | comment/prose; rules already gone; `r1_fs_channel.o` rule (Makefile:349-353) uses existing `variant/r1_fs_channel.cpp` | No |
| `R1FsCore`/`R1FsSession` | none live; only docs + `fs/r1_fs_core.mmp` TARGET name `portfilesrv.exe` (proposed MMP, not built) | comment/prose | No |
| `user/portfilesrv.cpp + fs/r1_fs_*` gate label | `tools/r1_genuine_retire_test.py:427` | description string only; gate checks genuine build artifacts, not the deleted file | No |
| `PORTFILESRV.E32` staging as `EFSRVMIN.E32` host | `tools/r1_genuine_efsrv_test.py:91,152`; `tools/r1_genuine_fat_io_test.py:94,159` | test-fixture name on staged disk; uses genuine prebuilt, not deleted files | No |
| `::portfilesrv` disk staging from cache | `tools/test_e32*.py`, `make_disk.py`, `make_fat_disk.py`, `inspect_imports.py`, `inspect_relocs.py`, `r1_e32_validate_test.py`, `r1_qemu_smoke.py:36`, `r1_genuine_ewsrv_test.py:39`, `r1_genuine_pointer_test.py:47` | test-fixture-path — cache prebuilt (`~/.cache/sybian-r1/r1-portfilesrv-{e32,flat}-v1/output/portfilesrv`, both exist) | No |
| `portfilesrv` shell/keyword strings | `tools/r1_qemu_smoke.py`, `r1_selftest.py`, `r1_shot_test.py`, `r1_genuine_fullchain_test.py:75,274,299`, `tests/r1_file_regression/` | test keyword/log matching or cache fixture path | No |
| `portfilesrv.exe` prose | `services/r1_service_host.{cpp,h}`, `services/r1_service_state.h` | comment/prose only; includes are `r1_service_host.h`/`r1_service_state.h`/`e32std.h`/`e32svr.h` — nothing deleted; not referenced by any Makefile | No |
| `tests/r1_fs_core/core_test.cpp` `#include "r1_fs_core.h"` | `tests/r1_fs_core/` | live include of existing `fs/r1_fs_core.h` | No |
| `build/user/PORTFILESRV.E32` path | nowhere in Makefile/toolchain.mk/user.mk/tools/variant/services/tests | — (fully purged) | No |

## Verdict: **CLEAN**

No build-breaking references remain. All hits are comment/prose, live rules
whose sources still exist, or test fixtures served from the prebuilt cache.
`make -n` dry-runs for userland / kernfull / isogrub all succeed with zero
dangling references. Safe to proceed with the cleanup commit.
