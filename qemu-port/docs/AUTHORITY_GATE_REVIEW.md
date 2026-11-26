# AUTHORITY GATE REVIEW — independent review of the full 11/11 strict verification

Reviewed: 2026-11-24 ~19:02 EET, read-only (no edits outside this file, no QEMU run,
no commit/push, kernelhwsrv/ untouched). Reviewer: subagent session.

## Verdict

**PARTIAL — a trustworthy full 11/11 PASS exists only for the `d26fade` tree
(03:55 strict run). On the current post-cleanup / post-svcmgr-edit working tree the
strict run NEVER CONCLUDED: no `OVERALL:` line was captured for it, and its per-test
logs show 10/11 PASS with the single FAIL being a transient mid-rebuild artifact
(superseded by two later PASS smoke runs). The tree is still under active concurrent
edit/rebuild, so no clean 11/11 on the current tree can be claimed yet.**

## Evidence table

| Test / artifact | Verdict | Log path | mtime (2026-11-24) |
|---|---|---|---|
| strict-skip OVERALL, d26fade tree | **PASS — OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run)** | `/tmp/opencode/final_verify_strict_20261124.log` | 03:55:15 |
| strict regression run on d26fade (PID unknown) | truncated at `genuine_ewsrv`, no OVERALL | `/tmp/opencode/final_verify_strict_regression_d26fade.log` | 18:37 |
| 18:46 run (PID 884980, now dead) — selfcheck step | step FAIL (internal qemu_smoke transient FAIL, 0 faults) | `/tmp/opencode/final_verify_selfcheck.log` | 18:50:37 |
| 18:46 run — qemu_smoke | PASS (RESULT: PASS, exports=2503, EUSER resident) | `/tmp/opencode/final_verify_qemu_smoke.log` | 18:51:27 |
| 18:46 run — genuine_retire | PASS (efsrv/ewsrv/estart/euser, 4 passed 0 failed) | `/tmp/opencode/final_verify_genuine_retire.log` | 18:52:18 |
| 18:46 run — genuine_efsrv | PASS (F32PROBE/F32CLIENT IPC round trip, port-shim=0) | `/tmp/opencode/final_verify_genuine_efsrv.log` | 18:52:33 |
| 18:46 run — genuine_ewsrv | PASS (EWCLIENT Create/SetCursorPos/Write2 r=0, 232 PE2 imports) | `/tmp/opencode/final_verify_genuine_ewsrv.log` | 18:52:52 |
| 18:46 run — genuine_eshell | PASS (all 8 commands, fault-free) | `/tmp/opencode/final_verify_genuine_eshell.log` | 18:53:14 |
| 18:46 run — genuine_fat_io | PASS (FATIO-MARKER-42A7 on volume, round trip OK) | `/tmp/opencode/final_verify_genuine_fat_io.log` | 18:53:29 |
| 18:46 run — doom_main | PASS (frames advancing [1,35,70]) | `/tmp/opencode/final_verify_doom_main.log` | 18:54:45 |
| 18:46 run — doom_audio | PASS (AU pcm, DOOM TITLE/FRAME) | `/tmp/opencode/final_verify_doom_audio.log` | 18:55:19 |
| 18:46 run — doom_playtest | PASS (max frame 2870, clean Esc DONE, prompt returns) | `/tmp/opencode/final_verify_doom_playtest.log` | 18:56:32 |
| 18:46 run — doom_multikey | PASS (12 scenario loops, STALE swallowed: 0) | `/tmp/opencode/final_verify_doom_multikey.log` | 18:57:37 |
| 18:46 run — OVERALL | **never captured** (PID 884980 dead after 18:57:37; no `final_verify_overall`/stdout file found) | — | — |
| post-removal fullchain (separate gate) | PASS — RESULT: PASS (14/14 rows, 0 fault lines) | `/tmp/opencode/post_removal_fullchain.log` | 18:51 |
| fullchain rerun on current tree | boot chain EFILE pid=9 → ESTART 13 → EWSRV 15 → ESHELL 17, "svcmgr fallback skipped", 0 KERN-/FAULT lines | `/tmp/opencode/r1_fullchain.log` (+ copy in `/home/yuzi/.cache/sybian-r1/r1_fullchain.log` 18:54) | 18:58:19 |
| concurrent selfcheck smoke (in-flight run) | RESULT: PASS | `/tmp/opencode/r1_os_selfcheck_qemu_smoke.log` | 18:58:35 |

Newest logs in `/home/yuzi/.cache/sybian-r1/` are only `r1_fullchain.*` (18:54) —
no newer final_verify logs there; `/tmp/opencode/` holds the newest strict logs.

## Fault cross-check (task 2)

- Grep for `FAULT|Exc |Panic|#PF|KERN-` across all 11 `final_verify_*.log` (18:46
  run) plus `r1_qemu_smoke.log`, `r1_fullchain.log`, `r1_selftest.log`,
  `r1_ca_test.log`, `cp4_stress_907536.log`, `post_removal_fullchain.log`:
  **zero real fault lines**. Every gate log ends with a "no Exc/PanicKernExec/FAULT/#PF"
  PASS line — including the failed 18:50 selfcheck smoke, which failed on missing
  E32Validate/EUSER-resident markers only, with `[PASS] no Exc or PanicKernExec`.
- Stale/superseded only: `r1_retire_full.log` (2026-11-23 22:41) KERN 0x62 FAULTs from
  an old exploratory run, superseded by the 18:52 4/4 PASS; `IPCCLI: connect failed`
  lines in `cp4_stress_907536.log` are non-fatal (0 fault-pattern lines).

## CLEANUP_AUTHORITY_CHECK.md (task 3)

**ABSENT.** `find docs -iname '*AUTHORITY*'` → nothing; `docs/CLEANUP_STEP_STATUS.md`
also lists it under "Absent (pending)". So the authority check document does not
exist yet — consistent with the strict run on the current tree never concluding.

## Caveats (task 4)

1. **Concurrent svcmgr-apply edits during the 18:46 run:** `Makefile` 18:49:30,
   `variant/qemu_variant.h` 18:49:37, `variant/qemu_variant_asic.cpp` 18:49:36,
   `user/user.mk`, `variant/e32load.cpp` (working-tree diff vs `d26fade`: −164/+78
   lines), with `build/qemu_variant_*.o` rebuilt 18:49:39. The 18:50 selfcheck
   qemu_smoke FAIL coincides exactly with this relink race (booted a half-written
   `kernfull.elf`); steps from 18:51 onward ran on the post-edit kernel and PASSED.
2. **Active rebuild right now:** two `r1_os_selfcheck.py` instances running
   (PIDs 903180, 906216, started 18:56/18:57) each doing `make build`/`make isogrub`
   + QEMU; `build/kernfull.elf` relinked again at 18:59:35 and 19:01:58;
   `build/symbian-grub.iso` 18:57:44. `.sybian/qemu-slot.lock` exists but is empty
   (mtime Nov 23) — not a reliable lock, so mtimes were used. **Per instructions, no
   QEMU / final_verify was run by this review.**
3. DOOM steps staged `/tmp/opencode/r1_doom.iso` at run start (18:46); the ISO was
   rebuilt 18:57:44, so early DOOM results may reflect the pre-18:49 image.
4. `docs/SVCMGR_APPLY_STATUS.md` (18:56) claims "strict 11/11 PASS" as baseline, but
   the only captured strict OVERALL 11/11 is the 03:55 d26fade run — treat that doc's
   wording as referring to d26fade, not the current tree.

## Recommendation (task 4)

Do not cite "11/11 on the current tree" yet. Once the svcmgr apply agent finishes
(no `make`/QEMU processes, `Makefile`/`variant/*` mtimes stable for several minutes),
run `python3 tools/r1_final_verify.py --strict-skip` from `qemu-port/` and require
`OVERALL: PASS (11 pass, 0 skip, 0 fail)` with all `final_verify_*.log` fault-free;
then write `docs/CLEANUP_AUTHORITY_CHECK.md` with that run as the authority and update
`docs/CLEANUP_STEP_STATUS.md` / `docs/SVCMGR_APPLY_STATUS.md`. Rollback snapshots
(`/tmp/opencode/shim_retire_backup_*`, `git show d26fade:<path>`) remain valid.

---

> **SUPERSESSION 2026-11-26 — authoritative gate is 15 rows, `FINAL27_RC=0`.**
> The 11/11 figures in the title (`:1` "the full 11/11 strict verification"), the
> verdict (`:10` "a trustworthy full 11/11 PASS exists only for the `d26fade`
> tree"), and the recommendation (`:83` "require `OVERALL: PASS (11 pass, 0 skip,
> 0 fail)`") are **historical** and are all cleared by the authoritative run:
> `/tmp/opencode/final27_gate.log` at HEAD `ee8490e`, ending
> `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` → **15/15 PASS, `FINAL27_RC=0`**.
> Roster confirmed by `tools/r1_final_verify.py --list`: 15 rows, row 15 =
> `hal_probe` (`tools/r1_genuine_hal_test.py`). The review's standing caution — do
> not cite an 11-row PASS — is superseded by the 15-row PASS; `FINAL26_RC=0`,
> `FINAL23_RC=0`, `FINAL20_RC=0` and every 11-row count are superseded history
> only. Body left unedited. No QEMU, no gates run, no source edits, no commits/push,
> no `make clean`, `kernelhwsrv/` untouched.
