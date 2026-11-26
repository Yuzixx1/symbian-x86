# Known gaps — deliberately NOT fixed (verified 2026-11-26)

Scope: gaps that remain after the winsrv/GUI retirement and the genuine
EWSRV migration, each re-verified read-only on 2026-11-26 against source,
build tree and serial logs. None of them is a regression; each was left in
place on purpose, and the reason is recorded with the evidence. This document
authorises nothing: no gate is re-wired, no shim is retired, `kernelhwsrv/`
is untouched.

## 1. (HISTORICAL — superseded by the 2026-11-26 correction at line 126) Two gate scripts still stage the retired `portfilesrv` from `~/.cache`

`qemu_smoke` and `selfcheck` in `tools/r1_final_verify.py:129-130` are in
`DEFAULT_TESTS`, and their `ProcessCreate` / `Loaded` / `portfilesrv started`
markers are produced by the shim binary, not by a genuine module.

Evidence:
- `tools/r1_qemu_smoke.py:36` — `PORTFILESrv =
  "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv"`;
  `:65` stages it onto the FAT image as `portfilesrv`.
- `tools/r1_qemu_smoke.py:49-50` — required markers `"ProcessCreate r=0"`
  and `"Loaded r=0"`; `:168` types `portfilesrv` at the serial console;
  `:230` only echoes those keys on failure.
- `/tmp/opencode/r1_qemu_smoke.log:278` (`e32run: ProcessCreate r=0
  proc=101ff58`), `:304` (`e32run: Loaded r=0`), `:316` (`portfilesrv
  started (user process, pid=9)`) — all three come from that staged shim.

Why not fixed: no genuine replacement emits this marker family. Deleting the
staging line or the markers would red `qemu_smoke` immediately, i.e. a purely
cosmetic retirement would trade a passing gate for a failing one. Retiring it
correctly means first giving a genuine process launcher those three markers,
which is a feature, not a cleanup.

## 2. `genuine_input` / `genuine_pointer` have no SKIP path, so they stay out of `DEFAULT_TESTS`

Both scripts boot QEMU themselves and exit `1` (not SKIP) when their
prerequisites are missing, so they cannot sit in the default gate list without
turning a green run red on an unbuilt tree.

Evidence:
- `tools/r1_genuine_input_test.py:102` (`sys.exit("genuine EWSRV/EWCLIENT
  images missing (build first)")`), `:117`, `:121`, `:137` — all
  `sys.exit` with a message, none prints a `RESULT: SKIP` verdict; `:230`
  builds and launches its own `qemu-system-i386` invocation.
- `tools/r1_genuine_pointer_test.py:93`, `:108`, `:112`, `:128`, and its
  own QEMU launch at `:202`.
- `tools/r1_final_verify.py:125-143` — neither script appears in
  `DEFAULT_TESTS`.

Why not fixed: converting `sys.exit(msg)` into a `RESULT: SKIP` path is a
behaviour change to two gates nobody re-ran after the EWSRV migration, and a
self-skip that never runs is not evidence. They stay runnable by hand
(`python3 tools/r1_genuine_input_test.py`) and out of the strict list.

## 3. `r1_winsrv_test.py` / `r1_window_system_test.py` still name the deleted winsrv binary

Both drive `build/user/WINSRV.E32` and type `winsrv` at the console. The
binary and its sources are gone (`ee8490e`, `94ce9c9`), so at best they
self-skip; neither can ever PASS.

Evidence:
- `tools/r1_winsrv_test.py` — 17 case-insensitive `winsrv` hits, e.g. `:29`
  (`E32 = REPO + "/build/user/WINSRV.E32"`), `:65-68` (ISO staging), `:166`
  (`vm.stdin.write(b"winsrv\n")`), `:66` `RESULT: SKIP`.
- `tools/r1_window_system_test.py` — 19 hits, e.g. `:43-46` (the `WINSRV`
  spec, `"cmd": b"winsrv\n"`, markers `WSV:`/`WINSRV:`/`WSRV:`/`WS:`),
  `:346`, `:87` `RESULT: SKIP`.
- `build/user/WINSRV.E32` does not exist; `user/winsrv.c` and
  `user/ws_main.cpp` do not exist. `build/user/WSDL.E32` survives as a stale
  artifact only.
- Neither name appears in `tools/r1_final_verify.py` — both are unwired.

Why not fixed: deleting the scripts would lose the QMP screendump / PIL
colour-diversity harness that the genuine-EWSRV GUI work will want back, and
rewriting them against `EWSRV.E32` is migration work, not gap-closing. They
are kept unwired with an explicit self-skip and a docstring stating the
retirement (`r1_window_system_test.py:19-23`, `r1_winsrv_test.py:10-14`).

## 4. FreeDM asset is user-supplied and still absent

`docs/ASSET_RECOVERY_NOTE.md:10-11` — no `freedm*.wad` exists under `/tmp` or
`$HOME`; the Freedoom directory holds only `freedoom1.wad`, so the P3 FreeDM
gate stays SKIP pending an explicit user drop-off.
`docs/ASSET_STAGING_NOTE.md:10` records the same, with the no-download rule.
`tools/r1_freedm_test.py:24-25` states it is deliberately NOT in
`DEFAULT_TESTS` and that the strict run stays green without it.

Why not fixed: it is a licensing-bound third-party asset that must come from
the user. Nothing in the tree can close it, and downloading it is forbidden.

## 5. Written gate count drifted to `11/11` while the tool runs 15 (was 14; superseded — see corrections at the end)

The count quoted in prose predates the rows added since (three cheap static
pre-gates, then `hal_probe`).

Evidence:
- `tools/r1_final_verify.py:125-143` — `DEFAULT_TESTS` held 14 entries at the
  time of this reading (historical; the live roster is **15** at `:134-150`,
  `hal_probe` added as row 15):
  `g12_residue`, `g12_negative`, `missing_statics`, `selfcheck`,
  `qemu_smoke`, `genuine_retire`, `genuine_efsrv`, `genuine_ewsrv`,
  `genuine_eshell`, `genuine_fat_io`, `doom_main`, `doom_audio`,
  `doom_playtest`, `doom_multikey`.
- At this reading `AGENTS.md:88` and `AGENTS.md:104` said `11/11 PASS` (since
  corrected — see below); `docs/FINAL_GATE_STATUS.md:85` is a 2026-11-24
  snapshot that enumerates 11 names — the three static gates and `hal_probe`
  are missing from that list.

Why not fixed: the earlier reading recorded here — that `AGENTS.md` is a shared
operations file left open by other work, and that `AGENTS.md:88` / `:104` still
said `11/11 PASS` — no longer holds either: `AGENTS.md:88-94` now states the
15-row roster (`tools/r1_final_verify.py:134-150`) and cites `FINAL27_RC=0` (the
`/tmp/opencode/final27_gate.log` run ending `OVERALL: PASS (15 pass, 0 skip,
0 fail/not-run)`) as the authoritative gate, so the root `AGENTS.md` and the
authority recorded at the end of this file now agree; `FINAL20_RC` and any
`FINAL26_RC` citation are superseded history. That root `AGENTS.md` remains
outside this document's ownership, so it is left untouched here, and nothing in
it needs superseding. The residual drift is the 2026-11-24
`docs/FINAL_GATE_STATUS.md:85` snapshot, and it is cosmetic for pass/fail (the
four added rows are sub-15s checks that pass); the authoritative denominator is
always the live `DEFAULT_TESTS` list, not the prose.

## Verification method

Read-only: `sed`/`grep` over `tools/*.py`, `/tmp/opencode/r1_qemu_smoke.log`,
`docs/*.md` and the root `AGENTS.md`, plus `git log`/`ls` to confirm the
winsrv deletions. No QEMU run, no build, no `make clean`, no downloads, no
commit, no push, `kernelhwsrv/` untouched.

Correction (2026-11-26): superseding the 14-row entry in section 5 above, the authoritative gate is **15 rows** (`hal_probe` added as `DEFAULT_TESTS` row 15, confirmed by `tools/r1_final_verify.py --list`: 15 rows, row 15 = `hal_probe`; roster block `:134-150`), with `OVERALL: PASS (15 pass, 0 skip, 0 fail/not-run)`; the run that first established that row count was `FINAL20_RC=0` in `/tmp/opencode/final20_gate.log`, now superseded as live authority by `FINAL27_RC=0` in `/tmp/opencode/final27_gate.log` (all 15 rows `PASS`, `OVERALL: PASS (15 pass, 0 skip, 0 fail/not-run)`, 15/15 PASS); `FINAL20_RC` and `FINAL26_RC` are superseded history only; HEAD `ee8490e`, no commits.

Correction (2026-11-26): superseding section 1 above, `qemu_smoke` no longer stages the retired `portfilesrv` from `~/.cache`. `tools/r1_qemu_smoke.py` has no `~/.cache` reference at all: it stages the repo-built `build/user/GUISHELL.E32` (`:50`) and the tracked repo-root `EUSER.DLL` (`:51`) at `:142`, and derives its bind-slot/ordinal/export markers from those two images' headers rather than a cache contract (`:16-17`). `/tmp/opencode/final23_gate.log` recorded `qemu_smoke PASS 50.1s` inside `OVERALL: PASS (15 pass, 0 skip, 0 fail/not-run)` with `FINAL23_RC=0`, so GATE-PORTFILESRV-DECOUPLE was proven for that row at that run; the live authority is now `/tmp/opencode/final27_gate.log`, which records the same `qemu_smoke PASS 50.1s` under `FINAL27_RC=0` with `OVERALL: PASS (15 pass, 0 skip, 0 fail/not-run)`, so the row stays proven (`final23_gate.log` / `FINAL23_RC` and `final26_gate.log` / `FINAL26_RC` are now superseded history only). Section 1's premise no longer holds for any row, `selfcheck` included: `tools/r1_os_selfcheck.py` stages no `portfilesrv` and carries no `~/.cache` reference at all, and `tools/r1_genuine_ewsrv_test.py:39` is now `EUSER_DLL = PORT / "EUSER.DLL"` — the tracked repo-root binary (`git ls-files` → `qemu-port/EUSER.DLL`, 412104 bytes, blob `d768c67`), staged at `:93` behind the same hard `sys.exit(f"missing staged file: {src}")` guard at `:95` and re-asserted in the FAT at `:116`. `grep portfilesrv tools/r1_genuine_ewsrv_test.py` returns nothing, so the earlier "live retired-shim cache residual" reading of `:39` is wrong twice over: the constant is not `portfilesrv`, and no `~/.cache` read remains on the row. Real remaining coupling, OFF-ROSTER: `tools/r1_genuine_input_test.py:59-60` and `tools/r1_genuine_pointer_test.py:47-48` still hardcode `PORTFILESrv = "/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv"` alongside a cached `euser.dll`, stage it at `:114`/`:105` and assert it in the FAT at `:135`/`:126`. Neither script is in `DEFAULT_TESTS` (`tools/r1_final_verify.py:134-150`, 15 rows), so both are off-roster and the 15/15 gate is unaffected by them; they would break if `~/.cache` were pruned. `tools/r1_genuine_retire_test.py:55` is genuinely optional best-effort (`files.append` guarded by `PORTFILESRV.is_file()` at `:538-539`, never asserted), so it is a cache read but not a gate dependency. Read-only apart from this append: `grep`/`sed` over `tools/*.py` and `docs/*.md`, `/tmp/opencode/final23_gate.log`; no build, no `make clean`, no QEMU, no downloads, no commit, no push, `kernelhwsrv/` untouched.
