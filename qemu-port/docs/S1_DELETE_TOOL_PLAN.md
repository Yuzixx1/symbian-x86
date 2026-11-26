# S1 guarded delete — `fs/r1_fs_core.mmp` (design only, nothing deleted)

**Pick (b), implemented by IMPORT, not by copy.** `r1_winsrv_delete.py:79` already reuses `r1_shim_retire_apply` "BY IMPORT (never copied)" (`:9`); a 746-line copy drifts. Option (a) instead edits a proven tool, risking the winsrv retire, and puts two delete scopes behind one confirm path. New `tools/r1_wave2_delete.py` (~140 lines, additive) owns only:

| scope | exactly `fs/r1_fs_core.mmp` (962 B, sha256 `deb2d548…f53ffc`) in a one-path frozenset; any other arg → exit 2, nothing touched |
| confirm | `--apply --confirm DELETE-WAVE2-S1-deb2d548` — content-bound sha256[:8], own token, so a wave2 confirmation can never delete a winsrv path |
| backup | `/tmp/opencode/wave2_s1_backup_<YYYYmmdd_HHMMSS>/` + `r1-wave2-delete-backup/v1` manifest, per-file sha256 verified at copy |
| rollback | token `ROLLBACK-WAVE2-S1-<YYYYmmdd_HHMMSS>`; `--rollback DIR --confirm TOKEN`, sha256 checked before and after restore |
| reuse | `base.sha256/safe_target/git_porcelain/kernelhwsrv_clean/run_gate`; refuses dirty `kernelhwsrv/`, paths outside `qemu-port/`, argv `git commit`/`git push`/`make clean`/`rm -rf` |

**Gates.** Pre: retire 4/4, fullchain 14/14, G12 count **18**, strict 11/11. Post (and after any rollback): G12 count **exactly 17** — the exit code alone is not evidence; 16 or 18 is NO-GO — plus strict 11/11, 4/4, 14/14. Any shortfall auto-rolls back from the manifest and re-gates. No QEMU, no commit, no push, no `make clean`.

**Why a bare `rm` is not acceptable.** It records no pre-state (no sha256 manifest), so there is no verifiable restore; it runs no post-gate, so a wrong or partial delete is indistinguishable from success; it refuses no path, so one typo or glob reaches the 7 LIVE `fs/r1_fs_*` keepers or `kernelhwsrv/`; and it leaves no gate evidence for review. The guard set — exact set, content-bound token, manifest, gates — is what makes this deletion reversible and auditable, not the unlink itself.

---

**SUPERSEDED 2026-11-26.** The 11-row / 10-11-pass strict gate asserted above is historical and no longer current. Current authority is **15 rows / `FINAL27_RC=0`** — 15 pass, 0 skip, 0 fail — recorded in `/tmp/opencode/final27_gate.log`. The body above is preserved byte-identical for audit history.
