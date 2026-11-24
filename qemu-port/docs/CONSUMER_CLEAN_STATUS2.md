# CONSUMER_CLEAN_STATUS2 — P1 winsrv consumer-match recheck after cleanup agent

Date: 2026-11-24. READ-ONLY monitor; no QEMU; dry-run only; kernelhwsrv/ git-clean (0 dirty lines).

- P1 grep (`QE_EXEC_GUI_|QE_EXEC_WS_|guishost\.h|wsclient\.h|qemu_windowsrv\.h`) in `user/` outside DELETE_SET (winsrv.c, guishost.h, wsclient.c, wsclient.h, wsdl.c, guilib.c): **24 line-matches / 6 files — still 24, NOT 0** (same as the "was 24" baseline; prior CONSUMER_CLEAN_STATUS.md saw 26/8 with a wider exclusion set).
- Per-file P1 residue: user/user.mk 15, ws_e32_client.c 5, ws_e32_draw_test.c 1, shot_e32.c 1, dtedit.c 1, dtcalc.c 1.
- Live (non-comment) refs: 15 — 13 `user/user.mk` dep-token rows (guishell/saver/breakout/guimenu/desktop/gcblittest + winsrv/wsclient/wsdl/ws_e32_* build rows) + `#include "wsclient.h"` at `ws_e32_client.c:40` and `ws_e32_draw_test.c:21`. Remaining 9 matches are comments only. Zero `QE_EXEC_GUI_*`/`QE_EXEC_WS_*` exec calls remain outside DELETE_SET.
- `tools/r1_winsrv_delete.py` dry-run (no args): prints plan — 30 edits + 9 DELETE_SET deletes (incl. user/winsrv.c, user/guishost.h, user/wsclient.*, user/wsdl.c, user/guilib.c, variant/qemu_windowsrv.*, variant/qemu_gui_exec.cpp), refuses rc=2 without `--apply --confirm DELETE-WINSRV`; no writes made by this monitor.

**Verdict: P1 still N = 24 line-matches / 6 files (> 0).** Cleanup agent removed all live exec refs but dep tokens and 2 wsclient.h includes remain; P1 gate not passable until user.mk rows are re-pointed and the two includes dropped (both covered by the pending DELETE_SET delete plan).
