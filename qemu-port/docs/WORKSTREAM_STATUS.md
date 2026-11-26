# WORKSTREAM_STATUS — read-only snapshot, 2026-11-24 ~20:25 EET

Read-only monitor pass. No edits besides this file; no QEMU, no commit/push, `kernelhwsrv/` untouched. HEAD `0f9ec6a` on `main`; tree dirty (many `user/`, `variant/`, `docs/` modified by concurrent agents).

| Workstream | Status | Evidence | Next |
|---|---|---|---|
| guilib chunk B (`user/guilib.c` et al.) | Sources migrated; runtime connect failing | `guilib.c` mtime 20:11, header claims execs 249/250/251 gone; `chunkb_guihello.log` (20:19) shows `WSE32: Connect r=-28` | Diagnose r=-28 on EWSRV session connect in wse32 path |
| desktop D4 (`user/desktop.c`) | Not migrated | mtime 2026-11-21 (old); still `#include "guishost.h"` + `QE_EXEC_GUI_*` at lines 24/74/691/692/700; no desktop test log in /tmp/opencode | Migrate to EWSRV client path like dtcalc/dtedit, then gate |
| user.mk consolidation | Report done; rules being applied elsewhere | `docs/USERMK_CONSOLIDATION.md` exists (20:13, 11.6 KB); `user/user.mk` mtime 20:16:54 (edited after report) | Re-verify line numbers post-edit; shot_e32.c still has no rule |
| cp4_stress retry | Blocked — real regression, not stale objects | `CP4_STRESS_RECURRENCE.md`: exec 269 retired in `dd22ae2`, `killproc.c:26` now hits genuine slot 269; 20:19 retry log died at `make` (exit 2, tree busy); newest cp4 log 19:45 has no verdict | Re-target cp4 peer kill off exec 269, then rerun |
| winsrv exec refs in `user/` | 4 files still live | `shot.c`, `winsrv.c`, `desktop.c`, `ws_e32_draw_test.c` still include `guishost.h`/call `QE_EXEC_GUI_*`; `guilib.c`, `guihello.c`, `dtcalc.c`, `dtedit.c`, `saver.c` migrated | Migrate shot/desktop/ws_e32_draw_test, then delete winsrv.c per WINSRV_DELETION_GATE.md |

Gate state: strict final_verify 03:55 = OVERALL PASS 11/11; authority run 19:18 = OVERALL FAIL (9 pass, selfcheck+ewsrv fail); 20:19 slot269 retry in progress (selfcheck running at snapshot time).

---

**SUPERSEDED 2026-11-26.** The 11-row / 10-11-pass strict gate asserted above is historical and no longer current. Current authority is **15 rows / `FINAL27_RC=0`** — 15 pass, 0 skip, 0 fail — recorded in `/tmp/opencode/final27_gate.log`. The body above is preserved byte-identical for audit history.
