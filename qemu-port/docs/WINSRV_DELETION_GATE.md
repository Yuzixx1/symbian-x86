# WINSRV_DELETION_GATE — terminal checklist for migration-plan chunk E (winsrv deletion)

**Status:** READ-ONLY planning doc, 2026-11-24. This file is the only edit. No QEMU
runs, no commit/push, no `make clean`, `kernelhwsrv/` untouched. This gate executes
**only after chunks A–D land** (`docs/WINSRV_EXEC_USERS_AUDIT.md` §3) and is one
reviewed, human-gated step (no apply path — `tools/r1_shim_retire_plan.py` winsrv
entry, gate=`ewsrv`).

Sources: `docs/WINSRV_MIGRATION_PLAN.md` (§4 deletion gate), `docs/WINSRV_EXEC_USERS_AUDIT.md`,
`docs/DOOM_WINSRV_AUDIT.md`, `docs/WINSRV_EDISP_SCOPE.md` (§6), `docs/EWSRV_INPUT_INJECTION.md`,
`docs/CRITICAL_PATH_STATUS.md`, `Makefile:258-262,318`, `variant/qemu_variant.h:106,246-251`,
`variant/qemu_mouse.cpp:173`, `variant/qemu_variant_int.cpp:649`, `variant/qemu_windowsrv.cpp:215-239`,
`user/user.mk`, `ports/doom/e32_platform.c`.

---

## 1. Preconditions — ALL must be green before any file is touched

- [ ] **P1 — Consumers grep-clean.** All 14 audited binaries are off execs
  249/250/251 and 271/272/273. Grep `user/` + `ports/` for
  `QE_EXEC_GUI_|QE_EXEC_WS_|guishost\.h|wsclient\.h|qemu_windowsrv\.h` — matches may
  exist **only inside the deletion set** (§2). The 14: GUISHELL, SAVER, SHOT,
  GUIHELLO(+guilib), BREAKOUT, GUIMENU, DESKTOP, DTEDIT(+guilib), DTCALC(+guilib),
  GCBLITTEST, WINSRV.E32 (delete-not-migrate), WSDL(+wsclient), DOOM.E32.
  FBBLITTEST (exec 270 only) and the orphan `user/shot_e32.c` are outside the set.
  WSE32TEST.E32 (`user/ws_e32_client.c`) is the already-migrated model — stays.
- [ ] **P2 — Input injection verified incl. keymap fix.** `tools/r1_genuine_input_test.py`
  PASS on **both** boots: pointer (`\KBINJ.TXT`=`ptr`, `EPointerMove … consumed=1`,
  0 faults) **and** keyboard (`key`, `EKeyDown … consumed=1`, **0 faults** — requires
  the EWSRV keymap fix landed: `CKeyTranslatorX` conv/func tables zero-initialized /
  staged `EKDATA.DLL`, closing the KERN-8 #PF at `cr2=0xf0000010` recorded in
  `docs/EWSRV_INPUT_INJECTION.md` §"Known blocker" and `docs/CRITICAL_PATH_STATUS.md`).
  Kernel→genuine-EWSRV delivery via `Kern::AddEvent` (DFC path) proven; client-side
  `RWsSession::GetEvent`/`TWsEvent` loop closed by an event-reading client.
- [ ] **P3 — edisp rendering via genuine EWSRV.** `tools/r1_genuine_ewsrv_test.py`
  PASS **including the EWCLIENT client block** (CLIENT_MARKERS all `r=0`, 0 faults,
  `EWCLIENT: window drawn`): genuine EWSRV renders through the landed edisp
  (`genuine/ewsrv/edisp_glue.cpp`, `build_edisp.py`; single ordinal-1
  `CScreenDriver::New` DLL — no LDD/PSL split). If `patched/co_cli.cpp` was touched,
  re-run `build_client.py` + `build_ewsrv.py` and restage both E32s (stale-image trap).
- [ ] **P4 — DOOM 251 fallback removed** (`docs/DOOM_WINSRV_AUDIT.md` §4): `guishost.h`
  include, `blit_via_gcrect()`, and the `DG_DrawFrame` fallback branch deleted from
  `ports/doom/e32_platform.c`; fbblit (exec 270 op 3) primary untouched; frame
  byte-for-byte unchanged. Done **last** among consumers, playtest-gated.
- [ ] **P5 — Chunks A–D landed:** ews client lib (WSDL/wsclient port), guilib backend
  + GUIHELLO/DTEDIT/DTCALC, SAVER/SHOT/GCBLITTEST, GUISHELL/GUIMENU/DESKTOP/BREAKOUT
  on `TWsEvent` input.
- [ ] **P6 — Standing invariants hold:** EFILE guard-only model intact (no trampoline;
  port-shim `0`, `#PF=0`; never accept `SessionPath r=-15` as IPC success);
  `!E32WindowServer` runs with `ECapabilityProtServ` (granted by E32Run).

## 2. Exact deletion set

**User / variant sources (delete last):**
- [ ] `user/winsrv.c`, `user/guishost.h`, `user/wsclient.c`, `user/wsclient.h`
      (old exec-based backend only — the RWsSession-ported lib that replaces it stays),
      `user/wsdl.c` (old backend), old guishost/guilib display-list backends in
      `user/guilib.c` (whatever remains after chunk B).
- [ ] `variant/qemu_windowsrv.cpp`, `variant/qemu_windowsrv.h`, `variant/qemu_gui_exec.cpp`.
- [ ] `ports/doom/e32_platform.c:31,183-211,246-250` (P4 — may land just before the gate).

**Kernel hooks / exec table:**
- [ ] `variant/qemu_variant.h:106` (`gui_init` decl), `:246-251` (`windowsrv_init`,
      `wsrv_publish_mouse`, `wsrv_publish_key` decls).
- [ ] Call sites: `variant/qemu_mouse.cpp:173` (`wsrv_publish_mouse`),
      `variant/qemu_variant_int.cpp:649` (`wsrv_publish_key`).
      **KEEP** the KBINJ genuine-EWSRV bridge (`QemuEwsrvPushKey/PointerMove/Button`,
      `qemu_variant_int.cpp:232-530,579-586`, `qemu_mouse.cpp:157-190` pointer side,
      `qemu_variant.h:253-280`) — that is the live input path to genuine EWSRV, not winsrv.
- [ ] Exec table: remove `WsExecInstall()` count bump in `variant/qemu_windowsrv.cpp:215-239`
      → count back **274→271**; slots 249/250/251 and 271/272/273 return to the verified
      zero pad (pad returns to zero). `gui_init()`'s exec installs (249/250/251) go with
      `qemu_gui_exec.cpp`.

**Build wiring:**
- [ ] `Makefile:258-259` (`qemu_gui_exec.o` rule), `:261-262` (`qemu_windowsrv.o` rule),
      both object names out of `KERNFULL_OBJS` at `:318`.
- [ ] `user/user.mk` rows for the deleted/migrated targets — object, `.elf`, `.E32`
      rules and `userland:` deps: guishell `:99`, GUIHELLO/guilib `:1215-1222`,
      saver `:559`, shot `:667`, breakout `:1249`, guimenu `:1289`, desktop `:1305`,
      dtedit `:1323`, dtcalc `:1339`, winsrv `:1355`, wsclient/wsdl `:1364-1374`,
      gcblittest `:1513` (migrated targets keep their rows, re-pointed at the ews
      client lib; deleted targets' rows go).

**Explicitly KEPT (not in the set):** `variant/qemu_fbblit.*` + exec 270 ops 2/3
(svcmgr-owned, separate later retirement — DOOM keeps `qemu_fbblit.h`/`QE_EXEC_FBCTL`),
all `r1_*` harnesses/tests, netstack (never), PS/2/AC'97/storage/display bridges,
KBINJ input bridge, `user/ws_e32_client.c` (WSE32TEST).

## 3. Verification gates — run in this order, ALL must PASS

1. [ ] `python3 tools/r1_os_selfcheck.py` (pre-flight).
2. [ ] `tools/r1_qemu_smoke.py` PASS.
3. [ ] `tools/r1_genuine_ewsrv_test.py` PASS incl. EWCLIENT CLIENT_MARKERS (P3).
4. [ ] `tools/r1_genuine_input_test.py` PASS, both boots, 0 faults (P2).
5. [ ] DOOM four: `r1_doom_test.py`, `r1_doom_audio_test.py`, `r1_doom_playtest.py`,
       `r1_doom_multikey_test.py` — all PASS; QMP screendump PNGs match the
       fbblit-produced baseline (frame byte-for-byte unchanged).
6. [ ] `tools/r1_genuine_fullchain_test.py` 14/14 PASS.
7. [ ] `tools/r1_genuine_retire_test.py` 4/4 PASS.
8. [ ] `python3 tools/r1_final_verify.py --strict-skip` 11/11 PASS (authoritative gate).
9. [ ] GUI-app tests that launched migrated binaries re-pointed at EWSRV versions or
       retired in the same commit (`r1_guishell_test.py`, `r1_saver_test.py`,
       `r1_shot_test.py`, desktop/window tests).
10. [ ] Human review of the diff; no apply path.

## 4. Execution order (source deleted LAST, after gates)

1. [ ] Snapshot first (§5), then confirm P1–P6 and run gate set §3 on the pre-deletion tree.
2. [ ] Delete kernel-side hooks + exec-table bump (decls, ISR call sites, count 274→271),
       rebuild kernel objects (no `make clean` — incremental only).
3. [ ] Remove `Makefile` object rows / `KERNFULL_OBJS` and `user/user.mk` deleted-target rows; rebuild.
4. [ ] Run gates §3 again on the rebuilt image.
5. [ ] Only then delete the source files themselves (§2 "User / variant sources"), rebuild,
       and run the full gate set §3 one final time.
6. [ ] Record the retirement in `docs/GENUINE_PROGRESS.md` / `docs/REMAINING_SHIMS_AUDIT.md`
       (winsrv row → retired), per the `d26fade` portfilesrv precedent: bypass first,
       deletion in the same reviewed commit.

## 5. Rollback

- [ ] Before touching anything: `tar` snapshot of `qemu-port/` to
      `/tmp/opencode/winsrv_retire_backup_<ts>/` (same pattern as
      `/tmp/opencode/shim_retire_backup_*` used for `d26fade`).
- [ ] Per-file restore: `git show <pre-gate-sha>:qemu-port/<path> > <path>`
      (deletion commit's parent restores every file in one step:
      `git checkout <sha> -- qemu-port/`), then rebuild + re-run §3.
- [ ] Exec table self-heals: with `qemu_windowsrv.cpp` restored, count returns 271→274;
      with it deleted, 274→271 and the pad is zero again.

## 6. Standing caveats

- Widest blast radius of all shims — winsrv retires last; exec 270 is untouched by this step.
- Never `make clean`; never edit/commit in `kernelhwsrv/`; no commit/push without explicit request.
- Never accept `SessionPath r=-15` as IPC success; EFILE stays guard-only.
- If any gate regresses after deletion, roll back per §5 — do not patch forward in the
  same step.

## Supersession (2026-11-26) — checklist rows pinned to 11/11 and 14/14 are stale

Items 6 and 8 of the deletion checklist (`:100-:101`) pin
`r1_genuine_fullchain_test.py` at 14/14 and the authoritative
`r1_final_verify.py --strict-skip` at 11/11. Both row counts are historical. Current
authority: `DEFAULT_TESTS` in `tools/r1_final_verify.py:134-150` = **15** rows, last full
run `/tmp/opencode/final27_gate.log` = `OVERALL: PASS (15 pass, 0 skip, 0 fail/not-run)`,
`FINAL27_RC=0`. When this gate is next executed, a green run means 15/15, not 11/11.
The gate conditions themselves (no faults, PNG baseline match, rollback on regression)
are unchanged; nothing above is rewritten.
