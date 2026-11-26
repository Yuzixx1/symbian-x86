# WAVE2 shim-removal plan — r1_fs channel set + residue (plan only; nothing deleted)

Read-only inventory 2026-11-25 at HEAD `ee8490e`. Ordered steps, per-step gate, rollback token.
Gates: **G12** = new static `tools/r1_shim_residue_sweep.py` (<5 s, first in `DEFAULT_TESTS`);
**R** = `r1_genuine_retire_test.py` 4/4; **F** = `r1_genuine_fullchain_test.py` 14/14;
**S** = `r1_final_verify.py --strict-skip` 11/11. Any FAIL/SKIP/124 = NO-GO.

## Classification (grep of Makefile, user.mk, *.cpp/*.h/*.sh/*.py)

| Item | Verdict | Evidence |
|---|---|---|
| `fs/r1_fs_{core,server}.{cpp,h}`, `r1_fs_backend.h`, `fs/r1_fs_fat.{cpp,h}` (7) | **LIVE — keep** | `genuine/efsrv_min/efsrv_min.cpp:30,32` includes; `build_server.sh:16` links `build/user/r1_fs_{core,server}.o` |
| `fs/r1_fs_channel.{cpp,h}`, `r1_fs_channel_eka.{cpp,h}`, `r1_fs_channel_state.h`, `r1_fs_wire_logic.h` (6) | **ORPHAN** | no non-doc consumer; only self-includes + `Makefile:345-349` |
| `fs/r1_fs_core.mmp` | **ORPHAN** | "proposed MMP, not built" (`tests/r1_fs_core/README.md:50`) |
| `variant/r1_fs_channel.{cpp,h}` (2) | **ORPHAN** | sole compile is opt-in `R1_FS_CHANNEL=0` block, obj empty by default |
| `tests/r1_fs_channel/{adapter,state,wire}_test.cpp` | **DIES WITH THE SET** | includes the 6 headers above |
| `services/r1_service_host.{cpp,h}`, `r1_service_state.h` | **ALREADY GONE (uncommitted `D`)** | `git status` D; zero build refs; only a doc comment in `efsrv_min.cpp:10` |
| `PORTFILES.RV.E32`, `PORTFILESRV.E32`, `r1_fs_*.o`, `r1_service_*.o` | **ABSENT** | none on disk; only `docs/*` prose mentions |
| `build/user/{desktop_e32,guimenu_e32,guishelle32}.o`, `build/user/{GUIMENU,GUIMENU}E32/GUISHELLE32.E32` | **STALE ARTIFACTS** | basenames have no source anywhere; non-gating |
| `user/user.mk` | **CLEAN** | no `r1_fs`/`r1_service`/`portfilesrv` rules remain; only comments |

## Ordered steps

1. **S0 pre-gate** — capture R + F + S green and `git -C kernelhwsrv status --porcelain` empty. NO-GO otherwise.
2. **S1** — delete `fs/r1_fs_core.mmp`. Gate: **G12** + S.
3. **S2** — remove `Makefile:342-351` (opt-in comment, `R1_FS_CHANNEL`, `build/r1_fs_channel.o` rule, `KERNFULL_OBJS +=`). Gate: G12 + S (`make -n` must not name `r1_fs_channel`).
4. **S3** — delete `variant/r1_fs_channel.{cpp,h}`, the 6 `fs/r1_fs_channel*`/`r1_fs_wire_logic.h` files, and `tests/r1_fs_channel/` in one step. Gate: G12 + **R 4/4** + **F 14/14** + S.
5. **S4** — `services/` residue: the 3 deletions already sit in the tree; assert zero refs and leave them uncommitted/out of any commit. Gate: G12 whitelist.
6. **S5 (non-gating)** — delete stale artifacts by explicit path only; never `make clean`, never `rm -rf`.

**G12 assertions:** each orphan path absent; no `Makefile`/`user.mk`/`*.cpp`/`*.h` reference to them; whitelist for
`genuine/efsrv_min/*` + `tests/r1_fs_core/*` (live probe consumers).

## Snapshot / token scheme

`/tmp/opencode/wave2_shim_backup_<UTCstamp>/` mirroring each path, token `ROLLBACK-WAVE2-<UTCstamp>`;
fallback `git show ee8490e:<path>`. `r1_shim_retire_apply.py` / `r1_winsrv_delete.py` must keep refusing
commit/push/`make clean`/`rm -rf`. No commit, no push in this chunk.

## Frozen — untouched by every step

Exec **81** (`FsRegisterThread`), **269** (`PortKillExec`), **270** (`PortCtlExec` ops 2/3/4);
`GenuineBootLaunch()` order + `svcmgr`-fallback suppression (`docs/KERNEL_HOOK_FREEZE.md`); `kernelhwsrv/`
read-only; keep `wsdl.c`, `guilib.c`, KBINJ, netstack; `efsrv_min` probe host and `tests/r1_fs_core`.

---

**2026-11-26 footer — supersession of gate S.** The `--strict-skip` **11/11** gate definition in line 6 is
historical only. Current gate authority is **15 rows / FINAL27_RC=0**: 15 pass, 0 skip, 0 fail, logged in
`/tmp/opencode/final27_gate.log`. Where line 6 and this footer disagree, the 15-row result governs. Body above
is otherwise unchanged.
