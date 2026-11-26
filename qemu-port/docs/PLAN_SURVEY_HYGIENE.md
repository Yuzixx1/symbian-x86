# PLAN SURVEY — working-tree hygiene after the winsrv retirement

Read-only survey of the uncommitted working tree at HEAD `ee8490e` (winsrv/GUI
retirement closeout). No deletions, no edits, no commit/push, no `make clean`,
no QEMU. `kernelhwsrv/` untouched (its own repo reports clean; root
`.gitignore` excludes `/kernelhwsrv/`). Written only to this file.

## Inventory

`git status --porcelain` shows **142 entries**: 136 untracked (`??`) + 6
modified (`M`).

| Group | Count | Entries |
|---|---|---|
| `qemu-port/docs/WINSRV_DELETE_WATCH*.md` | 128 | untracked monitor reports |
| Substantive modified docs | 5 | `AGENTS.md`, `qemu-port/docs/GO_NO_GO.md`, `CLEANUP_LOG.md`, `FINAL_GATE_STATUS.md`, `DOCS_INDEX.md` |
| Coordinator state | 1 | `qemu-port/.sybian/coordinator/state.json` (modified; timestamp bump only: `2026-10-29` → `2026-11-23`) |
| Substantive untracked docs | 5 | `WINSRV_RETIREMENT_FINAL.md`, `PLAN_SURVEY_ESOCK.md`, `PLAN_SURVEY_MEDIA.md`, `PLAN_SURVEY_SHIMS.md`, `PLAN_SURVEY_UIKON_GDI.md` |
| Stray runtime/lock files | 2 | `qemu-port/9` (0-byte stray; WATCH27 notes it was "not created by this monitor"), `qemu-port/.sybian/qemu-slot.lock` (0-byte flock used by `.sybian/bin/coord` and `.sybian/bin/work`) |
| Preservation archive | 1 | `symbian-x86-archive-56fbaff.tar.gz` (2.4 MB gzip, 983 entries) |

Notes: all 128 WATCH files have distinct content (md5-unique), 8–30 lines each,
~2,419 lines total — steady-state "re-apply APPLY_RC=0, gates PASS" reports.
Root `.gitignore` covers `*.log`/`*.zip` but **not** `*.tar.gz`, `qemu-port/9`,
or `qemu-slot.lock`, so all three show as untracked.

## Safe to archive/delete

- **The 128 `WINSRV_DELETE_WATCH*.md`** — redundant once
  `WINSRV_RETIREMENT_FINAL.md` exists and is committed: that closeout doc
  explicitly states it "supersedes the `WINSRV_DELETE_WATCH*` steady-state
  reports" and records the same evidence (APPLY_RC=0, P1–P3 PASS, commit
  `94ce9c9`, rollback token, kernelhwsrv clean). Archive (e.g. move to an
  `archive/` dir) or delete after the closeout is committed.
- **`qemu-port/9`** — 0-byte accidental artifact; safe to delete.

## Must keep/commit

- `qemu-port/docs/WINSRV_RETIREMENT_FINAL.md` — the single authoritative
  closeout; commit it first so the WATCH reports' evidence has a home.
- `qemu-port/docs/PLAN_SURVEY_ESOCK.md`, `PLAN_SURVEY_MEDIA.md`,
  `PLAN_SURVEY_SHIMS.md`, `PLAN_SURVEY_UIKON_GDI.md` — substantive planning
  surveys (read-only kernelhwsrv-vs-port analyses); commit.
- Modified: `AGENTS.md` (2026-11-24 verified-state block), `GO_NO_GO.md`,
  `CLEANUP_LOG.md`, `FINAL_GATE_STATUS.md` (all GREEN retirement updates),
  `DOCS_INDEX.md` (append-only index backfills) — substantive; commit.
- `qemu-port/.sybian/coordinator/state.json` — tracked coordinator state; only
  the `updated_at` timestamp changed. Commit with the docs or restore; leaving
  it modified keeps noise in `git status`.

## Preservation points

- **`symbian-x86-archive-56fbaff.tar.gz` — must stay untracked.** It is the
  983-entry preservation snapshot of `archive/handwritten-r25` at `56fbaff`
  referenced by `DOCS_INDEX.md` and `AGENTS.md` ("Preservation:
  `archive/handwritten-r25` at `56fbaff` + tarball; never `git add -A`").
  Keep on disk, never commit; add `*.tar.gz` to `.gitignore` to silence status
  noise. Rollback snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`
  (token `ROLLBACK-WINSRV-20261124_215436`) is unused and lives in /tmp.
- `qemu-port/.sybian/qemu-slot.lock` — transient runtime lock created by
  `.sybian/bin/coord`/`work`; keep untracked, never commit; add to `.gitignore`.

## Recommended sequence

1. Commit the closeout + substantive docs in one change: `WINSRV_RETIREMENT_FINAL.md`,
   the 4 `PLAN_SURVEY_*.md`, and the 5 modified docs (`AGENTS.md`, `GO_NO_GO.md`,
   `CLEANUP_LOG.md`, `FINAL_GATE_STATUS.md`, `DOCS_INDEX.md`), plus the
   `state.json` timestamp bump.
2. Archive (or delete) the 128 `WINSRV_DELETE_WATCH*.md` — now superseded by the
   committed closeout.
3. Delete the 0-byte stray `qemu-port/9`.
4. Extend `.gitignore` with `*.tar.gz`, `qemu-port/.sybian/qemu-slot.lock`, and
   a stray-file pattern — leaves only intentional preservation artifacts
   untracked.
5. Verify `git status` is clean except the tarball (intentional preservation
   point) and confirm `kernelhwsrv/` remains untouched.
