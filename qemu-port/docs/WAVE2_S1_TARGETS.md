# WAVE2 S1 deletion targets — plan only, nothing deleted (HEAD `ee8490e`, 2026-11-25)

S1 deletes **exactly one** path (verified `ls -l fs/` + `sha256sum`) — not the 7 LIVE keepers:

| # | Target | sha256 | Size | Why orphan |
|---|---|---|---|---|
| 1 | `fs/r1_fs_core.mmp` | `deb2d548f85a80a39156b18b0cf79bedf84d4d2465d94bd81db5d29757f53ffc` | 962 B | "proposed MMP, not built" (`tests/r1_fs_core/README.md:50`); zero `Makefile`/`toolchain.mk`/`*.mk` build input |

NOT targets — LIVE, `genuine/efsrv_min/efsrv_min.cpp:30,32` includes + `build_server.sh:16` links; G12 lists them under ALLOWED/LIVE_KEEP, never FAIL (sha256[:8]/size): `fs/r1_fs_core.cpp` 314c3e41/16664, `r1_fs_core.h` de42dde3/3898, `r1_fs_server.cpp` fe6f9ae3/9694, `r1_fs_server.h` 3503e0a7/949, `r1_fs_backend.h` c208e0e1/3224, `r1_fs_fat.cpp` 586bc3c7/9411, `r1_fs_fat.h` eae36628/1849.

Gates, run before the delete and again after (FAIL/SKIP/124 = NO-GO, restore from snapshot): **G12** `python3 tools/r1_shim_residue_sweep.py` (static, ~1.4 s, exit 1 on residue) and **S** `python3 tools/r1_final_verify.py --strict-skip` → 11/11; re-run **R** `python3 tools/r1_genuine_retire_test.py` 4/4 and **F** `python3 tools/r1_genuine_fullchain_test.py` 14/14 after S1 (S1 touches no build/link path).

Snapshot `/tmp/opencode/wave2_shim_backup_<YYYYmmdd_HHMMSS>/` mirroring the path + sha256 manifest; rollback token `ROLLBACK-WAVE2-<stamp>`; fallback `git show ee8490e:fs/r1_fs_core.mmp`. Reuse `tools/r1_winsrv_delete.py` guards — refuses commit/push/`make clean`/`rm -rf`; never bare `rm`.

Expected G12 after S1: **17 residue** (18 → 17), not 0 — only the mmp finding drops; 9 path findings remain (`fs/r1_fs_channel{,_eka}.{cpp,h}`, `r1_fs_channel_state.h`, `r1_fs_wire_logic.h`, `variant/r1_fs_channel.{cpp,h}`, `tests/r1_fs_channel`) plus 8 `Makefile:342-351` rules, which S2/S3 clear.

Frozen/untouched: `kernelhwsrv/` read-only, `efsrv_min` probe host, `tests/r1_fs_core`; no commit, no push, no QEMU.

2026-11-26 supersession note: the 11-row / 10-11-pass gate described above is historical. Current authority is 15 rows / FINAL27_RC=0 (15 pass, 0 skip, 0 fail, /tmp/opencode/final27_gate.log).
