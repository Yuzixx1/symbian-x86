# Migration Commit Inventory — HEAD 0f9ec6a

Prepared 2026-11-24 for a future **authorized** commit. No commit/push performed.
HEAD: `0f9ec6ab4ae63a8aae1bae11640924cdabb2a142` (2026-11-24 19:31:05 +0200).

## Scope rules

- INCLUDED: uncommitted migration work under `qemu-port/` (variant, genuine/ewsrv, user/, tools/, docs/).
- EXCLUDED: `AGENTS.md` (root), `qemu-port/.sybian/coordinator/state.json`, `qemu-port/.sybian/qemu-slot.lock`, `symbian-x86-archive-56fbaff.tar.gz`, `qemu-port/build/`, junk empty files `qemu-port/200`, `qemu-port/9`.
- IN FLIGHT (mtime < 3 min at scan time — exclude from any commit until quiescent):
  `qemu-port/docs/DOCS_INDEX.md`, `qemu-port/docs/CLEANUP_LOG.md`,
  `qemu-port/docs/FRESH_STRICT_VERIFY.md`, `qemu-port/docs/EWSRV_UTF16_RUNTIME_VERIFY.md`,
  `qemu-port/docs/WINSRV_DELETE_TOOL_STATUS.md`, `qemu-port/docs/WINSRV_DELETE_TOOL_STATUS2.md`,
  `qemu-port/user/desktop.c`, `qemu-port/200`, `qemu-port/9`.
- `tools/r1_winsrv_delete.py` does **not** exist on disk (only `r1_winsrv_test.py`) — not in inventory.

## Groups and counts

| Group | Modified | New | Total |
|---|---|---|---|
| variant/ | 4 | 0 | 4 |
| genuine/ewsrv/ (incl. patched/) | 6 | 3 | 9 |
| user/ (incl. user.mk) | 16 | 3 | 19 |
| tools/ | 1 | 2 | 3 |
| docs/ | 3 | 26 | 29 |
| **Total** | **30** | **34** | **64** |

(6 of the 29 docs + `user/desktop.c` are IN FLIGHT per above; committable subset ≈ 57.)

## Exact `git add` paths (never `git add -A`)

### Code group
```
git add qemu-port/variant/e32load.cpp qemu-port/variant/qemu_mouse.cpp \
  qemu-port/variant/qemu_variant.h qemu-port/variant/qemu_variant_int.cpp
git add qemu-port/genuine/ewsrv/build_client.py qemu-port/genuine/ewsrv/build_ewsrv.py \
  qemu-port/genuine/ewsrv/ewsrv_shims.cpp qemu-port/genuine/ewsrv/patched/co_cli.cpp \
  qemu-port/genuine/ewsrv/patched/ky_tran.cpp qemu-port/genuine/ewsrv/patched/ws_main.cpp
git add qemu-port/genuine/ewsrv/build_edisp.py qemu-port/genuine/ewsrv/edisp_glue.cpp \
  qemu-port/genuine/ewsrv/link_edisp.ld
git add qemu-port/user/breakout.c qemu-port/user/dtcalc.c qemu-port/user/dtedit.c \
  qemu-port/user/gcblittest.c qemu-port/user/guihello.c qemu-port/user/guilib.c \
  qemu-port/user/guilib.h qemu-port/user/guimenu.c qemu-port/user/guishell.c \
  qemu-port/user/saver.c qemu-port/user/shot.c qemu-port/user/user.mk \
  qemu-port/user/wsclient.c qemu-port/user/wsclient.h qemu-port/user/wsdl.c
git add qemu-port/user/shot_e32.c qemu-port/user/ws_e32_client.c qemu-port/user/ws_e32_draw_test.c
git add qemu-port/tools/r1_cp4_ca_stress_test.py qemu-port/tools/build_ews_app.py \
  qemu-port/tools/r1_genuine_input_test.py
```
(If `qemu-port/user/desktop.c` is still being edited, omit it and commit it separately once quiescent.)

### Docs group
```
git add qemu-port/docs/POST_SVCMGR_VERIFY.md
git add qemu-port/docs/CONNECT_DEBUG_STATUS.md qemu-port/docs/CONNECT_NEG28_STATUS.md \
  qemu-port/docs/CP4_STRESS_POSTFIX_VERIFY.md qemu-port/docs/CP4_STRESS_RECURRENCE.md \
  qemu-port/docs/CRITICAL_PATH_STATUS.md qemu-port/docs/DOOM_WINSRV_AUDIT.md \
  qemu-port/docs/EWSRV_INPUT_INJECTION.md qemu-port/docs/EWSRV_UTF16_VERIFY_STATUS.md \
  qemu-port/docs/EXEC269_FIX_STATUS.md qemu-port/docs/FSHORTWCHAR_EWSRV_STATUS.md \
  qemu-port/docs/GUISHELL_MIGRATION_MAP.md qemu-port/docs/HANDLE_FIX_STATUS.md \
  qemu-port/docs/HANDLE_RETURN_DIAGNOSIS.md qemu-port/docs/HANDLE_RETURN_STATUS.md \
  qemu-port/docs/KEYMAP_FIX_STATUS.md qemu-port/docs/USERLAND_BUILD_STATUS.md \
  qemu-port/docs/USERMK_CONSOLIDATION.md qemu-port/docs/WINSRV_DELETION_EXECUTOR.md \
  qemu-port/docs/WINSRV_DELETION_GATE.md qemu-port/docs/WINSRV_ENDGAME_STATUS.md \
  qemu-port/docs/WINSRV_EXEC_USERS_AUDIT.md qemu-port/docs/WORKSTREAM_STATUS.md
```
(Exclude the 6 IN FLIGHT docs listed above until their writers finish.)

## Suggested split

1. **Code commit** — variant + genuine/ewsrv + user + tools (47 paths above).
2. **Docs commit** — docs/ paths above (separate, can land independently).

## Suggested commit messages

- Code: `qemu-port: EWSRV migration — edisp glue, E32 userland clients, variant input/loader updates`
- Docs: `docs: status reports for EWSRV migration, winsrv deletion plan, CP4 stress, verify gates`

## Preconditions (before authorizing)

- Fresh strict gate from `qemu-port/`: `python3 tools/r1_final_verify.py --strict-skip` must be **15/15 PASS** (authoritative roster; `FINAL27_RC=0`).
- `python3 tools/r1_os_selfcheck.py` clean.
- Re-run `git status --short` at commit time; drop any path still IN FLIGHT.
- Confirm `kernelhwsrv/` shows no entries in `git status` (verified clean at scan time).

## Rollback

Per file: `git show 0f9ec6a:<path> > <path>` (or `git checkout 0f9ec6a -- <path>` for tracked files; delete untracked new files manually).
Example: `git show 0f9ec6a:qemu-port/variant/e32load.cpp > qemu-port/variant/e32load.cpp`.

## Verification notes (scan time)

- `kernelhwsrv/`: clean (no modified/untracked entries).
- Secrets scan over all new files: no keys/tokens/passwords found (only the word "token" in docs describing the rollback-token mechanism). Nothing to redact.
