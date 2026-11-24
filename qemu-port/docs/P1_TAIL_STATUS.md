# P1 Tail Status — wsclient.h-include + user.mk-dep-token cleanup

Date: 2026-11-24 (read-only monitor pass; no edits outside this file; no QEMU; kernelhwsrv/ untouched)

## Verdict: PARTIALLY LANDED — reduced, not clean

P1 baseline was 24 matches / 6 files for the wsclient.h `#include` +
`user.mk` dep-token pattern. Current count:

- **10 matches / 5 files** (was 24 / 6):
  - `#include "wsclient.h"` — 4 matches in 4 files:
    `user/wsclient.c:11`, `user/wsdl.c:21`, `user/ws_e32_client.c:40`,
    `user/ws_e32_draw_test.c:21`
  - `user.mk` dep tokens — 6 matches: lines 1227, 1382, 1384, 1406, 1408, 1428
    (guilib.o, wsclient.o, wsdl.o, ws_e32_client.o, ws_e32_test.o, ws_e32_draw_test.o)

So the cleanup landed only in part: the count dropped ~58% but 4 source
includes and all 6 user.mk dep tokens still reference `wsclient.h`.

## Per-header refs in user/ (all matches, incl. comments)

- `wsclient.h`: wsclient.c 1, wsdl.c 1, ws_e32_client.c 5, ws_e32_draw_test.c 1,
  wsclient.h 1, user.mk 6
- `guishost.h`: dtcalc.c 1, dtedit.c 1, guilib.c 1, shot_e32.c 1, winsrv.c 3, user.mk 9
- `qemu_windowsrv.h`: winsrv.c 3, ws_e32_client.c 1, wsclient.h 2, user.mk 6

## Full P1 gate pattern (QE_EXEC_GUI_|QE_EXEC_WS_|guishost.h|wsclient.h|qemu_windowsrv.h)

57 matches / 13 files across `user/` + `ports/`; 9 files outside the winsrv
deletion set (user.mk, wsdl.c, ws_e32_client.c, ws_e32_draw_test.c, guilib.c,
dtcalc.c, dtedit.c, shot_e32.c, ports/doom/e32_platform.c). P1 (consumers
grep-clean) is therefore NOT green.

## r1_winsrv_delete.py dry-run (no --apply; script has no --dry-run flag, bare run is the dry run)

Plan only, rc=2 refusing without `--apply --confirm DELETE-WINSRV`:
10 exact-text edits (variant/qemu_variant.h, qemu_mouse.cpp, qemu_display.cpp,
kern_shell.cpp, qemu_variant_int.cpp) + 7 deletes (user/winsrv.c, guishost.h,
wsclient.c, wsclient.h, variant/qemu_windowsrv.{cpp,h}, qemu_gui_exec.cpp).

## mtimes (ls --time-style=full-iso)

- user/ws_e32_client.c — 2026-11-24 20:31:33 +0200
- user/user.mk        — 2026-11-24 20:58:20 +0200
- user/wsdl.c         — 2026-11-24 19:52:43 +0200

## Conclusion

The wsclient.h-include + user.mk-dep-token cleanup did NOT fully land:
10/24 matches and 5/6 files remain. Do not treat P1 as green; the winsrv
deletion gate (docs/WINSRV_DELETION_GATE.md §1) still fails.
