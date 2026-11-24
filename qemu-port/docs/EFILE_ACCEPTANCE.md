# EFILE acceptance checker — `tools/r1_efile_acceptance.py`

Strict, read-only verifier for the final EFILE/EFSRV fix. It implements the
exact EFSRV-gate PASS acceptance criteria from `docs/EFILE_CONVERGENCE.md`
section 2 (the 9 runtime criteria, not the harness's own `[PASS]` line). It is
a verifier only: it never edits sources, never rebuilds, and never fakes a
PASS. `kernelhwsrv/` is READ-ONLY and untouched.

## Criteria (all counts from ONE complete raw serial log)

| # | Criterion (substring counted) | Expected | Why |
|---|---|---|---|
| 1 | `EFILE: genuine file server starting` | `== 1` | genuine server entered exactly once |
| 2 | `File server thread registered` | `== 1` | one protected `!FileServer` publisher |
| 3 | `E32Main returned` | `== 0` | genuine E32Main must not exit early |
| 4 | `#PF` | `== 0` | catches `PD: #PF` / `E32ENTRY … iFunction=00000000` reentry |
| 5 | `EFILE: port-shim` | `== 0` | no `User::Panic` 19/38 fallback |
| 6 | `F32CLIENT: IPC round trip OK` | `>= 1` | genuine client got a live reply |
| 7 | `SessionPath r=-15` | `== 0` | no `KErrServerTerminated` (dead owner) |
| 8 | `Exc` / `PanicKernExec` / `FAULT` | `== 0` | no guest fault lines |
| 9 | `GenuineBoot: EFILE.E32 r=0` | `== 1` | genuine auto-launch, not the shim path |

Criterion 8 is line-based: a line containing any of `Exc `, `PanicKernExec`,
`FAULT:` counts once. All others are literal substring counts. The tool prints
a `(criterion, expected, actual, PASS/FAIL)` table and a failed-criteria list.

## Commands

```bash
cd qemu-port

# 1. Plan only — no QEMU, no log analysis, no verdict (exit 0 = plan printed).
python3 tools/r1_efile_acceptance.py --dry-run

# 2. Full check — boot once, drive f32client, write
#    /tmp/opencode/efile_acceptance.log, then parse it.
python3 tools/r1_efile_acceptance.py

# 3. Analyse an existing raw serial log — NO QEMU launched.
python3 tools/r1_efile_acceptance.py --log /tmp/opencode/efile_acceptance.log

# 4. Scratch disk + log directory override.
python3 tools/r1_efile_acceptance.py --log-dir /tmp/opencode --log my.log
```

Mode 2 stages `EUSER.DLL`, genuine `EFILE.E32`, `EFAT32.FSY` and
`F32CLIENT.E32` onto a scratch FAT image **under `--log-dir`**, waits for
`KERN spawning genuine boot DThread` and the `C:/> ` prompt, then sends
`f32client` from the shell. Genuine EFILE is auto-launched by
`GenuineBootLaunch()` during `Init3` — it is **never** launched from the shell
(a second instance dies with a spurious fault). Nothing is rebuilt.

## Interpretation

* `RESULT: PASS (9/9 ...)` and exit **0** — every criterion met; the final fix
  is accepted.
* `RESULT: FAIL (n/9 ...)` and exit **1** — one or more criteria failed; the
  failed list names each criterion with expected vs actual count. Do not
  accept the fix.
* `--dry-run` prints `SKIP (dry-run)` for every actual count and exits 0; it
  deliberately issues **no** PASS verdict.
* A missing log file, missing artifacts/tools, or a QEMU run that produces no
  log all exit **1** — a missing or unreadable log can never yield a PASS.

Do not accept the shim-only `sub_efsrv_test.log` / `r1_genuine_efsrv.log`
round trips as proof; criterion 9 specifically requires the genuine boot path.
This checker complements (does not replace) `r1_final_verify.py --strict-skip`.

## Validation performed

`py_compile` passed; `--dry-run` printed the 9-criterion plan with no PASS
claimed. Synthetic logs proved the parser: an all-pass log gave 9/9 PASS /
exit 0, a log with `EFILE: port-shim`×2 + `SessionPath r=-15`×1 gave 7/9 FAIL /
exit 1 with exactly those two listed, a duplicate-start + `#PF` log gave 3
FAILs, and a missing log gave exit 1. No QEMU was launched during validation.
