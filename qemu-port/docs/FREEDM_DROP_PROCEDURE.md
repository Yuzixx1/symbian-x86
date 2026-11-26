# FREEDM_DROP_PROCEDURE — supplying freedm.wad by hand (no build download)

FreeDM is BSD-3-Clause but never fetched: the build has no network path, so per
`docs/DOOM_PORTING.md` §4 every WAD stays user-supplied and uncommitted.

1. Drop the WAD, plus its `COPYING.txt`, uncommitted, at the first accepted path
   `qemu-port/wads/freedm.wad` (also `FREEDM.WAD` in `wads/` or `build/wad/`,
   `/tmp/opencode/doom/{freedoom-0.13.0/,}freedm.wad` — `tools/r1_freedm_test.py:58`).
2. Pin the bytes: `stat -c %s wads/freedm.wad`, then set that number as
   `FREEDM_SIZE` (`tools/r1_freedm_test.py:92`). While unpinned the gate only warns
   and uses the drop-off size; a Freedoom IWAD (28795076 B) must never satisfy it.
3. Stage on `C:`: `make isogrub` (never `make clean`), then `mdir -i
   build/programs.img@@1048576 ::` must list `FREEDM.WAD`; the gate stages the same
   bytes under the 8.3 alias `FDOOM1.WAD`, so `DOOM.E32` boots FreeDM MAP01 as is.
4. Run the gate: `python3 tools/r1_freedm_test.py` (`--disk-boot` for AHCI `C:`).
   Missing asset → `RESULT: SKIP`, exit 0; present → `RESULT: PASS` on the harness's
   `FREEDM:` family incl. the byte pin, evidence `/tmp/opencode/freedm_final.png`.
