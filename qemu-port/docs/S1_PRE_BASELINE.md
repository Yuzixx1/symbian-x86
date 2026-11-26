# S1 Pre-Delete Baseline (before-state)

Captured read-only before the guarded S1 `fs/r1_fs_core.mmp` delete. No deletes, no QEMU, no commits.

- Sweep: `python3 tools/r1_shim_residue_sweep.py` -> `RESULT: FAIL (18 residue, 43 allowed comments)`, rc 1.
- Residue 18 = 10 `[file] r1_fs` + 8 `[rule] r1_fs` Makefile:342-351 hits.
- Target: `fs/r1_fs_core.mmp` — 962 bytes, mode 644.
- Target sha256: `deb2d548f85a80a39156b18b0cf79bedf84d4d2465d94bd81db5d29757f53ffc`.
- Repo root `git log -1`: `ee8490e Finish winsrv/GUI retirement: drop stale build wiring for deleted GUI objects`.
- Repo root `git status --short kernelhwsrv/`: empty (clean, READ-ONLY preserved).

Expected after-state of the S1 delete: 17 residue (one `[file] r1_fs` entry, `fs/r1_fs_core.mmp`, gone; Makefile rules untouched).

## 2026-11-25 — S1 Post-Delete Result: PASS

Guarded delete of `fs/r1_fs_core.mmp` executed; earlier baseline above left intact.

- Apply: `APPLY_RC=0`; END `22:16:12`, wall `24m12s`; `qemu-slot` flock serialized.
- Deleted: `fs/r1_fs_core.mmp` (sha256 `deb2d548...`, 962 B). Backup: `/tmp/opencode/wave2_s1_backup_20261125_220404`.
- Rollback token: `ROLLBACK-WAVE2-S1-20261125_220404` — **not used**.
- Residue: G12 `18 -> 17` (expected 17) PASS. Gates pre+post: G12 PASS, R `4/4` PASS, F `14/14` PASS, S strict `11/11` PASS.
- State: no commits; `kernelhwsrv/` untouched (READ-ONLY).
- Next: S1 unblocks S2, which expects `17 -> 9`.

- 2026-11-25 — Strict pre/post timing: 234.2s vs 236.1s (+0.8%), same 11 tests all PASS; delta entirely cp4_stress jitter; doom_multikey 65.8s vs 65.2s; no regression.
- 2026-11-25 22:34 — independent S1 gate re-run (post-delete): `flock .sybian/qemu-slot.lock`, `LD_LIBRARY_PATH` empty, lock acquired 22:21:51, END 22:34:06. R `4/4` PASS rc 0; F `14/14` PASS rc 0; S strict `11/11` PASS rc 0 (0 skip, 0 fail). Sweep re-checked 22:22: `RESULT: FAIL (17 residue, 43 allowed comments)` rc 1 — exactly 17, no rollback; snapshot `/tmp/opencode/wave2_s1_backup_20261125_220404`, token `ROLLBACK-WAVE2-S1-20261125_220404` unused. Sibling S2 `--apply` held the lock 22:16:51-22:21:51 and landed docs-only changes (residue stayed 17), so all three gates measure the post-S1 tree. No commits; HEAD `ee8490e`; `kernelhwsrv/` untouched.
