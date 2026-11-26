# S2 plan — remove the R1_FS_CHANNEL opt-in block (Makefile only)

Delete `Makefile:342-351` as one hunk. Residue-counted lines: 342 `R1_FS_CHANNEL ?= 0`,
343 `_OBJ :=`, 344 `ifeq`, 345 `_OBJ := build/...`, 346 object rule + deps, 347 deps
continuation, 349 compile recipe, 351 `KERNFULL_OBJS +=` (8). 348 (rule prereqs
continuation) and 350 `endif` are deleted with them but not counted. Comment prose
334-341 is non-residue; it goes stale with the opt-in and should be reworded to a
retirement note, not deleted as evidence. Channel sources `fs/r1_fs_channel*`,
`fs/r1_fs_channel_state.h`, `fs/r1_fs_wire_logic.h`, `variant/r1_fs_channel.*`,
`tests/r1_fs_channel` stay for S3 — this step unwires, it does not delete.

Snapshot/token: `sha256sum Makefile` = d6f55b8d047db5cf1f8501af374a3c1121ab0cd08f4ba76ff1f46b939306ef98;
G12 token `RESULT: FAIL (18 residue, 43 allowed comments)`.

Pre-gate (static, no QEMU): sweep FAIL(18); `make -n kernfull | grep r1_fs_channel.o` non-empty.
Post-gate: sweep `FAIL (9 residue)` = 18 − 8 (this block) − 1 (`fs/r1_fs_core.mmp`,
sibling step, not owned here); `grep -n R1_FS_CHANNEL Makefile` returns only comment text;
`make -n kernfull` names no channel object; `make build/kernfull.elf` links (build only).
Rollback: `git checkout -- Makefile`, re-run the sweep for the 18-residue pre-token; no `build/r1_fs_channel.o` exists by default, so no artifact sweep is needed.
2026-11-25 S2 apply pass 2: all pre-gates PASS (MW two-part, G12 17, R 4/4, F 14/14, S 11/11) but crashed at make_backup KeyError exp (rc 1) before the Makefile edit; no manifest/rollback token; Makefile sha d6f55b8d... unchanged; failure is a tool bug, not a tree regression; fix and rerun pending.
2026-11-25 correction: the pre-token above (G12 residue 18, line 13/15) is stale after S1 (`fs/r1_fs_core.mmp` deleted); the correct pre expectation is 17 and the post expectation is 9 (17 − 8 for this block), so the post-gate formula on line 16 should read 17 − 8 = 9, not 18 − 8 − 1. Artifact caveat: the sweep reads 18 again — not 17 — while the orphan `build/r1_fs_channel.o` still exists in the tree; that object must be removed before apply, or the residue count is off by one and the pre-gate appears to regress.
2026-11-25 S2 apply pass 3: APPLY_RC=0 (success — Makefile hunk applied, 10 lines removed at Makefile:342-351, 1 file); pre gates ALL PASS (MW two-part, G12 17, R 4/4, F 14/14, S 11/11 strict) and post gates ALL PASS (MW-a default absent + MW-b R1_FS_CHANNEL=1 absent, G12 9, R 4/4, F 14/14, S 11/11 strict); residue 17 -> 9 (S1 fs/r1_fs_core.mmp already gone; this block = -8); Makefile sha256 e12dcf8af25378d777ee62e821fa285749f85b58ee2cbecaf2ada23f2d3df133 (was d6f55b8d047db5cf1f8501af374a3c1121ab0cd08f4ba76ff1f46b939306ef98, matched expected post-sha); backup /tmp/opencode/wave2_s2_backup_20261125_231352 with rollback token ROLLBACK-WAVE2-S2-20261125_231352 (git fallback: git checkout -- qemu-port/Makefile); UNCOMMITTED — git shows M qemu-port/Makefile, HEAD ee8490e, no commit made; next steps remain human-reviewed: reword Makefile:337-341 stale comment prose, build/link kernfull, commit when authorized.
2026-11-25 S2 independent verification (post pass 3): APPLY_RC=0; residue 17 -> 9 re-measured with tools/r1_shim_residue_sweep.py --json; Makefile sha256 e12dcf8af25378d777ee62e821fa285749f85b58ee2cbecaf2ada23f2d3df133 equals the expected post-state; git diff = 1 file, exactly 10 deletions at Makefile:342-351, nothing else; `make -n kernfull` names no channel object; `grep -n R1_FS_CHANNEL Makefile` now matches nothing; no `build/r1_fs_channel.o` (a stray one from an earlier gate build made the sweep read 18 and was removed to restore the documented 17-line pre-state before the apply); kernelhwsrv/ clean, still READ-ONLY; gates (MW-a + MW-b, R 4/4, F 14/14, S 11/11 strict) all PASS under flock .sybian/qemu-slot.lock; snapshot /tmp/opencode/wave2_s2_backup_20261125_231352, token ROLLBACK-WAVE2-S2-20261125_231352; rollback NOT used; edit left UNCOMMITTED.

2026-11-26 supersession note: the 11-row / 10-11-pass gate described above is historical. Current authority is 15 rows / FINAL27_RC=0 (15 pass, 0 skip, 0 fail, /tmp/opencode/final27_gate.log).
