# P1_TAIL_STATUS2 — final P1 count after the include/dep-token cleanup attempt

Date: 2026-11-24. READ-ONLY monitor; no QEMU; dry-run only; kernelhwsrv/ git-clean (0 dirty lines).

- `python3 tools/r1_winsrv_delete.py` (no args = dry-run): prints plan — 11 exact-text EDITS (qemu_variant.h x3, qemu_mouse.cpp x5, qemu_display.cpp, kern_shell.cpp, qemu_variant_int.cpp) + 7 DELETE_SET deletes (user/winsrv.c, user/guishost.h, user/wsclient.c, user/wsclient.h, variant/qemu_windowsrv.cpp/.h, variant/qemu_gui_exec.cpp); refuses rc=2 without `--apply --confirm DELETE-WINSRV`; no writes made.
- P1 grep (`QE_EXEC_GUI_|QE_EXEC_WS_|guishost\.h|wsclient\.h|qemu_windowsrv\.h`) in `user/` outside DELETE_SET (winsrv.c, guishost.h, wsclient.c, wsclient.h, wsdl.c, guilib.c): **24 line-matches / 6 files — still 24, NOT 0** (unchanged vs CONSUMER_CLEAN_STATUS2.md; the include/dep-token cleanup attempt did not move the count).
- Per-file P1 residue: user/user.mk 15, ws_e32_client.c 5, ws_e32_draw_test.c 1, shot_e32.c 1, dtedit.c 1, dtcalc.c 1.
- Narrow grep (`wsclient\.h|guishost\.h|qemu_windowsrv\.h`) across `user/` incl. DELETE_SET: 36 line-matches / 11 files; outside DELETE_SET: ws_e32_draw_test.c 1, shot_e32.c 1, ws_e32_client.c 5, user.mk 15, dtedit.c 1, dtcalc.c 1 = 24. Live (non-comment) residue: 13 user.mk dep rows + 2 `#include "wsclient.h"` (ws_e32_client.c:40, ws_e32_draw_test.c:21); zero live `QE_EXEC_GUI_*`/`QE_EXEC_WS_*` calls outside DELETE_SET.

**Verdict: P1 still N = 24 line-matches / 6 files (> 0).** P1 gate remains not passable until the user.mk dep rows are re-pointed and the two wsclient.h includes dropped (covered by the pending winsrv delete plan, not yet applied).
