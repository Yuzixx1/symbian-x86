# Sybian x86 — project instructions

## Goal

A bootable PC OS built around the genuine EKA2 kernel. **Use as much REAL
Symbian code as possible:** everything above the kernel should be genuine Symbian
source compiled from `kernelhwsrv/`. The old hand-written userland is being
retired, not extended: custom userspace changes would not work on the user's
friend's port. If the kernel works, the real Symbian components should work.

**Only write our own code to make genuine Symbian work on x86/PC** — a port
shim, driver, loader, or bridge, never a replacement for real Symbian logic.
Follow the layout, structure, and frame of actual Symbian as closely as possible:
same component names, file layout, APIs, E32/DLL/server shape, and conventions.
**GRUB is the one explicit exception** — use it as the boot loader.

## Hard boundaries

- `kernelhwsrv/` is **READ-ONLY**. Never edit, delete, or `make clean` there —
  compile FROM it only.
- All new implementation lives under `qemu-port/`.
- Primary deliverable: bootable GRUB ISO `qemu-port/build/symbian-grub.iso`
  (`make isogrub`).
- Quick check: from `qemu-port/`, `python3 tools/r1_os_selfcheck.py` (full
  one-command verification is in "Verification gates" below).
- Do not commit/push unless explicitly asked.
- Preservation points: git tag + branch `archive/handwritten-r25` at `56fbaff`,
  tarball `symbian-x86-archive-56fbaff.tar.gz` (git source only; build artifacts
  are reproducible with `make`).

## Genuine-userspace pivot

- Keep the prebuilt genuine `EUSER.DLL` (2503 exports); do not rebuild EUSER.
- Genuine sources present: EFSRV/F32, EWSRV, HAL, estart, etshell, domainmgr.
  Absent from this tree: ESOCK/sockets, comms DB, Bluetooth, UIKON/GDI.
- Boot in real VGA text mode; switch lazily to VBE graphics for graphical apps;
  never call int10 to switch back after graphics.
- DOOM/Freedoom is the integration proof; keep authentic feel (`EKA2_CRISP_STOP`
  stays reverted).
- `!`-prefixed server names need `ECapabilityProtServ` (granted in `E32Run`).
  The genuine boot chain runs on a supervisor DThread from `Asic::Init3`.
- Genuine boot order: EUSER → EFILE/EFSRV → ESTART → EWSRV → ESHELL, with
  `svcmgr`/`portfilesrv` as fallback until each retire gate passes.

### EFILE single-owner decision

- Keep the guard-only EFILE model: UID3 `0x100000bf`, atomic single-owner, no
  thread-entry trampoline. **DO NOT re-add the trampoline/veneer.** It breaks IPC
  (`#PF=2`, port-shim=`2`, IPC=`0`) because genuine EUSER TLS uses absolute low
  addresses (e.g. `0x110010`) mapped supervisor-only, so the trampoline faults on
  EFILE re-entry. Guard-only gives IPC round trip ×2, port-shim=`0`, `#PF=`0`.
  Fixing this properly needs per-process TLS mapping in `mmu/pd.cpp` / the EUSER
  loader — that is a separate, explicitly approved task, not a trampoline.
- Intra-process re-entry markers are tolerated **only if** IPC works, port-shim
  stays `0`, and there are no faults.
- Rationale and measurements: `docs/EFILE_GUARD_VS_TRAMPOLINE.md` and
  `docs/GUARD_TRAMPOLINE_REVIEW.md`.

## Verification gates

- **Full one-command verification:** from `qemu-port/`,
  `python3 tools/r1_final_verify.py` — the authoritative gate, running 15 rows
  in order (g12_residue → g12_negative → missing_statics → selfcheck → smoke →
  retire → efserver → ewsrv → eshell → fat_io → doom_main, doom_audio,
  doom_playtest, doom_multikey → hal_probe), staging a fresh DOOM ISO by
  default. Flags: `--list`, `--dry-run`, `--only NAME`, `--skip NAME`,
  `--skip-selfcheck`, `--keep-going`/`--no-keep-going`, `--rebuild-iso`,
  `--strict-skip`, `--log-dir DIR`. Logs land in
  `/tmp/opencode/final_verify_<name>.log`. Reviewed/fixed in
  `docs/FINAL_VERIFY_REVIEW.md`. Keep `r1_os_selfcheck.py` (above) as the fast
  pre-flight check only, not the full gate.
- `python3 tools/r1_genuine_retire_test.py` — per-subsystem EFSRV/EWSRV/ESTART/EUSER
  gates. **No shim is retired until its gate passes.**
- Also: `tools/r1_qemu_smoke.py`, `tools/r1_genuine_efsrv_test.py`,
  `tools/r1_genuine_ewsrv_test.py`, `tools/r1_genuine_eshell_test.py`.
- Build genuine components with `tools/genuine_build.py` (see `genuine/README.md`);
  PE2 import slots raised to 512; use `genuine/pack_multi.py` for multi-import images.
- Status docs: `docs/GENUINE_PROGRESS.md`, `docs/GENUINE_USERSPACE.md`,
  `docs/GENUINE_BUILD_REVIEW.md`. Keep them accurate.

### Current state (2026-11-26)

- **Verified — pivot:** commit `7f76900` pushed — genuine EKA2 kernel plus genuine
  Symbian userspace (prebuilt `EUSER.DLL`; EFSRV/F32/EFAT32, EWSRV, estart/etshell,
  HAL, domainmgr compiled from read-only `kernelhwsrv/`). GRUB remains the sole
  bootloader exception; shims are x86/PC glue only, never replacements for real
  Symbian logic.
- **Verified — tests (FINAL27, 2026-11-26):** from `qemu-port/`, the gate roster is
  **15 rows** (`tools/r1_final_verify.py:134-150`; rows `g12_residue`,
  `g12_negative`, `missing_statics` and `hal_probe` were added since the old
  11-row count) and the authoritative run is `/tmp/opencode/final27_gate.log`,
  ending `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)` with
  **`FINAL27_RC=0`** — i.e. 15/15 PASS. Also:
  `r1_genuine_retire_test.py` = 4/4; `r1_genuine_fullchain_test.py` = 14/14;
  DOOM PASS.
- **Verified — shim retirement:** commit `d26fade` (`d26fade5db13a140845a30e0faabe4550fb3940d`)
  user-authorized, committed AND pushed to `origin/main` after the `7f76900` pivot:
  `portfilesrv` + `r1_fs` retired; genuine EFSRV is the sole file server and the
  shim-only fallback is removed. Snapshots at `/tmp/opencode/shim_retire_backup_*`
  and rollback via a snapshot or `git show 7f76900:<path>` remain valid.
- **Caveats:** never accept `SessionPath r=-15` as IPC success; never re-add the
  EFILE trampoline (see EFILE single-owner decision above). Preservation:
  `archive/handwritten-r25` at `56fbaff` + tarball; never `git add -A`; no
  commit/push without an explicit request.
- **User decision (2026-11-24):** GUI/winsrv desktop is being removed (it failed);
  console shell + DOOM/Freedoom are the UI for now. Genuine EWSRV remains the
  target window server when the GUI returns.
- **Verified — winsrv/GUI retirement (2026-11-25):** deletion committed AND pushed
  as `94ce9c9` (`0f03f7e..94ce9c9`); 7 GUI files deleted. Guarded apply `APPLY_RC=0`
  with P1–P3 gates PASS; the post-deletion `r1_final_verify.py` run passed every
  row of the then-current **11-row** roster, which the 15-row FINAL27 gate above
  supersedes. Isolated `qemu_smoke` re-run PASS (one earlier re-run smoke FAIL was QEMU
  slot contention, exit 124 at 50s cap). Rollback snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436` unused; `kernelhwsrv/`
  clean.

#### Remaining hand-written components

- **Retirement = bypass, not deletion:** `d26fade` only commented two lines
  (`e32load.cpp:1552` `SvcMgrBootLaunch` fallback, `svcmgr.cpp:250` `PORTFILESRV`
  row); shim files and their kernel objects are still compiled/linked. Genuine
  EFSRV is the sole live file server.
- **SvcMgrExecInstall trap — RETIRED, stale warning withdrawn:** the old claim that
  `qemu_variant_asic.cpp:194` `SvcMgrExecInstall()` is still LIVE is dead — the
  function no longer exists (`grep -rn SvcMgrExecInstall` over `qemu-port/`
  sources = 0 hits; only docs and `tools/r1_shim_retire_plan.py` still name it,
  and that line is no longer a call site). svcmgr landed as `dd22ae2`; the trap
  was preserved under its new name `PortTrapExecInstall()`
  (`variant/e32load.cpp:1447`, called from `variant/qemu_variant_asic.cpp:171`),
  which still wraps the genuine exec 81 `FsRegisterThread` (clears critical
  flags → 0 FAULT) and hosts the PTRINJ sink; **do not remove blindly**
  (reintroduces KERN 4). winsrv has no apply path.
- **Still obsolete:** execs 268–270 (`SvcMgrBootLaunch`/kill/ctl set). **Retired
  since, files gone from HEAD:** svcmgr — `user/svcmgr.cpp` DELETED in `dd22ae2`
  (trap kept as `PortTrapExecInstall()`); `euser_shim.cpp` — DELETED in `0f9ec6a`,
  replaced by `user/ca_glue.cpp` PE2 IAT slots. Still superseded by the
  genuine stack: winsrv/`guishost.h`/`qemu_windowsrv.*`/GUI execs (DELETED
  2026-11-25 in `94ce9c9`) and the file-server shim files.
- **Legitimate bridges — KEEP:** PS/2 input, AC'97 audio, storage, display/PCI/
  variant/loader, `kern_shell`, `r1_*` harnesses, and netstack (authored
  exception; ESOCK absent).
- **Cleanup order:** file-server files → svcmgr → winsrv (DONE 2026-11-25; was
  after `edisp` + window gate) → EUSER shim; netstack **never**. Full inventory:
  `docs/REMAINING_SHIMS_AUDIT.md`.

## Working style

- Follow the standing subagent/model rules in the global
  `~/.config/opencode/AGENTS.md` (4+ in flight, Mistral → HY4 fallback, no polling).
