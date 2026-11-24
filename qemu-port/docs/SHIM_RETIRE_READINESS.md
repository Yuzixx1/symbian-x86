# SHIM_RETIRE_READINESS — dry-run readiness audit (2026-11-24)

**Scope:** read-only. No source edits, no `--apply`, no QEMU. `kernelhwsrv/` untouched
(`git status --porcelain -- kernelhwsrv/` empty). The only file created is this report.

## 1. Verdict

All four genuine retire gates now report **PASS in the authoritative full run**, but the
apply tool's **default dry-run preview always reports BLOCKED/SKIP by design** (a
`--dry-run` gate never executes runtime, so it can never prove readiness). Feeding a
captured full-run PASS log via `--from-file` shows all four shims **READY** with unique
edit matches. **Apply would still be refused today** for a concrete reason: two edit
targets are already dirty (`e32load.cpp`, `qemu_variant_asic.cpp`), and MED #9 of the
review intentionally refuses apply on a dirty target. No apply was attempted.

## 2. Gate evidence (full runs, not this dry-run)

- `/tmp/opencode/final_verify_genuine_retire.log` (03:50, `dry-run=False`):
  `EFSRV/F32 PASS`, `EWSRV PASS`, `ESTART/ESHELL PASS`, `EUSER PASS` —
  `RESULT: PASS (4 passed, 0 skipped [not built yet], 0 failed)`.
- `/tmp/opencode/final_verify_strict_20261124.log` (03:55): all 11 authoritative gates
  PASS, `OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run)`.
- `/tmp/opencode/estart_uid_retire_test.log` (03:23, efsrv FAIL) is superseded by the
  03:50 run above (F32CLIENT `IPC round trip OK` now present).

## 3. Per-shim readiness

| Shim | Gate (full run) | Default preview | `--from-file` preview | Edit set (`--apply` would change) | Apply permitted? |
|---|---|---|---|---|---|
| portfilesrv | EFSRV/F32 PASS | BLOCKED (SKIP) | **READY** | `variant/e32load.cpp:1304` (SvcMgrBootLaunch fallback), `user/svcmgr.cpp:250` (`\PORTFILESRV.E32` row) — both unique | No: `--apply` absent; targets dirty (e32load.cpp) |
| r1_fs_* | EFSRV/F32 PASS | BLOCKED (SKIP) | **READY** | same two edits as portfilesrv — both unique | No: same reasons |
| winsrv | EWSRV PASS | BLOCKED (SKIP) | **READY** | **NONE** — file removal only, human review; genuine `\EWSRV.E32` + GenuineBootSpawn/ShellSpawnFull stay | No: tool has no apply path (returns rc 2) |
| svcmgr | ESTART/ESHELL PASS | BLOCKED (SKIP) | **READY** | `variant/qemu_variant_asic.cpp:194` (`SvcMgrExecInstall();`) — unique | No: `--apply` absent; target dirty |

## 4. Exact preview output

Default preview (`python3 tools/r1_shim_retire_apply.py`), trimmed to the substantive lines:

```
r1_shim_retire_apply -- DRY-RUN PREVIEW: NO file is changed; no QEMU was booted (gate --dry-run).
gate status source: live gate --dry-run (no QEMU)
=== portfilesrv: BLOCKED  (gate EFSRV/F32 = SKIP) ===
  [unique] variant/e32load.cpp:1304 -> variant/e32load.cpp  (unique exact match)
  [unique] user/svcmgr.cpp:250 -> user/svcmgr.cpp  (unique exact match)
  confirm token (if gate ever PASSes): RETIRE-PORTFILESRV
  REFUSE: --apply not set (or gate not PASS) -- no file changed.
=== r1_fs_* (core/server/client): BLOCKED  (gate EFSRV/F32 = SKIP) ===
  [unique] variant/e32load.cpp:1304 -> variant/e32load.cpp  (unique exact match)
  [unique] user/svcmgr.cpp:250 -> user/svcmgr.cpp  (unique exact match)
  confirm token (if gate ever PASSes): RETIRE-R1FS
  REFUSE: --apply not set (or gate not PASS) -- no file changed.
=== winsrv: BLOCKED  (gate EWSRV = SKIP) ===
  edit set: NONE (hand-written shims only; genuine entries kept)
  note: no hand-written boot-line shim: genuine \EWSRV.E32 entry and GenuineBootSpawn/ShellSpawnFull must stay; retire winsrv by removing its files per docs/SHIM_RETIREMENT_PLAN.md
  confirm token (if gate ever PASSes): RETIRE-WINSRV
  REFUSE: --apply not set (or gate not PASS) -- no file changed.
=== svcmgr: BLOCKED  (gate ESTART/ESHELL = SKIP) ===
  [unique] variant/qemu_variant_asic.cpp:194 -> variant/qemu_variant_asic.cpp  (unique exact match)
  confirm token (if gate ever PASSes): RETIRE-SVCMGR
  REFUSE: --apply not set (or gate not PASS) -- no file changed.
Apply needs: --shim <name> --apply --confirm <TOKEN>; the real (non-dry-run) gate must report PASS first.
```

Per-shim dry-runs (`--shim portfilesrv|r1_fs|winsrv|svcmgr`) reproduced the matching
single-shim blocks above verbatim.

Preview from captured full-run PASS log (`--from-file /tmp/opencode/final_verify_genuine_retire.log`):

```
r1_shim_retire_apply -- DRY-RUN PREVIEW: NO file is changed; no QEMU was booted (gate --dry-run).
gate status source: captured log: /tmp/opencode/final_verify_genuine_retire.log
=== portfilesrv: READY  (gate EFSRV/F32 = PASS) ===
  [unique] variant/e32load.cpp:1304 -> variant/e32load.cpp  (unique exact match)
  [unique] user/svcmgr.cpp:250 -> user/svcmgr.cpp  (unique exact match)
  confirm token (if gate ever PASSes): RETIRE-PORTFILESRV
  REFUSE: --apply not set (or gate not PASS) -- no file changed.
=== r1_fs_* (core/server/client): READY  (gate EFSRV/F32 = PASS) ===
  [unique] variant/e32load.cpp:1304 -> variant/e32load.cpp  (unique exact match)
  [unique] user/svcmgr.cpp:250 -> user/svcmgr.cpp  (unique exact match)
  confirm token (if gate ever PASSes): RETIRE-R1FS
  REFUSE: --apply not set (or gate not PASS) -- no file changed.
=== winsrv: READY  (gate EWSRV = PASS) ===
  edit set: NONE (hand-written shims only; genuine entries kept)
  note: no hand-written boot-line shim: genuine \EWSRV.E32 entry and GenuineBootSpawn/ShellSpawnFull must stay; retire winsrv by removing its files per docs/SHIM_RETIREMENT_PLAN.md
  confirm token (if gate ever PASSes): RETIRE-WINSRV
  REFUSE: --apply not set (or gate not PASS) -- no file changed.
=== svcmgr: READY  (gate ESTART/ESHELL = PASS) ===
  [unique] variant/qemu_variant_asic.cpp:194 -> variant/qemu_variant_asic.cpp  (unique exact match)
  confirm token (if gate ever PASSes): RETIRE-SVCMGR
  REFUSE: --apply not set (or gate not PASS) -- no file changed.
Apply needs: --shim <name> --apply --confirm <TOKEN>; the real (non-dry-run) gate must report PASS first.
```

## 5. Safety confirmations (no apply run)

- `--shim winsrv --apply` → `REFUSE: --apply requires --confirm RETIRE-WINSRV. Nothing changed.` rc=2.
- `--shim winsrv --apply --confirm WRONGTOKEN` → `REFUSE: wrong --confirm token ...` rc=2.
- `--shim winsrv --apply --dry-run --confirm RETIRE-WINSRV` → `REFUSE: --dry-run cannot be combined with --apply.` rc=2.
- No `--apply` with a correct token was run; no QEMU and no writes occurred.
- Review fixes confirmed present in `tools/r1_shim_retire_apply.py`: HIGH #1 post-gate +
  rollback inside one `try/except/finally` with rollback rc checked (`:471-516`); HIGH #2
  `BOOT_EDIT_INDEX = {efsrv:(1,2), ewsrv:(), estart:(3,)}` (`:102-106`) — genuine
  EFILE/EFSRV/ESTART/E32STRT/EWSRV/ESHELL entries and GenuineBootSpawn/ShellSpawnFull are
  excluded and documented as required by the gate (`:91-101`); MED #7 rollback limits paths
  to `planned_paths()` and enforces the rollback token (`:324-356`); MED #8 backup/restore
  verify sha256 and stage atomically (`:314-319`, `:362-407`); MED #9 `edits_clean()`
  refuses dirty edit targets (`:239-242`, `:441-447`).
- `git status --porcelain -- kernelhwsrv/` empty. `variant/e32load.cpp` and
  `variant/qemu_variant_asic.cpp` are already modified (pre-existing work); this is the
  concrete apply blocker, by design.

## 6. ESTART UID + full chain (context)

- Caller UID3 re-stamp landed: `estart_.E32` uid3 `0x10272C04` (was `0x100000bf`),
  `etshell_.E32` uid3 `0x101F7AE7`; `0x10000111` workaround removed
  (`docs/ESHELL_UID_GATE_CHECK.md`, `docs/FINAL_GATE_STATUS.md`).
- `docs/FULLCHAIN_VERIFICATION.md` §"Re-verification 2026-11-24 03:25 EET — PASS 14/14":
  `RESULT: PASS (14/14 rows PASS)`, exit 0; four genuine components `r=0`, 0
  Exc/PanicKernExec/FAULT. The 11/14 FAIL in `/tmp/opencode/fullchain_tool_rerun.txt`
  (03:14) predates the UID/EFSRV fixes.
- `docs/GENUINE_PROGRESS.md`, `GO_NO_GO.md`, `FINAL_GATE_STATUS.md` agree the chain is green.

## 7. Blockers to an actual apply (for the human)

1. Apply requires a real (non-dry-run) gate QEMU run — explicitly out of scope here.
2. Dirty edit targets `variant/e32load.cpp` and `variant/qemu_variant_asic.cpp` → MED #9
   refusal (rc 2) until committed/stashed or explicitly confirmed.
3. `winsrv` has no apply path by design (file removal + human review).
4. This tool must never be run by an autonomous agent without explicit user request.

## 8. User command — ONLY on explicit request

Do **not** run any of these unless the user explicitly asks for retirement. If asked,
first clear the dirty-target gate, then (per shim):

```bash
cd qemu-port
python3 tools/r1_shim_retire_apply.py --shim portfilesrv --apply --confirm RETIRE-PORTFILESRV
# r1_fs: RETIRE-R1FS ; svcmgr: RETIRE-SVCMGR ; winsrv: file removal + human review (no apply)
# rollback if needed: python3 tools/r1_shim_retire_apply.py --rollback <backup-dir> --confirm ROLLBACK-<SHIM>-<ts>
```
