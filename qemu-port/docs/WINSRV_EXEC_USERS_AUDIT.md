# WINSRV_EXEC_USERS_AUDIT — exact consumers of execs 249/250/251 and 271/272/273

**Status:** READ-ONLY audit, 2026-11-24 (post wsclient chunk 1). No edits except
this file. No QEMU runs, no commit/push, `kernelhwsrv/` untouched.
Sources: grep over `user/` + `ports/`; op meanings from
`variant/qemu_gui_exec.cpp` (249=LFB phys base, 250=LFB size, 251=multiplexed
display list: arg1==0 → `gui_shared*`, arg1>0 → run N cmds) and
`variant/qemu_windowsrv.{cpp,h}` (271=WS_BASE, 272=WS_SIZE, 273=WS_CTL over
`ws_shared` + 64-entry event ring; WSO_* op codes in `user/winsrv.c:70-73`,
`user/wsclient.c`).

## 1. Per-binary table (14 user.mk targets + notes)

| # | user.mk target | execs 249/250/251 call sites | execs 271/272/273 call sites | migrated? |
|---|----------------|------------------------------|-------------------------------|-----------|
| 1 | GUISHELL.E32 | 251 `guishell.c:122,134,149`; 249 `:143`; 250 `:145` | — | no |
| 2 | SAVER.E32 | 251 `saver.c:178,196`; 249 `:190`; 250 `:192` | — | no |
| 3 | SHOT.E32 | 251 `shot.c:131,151`; 249 `:142`; 250 `:143` | — | no |
| 4 | GUIHELLO.E32 | raw 249 `guihello.c:82`, 250 `:84`; + guilib.o: 249/250/251 `guilib.c:84-86,249` | — | no |
| 5 | BREAKOUT.E32 | 251 `breakout.c:323,345`; 249 `:335`; 250 `:340` | — | no |
| 6 | GUIMENU.E32 | 251 `guimenu.c:62,347`; 249 `:337`; 250 `:343` | — | no |
| 7 | DESKTOP.E32 | 251 `desktop.c:74,700`; 249 `:691`; 250 `:692` | — | no |
| 8 | DTEDIT.E32 | 251 `dtedit.c:386`; + guilib.o (as #4) | — | no |
| 9 | DTCALC.E32 | 251 `dtcalc.c:315`; + guilib.o (as #4) | — | no |
| 10 | GCBLITTEST.E32 | 251 `gcblittest.c:102,130` | — | no |
| 11 | WINSRV.E32 (server) | 251 `winsrv.c:156,601` | 273 `winsrv.c:432,443,511,570` (WSO_FRAME_TICK/POP_EVENT/REGISTER_SERVER) | n/a — delete, not migrate |
| 12 | WSDL.E32 | — (no direct execs in `wsdl.c`) | via wsclient.o: 271 `wsclient.c:99`; 272 `:100`; 273 `:158,181,271,281` (WSO_CREATE/DESTROY/SET_DIRTY/POP_EVENT) | no (chunk 1 = wsclient backend only) |
| 13 | FBBLITTEST.E32 | — | — | **outside set**: exec 270 ops 2/3 only (`fbblittest.c:44,76,85`, `qemu_fbblit.h`) — retires with svcmgr |
| 14 | DOOM.E32 | 251 fallback `ports/doom/e32_platform.c:188,207,209` (`blit_via_gcrect`) | — | DOOM-special: primary = exec 270 op 3 `e32_platform.c:136,245` (FBO_FBBLIT); only the 251 fallback is winsrv-bound |

Notes: `user/shot_e32.c` (exec 270 only) has **no user.mk target** — orphan
source, outside the set. **WSE32TEST.E32** (`user/ws_e32_client.c`,
user.mk:1393-1400) is the **already-EWSRV** reference client — RWsSession/
RWindow/GetEvent, zero winsrv execs; it is the migration model, not a consumer.
No `user/` or `ports/` file other than those listed touches 249/250/251/271/272/273.

## 2. Classification

- **Already-EWSRV:** WSE32TEST.E32 (ws_e32_client.c) — done, no winsrv execs.
- **Chunk-assigned (class b, swap guishost/guilib/wsclient backend → RWsSession):**
  #1-#10, #12. Shared guilib.o serves #4/#8/#9, so one guilib port covers three.
- **Delete-not-migrate:** #11 WINSRV.E32.
- **Outside winsrv deletion set:** #13 FBBLITTEST.E32 (exec 270, svcmgr-owned).
- **DOOM-special:** #14 — keep fbblit primary; delete the exec-251 GC_RECT
  fallback only (frame must stay byte-identical), last, playtest-gated.

## 3. Recommended chunk split (3-4 binaries per chunk)

- **Chunk A (lib pilot):** WSDL.E32 + wsclient.c port → ews client lib
  (RWsSession/RWindowGroup/RWindow/GetEvent); EWCLIENT pack pattern. Covers #12.
- **Chunk B (guilib backend + its 3 apps):** guilib.c → GUIHELLO.E32,
  DTEDIT.E32, DTCALC.E32 (#4/#8/#9). One lib port, three front-ends.
- **Chunk C (simple 249/250/251 apps, no input):** SAVER.E32, SHOT.E32,
  GCBLITTEST.E32 (#2/#3/#10).
- **Chunk D (interactive, needs TWsEvent input path):** GUISHELL.E32,
  GUIMENU.E32, DESKTOP.E32, BREAKOUT.E32 (#1/#6/#7/#5) — blocked on the
  EWSRV input-injection bridge (plan §5).
- **Chunk E (terminal):** DOOM.E32 fallback removal (#14) + WINSRV.E32 deletion,
  after the winsrv deletion gate in `docs/WINSRV_MIGRATION_PLAN.md` §4.

**2026-11-26 — correction (supersedes this file's live/pending winsrv-or-GUI premise):** the winsrv/GUI desktop was **RETIRED** in `94ce9c9` — 7 files deleted (`user/winsrv.c`, `user/guishost.h`, `user/wsclient.c`, `user/wsclient.h`, `variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h`, `variant/qemu_gui_exec.cpp`), all confirmed gone from disk — so execs 249/250/251, the 271/272/273 event ring, and the WINSRV launch path no longer exist; the console shell + DOOM/Freedoom are the UI, and genuine **EWSRV** is the target when the GUI returns (per `AGENTS.md` lines 99-103), which makes §3 chunks B-E above historical rather than pending work; the authoritative gate is the 15-row roster, 15/15 PASS (`OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`, `/tmp/opencode/final27_gate.log`) at HEAD `ee8490e` with 0 commits after it.
