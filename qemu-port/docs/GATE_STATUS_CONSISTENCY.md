# GATE STATUS — doc consistency check (read-only)

**Checked:** 2026-11-25 20:32 EET. Scope: [FINAL_GATE_STATUS.md](FINAL_GATE_STATUS.md) vs `/tmp/opencode/final_verify_*.log`. No gate run; no source/commit touched; `kernelhwsrv/` untouched.

1. **P0 section present — PASS (one defect).** `FINAL_GATE_STATUS.md` §"2026-11-25 ~20:05 EET — P0 baseline gate" records strict NO-GO: 7/11 PASS, 4 DOOM SKIP, retire 4/4, fullchain 14/14.
   *Defect:* cause mis-attributed — the doc says the Freedoom IWAD is "user-supplied … absent from the machine" and SKIP is "expected during staging, not a regression". The SKIP logs (`final_verify_doom_main.log`) instead show `mcopy: File "::FDOOM1.WAD" not found` / "IWAD not present (/tmp/opencode/doom/)", and the asset reappears at 20:13 (`/tmp/opencode/doom/freedoom-0.13.0/freedoom1.wad`, 28,795,076 B). Read: **wiped from /tmp and since restored** — an environmental NO-GO, not a never-supplied asset.
2. **Recovery re-run recorded in doc — FAIL (premature).** The doc has no post-recovery section. `final_verify_recovery.log` (20:30–20:32) shows selfcheck PASS 234.4s, qemu_smoke PASS 50.1s, genuine_retire PASS 45.7s, `genuine_efsrv` still running; `ps` confirms PID 40967 `r1_final_verify.py --strict-skip` live. **No strict 11/11 PASS exists yet.**

> **Superseded 2026-11-26.** Item 2 above no longer holds. The current gate is the **15-row** strict run in `/tmp/opencode/final27_gate.log`: `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0` — g12_residue, g12_negative, missing_statics, selfcheck, qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv, genuine_eshell, genuine_fat_io, doom_main, doom_audio, doom_playtest, doom_multikey, hal_probe. The 11-row snapshots in this doc and in `FINAL_GATE_STATUS.md` are historical; body above kept intact, `kernelhwsrv/` untouched.
3. **`final_verify_strict.log` is stale — do not cite.** mtime 20:12, verdict `selfcheck FAIL (0.1s)`, `qemu_smoke FAIL (1.6s)`: poisoned-environment artifact (same class as the documented `LD_LIBRARY_PATH` breakage), not a real regression.
4. **Most recent authoritative verdict:** `p0_final_verify_strict_skip_rerun.log` (20:09) — `OVERALL: FAIL (7 pass, 4 skip)`. Newest evidence overall: the in-flight recovery run above.

**Verdict:** the doc's *numbers* are right, its *cause text* and *completeness* are not. Update §P0 wording to "wiped from /tmp, restored 20:13" and append the recovery verdict once PID 40967 exits.
