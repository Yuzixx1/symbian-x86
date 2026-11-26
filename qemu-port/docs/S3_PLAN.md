# S3 guarded delete — orphan R1Fs channel set (design only, nothing deleted)

Scope is the 9 residue paths G12 still reports once S1 (`fs/r1_fs_core.mmp`) and S2 (`Makefile:342-351`, the 8 `[rule]` hits) have landed. Order is load-bearing: skipping S2 leaves residue 8, not 0.
| path (under `qemu-port/`) | bytes | sha256[:12] |
|---|---|---|
| `fs/r1_fs_channel.cpp` | 6518 | `68c27fade26c` |
| `fs/r1_fs_channel.h` | 2959 | `f89aa078aad5` |
| `fs/r1_fs_channel_eka.cpp` | 1860 | `f5a5cb29f14e` |
| `fs/r1_fs_channel_eka.h` | 960 | `e70d3cde7141` |
| `fs/r1_fs_channel_state.h` | 5445 | `1d5588725737` |
| `fs/r1_fs_wire_logic.h` | 2217 | `9c3aaa1973dd` |
| `variant/r1_fs_channel.cpp` | 4182 | `b7618402534d` |
| `variant/r1_fs_channel.h` | 528 | `65849335346f` |
| `tests/r1_fs_channel/` (4 files: README `61c6e461620f`, adapter `34ae6d68d3fc`, state `f4b75dd3e961`, wire `8f3e3ca8b73a`) | 19321 | dir |
**Never touched** — the 7 G12 `LIVE_KEEP` files (`fs/r1_fs_core.cpp|.h`, `fs/r1_fs_server.cpp|.h`, `fs/r1_fs_backend.h`, `fs/r1_fs_fat.cpp|.h`), `tests/r1_fs_core/`, `fs/r1_fs_core.mmp` (S1's), `docs/r1_fs_channel_*.md` + `docs/r1_wire_research_v0.md` (history; G12 skips `docs/`), and `kernelhwsrv/` (READ-ONLY).
**Snapshot before any delete (read-only):** `python3 tools/r1_shim_residue_sweep.py --json` must read `residue 9`, zero `[rule]`, 43 allowed comments (18 today); `git log -1` = `ee8490e`; `git status --short kernelhwsrv/` empty; `build/r1_fs_*.o` absent. Then back up all 12 files to `/tmp/opencode/wave2_s3_backup_<YYYYmmdd_HHMMSS>/` with an `r1-wave2-delete-backup/v1` manifest, sha256 verified per file.
**Token:** a second exact-set frozenset scope (12 paths, no globs) in the wave2 delete tool, any other argv → exit 2 with nothing touched; `--apply --confirm DELETE-WAVE2-S3-d861b6e1`, where `d861b6e1` is sha256[:8] over the 12 sorted `sha256  path` lines.
**Gates, pre and post:** G12 residue **exactly 0**, rc 0 (1 is NO-GO; the exit code alone is not evidence), retire 4/4, fullchain 14/14, `tools/r1_final_verify.py --strict-skip` 11/11. No QEMU, no `make clean`, no commit, no push; `kernelhwsrv/` untouched.
**Rollback:** `--rollback <backup dir> --confirm ROLLBACK-WAVE2-S3-<YYYYmmdd_HHMMSS>`, sha256 checked before and after restore, then re-gate back to 9 — a wrong or partial delete must be indistinguishable from no delete.

## 2026-11-25 — S3 applied (RESULT: PASS)

Ran `tools/r1_wave2_s3.py --apply --confirm DELETE-WAVE2-S3-d861b6e1`; log `/tmp/opencode/s3_apply_clean.log` ends `APPLY_RC=0`, driver exited, no rollback taken. Pre-state matched the design exactly: scope digest `affa56c7` MATCH, 12 files verified, `tests/r1_fs_channel/` held exactly its 4 files, `op targets git state: clean`, `kernelhwsrv/ clean: clean`.

| gate | pre | post |
|---|---|---|
| G12 residue | PASS 9 (expected 9) | PASS **0** (expected 0) |
| R retire | PASS 4/4 (0 skip, 0 fail) | PASS 4/4 (0 skip, 0 fail) |
| F fullchain | PASS 14/14 rows | PASS 14/14 rows |
| S strict | PASS 11/11, 0 skip, 0 fail | PASS 11/11, 0 skip, 0 fail |

**Deleted — 12 paths + 1 `rmdir`, the predicted set and no wider:** `fs/r1_fs_channel.cpp`, `fs/r1_fs_channel.h`, `fs/r1_fs_channel_eka.cpp`, `fs/r1_fs_channel_eka.h`, `fs/r1_fs_channel_state.h`, `fs/r1_fs_wire_logic.h`, `variant/r1_fs_channel.cpp`, `variant/r1_fs_channel.h`, `tests/r1_fs_channel/{README.md,adapter_test.cpp,state_test.cpp,wire_test.cpp}`, then `rmdir tests/r1_fs_channel/`.
**Residue 9 → 0.** Re-run by hand after the driver exited: `RESULT: PASS (0 residue, 43 allowed comments)`, rc 0; all 9 residue paths and the test dir confirmed absent.
**Backup:** `/tmp/opencode/wave2_s3_backup_20261125_233833` (`fs/`, `tests/`, `variant/`, `manifest.json`), rollback token `ROLLBACK-WAVE2-S3-20261125_233833`.
**State:** HEAD `ee8490e` unchanged, index empty, **no commits** — the 12 deletions are uncommitted working-tree changes and committing them is a separate, human-authorized step. `kernelhwsrv/` clean, no QEMU, no `make clean`.
**LIVE_KEEP untouched** — the 7 `LIVE_KEEP` files, `tests/r1_fs_core/`, and `docs/` history all survive; the post R/F/S ladder would have caught collateral damage.

**2026-11-25 correction:** the H1 `(design only, nothing deleted)` parenthetical is historical, written before the apply, and is superseded by the RESULT section above — S3 was applied with `APPLY_RC=0`, deleting 12 files plus 1 directory. Line 1 is left as-is deliberately; read it as the design-time title only.

---

## Supersession note — 2026-11-26

**Any 11-row / 11-pass `r1_final_verify.py --strict-skip` requirement or claim in this document is historical.** The current authority is 15 rows: `FINAL27_RC=0` (15 pass, 0 skip, 0 fail), captured in `/tmp/opencode/final27_gate.log`. Read every "11/11", "11 rows", or "PASS (11 pass, 0 skip, 0 fail)" above as a record of the state at its own date, not as a live gate. The body of this file is left byte-identical; this note is the only change.
