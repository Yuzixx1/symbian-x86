# WINSRV_DELETE_WATCH230 — post-winsrv-deletion r1_final_verify.py --strict-skip

Log: `/tmp/opencode/final_verify_post_winsrv_delete.log` (43 lines, run finished, 0 procs left).
HEAD: `origin/main` = 94ce9c9 (winsrv/GUI retired).

| test | result | time |
|---|---|---|
| selfcheck | PASS | 236.7s |
| qemu_smoke | **FAIL** | 50.1s |
| genuine_retire | PASS | 47.4s |
| genuine_efsrv / ewsrv / eshell / fat_io | PASS | 16.5/17.8/20.7/15.7s |
| doom_main / audio / playtest / multikey | PASS | 75.7/33.9/71.7/65.1s |

OVERALL: **FAIL (10 pass, 0 skip, 1 fail)**; failed: qemu_smoke. RC nonzero.
qemu_smoke failure: E32Validate/E32LoadDll of staged EUSER.DLL (r=0) — boot never reached ProcessCreate; only "no Exc/PanicKernExec" passed. kernelhwsrv/ untouched.

---

**2026-11-26 — superseded.** The historical 11-row gate above is superseded by the 15-row
gate: FINAL27_RC=0 (15 pass, 0 skip, 0 fail), log `/tmp/opencode/final27_gate.log`. The body
above is retained unchanged for historical record only.
