# EFILE / EFSRV convergence — concurrent-fix status coordination

Date: 2026-11-24 (~02:47). Scope: read-only source/log audit plus this report.
`kernelhwsrv/` READ-ONLY and untouched. No QEMU run, no rebuild, no commit/push
by this audit. Other agents are actively editing sources and rewriting logs, so
mtimes below are a snapshot; re-check before acting.

## 1. Fix state table

| Fix | Owner file (mtime) | State | Evidence |
|---|---|---|---|
| **A1 — single-owner guard** (refuse a second EFILE-family image) | `variant/e32load.cpp` (02:46:54; guard at :30-110, check at :842-850) | **VERIFIED LANDED (guard only)** | Built kernel `build/kernfull.elf` (02:45:20) contains `e32run: single-owner: %s already running or !FileServer published; launch refused`. Fresh raw log `/tmp/opencode/guard_duplicate_test.log` (02:46:12) shows a second `C:/> efilex` (its line 1255) ending in `e32run: single-owner: efilex.E32 already running or !FileServer published; launch refused` (line 1309) — D1 guard path exercised, PASS for the guard itself. |
| **A2 — entry trampoline** (dispatch reentries to `SThreadCreateInfo.iFunction`, NULL guard, exec 115) | `variant/e32load.cpp` (02:46:54; asm :120-150, install :1080-1088) | **PENDING — source only, not built/proven** | `strings build/kernfull.elf` (02:45:20) has **no** `EFILE single-entry veneer` marker, and `guard_duplicate_test.log` (02:46:12, i.e. after the source edit but with the older kernel) still has `genuine file server starting`=3, `E32Main returned`=2, veneer markers=0. Prior veneer logs `diag5_prove.log`/`diag6_prove.log` (02:38/02:40) gave 1 start / 1 registered / 0 returns but `E32ENTRY: reentry reason=1 iFunction=00000000` → `PD: #PF` → `EFILE: port-shim` 19/38. The current NULL-entry guard + `_FOFF(SThreadCreateInfo,iFunction/iPtr)` offsets are unbuilt and unverified. |
| **B — F32CLIENT genuine name/acceptance fix** | `genuine/f32_probe/fsclient.cpp` (02:42:58) → `genuine/f32_probe/F32CLIENT.E32` (02:43:03) | **VERIFIED LANDED at source+binary; end-to-end still FAILS** | Source has genuine `TLitC16<12> KFileServerName={11,L"!FileServer"}` → `GenuineTBuf8` → `exec4(128,…)` + EConnect; acceptance now counts any non-`KRequestPending` reply. Binary is newer than source and contains UTF-16 `!FileServer` (`strings -el`=1). Shim-only `sub_efsrv_test.log` (02:45:57): `SessionPath r=0`×2 + `IPC round trip OK`×2 (start=0 ⇒ EFSRVMIN, not genuine). Against FULL EFILE: `fallback_efile_genuine.log` (02:45) and `current_prove3.log` (02:45): `SessionPath r=-15`×2 (= KErrServerTerminated), `IPC round trip OK`×2 — client name fix works (connects, gets a reply) but the server is dying from unfixed A2. |
| **Gate typo fix** | `tools/r1_genuine_retire_test.py` (02:42:56, line 77) | **VERIFIED LANDED** | Test expects `b"GenuineBoot: file server up, svcmgr fallback skipped"`; kernel prints exactly `GenuineBoot: file server up, svcmgr fallback skipped` (`variant/e32load.cpp:1623`, present in `kernfull.elf`). `retire_typofix_full.log` (02:44:48): ewsrv/estart/euser PASS, efsrv FAIL, `RESULT: FAIL`. |
| **Fresh gate log** | `/tmp/opencode/r1_retire_efsrv.log` | **INCONCLUSIVE (mid-run / overwritten)** | Re-written at 02:46:22 (3437 B) and ends mid-boot at `DISPLAY: EHalGroupDisplay(4) registered` — no EFILE launch, no F32CLIENT, no EFSRV verdict. Earlier 02:46:16 copy (32331 B) showed 3 starts / 2 returns / `SessionPath r=-15`×2 / IPC OK×2, terminated by SIGTERM (signal 15); not a complete gate run. |

**Overall: FIX A = PARTIAL (guard landed, trampoline pending); FIX B = SOURCE/BINARY LANDED but gated on A; EFSRV gate = FAIL / incomplete proof.**

## 2. Exact EFSRV-gate PASS acceptance criteria

All counts from **one complete raw serial log** (harness stdout repeats markers and overcounts). Required simultaneously:

1. `EFILE: genuine file server starting` == **1**
2. `File server thread registered` == **1**
3. `EFILE: E32Main returned` == **0**
4. `#PF` == **0** (no `PD: #PF`, no `E32ENTRY … iFunction=00000000` reentry)
5. `EFILE: port-shim` == **0** (no `User::Panic` 19/38 fallback)
6. `F32CLIENT: IPC round trip OK` >= **1**
7. `SessionPath r=-15` == **0** (KErrServerTerminated ⇒ dead owner)
8. `Exc` / `PanicKernExec` / `FAULT` == **0**
9. `GenuineBoot: EFILE.E32 r=0` == **1** and `GENUINEFS: starting` == 0 (genuine server, not shim)
10. `tools/r1_genuine_retire_test.py` reports `[PASS]` for EFSRV and `RESULT: PASS`.

Do **not** accept the shim-only `sub_efsrv_test.log` / `r1_genuine_efsrv.log` round trips as proof.

## 3. D1 / D2 / D3 (per `docs/EFILE_DUPLICATE_SOURCES.md` §5)

- **D1 cross-launcher duplicate:** guard branch now demonstrated (`launch refused` ×1 for second `efilex`), but first launch still 3 starts / 2 returns because the built kernel lacks A2 ⇒ **PENDING overall**.
- **D2 intra-process re-entry:** earlier veneer logs fixed counts but hit `iFunction=0` → `#PF` → `port-shim`; NULL guard + corrected offsets are unbuilt ⇒ **PENDING**.
- **D3 shim/genuine collision:** **UNTESTED**.

## 4. Next command (only when A2 is rebuilt AND one complete prove run meets §2)

```bash
cd qemu-port && python3 tools/r1_final_verify.py --strict-skip
```

Blockers: (a) rebuild `build/kernfull.elf` from the 02:46:54 `variant/e32load.cpp` and confirm `EFILE single-entry veneer` is in the binary; (b) let a prove/gate run finish without SIGTERM and re-count §2; (c) do not race the other agents still editing sources/logs. No shim retirement until the EFSRV gate reports PASS. `kernelhwsrv/` untouched.
