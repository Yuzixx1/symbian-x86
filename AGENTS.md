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
  `python3 tools/r1_final_verify.py` — the authoritative gate, running 11 tests
  in order (selfcheck → smoke → retire → efserver → ewsrv → eshell → fat_io →
  doom_main, doom_audio, doom_playtest, doom_multikey), staging a fresh DOOM ISO
  by default. Flags: `--list`, `--dry-run`, `--only NAME`, `--skip NAME`,
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

## Working style

- Follow the standing subagent/model rules in the global
  `~/.config/opencode/AGENTS.md` (4+ in flight, Mistral → HY4 fallback, no polling).
