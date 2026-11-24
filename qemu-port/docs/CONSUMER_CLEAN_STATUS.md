# CONSUMER_CLEAN_STATUS — P1 winsrv consumer-match cleanup check

Date: 2026-11-24 (~now). READ-ONLY monitor; no QEMU; dry-run only; kernelhwsrv/ git-clean (0 dirty lines).

- P1 grep (`QE_EXEC_GUI_|QE_EXEC_WS_|guishost\.h|wsclient\.h|qemu_windowsrv\.h`) in `user/` outside DELETE_SET: **26 line-matches / 8 files now vs 24 / 6 files before** (baseline docs/DELETE_TOOL_STATUS4.md).
- Live (non-comment) refs left: **17** — `#include "wsclient.h"` in `ws_e32_client.c:40`, `ws_e32_draw_test.c:21`, `wsdl.c:21` + 14 `user/user.mk` dep-token rows (guishell/saver/guilib/breakout/guimenu/desktop/gcblittest/ws_e32_*/winsrv/wsclient/wsdl). Remaining 9 matches are comments only (shot_e32.c, dtedit.c, dtcalc.c, guilib.c, user.mk notes).
- Zero `QE_EXEC_GUI_*`/`QE_EXEC_WS_*` exec calls remain in `user/` outside DELETE_SET — all GUI exec usage is migrated; residue is header names + make deps.
- Owner-file mtimes (cleanup agent active today): user.mk 20:58:20, guilib.c 20:58:16, ws_e32_client.c 20:31:33, ws_e32_draw_test.c 20:31:41, dtcalc.c 20:00:32, dtedit.c 20:00:28, wsdl.c 19:52:43, shot_e32.c 19:48:03. DELETE_SET files untouched (winsrv.c/guishost.h still Nov 21).
- `tools/r1_winsrv_delete.py` dry-run: plan prints (12 edits, 7 deletes), refuses rc=2 without `--apply --confirm`; no writes made.

**Verdict: IN PROGRESS, P1 still BLOCKED (26 > 0).** Cleanup agent cleared all live exec refs; what remains is 3 wsclient.h includes + 14 user.mk dep tokens (+ comments). P1 gate not passable until those are re-pointed/dropped.
