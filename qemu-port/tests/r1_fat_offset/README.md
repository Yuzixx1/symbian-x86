# R1-FAT-OFFSET — implementation and exact dirty-baseline diff

## Card 18 follow-up — storage flush and quarantine (2026-10-23)

**This section supersedes card-16 statements below about missing default flush,
per-volume-only fail-stop and external legacy quarantine.** All source remains
unrun in a guest. No device writes, QEMU, target link or shared build edits.

Owned additional files: qemu_ata.cpp, qemu_ahci.cpp and **only two storage
declarations** in qemu_variant.h. Current dirty main copied/hash-recorded before
editing in `/home/yuzi/.cache/sybian-r1/r1-storage-flush-v1`. Exact originals and
SHA-256 map: `card18-baseline/`, `card18-baseline/provenance.json`. Diff against
those exact five dirty files: **card18-storage.diff**, not against old HEAD.
Original card-16 baseline/diff retained as historical evidence, not this merge
base. Initial fat32 SHA-256:
`5fb21bc0ffb3f1bcbd10e32125475f937b66547071bcb52231e25b9f0f2e4a4f`.

### Early source/header checkpoint and API

`AtaFlush(int actualDevice)` routes 0..3 to IDE channel/slave, 4..7 to
`AhciFlush(actualDevice-4)` (local AHCI port index). Invalid indices rejected;
absent/ATAPI rejected. Caller retains existing storage serialization. No new
lock, asynchronous ownership or channel ABI. `R1Fat::Flush(f)` now defaults to
real AtaFlush; `Flush(f, injectedHook)` retained for tests/adapters. Header
checkpoint was returned before host driver/fault validation.

IDE explicit flush waits channel idle, selects exact device, rejects active DRQ,
issues E7 FLUSH CACHE, delays for status settling then polls BSY/DRQ completion;
ERR/DF, absence and finite timeout propagate. Existing read/write bodies unchanged
(including old implicit write flush behavior); use explicit flush for the reviewed
completion boundary. Unsupported command abort is error, never fake success.

AHCI rejects occupied CI/SACT before touching shared command tables, waits BSY/DRQ
clear, issues non-data E7 or EA for selected port/LBA profile with PRDTL=0, polls
actual CI and task-file/error status, checks ERR/DF, TFES/host/interface errors and
finite timeout. Command-table compiler barrier before CI; x86 coherent identity
DMA assumptions unchanged. Error while CI remains set propagates without reuse or
recovery attempt. Caller must quarantine. Timeout does not mean command was
cancelled. No physical power-loss durability, controller recovery or time-based
deadline claim (iteration budgets are existing bring-up polling style).

### Quarantine and operation tracking

Boot-lifetime quarantine is indexed by **dispatch device 0..7**, not reusable
volume slot/letter/partition. All partitions and duplicate mount aliases on that
device fail-stop together. Drop/remount/format/close cannot clear it; reusing a
mount slot for another healthy device does not transfer poison. Conservative
scope also covers future replacement media at the same dispatch number. No
serial-number remapping/physical identity proof across two different dispatch
numbers or external raw-driver callers; such remapping/raw writes must stay out
of the serialized FAT namespace. Read-only legacy access/topology may still work;
all FAT writes are denied. No repair tool provided.

FatWrite/FatRename/FatDel/FatMkdir/FatFormat check before mutation; path wrappers
reach those same checks. A guarded sector-write boundary covers **every write
from fat32.cpp**, including helpers, directory extension, FSInfo and format,
preventing bypass even through topology aliases. Healthy device legacy behavior
unchanged (including disk-full overwrite preservation and old error mapping).

New API operation scope records device write-attempt counter on selection. Any
unsuccessful exit after an attempted write quarantines, including successful
end-marker writes followed by later reads failing, directory extension, actual
create publication followed by final entry read failing. Write attempts count
even when lower layer returns error, because completion can be uncertain. Open,
Write and Resize mark success only at their complete exit. Existing conservative
MutationError path remains (can quarantine disk full without a write). Read-only
preflight errors before any write do not automatically quarantine. Real/injected
flush failure always quarantines. Whole original volume/CWD state restores on
all exits. Legacy operations' own historic errors do not introduce new poison
policy; after new API poison their writes cannot proceed.

### Validation and limits

After actual changes, `bash tests/r1_fat_offset/run.sh <private-output>` (from
qemu-port) passes three actual-source host executables:

* FAT existing boundaries/resize/fault/legacy tests plus default flush routing,
  all legacy mutation denial after poison, device partition/letter alias,
  drop/re-add persistence, healthy other-device writes, create read-fault sweep
  before/after end-marker/extension/publication, successful writes not poisoned.
* ATA source with injected port reads/writes: four IDE selectors, AHCI routing,
  invalid/absent device, delayed completion, ERR, DF, busy and stuck DRQ timeout.
* AHCI source with injected MMIO: all four ports, E7/EA FIS, zero PRDT/write bit,
  occupied slots/SACT, preissue busy, CI timeout, task-file ERR/DF/postcompletion
  busy, TFES/host-bus errors, absent and invalid ports. No real port/MMIO access.

Driver injection is compiled only under `R1_STORAGE_HOST_TEST`; production
register access otherwise unchanged. On 64-bit host AHCI tests use -fpermissive
solely for existing 32-bit DMA pointer casts; no DMA dereference simulated.
Private **32-bit genuine-header compile-only PASS** for all three modified .cpp
files with current main KERN_FLAGS/DEFS/INCLUDES and private copied dirty header.
Exact argv/rc/warnings: private `test-output/{fat32,qemu_ata,qemu_ahci}-compile.json`;
host logs `test-output/tests.log`. No target link, sanitizer claim or stable-suite
run. Existing upstream offsetof/temporary-ref/legacy warnings remain disclosed.

Coordinator aligns card-15 backend with same header/lock/mutation semantics,
then serializes integration. Later disposable-image tests: flush error propagation,
PortFile snapshot/disk-full, poisoned alias/remount/format denial, healthy-volume
regressions, two-boot persistence and owner PC observations. Host completion
models do not establish device behavior, actual durability or runtime acceptance.

---

## Historical card 16 handoff (superseded where stated above)

Card 16, 2026-10-23. Source + compiled host tests; **no device writes, disk-image
access, guest execution, QEMU, shared integration, commits or pushes**.

## Ownership and merge baseline

Only `variant/fat32.cpp`, new `variant/r1_fat_offset.h`, and this test directory.
Isolated worktree: `/home/yuzi/.cache/sybian-r1/r1-fat-offset-v1`, detached port
HEAD `fa62323eea7bc96cefd666e40f5189567731bd44`. Copied current dirty main before
editing; **not an old-HEAD patch**. Exact original retained as
`baseline/fat32.cpp`; SHA-256:

`074dda7f6d50b46bae010416c27a7cedffab9df6cdf25d2c5fc63247df02a920`

`baseline/provenance.json` records main path, HEAD, initial dirty status, FAT and
shared-header hashes. `fat32-baseline.diff` applies to that exact FAT file.
Before merge verify main still has that hash; otherwise coordinator must
reconcile, never overwrite a newer dirty baseline. New files copy separately.
Original multi-volume/CWD/path/rename/whole-file implementations are unchanged
except top comment/include and new appended backend before FatFormat.
No qemu_variant.h, userfiles/channel/ATA/AHCI/Makefile/upstream edits.

## Early integration interface (proposal, not frozen shared ABI)

`R1Fat::Open(drive, absoluteShortNamePath, createExclusive, File&)`, Size,
Read/Write(offset,length,done), Resize, Flush, Close. Header contains exact
types/ownership. Coordinator routes this checkpoint to card 15; no automatic
cross-worker header edits. Kernel-only trusted calls; channel/IPC marshaling,
user pointers, public namespace authorization, path encoding, share rules and
process lifecycle belong to later adapters. Reject drive prefixes, LFN, empty
components, dot/dotdot and non-8.3 components; directories must exist. Create is
exclusive, Open never truncates. File token re-reads actual entry/size/chain per
operation and matches short-name identity, drive/device/partition and entry slot.
Not a public persistent handle or remount-generation-safe token.

Caller holds existing **UserFilesLock** for ALL API operations, even Close;
same serialization required for shell/mount/legacy operations. API never locks
recursively, waits for IPC, allocates whole files or touches user memory.
Synchronous sector I/O/optional flush only; integration must select a context
where blocking/polled I/O is allowed. Close all tokens before mount/drop/format/
rename/delete; sharing/lifetime is service-owned. Scope restores exact original
selected volume and CWD even on failure and failed opens.

## Offset/error contract

- FAT32 uint32 size/offset limit (not a 64-KiB wrapper). Read beyond EOF succeeds
  with zero bytes. Positioned write clamps beyond EOF to EOF, no holes; overflow
  checked after clamp. Zero length no mutation. Errors preserve actual code.
- `done` starts zero, counts only copied read bytes or successfully written DATA
  bytes. Metadata can fail after a successful data sector: `done>0` with error
  is intentional, **not a published/durable byte promise**. FS must inspect both.
- Resize changes on-disk size only, never caller's FS position. Growth contents
  unspecified; fresh clusters zeroed, retained partial-cluster bytes may reappear.
  Shrink publishes size before detaching/freeing chain; no freed bytes exposed as
  live after successful sector calls. Same size is no-op.
- New path uses error-aware bounded chain/cluster/slot helpers and real geometry,
  updates all FAT copies through existing FatSet, checks directory-sector errors,
  guards cyclic/short/invalid chains and volume/device bounds. Existing helper
  error collapsing/unchecked freeing remains legacy-only, not reused by new
  allocation/free operations. Primary FSInfo (+1 in existing port profile) valid
  free/next hints invalidated to unknown before allocation/free; I/O errors
  propagate, not cosmetic success. No FSInfo repair/backup-hint guarantee.
- Mutation-stage failure (including disk full) conservatively **fail-stops that
  API volume**. Can leave additional linked capacity, orphan clusters, divergent
  FAT copies, modified data, shortened size or uncertain directory sector. No
  speculative rollback after uncertain writes, atomicity, journaling or crash
  recovery promise. Other tokens on that volume fail Corrupt. Existing API does
  not clear state on remount: reboot/offline repair required. Earlier read-only
  preflight errors do not poison. DiskFull is KErrDiskFull in the new API, legacy
  allocation remains KErrNoMemory to preserve old behavior.
- Legacy APIs deliberately unchanged; they do NOT consult new fail-stop state.
  After backend mutation error integration must quarantine the volume from legacy
  writes externally as well. Do not offer simultaneous uncoordinated mutations.
- Metadata end-marker preservation: when consuming directory end slot, install
  next end marker first across entry/sector/cluster boundaries. Extra directory
  capacity can remain on create failure; failed open has no live File token.

## Flush is deliberately not fake durability

Current qemu_variant.h exposes AtaRead/Write, **no correct-drive cache-flush
hook**. `Flush(file)` therefore returns **KErrNotSupported**. Future integration
may pass `FlushDrive(int actualDevice)`; it must issue real drive-cache flush,
wait for completion and propagate actual error. Callback runs under same lock,
must not recurse/wait for IPC. Test callback proves routing/error propagation
only, not any device flush. Flush error fail-stops API volume. Close releases
token only, never silently flushes or claims durability. ATA/AHCI/shared-header
extension remains coordinator-owned blocker before durable file acceptance.

## Validation

From project root, with private output directory:

```sh
bash qemu-port/tests/r1_fat_offset/run.sh /path/to/private/output
```

PASS actual compiled **fat32.cpp** (included unchanged into test TU) under C++98,
Wall/Wextra/Werror. Only pre-existing unused-function and set-but-unused legacy
warnings disabled. Shim supplies Symbian scalar/error declarations; sector
functions use two RAM vectors only, no device/file-image I/O. No algorithm model
substitutes for production implementation. Source never links driver code.

Coverage: 1/2-sector clusters, sector/cluster crossings, exact 65535/65536/65537,
unaligned overwrite with preserved neighbors, EOF clamp/read, shrink/regrow/zero,
reopen, duplicate create, malformed names/missing parents, nested directories,
directory extension, original CWD/drive restoration, partial read/write errors,
metadata after data failure, first/second FAT failures, FSInfo write error,
shrink detach failure, cycle rejection, missing flush and correct-drive flush
error, write fault sweep, old whole-file disk-full overwrite retains old data,
old snapshot read buffer TooBig. Not a complete filesystem fuzz/corruption audit.

Private kernel **compile-only PASS** using current main Makefile KERN_FLAGS,
KERN_DEFS and KERN_INCLUDES, genuine upstream headers plus copied dirty shared
header. Exact argv/rc/diagnostics in worktree `test-output/kernel_compile.json`;
output `fat32.o`, no target link. Warnings are existing upstream temporary-ref/
offsetof and unchanged legacy enum conditional warnings. Initial harness command
split include paths containing spaces incorrectly; corrected argument formation,
no production change. Host test first compile fixed shim declarations only.

Sanitizer compile attempted, **unavailable**: linker cannot find
`/usr/lib64/libasan.so.8.0.0`. No installs or sanitizer pass claimed.
All actual validation commands operate only in private scratch/RAM.

## Future serialized acceptance / handoff

Coordinator reviews interface against concrete card-15 backend before wiring.
Need real drive flush, legacy-volume quarantine and exclusive topology rules;
no channel/shared-header adapter supplied here. Stable suite NOT run. Later
serialized disposable-image QEMU: PortFile snapshot and disk-full regression,
multi-drive nested paths/restoration, offset fixture >64 KiB, service lifecycle,
cancel/IPC errors, genuine flush failures, two-boot persistence. Owner PC media
checks later, never infer them from RAM-sector tests. Controlled service failures
remain distinct from unexpected kernel/scheduler failures.

Return paths: this source/header/tests and **fat32-baseline.diff**, baseline hash
above. No ABI freeze, no runtime acceptance, no QEMU-ready claim. Preserve all
existing peer/user changes during coordinator merge.
