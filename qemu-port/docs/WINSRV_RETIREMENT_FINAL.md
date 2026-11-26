# WINSRV Retirement — Final Closeout

Single authoritative closeout for the GUI/winsrv retirement; supersedes the
`WINSRV_DELETE_WATCH*` steady-state reports.

## Decision (2026-11-24)
- GUI/winsrv desktop failed; console shell + DOOM/Freedoom are the UI.
- Genuine EWSRV remains the target window server if the GUI returns.

## Change
- Deleted 7 files: `user/winsrv.c`, `user/guishost.h`, `user/wsclient.c`,
  `user/wsclient.h`, `variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h`,
  `variant/qemu_gui_exec.cpp`.
- Kept: `wsdl.c`, `guilib.c`, fbblit exec 270, KBINJ EWSRV bridge,
  `ws_e32_client.c`, netstack (legitimate x86/PC bridges).
- `kernelhwsrv/` untouched (READ-ONLY honored; `git status --short` clean).

## Apply
- `tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`:
  `APPLY_RC=0`, P1–P3 gates PASS.
- Snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`
  (rollback token `ROLLBACK-WINSRV-20261124_215436`) unused.

## Verification
- Post-deletion `r1_final_verify --strict-skip`: 11/11 PASS at apply time.
- One redundant re-run: 10/11, `qemu_smoke` FAIL at 50s harness cap under
  load ~4 (`exit 124`); isolated re-run `RESULT: PASS RC=0` on a free slot.
- Effective result: 11/11 PASS — no product regression.

## Delivery
- Commit `94ce9c9fce60b8202eda2317182edb5ed7a1db4` pushed to `origin/main`
  (`0f03f7e..94ce9c9`).
- `GO_NO_GO.md`, `CLEANUP_LOG.md`, `FINAL_GATE_STATUS.md`: GREEN.
- Steady state confirmed; no pending verification or rollback needed.

## Supersession (2026-11-26)

The historical 11-row gate recorded above is superseded and no longer current.
Current gate: 15 rows, `FINAL27_RC=0` - 15 pass, 0 skip, 0 fail
(`/tmp/opencode/final27_gate.log`).
