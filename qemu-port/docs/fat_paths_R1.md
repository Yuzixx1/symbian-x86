# R1-FAT-PATHS — FAT I/O path mapping (research note, no code)

Card: **R1-FAT-PATHS** (owner-directed; complements the R1-FAT card in
`parallel_work.md`). Research-only: map the FAT backend's actual I/O paths,
sizes, error/transaction semantics and crash risks, and derive design
requirements for R1-FAT's stated goals — *"general offset read/write/seek,
metadata/error handling and >64 KiB"* — **without implementing anything**.
No code or tests changed; this note is the deliverable. Companion to
`startup_paths_R1.md` (exec loads its images through the paths described
here). Base tree: commit `fa62323` + current uncommitted changes.

## 1. The layered I/O path (host/user verb → FAT → disk)

```
user code (user/runtime.c)            DPortFile (variant/userfiles.cpp)
  file_open/open-for-write/getcwd   → channel Request fn 0/2/5/6
  file_read/file_append/file_commit→ fn 1/3/4            │
  change_dir/get_cwd                → fn 5/6             │
shell built-ins (kern_shell.cpp)                          │
  TYPE/COPY/REN/DEL/MD/RD/FAT tools → FatRead/Write/Rename/Del/Mkdir/Cd (fat32.cpp)
                                                          ▼
  path resolution: ProcExtResolve (userfiles.cpp:101 — per-process drive+CWD)
  volume switching: FatUse(drive)…restore (userfiles.cpp:257–261, 337–342, 364–377)
  FAT core ops (fat32.cpp, guarded by UserFilesLock)      ▼
  sector I/O: AtaRead/AtaWrite on sDev  (one 512-byte sector per call, sync)
```

Key files and their single-responsibility lines today:

| Concern | Owner | Notes |
| --- | --- | --- |
| device/partition discovery | `fat32.cpp:544` | `FatParts` |
| mount + superpage validation | `fat32.cpp:577,647` | per-volume state in `sVols[]` |
| global state switching | `fat32.cpp:702` + `LoadCur` 141–144 | `FatUse` swaps volume globals **on every I/O** |
| cluster math | `ClusLba` fat32.cpp:146 | pure arithmetic, LBA-correct |
| FAT entry maintenance | `FatGet/FatSet` 151–183 | FatSet rewrites **every FAT copy per call** |
| allocation/free | `AllocClus/FreeChain` 201–244 | linear scan; allocates+zeroes full cluster; FSInfo best-effort |
| directory walk/mutation | `DirFind/DirAllocSlot/DirWriteAt` ~313–470 | LFN read + 8.3 write |
| file ops | `FatRead 1043, FatWrite 1084, FatRename 1173, FatDel 1219, FatMkdir 1264, FatCd 1316` | whole-file granularity |
| per-process dirs | `userfiles.cpp:9–101` | `sProcs[16]`, validated chdir/getcwd |
| PortFile channel | `userfiles.cpp:188–415` | the 64 KiB snapshot transport |
| timestamps/entries | `StampNow 475, Make83Ent 511` | write-time only, no metadata verbs |

## 2. The current data path in detail (snapshot model)

### Open-for-read (fn 0, `userfiles.cpp:238–280`)
- Resolve path (≤128 chars, overflow rejected **before** any I/O: line 242
  *"never open a truncated path"*), then **read the entire file** into a
  fresh 64 KiB kernel buffer (`Kern::Alloc(65536)`, 253). `FatRead` refuses
  `KErrTooBig` when `f.size > aMax` (fat32.cpp:1057–1058) — so a >64 KiB
  file is unopenable for reads, not read-truncated. Good failure semantics.
- The channel then serves reads **from RAM** at `iPosition`, ≤256 bytes per
  request (`file_read` caps at 256, `runtime.c:205–210`; short reads allowed
  at EOF). "Seek-ish" behavior exists only as implicit position advance;
  there is **no seek verb**; `iPosition` support covers sequential reads but
  cannot rewind or jump.

### Open-for-write / append / commit (fn 2/3/4, `userfiles.cpp:293–345`)
- Write channels stage **all bytes in a 64 KiB RAM buffer**; commit
  (fn 4) under `UserFilesLock` calls `FatWritePath(iPath, iBytes,
  iLength)` — a **whole-file replacement**, not a delta.
- `FatWrite` (fat32.cpp:1084–1169) is transactional in a specific, deliberate
  order at fat32.cpp:1100–1102 and 1150–1168:
  1. allocate + fully write the **new** cluster chain to data sectors
     (freeing it on any ATA error — 1119, 1145);
  2. write the **directory entry** (`DirWriteAt`, 1161) — failure frees the
     new chain, old file untouched;
  3. only after that, free the **old** chain (1167).
  This ordering yields the "disk-full must not erase the old file"
  property that `test_cli_diskfull.py` pins at the FAT-bit level and
  `test_cli_diskfull` checks end-to-end (`COPY /Y` failing leaves the old
  content readable; fresh destination not half-created).
- 8.3 names only on the write side (`Prep83` gate at 1088–1090; long-name
  *reads/exec* work, documented in `manual_test_order.md` Boundaries).

### Concurrency model
- **One global mutex** (`UserFilesLock`, userfiles.cpp:184–186) serializes
  **all** FAT/ATA access — no parallel I/O by design, no per-file locks; two
  writers commit sequentially and the later commit wins wholesale; readers
  always see a consistent chain under the lock. Sharing modes do not exist;
  R1-FS introduces them.

### Path/namespace caps
- path ≤127 chars (resolve buffer 128), LFN component ≤255 (`fat32.cpp:1410`),
  8.3 for new writes, CWD stack depth 8 (`sCwdStack[8]`, fat32.cpp:52).
- Whole-disk FORMAT exists (`FatFormat 1507`); there is **no fsck/repair
  tool at all** — recovery story today is disposable images (`mkdisk.py
  seeded`). Parallel-work's "FAT mutation/crash cases" concern lands here.

## 3. Where the >64 KiB ceiling actually bites

There are **two independent 64 KiB boundaries**, easy to conflate:

1. **Shell exec buffer** (`kern_shell.cpp:513` `static TUint8 eBuf[65536]`) —
   caps *executable image size only*; belongs to the startup path, not to
   data files. R1-FS data does not touch it; R1-LOADER might.
2. **PortFile channel buffers** (`userfiles.cpp:253, 309, 329` — 65536 and
   the `iLength + chunk.Length() > 65536` check at 329) — the *data file*
   ceiling: reads KErrTooBig beyond 64 KiB even after a >64 KiB write would
   exist, appends rejected, and `manual_test_order.md` step 5/CP documents
   "sources over that limit fail at open".

Consequence chain for R1-FAT design: any general offset read/write/seek +
>64 KiB support must replace the snapshot model in DPortFile with
**streaming access at the FAT layer**, which is exactly what the R1-FAT card
says it will design. The note below derives requirements from today's code.

## 4. Derived requirements for R1-FAT (post-contract-freeze implementation)

Ranked by coupling risk to existing behavior that today's tests pin:

1. **Streaming read/write with explicit offset (seek)** — needs a new FAT-layer
   API (`offset read`, `offset write`, `truncate/extend`) instead of
   FatRead/FatWrite's `TUint8*` whole-file signatures. Dealers: DPortFile
   must re-express fn 1/3/4 on top of it; the shell TYPE (via ExecRec?)
   only needs unchanged behavior — keep the snapshot *results* identical
   where tests observe them.
2. **Cluster-chain bookkeeping stays exact** — `FatSet`'s all-copies
   semantics (170–181) and `kEoc`/mirror logic are pinned by
   `mkfat.py`+DiskFull; a partial-write improvement (write one copy,
   sync both at commit) would change crash behavior and must be gated on a
   recorded crash test.
3. **Extend/final-cluster-shorten support** — today `f.size` is always known
   at Make83Ent time; offset-write requires updating entry size **fresh**
   after each mutation (`DirWriteAt` on the live file), which breaks the
   current "size only changes at commit" invariant — this is the hard
   mid-path change for metadata handling.
4. **Metadata verbs** — currently zero metadata beyond name/size/dir-bit+
   timestamp-from-write (`StampNow 475`); attributes are forced 0x20 (1151),
   dirs 0x10. add: attributes, timestamps, size query; contract must state
   how LFN aliases' attribute byte is preserved (write path keeps
   `ent[11] = old|0x20` at 1159 — beware 0x0F interference).
5. **Error-code fidelity as contract surface** — mapped codes today:
   `KErrNotReady` (unmounted), `KErrArgument` (bad name/8.3, path overflow),
   `KErrNotSupported` (dir-as-file, 8.3+rename-violations), `KErrTooBig`
   (39 read overflow / write by fn 1 >256 / size+append cap), `KErrCorrupt`
   (nonzero size whose chain is empty: 1060–1061, or chain ends early:
   1078–1079), `KErrNoMemory` (disk full), `KErrAlreadyExists` (rename
   collision 1210), `KErrNotFound`. R1-FS sharing/error design hangs off
   these; do not re-purpose any of them silently.
6. **Allocator performance disclosure** — `AllocClus` linear-scans the FAT
   (203–226: one 512-byte read per 128 clusters, both FAT copies touched by
   FatSet, full cluster zeroing included). Combined with **no sector cache**
   (every chain hop = AtaRead per sector + FatGet sector re-read), a
   1 MiB file write costs ~2k sector ops. Fine on QEMU; R1-FAT may decide a
   cache is *optional* but should record this cost instead of "fixing" it
   invisibly.
7. **Rename/copy semantics tied to same-dir 8.3** — `FatRename` refuses cross-
   dir renames (shell prints "ren: not supported"), refuses entries adjacent
   to LFN records (1171–1207) because *"changing their short alias alone
   would leave the associated LFN records pointing to an inconsistent
   checksum"* — a contract point R1-FS inherits verbatim or must change
   knowingly (LFN-aware rename would need all LFN records rewritten).

## 5. Interrupted-write / crash case inventory (for the dedicated regression)

Headers today, ranked by user-visible damage, no disk corruption reports:

| Window (order per `FatWrite`) | State left on disk | Detectable? |
| --- | --- | --- |
| During new-chain writes (1113–1149) before dir entry | orphan chain (unreferenced, EOC'd) | ✗ no fsck → lost clusters |
| After dir entry write (1161), before old-chain free (1167) | orphan old chain | ✗ same |
| Mid `DirWriteAt` sector write | half-written 32B entry (CRC-less FAT has no guard) | only if name bytes were clobbered → `DirFind` mismatch/corrupt |
| Mid `FreeChain`/`FatSet` mirror pass | one FAT copy updated, other stale | yes, by comparing FAT1 vs FAT2 — trivially detectable |
| `FatCdPath` global CWD stack mutation | harmless next-mount state | no |
| exec reads torn image (size field intact, content torn) | image data ≠ CRC (E32Validate rejects before ProcessCreate) | yes, at exec time — but FAT *size* > actual data remains undetected |
| `FatCdPath` global CWD stack mutation | harmless next-mount state | no |

Actionable recovery steps per card R1-FAT's *"interrupted-write risks and
disposable-image recovery"*: (a) never store "torn image can be re-repaired"
claims — no fsck exists; (b) write regression tests using a disposable QEMU
image where the host tool forces FAT1/FAT2 divergence (easy: bit-flip both
copies like `test_cli_diskfull.py` does) and where a partial CLUSTER exists
without dir entry, asserting the current honest behaviors (read KErrCorrupt
case at fat32.cpp:1060–1079); (c) document the two permanent-orphan windows
above as accepted costs until a validation pass lands.

## 6. Handoff

```text
Card / base commit / owned branch: R1-FAT-PATHS / fa62323 + dirty tree /
  research-only
Contract revision and dependencies: none frozen; feeds R1-FAT design
  (which itself waits on R1-ABI contract freeze)
What researched (source evidence): variant/fat32.cpp:41-244,300-470,475-530,
  820-1240,1264-1520; variant/userfiles.cpp:9-438; shell exec path in
  startup_paths_R1.md; tools/mkfat.py+mkdisk.py (layout/fixtures);
  fixes pinned by tools/test_cli_diskfull.py
Tests run: none (research card)
Manual step drafted/updated: none
Compatibility and failure limits: §2–§5 lists with line citations; two
  64 KiB ceilings identified (exec eBuf vs PortFile channels)
Blocked assumptions: LFN-adjacent-rename refusal and all-copies FatSet
  semantics are current invariants — changing either needs a recorded
  regression + integrator agreement (R1-FAT owns variant/fat32.cpp
  exclusively once its slot is opened)
Recommended integration order and next owner: deliver §4 requirement list
  to the R1-FAT owner when the R1-ABI contract freezes; R1-FS consumes §4
  error-code map + §2 sharing model; nobody else edits userfiles.cpp
  messages or fat32.cpp semantics in parallel
```
