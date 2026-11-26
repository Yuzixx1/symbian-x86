# WINSRV_DELETE_WATCH229 — post-deletion r1_final_verify.py --strict-skip

Log: `/tmp/opencode/final_verify_post_winsrv_delete.log` (43 lines, run finished, 0 procs left).
HEAD: `94ce9c9` (winsrv/GUI delete on origin/main); `kernelhwsrv/` clean (0 modified).

| test | result | secs |
|---|---|---|
| selfcheck | PASS | 236.7 |
| qemu_smoke | **FAIL** | 50.1 |
| genuine_retire | PASS | 47.4 |
| genuine_efsrv | PASS | 16.5 |
| genuine_ewsrv | PASS | 17.8 |
| genuine_eshell | PASS | 20.7 |
| genuine_fat_io | PASS | 15.7 |
| doom_main | PASS | 75.7 |
| doom_audio | PASS | 33.9 |
| doom_playtest | PASS | 71.7 |
| doom_multikey | PASS | 65.1 |

OVERALL: FAIL (10 pass, 0 skip, 1 fail) — failed: qemu_smoke (E32Validate/E32LoadDll/e32run markers all FAIL; RESULT: FAIL).

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.
