# R1FS_GLUE_FEASIBILITY — are kernel-linked r1_fs objects load-bearing? (2026-11-24)

**Scope:** read-only analysis. No source/build edits, no QEMU, no commit/push.
`kernelhwsrv/` untouched. Question: if `Makefile:326` drops
`build/r1_fs_core.o build/r1_fs_fat.o` from `KERNFULL_OBJS`, what breaks —
specifically, does the *genuine* EFILE/EFSRV path depend on them, or only the
retired portfilesrv shim?

## Verdict

**SAFE TO REMOVE from `kernfull.elf`, but only as a coupled edit.** The
kernel-linked `r1_fs` objects are **not** used by the genuine EFILE/EFSRV
runtime path and not by `PortFileServerPublished()`. Their sole in-kernel
consumer is the `Asic::Init3` mount block in `variant/qemu_variant_asic.cpp`,
whose `R1Fs::Core`/`R1FsSession` exports have **zero callers**. One indirect
boot side effect must be preserved: that block is currently the **only**
`FatMount(4, 0)` call in the boot path, i.e. it mounts `C:`. Removal must
replace it with a direct `FatMount(4, 0)`.

## 1. Dependency map (symbol → users)

Symbols provided by `build/r1_fs_core.o` (`fs/r1_fs_core.cpp`, class
`R1Fs::Core`, `fs/r1_fs_core.h:21-70`) and `build/r1_fs_fat.o`
(`fs/r1_fs_fat.cpp`, class `R1Fs::FatBackend`, `fs/r1_fs_fat.h:13-56`),
confirmed present in `build/kernfull.elf` via `nm -C`:

| Symbol group | Defined in | In-kernel user (undefined refs) |
|---|---|---|
| `R1Fs::Core::*` (Connect/Open/Read/Write/Close/…) | `r1_fs_core.o` | **only** `build/qemu_variant_asic.o` — sweep of *all* `build/*.o` with `nm -uC` shows no other object references any `R1Fs` symbol |
| `R1Fs::FatBackend::*` (Mount/Lookup/Open/ReadAt/…) | `r1_fs_fat.o` | **only** `build/qemu_variant_asic.o` |
| `R1FsCore()`, `R1FsSession()` (exported C wrappers, `qemu_variant_asic.cpp:24-25`, declared `variant/qemu_variant.h:13-14`) | `qemu_variant_asic.o` | **no callers anywhere in the tree** (grep over variant/, user/, genuine/, tools/, tests/) |

Exact use site — `variant/qemu_variant_asic.cpp:148-164` (`QemuVariantAsic::Init3`):

- `:151` `gFatBackend = new R1Fs::FatBackend();`
- `:153` `gFatBackend->Mount(4, 0);` — dev 4, part 0
- `:155` `gCore = new R1Fs::Core(*gFatBackend);`
- `:158` `gCore->Connect(0x100039e5, gSession);` — session to the SID portfilesrv used
- globals at `:19-21`; exports at `:24-25`

## 2. Answers

**(a) Which kernfull symbols come from the r1_fs objects?**
`R1Fs::Core::{Core,Connect,Disconnect,GetPath,SetPath,Open,Commit,CheckShare,Abort,Close,Read,StageRead,ConfirmRead,Write,Seek,Size,Resize,Flush,Writable,Readable,SetDrive,Find,ValidateSession,CloseShare,UpdateSize}`
and `R1Fs::FatBackend::{FatBackend,~FatBackend,Lookup,Open,ReadAt,WriteAt,Resize,Flush,Close,Mount,Find,PathToLeaf,LoadWholeFile}`
(plus `R1Fs::Decode/Canonical` from core). All listed by `nm -C build/kernfull.elf`.

**(b) Genuine EFILE/EFSRV path or retired shim only?**
**Neither uses the kernel objects at runtime; the only kernel consumer is the
retired-shim-era Init3 block.** Evidence:
- `PortFileServerPublished()` (`variant/e32load.cpp:74-83`) checks
  `K::Containers[EServer]` for the `!FileServer` name — **no R1Fs reference**;
  its uses at `e32load.cpp:797` and `:1529` are independent of r1_fs.
- Genuine EFILE/EFSRV images are built purely from `kernelhwsrv/` +
  prebuilt `EUSER.DLL` (`tools/genuine_build.py` has no `r1_fs`/`R1Fs`
  reference; `genuine/README.md` neither). Their on-disk FAT cluster I/O goes
  through the **user-mode** bridge: `genuine/efsrv_min/efsrv_min.cpp:77`
  `DiskFatBackend : R1Fs::Backend` → `file_*()` (`user/runtime.c:298+`,
  PortFile/PortBigFile IPC) → kernel `PortFile`/`R1Fat` layer
  (`variant/fat32.cpp`, i.e. **`build/fat32.o`**, a separate object on
  `Makefile:318` that stays) → `AtaRead`/`AtaWrite`
  (`genuine/F32_EFSRV_REPORT.md:46-52,58-67`). That host links its **own
  user-mode copy** `build/user/r1_fs_core.o` (`genuine/efsrv_min/build_server.sh:16`,
  `user/user.mk:383`) — not the kernel objects from `Makefile:326`.
- The retired shim (`user/portfilesrv.cpp`, `user/svcmgr.cpp`) used the
  *user-mode* `r1_fs_server.o`/`r1_fs_core.o` too; its boot rows are already
  commented out (`e32load.cpp:1552`, `svcmgr.cpp:250`).

**(c) Does `tools/r1_genuine_fat_io_test.py` depend on them in-kernel?**
**No.** It boots `build/kernfull.elf` but all FAT I/O is performed by the
user-mode `EFSRVMIN.E32`/`FATIOCLIENT.E32` staged on the disk
(`tools/r1_genuine_fat_io_test.py:5-21,40-47`); the file contains no
`r1_fs`/`R1Fs`/`FatBackend` reference. The in-kernel `gCore`/`gSession` play
no part.

**(d) What breaks if `Makefile:326` drops them?**
- Link failure only, until the coupled edit: `qemu_variant_asic.o` is the sole
  object with undefined `R1Fs` refs, so removing the objects without touching
  `qemu_variant_asic.cpp` breaks the build, not the boot.
- Boot regression if the Init3 block is deleted naively: `FatBackend::Mount`
  is a thin wrapper over `FatMount` (`fs/r1_fs_fat.cpp:121-123`,
  `extern int FatMount(int,int)` at `:14`), and `FatMount` makes the new
  volume active with the next free letter — first mount = `C:`
  (`variant/fat32.cpp:669-717`). `asic.cpp:153` is the **only** `FatMount(4,0)`
  in the boot path (the `:172-185` block only adds the `X:` alias via
  `FatMountAlias`, `fat32.cpp:720-725`, without stealing the active drive).
  `GenuineBootTry` then does `FatUse('C')` + `FatReadPath`
  (`e32load.cpp:1501-1504`), and `E32LoadDll` reads `\EUSER.DLL` via
  `FatReadPath` (`e32load.cpp:668`) — all rely on `C:` existing. Without a
  replacement `FatMount(4, 0)`, the boot volume is never mounted and the
  genuine chain cannot load.

## 3. Exact removal steps (if taken)

1. `Makefile:326` — delete `KERNFULL_OBJS += build/r1_fs_core.o build/r1_fs_fat.o`;
   delete rules at `Makefile:328-332`. Keep `build/fat32.o` (`Makefile:318`) —
   it is the `R1Fat`/`FatReadPath` layer used by `e32load.cpp`, `kern_shell.cpp`,
   `userfiles.cpp`, `qemu_display.cpp`.
2. `variant/qemu_variant_asic.cpp` — remove includes `:14-15`, globals
   `:19-21`, exports `:24-25`, and replace the block `:148-164` with:
   `if (FatMount(4, 0) == KErrNone) { /* C: mounted */ }` (keep the
   `X:` alias block `:165-185` untouched).
3. `variant/qemu_variant.h:10,13-14` — remove the `R1Fs::Core` forward
   declaration and `R1FsSession()`/`R1FsCore()` declarations.
4. Re-check the opt-in channel: `R1_FS_CHANNEL=0` by default and not linked
   (`Makefile:354-363`); `variant/r1_fs_channel.cpp` uses `R1FsWire`, not
   `R1Fs::Core`, but re-verify if ever enabled.
5. Do **not** touch: `fs/r1_fs_*` sources (still used by user-mode
   `portfilesrv`/`svcmgr`/`efsrv_min` builds and host unit tests
   `tests/r1_fs_core/core_test.cpp`), `user/user.mk` user-mode rules,
   `variant/fat32.cpp`.
6. Verify with `python3 tools/r1_os_selfcheck.py` then
   `python3 tools/r1_final_verify.py --strict-skip` (all 11 gates, incl.
   `fat_io`).

## 4. Risks

- **C: mount regression** (above) — the one real functional dependency; the
  r1_fs glue merely *carries* the `FatMount(4,0)` call, it does not provide it.
- `gCore->Connect(0x100039e5, …)` session is exported but unused; nothing in
  the genuine path consumes `R1FsCore()`/`R1FsSession()` — safe to drop.
- Removal is bypass-consistent with `d26fade` (boot path already bypasses
  portfilesrv), but keep rollback snapshot
  `/tmp/opencode/shim_retire_backup_20261124_083011` until the full gate is
  green after the edit.
- `fs/r1_fs_core.h` enum must keep matching genuine `TFsMessage` opcodes for
  `tools/r1_genuine_retire_test.py:9,127` — that test reads the header, not the
  kernel objects, so file removal of the *objects* is unaffected; do not
  delete the header while the retire gate uses it.
