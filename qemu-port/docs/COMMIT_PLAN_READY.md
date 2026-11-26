# Commit plan (prepared, NOT executed)

Status: `HEAD = origin/main = ee8490e`; nothing staged; no commit or push has been made.
This file is a staging plan only. Commit/push requires explicit user authorization.
Never run `git add -A`.

## Working-tree inventory (read from `git status --porcelain` at plan time)

| Class | Count | Examples |
|---|---:|---|
| `M` tracked | 179 | mostly `qemu-port/docs/*.md`; source/tool list below |
| ` D` tracked deletions | 16 | `qemu-port/fs/r1_fs_channel*`, `qemu-port/services/r1_service_host*`, `qemu-port/tests/r1_fs_channel/*`, `qemu-port/variant/r1_fs_channel*` |
| `??` untracked | 216 | 205 under `qemu-port/docs/`, 11 under `qemu-port/tools/` |

## Non-doc modified tracked files (review before staging)

- `.gitignore`, `AGENTS.md`
- `qemu-port/.sybian/coordinator/state.json` (runtime state — exclude from commit)
- `qemu-port/Makefile`
- `qemu-port/genuine/`: `dm/build_dm.sh`, `efsrv_min/build_server.sh`, `efsrv_min/efsrv_min.cpp`, `f32_probe/build_probe.sh`, `hal/build_hal.sh`, `hal/hal_glue.cpp`
- `qemu-port/tools/`: `mkgrubiso.py`, `r1_cp4_ca_stress_test.py`, `r1_cp4_ca_test.py`, `r1_final_verify.py`, `r1_genuine_efsrv_test.py`, `r1_genuine_eshell_test.py`, `r1_genuine_ewsrv_test.py`, `r1_genuine_fat_io_test.py`, `r1_genuine_fullchain_test.py`, `r1_genuine_retire_test.py`, `r1_qemu_smoke.py`, `r1_selftest.py`, `r1_shim_retire_plan.py`, `r1_window_system_test.py`, `r1_winsrv_test.py`
- `qemu-port/user/`: `fcopy.c`, `filemgr.c`, `user.mk`
- `qemu-port/variant/`: `kern_shell.cpp`, `qemu_pci.cpp`, `qemu_usb.cpp`

Note: `tools/r1_final_verify.py` carries the uncommitted SKIP-fatal change; HEAD alone
still has advisory SKIP semantics. Decide explicitly whether that belongs in the commit.

## Recommended path-scoped groups

1. Documentation: `qemu-port/docs/` (tracked `M` + 205 untracked), plus root `AGENTS.md`, `.gitignore` if the doc-supersession edits are intended.
2. Harness/scripts: `qemu-port/tools/` (tracked `M` + 11 untracked), reviewed individually.
3. Source/build: `qemu-port/genuine/`, `qemu-port/user/`, `qemu-port/variant/`, `qemu-port/Makefile`.
4. Shim removal (deletions): the 16 `r1_fs_channel*` / `r1_service_host*` paths — stage with `git add -u <paths>`, only if the retirement is meant to ship.

Exclude: `qemu-port/.sybian/coordinator/state.json` (runtime state), `__pycache__`, lock files, and any generated ISO/build output.

## Gate relationship

Authoritative gate: `FINAL27_RC=0`, 15 pass / 0 skip / 0 fail in `/tmp/opencode/final27_gate.log`
(run 2026-11-26 03:26). Repeated checks confirmed no source/tool file postdates the log;
post-gate edits are docs and runtime state only. `kernelhwsrv/` remains clean at `0c32086`.

## Execution (only after user says go)

Stage by explicit path, e.g.:
`git add qemu-port/docs AGENTS.md .gitignore qemu-port/tools ...` then `git diff --cached --stat`
and review before `git commit`. Do not use `git add -A`. Do not push unless asked.
