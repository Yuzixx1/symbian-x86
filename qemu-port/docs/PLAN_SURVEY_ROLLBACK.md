# PLAN SURVEY — reusable rollback protocol for guarded deletions

Read-only survey of the existing guarded deletion/snapshot tooling at HEAD
`ee8490e`. No edits, no deletions, no commit/push, no `make clean`, no QEMU.
`kernelhwsrv/` untouched. Written only to this file.

## Existing tooling inventory

| Tool | Role | Mutation? |
|---|---|---|
| [tools/r1_shim_retire_plan.py](/home/yuzi/Documents/Sybian\ REAL/qemu-port/tools/r1_shim_retire_plan.py) | Audit-only planner; `DRY_RUN_ONLY` assert + argv guard; parses `r1_genuine_retire_test.py` verdicts (`SUBSYSTEM <name>: PASS|FAIL|SKIP`) with `--dry-run` or `--from-file` | Never mutates |
| [tools/r1_shim_retire_apply.py](/home/yuzi/Documents/Sybian\ REAL/qemu-port/tools/r1_shim_retire_apply.py) | Guarded shim retire; imports plan strings; helpers `sha256`, `git_porcelain`, `kernelhwsrv_clean`, `safe_target`, `check_edits` (exactly-one occurrence), `run_gate`; backup schema `r1-shim-retire-apply-backup/v2`; tokens `RETIRE-<SHIM>` / `ROLLBACK-<SHIM>-<ts>` | Edits only, gated |
| [tools/r1_winsrv_delete.py](/home/yuzi/Documents/Sybian\ REAL/qemu-port/tools/r1_winsrv_delete.py) | Guarded file-deletion executor (the strictest); imports helpers from shim_retire_apply; static `DELETE_SET` allowlist; phases P1→P2→P3 with per-phase gates; backup schema `r1-winsrv-delete-backup/v1`; tokens `DELETE-WINSRV-EWSRV` / `ROLLBACK-WINSRV-<ts>` | Deletes+edits, gated |
| [tools/r1_genuine_retire_test.py](/home/yuzi/Documents/Sybian\ REAL/qemu-port/tools/r1_genuine_retire_test.py) | The gate: `--only efsrv|ewsrv|estart|euser`, `--dry-run` (SKIP, no QEMU) vs real boot; prints `SUBSYSTEM <name>: <verdict>` | Read-only harness |
| `/tmp/opencode/winsrv_retire_backup_20261124_{215436,215742,220232,220402}/` + `svcmgr_retire_backup_20261124_185621/` | Live snapshot dirs, each with `manifest.json` (schema, gate, confirm/rollback tokens, per-file sha256) | Artifacts |
| `FORBIDDEN_ARGV` / `FORBIDDEN_VERBS` | Both executors refuse argv containing `git commit`, `git push`, `make clean`, `rm -rf` (winsrv tool scans argv only, not its own source) | Guard |

## Snapshot contents

Snapshot **every op target — edited AND delete-listed** — before the first
write, into `/tmp/opencode/<prefix>_<YYYYMMDD_HHMMSS>/` (suffix `_N` on
collision). Per file: `copy2` the source, verify `sha256(src) == sha256(dst)`
at backup time, record `{path, sha256, op, id}` in `manifest.json` alongside
`schema`, `created`, `dir`, `gate`, `confirm_token`, `rollback_token`. Restore
path (shared by both tools): validate schema + token + per-entry path
(`safe_target`: inside qemu-port/, resolved, never under kernelhwsrv/, no
absolute/`..`), re-verify snapshot sha256, stage each restore as
`<name>.<tool>_tmp`, verify staged hash, then `os.replace` and re-verify
post-restore — all-or-nothing, any mismatch REFUSEs before/without partial
writes. Git fallback remains: `git -C <repo> show <pre-gate-sha>:qemu-port/<path>`.

## Confirmation/token scheme

Two distinct token classes, never interchangeable:

- **Apply token** (proves intent to mutate): static per-op-set string printed
  by the plan, e.g. `DELETE-WINSRV-EWSRV` (prefix `DELETE-WINSRV-`) or
  `RETIRE-<SHIM>`. Required together with `--apply`; wrong/missing → rc 2, no
  writes.
- **Rollback token** (proves authority over a specific snapshot): dynamic
  `ROLLBACK-<SCOPE>-<timestamp>` stored in that snapshot's manifest; required
  with `--rollback DIR --confirm <token>`; mismatch → rc 2, nothing restored.

Tokens are matched by exact string equality (plus prefix check for apply
tokens); the tool prints both tokens in the plan output so they are
copy-pasteable and auditable in logs.

## Gate ordering

1. **Preview** (no args / `--dry-run`): print full op plan, per-op
   uniqueness/allowlist checks, consumer grep, gate in `--dry-run` (no QEMU);
   always REFUSE to change anything.
2. **Preconditions**: `kernelhwsrv/` porcelain-clean; op targets unmodified
   (no stacking; auto-skip only for ops whose old-string already vanished =
   applied by an earlier commit); consumer grep clean outside the deletion
   set; standing invariants (EFILE guard-only); **pre-apply gate real run
   must be PASS** (rc 3 otherwise).
3. **Snapshot** (above), then phases in fixed order — P1 hooks/edits →
   P2 build wiring → P3 deletions last — each followed by an incremental
   `make` (never `make clean`) + gate set + `run_gate(..., dry_run=False)`;
   any non-PASS → exception → auto-rollback → re-run gate after rollback.
4. **Post**: success prints the rollback token for manual restore; human
   commits separately (the tool never commits).

## Verification checklist

- [ ] Plan prints every op with `unique`/`applied`/`in DELETE_SET` status; zero `AMBIGUOUS`.
- [ ] Pre-apply gate verdict == PASS (real run, not dry-run).
- [ ] `git status --porcelain -- kernelhwsrv/` empty; op targets clean.
- [ ] Snapshot dir created with manifest; every file's sha256 verified at copy time.
- [ ] Each phase gate PASS before the next phase starts.
- [ ] On any post-write failure: auto-rollback rc == 0 and gate re-run after rollback.
- [ ] Rollback tokens consumed only via `--rollback DIR --confirm ROLLBACK-...` with exact match — a token is single-purpose: apply tokens can never trigger restore, rollback tokens can never trigger apply, and a rollback token only ever names its own manifest dir (schema + token + path allowlist all checked). No code path deletes a snapshot dir, so tokens cannot be consumed "accidentally" by deletion; the residual risk is only *manual misuse*, mitigated by exact-match refusal (rc 2) and refusal on unknown schema/empty manifest/duplicate paths.

## Risks

- **Snapshot lifetime**: backups live in `/tmp/opencode` and vanish on reboot — copy manifests/logs into the closeout doc or `archive/` before reboot; git-history fallback (`git show <sha>:<path>`) is the durable backstop.
- **Token leakage**: tokens are printed in plaintext logs; anyone with log access can restore — acceptable because restore is non-destructive (returns to pre-apply state), but rollback tokens must never be reused as apply tokens (prefixes differ by design).
- **Auto-skip semantics**: an edit whose old-string is absent is treated as already applied, not as failure — a wrong plan could silently no-op; mitigate by reviewing the plan's `applied` count before `--apply`.
- **/tmp tampering**: snapshot dirs are not signed; sha256 checks catch corruption but not malicious re-creation with a matching manifest — keep `/tmp/opencode` permissions tight.
- **Phase-gate cost**: P3 gate set boots QEMU several times (smoke, ewsrv, input, doom, fullchain, retire, final-verify) — long but that is the price of proof; never shortcut by skipping the post-rollback gate.
- **Scope creep**: `DELETE_SET` is static per tool; a reusable protocol must keep the allowlist explicit per deletion batch (never glob), or rollback safety is lost.
