# DELETE_TOOL_STATUS4 — did tools/r1_winsrv_delete.py land?

Date: 2026-11-24. Read-only monitor pass; no QEMU; no apply; kernelhwsrv/ untouched
(git status on kernelhwsrv/: 0 dirty lines).

## Result: YES, the tool landed

- `ls -la tools/r1_winsrv_delete.py` → present, 42354 bytes, 2026-11-24 21:09.
- `python3 -m py_compile tools/r1_winsrv_delete.py` → OK (rc=0).

## Behavior checks (dry-run only, never applied)

- No args → full dry-run preview plan (P1/P2/P3, DELETE_SET, gates) printed,
  ends `REFUSE: --apply not set -- no file changed.` → rc=0.
- `--apply` without `--confirm` token → `REFUSE: --apply requires --confirm
  RETIRE-WINSRV-<ts>. Nothing changed.` → rc=2 (matches the tool's own usage
  doc, line 67: "--apply # REFUSED rc=2 (no token)").
- No `--apply` was ever executed; no file was modified by these checks.

## Preview findings (why apply would refuse today)

- P1 consumer grep: BLOCKED — 24 match(es) outside DELETE_SET
  (dtcalc.c, dtedit.c, shot_e32.c, user.mk rows, ws_e32_client.c,
  ws_e32_draw_test.c — chunks A–D / shot.c migration must land first).
- Edit/delete checks: 38/39 ok; 1 NOT ok — `[AMBIGUOUS] variant/qemu_mouse.cpp:302`
  (expected 1 occurrence, got 2). Apply would refuse on this.
- Gates are preview-skipped; a real apply re-runs the full gate set
  (retire 4/4, fullchain, final_verify 11/11).

## Safety rails confirmed in source

- FORBIDDEN argv guard (commit/push/add/checkout/reset/clean/stash, rm -rf,
  make clean) → refuse rc=2 (main, ~line 782).
- `--dry-run` + `--apply` combo refused rc=2; wrong token refused rc=2.
- Apply requires `--apply --confirm RETIRE-WINSRV-<ts>` (DELETE-WINSRV also
  accepted) and a real pre-apply gate PASS.

Conclusion: the self-contained minimal delete tool is in place, compiles,
plans correctly, and safely refuses to apply without a token + gate pass.
