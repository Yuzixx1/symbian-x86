# FINAL GATE STATUS — live one-page snapshot

>> 2026-11-26: the snapshot table below is HISTORICAL; authoritative verdict is 15/15 PASS (FINAL27_RC=0) in the correction footer at EOF; genuine EFSRV PASS; docs 728/728.

**Snapshot:** 2026-11-24 03:26 EET (refreshed from the 03:22 snapshot).
**Nature:** read-only synthesis of existing docs, log mtimes/tails and running-process
names. No QEMU, build or test launch was made for this page. No source/commit touched.
`kernelhwsrv/` is READ-ONLY and untouched.
Where sources disagree, the newest reconciled evidence wins and the conflict is called
out in the caveat column. Statuses: **GREEN** = fix/verdict PASS and no newer
contradicting evidence; **IN FLIGHT** = fix landed but re-verification active, or a
root-cause fix not yet authoritatively re-run; **FAIL** = last authoritative run
returned FAIL; **NOT DONE** = no authoritative run.

## Gate table

| Gate / tool | Last verdict | Evidence (doc + log + timestamp, local EET) | Caveat |
|---|---|---|---|
| **Retire EFSRV/F32** — `r1_genuine_retire_test.py`, `r1_genuine_efsrv_test.py` | **FAIL** — honest `r=-15` | [SESSIONPATH_NEG15.md](SESSIONPATH_NEG15.md) §6; `r1_genuine_retire_test.py` **FAIL 3/4** (EFSRV/F32 FAIL; EWSRV/ESTART/EUSER PASS), strict acceptance **FAIL 5/9**; `/tmp/opencode/efile_acceptance.log`, `/tmp/opencode/sessionpath_after_stable.log` | Honest FAIL: full EFILE `SessionPath r=-15`×2, `OK`×0. Shim/minimal-server `r=0`→`OK`×2 still PASSes honestly — gate not weakened; root cause is the re-entry row below. |
| **Retire EWSRV** — `r1_genuine_retire_test.py`, `r1_genuine_ewsrv_test.py` | **GREEN** — PASS | [GO_NO_GO.md](GO_NO_GO.md) §1, [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) §1; `/tmp/opencode/r1_retire_ewsrv.log` 03:09 (`EWSRV.E32 r=0 pid=9`, registered, no fault) | Production `winsrv.c` shim still live; retirement pending stable tree. |
| **Retire ESTART/ESHELL** — `r1_genuine_retire_test.py` | **GREEN** — PASS (guard-over-block resolved) | [GUARD_ESTART_BLOCK_ANALYSIS.md](GUARD_ESTART_BLOCK_ANALYSIS.md), [GO_NO_GO.md](GO_NO_GO.md) §1; fullchain re-run: `ESTART.E32 r=0 pid=13`, `ESHELL r=0 pid=17`, four genuine in order | Caller UID3 re-stamp landed (see pack_multi row); `-11` block cleared. |
| **Retire EUSER** — `r1_genuine_retire_test.py` | **GREEN** — PASS | [GO_NO_GO.md](GO_NO_GO.md) §1, [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) §1; `e32run: EUSER.DLL resident ... exports=2503` | Genuine prebuilt DLL; static shim removal separate. |
| **Full chain** — `r1_genuine_fullchain_test.py` | **GREEN** — PASS 14/14 (stable re-run) | [FULLCHAIN_VERIFICATION.md](FULLCHAIN_VERIFICATION.md), [GO_NO_GO.md](GO_NO_GO.md) §2; fullchain re-run after marker fix: 14/14, ESTART `r=0 pid=13`, ESHELL `r=0 pid=17`, four genuine in order, 0 Exc/Panic/FAULT/#PF/port-shim, screendump PASS | Test-marker attribution fixed + re-run stable. Does not clear the EFSRV FAIL row. |
| **DOOM / Freedoom** — `r1_doom_{main,audio,playtest,multikey}_test.py` | **GREEN** — PASS | [DOOM_INTEGRATION_VERIFICATION.md](DOOM_INTEGRATION_VERIFICATION.md) §4–6; `/tmp/opencode/doom_verify_{main,audio,playtest,multikey_final}.log` | `multikey` FAIL on `diagonal_2` is a documented harness marker-attribution artifact. No re-run after 03:06–03:15 rebuilds. |
| **ESHELL `ps` / `t`** — `r1_genuine_eshell_test.py` | **GREEN** — PASS | [ESHELL_PS_STATUS.md](ESHELL_PS_STATUS.md), [GO_NO_GO.md](GO_NO_GO.md) §2; `/tmp/opencode/r1_genuine_eshell.log` (`ps` ID 5..12, `ps>q`, `E32Main returned 0`), `/tmp/opencode/r1_qemu_smoke.log` 03:15 PASS; length-bounded `%S` in `genuine/estart_eshell/shims.cpp` | PS works; UID re-stamp landed (`etshell_=0x101F7AE7`, `0x10000111` removed), eshell/retire/smoke PASS. |
| **Pointer bridge** — `r1_genuine_pointer_test.py` | **GREEN** — PROVEN | [TRAWEVENT_POS_PROOF.md](TRAWEVENT_POS_PROOF.md), [POINTER_BRIDGE_REVIEW.md](POINTER_BRIDGE_REVIEW.md); `/tmp/opencode/genuine_pointer.log` 02:30 | Review SAFE. No re-run after newest rebuild. |
| **Security** — static audit | **GREEN** — SAFE | [SECURITY_AUDIT.md](SECURITY_AUDIT.md) 02:53 (no credentials, no injection in scope) | Static only; in-scope findings false positives/out of scope. |
| **Preservation** | **GREEN** — PASS | [PRESERVATION_AUDIT.md](PRESERVATION_AUDIT.md) 03:11; branch/tag/tarball → `56fbaff`, 2,481,838 B / 983 entries | `HEAD=56fbaff`, tree dirty; tarball untracked — never `git add -A`. |
| **Docs index** | **GREEN** — PASS (146/146) | [DOCS_INDEX.md](DOCS_INDEX.md); link check **146/146** | Regenerate after this page lands. |
| **Final verify** — `r1_final_verify.py` | **NOT DONE** — no authoritative full run | [FINAL_VERIFY_REVIEW.md](FINAL_VERIFY_REVIEW.md), [FINAL_VERIFY_RUNBOOK.md](FINAL_VERIFY_RUNBOOK.md); no `/tmp/opencode/final_verify_*.log` | Runbook ready. Two known false-PASS paths — require `--strict-skip` and every individual gate PASS. |
| **`-15` integrity fix** — `genuine/f32_probe/fsclient.cpp` | **GREEN** — fix verified honest | [SESSIONPATH_NEG15.md](SESSIONPATH_NEG15.md) §3–5; marker emitted only when `r==0`, else `SessionPath FAILED` + return 3; shim `r=0`→`OK`×2 honest (`sessionpath_efsrv.log`), full EFILE `r=-15`×2→`OK`×0 honest | GREEN is about marker honesty, **not** EFSRV health — the honest full-EFILE verdict is FAIL (EFSRV row above). |
| **Trampoline final removal** (`variant/e32load.cpp`) | **GREEN** — definitively removed | [GUARD_TRAMPOLINE_REVIEW.md](GUARD_TRAMPOLINE_REVIEW.md), [EFILE_REENTRY_FIX.md](EFILE_REENTRY_FIX.md); guard-only rebuild 03:15: IPC round trip `OK`×2, port-shim=0, 0 faults, single owner=1; `trampoline_removal_gates/` 03:18 | AGENTS.md permanent **no-trampoline** rule; never reintroduce `PortE32EntryTrampoline` (port-shim=2, `#PF`, no IPC). Residual `-15` is re-entry, not the trampoline. |
| **Caller UID3 re-stamp** (`pack_multi.py --uid3`) | **GREEN** — LANDED | [GUARD_ESTART_BLOCK_ANALYSIS.md](GUARD_ESTART_BLOCK_ANALYSIS.md) §2; `estart_=0x10272C04`, `etshell_=0x101F7AE7`, dbg distinct, `0x10000111` removed, e32read OK imports 95/190/190; eshell/retire/smoke PASS | Re-stamp applied to callers/build; unblocked ESTART (r=0). |
| **EFILE re-entry suppression** (full EFILE) | **IN FLIGHT** — root-caused, shim suppression active | [SESSIONPATH_NEG15.md](SESSIONPATH_NEG15.md) §7, [EFILE_REENTRY_FIX.md](EFILE_REENTRY_FIX.md); `/tmp/opencode/efile_acceptance.log`: 3 starts / 2 `E32Main returned 0` → owning `DServer` detaches sessions → `r=-15` | Re-entry markers tolerated only if IPC works, port-shim=0, no faults — full-EFILE IPC currently fails. Shim suppression IN FLIGHT; no authoritative clean prove yet. |

**Row counts:** 16 gate rows — **13 GREEN / 1 IN FLIGHT / 1 FAIL / 1 NOT DONE**.
**Overall: NOT DONE.** (EFSRV honest FAIL `r=-15` still blocks any GO.)

## BLOCKERS (root-caused, fixes not yet authoritative)

- **EFILE re-entry** — 3 starts / 2 returns detach the `DServer` session → `r=-15`; shim suppression IN FLIGHT, no clean prove yet.
- **Strict EFILE acceptance** — honest tool FAIL 5/9; needs one clean prove with IPC round trip ≥ 1 and no `-15`.
- **Final verify** — no authoritative `--strict-skip` run; runbook ready.

**Resolved (no longer blockers):** caller UID3 re-stamp LANDED (`0x10000111` removed); full chain PASS 14/14 stable.

## IN FLIGHT (owners active — do not take over)

- **EFILE re-entry suppression** — owner of full-EFILE boot/owner path; `sessionpath_after_stable.log`.
- **Docs/publishing hygiene** — regenerate `docs/DOCS_INDEX.md` after this page lands; `GENUINE_PROGRESS.md` / `GO_NO_GO.md` / `ESHELL_PS_STATUS.md` still reconcile some items against the older tree.
- **Port-side cleanup** (lower priority, [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) §3): B3 `fs/r1_fs_backend.h` non-virtual `Backend`, EWSRV external client window, real PE2 imports for all user E32s.

## NOT DONE

- **Shim retirement** — 0 READY / 4 BLOCKED in [SHIM_RETIREMENT_PLAN.md](SHIM_RETIREMENT_PLAN.md); `r1_shim_retire_apply.py` only when every gate is PASS on a stable tree, never with any FAIL/SKIP.
- **Commit / push** — none; `HEAD=56fbaff` with a dirty tree. Explicit request required; stage by path (no `git add -A`).
- **Full `r1_final_verify.py --strict-skip` PASS** — no logs, no authoritative run (runbook ready).
- **One uninterrupted clean EFILE prove** — `SessionPath r=0`, IPC round trip ≥ 1, port-shim=0, no `-15`.

## One command sequence to decide

```bash
cd qemu-port
python3 tools/r1_final_verify.py --dry-run          # confirm the 14 commands, no QEMU (2026-11-26)
# caller UID3 re-stamp already LANDED (0x10000111 removed)
python3 tools/r1_final_verify.py --strict-skip      # stable, freshly built tree; require EVERY row PASS
python3 tools/r1_efile_acceptance.py --log <one-uninterrupted-clean-prove.log>
```

`--strict-skip` is the deciding gate because SKIP is otherwise non-fatal and the tool
has two known false-PASS paths (stale DOOM ISO, nonzero rc overridden by a printed
PASS). The EFILE acceptance run must come from one uninterrupted raw log, never from
a SIGTERM'd harness log. Even a full GO does **not** authorise commit/push or shim
retirement without the explicit conditions above.

---
*Read-only synthesis; no gate was executed for this page. Timestamps are log/doc mtimes
observed at the 2026-11-24 03:26 EET snapshot.*

---

## 2026-11-24 ~03:55 EET — authoritative final verification run

Supersedes the pre-run "no logs / no authoritative run" note above. From `qemu-port/`:
`python3 tools/r1_final_verify.py --dry-run` (exit 0), then
`python3 tools/r1_final_verify.py --strict-skip` (exit 0).
**OVERALL: PASS — 11/11 PASS, 0 SKIP, 0 FAIL** (selfcheck, qemu_smoke, genuine_retire
4/4, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main,
doom_audio, doom_playtest, doom_multikey). No Exc/Panic/KERN-EXEC/#PF/FAULT in any log;
EFSRV PASS incl. IPC round trip. Logs: `/tmp/opencode/final_verify_*.log`, orchestrator
`/tmp/opencode/final_verify_strict_20261124.log`. Full table/details:
[FINAL_VERIFICATION_RESULT.md](FINAL_VERIFICATION_RESULT.md). Verify-only; kernelhwsrv/
untouched, no commit/push, no shim retirement (still needs explicit authorisation).

---

## 2026-11-25 — final post-winsrv-deletion gate verdict

- **Commit:** `94ce9c9` pushed (`0f03f7e..94ce9c9`) — winsrv/GUI desktop retired
  (winsrv.c, guishost.h, wsclient.*, qemu_windowsrv.*, qemu_gui_exec.cpp deleted).
- **Authoritative gate:** the P3 `r1_final_verify.py --strict-skip` **11/11 PASS**
  recorded in the `94ce9c9` commit message (apply gates P1–P3 PASS).
- **Redundant re-run** (`/tmp/opencode/final_verify_post_winsrv_delete.log`):
  **10/11**, sole FAIL `qemu_smoke` at the 50s harness cap under QEMU contention
  (load 3.2–4.6, exit 124) — harness timeout, not a regression; all other tests
  PASS incl. retire/efsrv/ewsrv/eshell/fat_io and DOOM ×4.
  Smoke re-run result pending — see `r1_qemu_smoke_rerun.log`.
- `kernelhwsrv/` clean/untouched (read-only, verified via git status).

---

## 2026-11-25 — isolated qemu_smoke re-run (contention question closed)

- Isolated `r1_qemu_smoke.py` re-run on the free QEMU slot → **RESULT: PASS, RC=0**
  (`/tmp/opencode/r1_qemu_smoke_rerun.log`, load avg 1.15).
- The 22:40 run's smoke FAIL was **QEMU-slot contention** (exit 124 at the 50s cap
  under load 3.2–4.6), **not a regression**.
- **Effective post-deletion gate: 11/11** — apply P3 gate 11/11 PASS (commit
  `94ce9c9`), re-run 10/11 + smoke PASS in isolation.
- HEAD origin/main = `94ce9c9`.

---

## 2026-11-25 ~20:05 EET — P0 baseline gate (PLAN_OVERNIGHT_WAVE1)

- **Hygiene:** stray 0-byte `qemu-port/9` deleted; root `.gitignore` appended
  with `/qemu-port/9`. Freeze list recorded in
  [KERNEL_HOOK_FREEZE.md](KERNEL_HOOK_FREEZE.md) (exec 81/269/270 +
  `GenuineBootLaunch()` boot contract FROZEN). No commit/push; `kernelhwsrv/`
  porcelain empty (READ-ONLY honored). No WATCH move, no `make clean`.
- **Environment note:** this shell's `LD_LIBRARY_PATH` (Vibe `_internal`) broke
  gcc/g++ (GLIBCXX errors); all gates re-run with it unset. First strict run also
  missed the `ca_test.e32` fixture — built via `tools/mke32_ca_test.py`
  (`/tmp/opencode/ca_test.e32`) before the isolated re-run.
- **Gate runs** (each serialized with `flock qemu-port/.sybian/qemu-slot.lock`,
  stable uninterrupted, no exit-124):
  - `r1_genuine_retire_test.py` → **RESULT: PASS 4/4** (efsrv/ewsrv/estart/euser),
    exit 0 — `/tmp/opencode/p0_genuine_retire.log`.
  - `r1_genuine_fullchain_test.py` → **RESULT: PASS 14/14**, exit 0 —
    `/tmp/opencode/p0_genuine_fullchain.log`.
  - `r1_final_verify.py --strict-skip` isolated re-run → **7/11 PASS, 4 SKIP**,
    tool verdict FAIL under `--strict-skip` (exit 1) —
    `/tmp/opencode/p0_final_verify_strict_skip_rerun.log`. PASS rows: selfcheck
    (235s, incl. cp4_ca with the rebuilt fixture), qemu_smoke, genuine_retire,
    genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io. The 4 SKIPs are
    doom_main/audio/playtest/multikey. **Root cause corrected:** the Freedoom IWAD
    was **wiped from `/tmp` during a machine cleanup** — the DOOM logs show the ISO
    auto-recovery failing with `mcopy: File "::FDOOM1.WAD" not found` and only then
    the harness's `[SKIP] Freedoom IWAD not present ... expected during staging`
    notice (`/tmp/opencode/final_verify_doom_{main,audio,playtest,multikey}.log`).
    The asset existed and was lost to the filesystem, not merely absent by design,
    so this was an environment loss, not a code regression.
- **P0 gate verdict: NO-GO on the strict row** (11/11 not reached — only the 4
  asset-absent DOOM SKIPs stand between 7/11 and 11/11); **retire 4/4 GO,
  fullchain 14/14 GO**, both stable under the slot lock. Per the wave rule the
  first NO-GO stops the wave; blocker is external and transient (the IWAD was
  wiped from `/tmp` by a machine cleanup — since restored), not a code regression.

---

## 2026-11-25 20:37 EET — P0 recovery re-run (WAD restored): 11/11 PASS

- **Verdict: OVERALL PASS — 11 pass, 0 skip, 0 fail/not-run** —
  `/tmp/opencode/final_verify_recovery.log`. All 4 DOOM rows now PASS
  (main 75.7s, audio 33.5s, playtest 70.5s, multikey 64.4s) after the Freedoom
  IWAD was restored at **28,795,076 B** (`/tmp/opencode/doom/freedoom-0.13.0/freedoom1.wad`).
  The P0 NO-GO is cleared; the blocker was the wiped `/tmp` asset, not code.
  Gate run by the recovery owner (PID 40967); this line only records its verdict —
  no QEMU gate, build or `make clean` was started for this doc. `kernelhwsrv/`
  READ-ONLY, untouched; no commit/push.

---

## 2026-11-25 ~20:38 EET — P0-RECOVERY: Freedoom IWAD restored, strict gate re-run

- **Precondition restored:** `/tmp/opencode/doom/freedoom-0.13.0/` had been wiped
  by machine /tmp cleanup, which made the 4 DOOM gates SKIP (strict NO-GO).
  Recovered `freedoom1.wad` (FDOOM1.WAD, 28,795,076 B, `IWAD` magic verified)
  from our own built artifacts with local tools only (7z + mtools, no
  downloads): the current `build/symbian-grub.iso` was built post-wipe so its
  `programs.img` lacks the WAD; the copy was extracted from the pre-wipe
  FAT volume in `/tmp/opencode/freedm_recovered.img` (staged by the project's
  own harness) via `mcopy -i <img>@@1048576 ::FDOOM1.WAD`. FREEDM.WAD was not
  recoverable from any local artifact (not staged in any ISO/img on disk).
- **Gate re-run:** `python3 tools/r1_final_verify.py --strict-skip` under
  `flock qemu-port/.sybian/qemu-slot.lock`, `LD_LIBRARY_PATH` unset, log at
  `/tmp/opencode/final_verify_recovery.log`.
- **Result: OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run)** — the 4 DOOM
  gates (doom_main 75.7s, doom_audio 33.5s, doom_playtest 70.5s,
  doom_multikey 64.4s) now **PASS**, no longer SKIP. P0 strict blocker cleared.
- No source/Makefile/tools/.gitignore edits; no commit/push; no `make clean`;
  `kernelhwsrv/` untouched.

---

## 2026-11-25 22:52 EET — correction pass: stale claims in the snapshot above

Append-only correction. The 2026-11-24 03:26 snapshot at the top of this page is
**historical**; every claim corrected below was re-checked with read-only commands
(`sed`/`grep`/`find`/`git rev-parse`, `git status`, `ls`, `tar tzf`). No QEMU, no
build, no `make clean`, no commit, no push; `kernelhwsrv/` READ-ONLY and untouched.

| # | Stale claim (line above) | Verified current fact |
|---|---|---|
| 1 | line 3 `**Snapshot:** 2026-11-24 03:26 EET` | Header is stale: this file now carries verdicts through **2026-11-25 ~20:38 EET** (sections at lines 159–190), the newest evidence being `/tmp/opencode/final_verify_recovery.log`, mtime 2026-11-25 20:37. Read the newest section first, not the header. |
| 2 | line 17 **Retire EFSRV/F32 = FAIL `r=-15`**; lines **34–35** `16 gate rows — 13 GREEN / 1 IN FLIGHT / 1 FAIL / 1 NOT DONE`, `Overall: NOT DONE` | Superseded by `/tmp/opencode/final_verify_recovery.log`: `OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run)`, including `-- genuine_efsrv: PASS (14.9s)` (its per-gate log tail: `F32CLIENT: IPC round trip OK`, `EUSER ord 915 bound (2 images)`, `no Exc/PanicKernExec/FAULT`). No six-row distribution applies any more; the roll-up is one strict full run, 11/11 PASS. |
| 3 | line 28 `no /tmp/opencode/final_verify_*.log` | False. `final_verify_recovery.log` exists (3,250 B, 20:37) alongside per-gate `final_verify_*.log` files — 13 `final_verify_*.log` in `/tmp/opencode`. What is true: older logs cited by the table were removed from `/tmp` — `efile_acceptance.log`, `sessionpath_after_stable.log`, `sessionpath_efsrv.log`, `genuine_pointer.log` and `doom_verify_{main,audio,playtest,multikey_final}.log` are absent (8 cited paths); `r1_retire_ewsrv.log`, `r1_genuine_eshell.log`, `r1_qemu_smoke.log` survive. Those rows now rest on surviving docs, not on their logs. |
| 4 | line 18 caveat `Production winsrv.c shim still live` | False. `find . -name winsrv.c -o -name guishost.h -o -name 'wsclient.*' -o -name 'qemu_windowsrv.*' -o -name qemu_gui_exec.cpp` returns **nothing**; the files were deleted in `94ce9c9` (commit subject names them) and the stale build wiring dropped in `ee8490e`. The EWSRV gate now exercises genuine EWSRV. |
| 5 | line 27 `Docs index — PASS (146/146)` | Stale. `docs/DOCS_INDEX.md` line 4 self-reports `725 files, verified 2026-11-25`, and `ls docs/*.md | wc -l` = **725**. Use 725/725. |
| 6 | lines 54 / 118 `HEAD=56fbaff`, `HEAD origin/main = 94ce9c9` | Both stale. `git rev-parse HEAD` = `git rev-parse origin/main` = **`ee8490e`** (2026-11-25 16:17 +0200). Working tree is **dirty**: 234 `git status --porcelain` entries, including the S1 working-tree deletion `qemu-port/fs/r1_fs_core.mmp`. Preservation itself is intact and not stale: `/home/yuzi/Documents/Sybian REAL/symbian-x86-archive-56fbaff.tar.gz` is on disk at exactly **2,481,838 B / 983 entries** (the size/entry count line 26 cites), and `56fbaff` is a commit that is an ancestor of HEAD on branch/tag `archive/handwritten-r25`. Only the HEAD pointers were wrong — the preserved artifact survives. |

Consequences for anyone reading the snapshot above: the EFSRV `-15` blocker is
closed on evidence (row 2), the `winsrv.c` caveat no longer exists (row 4), and the
page's headline `Overall: NOT DONE` is superseded. Rows 1, 3, 5 and 6 are
bookkeeping drift to be fixed in place next time this page is regenerated.
Nothing in the snapshot above was rewritten; it is preserved as the 03:26 record.

---

**2026-11-26 — correction:** the gate table at the top of this page and the 2026-11-25 22:52 correction section are both superseded by the authoritative strict run in `/tmp/opencode/final13_gate.log` (mtime 2026-11-26 00:03 EET): **13 rows, all PASS** — `OVERALL: PASS (13 pass, 0 skip, 0 fail/not-run)` with `FINAL13_RC=0`, and both `g12_residue` PASS (1.5s) and `missing_statics` PASS (7.4s) are among the 13 green rows; recorded after S1+S2+S3 landed and the G12+W5 wiring, at HEAD `ee8490e`, with no commits made and `kernelhwsrv/` clean.

**2026-11-26 — correction (snapshot table):** the 2026-11-24 03:26 snapshot table at the top of this page, including its **Retire EFSRV/F32 = FAIL `r=-15`** row (16 rows by that table's own line 34), is superseded by the 13-row gate run recorded in `/tmp/opencode/final13_gate.log`: **PASS 13/13**, `OVERALL: PASS  (13 pass, 0 skip, 0 fail/not-run)`, `FINAL13_RC=0`.

**2026-11-26 — correction (authoritative strict gate, supersedes the two 13-row paragraphs directly above):** the authoritative strict gate is now **14 rows, all PASS**. Both `/tmp/opencode/final14_gate.log` and `/tmp/opencode/final15_gate.log` end identically: `OVERALL: PASS  (14 pass, 0 skip, 0 fail/not-run)` with `FINAL14_RC=0` and `FINAL15_RC=0`. The 14th row is `g12_negative` — the W7 falsifiability probe, PASS in both runs (0.8s FINAL14, 0.4s FINAL15; its log: "sweep fails on a seeded retired r1_fs file and passes on a clean tree"). W1 advisory cleanup landed: the `PORTFILESRV.E32` dead literals are removed from `user/fcopy.c` and `user/filemgr.c` (`grep -n PORTFILESRV` on both files returns no match), which takes the residue sweep from its FINAL14 state `ADVISORY (2) — live code names a retired artifact, no build effect (not residue)` to FINAL15's `RESULT: PASS (0 residue, 43 allowed comments)` with **no advisory line at all** — advisory count 2 to 0. Recorded at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` clean (`git status --porcelain` empty).

**2026-11-26 — correction (post-cleanup relink; supersedes the 13-row and 14-row paragraphs above):** the authoritative gate after the inert-WINSRV cleanup and relink is the 14-row run in `/tmp/opencode/final16_gate.log`: `OVERALL: PASS  (14 pass, 0 skip, 0 fail/not-run)` with `FINAL16_RC=0`, every row green — `g12_residue` 1.5s, `g12_negative` 0.4s, `missing_statics` 6.8s, `selfcheck` 233.9s, `qemu_smoke` 50.1s, `genuine_retire` 56.2s, `genuine_efsrv` 16.0s, `genuine_ewsrv` 17.6s, `genuine_eshell` 20.1s, `genuine_fat_io` 14.9s, `doom_main` 75.7s, `doom_audio` 33.5s, `doom_playtest` 71.4s, `doom_multikey` 64.6s. WINSRV is absent from source and binary: `grep -c -i winsrv variant/kern_shell.cpp` = 0 and `nm build/kernfull.elf | grep -c -i winsrv` = 0. The ISO carries that WINSRV-free kernel — `build/grubwork/iso/boot/kernfull.elf` is byte-identical to `build/kernfull.elf` (`cmp` clean; both 1,007,372 B; nm and string grep for winsrv = 0). Sweep: `RESULT: PASS (0 residue, 43 allowed comments)`. Recorded at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` clean (`git status --porcelain` empty).

**2026-11-26 — correction (post C2/C3 + HAL veneer; supersedes the FINAL16 paragraph above):** the gate now stands at the two 14-row runs `/tmp/opencode/final17_gate.log` and `/tmp/opencode/final18_gate.log`, both ending `OVERALL: PASS  (14 pass, 0 skip, 0 fail/not-run)` with `FINAL17_RC=0` and `FINAL18_RC=0` respectively. FINAL17 records the state after the C2 selftest-feed change and the C3 winsrv self-skip harness change (per-row times differ from FINAL16 only by noise: `selfcheck` 234.3s, `genuine_retire` 44.7s, `genuine_efsrv` 14.5s, `genuine_ewsrv` 17.3s, `genuine_eshell` 20.1s, `genuine_fat_io` 14.7s, `doom_playtest` 70.8s). FINAL18 records the state after the HAL veneer: the two `UserSvr::HalFunction` stubs in `genuine/hal/hal_glue.cpp` (lines 164-168) now forward to `Exec::HalFunction` instead of returning `KErrNotSupported`, the tree was relinked, and no new exec slots were introduced. Caveat: the HAL forwarding behaviour itself is **not yet covered by a gate row** — no row in either run asserts a HAL call round-trip, so a dedicated HAL test is still owed before that change can be called gate-verified. Shared evidence for both runs: sweep `RESULT: PASS (0 residue, 43 allowed comments)`, WINSRV absent from source (`grep -c -i winsrv variant/kern_shell.cpp` = 0) and from the ELF (`nm build/kernfull.elf | grep -c -i winsrv` = 0), HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` clean (`git status --porcelain` empty).

**2026-11-26 — the HAL caveat in the paragraph directly above is now closed:** the manual HAL probe passes. `tools/r1_genuine_hal_test.py` builds the `hal_smoke` exe twin with `-DHAL_PROBE_SCREENINFO` and boots it against the genuine HALU; `/tmp/opencode/hal_probe.log` (22,857 B, mtime 2026-11-26 01:10) ends `RESULT: PASS` / `HAL_PROBE_RC=0`, with every HALPROBE field assert green — `r == 0 (KErrNone)` (got 0), `addr == 0xFD000000` (got fd000000), `w == 1024` (got 1024), `h == 768` (got 768), `EUSER ord 915 bound by loader`, and no `Exc '`/`PanicKernExec`/`FAULT:` line anywhere in the serial log. That is a real in-guest round-trip through the veneer, so the `UserSvr::HalFunction` -> `Exec::HalFunction` forwarding in `genuine/hal/hal_glue.cpp` is proven end-to-end via genuine HALU rather than merely relinked. Scope note, unchanged: this is a **manual harness**, not a `DEFAULT_TESTS` row — the strict gate remains **14 rows**, and no row was added for it in this change.

**2026-11-26 — final19 gate (after the manual HAL probe harness and HAL E32s rebuilt with HALPROBE):** `/tmp/opencode/final19_gate.log` (mtime 2026-11-26 01:21) ends `OVERALL: PASS  (14 pass, 0 skip, 0 fail/not-run)` with `FINAL19_RC=0`; every row green — `g12_residue` 1.7s, `g12_negative` 0.4s, `missing_statics` 7.3s, `selfcheck` 233.9s, `qemu_smoke` 50.1s, `genuine_retire` 45.0s, `genuine_efsrv` 15.1s, `genuine_ewsrv` 17.3s, `genuine_eshell` 20.3s, `genuine_fat_io` 14.7s, `doom_main` 75.7s, `doom_audio` 33.9s, `doom_playtest` 70.9s, `doom_multikey` 64.6s. The HAL probe is green as a **manual harness, not a DEFAULT_TESTS row**: `/tmp/opencode/hal_probe.log` (22,857 B, mtime 2026-11-26 01:10) ends `RESULT: PASS` / `HAL_PROBE_RC=0` with `[PASS] HALPROBE line printed` and every field assert green — `r == 0 (KErrNone)` (got 0), `addr == 0xFD000000` (got fd000000), `w == 1024` (got 1024), `h == 768` (got 768). Sweep: `RESULT: PASS (0 residue, 43 allowed comments)`. Recorded at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` clean (`git status --porcelain` empty).

**2026-11-26 — H1 landed (HAL probe registered as a gate row; 15-row authoritative gate):** the manual HAL probe gained a **SKIP-on-missing-artifact** path and was promoted into the strict gate. `tools/r1_genuine_hal_test.py` now checks `skip_reason()` before the tool probe and before any disk or QEMU work (lines 244-249), prints `[SKIP] <reason>` / `RESULT: SKIP` and exits **rc 0** when the `hal_smoke`/HALU/EUSER artifact is absent; only a missing artifact skips, and a boot or assertion failure is still FAIL (docstring lines 8-11). The skip is deliberately **fatal under `--strict-skip`** — `r1_final_verify.py:157-159` treats `SKIP` as a failure when that flag is set (`is_failure` returns `verdict in BAD or (strict_skip and verdict == "SKIP")`), so a missing image can no longer fake green. It is registered as **DEFAULT_TESTS row 15**, `Test("hal_probe", "tools/r1_genuine_hal_test.py", [], 240)` at `tools/r1_final_verify.py:143`. The authoritative gate is therefore now **15 rows**: `/tmp/opencode/final20_gate.log` ends `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` with `FINAL20_RC=0`, and the new row ran green rather than skipped — `hal_probe PASS 14.1s` (`/tmp/opencode/final20_gate/final_verify_hal_probe.log`, `RESULT line: PASS`). The `UserSvr::HalFunction` -> `Exec::HalFunction` forwarding in `genuine/hal/hal_glue.cpp` is now **gate-covered AND manually proven**, closing the FINAL18 caveat and the "manual harness only" scope note in the FINAL19 paragraph above. Sweep: `RESULT: PASS (0 residue, 43 allowed comments)`. Recorded at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` clean (`git status --porcelain` empty).

**2026-11-26 — supersession notice (read this before any 14-row claim above):** every prior statement in this footer that calls a **14-row** gate "authoritative" — line 224 (`the authoritative strict gate is now **14 rows, all PASS**`, FINAL14/FINAL15), line 226 (`the authoritative gate after the inert-WINSRV cleanup and relink is the 14-row run`, FINAL16), line 228 (the two 14-row runs FINAL17/FINAL18), and line 230 (the `the strict gate remains **14 rows**` scope note) — is **superseded** by the **15-row** FINAL20 entry directly above this line. The authoritative gate is **15 rows**: `/tmp/opencode/final20_gate.log` ends `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` with `FINAL20_RC=0`, row 15 being `hal_probe` (`tools/r1_final_verify.py:143`, PASS 14.1s, not skipped). Those earlier paragraphs remain in place as dated history of how the row count grew 13 -> 14 -> 15; they are not current claims. No line was edited in this change, at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` untouched.

**2026-11-26 — correction (SKIP is now fatal by default; supersedes the lines 30, 43, 57 and 70 claims that `--strict-skip` is the deciding gate):** every statement above that makes `--strict-skip` the deciding flag because "SKIP is otherwise non-fatal", or that reads the gate as "require `--strict-skip`", is **stale**. In `tools/r1_final_verify.py:475` the shipped logic is `skip_fatal = args.strict_skip or not args.allow_skip`: **SKIP is fatal by default**, only `--allow-skip` makes it advisory, and `--strict-skip` still works and wins over `--allow-skip` (it is now the redundant-but-supported form kept for old command lines, not the deciding gate). Line 30 ("Final verify — NOT DONE", requiring `--strict-skip` and every individual gate PASS), line 43 and line 57 ("no authoritative `--strict-skip` run") therefore no longer describe a stricter mode than the default — a plain default run is already the strict one, so the outstanding work is one authoritative **default** full run, not a `--strict-skip` one; line 70 ("`--strict-skip` is the deciding gate because SKIP is otherwise non-fatal") is superseded outright. The flip is proven by `python3 tools/r1_verify_skip_negative_test.py`, which exits 0 with `RESULT: PASS` across all 8 seeded cases (`default_skip_is_fatal`, `allow_skip_advisory`, `strict_beats_allow_skip`, `strict_skip_still_fatal`, `pass_unchanged`, `fail_unchanged`, …) against the real verifier (sha256 a00f431c4e7639df, roster 15 rows; no QEMU, no make, no ISO, temp dir only). The 15-row / FINAL20 status in the footer below is unaffected; this correction touches only the SKIP-fatality claim. Appended only, no body line rewritten; at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` untouched.

**2026-11-26 — post-B4, post-cp4fix gate (authoritative):** the post-B4, post-cp4fix gate is authoritative — `/tmp/opencode/final21_gate.log` (run2, started after the cp4fix parser patch) ends `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` with `FINAL21_RC=0`, and `selfcheck` PASS (234.1s) carries `cp4_stress` PASS (32.7s) plus `hal_probe` PASS (14.7s). Attempt 1 (`FINAL21_RC=1` in `/tmp/opencode/final21_gate_attempt1.log`) was a harness parse race, not a product failure: the `id=15` row was torn by interleaved `CPSTRESS` progress output while all 4 clients were alive. `cp4fix` (`strip_progress_noise` + `parse_ps_ids` in `tools/r1_cp4_ca_stress_test.py:139-160`) strips each progress line with its own terminator so torn rows rejoin before matching, and it is validated non-vacuous (3-client logs still FAIL). Sweep `RESULT: PASS (0 residue, 43 allowed comments)`, at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` clean. Appended only, no earlier line rewritten.

**2026-11-26 — GATE-PORTFILESRV-DECOUPLE landed (authoritative 15-row gate, no out-of-repo cache dependency):** `/tmp/opencode/final23_gate.log` ends `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` with `FINAL23_RC=0`. The `qemu_smoke` row is PASS 50.1s and now uses the repo-built `build/user/GUISHELL.E32` plus the tracked root `EUSER.DLL`, with **no `~/.cache` fixture** (`tools/r1_qemu_smoke.py:17` states no path under `~/.cache` or `/tmp` is used for the payload); its markers are bound from the image headers rather than a cache contract — bind slots 32768..32784 (ords 211, 1547, 120, 915, 1548), `imports bound dll (5 ordinals)`, `exports=2503`, `E32Validate: expDesc tail OK`, and `no Exc or PanicKernExec in log`. The authoritative smoke row therefore no longer depends on the out-of-repo retired **portfilesrv** cache. Residual (pre-existing, outside this patch): `tools/r1_genuine_ewsrv_test.py` still cache-stages portfilesrv — `PORTFILESrv = "~/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv"` at line 39, staged at lines 95 and 115-118, so that row still reads a `~/.cache` artifact. Sweep: `RESULT: PASS (0 residue, 43 allowed comments)`. Recorded at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` clean (`git status --porcelain` empty). Appended only; no earlier line rewritten except the line-3 banner token `FINAL21_RC=0` -> `FINAL23_RC=0`.

**2026-11-26 — GATE-EUSER-TRACKED landed (authoritative 15-row gate, gate scripts off the cache euser.dll):** seven genuine gate scripts were repointed from the cached `~/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll` to the byte-identical tracked root `EUSER.DLL` (sha256 `4851fec4b6439bf5263e6f10f7f3010828f84df55df050c710f57077f2dd017b`; `sha256sum` of the cache copy and the tracked file agree, so the payload is unchanged): `tools/r1_genuine_efsrv_test.py`, `tools/r1_genuine_eshell_test.py`, `tools/r1_genuine_ewsrv_test.py`, `tools/r1_genuine_fat_io_test.py`, `tools/r1_genuine_fullchain_test.py`, `tools/r1_genuine_retire_test.py` and `tools/r1_qemu_smoke.py` all now resolve `EUSER.DLL` under the repo (`REPO + "/EUSER.DLL"` / `PORT / "EUSER.DLL"` / `os.path.join(REPO, "EUSER.DLL")`). `/tmp/opencode/final24_gate.log` ends `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` with `FINAL24_RC=0`, and all 15 rows are green — `g12_residue` 1.5s, `g12_negative` 0.4s, `missing_statics` 7.0s, `selfcheck` 233.9s, `qemu_smoke` 50.1s, `genuine_retire` 45.7s, `genuine_efsrv` 15.5s, `genuine_ewsrv` 17.5s, `genuine_eshell` 20.8s, `genuine_fat_io` 15.1s, `doom_main` 75.7s, `doom_audio` 34.3s, `doom_playtest` 71.5s, `doom_multikey` 65.1s, `hal_probe` 14.7s; `qemu_smoke` and `hal_probe` both end `RESULT: PASS` with `[PASS] EUSER ord 915 bound by loader`. Residual (pre-existing, non-roster, gate unaffected): `tools/r1_genuine_input_test.py:60` and `tools/r1_genuine_pointer_test.py:48` still hold the stale cache constant `EUSER_DLL = "~/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"`; neither script is in `DEFAULT_TESTS`, so no gate row reads the cache. Sweep: `RESULT: PASS (0 residue, 43 allowed comments)`. Recorded at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` clean (`git status --porcelain` empty). Appended only; the single line-3 banner token `FINAL23_RC=0` -> `FINAL24_RC=0` is the only earlier text changed.

**2026-11-26 — GATE-EWSRV-CACHE-DECOUPLE landed (authoritative 15-row gate, last `~/.cache` dependency in a roster script removed):** `tools/r1_genuine_ewsrv_test.py` no longer stages or asserts anything from the retired portfilesrv cache. The inert `PORTFILESrv = "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv"` constant is deleted, the `(PORTFILESrv, "portfilesrv")` tuple is gone from the FAT staging loop in `build_disk()` (now `((E32, "EWSRV.E32"), (CLIENT, "EWCLIENT.E32"), (EUSER_DLL, "EUSER.DLL"))`), the `"portfilesrv"` token is gone from the `mdir` assertion list (now `("ewsrve32", "ewcliente32", "euserdll")`), and the staging message now reads `FAT OK: EWSRV.E32 + EWCLIENT.E32 + EUSER.DLL staged` instead of also naming `portfilesrv` — every edit is provably inert, since the retired artifact was copied and asserted but never launched or read by the test. `EUSER_DLL` moves from the `~/.cache/sybian-r1/r1-build-v1/.../euser.dll` path to the tracked `PORT / "EUSER.DLL"`, so the row reads no out-of-repo payload at all. Targeted run: PASS — all 6 `MARKERS` and all 9 `CLIENT_MARKERS` present (`[INFO] boot-time EWSRV markers present at byte 18410`, `[INFO] client markers present at byte 21824`), `[PASS] 232 PE2 EUSER imports bound (expected >= 212)`, `[PASS] no guest exception/panic/FAULT`; the `--only genuine_ewsrv` run PASSes in **16.8s** (`/tmp/opencode/ewsrv_decouple_only.log`: `genuine_ewsrv PASS 16.8s`, `OVERALL: PASS (1 pass, 0 skip, 0 fail/not-run)`), and `grep -n "\.cache/sybian-r1" tools/r1_genuine_ewsrv_test.py` returns **0 matches**. The authoritative gate after this change is `/tmp/opencode/final25_gate.log`, ending `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` with `FINAL25_RC=0`, every row green — `g12_residue` 1.5s, `g12_negative` 0.4s, `missing_statics` 6.6s, `selfcheck` 234.0s, `qemu_smoke` 50.1s, `genuine_retire` 43.9s, `genuine_efsrv` 14.7s, `genuine_ewsrv` 16.9s, `genuine_eshell` 19.9s, `genuine_fat_io` 14.5s, `doom_main` 75.7s, `doom_audio` 33.5s, `doom_playtest` 70.7s, `doom_multikey` 64.4s, `hal_probe` 13.9s. Recorded at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` clean (`git status --porcelain` empty). Appended only; the single line-3 banner token `FINAL24_RC=0` -> `FINAL25_RC=0` is the only earlier text changed.

**2026-11-26 — SELFCHECK-EUSER-TRACKED landed (authoritative 15-row gate; the last cache-less-checkout gate FAILs are closed):** the two remaining roster-reachable scripts that still hardcoded the out-of-repo cached `euser.dll` are repointed to the tracked root `EUSER.DLL`: `tools/r1_selftest.py:37` is now `DLL = os.path.join(ROOT, "EUSER.DLL")` (was `"/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"`) and `tools/r1_cp4_ca_test.py:39` is now `EUSER_DLL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "EUSER.DLL")` (was the same cache path). The payload is byte-identical, so nothing in the executed image changes: `sha256sum qemu-port/EUSER.DLL` = `4851fec4b6439bf5263e6f10f7f3010828f84df55df050c710f57077f2dd017b`, matching the cache copy, and `git ls-files --error-unmatch qemu-port/EUSER.DLL` confirms the file is tracked rather than an ignored build product. This closes the last cache-less-checkout gate FAILs: `selfcheck` runs both `cp4_ca` (`tools/r1_cp4_ca_test.py`, PASS 60.6s) and `selftest` (`tools/r1_selftest.py`, PASS 90.1s), so on a checkout with no `~/.cache/sybian-r1` these two sub-rows previously died on a missing fixture path and are now cache-free. `/tmp/opencode/final26_gate.log` ends `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` with `FINAL26_RC=0`, and all 15 rows are green — `g12_residue` 1.5s, `g12_negative` 0.4s, `missing_statics` 7.4s, `selfcheck` 233.9s, `qemu_smoke` 50.1s, `genuine_retire` 44.0s, `genuine_efsrv` 14.7s, `genuine_ewsrv` 16.8s, `genuine_eshell` 19.9s, `genuine_fat_io` 14.7s, `doom_main` 75.7s, `doom_audio` 33.3s, `doom_playtest` 70.7s, `doom_multikey` 64.8s, `hal_probe` 13.9s; the `selfcheck` row's own summary reads `OVERALL: PASS` at 233.9s total. Non-roster residue, unchanged by this patch and gate-irrelevant: `tools/r1_genuine_input_test.py:60` and `tools/r1_genuine_pointer_test.py:48` still carry the stale cache constant but appear in no `DEFAULT_TESTS` row. Recorded at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` untouched (`git status --porcelain kernelhwsrv/` empty). Appended only; the single line-3 banner token `FINAL25_RC=0` -> `FINAL26_RC=0` is the only earlier text changed.

**2026-11-26 — PORTABILITY-CLEANUP landed (kernel path now `__file__`-derived; tracked-path resolution for the selfcheck chain complete):** `tools/r1_cp4_ca_test.py:40` now reads `DEFAULT_KERNEL = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build", "kernfull.elf")`, replacing the hardcoded absolute `"/home/yuzi/Documents/Sybian REAL/qemu-port/build/kernfull.elf"`. Together with the `EUSER_DLL` repoint recorded in the row above, every fixture path this script resolves is now relative to the tracked checkout: `git diff qemu-port/tools/r1_cp4_ca_test.py` shows exactly those two lines changed, no absolute host path and no `~/.cache/sybian-r1` reference remains in the file, and `grep -n "/home/yuzi" tools/r1_cp4_ca_test.py` returns **0 matches**, so the `cp4_ca` sub-row of `selfcheck` is relocation-safe and dies on no host-specific path. The executed image is unchanged — only how the paths are spelled moved. `/tmp/opencode/final27_gate.log` ends `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` with `FINAL27_RC=0`, and all 15 rows are green — `g12_residue` 1.6s, `g12_negative` 0.5s, `missing_statics` 7.3s, `selfcheck` 234.1s, `qemu_smoke` 50.1s, `genuine_retire` 44.4s, `genuine_efsrv` 15.1s, `genuine_ewsrv` 17.0s, `genuine_eshell` 19.7s, `genuine_fat_io` 14.7s, `doom_main` 75.7s, `doom_audio` 33.5s, `doom_playtest` 70.9s, `doom_multikey` 64.4s, `hal_probe` 14.1s. Recorded at HEAD `ee8490e`, **no commits made**, `kernelhwsrv/` untouched (`git status --porcelain kernelhwsrv/` empty). Appended only; the single line-3 banner token `FINAL26_RC=0` -> `FINAL27_RC=0` is the only earlier text changed.
