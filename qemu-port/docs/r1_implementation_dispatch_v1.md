# R1 implementation dispatch — cards 14–17

Latest: cards 22/23/24 returned. New offers 25/26 below. Card 25 exclusive QEMU.

Latest: 18 returned/reviewed; 19 recovery stop resolved by coordinator note
`r1_console_data_v2_recovery.md`. **Resume 19 without original prose report**,
verify surviving source hashes, then resolver/fix/single EKDATA-only build.
20/21 continue. Card 18 worker may stay available for integration fixes; no
extra standalone task required while kernel link/channel handoffs are pending.

Current: 14/15 returned; 18/19 continue pending handoffs. New offers 20/21
below. No guest execution or QEMU allocated. New cards supersede old statuses.

Latest partial return: **14/15 continue; 16/17 returned; 18/19 offered below.**
Prior offered statuses are historical. No need to wait for all workers.

2026-10-23. **Offered, not confirmed launched.** Read Common instructions in
`r1_next_wave.md`, latest reconciliation and current dirty source. Explicit
ownership below overrides common prohibition on implementation/existing files
only for named paths. Four strong workers; no agents spawned automatically.
No upstream edits, commits/pushes, installs, artifact cleanup or QEMU. Shared
integration/Makefile/stable-suite work is reserved for coordinator after review.
Use current dirty baseline in isolated worktrees, never old HEAD alone.

## 14 — R1-KERNEL-ADAPTER (strong; critical-path implementation)

Own new `qemu-port/variant/r1_g_objects.{h,cpp}`,
`qemu-port/variant/r1_g_bootstrap.{h,cpp}` and
`qemu-port/tests/r1_kernel_adapter/` only. Read current dirty e32load.cpp,
genuine kernel source, actual reviewed core headers and reconciliation.
Implement LoaderObjects with real Direct-model DProcess/DCodeSeg objects,
temporary refs, create-info copyback, data template, dependency refs/finalization
and abort. Implement bootstrap API accepting trusted immutable manifest byte
views and argument-only command line; instantiate bounded ImageSource/storage.
Do not route G through legacy parser/CRC/pilot imports.

First target: suspended zero-capability R1-A heap/sched EXE plus genuine
EUSER/EFSRV. Preserve true security, effective kernel flags, H0, genuine entry
and no double-base. Existing L untouched. Check locking/CS and stillborn
ownership against real source, not fake-adapter assumptions. If core contract
cannot express lifecycle, return exact minimal correction immediately; no fake
success or upstream patch. Bootstrap-only publication may return a real handle
to the launching kernel thread, with explicit ClientContext contract; **never
impersonate arbitrary user clients**. Dynamic library publication may return
NotSupported pending genuine client-handle rollback. Leave process suspended;
integration explicitly resumes after diagnostics.

Private kernel compile-only allowed with current flags/includes. No shared
Makefile/e32load.cpp/shell edits, target link, guest execution or SDK rebuild.
Return usable source, not design-only. README includes compile evidence, kernel
ownership mapping, exact integration calls, pre-Resume diagnostics, future
QEMU/stable assertions and manual launch/reap. Send early interface/blocker
checkpoint; coordinator integrates shared files after review.

## 15 — R1-FS-CORE (strong; user-mode implementation)

Own new `qemu-port/fs/` and `qemu-port/tests/r1_fs_core/` only. Implement
bounded compatible !FileServer first-stage source: genuine EUSER CServer2,
sessions/typed IPC, file subsessions and minimum fixture operations through an
injectable backend. Adopt file-review semantics, exact opcodes/package sizes,
share rules, panic/error ordering, offset/size state and Flush vs public Close.
Async ownership must be honest: implement bounded pending queue/cancel state
or reject unsupported operations explicitly, never fake completion. No private
replacement EFSRV/public client ABI. No !Loader registration in first FS phase.

Deliver concrete C++98 backend header early under fs/: paths/handles, partial
counts/error state, Resize/Flush/Close and authorization requirements. Kernel
channel is separate integration; use fake backend tests first, no raw pointer
IPC. Public fixture-data namespace only; executable sysbin manifest is never
writable here. Supply real MMP with service capabilities justified from source;
fixture remains NONE. Startup follows adopted phased service boundary.

Host-test actual portable state/decoder logic: malformed frames, both share
orders, EOF/no-hole, shrink/regrow retained position, failed opens and partial
I/O errors. Compile-only genuine user source permitted in private matching SDK,
no target link/runtime/shared build edits. Tests prove no genuine IPC behavior.
README lists console/ESHELL unsupported calls, six-driver gaps and future
QEMU/stable/manual checks. Return early header checkpoint, then finish source.

## 16 — R1-FAT-OFFSET (strong; exclusive dirty-file implementation)

Own `qemu-port/variant/fat32.cpp`, new
`qemu-port/variant/r1_fat_offset.h` and `qemu-port/tests/r1_fat_offset/` only.
Copy/hash **current dirty main fat32.cpp** baseline first; preserve existing
multi-volume/PortFile work and return diff against that baseline, never old HEAD.
No other worker owns fat32.cpp. No qemu_variant.h/userfiles.cpp/channel/ATA/AHCI
edits. Expose narrow internal lookup/create/offset read/write/resize/flush/close
methods with explicit count/error/failure-state contracts.

Use real private geometry/chain helpers, error-aware FAT/directory updates,
short-name creation and prior-volume restoration. Define existing UserFilesLock
contract, no recursive lock or waiting for IPC under lock. Preserve PortFile
snapshot/disk-full overwrite behavior. No 64KiB whole-file wrapper. Specify
partial writes and resize failure state honestly; no unproved atomicity/crash
guarantees. Flush needs real correct-drive cache-flush error propagation; if a
lower hook is missing, report it, never fake successful durable flush.

Host-test actual compiled implementation through injected sector I/O where
feasible: sector/cluster and 65535/65536/65537 boundaries, grow/shrink, disk full,
metadata/I/O faults, old-path compatibility and volume restoration. Private
compile-only allowed, no device writes/shared disk images/guest runs. Align
backend with card 15 through coordinator after early checkpoint; independent
kernel header remains owned here. README records future PortFile/stable/manual
tests and exact merge baseline/hash. Shared integration stays serialized.

## 17 — R1-CONSOLE-STRICT-BUILD (strong; narrow fix + corrected build)

Own `qemu-port/r1-console/hal/hal.mmp`,
`qemu-port/r1-console/keymap/ekdata.mmp`, new
`qemu-port/r1-console/keymap/ekdatau.def`, associated README/checker updates,
and new `qemu-port/docs/r1_console_data_build_v2.md` only. Review v1 report.
Authorize HAL strict DEFFILE stem hal.def resolving genuine halu.def, and local
EKDATA stem ekdata.def resolving port-owned byte-identical ekdatau.def from
genuine eabi/ekdata.def, with notices/provenance. No upstream changes,
NOSTRICTDEF or invented ordinals. Resolve InitialValue DATA 428 vs actual
484-byte generated array from source/tool behavior. Do not rewrite genuine
def or truncate data; report whether metadata blocks correct ordinal use.

One corrected isolated incremental HAL/EKDATA sequence in unused home-backed
`/home/yuzi/.cache/sybian-r1/r1-console-data-build-v2`; private SDK/prefix/temp,
verified reused inputs; preserve v1 artifacts. Above source recipe fixes may
precede build; stop first compile/link blocker, no retry/additional source patch
without review. Audit six HAL/three EKDATA ordinals against actual linked
symbols, imports/security/data/BSS and byte-identical aliases; no auto-exports.
No E32 execution/QEMU/shared Makefile edits. Link success alone is insufficient.

## 25 — R1-G-STABLE-INTEGRATION (strong; exclusive QEMU + stable)

Own `qemu-port/variant/kern_shell.cpp`, card-22 files (`docs/r1_g_runtime_v1.md`,
`tests/r1_g_boot/`, `tools/test_stable.py`), card-23 channel files after review,
and new `qemu-port/docs/r1_g_stable_v1.md`. Coordinator authorizes supervisor
stack fix: set `info.iSupervisorStackSize=0x8000` in `ShellSpawnFull`. No other
kernel/runtime changes; no adapter/core/security bypass.

Apply stack fix, rebuild private asset-enabled ELF, rerun card-22 targeted
sequence (reject, suspended check/abandon, heap×2, sched×2, reap, legacy survival)
and verify no FAULT/Exc. If any failure, stop with transcript and proposed fix;
do not run stable. Then integrate hardened FS channel source into main tree via
coordinator-approved copy (preserve storage header), compile private kernel channel
unit, and stage host assets. Run `tools/test_stable.py` with `R1_G_ASSET_DIR` in
authorized slot; existing 15 tests must remain PASS, G must be honestly SKIP or
PASS according to asset availability and runtime outcome. No test weakening.

Capture first G stable result, exact timings, skips, exit types/reasons, repeated
launch ownership and legacy survival. If QEMU slot exhausts, document pending
gap rather than claiming pass. No physical hardware, commits or pushes. Return
early if a blocker appears before stable wiring.

## 26 — R1-CONSOLE-STAGING (strong; host/build only)

Own new `qemu-port/r1-console/hal/hal_asset.manifest`, recipe copy updates,
and `qemu-port/docs/r1_console_staging_v1.md` only. No HAL/EKDATA logic or
genuine-definition changes. Consume card-24 manifest and accepted HAL/EKDATA
hashes. Verify byte-identical staged HAL/EKDATA.DLL/EKDATA.00 and manifest
hashes against evidence. Produce one private local asset directory with no
redistribution; default builds remain asset-free. Document exact sysbin placement,
immutable image, UID/SID/caps and zero-data requirements for future G manifest.

Do not rebuild, link, run QEMU or claim console closure. If any evidence mismatch,
stop and report; no source rewrite or NOSTRICTDEF. Coordinator copies assets and
manifest after review; no commits/pushes. Manual/hardware remains pending.

## 18 — R1-STORAGE-FLUSH-QUARANTINE (strong; follow-up to card 16)

Own `variant/fat32.cpp`, `variant/r1_fat_offset.h`, `variant/qemu_ata.cpp`,
`variant/qemu_ahci.cpp`, storage declarations only in `variant/qemu_variant.h`,
and `tests/r1_fat_offset/` (all under qemu-port). Explicitly authorized existing
dirty-file scope; capture hashes/diffs of current main before changes. Do not
overwrite unrelated shared-header changes. No userfiles/channel/loader/build
edits. Return small separately reviewable storage diff. Core adapters may read
header; coordinate any new storage declaration through coordinator.

Implement explicit correct-device AtaFlush/AHCI equivalent with real completion,
timeout/error propagation and routing for IDE vs AHCI. Preserve old read/write
behavior and no device writes in tests. Wire R1Fat Flush default to real hook;
retain injected callback tests. Never claim physical power-loss durability.
Implement fail-stop quarantine so **all legacy/public mutation entry points**
cannot write affected storage after new-backend uncertain mutation/flush error.
Account for device/partition aliases, mount/drop/remount/format and CWD restore;
no automatic clearing by remount or token close. Healthy volumes retain legacy
behavior. No repair/disk-management utility. Pick conservative scope if aliases
cannot safely be distinguished, document it. Audit mutation tracking including
create directory end-marker/extension writes followed by later read failures;
quarantine must not miss an already-mutated failing operation.

Host-test actual compiled paths with injected driver/sector completion where
feasible: correct device, errors/timeouts, old overwrite preservation when
healthy, legacy writes denied after poison, aliases/topology and other-volume
behavior. Private compile-only permitted; no shared Makefile, target link/QEMU.
Do not repeat old tests/build merely to reproduce handoff; run relevant tests
after actual changes. README states backend interface for continuing card 15,
future stable/manual steps and limits. Source/header checkpoint early.

## 19 — R1-EKDATA-LOCAL-DEF (strong; recover then narrow correction)

Override: original prose recovery is waived by coordinator; preserved evidence
is sufficient, see recovery note above. Do not repeat failed recovery search.

First recover existing card-17 report/owned diffs from original worker or
preserved v2 build copies. Claimed delivery /tmp worktree is unavailable in
coordinator environment. **Do not rebuild to recreate missing evidence.**
Return exact surviving paths/hashes; if unrecoverable, stop and report. Own
same card-17 recipe/README/checker paths and ekdatau.def, recovered v2 report,
plus new `qemu-port/docs/r1_console_data_build_v3.md`; no keymap/HAL logic edits.
Deliver recovered source baseline checkpoint for coordinator copy/review.

After recovery, inspect actual strict-DEFFILE resolution. Authorized one-line
correction `DEFFILE ./ekdata.def`, with local ekdatau.def hash-identical to
upstream genuine definition, no NOSTRICTDEF/auto-export tolerance. One private
**EKDATA-only** incremental build in unused home-backed
`/home/yuzi/.cache/sybian-r1/r1-ekdata-build-v3`; check quota, preserve v1/v2,
reuse verified SDK inputs. No HAL rebuild. Stop first failure, no retry.

Audit required ordinals 1/2/3 against actual linked ConvTable/FuncTable/Settings,
exact count three/no ignore export, imports/security/data/BSS, CRC/relocations
and identical EKDATA.DLL/EKDATA.00 aliases. Recovered HAL v2 audit may be checked
without rebuilding: six ordinals and entire InitialValue array plus prepdef
nodatasizes evidence. Report HAL separately, do not imply EKDATA success repairs
unreviewed HAL evidence. No E32/QEMU/shared build edits or runtime acceptance.

## 20 — R1-G-BOOT-INTEGRATE (strong; source integration + private link)

Own `qemu-port/variant/e32load.cpp`, new `variant/r1_g_launch.{h,cpp}`,
`variant/kern_shell.cpp`, required G-only declarations in `variant/qemu_variant.h`,
`qemu-port/Makefile`, new `tools/stage_r1_g.py`, `tools/test_r1_g_boot.py` and
`tests/r1_g_boot/`. Use exact current dirty snapshots/hashes; no old-HEAD overwrite.
Shared-header edits only launch declarations; card 18 owns storage declarations.
Do not touch adapter/core implementations without coordinator approval.

Integrate explicit new genuine launch command, never auto-fallback from G to L.
Stage three pinned genuine fixture/EUSER/EFSRV bytes through generated immutable
kernel-owned read-only arrays and literal sysbin manifest; record hashes/provenance,
local-only research assets, no redistribution claim. Gate inclusion with explicit
local asset provisioning, fail closed on missing/hash-mismatched assets. Default
make still rebuilds all existing ISOs; do not require proprietary-local assets for
ordinary legacy builds. Export shared existing TSS prerequisite without calling
legacy E32Run. Diagnostic suspended-load path first, explicit heap/sched launch
second; argument-only forwarding, measured effective security, IAT/stamp/template
checks before Resume. Genuine lifecycle/handle ownership, bounded exit observation,
no borrowed pointer after close or legacy reaper double-close. Implement abandon
cleanup for suspended success followed by diagnostic failure. Separate G process
tracking from legacy creator-ref records.

Draft QEMU regression driver and manual steps but **do not run QEMU**. Driver
assertions must include heap/sched markers, exit status, repeat launch/cleanup,
invalid manifest/import rejection and legacy survival, no file-server prerequisite.
Coordinator owns test_stable.py wiring after review. Private compile/link build
allowed in unique home-backed outputs, using copies of current generated exports;
no shared writable images/logs/default output writes. Report exact unresolved/link
issues rather than inventing kernel stubs. No SDK rebuild; no target execution.

## 21 — R1-FS-FAT-CHANNEL (strong; implementation + boundary corrections)

Own new `qemu-port/fs/r1_fs_channel.{h,cpp}`, new
`qemu-port/variant/r1_fs_channel.{h,cpp}`, `tests/r1_fs_channel/`, and reviewed
`fs/r1_fs_{backend,core,server}.{h,cpp}` only where needed for alignment. No
fat32/ATA/AHCI/shared-header/Makefile edits. Card 18 retains storage ownership.
Use copied card-15 baseline/backend SHA recorded in reconciliation.

Implement genuine private logical channel and user Backend adapter, fixed bounded
typed wire (no raw pointers/kernel objects), generation-bearing keys, bounded
copies, exact data namespace, correct locking around R1Fat calls. Authenticate
pinned **process object** on open, handle transfer and every request; SID/name/
capabilities alone do not suffice. Expose kernel-only pin/revoke API for future
bootstrap integrator, default deny until pin. No E32Loader executive via channel.
No production host launch/registration this card: bootstrap zero-cap restriction
remains; separate trusted privileged image launch policy needs coordinator review.

Resolve backend contract mismatch: R1Fat done counts data-sector success even
when metadata/volume state uncertain; Backend committed/size must not pretend
known published size. Carry explicit unknown/unstable result and fail closed,
update owned interface/core tests narrowly if required. Whole-volume quarantine
dominates path-only poison; do not recover merely by closing/reopening a share.
Align to card-18 final header via coordinator checkpoint, not simultaneous edits.
Audit actual server slot/descriptor handling against genuine EFSRV, ordinary and
wide RFile64, zero/partial/multichunk reads/writes and validation order. Keep async
queue/cancel explicitly unsupported; this is not completion of full matrix.

Host-test actual portable decoder/adapter boundaries with fake channel/storage;
private matching user/kernel compile-only allowed. No target link/host E32Main/
FsRegisterThread/QEMU. README gives exact integration/pin/channel API, permission
negatives, unknown-size/error mapping, future typed IPC and stable/manual checks.
