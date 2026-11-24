# WINSRV_DELETION_EXECUTOR — guarded executor spec for the winsrv deletion

**Status:** READ-ONLY planning doc, 2026-11-24. This file is the only edit. No QEMU
runs, no commit/push, no `make clean`, `kernelhwsrv/` untouched. It specifies the
tool that will one day execute `docs/WINSRV_DELETION_GATE.md`, modeled on the
proven `tools/r1_shim_retire_apply.py` (token-gated, snapshots, auto-rollback,
real-gate checks). All line refs below were verified by read-only grep on
2026-11-24.

---

## 1. Decision: new `tools/r1_winsrv_delete.py`, reusing helpers — do NOT extend `--shim winsrv`

**Recommendation: a new tool `tools/r1_winsrv_delete.py` that imports the helper
functions from `r1_shim_retire_apply.py` (and the gate parser from
`r1_shim_retire_plan.py`), rather than adding a winsrv mode to the existing tool.**

Rationale:

- `r1_shim_retire_apply.py`'s HARD CONTRACT (lines 36-37) says it "NEVER deletes
  files", and its `FORBIDDEN` substring scan (line 78) rejects `os.remove`,
  `os.unlink`, `pathlib.Path.unlink`, `rm -rf` appearing anywhere in argv. Adding
  file deletion to that tool means weakening its own safety contract; the winsrv
  step is the one shim whose retirement *is* deletion + multi-file edits, so it
  gets its own tool with its own (stricter) allowlist.
- The old tool's model is one shim → one exact-text edit set against
  `plan.BOOT_LIST_LINES`, applied only after a single pre-gate. Its own
  `NO_EDIT_NOTE["ewsrv"]` already says winsrv has "no safe boot-line edit" and is
  retired "by removing its files". The winsrv deletion set spans ~7 files across
  three phases (hooks → build wiring → sources, per gate doc §4), each phase
  needing its own rebuild + gate. That is a different apply engine, not a new
  index into `BOOT_EDIT_INDEX`.
- What *is* reused by import (do not copy): `sha256`, `git_porcelain`,
  `kernelhwsrv_clean`, `safe_target`, `check_edits` (exactly-one-occurrence
  assertion), `run_gate` pattern (`r1_genuine_retire_test.py --only ewsrv`,
  dry-run vs real), the snapshot manifest schema + `ROLLBACK-…-<ts>` token
  pattern, and the verified staged-temp + `os.replace` all-or-nothing rollback
  pattern from `do_rollback`.

### Extension needed vs. the existing tool (the delta the new tool adds)

1. **Op model with two op types.** Each op is `{type, path, old, new}` for
   `edit` (exact-text replace, uniqueness asserted, like `check_edits`) or
   `{type: "delete", path}` for file removal. Deletes are allowlisted: the path
   must be in the static `DELETE_SET` below, pass `safe_target()` (inside
   `qemu-port/`, never under `kernelhwsrv/`), and `git status --porcelain` on it
   must be clean.
2. **Snapshot-before-delete.** `make_backup` is extended so every delete-target
   is copied to the backup dir *before* removal; the manifest records its
   sha256. Rollback therefore recreates deleted files (the old `do_rollback`
   already restores via staged temp + `os.replace`, which works when the
   destination is missing — just extend `allowed_rollback_paths` to include
   delete paths).
3. **Multi-phase apply.** Phases run in fixed order P1 hooks/exec-table →
   P2 build wiring → P3 source deletion (§3 below). Between phases an
   incremental `make` (never `make clean`) and the phase gate run; any failure
   auto-rolls back *all* touched files (edits + deletes) and re-runs the gate.
4. **Token + dry-run contract kept identical:** default is a pure preview
   (gate `--dry-run`, no QEMU, no writes); real change needs
   `--apply --confirm DELETE-WINSRV`; the real pre-apply gate
   (`r1_genuine_retire_test.py --only ewsrv`, non-dry-run) must be PASS, plus
   the P1–P6 precondition checks from the gate doc.

---

## 2. Preconditions (gate doc §1 P1–P6) — the executor checks these before apply

- P1 grep: `QE_EXEC_GUI_|QE_EXEC_WS_|guishost\.h|wsclient\.h|qemu_windowsrv\.h`
  in `user/` + `ports/` may match **only inside the deletion set** (see §3).
  Known extra sites the executor must verify are re-pointed by chunks A–D first
  (found by read-only grep, not in gate doc §2): `user/ws_e32_client.c:40` and
  `user/ws_e32_draw_test.c:21` still `#include "wsclient.h"`, and
  `user/user.mk:1227,1382,1384,1406,1408,1428` dep lists still name
  `user/wsclient.h` / `variant/qemu_windowsrv.h`. `user/shot.c` (guishost.h +
  QE_EXEC_GUI_*) is outside the set — must be migrated or its row dropped first.
- P2 `tools/r1_genuine_input_test.py` PASS both boots, 0 faults (EWSRV keymap fix).
- P3 `tools/r1_genuine_ewsrv_test.py` PASS incl. EWCLIENT markers.
- P4 DOOM fallback removed (see edit E7 below).
- P5 chunks A–D landed. P6 standing invariants (EFILE guard-only, no
  `SessionPath r=-15` as success, `ECapabilityProtServ` for `!E32WindowServer`).

---

## 3. Exact edit/delete list (current line refs, verified 2026-11-24)

### Phase P1 — kernel hooks + exec-table (edit; file deletion comes in P3)

| # | File:line | Op | Content |
|---|-----------|----|---------|
| E1 | `variant/qemu_variant.h:103-111` | edit (delete block) | comment + `TInt gui_init();` (:106), `gui_publish_buttons` (:110), `gui_button_state` (:111) decls — all defined in deleted `qemu_gui_exec.cpp` |
| E1b | `variant/qemu_variant.h:100` | edit | `void GuiGeometryReset();` decl — defined in `qemu_gui_exec.cpp:339` (NEW vs gate doc; required by E4) |
| E2 | `variant/qemu_variant.h:243-251` | edit (delete block) | comment (:243-245) + `windowsrv_init` (:246), `windowsrv_base/size` (:247-248), `wsrv_publish_mouse` (:250), `wsrv_publish_key` (:251). **KEEP :253-280** (KBINJ genuine-EWSRV bridge) |
| E3 | `variant/qemu_mouse.cpp:173` | edit (delete line) | `wsrv_publish_mouse(gX, gY, gButtons, gButtonSeq);` |
| E3b | `variant/qemu_mouse.cpp:44,163,172,302,346,420` | edit (delete lines) | local decl `gui_publish_position` (:44); ISR calls `gui_publish_buttons` (:163), `gui_publish_position` (:172), `gui_publish_buttons` (:302); diagnostic `gui_button_state` (:346, :420) — NEW vs gate doc, required because `qemu_gui_exec.cpp` is deleted. **KEEP :174-189** (QemuEwsrvPush* KBINJ bridge) |
| E4 | `variant/qemu_display.cpp:400` | edit (delete line) | `GuiGeometryReset();` call — NEW vs gate doc |
| E5 | `variant/kern_shell.cpp:1751` | edit (delete line) | `gui_button_state(b, s);` diagnostic — NEW vs gate doc |
| E6 | `variant/qemu_variant_int.cpp:649` | edit (delete line) | `wsrv_publish_key((unsigned int)c);` — KEEP :648 `QemuKbdPush(c)` and the KBINJ path :232-530,579-586 |
| E6b | exec-table count | automatic | only writer of `tab[0]=274u` is `variant/qemu_windowsrv.cpp:257` (verified: sole `274u` in variant/ besides the unrelated logical `QE_EXEC_FB_BLIT 274u` in `qemu_fbblit.h:29`); deleting that file returns count 274→271 and slots 271-273 to zero pad. No separate edit needed |

### Phase P2 — build wiring (edit)

| # | File:line | Op | Content |
|---|-----------|----|---------|
| E7 | `Makefile:258-259` | edit (delete 2 lines) | `build/qemu_gui_exec.o` rule |
| E8 | `Makefile:261-262` | edit (delete 2 lines) | `build/qemu_windowsrv.o` rule |
| E9 | `Makefile:318` | edit (one-line exact replace) | remove `build/qemu_gui_exec.o build/qemu_windowsrv.o ` from `KERNFULL_OBJS`; **keep `build/qemu_fbblit.o`** |
| E10 | `user/user.mk:1367-1372` | edit (delete block) | `userland: build/user/WINSRV.E32` (:1367) + winsrv.o/.elf/.E32 rules (:1368-1372) |
| E11 | `user/user.mk:1377-1388` | edit (delete block) | old exec-based backend: `userland: WSDL.E32` (:1381), `wsclient.o` (:1382), `wsdl.o` (:1384), `wsdl.elf` (:1386), `WSDL.E32` (:1388) — gate doc's ":1364-1374" refs are stale; current block is 1377-1388. (WSDL the *migrated* target keeps a row re-pointed at the ews client lib if chunks A–D did that; WSE32TEST :1405/:1408 stays) |
| E12 | `user/user.mk` dep tokens | edit (exact-text per line) | remove ` user/guishost.h` from dep lists of migrated targets: guishell.o :100, saver.o :561, breakout.o :1260, guimenu.o :1301, desktop.o :1316, gcblittest.o :1536; remove `user/wsclient.h variant/qemu_windowsrv.h` tokens from guilib.o :1227, ws_e32_client.o :1406, ws_e32_test.o :1408, ws_e32_draw_test.o :1428. Migrated-target rules (GUIHELLO :1226-1233, BREAKOUT :1259-1264, GUIMENU :1300-1305, DESKTOP :1315-1320, DTEDIT :1334-1339, DTCALC :1351-1356, GCBLITTEST :1535-1540, SHOT :670-675) otherwise stay. Doc refs :99/:559/:667/:1215-1222/:1249/:1289/:1305/:1323/:1339/:1355/:1513 are comment-block starts; current rule lines listed here |

### Phase P3 — source deletion (delete; LAST, after P1+P2 gates pass)

| # | Path | Notes |
|---|------|-------|
| D1 | `user/winsrv.c` | includes guishost.h (:32) |
| D2 | `user/guishost.h` | also referenced by `user/shot.c` — P1 must have cleared that first |
| D3 | `user/wsclient.c` | old exec backend (:11 includes wsclient.h) |
| D4 | `user/wsclient.h` | ws_e32_client.c:40 / ws_e32_draw_test.c:21 includes must be re-pointed first |
| D5 | `user/wsdl.c` | old backend (:21) |
| D6 | `user/guilib.c` | **only the residual old display-list backend** — if chunk B left winsrv-only code, delete the file only if fully migrated, else exact-text edit the residual block; P1 grep decides |
| D7 | `variant/qemu_windowsrv.cpp` | `WsExecInstall` :215-258, `windowsrv_init` :261-279, boot ctor :324-326, `wsrv_publish_*` :292/:314 all go with it |
| D8 | `variant/qemu_windowsrv.h` | included by qemu_windowsrv.cpp:14 and the user.mk dep tokens removed in E12 |
| D9 | `variant/qemu_gui_exec.cpp` | execs 249/250/251 install (:298 `gui_init`, ctor :354-355), `gui_publish_*` :275/:285, `GuiGeometryReset` :339 |
| D10 | `ports/doom/e32_platform.c:31,181-211,246-250` | P4 edit (not whole-file): delete `#include "guishost.h"` (:31), comment+`:181-211` (`blit_via_gcrect`), and the fallback branch `:246-250` in `DG_DrawFrame`; **keep** :245 `exec2(QE_EXEC_FBCTL, FBO_FBBLIT, …)` primary — frame byte-for-byte unchanged |

**Explicitly KEPT (never in any op):** `variant/qemu_fbblit.*` + exec 270 ops 2/3
(DOOM keeps `qemu_fbblit.h`/`QE_EXEC_FBCTL`), all `r1_*` harnesses, netstack,
PS/2/AC'97/storage/display bridges, the KBINJ bridge (`qemu_variant.h:253-280`,
`qemu_variant_int.cpp:232-530,579-586`, `qemu_mouse.cpp:157-190` pointer side),
`user/ws_e32_client.c` (WSE32TEST), `user/shot_e32.c` orphan (outside set).

---

## 4. Pre/post gate commands (executor runs these; dry-run preview never boots QEMU)

Pre-apply (all must PASS; run on the pre-deletion tree, gate doc §3 order):

```
cd qemu-port
python3 tools/r1_os_selfcheck.py
python3 tools/r1_qemu_smoke.py
python3 tools/r1_genuine_ewsrv_test.py          # incl. EWCLIENT markers (P3)
python3 tools/r1_genuine_input_test.py          # both boots, 0 faults (P2)
python3 tools/r1_doom_test.py && python3 tools/r1_doom_audio_test.py \
  && python3 tools/r1_doom_playtest.py && python3 tools/r1_doom_multikey_test.py
python3 tools/r1_genuine_fullchain_test.py      # 14/14
python3 tools/r1_genuine_retire_test.py         # 4/4, and --only ewsrv PASS (tool's pre-gate)
python3 tools/r1_final_verify.py --strict-skip  # 11/11 authoritative
```

Phase gates: after P1 edits → incremental `make` (no `make clean`) + selfcheck +
smoke + retire `--only ewsrv`; after P2 wiring → `make isogrub` + selfcheck +
smoke; after P3 deletions → full §3 set again incl. `r1_final_verify.py
--strict-skip` 11/11 and the GUI-app tests re-pointed/retired in the same step.
Post-apply: the SAME gate re-run after every write phase; anything ≠ PASS →
auto-rollback (§5). Executor's own pre-apply gate mirrors `apply_shim`: real
(non-dry-run) `r1_genuine_retire_test.py --only ewsrv` must be PASS before any
write, else exit 3, no file touched.

---

## 5. Snapshot + rollback recipe

- Before the first write: snapshot every op target (edited AND delete-listed
  files) to `/tmp/opencode/winsrv_delete_backup_<ts>/` preserving relative
  paths, `manifest.json` with schema `r1-winsrv-delete-backup/v1`, per-file
  sha256, and rollback token `ROLLBACK-WINSRV-<ts>` (same pattern as
  `shim_retire_backup_*` used for `d26fade`); verify each copy's hash at
  backup time.
- Auto-rollback on ANY failure after a write (edit ambiguity, write error,
  phase-gate fail): restore every manifest file via staged temp + verified
  `os.replace` (recreates deleted files), check post-restore sha256, then
  re-run the phase gate. Manual: `--rollback DIR --confirm ROLLBACK-WINSRV-<ts>`.
- Git fallback (tool never commits/pushes):
  `git -C <repo> show <pre-gate-sha>:qemu-port/<path> > qemu-port/<path>`
  or `git checkout <pre-gate-sha> -- qemu-port/`, then rebuild + re-run §4.
- Exec table self-heals: with `qemu_windowsrv.cpp` restored, count 271→274;
  deleted, 274→271, pad zero again.

---

## 6. Safety rules (hard contract of `tools/r1_winsrv_delete.py`)

1. Never touch `kernelhwsrv/`: refuse if `git status --porcelain -- kernelhwsrv/`
   is non-empty; every op path must pass `safe_target()` (inside `qemu-port/`,
   resolved, no `..`, not absolute).
2. Token-gated: change requires `--apply --confirm DELETE-WINSRV`; default and
   `--dry-run` are pure preview (gate `--dry-run`, no QEMU, no writes, exit 0).
3. Refuse if any op target is already modified (`git status --porcelain` on the
   target set) — no stacking on unreviewed changes.
4. Every `edit` op asserts its `old` string occurs **exactly once**; 0 or many →
   abort before writing. `delete` ops only for paths in the static DELETE_SET.
5. Real pre-apply gate PASS required (exit 3 otherwise); post-phase gate PASS
   required; ANY failure after a write → auto-rollback, rollback rc checked,
   gate re-run after rollback (exit 4 write / 5 post / 6 rollback-failed).
6. Phase order fixed: P1 hooks → P2 wiring → P3 deletions; stop at first failure,
   never patch forward in the same step.
7. Never `make clean` (incremental `make` / `make isogrub` only), never commit
   or push, never boot QEMU in preview mode, never accept `SessionPath r=-15`
   as IPC success, EFILE stays guard-only (no trampoline).
8. After success: record the retirement in `docs/GENUINE_PROGRESS.md` and
   `docs/REMAINING_SHIMS_AUDIT.md` (winsrv row → retired), per the `d26fade`
   precedent — in a human-reviewed commit, not by the tool.
