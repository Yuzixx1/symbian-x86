# FINAL VERIFY RUNBOOK — execute only after the five in-flight fixes land

**Basis:** `docs/FINAL_GATE_STATUS.md` snapshot 2026-11-24 03:17 EET — **8 GREEN /
5 IN FLIGHT / 1 FAIL / 1 NOT DONE**; overall **NOT DONE**. This page is a read-only
plan: no QEMU, no build, no source edit, no commit, no shim retirement was made
for it. `kernelhwsrv/` is READ-ONLY and untouched. **This runbook is not
authorization** — even an all-PASS run does not permit commit/push or shim
retirement without an explicit user request (§4).

## 1. PREREQUISITES — all must hold before §2

### 1.1 Fix-landing evidence (check each; current tree still shows several NOT landed)

| ID | Fix | File / marker evidence to confirm | Current observed state |
|---|---|---|---|
| **A** | Trampoline final removal | `variant/e32load.cpp`: **0** matches for `PortE32EntryTrampoline`, `sPortOrigEntry`, `sPortReentry`, `E32ENTRY`, `SThreadCreateInfo`; guard-only `single-owner:` claim at ~873/881; source + rebuilt ELF/ISO agree. See `TRAMPOLINE_FINAL_REMOVAL.md`, `GUARD_TRAMPOLINE_REVIEW.md`. | Source grep is clean; **runtime acceptance still open** (last prove ends in harness `terminate()`). AGENTS.md permanently forbids the trampoline (it gave port-shim=2, `#PF`=2, IPC=0) — never reintroduce it. |
| **B** | `pack_multi.py --uid3` + callers | `genuine/pack_multi.py`: `def pack(..., uid3=0x100000bf)` (~31), `build_args_e32(0x1000008c, uid3, 0)[:156]` (~121), `--uid3` CLI (~166). Callers in `genuine/estart_eshell/build_estart_eshell.py:277` must pass `uid3=0x10272C04` (ESTART) / `0x101F7AE7` (ESHELL); the `0x10000111` workaround (lines 278–296) must be **deleted**. See `PACK_MULTI_UID_FIX.md`, `GUARD_ESTART_BLOCK_ANALYSIS.md` §2B. | Tooling landed; **callers NOT landed** (`0x10000111` still at :291). Root cause of ESTART `r=-11`. |
| **C** | `-15` integrity in f32_probe | `genuine/f32_probe/fsclient.cpp:111-127`: require `r == KErrNone` before `F32CLIENT: IPC round trip OK`; print `F32CLIENT: SessionPath FAILED (no genuine KErrNone reply)` **only on failure**; no `IPC round trip OK` for `r=-15`. Client binary `F32CLIENT.E32` must be rebuilt from it and staged by the prove. | Source has the check; fresh full run must show **0** `SessionPath r=-15`, **≥1** `IPC round trip OK`. |
| **D** | ESHELL UID gate check | Guard must not refuse ESTART/ESHELL: `PortIsEfileImage()` (`variant/e32load.cpp:62-66`) requires genuine UID3 + EFILE-family name; EFILE claim strictly inside `if (isEfile)` (~796/869), `launch refused` = 0 for non-EFILE. Discriminators: `E32Validate ... uid3=10272c04` / `uid3=101f7ae7`, `GenuineBoot: ESTART.E32 r=0` (was -11), `ESHELL r=0`. See `GUARD_ESTART_BLOCK_ANALYSIS.md` §2A/§4. | IN FLIGHT — confirm in the §2 fullchain log, not by source reading alone. |
| **E** | eshell PS landed | `genuine/estart_eshell/shims.cpp` `wide_format()` case `'S'` uses `d->Length()` via `wide_append_n()` (no NUL scan of `TDesC16::Ptr()`); `tools/r1_genuine_eshell_test.py` PASS with `ps` thread rows and 0 FAULT. See `ESHELL_PS_STATUS.md`. | Fix landed untracked; formal post-fix gate result still required. |

### 1.2 Tree stability (no concurrent rebuild / QEMU)

- `.sybian/qemu-slot.lock` must be **empty** (it is currently an empty untracked file); no live owner process.
- `ps -eo pid,cmd | grep -E 'qemu|make |genuine_build|r1_'` shows no active build/QEMU/test job. *(An active `r1_genuine_fullchain_test.py` + QEMU and a smoke run were observed during this snapshot — do not start §2 until they finish.)*
- Sample `stat -c '%y %s' build/kernfull.elf build/symbian-grub.iso` twice, ≥60 s apart: mtimes and sizes must be unchanged, ISO not older than ELF. Last observed: ELF 03:15:15 (1,025,004 B), ISO 03:15:27 (75,882,496 B). If stale, rebuild with `make kernfull isogrub` — **never `make clean`**.
- Do not take over files owned by the in-flight agents (`variant/e32load.cpp`, `genuine/pack_multi.py`, `genuine/estart_eshell/`, `genuine/f32_probe/`, `tools/r1_*`, `build/`).

### 1.3 Disk / tool guards

- `df -h /tmp/opencode` — need **≥1 GiB** free (`MIN_FREE_BYTES` in `tools/r1_final_verify.py`; currently 5.8 G free). The disk guard auto-selects a unique `~/.cache/sybian-r1/final-verify-<stamp>` dir when the default is low; an explicit `--log-dir` is honored with a warning.
- `r1_final_verify.py` already contains the landed false-PASS fixes: stale DOOM ISO deleted and a fresh ISO staged before DOOM rows (~232–297) and nonzero rc with a printed PASS/SKIP forced to FAIL (~347–348). Confirm with `--dry-run` (expect 11 commands).

## 2. ORDERED COMMANDS — all from `qemu-port/`, in this order

```bash
cd "/home/yuzi/Documents/Sybian REAL/qemu-port"

# 1. Plan only — confirm the 11 commands; no QEMU, no verdict.
python3 tools/r1_final_verify.py --dry-run

# 2. Authoritative 11-test gate (incl. DOOM). Use --rebuild-iso if §1.2 rebuilt.
python3 tools/r1_final_verify.py --strict-skip
#    If an explicit --log-dir is needed: append --log-dir /tmp/opencode
#    Require EVERY row PASS; a SKIP is fatal here. Logs: /tmp/opencode/final_verify_<name>.log

# 3. One-boot full chain — all four genuine components in order, no SIGTERM.
python3 tools/r1_genuine_fullchain_test.py --log-dir ~/.cache/sybian-r1

# 4. EFILE: one uninterrupted clean prove, then analyse THAT raw log (no QEMU in analysis).
python3 tools/r1_efile_clean_prove.py --dry-run
python3 tools/r1_efile_clean_prove.py --log-dir ~/.cache/sybian-r1
python3 tools/r1_efile_acceptance.py --log ~/.cache/sybian-r1/efile_clean_prove.log

# 5. Per-subsystem retire gates — all four must PASS.
python3 tools/r1_genuine_retire_test.py --log-dir ~/.cache/sybian-r1

# 6. DOOM main/audio/playtest/multikey are rows 8–11 of step 2; do not accept a
#    stale-ISO or SKIP DOOM result. Inspect /tmp/opencode/final_verify_doom_*.log.
```

Ordering rationale: `--strict-skip` is the deciding gate (non-fatal SKIP and the
two false-PASS paths otherwise); the fullchain and clean prove use private log
dirs so no other agent can overwrite the raw logs; the acceptance checker must
read one uninterrupted raw log, never a SIGTERM'd harness log.

## 3. PASS CRITERIA — exact counts / markers, and where to look on FAIL

| Gate | Command | Required result (exact) | On FAIL — consult / owner |
|---|---|---|---|
| Final verify plan | `--dry-run` | 11 commands, exit 0, no verdict | `FINAL_VERIFY_REVIEW.md` |
| Final verify authoritative | `--strict-skip` | every one of 11 rows PASS; `OVERALL: PASS`, exit 0; **0 SKIP**; logs fresh (§1.2) | `FINAL_VERIFY_REVIEW.md` (false-PASS paths); owning agent of the failing tool |
| Full boot chain | `r1_genuine_fullchain_test.py` | **14/14** rows PASS in one boot: `KERN spawning genuine boot DThread`, `GenuineBoot: probing boot volume`, `e32run: EUSER.DLL resident`, `exports=2503`, `GenuineBoot: EFILE/ESTART/EWSRV/ESHELL.E32 r=0`, `ESHELL 0.01(4011)`, `file server up, svcmgr fallback skipped`, `C:/>`, screendump; `Exc `/`PanicKernExec`/`FAULT`/`port-shim` = **0**; `launch refused` = **0**; ESTART `r=-11` absent | `FULLCHAIN_VERIFICATION.md` (marker-drift caveat: kernel dropped the word "genuine" in the file-server-up line — confirm against the tool's `--dry-run` expected list, do not weaken the gate); `GUARD_ESTART_BLOCK_ANALYSIS.md` §4 (fix B/D) |
| EFILE clean prove | `r1_efile_clean_prove.py` | 464 binds; start=1; registered=1; port-shim=0; IPC OK ≥1; `SessionPath r=-15`=0; `#PF`/`FAULT:`/`PanicKernExec`/`Exc `/SIGTERM=0; ends at `C:/>` | `EFILE_CLEAN_PROVE.md`, `EFILE_GUARD_VS_TRAMPOLINE.md`; owner of `f32_probe` / `~/.cache/sybian-r1` |
| EFILE strict acceptance | `r1_efile_acceptance.py --log <fresh>` | `RESULT: PASS (9/9)`, exit 0: `genuine file server starting`=1, `File server thread registered`=1, `E32Main returned`=0, `#PF`=0, `EFILE: port-shim`=0, `F32CLIENT: IPC round trip OK`≥1, `SessionPath r=-15`=0, `Exc`/`PanicKernExec`/`FAULT`=0, `GenuineBoot: EFILE.E32 r=0`=1 | `EFILE_ACCEPTANCE.md` (criterion table), `GUARD_TRAMPOLINE_REVIEW.md`; `-15` owner / fix C |
| Retire gates | `r1_genuine_retire_test.py` | `RESULT: PASS (4 passed, 0 skipped, 0 failed)` — EFSRV/F32, EWSRV, ESTART/ESHELL, EUSER all PASS | `GO_NO_GO.md` §1, `GENUINE_PROGRESS.md`, `SHIM_RETIREMENT_PLAN.md`; per-subsystem `r1_genuine_{efsrv,ewsrv,eshell}_test.py` |
| DOOM / Freedoom | rows 8–11 of step 2 | main, audio, playtest, multikey PASS; 0 guest faults; `EKA2_CRISP_STOP` absent; fresh ISO staged | `DOOM_INTEGRATION_VERIFICATION.md` §4–6 (`multikey` `diagonal_2` FAIL is a documented marker-attribution artifact); `FINAL_VERIFY_REVIEW.md` §1 (stale ISO) |

A FAIL in any row stops the sequence — fix and re-run from step 1 on a fresh,
stable tree; do not proceed to §4 and do not edit a gate to make it pass.

## 4. POST-PASS — ONLY with explicit user authorization

1. **Guarded shim retirement first, dry-run / preview:** `tools/r1_shim_retire_plan.py`
   then `tools/r1_shim_retire_apply.py` preview →
   `--shim <name> --apply --confirm <TOKEN>` per `SHIM_RETIREMENT_PLAN.md` §4; it
   re-runs the real gate, refuses FAIL/SKIP and rolls back on failure. No shim is
   retired while any gate is FAIL/SKIP (0 READY / 4 BLOCKED at snapshot).
2. **Stage BY PATH** per `CHANGE_INVENTORY.md` §4 (e.g.
   `git add AGENTS.md README.md qemu-port/variant/ qemu-port/fs/ qemu-port/tools/ qemu-port/docs/ qemu-port/genuine/ ...`).
   **NEVER `git add -A`** — `symbian-x86-archive-56fbaff.tar.gz` is untracked and
   NOT gitignored. Also exclude `.sybian/qemu-slot.lock`,
   `.sybian/coordinator/state.json`, `build/`, `obj*/`, `*.E32`, `*.FSY`, `*.pyc`,
   `/tmp/opencode` scratch, and **everything under `kernelhwsrv/`**.
3. **Precommit checks:** `git status --short` (no `kernelhwsrv/` entries),
   `git diff --check`, secret scan, tree matches `CHANGE_INVENTORY.md` —
   see `PRECOMMIT_AUDIT.md` §1–§3.
4. **Commit** using the `PRECOMMIT_AUDIT.md` §1 template; **push only on an
   explicit user request**. Final GO/NO-GO authority is `GO_NO_GO.md` §2–§4.

---
*Written 2026-11-24 EET as a read-only plan for the five in-flight fixes (A–E
above). This runbook is NOT authorization to run QEMU, retire a shim, commit or
push; it records the sequence and evidence required once the owning agents land
their fixes and the user explicitly asks for final verification.*
