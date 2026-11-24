# WINSRV_ENDGAME_STATUS — read-only snapshot, 2026-11-24 ~20:30 EET

Monitor pass per docs/WINSRV_DELETION_GATE.md. No QEMU, no commit/push, no
`make clean`, `kernelhwsrv/` untouched. HEAD `0f9ec6a`, tree dirty (concurrent agents).

## 1. Files still holding live winsrv refs in `user/` (grep QE_EXEC_GUI_|guishost.h|gui_shared|QE_EXEC_WS_|wsclient.h)

| File | mtime | State |
|---|---|---|
| `user/shot.c` | 2026-11-20 20:05 (old) | **NOT migrated** — live `#include "guishost.h"`, `struct gui_shared* gSh`, `exec2(QE_EXEC_GUI_DRAW/FBBASE/FBSIZE)` |
| `user/ws_e32_draw_test.c` | 20:12:38 | live `#include "wsclient.h"` + `"guishost.h"` + `exec0(QE_EXEC_GUI_FBBASE)` |
| `user/winsrv.c`, `guishost.h`, `wsclient.c`, `wsclient.h`, `wsdl.c` | 19:46–Nov 21 | the deletion set itself (gate §2) |
| `user/user.mk` | 20:22:10 | 20 hits but rule bodies migrated; residual = stale dep comments (`guishost.h` still listed as dep for guishell/saver/shot/breakout/guimenu/desktop objects) |
| `user/desktop.c` | **20:22:22** | **chunk D4 LANDED** — now `#include "ws_e32_client.c"`, zero guishost/QE_EXEC_GUI refs |
| `user/guilib.c` (20:23:34), `guihello.c` (19:59), `dtcalc.c`/`dtedit.c` (20:00), `saver.c` (20:02), `ws_e32_client.c` (20:12), `fbblittest.c`, `shot_e32.c` | — | comment-only mentions; migrated |

## 2. Build wiring / guilib Connect

- USERMK_CONSOLIDATION **landed**: WSDL.E32 now links `ws_e32_client.o` + pack_multi (user.mk:1384-1387), SHOTE32 rule added (:1440-1442) via `tools/build_ews_app.py` (exists, 20:15), DESKTOP.E32 = pack_multi `--ordinals 211,1547,120,915,1548`; `make -n userland` resolves DESKTOP cleanly.
- **Connect r=-28 UNFIXED**: `chunkb_guihello.log` (20:19) `WSE32: Connect r=-28`; `dt4_desktop.log` (20:23/24) `DT: Connect r=4294967268` (=-28). **No log anywhere shows Connect r=0.** Root cause per `docs/EWSRV_CLIENT_BLOCKER.md:40-59`: `!`-prefix name mismatch port-side (`ws_main.cpp`/`co_cli.cpp` literal vs `!`); E32Run already grants ProtServ — port-side image fix pending, not kernel.

## 3. Newest log verdicts (2026-11-24 20:2x)

| Log | Time | Verdict |
|---|---|---|
| `final_verify_slot269_retry.log` | 20:23 | selfcheck **FAIL**, qemu_smoke PASS — OVERALL FAIL |
| `final_verify_genuine_retire.log` | 20:23 | retire gates **4/4 PASS** (efsrv/ewsrv/estart/euser) |
| `r1_retire_ewsrv.log` | 20:23 | genuine EWSRV boots r=0, scheduler READY (server side healthy) |
| `final_verify_genuine_ewsrv.log` | 20:24 | mid-run (EWSRV.E32+EWCLIENT.E32 restaged, fs img built) — **no verdict yet** |
| `dt4_desktop.log` | 20:23 | desktop loads r=0 (5 imports), runs, but Connect r=-28 |
| `r1_os_selfcheck_cp4_stress.log` | 20:21 | died at `make` exit 2 (tree busy) — no verdict; 19:45 cp4 log also verdict-less |
| last strict final_verify full PASS | 03:55 / 19:51 run | 11/11 (per WORKSTREAM_STATUS / CRITICAL_PATH_STATUS) |

## 4. Blockers before winsrv deletion (gate P1-P6)

1. **P1 incomplete**: `shot.c` + `ws_e32_draw_test.c` still live on execs 249/250/251 (desktop now clean).
2. **Client Connect r=-28** blocks every migrated GUI app at runtime (guilib chunk B, desktop D4) — winsrv gate P2/P3 cannot pass until the port-side `!E32WindowServer` name fix lands and a boot shows Connect r=0.
3. **Keyboard-injection KERN-8 #PF** (cr2=0xf0000010, EWSRV pid=9) still open per CRITICAL_PATH_STATUS (P2 keyboard arm).
4. **cp4_stress / slot 269** regression unresolved (selfcheck FAIL in newest final_verify run).
5. Kernel-side deletion set (variant/qemu_windowsrv.*, qemu_gui_exec.cpp, exec table 274→271, Makefile rows) untouched — correctly, gate is human-gated, no apply path.
