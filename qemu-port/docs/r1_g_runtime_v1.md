# R1-G first runtime — card 22

2026-10-25. Exclusive serialized QEMU slot used. Card-22 scope only:
G integration files, `tools/test_stable.py`, `tests/r1_g_boot/`, and this doc.
Core/adapter implementations and upstream untouched. No commits/pushes.

## Linked-input provenance

- Base `fa62323eea7bc96cefd666e40f5189567731bd44`, current dirty tree (270
  snapshot files) copied into an isolated home-backed worktree.
- Card-20 source verified against its delivered hashes; only expected drift is
  `qemu_variant.h` launch declarations moved inside the existing guard by the
  coordinator merge.
- Asset-enabled ELF and asset-free ELF both link with `nm -u` empty; ELF hashes
  match card 20: G `c3be50b8438d68311a74f9232db23f1180f29d1e9720ffcf1c95dd78d58ea91f`,
  legacy `678d24f345eed4a16a2a9445c2cb9460c98c5819a29427da2ab58b80c76c520d`.
- Assets staged locally from pinned v1/fixture bytes and hash-verified by
  `tools/stage_r1_g.py`; never distributed.

## First genuine runtime failure

Targeted sequence on the original (unmodified) asset-enabled ELF:

1. `gboot reject-manifest` succeeded (`G:rejection reject-manifest result=-11`,
   `gboot: result=0`).
2. `gboot reject-import` faulted:

```
G:buildplan enter handlers=00187bc0
G:before open handlers=75623a47
G:open enter handlers=00000000
...
G:dropedges ... handlers=6820656e
FAULT: KERN 0x00000062 (98)
```

`0x62` is `K::EMutexWaitNotDThread`: `DMutex::Wait` found
`NCurrentThread()->iHandlers != &EpocThreadHandlers`. The handlers field is
corrupted during `BuildPlan` (between `Node root={};` zero-initialization and
`Open`), not by G logic itself. The corruption is ASCII fragments of format
strings, consistent with supervisor-stack overflow overwriting the thread's
`NThread` structure. Preserved transcript:
`/home/yuzi/.cache/sybian-r1/r1-g-runtime-20261025/qemu/12-single6/serial.log`.

## Proposed fix

The shell thread is created in `ShellSpawnFull()` (`qemu-port/variant/kern_shell.cpp`)
with `iSupervisorStackSize = 0`, which resolves to `K::SupervisorThreadStackSize`
(4 KiB). G's `BuildPlan`/`Open`/`Expand` use large automatic objects
(`Node root`, `Node best`, `Node candidate`, 80-byte buffers) and exceed 4 KiB.

Private diagnostic build with `iSupervisorStackSize = 0x8000` eliminated the
fault; handlers stayed valid and `reject-import` returned `gboot: result=0`.
Recommended fix: set the shell supervisor stack to at least 0x8000 (or, better,
reduce automatic storage in `e32g_plan.cpp`/`BuildPlan` and `Open`). This is a
kernel/runtime change outside card-22 ownership and requires coordinator
approval before it is committed; card 22 does not modify `kern_shell.cpp`.

## Targeted results with the diagnostic (larger stack) build

Full sequence passed with no `FAULT:`/`Exc ` markers:

- `gboot reject-manifest`: `G:rejection reject-manifest result=-11`, result 0.
- `gboot reject-import`: `G:rejection reject-import result=-1`, result 0.
- `gboot check`: suspended load, diagnostics, `G:abandon requested`, result 0.
- `gboot abandon`: same, result 0.
- `gboot heap`: `R1A:heap:heap-cbase-leave-trap:PASS`,
  `R1A:heap:done:PASS`, `G:exit ... type=0 reason=0`, result 0.
- `gboot sched`: `R1A:sched:timer-runl:PASS`, `R1A:sched:done:PASS`,
  `G:exit ... type=0 reason=0`, result 0.
- Repeated `heap`/`sched`: both pass again.
- `gboot reap`: `G:reap count=0`.
- Legacy survival: `mount` then `ECHO.E32 legacy-after-g` prints
  `ECHO: legacy-after-g`; `gboot reap` returns 0.

Diagnostics observed include effective security flags, entry/export/code/data
mapping, stamp, IAT against real EUSER/EFSRV lookups, and relocated template
bytes. No file modes or dynamic publication were exercised.

## Stable-suite wiring

`tools/test_stable.py` now:
- builds with `R1_G_ASSET_DIR=...` only when that environment variable points
  at a valid staged asset directory; otherwise the default asset-free build.
- runs the existing checks unchanged.
- invokes `tools/test_r1_g_boot.py` with a freshly provisioned disposable disk
  only when assets are available; otherwise prints an explicit
  `SKIP test_r1_g_boot` (never a pass).
- preserves all prior tests and default all-ISO rebuilding.

## Card 25 — fix applied and revalidated (2026-10-28)

Coordinator approved the shell stack change, and it is now applied in main:

```
qemu-port/variant/kern_shell.cpp, ShellSpawnFull():
    info.iSupervisorStackSize = 0x8000;   // was 0 (=> K::SupervisorThreadStackSize, 4 KiB)
```

No other change was made to `kern_shell.cpp` (`git diff` shows only that one
line). Private asset-enabled ELF rebuilt with a clean source copy in
`/home/yuzi/.cache/sybian-r1/r1-g-stable-20261025` (sha256
`7af27986620ecbd74675b437c8424a263ee7d97c41f04cdc254a04fa1bbab9a8`).

### Card-22 targeted sequence, rerun on the fixed build

One boot, 12 commands, 43 s total (20:45:27–20:46:10 on 2026-10-25); transcript
`qemu/fullseq/serial.log` in that tree:

- `gboot reject-manifest` → `G:rejection reject-manifest result=-11`, result 0
- `gboot reject-import` → `G:rejection reject-import result=-1`, result 0
- `gboot check`, `gboot abandon` → resumed-suspended, `G:abandon requested`,
  `G:exit id=8/10 type=0 reason=-3` (abandon), result 0
- `gboot heap` ×2 → `R1A:heap:heap-cbase-leave-trap:PASS`,
  `R1A:heap:done:PASS`, `G:exit id=12/16 type=0 reason=0`, result 0
- `gboot sched` ×2 → `R1A:sched:timer-runl:PASS`, `R1A:sched:done:PASS`,
  `G:exit id=14/18 type=0 reason=0`, result 0
- `gboot reap` → `G:reap count=0`
- `mount` → `C:/>`; `ECHO.E32 legacy-after-g` → `ECHO: legacy-after-g`;
  final `gboot reap` → `G:reap count=0`
- `grep -c 'FAULT:\|Exc ' serial.log` → `0`

### Stable suite in the authorized serialized slot

Both runs are sequential, single-QEMU, and use the unchanged 15 checks.

| Run | `R1_G_ASSET_DIR` | Result | Wall clock |
| --- | --- | --- | --- |
| default | unset | build + 15 tests PASS, `SKIP test_r1_g_boot: R1_G_ASSET_DIR not set` | 8 m 42 s (17:22:18–17:31:00) |
| asset-backed | staged v1/fixture | build + 15 tests PASS, `PASS test_r1_g_boot` | 9 m 09 s (17:31:51–17:41:00) |

Asset-backed per-test end times (2026-10-28): build 17:31:54, e32_relocs
17:31:54, args 17:32:29, dll 17:33:01, data_reloc 17:33:38, userland 17:34:21,
ipc 17:34:58, cli 17:35:42, cli_diskfull 17:36:17, crash 17:36:51, conc
17:37:27, interact 17:38:02, sdk 17:38:38, samples 17:39:14, wait 17:39:51,
chunk 17:40:26, r1_g_boot 17:41:00. Each guest check is 34–44 s.

`tools/test_r1_g_boot.py` re-ran the same in-guest sequence from the asset-enabled
`build/kernfull.elf` against a disposable 64 MB seeded disk and printed
`PASS genuine heap/sched/repeat/reject/abandon and legacy survival`; its serial
transcript (no `FAULT:`, no `Exc `) is
`/tmp/opencode/stabilization/r1_g_boot_scratch/serial.log`, with the same
`type=0 reason=0` exits, `G:reap count=0` after each phase, and
`ECHO: legacy-after-g`. Assets and guest disks stayed outside the repo; no
commits, pushes or installs were made.

## Manual/hardware gate

Still pending and not claimed: no physical hardware run, no SDK rebuild beyond
the suite's own checks, and no Resume-based acceptance outside `gboot`. The
0x8000 value is a working fix, not a proof that G automatic-storage use is
bounded; reducing `BuildPlan`/`Open` stack frames remains a worthwhile cleanup.
