# Card 20 — explicit genuine bootstrap integration

2026-10-23; source/private-link handoff, **not guest acceptance**. Only card-20
paths changed. Core/adapter implementations, upstream and test_stable unchanged.
Early link checkpoint: private asset-enabled full kernel link passed without
undefined symbols; no interface/core correction required. Published interface is
`TInt R1GCommand(const char*)` and exported existing `E32EnsureUserTss()`.

## Provisioning and build

Local-only research artifacts are generated outside git, not distributed:

```sh
python3 tools/stage_r1_g.py \
  --v1 /home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel \
  --fixture /home/yuzi/.cache/sybian-r1/r1-fixture-build-v1/epoc32/release/x86/urel/r1a_fixture.exe \
  --out /home/yuzi/.cache/sybian-r1/r1-g-boot-20261023/assets
make build/kernfull.elf R1_G_ASSET_DIR=/home/yuzi/.cache/sybian-r1/r1-g-boot-20261023/assets
```

Run make only in an authorized private copy when validating; these commands do
not authorize shared-output writes or execution. Stage output must be absent.
Script pins fixture SHA256
`87fde53b240e54ac9aecf4024546b77c91f06a57d83f60c0101c704e0ed6a6a0`, EUSER
`4851fec4b6439bf5263e6f10f7f3010828f84df55df050c710f57077f2dd017b`, EFSRV
`332d97274a151bb17a18112e728684b98c8462fe8723bfadb1de0d471e372b6d`.
Provenance derives from CLEAN-BUILD v1 and fixture-build v1 reports. No SDK rebuild,
new binaries, image patches or proprietary redistribution permission is implied.
Every opted-in launch-object compile verifies decoded bytes AND exact canonical
generated source, rejecting extra injected code, missing/edited arrays or names.
Literal UTF-16 C:\sys\bin manifest and const arrays live in kernel rodata.
Assets are trusted kernel-owned staging, not public writable executable files.

Default `make` retains its existing all/kernel/ISO/userland prerequisites and
requires no assets. With empty R1_G_ASSET_DIR, a small unavailable-command object
is linked; no G core/adapter or image arrays. Launch object rebuild is forced to
avoid stale enabled/disabled configuration reuse. Opt-in directory must be a
space-free path for current Makefile conventions. No `kernfull` target was run:
that historical target executes QEMU; use **build/kernfull.elf** for link-only.

## Commands and ownership

* `gboot check`: load suspended, diagnose, abandon via genuine process Die,
  bounded death observation/reap; never Resume.
* `gboot abandon`: same positive cleanup test, no user instruction.
* `gboot heap`, `gboot sched`: load/diagnose then explicitly Resume. Only those
  argument-only UTF-16 strings are forwarded, never executable path/argv[0].
* `gboot reject-manifest`: duplicate manifest name must fail before publication.
* `gboot reject-import`: fixture-only manifest cannot resolve EUSER/EFSRV; must
  fail closed without publication. Immutable bytes are never mutated.
* `gboot reap`: poll and close dead G process handles; idempotent.

Serial kernel-shell thread only, same owner throughout; four bounded records.
TSS prerequisite is precisely existing lazy implementation renamed/exported;
legacy call preserved and no legacy E32Run call occurs from G. Diagnose measures
effective K::KernelConfigFlags and forced security bits without changing them;
checks zero-cap root SID/VID, absolute entry/veneer mapping, nonzero valid stamp,
every fixture IAT against real dependency Lookup, separate dataLoad/dataRun and
every initialized-template byte against original plus independently recomputed
data relocation. It logs entry/export/code/data mapping and template/BSS sizes.
It intentionally does not demand final process data/BSS initialization before
genuine StaticCallList/InitData. True EUSER/EFSRV security remains adapter-owned
and unchanged. No file-server prerequisite for heap/sched static binding.

On diagnostic failure or requested suspended cleanup, published process uses
Die(EExitKill,KErrCancel), NOT Stillborn/Release. Handle is transferred to G table
before that operation and retained until thread-empty death observation. Resume
is under CS/system lock; polling is bounded to 10000 kernel ticks with sleeps.
Timeout is explicit, keeps handle/pointer protected and instructs reap/kill.
Reap reads exit status while handle protects object, clears record before closing
handle, never touches borrowed pointer afterward. There is **no** G creator-code
ref close or legacy ExecRecRemember registration. Legacy E32Reap remains unchanged
and cannot double-close G. This is separate G exit diagnostics, not legacy exits/
wait ring integration. Kill on timeout uses existing shell `kill <id>`, followed
by `gboot reap` on launching thread. Never invoke adapter Abort on a published G.

## Private validation evidence

Base fa62323eea7bc96cefd666e40f5189567731bd44 plus exact dirty snapshot, **260**
file hashes in `/home/yuzi/.cache/sybian-r1/r1-g-boot-20261023/baseline.json`.
Detached worktree in that root; upstream read-only
0c3208650587ac0230aed8a74e9bddb5288023eb. Copied existing build objects/exports to
private worktree; no shared generated-output writes. Incremental private make
recompiled prerequisites as required, then linked actual current integration.
This is not a cold rebuild nor guest execution. Initial manual compile failed
owned diagnostic code's mistaken SID/VID `.iId` access; corrected scalar fields.
First link and both final configuration links passed. Existing upstream warnings
and RWX LOAD-segment warning remain, not warning-free/security isolation evidence.

Evidence root above contains `link1.json`, `link-g.json`, `link-legacy.json`,
`symbols-{g,legacy}.txt`, `hostchecks.log`. Final ELF hashes:

* G `c3be50b8438d68311a74f9232db23f1180f29d1e9720ffcf1c95dd78d58ea91f`
* legacy `678d24f345eed4a16a2a9445c2cb9460c98c5819a29427da2ab58b80c76c520d`

Both `nm -u` empty. G has three `r` arrays, lengths 0x2ccc/0x649c8/0x115b0;
legacy none and no LoadSuspended symbol. Host `tests/r1_g_boot/check.py --assets
<assets> --scratch <fresh>` passed pin verification, byte tamper and extra source
tamper rejection, source-level separate lifecycle checks. No runtime refcount,
abandonment, effective security, scheduler, heap or legacy-survival pass claimed.

## Draft guest regression/manual checks — NOT RUN

`tools/test_r1_g_boot.py` is a draft standalone driver, not stable-suite wiring.
Requires separately authorized slot, explicit `--allow-qemu`, G `--kernel`, a
disposable preseeded legacy disk with ECHO.E32, and absent `--scratch`. Uses disk
snapshot mode, no builds/asset provisioning itself. Future assertions: suspended
check/abandon no Resume, manifest/import rejection no fallback, heap and sched
mode-specific and final PASS, exit type/reason 0, repeated heap/sched launches,
empty G reap, mount and legacy ECHO survival, no kernel faults. Harness behavior
and genuine lifecycle remain unverified until reviewed guest run.

Manual future sequence: boot reviewed asset-enabled ELF, capture `gboot check`
diagnostics and cancellation death, run reject modes, heap, sched twice, gboot
reap. Require fixture BEGIN, heap-cbase-leave-trap/timer-runl PASS and matching
done PASS plus zero exit result, not just command success. Compare memory/handle/
segment baseline and repeated stamps. Inject diagnostic/handle/loader failures
under separately authorized testing; ensure abandon completes and G doesn't enter
legacy reaper. Kill/reap timed-out id explicitly. Then exercise unchanged legacy
launch/reap and coordinator stable suite. Asset-free build must report unavailable
on gboot and still run legacy. Do not start file modes before B3 or dynamic B4.
Manual hardware/disposable-PC checks require separate authorization; none here.

No QEMU, hardware, E32 execution, SDK rebuild, commits/pushes, installs or scratch
deletion. Core/adapter untouched; link success is not runtime compatibility.

## Card 22 runtime evidence (2026-10-25)

Exclusive serialized slot used. Original asset-enabled ELF faults on the second G
command: `FAULT: KERN 0x00000062 (98)` (`K::EMutexWaitNotDThread`), during
`BuildPlan`/`Open`, because the shell thread's 4 KiB supervisor stack overflows
while zero-initializing `Node root` and corrupts `NThread::iHandlers`. Preserved
transcript: `/home/yuzi/.cache/sybian-r1/r1-g-runtime-20261025/qemu/12-single6/serial.log`.

With a private diagnostic shell stack of 0x8000, the full targeted sequence
passed: reject-manifest, reject-import, check, abandon, heap, sched, repeats,
reap, and legacy ECHO survival. No `FAULT:`/`Exc ` markers. Proposed fix is
documented in `docs/r1_g_runtime_v1.md`; card 22 does not modify
`kern_shell.cpp`. Stable-suite wiring now lives in `tools/test_stable.py`.
