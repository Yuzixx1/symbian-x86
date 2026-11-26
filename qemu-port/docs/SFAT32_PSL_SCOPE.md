# SFAT32_PSL_SCOPE — scope of the 7 live `r1_fs_*` keepers + `variant/fat32.cpp` against genuine `sfile`/`sfsrv`/`sfat32`

Date 2026-11-25. Read-only scope definition. `kernelhwsrv/` READ-ONLY and untouched;
no commit, no push, no `make clean`, no QEMU, no download, no source edit. Every
claim below is a line anchor in the current tree, re-checked by `grep -n` / `cat -n`
at the time of writing. Where a document and the tree disagree, the tree wins.

Purpose: state exactly which `qemu-port` files are the *live* filesystem keepers,
which genuine `kernelhwsrv` sources they stand in for, and the single gate that
decides when those keepers may be retired. The gate is **NO-GO while they are live
in EFSRVMIN** — see [§5](#5-retirement-gate).

## 1. The eight files in scope

Seven `fs/` keepers plus one variant FAT engine. Line counts are from `wc -l`.

| # | File | Lines | Role |
|---|---|---|---|
| 1 | `fs/r1_fs_backend.h` | 60 | Injectable backend contract + Symbian status codes + geometry constants |
| 2 | `fs/r1_fs_core.h` | 72 | `Core` class, `TFsMessage` opcode mirror, `Frame`/`Decoded` wire shapes |
| 3 | `fs/r1_fs_core.cpp` | 346 | Name canonicalisation, share arbitration, handle/position/size state machine |
| 4 | `fs/r1_fs_server.h` | 24 | `FileServer : CServer2` declaration, `EPriorityNormal` disambiguation |
| 5 | `fs/r1_fs_server.cpp` | 201 | `FileSession::ServiceL` — IPC opcode dispatch, chunked read/write |
| 6 | `fs/r1_fs_fat.h` | 60 | `FatBackend : Backend` declaration |
| 7 | `fs/r1_fs_fat.cpp` | 307 | Whole-file RAM-buffered `FatBackend` over `FatRead/Write/Mount` |
| 8 | `variant/fat32.cpp` | 2022 | Freestanding FAT32 mount/read/write engine, 8.3 names only |

`variant/fat32.cpp` is counted with the keepers because `fs/r1_fs_fat.cpp` has no
other engine: `fs/r1_fs_fat.cpp:9` states "FatReadPath/FatWritePath/FatMount are
defined in variant/fat32.cpp" and `:12-14` declares all three `extern`.

## 2. Genuine-side counterpart inventory

`kernelhwsrv/userlibandfileserver/fileserver/` holds the three genuine trees the
keepers stand in for. Directory sizes from `ls | wc -l`.

- `sfile/` — 67 entries. The file engine: `sf_file.cpp`, `sf_ops.cpp`, `sf_ses.cpp`,
  `sf_svr.cpp`, `sf_mnt.cpp`, `sf_drv.cpp`, `sf_dir.cpp`, `sf_cache*.cpp`,
  `sf_plugin*.cpp`, `sf_decomp.cpp`, `sf_inflate.cpp`, `sf_ldr.cpp`, plus the
  `efile.mmp`/`efile.mmh` build pair.
- `sfsrv/` — 27 entries. The file-server session layer: `cl_file.cpp`, `cl_dir.cpp`,
  `cl_entry.cpp`, `cl_drive.cpp`, `cl_clamp.cpp`, `cl_blockmap.cpp`, plus
  `efsrv.mmp`/`efsrv.mmh`.
- `sfat32/` — 42 entries. The FAT32 FSY: `sl_mnt.cpp`, `sl_mnt32.cpp`,
  `fat_dir_entry.{h,inl}`, `fat_table32.cpp`, `fat_config.{cpp,h,inl}`,
  `base_f32_sfat32.mrp`, `bld.inf`, `efat32.mmp`.

The opcode numbering that both sides must agree on lives in
`fileserver/inc/F32plugin.h:42` (`enum TFsMessage`).

## 3. Opcode evidence — the keepers mirror the genuine enum exactly

`fs/r1_fs_core.h:9-12` declares the eleven opcodes the port serves. Each was
checked against the genuine enum; the two columns are identical.

| Port constant | `r1_fs_core.h` | Genuine enum member | `F32plugin.h` | Value |
|---|---|---|---|---|
| `SessionPath` | `:9` | `EFsSessionPath` | `:58` | 14 |
| `SetSessionPath` | `:9` | `EFsSetSessionPath` | `:59` | 15 |
| `FileClose` | `:9` | `EFsFileSubClose` | `:72` | 28 |
| `FileOpen` | `:10` | `EFsFileOpen` | `:74` | 30 |
| `FileCreate` | `:10` | `EFsFileCreate` | `:75` | 31 |
| `FileReplace` | `:10` | `EFsFileReplace` | `:76` | 32 |
| `FileRead` | `:10` | `EFsFileRead` | `:78` | 34 |
| `FileWrite` | `:10` | `EFsFileWrite` | `:79` | 35 |
| `FileSeek` | `:11` | `EFsFileSeek` | `:82` | 38 |
| `FileFlush` | `:11` | `EFsFileFlush` | `:83` | 39 |
| `FileSize` | `:11` | `EFsFileSize` | `:84` | 40 |
| `FileSetSize` | `:11` | `EFsFileSetSize` | `:85` | 41 |
| `FileReadCancel` | `:12` | `EFsFileReadCancel` | `:164` | 120 |

The enumeration is contiguous from 0 at `F32plugin.h:44` (`EFsAddFileSystem`), so
the value column is positional, not asserted. `EFsFileReadCancel` is explicitly
annotated `///< -- 120` at `:164`, matching `r1_fs_core.h:12` literally.

`fs/r1_fs_backend.h:13-16` uses the same approach for error codes: numeric Symbian
values (`Success=0, NotFound=-1, ..., TooBig=-40`) "kept numeric so host tests do
not need the SDK; the values are the genuine system codes."

## 4. Which keeper corresponds to which genuine responsibility

### 4.1 `r1_fs_core.{h,cpp}` — portable `sfile`/`sfsrv` semantics

| Keeper anchor | Behaviour | Genuine counterpart |
|---|---|---|
| `r1_fs_core.h:21-70` | `Core` owns sessions, shares, handles | `sfsrv/cl_file.cpp`, `cf_file` handling in `sfile/sf_file.cpp` |
| `r1_fs_core.h:9-12` | Opcode set | `inc/F32plugin.h:42` |
| `r1_fs_core.h:14-15` | `Frame` — "no raw IPC pointer" | `sfile/sf_request.cpp` message decoding |
| `r1_fs_core.cpp:50-87` | `Canonical` — drive-letter + component normalisation, `AccessDenied` on failure (`:87`) | Path parsing in `sfsrv/cl_*`; `sfile/sf_utl.cpp` |
| `r1_fs_core.cpp:12-26` | `Allowed` — restricts to `C:\R1TEST` fixture namespace | Narrower than genuine, deliberate (see §4.5) |
| `r1_fs_core.cpp:34-48` | `AllowedWithShares` — exclusive/readers/any arbitration | `RFile::Share` semantics in `sfsrv/cl_file.cpp` |
| `r1_fs_core.cpp:193-229` | `Core::Open`, share check `:216`, `KFileMax`/`ModeBig` guard `:223` | `EFsFileOpen/Create/Replace` handling |
| `r1_fs_core.cpp:231-245` | `Commit` / `CheckShare` / `Abort` / `Close` — publish-after-reply | File-object lifecycle in `sfile/sf_file.cpp` |
| `r1_fs_core.cpp:281-303` | `StageRead` / `ConfirmRead` — position advances only after copy | Asynchronous `EFsFileRead` completion path |
| `r1_fs_core.cpp:305-323` | `Write`, poisoned-share handling | `EFsFileWrite` |
| `r1_fs_core.cpp:325-339` | `Seek` — `mode==1` absolute, else relative with `KFileMax` clamp | `EFsFileSeek` |
| `r1_fs_core.cpp:340-346` | `UpdateSize` — size published only when `stable` | Cached-size update in `sfile/sf_cache*.cpp` |

### 4.2 `r1_fs_server.{h,cpp}` — `CServer2`/`CSession2` host

- `r1_fs_server.h:13` — `class FileServer : public CServer2`. The genuine server
  base, not a reimplementation. `CServer2(EPriorityNormal, EGlobalSharableSessions)`
  is constructed at `r1_fs_server.cpp:30`.
- `r1_fs_server.h:6-9` — `EPriorityNormal` is re-declared namespace-local because the
  genuine value at `kernel/eka/include/e32const.h:1851` (`EPriorityNormal=0`) clashes
  with the freestanding shim's global set. Verified: `e32const.h:1849-1852` is
  `EPriorityMuchLess=(-20), EPriorityLess=(-10), EPriorityNormal=0, EPriorityMore=10`.
- `r1_fs_server.h:10-12` — explicit integration contract: construct at B1, the
  integration owner alone calls `UserSvr::FsRegisterThread` at B2 and `Start` at B3;
  no `E32Main` here because the source links into the shared `portfilesrv.exe` host.
- `r1_fs_server.cpp:32` — `Publish()` calls `Start(_LIT(KName,"!FileServer"))`, the
  genuine server name.
- `r1_fs_server.cpp:33-39` — `NewSessionL` returns a `FileSession`.
- `r1_fs_server.cpp:41-200` — `ServiceL` dispatch: validation mask `:49-54`, panic on
  `BadHandle` `:77`, overflow panic `:115`, negative-size panic `:139`, 4 KiB chunk
  loop `:159-196` bounded by `KTransferBytes`, 1 MiB request ceiling `:161`,
  staged-read copy-then-confirm `:182-196` (mirrors the genuine Symbian rule that a
  short backend fill must not be reported as a full `RFile::Write`, `:159-160`).

### 4.3 `r1_fs_backend.h` — the seam that makes the keepers testable

`Backend` (`:28-58`) is the only abstraction: `Lookup :33`, `Open :37`,
`ReadAt :42`, `WriteAt :46`, `Resize :51`, `Flush :55`, `Close :57`. Two invariants
are stated in-line and are scope, not decoration:

- `:4-7` — "The channel implementation MUST authenticate the pinned `portfilesrv`
  process on open, handle transfer and every request; this interface is not an
  authorization bypass and must never accept `sys\bin` mutations."
- `:24-27` — `IoResult.stable`: size is authoritative only when `stable=true`;
  otherwise the caller fails the share closed.

Because the contract is virtual, `tests/r1_fs_core/core_test.cpp` (223 lines) can
drive `Core` without the SDK or a running server. That test is the reason the seven
keepers are cheap to hold live.

### 4.4 `r1_fs_fat.{h,cpp}` + `variant/fat32.cpp` — the FAT32 block layer

| Keeper anchor | Behaviour |
|---|---|
| `r1_fs_fat.h:13` | `class FatBackend : public Backend` |
| `r1_fs_fat.h:9-12` | Explicit scope: "Suitable for R1 checkpoint 2: small-to-medium files on a disposable FAT32 volume" |
| `r1_fs_fat.h:40` | `uint8_t* buffer; // whole-file image in RAM` |
| `r1_fs_fat.h:45` | `KMaxOpenFiles = 16` |
| `r1_fs_fat.cpp:76-119` | `PathToLeaf` — canonical path to a 13-byte 8.3 leaf (`char leaf[13]` at `r1_fs_fat.h:54`) |
| `r1_fs_fat.cpp:121-125` | `Mount` → `FatMount((int)dev,(int)part)` at `:123` |
| `r1_fs_fat.cpp:182` | `LoadWholeFile` → `FatReadPath(pathBuf, tmpBuf, ...)` — whole file into RAM |
| `r1_fs_fat.cpp:287` | write-back → `FatWritePath(pathBuf, s->buffer, s->size)` |
| `variant/fat32.cpp:672` | `TInt FatMount(int aDev, int aPart)` |
| `variant/fat32.cpp:602` | `FatMountInner` — boot-sector read/validate |
| `variant/fat32.cpp:1526` | `TInt FatReadPath(const char*, TUint8*, TUint, TUint&)` |
| `variant/fat32.cpp:1533` | `TInt FatWritePath(const char*, const TUint8*, TUint)` |
| `variant/fat32.cpp:536-548` | `Make83Ent` — writes 11-byte 8.3 directory entries only |
| `variant/fat32.cpp:351` | `LfnToAscii` — reads LFN but flattens to ASCII; long names are not created |

The genuine FAT32 side is `sfat32/sl_mnt.cpp:79` (`CFatMountCB::CFatMountCB`),
`:84` (`iState = ENotMounted`), `sl_mnt32.cpp:37` (`DoWriteBootSector`),
`:72` (`DoReadBootSector`), `:143` (`ReadBootSector`), with directory-entry layout
in `fat_dir_entry.h`/`.inl` and the table in `fat_table32.cpp`.

**This is where `variant/fat32.cpp` is not equivalent, and the scope must say so.**
It is a mount/read/write engine, not a PSL replacement: it has no `CFatMountCB`
state machine, no FAT32/16/12 FSY plugin dispatch, no long-filename creation, no
cluster-chain repair beyond what `FatMountInner` validates, and no `TFSFormat`
support. Retiring it therefore requires the genuine `sfat32` FSY to be in the link
set (see §5 G3), not merely a rename.

### 4.5 Deliberate narrowings — scope limits, not bugs

Three restrictions are intentional and must not be read as parity defects:

1. `r1_fs_core.cpp:15-16` — the only writable namespace is `C:\R1TEST`.
   `r1_fs_backend.h:31` repeats the restriction ("no `sys\bin` write").
2. `r1_fs_fat.h:40` — whole-file-in-RAM. Memory, not correctness, bounds file size.
3. `variant/fat32.cpp:536` — 8.3 names only on create.

## 5. Retirement gate

**Gate state: NO-GO.** It stays NO-GO for exactly as long as any of the eight files
is a live input to EFSRVMIN. Live means compiled or linked, not merely present.

The liveness fact that holds the gate shut is
`genuine/efsrv_min/build_server.sh`, which compiles both C++ keepers and links them
into the shipped image:

```
build_server.sh:16   gcc $CXXFLAGS -c fs/r1_fs_core.cpp   -o "$D/r1_fs_core.o"
build_server.sh:17   gcc $CXXFLAGS -c fs/r1_fs_server.cpp -o "$D/r1_fs_server.o"
build_server.sh:19-22 ld ... "$D/r1_fs_core.o" "$D/r1_fs_server.o" "$LIBGCC"
```

The comment at `build_server.sh:13-15` states why: the genuine user-mode FS engine
(R1Fs::Core + CServer2 FileServer) "has no `user/user.mk` rule since the svcmgr
retirement; build it here, as the sources under `fs/` cannot match the
`build/user/%.o` pattern (which only reaches `user/`)."

The second, independent witness is the link-set constant in
`tools/r1_missing_statics_check.py:135-143`, whose docstring `:133-134` names the
image "the image the plan says must never grow these symbols
(`genuine/efsrv_min/build_server.sh:18-24`)":

```
EFSRVMIN_LINK_OBJECTS = (
    "build/user/ipc.o", "build/user/malloc.o", "build/user/runtime.o",
    "build/user/start.o", "genuine/efsrv_min/efsrv_min.o",
    "genuine/efsrv_min/r1_fs_core.o",      # :141
    "genuine/efsrv_min/r1_fs_server.o",    # :142
)
```

`Makefile:318-320` records the current arrangement: kernel-side `r1_fs` glue was
removed, and "Genuine EFSRV uses the user-mode `r1_fs` copy + `build/fat32.o`."
So the keepers are no longer in the kernel — they moved whole into the user-mode
server. That relocation is why this gate, not a `KERNFULL_OBJS` check, is the
correct one.

### Gate conditions

All six must hold, from one clean log, before the state may flip to GO. Any single
unmet condition keeps it NO-GO.

| ID | Condition | Evidence to cite |
|---|---|---|
| G1 | `build_server.sh` no longer compiles either keeper and no longer links `r1_fs_core.o`/`r1_fs_server.o` | `build_server.sh:16-17`, `:19-22` |
| G2 | `EFSRVMIN_LINK_OBJECTS` drops the two `r1_fs_*.o` rows | `r1_missing_statics_check.py:141-142` |
| G3 | The replacement genuinely serving `SessionPath`…`FileReadCancel` is in the link set: `sfile` (`sf_file.cpp`, `sf_ops.cpp`, `sf_svr.cpp`) + `sfsrv` (`cl_file.cpp`) + `sfat32` (`sl_mnt.cpp`, `sl_mnt32.cpp`, `fat_table32.cpp`) | `inc/F32plugin.h:42`; `sfat32/sl_mnt.cpp:79` |
| G4 | `tests/r1_fs_core/core_test.cpp` retired, or repointed at the genuine `Backend` implementation | `tests/r1_fs_core/` |
| G5 | Forwarded references updated: `user/e32cmn.h:13` and `:40` both name `fs/r1_fs_server.cpp` by path | `user/e32cmn.h:13,40` |
| G6 | EFSRV PASS established from one clean log under the anti-false-pass rules | `docs/EFSRV_GATE_FLIP_CHECKLIST.md` |

G6 depends on G1–G3 for meaning: a PASS produced while the keepers are still the
serving engine says nothing about the genuine path. That dependency is the whole
reason the gate is NO-GO rather than "PASS pending cleanup."

### What the gate does not block

Building, testing, or running the server as-is is in scope and needs no gate
consultation. Only removal of a file from the link set is gated. Do not declare GO
on the strength of a green run, a rename, or a comment change; G1 and G2 are
mechanical and both must be observed absent, not merely marked.

## 6. Cross-references

- `docs/R1FS_GLUE_FEASIBILITY.md` — why the kernel-side glue was removed.
- `docs/R1FS_REMOVAL_STATUS.md` — removal snapshot; `Makefile:318-320` origin.
- `docs/EFSRV_GATE_FLIP_CHECKLIST.md` — the G6 precondition list.
- `docs/EFSRVMIN_BUILD_FIX.md` — how `build_server.sh` reached its current shape.
- `docs/EFSRV_UNBLOCK_CHECK.md` — remaining blockers on the genuine path.
- `docs/R1FS_REMOVAL_REVIEW.md` — review record for the same transition.
