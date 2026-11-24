# CLEANUP_AUTHORITY_CHECK — authoritative clean-tree verdict (attempt of 2026-11-24 19:07)

- **Run window:** 2026-11-24 19:07:47 → 19:19:48 EET (serial, background, one-shot quiet check at 19:07:42).
- **HEAD at launch:** `e29a072`; **HEAD at report time:** `dd22ae2` ("Retire svcmgr and drop r1_fs kernel glue") — **HEAD advanced mid-run at 19:11:24** by a concurrent agent.
- **kernelhwsrv/:** clean at every checkpoint (`git status --porcelain -- kernelhwsrv` = 0 lines; no file under `kernelhwsrv/` newer than 2026-11-24 18:00). Untouched by this run.
- **Verdict: NOT AUTHORITATIVE.** The captured strict run is `OVERALL: FAIL (9 pass, 0 skip, 2 fail/not-run)` and the tree did **not** stay settled during the capture window (see "Race evidence"). No trustworthy 11/11 on the current tree can be claimed from this run. See "Quiescence criteria" for the next attempt.
- Prior certification retained at the bottom: 2026-11-24 17:01 run certified the `d26fade` tree only.

## Commands run (from `qemu-port/`, serial, single background chain)

1. `python3 tools/r1_final_verify.py --strict-skip --log-dir /tmp/opencode/authority_20261124`
   (stdout: `/tmp/opencode/authority_final_verify.log`, `FV_EXIT=1`)
2. `python3 tools/r1_genuine_fullchain_test.py` (stdout: `/tmp/opencode/authority_fullchain.log`, `FC_EXIT=0`)
3. `python3 tools/r1_genuine_retire_test.py` (stdout: `/tmp/opencode/authority_retire.log`, `RT_EXIT=0`)

Per-test logs: `/tmp/opencode/authority_20261124/final_verify_<name>.log` (dedicated dir, so the
18:46-era evidence logs in `/tmp/opencode/` were not clobbered by this run).

## Captured OVERALL

```
OVERALL: FAIL  (9 pass, 0 skip, 2 fail/not-run)
failed: selfcheck, genuine_ewsrv
```

Companion gates (concluded cleanly in the same serial chain):

- `r1_genuine_fullchain_test.py`: **RESULT: PASS (14/14 rows)** — EUSER 2503 exports, EFILE/ESTART/EWSRV/ESHELL markers, "genuine file server up (no fallback)", svcmgr/portfilesrv not used, 0 fault lines.
- `r1_genuine_retire_test.py`: **RESULT: PASS (4 passed, 0 failed)** — efsrv (port-shim=0, F32CLIENT IPC OK), ewsrv (212 imports bound), estart, euser (2503 exports). Ran 19:19:00–19:19:48, i.e. **on the post-19:18:50 `kernfull.elf`** (post-`dd22ae2` kernel).

## Per-test table — `r1_final_verify.py --strict-skip` (11 rows, run 19:07:47–19:18:26)

| # | test | verdict | time | log | failure cause |
|---|------|---------|------|-----|---------------|
| 1 | selfcheck | **FAIL** | 225.4s | `authority_20261124/final_verify_selfcheck.log` | inner `cp4_stress` assertion `cycle 1: done ids [1,2,3] != survivors [1,3,4]`; all other 4 steps (build, isogrub, qemu_smoke, cp4_ca, selftest) PASS; 0 fault lines |
| 2 | qemu_smoke | PASS | 50.1s | `..._qemu_smoke.log` | RESULT: PASS |
| 3 | genuine_retire | PASS | 52.0s | `..._genuine_retire.log` | RESULT: PASS |
| 4 | genuine_efsrv | PASS | 17.2s | `..._genuine_efsrv.log` | PASS |
| 5 | genuine_ewsrv | **FAIL** | 0.7s | `..._genuine_ewsrv.log` | **compile error on a mid-edit file**: `edisp_glue.cpp:73: error: void value not ignored` (`TInt r = UserSvr::ScreenInfo(pckg);`) — `genuine/ewsrv/edisp_glue.cpp` was being edited by the edisp agent at that moment (mtime 19:16:17; current content has the fixed void-call form). No QEMU boot, 0 fault lines |
| 6 | genuine_eshell | PASS | 22.8s | `..._genuine_eshell.log` | PASS |
| 7 | genuine_fat_io | PASS | 17.8s | `..._genuine_fat_io.log` | PASS |
| 8 | doom_main | PASS | 76.0s | `..._doom_main.log` | RESULT: PASS |
| 9 | doom_audio | PASS | 36.2s | `..._doom_audio.log` | RESULT: PASS |
| 10 | doom_playtest | PASS | 73.9s | `..._doom_playtest.log` | RESULT: PASS |
| 11 | doom_multikey | PASS | 66.8s | `..._doom_multikey.log` | RESULT: PASS |

## Fault scan (step 3)

`grep -E 'FAULT|Exc |Panic|#PF|KERN-'` over all 11 `authority_20261124/final_verify_*.log` plus
`authority_final_verify.log`, `authority_fullchain.log`, `authority_retire.log`:
**zero real fault lines** — every match is a `[PASS] no Exc/Panic/FAULT/#PF` line or the
"note: SKIP never fails the run…" boilerplate. `final_verify_genuine_ewsrv.log` and
`final_verify_selfcheck.log` contain 0 matches (compile-fail / assertion-fail, no boot faults).

## Race evidence — why the tree did not stay settled during the window

Pre-run snapshot (`/tmp/opencode/authority_prerun_state.txt`, 19:07:42) vs post-run
(`/tmp/opencode/authority_postrun_state.txt`, 19:19:59):

| item | pre-run (19:07:42) | post-run (19:19:59) |
|---|---|---|
| HEAD | `e29a072` | `dd22ae2` (committed 19:11:24, mid-run) |
| `git diff` sha256 | `441652c0…` | `da8c5d0d…` |
| `user/user.mk` mtime | 19:01:46 | **19:18:15** |
| `build/kernfull.elf` mtime | 19:05:22 | **19:18:50** (relinked mid-run) |
| `build/symbian-grub.iso` mtime | 19:05:23 | **19:18:51** (rebuilt mid-run) |

Plus at 19:20:40–19:21:27 still active: a **second** `r1_final_verify.py --strict-skip`
(PID 934031, started 19:11 by the svcmgr agent, still in its DOOM phase), a retire test
(PID 946205), and a `r1_web_browser_test.py` (PID 946913). `genuine/ewsrv/edisp_glue.cpp`
(19:16:17), `build_ewsrv.py` (19:11:53), `ewsrv_shims.cpp` (19:12:20) were edited during the
window by the edisp agent; `user/euser_shim.cpp` deleted uncommitted. `.sybian/qemu-slot.lock`
exists but is empty (0 bytes) — not a reliable lock.

Both FAILs are attributable to this concurrency, not to a tree defect:
- `genuine_ewsrv` compiled the half-edited `edisp_glue.cpp` (fixed at 19:16:17; the concurrent
  agent's own ewsrv step at 19:17:53 produced a 46 KB log in the default dir).
- `selfcheck`'s `cp4_stress` kill/survivor assertion fired under concurrent full-suite load
  (two final_verify chains + retire running); the log shows no fault lines and all sibling
  steps PASS.

## Tree state at report time (2026-11-24 19:21:27 EET)

- HEAD `dd22ae2`; 18 uncommitted `git status` entries, incl. `M user/user.mk`,
  `M genuine/ewsrv/build_ewsrv.py`, `M genuine/ewsrv/ewsrv_shims.cpp`, `D user/euser_shim.cpp`,
  `M docs/CLEANUP_LOG.md`, `M docs/DOCS_INDEX.md`, `M AGENTS.md` (other agents' in-flight work —
  not touched by this run).
- Active processes at 19:21:27: `r1_final_verify.py --strict-skip` (PID 934031) + its QEMU,
  `r1_web_browser_test.py` (PID 946913). **Not quiet.**

## Quiescence criteria for the next attempt (all must hold at the one-shot check)

1. `ps aux | grep -E 'make|qemu-system|python3 tools'` → no matches.
2. HEAD stable and `git diff` sha256 unchanged across the run; watched mtimes
   (`Makefile`, `variant/e32load.cpp`, `variant/qemu_variant_asic.cpp`, `variant/qemu_variant.h`,
   `user/user.mk`, `genuine/ewsrv/*`, `qemu_display.cpp`, `build/kernfull.elf`,
   `build/symbian-grub.iso`) all older than ~5 min.
3. No second `r1_final_verify.py` in flight (it shares `/tmp/opencode` and relinks mid-run).
4. Then run: `python3 tools/r1_final_verify.py --strict-skip --log-dir /tmp/opencode/authority_<ts>`
   and require captured `OVERALL: PASS (11 pass, 0 skip, 0 fail)` + fault-free per-test logs
   before citing 11/11 on the current tree.

## Caveats

- The DOOM steps were insulated from the 19:18:51 ISO rebuild (fresh `/tmp/opencode/r1_doom.iso`
  staged at 19:07:47, `--no-build-iso`), so doom_* PASSes reflect the pre-`dd22ae2` image.
- The concurrent agent's in-flight strict run (PID 934031) writes its per-test logs to the
  default `/tmp/opencode/final_verify_*.log`, partially overwriting the 18:46-era evidence
  cited in `docs/AUTHORITY_GATE_REVIEW.md`; `final_verify_strict_20261124.log` (03:55 d26fade
  OVERALL) is still intact.
- `r1_genuine_fullchain_test.py` (19:18:26) booted the GRUB ISO concurrently with the 19:18:50
  relink; its 14/14 PASS is consistent but its image predates the final relink by <1 min.
- Per protocol, no commit/push was done by this run; no source/docs of other agents edited;
  only this one report file was written.

## History — prior certification (superseded as authority, kept for the record)

- **2026-11-24 17:01 run:** `r1_final_verify.py --strict-skip` effective 11/11 PASS (selfcheck
  run-1 FAIL attributed to cross-test interference, isolated re-run PASS), retire 4/4,
  fullchain 14/14 — all measured on the **`d26fade`** tree (HEAD advanced to `e29a072` during
  that run). It certified `d26fade` only and explicitly did not certify post-`e29a072` state.
- **2026-11-24 03:55 run:** captured `OVERALL: PASS (11 pass, 0 skip, 0 fail)` on `d26fade`
  (`/tmp/opencode/final_verify_strict_20261124.log`) — the only captured strict 11/11 to date,
  on the pre-cleanup tree.
