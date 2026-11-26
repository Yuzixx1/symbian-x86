# Wave-2 summary — 2026-11-25

HEAD `ee8490e` throughout; work is uncommitted. No commits, no push, no `make clean`, no QEMU.

| Chunk | Result |
|---|---|
| G12 residue gate | Tool implemented (`tools/r1_shim_residue_sweep.py`) with live-keeper whitelist (43 allowed). `RESULT: FAIL (18 residue)`, rc 1 — expected pre-S2/S3. **Not wired into `final_verify`.** |
| P5 provenance | **PASS** — genuine 8/8 sourced, 4/6 shims retired, byte-ratio N/A (needs build artifacts). |
| Shim plan | `docs/WAVE2_SHIM_PLAN.md`: 7 live (`r1_fs_{core,server,backend,fat}*`) / 7 orphan r1_fs; S1/S2 ready, gates G12+R+F+S. |
| EFSRV-min build | Fixed and **SUCCESS** — fresh `EFSRVMIN.E32` 16,396 B at 21:37:03 (stale Nov-24 image replaced), `FATIOCLIENT.E32` 6,412 B. |
| Doc drift | Corrected in `GENUINE_PROGRESS.md` §1 and `REMAINING_SHIMS_AUDIT.md` (retired 4/6 now stated). |
| Clean-tree audit | Zero build-active stale refs for GUI/service shims; remaining mentions are comment-only. |

Gates unchanged: 3/3 strict logs `OVERALL: PASS (11 pass, 0 skip, 0 fail/not-run)`.

Open: 18 r1_fs residue must clear via S1–S3 before G12 can be wired (gate order G12 first in `DEFAULT_TESTS`).

2026-11-25 21:41:48: continuous run checkpoint — HEAD ee8490e, kernelhwsrv clean, 3/3 strict logs OVERALL PASS, index 716/716, no live r1_ processes. Wave-3 scope ready (WAVE2_NEXT_STEPS.md); not committed per D4.

2026-11-26 supersession note: the 11-row / 10-11-pass gate described above is historical. Current authority is 15 rows / FINAL27_RC=0 (15 pass, 0 skip, 0 fail, /tmp/opencode/final27_gate.log).
