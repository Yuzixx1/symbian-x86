# EFSRV FAIL→PASS gate-flip checklist (anti-false-pass)

Date 2026-11-24. Read-only checklist. `kernelhwsrv/` READ-ONLY/untouched; no
commit/push, no `make clean`, no source/gate edits, no rebuild here — all
verification below is dispatched to the owning agents. Do **not** declare EFSRV
PASS until every box is ticked from ONE clean log.

## 1. Preconditions (all true before any prove)

- [ ] Re-entry suppression **landed with markers**: intra-process `app_main`
      reentries no longer start a second `E32Main`/DServer
      (`docs/EFILE_REENTRY_FIX.md`); markers visible in a fresh serial log.
      No trampoline/veneer — guard-only EFILE kernel (`AGENTS.md`,
      `docs/EFILE_GUARD_VS_TRAMPOLINE.md`, `GUARD_TRAMPOLINE_REVIEW.md`).
- [ ] UID re-stamp landed: `estart_ 0x10272C04`, `etshell_ 0x101F7AE7`;
      eshell/retire/smoke already PASS (`docs/GUARD_ESTART_BLOCK_ANALYSIS.md`,
      `PACK_MULTI_UID_FIX.md`). No `single-owner ... launch refused` on ESTART.
- [ ] Guard-only kernel rebuilt **without** `E32ENTRY: reentry` /
      `PortE32EntryTrampoline` strings; `port-shim == 0`, `#PF == 0`.
- [ ] **No active rebuild**: ISO/kernfull/F32CLIENT mtimes stable and newer than
      their sources; no in-flight agent rewriting the staged disk or log.
- [ ] tmpfs/cache space sufficient for a fresh staging disk + full raw log
      (`/tmp/opencode`, `/home/yuzi/.cache/sybian-r1`); archive/cache old logs if low.

## 2. Exact evidence — ONE clean prove + strict acceptance

Run exactly one uninterrupted boot, then analyse its log (no second QEMU needed):

```
cd qemu-port
python3 tools/r1_efile_clean_prove.py                      # ONE QEMU boot, do not terminate early
python3 tools/r1_efile_clean_prove.py --log <that raw log> # analysis only
python3 tools/r1_efile_acceptance.py --log <same raw log>  # strict 9/9
```

The raw log must show, all in the same capture (no SIGTERM, ends at `C:/>`):

| Marker / criterion | Required |
|---|---|
| `RESULT: PASS (9/9 ...)` from `r1_efile_acceptance.py`, exit 0 | **9/9 PASS** |
| `EFILE: genuine file server starting` | **1** (start == 1) |
| `File server thread registered` | **1** |
| `E32Main returned` | **0** |
| `F32CLIENT: IPC round trip OK` | **>= 1** (honest client, `r == 0` only) |
| `SessionPath r=-15` | **0** |
| `SessionPath FAILED` / `r=<n>` non-zero | **0** |
| `EFILE: port-shim` | **0** |
| `#PF` / `FAULT:` / `PanicKernExec` / `Exc ` | **0** |
| `GenuineBoot: EFILE.E32 r=0`, single-owner claim | **1 / 1** |

Baseline to beat: strict **FAIL 5/9** (start=3, E32Main=2, IPC=0, `r=-15`=2,
`FAILED`=2) — `docs/EFILE_CLEAN_PROVE.md`, `NEG15_SEMANTICS.md` §6,
`SESSIONPATH_NEG15.md`. `IPC round trip OK` counted only on `r == 0`; the
permissive client that counted any reply was the false PASS and is forbidden.

## 3. Official gate (after evidence, not before)

```
python3 tools/r1_genuine_retire_test.py --only efsrv --log-dir /tmp/opencode
python3 tools/r1_genuine_retire_test.py --log-dir /tmp/opencode
python3 tools/r1_qemu_smoke.py
```

- [ ] `r1_genuine_retire_test.py` EFSRV section reports `[PASS] F32CLIENT: IPC
      round trip OK`; EFILE server FULL (not `GENUINEFS`/`EFSRVMIN`/
      `R1FileServer`), `port-shim == 0`.
- [ ] All four subsystems in one retire run PASS: **EFSRV, EWSRV, ESTART/ESHELL,
      EUSER** (no shim retired before its gate passes).
- [ ] `r1_qemu_smoke.py` PASS (no Exc/PanicKernExec). Optionally confirm with
      `python3 tools/r1_final_verify.py --only genuine_efsrv`.

## 4. Full-chain (strict order, one boot)

```
python3 tools/r1_genuine_fullchain_test.py --log-dir /tmp/opencode
```

- [ ] **14/14 PASS**, `RESULT: PASS`, exit 0; `ESTART r=0` and `ESHELL r=0`
      with `SessionPath r=0` / honest IPC, in boot order EUSER → EFILE → ESTART
      → EWSRV → ESHELL, ending at `C:/>`.
- [ ] ESTART/ESHELL each show genuine UID3 (`10272c04` / `101f7ae7`), zero
      `launch refused`, zero `Exc `/`PanicKernExec`/`FAULT:`, `port-shim == 0`.

## 5. Anti-false-pass rules (invariant)

- Never accept `r=-15` (`KErrServerTerminated`) or any non-zero `SessionPath r=`
      as success — it is a dead server owner, not a benign status.
- Never edit/loosen a checker, threshold, or marker (`r1_efile_acceptance.py`,
      `r1_genuine_retire_test.py`, `fsclient.cpp` integrity check) to obtain PASS.
- Never use a permissive-client log (`IPC round trip OK` on any reply) as proof;
      only the log printing `SessionPath r=0` first counts.
- Never run a prove/gate during an active rebuild or against a transient kernel;
      verify ISO/ELF mtimes and trampoline-free bytes first.
- If tmpfs is low, **archive/cache the logs** — never truncate a capture early
      or reuse a stale/pre-fix log (e.g. the shim-based `GENUINEFS` run) as evidence.

## 6. If it still FAILs

- Do **not** edit any gate, checker, threshold, or accept `-15`. Re-read
  `docs/NEG15_SEMANTICS.md`, `docs/GUARD_ESTART_BLOCK_ANALYSIS.md`, and
  `docs/EFILE_REENTRY_FIX.md` (re-entry suppression), then re-dispatch the
  kernel/boot + single-owner (and, if TLS faults recur, `mmu/pd.cpp`/EUSER
  loader) owner with the failing criterion and the raw log path. Leave this
  checklist unchecked and the EFSRV gate FAIL until a clean prove satisfies §2.
