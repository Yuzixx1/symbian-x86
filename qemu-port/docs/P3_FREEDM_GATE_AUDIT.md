# P3 FreeDM Gate Audit (documentation only)

Read-only audit of `tools/r1_freedm_test.py`, `mkgrubiso.py`, `r1_final_verify.py`; no builds, no QEMU, no commits.

## 1. Missing asset yields SKIP, exit 0
`check_assets()` returns `None` when `DOOM.E32` or `freedm.wad` is absent; `main()` prints `RESULT: SKIP` and `return 0`. Verified by execution with no freedm.wad present: `[SKIP] ... RESULT: SKIP`, `EXIT=0`.

## 2. No downloads, user-supplied drop-off paths only
Only candidate paths are probed (`/tmp/opencode/doom/freedm.wad`, `.../freedoom-0.13.0/`, `wads/`, `build/wad/`). The lone non-local step, `recover_freedm_from_iso()`, reads the local `build/symbian-grub.iso` via pycdlib + mcopy. No urllib/requests/wget/curl; the only `socket` use is QMP `AF_UNIX`. Failure warns, never fatal.

## 3. mkgrubiso.py diff is append-only
`git diff --numstat`: 69 insertions, 0 deletions — only `find_wad2()`, `mdir_free_bytes()` and call sites. `DISK_MB, FS_MB, PART_LBA = 64, 48, 2048` is untouched; a short-space FAT32 volume raises instead of growing `FS_MB`.

## 4. DEFAULT_TESTS is 15 rows, no freedm gate
Live authority is `/tmp/opencode/final27_gate.log`: 15 rows, ending `OVERALL: PASS (15 pass, 0 skip, 0 fail/not-run)` with `FINAL27_RC=0`. `r1_final_verify.py --list` prints 15 tests (`g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`, `qemu_smoke`, five `genuine_*`, four `doom_*`, `hal_probe`); `grep -ci freedm` = 0.

FreeDM remains unwired: the asset is user-supplied and never downloaded (sections 1-2), so a freedm gate is deliberately absent from `DEFAULT_TESTS` rather than present-and-skipping. The earlier `/tmp/opencode/p3_freedm_verify/strict.log` (`OVERALL: PASS (11 pass)`) came from a superseded 11-row list.
