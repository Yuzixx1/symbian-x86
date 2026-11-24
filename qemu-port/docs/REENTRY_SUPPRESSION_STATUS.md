# EFILE re-entry suppression — status (read-only snapshot)

Date 2026-11-24 03:36 EET. Observer subagent; `kernelhwsrv/` READ-ONLY and
untouched; no QEMU/rebuild run here; no commit/push. All data below from
mtimes, `ps`, and read-only analysis of existing logs via
`tools/r1_efile_acceptance.py --log` (analyse-only, no QEMU launched).

## 1. Activity (owner actively iterating — do not interfere)

- `genuine/efile/efile_glue.cpp` mtime **03:24:28**;
  `genuine/efile/patched/sf_main.cpp` **03:34:15**;
  `genuine/efile/obj_build/{efile_pass1.elf,*.o}` **03:34:19**;
  `genuine/build/EFILE.E32` **03:34:19**. Entire `genuine/efile/` is **untracked**
  (`?? genuine/efile/` in git).
- At first observation (03:34) owner process `python3 tools/r1_efile_acceptance.py
  --log-dir .../efile-reentry-accept3` (pid 512982) + its `qemu-system-i386`
  were running; at 03:36 accept3 ended (`terminating on signal 15 from pid
  512982`), log final size 32822 bytes. Three acceptance runs in ~7 min
  (accept 03:30, accept2 03:32, accept3 03:35) plus `efile_debug_build{,2,3}.log`
  at 03:29/03:31/03:34. This is **live work**, not a stalled agent.
- `docs/EFILE_REENTRY_SUPPRESS.md` does **not** exist. Related docs:
  `EFILE_REENTRY_FIX.md` (03:12), `EFILE_CLEAN_PROVE.md` (03:15),
  `EFILE_CONVERGENCE.md` (02:47), `EFSRV_GATE_FLIP_CHECKLIST.md`.

## 2. Marker counts (one complete log each; prove + accept1/2/3 agree)

Counts from raw logs in `~/.cache/sybian-r1/efile-reentry-{prove,accept,
accept2,accept3}/`; strict checker run on each.

| Criterion (checklist §2) | Required | prove | accept | accept2 | accept3 (final) |
|---|---|---|---|---|---|
| `EFILE: genuine file server starting` | ==1 | 1 | 1 | 1 | 1 |
| `File server thread registered` | ==1 | 1 | 1 | 1 | 1 |
| `E32Main returned` | ==0 | 0 | 0 | 0 | 0 |
| `EFILE: re-entrant app_main suppressed` (fix marker) | present | 2 | 2 | 2 | 2 |
| `#PF` | ==0 | 0 | 0 | 0 | 0 |
| `EFILE: port-shim` | ==0 | 0 | 0 | 0 | 0 |
| `SessionPath r=-15` | ==0 | 0 | 0 | 0 | 0 |
| `SessionPath FAILED` / `SessionPath r=<n>` | ==0 | 0 | 0 | 0 | 0 |
| `Exc ` / `PanicKernExec` / `FAULT:` | ==0 | 0 | 0 | 0 | 0 |
| `GenuineBoot: EFILE.E32 r=0` | ==1 | 1 | 1 | 1 | 1 |
| `F32CLIENT: IPC round trip OK` | >=1 | **0** | **0** | **0** | **0** |
| **Strict checker result** | **9/9** | 8/9 | 8/9 | 8/9 | **8/9** |

Evidence tail (final accept3): `EFILE-DBG: session path chars 67,58,92 len=20`,
`EFILE-DBG: NewSessionL before Client`, `... NewSessionL done`,
`EFILE-DBG: ServiceL func 0xe main=1`, then `e32run: resumed`,
`f32client started (user process, pid=13)`, `C:/> `. Suppression markers sit
next to `commonInitialize`'s `RThread.Create` / `Logon` (log lines ~1244/1252).

**Re-entry goal met:** 1 start / 0 `E32Main returned`, suppression fired 2×,
no fault, no port-shim, `r=-15` eliminated (baseline had 3 starts / 2 returns /
2× `r=-15` / 2× FAILED — `docs/EFILE_CLEAN_PROVE.md`, checklist §2).

## 3. Comparison to `EFSRV_GATE_FLIP_CHECKLIST.md`

- Re-entry suppression precondition (§1): **satisfied** — guard-only, markers
  in a fresh log, no trampoline strings, port-shim=0, #PF=0.
- §2 strict table: **8/9**, single failure `IPC round trip OK >= 1`.
- §3/§4 official retire + full-chain gates: **not (re-)run on this build** by
  this observer; no 14/14 or 9/9 evidence available. §1 "no active rebuild"
  precondition was false during observation (owner rebuilding/testing at 03:34).
- Anti-false-pass rules intact: checker/threshold not edited by observer;
  `r=-15` not accepted; no permissive-client log used.

## 4. Verdict and remaining gap

**Verdict: IN PROGRESS** (suppression itself LANDED and verified; the one
remaining blocker is still open). Not LANDED overall (8/9, not 9/9); not
STALLED (owner active, 3 fresh runs); not INCONCLUSIVE (four consistent logs).

**Remaining gap:** `F32CLIENT: IPC round trip OK >= 1`. The server receives
SessionPath (`ServiceL func 0xe`, `main=1`) but never returns a reply: no
`SessionPath r=` / `no IPC reply` / `SessionPath FAILED` line is printed at all,
so the honest client is stuck in `ipc_call_args` awaiting a response (the
checker SIGTERMs QEMU after 60 s). This is the reply path / live server thread,
not the re-entry guard; consistent with `docs/EFILE_REENTRY_FIX.md` residual
blocker (genuine EUSER absolute TLS addresses vs supervisor-only kernel
mappings, `mmu/pd.cpp` / EUSER loader ownership).

## 5. Recommendation

**WAIT.** Owner is actively iterating on this exact criterion (accept3 just
finished; rebuilds minutes old). Do not take over, do not run QEMU/rebuild, do
not edit gates. Re-check `~/.cache/sybian-r1/efile-reentry-accept4/` (or newer)
and `genuine/efile/`, `genuine/build/EFILE.E32` mtimes; only accept a fresh
`RESULT: PASS (9/9 ...)` with `SessionPath r=0` printed before `IPC round trip
OK`. If no new run within ~20 min and mtimes freeze, escalate with the failing
criterion and the final accept3 raw-log path to the owner (reply-path/TLS,
`mmu/pd.cpp` / EUSER loader), not the re-entry glue.
