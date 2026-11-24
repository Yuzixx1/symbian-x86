# R1Fs kernel-glue removal — independent read-only review

Review snapshot: 2026-11-24 ~19:0 EET. File mtimes at review time:
`qemu-port/Makefile` 18:49:30, `variant/qemu_variant.h` 18:49:37,
`variant/qemu_variant_asic.cpp` 18:49:36, `build/kernfull.elf` 18:56:21
(rebuilt after the edits; variant files were being touched concurrently by
the svcmgr apply agent — this review reflects the above snapshot).
Read-only review; no QEMU runs; `kernelhwsrv/` untouched
(`git status --porcelain kernelhwsrv/` = empty).

## Verdict: CLEAN

The working-tree diff (`git diff -- qemu-port/Makefile
qemu-port/variant/qemu_variant_asic.cpp qemu-port/variant/qemu_variant.h`)
matches the landing report exactly, with no unrelated changes in those files.

## Evidence

1. **Makefile** — `build/r1_fs_core.o` and `build/r1_fs_fat.o` removed from
   `KERNFULL_OBJS` along with their two build rules; replaced by an
   explanatory comment (Makefile:322-324). `build/fat32.o` is still in
   `KERNFULL_OBJS` (Makefile:318) with its rule intact (Makefile:279-280) —
   the kept FAT32 port adapter used by the direct `FatMount` path.
2. **variant/qemu_variant_asic.cpp** — `#include "../fs/r1_fs_core.h"` /
   `r1_fs_fat.h`, the `gFatBackend`/`gCore`/`gSession` globals and the
   `R1FsSession()`/`R1FsCore()` exports are gone. `Asic::Init3()` now calls
   `TInt mr = FatMount(4, 0);` directly with error handling
   (`if (mr != KErrNone) KPrintf("KERN FatMount(4,0) failed r=%d\r\n", mr);`).
   Ordering verified: `FatMount(4,0)` (~line 140) executes before
   `SvcMgrExecInstall()` / `GenuineBootSpawn()` in the same `Init3`, and all
   `FatUse('C')` + `FatReadPath` consumers in `variant/e32load.cpp`
   (lines 668, 1302-1305, 1390-1392, 1458-1461, 1502-1505) run on the boot
   DThread spawned by `GenuineBootSpawn()` — i.e. strictly after the mount.
   First `FatMount` claims C:, matching the `FatUse('C')` consumers.
3. **variant/qemu_variant.h** — `namespace R1Fs { class Core; }` and the
   `extern "C" R1FsSession()/R1FsCore()` declarations removed; no dangling
   references.
4. **Symbols** — `nm build/kernfull.elf | grep -c "R1Fs::"` = **0**
   (report claimed 56 → 0; confirmed on the 18:56 build).

## Remaining R1Fs/r1_fs references (all intentional, none are the removed glue)

- `Makefile:322-324` — comment only.
- `variant/r1_fs_channel.{h,cpp}` — still compiled
  (`R1_FS_CHANNEL_OBJ`, Makefile:349-355, added at :355). This is the
  separate kernel-side channel pin/revoke/install bridge for the genuine
  user-mode file server (see its header: pin a DProcess before channel
  open), NOT the removed in-kernel `r1_fs_core`/`r1_fs_fat` glue. Out of
  scope for this removal; retained deliberately.
- `fs/r1_fs_*` (core/fat/server/channel/wire) — user-mode copy retained by
  design; genuine EFSRV/svcmgr link it via `user/user.mk:352-373`.
- `user/svcmgr.cpp`, `user/e32cmn.h`, `services/r1_service_host.*`,
  `genuine/efsrv_min/*`, `tests/r1_fs_*` — user-mode/test consumers of the
  retained `fs/` copy; unaffected.

## Issues

None. No blockers. `kernelhwsrv/` verified clean (no modifications).
