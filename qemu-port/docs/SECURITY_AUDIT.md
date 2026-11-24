# Static Security Audit — pre-commit (pivot files)

Scope: `qemu-port/tools/*.py` (new `r1_*.py`, `genuine_build.py`,
`r1_final_verify.py`, `r1_shim_retire_apply.py`, `r1_efile_acceptance.py`),
`qemu-port/genuine/**/*.py|cpp|h`, and the changed `variant/` + `fs/` files.
Out of scope: `kernelhwsrv/` (read-only, untouched), unchanged pre-existing
files. Static only — no QEMU, no edits other than this report.

## Verdict: SAFE

No real security findings. Zero hardcoded credentials (no `sk-`, `mtk_`,
`Bearer`, `api_key`, private-key blocks, or password literals in scope; the
only `secret=` literal is a pre-existing game-test fixture, not a credential).
No `shell=True`, `eval`/`exec` of untrusted input, or command injection in any
in-scope file. All subprocess calls use list form.

## Findings table

| # | Severity | Location | Issue | Real / False positive |
|---|----------|----------|-------|-----------------------|
| 1 | Info | `genuine/ewsrv/patched/ws_main.cpp:30,741,771,827,892` | "password notifier" UI strings | **False positive** — genuine Symbian EPL-sourced console password-dialog code, not a credential |
| 2 | Info | `genuine/**/patched/*.cpp`, `genuine/efile/patched/sf_main.cpp` | `http://www.eclipse.org/legal/epl-v10.html` | **False positive** — EPL license header URLs |
| 3 | Info | `tools/r1_shim_retire_apply.py:15,134-139,299,347-351,564-571` | "token"/`RETIRE-*`/`ROLLBACK-*` strings | **False positive** — local confirmation tokens, no secret value; values derived from shim names + timestamp |
| 4 | Info | `tools/r1_codebreak_test.py:44,179-183` (pre-existing) | `secret=RGBY` | **False positive / out of scope** — game-test fixture |
| 5 | Info | `tools/r1_*.py` (many) | `socket.AF_UNIX` connections | **False positive** — local QEMU monitor sockets only, no network egress |
| 6 | Info | `genuine/*.py`, `tools/r1_*.py` | hardcoded `/tmp/opencode`, `/home/yuzi/.cache/...`, kernelhwsrv paths | **False positive** — approved scratch dir / read-only build inputs; no writes to `/etc`, no `~` expansion, no `sudo`, no `chmod/chown` |
| 7 | Info | `genuine/prove_efile_genuine.py:19-21` | `EFILE_LOG`/`EFILE_E32`/`EFILE_CLIENT` env overrides | **False positive** — debug/prove script, intentionally user-configurable local paths |
| 8 | Info | `genuine/efsrv_min/add_capability.py:20-33` | patches arbitrary positional path | **False positive** — by-design dev build helper; callers pass fixed in-tree paths, no untrusted input |
| 9 | Info | `tools/test_e32.py:42`, `test_e32_simple.py:44`, `r1_e32_validate_test.py:116` | `subprocess.Popen(..., shell=True)` | **Out of scope** — pre-existing unchanged files (per `git diff HEAD`); not part of this pivot |

Secrets: none found, so none to redact.

## Re-verification: `tools/r1_shim_retire_apply.py`

- Cannot write outside `qemu-port/`: **confirmed**. `safe_target()` (L173-176)
  resolves each path and requires `PORT` in parents and `KHWSRV` not in parents;
  `build_edits()` only uses fixed inventory from `r1_shim_retire_plan.py`;
  every write is `PORT / e["path"]` (L479-484). Rollback re-validates each
  manifest path against the planned-path allowlist, `safe_target()`, abs-path
  and `..` checks (L371-375) before any write. Snapshots/backups confined to
  `/tmp/opencode/` (L67, L293-298).
- Cannot delete/commit/push: **confirmed**. `FORBIDDEN` guard on argv (L78-79,
  L539-542), `print_git_rollback()` only prints suggested commands (L411-419),
  no `git commit/push` or `rm` invocations anywhere.
- Extra guards present: kernelhwsrv-clean and dirty-target refusals
  (L234-247, L435-447), exact-one-occurrence edit assertions (L179-195),
  token-gated apply/rollback (L564-572, L347-351), atomic verified rollback
  with hash checks (L333-408). `--apply` also runs a real gate (QEMU) only in
  non-preview mode; preview is NO-QEMU as documented.

## Re-verification: `tools/r1_final_verify.py`

- List-form subprocess: **confirmed** — `Popen(["make","isogrub"], ...)` L168-170
  and `Popen(cmd, ...)` L215-217, where `cmd` is built by `command_for()` as
  `[sys.executable, <fixed script path>] + fixed argv` (L125-129). No
  `shell=True`, no string command execution.
- Logs confined to log-dir: **confirmed** — all logs via
  `os.path.join(logdir, ...)` (L163, L205) with `--log-dir` (default
  `/tmp/opencode`), created with `os.makedirs(logdir, exist_ok=True)`.
- No destructive commands: **confirmed** — only `os.remove(DOOM_ISO)` on the
  fixed `/tmp/opencode/r1_doom.iso` (L132-137, L192) and a copy into it
  (L194); `make isogrub` is the only build command. Timeout kills use
  `os.killpg` on its own process group (L180, L226). No `rm -rf`, no
  `git commit/push`, no `make clean`.

## Required fixes

None. No blockers to commit from a security standpoint.

## Notes

`kernelhwsrv/` untouched; no files edited except this report. No QEMU run.
