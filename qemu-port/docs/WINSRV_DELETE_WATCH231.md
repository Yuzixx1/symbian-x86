# WINSRV_DELETE_WATCH231 — isolated qemu_smoke re-run status (2026-11-25 08:13)

- Context: post-winsrv-delete final_verify (Nov 24 22:40, /tmp/opencode/final_verify_post_winsrv_delete.log) = 10 PASS / 1 FAIL; sole fail was qemu_smoke ("RESULT line: FAIL") while ewsrv/eshell/fat_io/doom×4 all PASS → suspected QEMU-slot contention, not a real regression.
- Isolated re-run launched 08:12: `python3 tools/r1_qemu_smoke.py > /tmp/opencode/r1_qemu_smoke_rerun.log` (timeout 300; single qemu-system-i386 PID 1696479, timeout 50).
- Status at 08:13: STILL RUNNING — log holds only mkfs.fat output, 0 RESULT lines, 3 smoke procs alive, no competing QEMU instances (contention cleared).
- Verdict: PASS not yet confirmed; re-run pending. kernelhwsrv/ untouched (git status clean, 0 changes). origin/main = 94ce9c9 (winsrv/GUI retirement, gates PASS, APPLY_RC=0).
