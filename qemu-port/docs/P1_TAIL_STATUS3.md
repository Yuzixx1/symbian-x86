# P1 tail status 3 — winsrv delete dry-run after include/dep-token cleanup

Date: 2026-11-24. Read-only monitor run; `kernelhwsrv/` untouched; no QEMU.

## Verdict: P1 still = 2 (was 24 before the include/dep-token cleanup attempt)

`python3 tools/r1_winsrv_delete.py --dry-run` reports:

```
P1 BLOCKED: 2 code match(es) outside the deletion set
(chunks A-D / ws_e32_client.c / shot migration must land first)
```

## Remaining P1 blockers (the only 2 code matches outside DELETE_SET)

| File | Line | Match |
|---|---|---|
| `user/ws_e32_client.c` | 40 | `#include "wsclient.h"` |
| `user/ws_e32_draw_test.c` | 21 | `#include "wsclient.h"` |

Both are in the migrated ws-client-lib files that must land first
(chunks A-D / shot migration); they are NOT in the deletion set, so the
script classifies them as P1 blockers rather than delete targets.

## user/ grep for wsclient.h | guishost.h | qemu_windowsrv.h

36 raw matches in `user/`; per the script's own P0 classification all are
`[ok]` except the 2 `[VIOLATION]` lines above. Breakdown:

- In DELETE_SET files (safe): `winsrv.c`, `wsclient.c`, `wsclient.h`,
  `wsdl.c`, `guilib.c`, `guishost.h` (includes + comments).
- `user/user.mk` dep rows (P2 edit targets, not P1): lines 100, 561, 1227,
  1260, 1301, 1316, 1368, 1382, 1384, 1406, 1408, 1428, 1536.
- Comments only: `dtcalc.c:17`, `dtedit.c:17`, `shot_e32.c:5`,
  `ws_e32_client.c:317/354/544/617`, `user.mk:1331/1346`.

## Not P1 = 0 yet

P1 reaches 0 only after the ws_e32_client.c / ws_e32_draw_test.c
`#include "wsclient.h"` lines are migrated (chunks A-D / shot migration
landing first). All other refs are deletion-set members, P2 dep rows, or
comments. Plan otherwise unchanged: 39/39 edit/delete checks ok,
apply still refused without `--apply` + gate PASS.
