# Wave-2 baseline snapshot — 2026-11-25T19:28:21Z

Dated pre-mutation baseline for wave 2. Captured read-only, before any wave-2 edit.

| Signal | Baseline |
|---|---|
| Repo HEAD (`git log --oneline -1`) | `ee8490e Finish winsrv/GUI retirement: drop stale build wiring for deleted GUI objects` |
| ISO `build/symbian-grub.iso` | 75,864,064 bytes, mtime 2026-11-25 21:03:26 +0200 |
| DOCS_INDEX header count | 702 declared; 702 `docs/*.md` actual (match) |
| `git status --short` entries (repo root) | 198 |
| kernelhwsrv clean status | clean, 0 entries; HEAD `0c32086 201043_12` |

Verification gates (OVERALL lines, all identical):

- `final_verify_recovery.log` — `OVERALL: PASS  (11 pass, 0 skip, 0 fail/not-run)` (234.4s)
- `p1_step2/strict.log` — `OVERALL: PASS  (11 pass, 0 skip, 0 fail/not-run)` (234.7s)
- `p4_strict.log` — `OVERALL: PASS  (11 pass, 0 skip, 0 fail/not-run)` (234.1s)

D4 = no commits: HEAD is unchanged from wave 1 close-out, so every wave-2 change
starts from this tree as **uncommitted** work. The 198 status entries above are the
pre-wave-2 uncommitted set, not wave-2 output.

Baseline is a reference point only: no commit, no push, no `make clean`, no QEMU run,
and `kernelhwsrv/` was read via its own git repo and never written.

2026-11-26 supersession note: the 11-row / 10-11-pass gate described above is historical. Current authority is 15 rows / FINAL27_RC=0 (15 pass, 0 skip, 0 fail, /tmp/opencode/final27_gate.log).
