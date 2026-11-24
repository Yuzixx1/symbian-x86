# ESHELL PS status snapshot (read-only)

Snapshot taken 2026-11-24 ~03:13 local. No QEMU launched, no files edited except
this report. `kernelhwsrv/` untouched.

## Owner activity (ACTIVE — do not take over)

- `python3 tools/r1_genuine_fullchain_test.py --log-dir /tmp/opencode` (PID
  492775, under `timeout 600` PID 492772) with its QEMU (PID 492784), started
  03:11, still running; log `/tmp/opencode/fullchain_tool_rerun.txt` reaches
  only `EFILE ... launched` (in progress, not a verdict).
- `python3 /tmp/opencode/sessionpath_prove_long.py` (PID 494254) started 03:12.
- Prior repro `repro_t.py`/QEMU (PIDs 494148/494149) had completed by snapshot.
- Working-tree mtimes confirm recent owner edits: `genuine/estart_eshell/shims.cpp`
  03:11:09, `obj/` 03:11:16, `build_estart_eshell.py` 03:06. `git status` shows
  `genuine/estart_eshell/` and `tools/r1_genuine_eshell_test.py` all untracked
  (`??`) — no committed fix yet.

## PS status: appears FIXED in latest repro (not yet gate-confirmed)

- Latest log `/tmp/opencode/repro_ps_t.log` (03:12, 3.5 KB), generated after the
  03:11 `shims.cpp` edit: `eshell` launched (pid=11), `pps` → `--PROCESSES-->`
  followed by process entries, `t` → thread entries
  (`... ID 5 (Proc 1), running` … `... ID 12 (Proc 11), running`), final `ps>`
  prompt. `grep` for `FAULT|#PF|Exc |Panic|GetThreadInfo|task exit|KernCSLocked`
  in this log: **none**. Earlier `repro_ps5.log` (02:59) was only a SIGTERM
  timeout, inconclusive.
- Root-cause fix is in `genuine/estart_eshell/shims.cpp` `wide_format()` case
  `'S'`: the old code NUL-scanned a length-based `TDesC16::Ptr()`, overflowing
  into the 0x29292929-prefilled user stack; it now uses `d->Length()` via
  `wide_append_n()`. This is exactly the genuine-PS fault path.
- Caveat: `/tmp/opencode/r1_retire_estart.log` (03:10) still prints one
  `Exc 00 EFLAGS=00051213 ErrCode=00000000` plus an ESHELL::Main register dump
  during startup, but ESHELL subsequently loads `r=0 pid=11` and reaches
  `C:/>`. The newest PS-specific repro shows no such exception. No formal
  `r1_genuine_eshell_test.py` re-run after the 03:11 fix is visible yet.

## ESTART gate

- Recorded PASS: `docs/GENUINE_PROGRESS.md` §1 (`estart PASS`), §2 retire table
  (`estart PASS, smoke PASS`); `docs/GO_NO_GO.md` §1 marks ESTART/ESHELL **PASS**
  (boot markers, `E32Main returned 0`, 0 faults), with eshell PS listed as an
  outstanding shell-command path that "does not block the gate".
- `/tmp/opencode/r1_retire_estart.log` shows genuine ESTART claim
  (`single-owner: ESTART.E32 uid3=100000bf claimed`), `ESTART.E32 r=0 pid=9`,
  `ESHELL.E32 r=0 pid=11`, `C:/>` prompt — gate evidence intact.
- `docs/GENUINE_PROGRESS.md` §2 still lists eshell PS as IN FLIGHT, so docs are
  stale relative to the 03:12 repro; needs owner update after a clean test.
- `docs/GO_NO_GO.md` §2 step 3 requires the PS fix "confirmed PASS" before GO;
  current repro is strong evidence but not the formal confirmation.

## Recommendation

**WAIT.** The owning agent is still driving full-chain verification and a
session-path prove; PS looks fixed by the 03:11 `shims.cpp` change and the 03:12
repro, but the formal `r1_genuine_eshell_test.py` re-run / `--strict-skip`
confirmation has not landed. Do not touch `genuine/estart_eshell/shims.cpp`,
`build_estart_eshell.py`, `obj/`, `tools/r1_genuine_eshell_test.py`, or the
`build/` artifacts, and do not launch QEMU. Take over only if the 600 s
full-chain process (PID 492772/492775) exits without producing a verdict and
no new repro/test log appears.

## Blockers

- No formal post-fix PS test result; no committed fix (all files untracked).
- `docs/GENUINE_PROGRESS.md` §2 and `docs/GO_NO_GO.md` §2/§3 still say PS IN
  FLIGHT / must be confirmed.
- One `Exc 00` in the 03:10 retire-estart log is unexplained; the 03:12 PS
  repro is clean but has no automated PASS/FAIL verdict.
- Full-chain re-verification still in progress at snapshot time.
