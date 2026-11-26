# DOOM_WINSRV_AUDIT — how DOOM uses the winsrv fallback vs fbblit — 2026-11-24

**Scope:** READ-ONLY audit. No source edits, no QEMU runs, no commit/push,
`kernelhwsrv/` untouched. Question answered: exactly which DOOM code paths call
exec 251 (winsrv display list) vs exec 270 op 3 (fbblit), under what condition,
and what deleting the 251 path means per `docs/WINSRV_MIGRATION_PLAN.md`
(DOOM = class c: keep fbblit primary, delete the 251 fallback **last**).

## 1. Call sites (all in `ports/doom/e32_platform.c`; `user/doome32.c` is clean)

| Site | Line | Exec | Role |
|------|------|------|------|
| `#include "guishost.h"` | 31 | — | only for the GC_RECT fallback struct/opcodes |
| `DG_Init` centering query | 136 | **270 op 2** `QE_EXEC_FBCTL/FBO_FBINFO` | gets LFB geometry → `sXOff/sYOff`. **Not winsrv** (svcmgr-owned, survives winsrv deletion) |
| `blit_via_gcrect()` | 183–211 | **251** `QE_EXEC_GUI_DRAW` | the ONLY exec-251 user in DOOM: `arg1==0` → `struct gui_shared*`, then builds a 40×25 = 1000-`GC_RECT` mosaic (sampled center pixels of the 640×400 RGBA frame), flushing at `GUI_MAX_CMDS` (640) via `arg1>0` |
| `DG_DrawFrame` primary blit | 245 | **270 op 3** `QE_EXEC_FBCTL/FBO_FBBLIT` | one bulk 640×400×4 32bpp blit of `DG_ScreenBuffer` to the VBE LFB |
| `DG_DrawFrame` fallback branch | 246–250 | **251** (indirect) | `if (r != 0) { marker; blit_via_gcrect(); }` |

`user/doome32.c`: no `guishost.h` / `QE_EXEC_GUI_*` / 251 / 270 references —
the launcher is display-agnostic. No `ws_`/`wsclient` usage anywhere under
`ports/doom/` or in `user/doome32.c`.

## 2. Condition that selects the 251 path

Exactly one: `exec2(QE_EXEC_FBCTL, FBO_FBBLIT, &req) != 0` in `DG_DrawFrame`
(e32_platform.c:245–250). In the current build exec 270 op 3 is always
installed (`SvcMgrCtlExec` in `e32load.cpp:1266-1275`, kernel side
`variant/qemu_fbblit.cpp`), so the fallback is **dead code in practice**; the
`DOOM: fbblit unavailable, GC_RECT fallback` marker (emitted only on frame 1)
never appears in any current log. There is no "no window server" runtime
detection — the fallback is purely "fbblit exec failed", not a window check.

## 3. Do the DOOM tests exercise exec 251?

**No.** Grep over `tools/r1_doom_*.py`: the only display assertions are
`DOOM: FRAME` / `DOOM: frame N` markers plus QMP screendump PNGs — all produced
by the fbblit path. No test greps for `fbblit unavailable` / `GC_RECT`, and none
forces an fbblit failure to drive the 251 path. `docs/DOOM_POST_UID_VERIFICATION.md`
(2026-11-24) records all four tests PASS
(`r1_doom_test.py`, `r1_doom_audio_test.py`, `r1_doom_playtest.py` 140→2205
frames, `r1_doom_multikey_test.py` 8/8) with `DISPLAY: VBE mode 0x118`, zero
Exc/Panic/FAULT/#PF — i.e. **passing via exec 270 op 3, 251 never taken**.

## 4. Migration step (what deleting 251 means for DOOM)

Per `WINSRV_MIGRATION_PLAN.md` §2 class (c), option (i) — **delete, do not
reroute**:

1. Remove `#include "guishost.h"` (e32_platform.c:31), `blit_via_gcrect()`
   (183–211), and the fallback branch (246–250) including the frame-1 marker.
2. Keep `DG_DrawFrame` on exec 270 `FBO_FBBLIT` and `DG_Init` on exec 270
   `FBO_FBINFO` — both are svcmgr-owned, **outside** the winsrv deletion set,
   and stay until the separate svcmgr step.
3. This makes `ports/doom/` grep-clean of `guishost.h`/`QE_EXEC_GUI_*`,
   satisfying one of the winsrv-deletion gate's grep-clean conditions
   (`WINSRV_MIGRATION_PLAN.md` §4). Done **last**, after all other consumers,
   playtest-gated.

## 5. Gate to keep after removing the 251 path

- `r1_doom_test.py`, `r1_doom_audio_test.py`, `r1_doom_playtest.py`,
  `r1_doom_multikey_test.py` — **all 4 PASS** post-edit.
- Graphical frame **byte-for-byte unchanged** (retire-plan extra condition):
  QMP screendump PNGs must match the fbblit-produced baseline — guaranteed
  because the deleted code was unreachable, but re-verify via screendump.
- `r1_final_verify.py --strict-skip` 11/11 and
  `r1_genuine_fullchain_test.py` 14/14 still PASS.

## 6. Risks

- **Black screen on fbblit failure:** after deletion, an `FBO_FBBLIT` error
  leaves no fallback renderer (only the marker). Acceptable: exec 270 op 3 is
  unconditionally installed in this build; a failure would already be a
  boot-level regression caught by the marker itself.
- **Do not touch exec 270:** it retires with svcmgr, a separate later step;
  DOOM must keep `qemu_fbblit.h` + `QE_EXEC_FBCTL` regardless of winsrv deletion.
- **Test-neutral change:** no test asserts the fallback marker, so removal
  cannot break a marker check; the only observable difference would be its
  absence, which is the goal.
- **Ordering:** this deletion is the final winsrv-gate item for DOOM; doing it
  before the other ~12 GUI consumers are migrated would leave `guishost.h`
  alive anyway — no harm, but no benefit either. Winsrv files themselves
  (`user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.*`,
  `variant/qemu_gui_exec.cpp`) are only removed at the full gate with human
  review (no apply path).

**CORRECTION 2026-11-26 — winsrv/GUI is gone; this audit is historical.**
`user/winsrv.c`, `user/guishost.h`, `user/wsclient.c`, `user/wsclient.h`,
`variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h` and
`variant/qemu_gui_exec.cpp` were deleted in commit `94ce9c9` (all 7 verified
absent from the working tree), so the "winsrv live" premise of §1–§6 — and the
§6 note that they "are only removed at the full gate" — is superseded. The
DOOM 4-test block this document gates (`doom_main`, `doom_audio`,
`doom_playtest`, `doom_multikey`) **PASSES** inside the 15-row gate at
**15/15** (`OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`,
`/tmp/opencode/final27_gate.log`) — correcting the earlier `final13` / 13/13
figure quoted here, which is stale.
