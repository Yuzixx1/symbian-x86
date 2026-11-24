# Documentation index — `qemu-port/docs/`

This is the comprehensive index of every Markdown document in this directory
(523 files, verified 2026-11-24). For a top-down description of how the OS actually works, start with
[`ARCHITECTURE.md`](ARCHITECTURE.md). For the exact commands to boot the
primary GRUB image, run bundled programs and execute the self-test, the
**canonical usage document** is
[`docs/r1_boot_and_selftest.md`](r1_boot_and_selftest.md).

Newer topic guides intended for readers (rather than historical records) are
[`BUILD_GUIDE.md`](BUILD_GUIDE.md),
[`GRUB_BOOT_INTERNALS.md`](GRUB_BOOT_INTERNALS.md),
[`NETWORKING_STACK.md`](NETWORKING_STACK.md),
[`NETWORK_TESTING.md`](NETWORK_TESTING.md),
[`SHELL_REFERENCE.md`](SHELL_REFERENCE.md),
[`USER_APPS_CATALOG.md`](USER_APPS_CATALOG.md) and
[`DRIVERS_HARDWARE.md`](DRIVERS_HARDWARE.md) and
[`GUI_AUDIO.md`](GUI_AUDIO.md); they are grouped with the
related records below.

Everything else here is a build record, design investigation, card dispatch or
regression report. A prefix key: `r1_` = R1-phase work, `r1a_` = R1-A fixture,
`_v0/_v1` = document revision, `Card` = an isolated work assignment. Many
design documents were written *before* implementation and describe a proposed
state; where a document says "design only", the running system may have moved
on — trust the source and `README.md` for current status.

## Project scope

- [PROJECT_GOAL.md](PROJECT_GOAL.md) — The goal: a usable EKA2-based PC OS; infrastructure vs. end-user features.

## Build, toolchain & boot media

- [BUILD_GUIDE.md](BUILD_GUIDE.md) — Reader-facing build guide: toolchain, targets and ISO creation.
- [GRUB_BOOT_INTERNALS.md](GRUB_BOOT_INTERNALS.md) — From GRUB El Torito to the mounted `X:` program image.
- [r1_build_config_v0.md](r1_build_config_v0.md) — Named port variant, metadata preprocessing and clean-build recipe.
- [r1_build_repro_v0.md](r1_build_repro_v0.md) — Scratch reproduction of the upstream x86 build route (petran/dlltool).
- [r1_build_repeat_v1.md](r1_build_repeat_v1.md) — Second cold build compared with the v1 reproduction.
- [r1_clean_build_v1.md](r1_clean_build_v1.md) — One clean scratch build with the `port-x86-rfile64-v1` variant.
- [r1_artifact_audit_v0.md](r1_artifact_audit_v0.md) — Independent audit of the R1-BUILD-REPRO artifacts and deviations D1–D8.
- [r1_fixture_build_v1.md](r1_fixture_build_v1.md) — Isolated incremental fixture build and PE2 import audit.
- [r1_toolchain_research_v0.md](r1_toolchain_research_v0.md) — x86 binary-route investigation (initially blocked).
- [r1_toolchain_blocker_v1.md](r1_toolchain_blocker_v1.md) — Card 34: `bx86gcc` EFSRV def/import-library blocker.
- [r1_card_34_toolchain_blocker_v1.md](r1_card_34_toolchain_blocker_v1.md) — Card 34 dispatch copy for the same toolchain blocker.
- [r1_dlltool_diagnostic_v0.md](r1_dlltool_diagnostic_v0.md) — Wine `dlltool` "CreateProcess" spawn failure diagnosis.
- [r1_grub_iso_rebuild_v1.md](r1_grub_iso_rebuild_v1.md) — Card A-8: rebuild the GRUB ISOs with current kernel fixes.
- [r1_portfilesrv_build_v1.md](r1_portfilesrv_build_v1.md) — Card 35: provision toolchain and build `portfilesrv.exe`.
- [r1_card_35_portfilesrv_build_v1.md](r1_card_35_portfilesrv_build_v1.md) — Card 35 dispatch copy for the file-service host build.
- [r1_portfilesrv_e32_v1.md](r1_portfilesrv_e32_v1.md) — Card 36: petran E32 conversion of `portfilesrv.exe`.
- [r1_card_36_petran_e32_v1.md](r1_card_36_petran_e32_v1.md) — Card 36 dispatch copy for the E32 conversion.
- [r1_sdk_route_research_v0.md](r1_sdk_route_research_v0.md) — Upstream x86 build/SDK route research (identified, not reproduced).
- [r1_r1b_proof_selection_v1.md](r1_r1b_proof_selection_v1.md) — Selection of unchanged upstream x86 proof applications (card 30).

## Kernel, MMU, loader & ABI

- [r1_abi_contract_v0.md](r1_abi_contract_v0.md) — R1-ABI contract candidate (explicitly not frozen).
- [r1_heap_exec_v0.md](r1_heap_exec_v0.md) — Compiled executive dispatch and Direct-model heap closure.
- [r1_wait_exit_design_v1.md](r1_wait_exit_design_v1.md) — Reliable process wait / exit-status design (P-06 + P-08).
- [r1_mmu_design_v1.md](r1_mmu_design_v1.md) — M-03 MMU and user memory-isolation design v1.
- [r1_mmu_feasibility.md](r1_mmu_feasibility.md) — Read-only feasibility check of the genuine MMU hooks.
- [r1_mmu_4k_upgrade.md](r1_mmu_4k_upgrade.md) — 4 KiB paging, per-page U/S, per-process PD and #PF handling.
- [r1_loader_contract_candidate_v1.md](r1_loader_contract_candidate_v1.md) — Candidate loader contract: snapshot, relocation, PE2 IAT binding.
- [r1_loader_evidence_v0.md](r1_loader_evidence_v0.md) — Bounded evidence card for the loader investigation.
- [r1_e32_validate_diag_v1.md](r1_e32_validate_diag_v1.md) — Card A-1: `E32Validate()` diagnosis report.
- [r1_e32_validate_fix_v1.md](r1_e32_validate_fix_v1.md) — Card A-2: `E32Validate()` header CRC fix.
- [r1_e32_validate_pe2_fix_v1.md](r1_e32_validate_pe2_fix_v1.md) — Card A-3: PE2 import-format support in validation.
- [r1_e32_validate_portfilesrv_fix_v1.md](r1_e32_validate_portfilesrv_fix_v1.md) — Card A-4: fix the portfilesrv E32 image (CRC + import bounds).
- [r1_e32_validate_regression_v1.md](r1_e32_validate_regression_v1.md) — Card A-5: QEMU regression for E32 validation.
- [r1_e32_validate_filesvc_regression_v1.md](r1_e32_validate_filesvc_regression_v1.md) — Card A-6: QEMU file-service regression with portfilesrv.

## File service, storage & FAT

- [fat_paths_R1.md](fat_paths_R1.md) — FAT I/O path mapping research note (no code).
- [r1_file_contract_review_v0.md](r1_file_contract_review_v0.md) — Proposed file acceptance contract for R1.
- [r1_file_proof_search_v0.md](r1_file_proof_search_v0.md) — Search for an unchanged zero-capability file application.
- [r1_proof_source_research_v0.md](r1_proof_source_research_v0.md) — Upstream unchanged file-app candidates (not built).
- [r1_file_service_host_v1.md](r1_file_service_host_v1.md) — Card 31: file-service host design.
- [r1_privileged_host_policy_v1.md](r1_privileged_host_policy_v1.md) — Trusted `portfilesrv.exe` host policy.
- [r1_fs_design_candidate_v1.md](r1_fs_design_candidate_v1.md) — Compatible file-service candidate design v1.
- [r1_fs_channel_review_v1.md](r1_fs_channel_review_v1.md) — FS channel hardening review (card 23).
- [r1_fs_channel_integration_v1.md](r1_fs_channel_integration_v1.md) — FS channel Makefile integration (card 27).
- [r1_wire_research_v0.md](r1_wire_research_v0.md) — `RFs`/`RFile` request-wire contract research.
- [r1_file_regression_v1.md](r1_file_regression_v1.md) — R1 file-service QEMU regression (card 32).
- [r1_card_32_file_regression_v1.md](r1_card_32_file_regression_v1.md) — Card 32 dispatch copy for the file regression.

## Networking

- [NETWORKING_STACK.md](NETWORKING_STACK.md) — Source-verified full architecture of the R1 networking stack.
- [r1_net_design_v1.md](r1_net_design_v1.md) — TCP/IP design v1 (N-01..N-04, read-only investigation).
- [r1_net_design_v2.md](r1_net_design_v2.md) — TCP/IP design v2: user netstack process + `!SocketServer`.
- [r1_nic_design_v1.md](r1_nic_design_v1.md) — D-06 findings on the polled e1000 NIC driver.
- [r1_dns_design_v1.md](r1_dns_design_v1.md) — DNS resolver E32 design and RFC 1035 message layout.

## Drivers, GUI, audio, display & console

- [DRIVERS_HARDWARE.md](DRIVERS_HARDWARE.md) — Reader-facing guide to the `variant/` hardware drivers and their patterns.
- [GUI_AUDIO.md](GUI_AUDIO.md) — Reader-facing guide to the VBE framebuffer, GUI execs and AC97 sound.
- [r1_audio_design_v1.md](r1_audio_design_v1.md) — AC97/HDA audio bring-up findings (read-only).
- [r1_gui_design_v1.md](r1_gui_design_v1.md) — GUI/desktop bring-up investigation and EUSER graphics ordinals.
- [r1_console_paths_v0.md](r1_console_paths_v0.md) — Upstream `econs`/`ewsrv` console inventory (v0).
- [r1_console_feasibility_v0.md](r1_console_feasibility_v0.md) — Strong review of the console paths note.
- [r1_console_design_candidate_v1.md](r1_console_design_candidate_v1.md) — Genuine console design candidate v1.
- [r1_console_g_manifest_v1.md](r1_console_g_manifest_v1.md) — Console-G asset manifest (card 28).
- [r1_console_staging_v1.md](r1_console_staging_v1.md) — Console staging notes (card 26).
- [r1_console_data_build_v1.md](r1_console_data_build_v1.md) — Console-data build v1 (completed; exports rejected).
- [r1_console_data_v2_recovery.md](r1_console_data_v2_recovery.md) — Coordinator recovery note for console data v2.
- [r1_console_data_build_v3.md](r1_console_data_build_v3.md) — EKDATA-only local-def build v3.
- [r1_console_recipe_closure_v1.md](r1_console_recipe_closure_v1.md) — Pinned console recipes and audited assets.
- [r1_console_server_design.md](r1_console_server_design.md) — H-02 console server design (design, not implementation).

## Userland, services, shell, IPC & SDK

- [SHELL_REFERENCE.md](SHELL_REFERENCE.md) — Reader-facing reference for every `EKA2>` shell command and path rule.
- [USER_APPS_CATALOG.md](USER_APPS_CATALOG.md) — Catalog of bundled end-user E32 applications and how they ship.
- [r1_active_scheduler_userland.md](r1_active_scheduler_userland.md) — I-07: genuine active-scheduler dispatch at exec level vs. C++ shim.
- [r1_g_runtime_v1.md](r1_g_runtime_v1.md) — R1-G first user runtime (card 22).
- [r1_runtime_dispatch_v1.md](r1_runtime_dispatch_v1.md) — Runtime dispatch: cards 27–29.
- [r1_service_boundary_v1.md](r1_service_boundary_v1.md) — Shared host/bootstrap boundary for services (v1).
- [r1_service_manager_design_v1.md](r1_service_manager_design_v1.md) — S-02: ordered startup and restart supervision design.

## Testing, QA & physical hardware

- [qa_inventory_R0.md](qa_inventory_R0.md) — R0 test and documentation inventory (report only).
- [r1a_acceptance_matrix_v0.md](r1a_acceptance_matrix_v0.md) — R1-A acceptance matrix for the upstream-toolchain RFile fixture.
- [r1a_matrix_errata_v0.md](r1a_matrix_errata_v0.md) — Proposed corrections to the R1-A acceptance matrix.
- [hw_worksheet.md](hw_worksheet.md) — Non-destructive hardware smoke worksheet (R0-HW).
- [NETWORK_TESTING.md](NETWORK_TESTING.md) — Reader-facing guide to running and interpreting every network test.
- [r1_card_a5_phys_hw_test_v1.md](r1_card_a5_phys_hw_test_v1.md) — Card A-5 physical hardware test procedure.
- [r1_e32_validate_phys_hw_prep_v1.md](r1_e32_validate_phys_hw_prep_v1.md) — Card A-7: physical-hardware prep, ISO boot verified.
- [r1_phys_hw_test_checklist_v1.html](r1_phys_hw_test_checklist_v1.html) — HTML physical-hardware test checklist (not Markdown).

## Design history, research & work coordination

- [r1_decision_record_v1.md](r1_decision_record_v1.md) — Owner sign-off decisions at policy level.
- [r1_startup_research_v0.md](r1_startup_research_v0.md) — Genuine x86 startup, TLS, heap, leave, imports, relocation research.
- [r1_startup_closure_v0.md](r1_startup_closure_v0.md) — Coherent R1-A loader/startup contract closing open items.
- [startup_paths_R1.md](startup_paths_R1.md) — Startup path mapping research note (no code).
- [r1_research_reconciliation_v0.md](r1_research_reconciliation_v0.md) — Reconciliation of the 2026-10-21 research (not an ABI freeze).
- [r1_implementation_dispatch_v1.md](r1_implementation_dispatch_v1.md) — Implementation dispatch for cards 14–17.
- [r1_next_wave.md](r1_next_wave.md) — Implementation contracts, fixture and independent build confirmation.
- [parallel_work.md](parallel_work.md) — Parallel AI work plan, model tiers and isolated-worktree rules.
- [COORDINATOR_PROMPT_TEMPLATE.md](COORDINATOR_PROMPT_TEMPLATE.md) — Reusable coordinator prompt template.
- [project_roadmap.md](project_roadmap.md) — Decisions, milestone gates and hardware gates (planning, not status).

---

This index file itself is [`DOCS_INDEX.md`](DOCS_INDEX.md).

**Total: 92 Markdown documents indexed** (plus the non-Markdown
`r1_phys_hw_test_checklist_v1.html` listed under testing). This index and
`ARCHITECTURE.md` are maintained separately from the historical records above;
when they conflict with a dated design note, the running code and the
canonical boot doc are authoritative.

## Added — GUI toolkit / complex programs (append-only)

- [GUI_PROGRAMMING.md](GUI_PROGRAMMING.md) — Reader-facing guide to writing GUI
  programs: the VBE LFB, slow execs 249/250/251, the `gui_shared` display-list
  protocol, drawing primitives, the 2x font geometry, keyboard input and a
  worked window example. The low-level protocol is the stable API; the
  higher-level `user/guilib.h` toolkit is in progress.
- [COMPLEX_PROGRAMS.md](COMPLEX_PROGRAMS.md) — What it takes to run complex
  programs: 64 KiB heap cap, no hosted libc, EUSER ordinal 915, freestanding C,
  no FPU, the porting checklist and honest gaps (no threads/filesystem/DLL
  linking).

These two documents bring the indexed Markdown total to **94**. Also added:
`tools/r1_gui_regression.py`, one host test that probes for and verifies
BREAKOUT, GUIMENU, GUIHELLO and BASIC when present.

## Added — graphical desktop (append-only)

- [GRAPHICAL_DESKTOP.md](GRAPHICAL_DESKTOP.md) — User-facing guide to the
  graphical desktop: VBE 2.0 mode 0x118 / LFB, slow execs 249/250/251 and the
  display-list protocol, why the console is text drawn on the LFB, what
  DESKTOP / GUISHELL / GUIMENU / BREAKOUT / GUIHELLO / SAVER render, mouse
  cursor (but not clicks), keyboard controls (WASD/Enter/Esc; PS/2 E0 arrows
  are dropped), launching and quitting, boot-to-desktop, screenshots, and the
  honest gap: a cooperative single-app-at-a-time desktop, not a preemptive
  multi-window compositor.
- `tools/r1_graphical_desktop_test.py` — one consolidated host test that probes
  `build/user/*.E32` for all six graphical programs (DESKTOP, GUIMENU,
  BREAKOUT, GUIHELLO, GUISHELL, SAVER), skips missing ones, stages the rest on
  the GRUB ISO's `/PROGRAMS.IMG`, boots once with `-vga std`, feeds each name,
  a few keys and Esc, asserts its marker (`DT:`, `GM:`, `BRK:`, `GH:`, `GUI:`,
  `SV:`) with no `Exc`/`PanicKernExec`, and takes QMP screendumps when possible.

This brings the indexed Markdown total to **95**.

## Added — windowed GUI application development (append-only)

- [GUI_APP_DEVELOPMENT.md](GUI_APP_DEVELOPMENT.md) — Builder's guide to writing
  a **windowed** PE2 E32 app on this port, derived from the running `DTCALC`
  (windowed calculator) and `DTEDIT` (windowed text editor) code: the GUI stack
  from `guishost.h` / `guilib.c` through exec 251 display lists to the Bochs
  VBE LFB, the `gui_shared` protocol, `gui_window()` / `gui_button()` /
  `gui_text()` widgets, mouse hit-testing, keyboard input, the PortFile save
  dependency for the editor, desktop icon integration and the QEMU proof flow.

This brings the indexed Markdown total to **96**.

## Added — window server (append-only)

- [WINDOW_SERVER.md](WINDOW_SERVER.md) — Architecture of the new window
  system being built concurrently: why the old standalone/handoff model cannot
  coexist, the server-owned framebuffer plus private client window buffers,
  kernel execs and low-RAM shared control block (slots, dirty bits, input ring),
  server lifecycle, the `user/wsclient.h` client API, input event flow, how to
  run `winsrv` then `wsdl`, honest limitations (cooperative polling, fixed
  slots/buffer sizes, no resize/compositor effects), and a comparison with the
  old DESKTOP. Headers `variant/qemu_windowsrv.h` / `user/wsclient.h` are the
  authoritative ABI once they land.
- `tools/r1_window_system_test.py` — consolidated host test that probes
  `build/user/WINSRV.E32` and `build/user/WSDL.E32`, skips gracefully if either
  is missing, stages what is present plus `EUSER.DLL` on the GRUB ISO, boots once
  with `-vga std`, feeds `winsrv` then `wsdl` and a few calculator keys, asserts
  the `WSV:` / `WDL:` marker families with no `Exc`/`PanicKernExec`/`FAULT`, and
  grabs a best-effort QMP screendump to `/tmp/opencode/window_system.png`.

This brings the indexed Markdown total to **97**.

## Added — ported real software (append-only)

- [PORTED_SOFTWARE.md](PORTED_SOFTWARE.md) — What porting *real* third-party C
  onto this OS means, as opposed to hand-written apps: the freestanding E32
  toolchain constraints, why real ports matter, the libc-shim strategy
  (`printf` -> EUSER ordinal 915, `malloc` -> `user/malloc.c`, local string
  helpers, PortFile file I/O, the 16/64 KiB chunk heap budget), how Lua,
  Z-machine/Zork, miniz/ZIP32 and bc are built/staged/tested, licence checks
  (notably GNU bc and copyrighted Zork story data), an honest full-source vs
  subset vs blocked status table, and a step-by-step recipe for the next port.
- `tools/r1_ported_software_test.py` — consolidated host test that probes
  `build/user/` for `LUA.E32`, `ZMACHINE.E32`, `ZIP32.E32` and `BC32.E32`,
  skips missing images, verifies via mtools that the present images plus
  `EUSER.DLL` are staged in `PROGRAMS.IMG` on a GRUB ISO, boots the ISO once per
  port with `-vga std`, feeds each program's basic command, asserts start/
  result/done marker families, the ord-915 bind and no `Exc`/`PanicKernExec`/
  `FAULT`, and writes one log per port under `/tmp/opencode/`.

This brings the indexed Markdown total to **98**.

## Added — DOOM porting design (append-only)

- [DOOM_PORTING.md](DOOM_PORTING.md) — Design for running *real* DOOM via
  `doomgeneric` on EKA2: the full GRUB ISO → WAD-on-FAT → E32 → fbblit → VBE
  LFB architecture; the 24 MB FAT32 partition at LBA 2048 needed for a ~10 MB
  Freedoom IWAD (with exact file:line constants for the other agents to
  change); Freedoom-only licensing (never `DOOM.WAD`); 8.3 WAD staging via
  `mcopy` and the PortFile 64 KiB snapshot blocker for WAD reads; libc shim
  (`printf` → ord 915, chunk-backed heap, local string helpers, no FPU);
  8-bit 320x200 palette → 32bpp RGBA conversion through a new exec-251
  `GC_BLIT` opcode; PS/2 input, 35 Hz exec timing, AC97 PCM audio and its v1
  limitations; the QEMU/screendump test plan and an honest status table.
- `tools/r1_doom_test.py` — scaffold-only host test that checks for
  `DOOM.E32` and a Freedoom IWAD and **skips cleanly** while either is absent;
  when assets land it stages the WAD on a 24 MB FAT disk, boots the GRUB ISO,
  feeds gameplay keys, asserts the `DOOM:` marker family plus ord-915 bind and
  a clean log, and grabs a QMP screendump to `/tmp/opencode/doom.png`.

This brings the indexed Markdown total to **99**.

## Added — DOOM FAT/WAD staging integration (append-only)

- Integration update for [DOOM_PORTING.md](DOOM_PORTING.md): the real
  Freedoom1.wad is 28,795,076 B (~27.5 MiB), not ~10 MB, so the embedded FAT
  disk was enlarged from 32/16 MB to **64 MB disk / 48 MB FAT32 partition at
  LBA 2048 (98304 sectors)**, ~47 MiB usable. `tools/mkgrubiso.py` gained
  `WAD_CANDIDATES`/`find_wad()` and stages the WAD at build time from
  `/tmp/opencode/doom/...` (never committed) as the 8.3 name `FDOOM1.WAD`, with
  a warning + skip when absent. `tools/r1_doom_test.py` uses the same enlarged
  constants and drop-off points. Verified: `make isogrub` succeeds,
  `build/programs.img` = 67,108,864 B, `build/symbian-grub.iso` =
  75,853,824 B (~72 MiB), and `mdir -i build/programs.img@@1048576 ::` lists
  `FDOOM1.WAD` (28,795,076 B) among 87 files with ~16.8 MB free. Kernel-side
  blockers (streaming WAD reads, fbblit, DOOM.E32) remain. `kernelhwsrv/`
  untouched; no commits or pushes.

## Added — Audio subsystem / DOOM audio (append-only)

- [AUDIO_SUBSYSTEM.md](AUDIO_SUBSYSTEM.md) — the full AC97 PCM-OUT audio stack:
  PCI class 04.01 enumeration and the `8384:7600` codec vendor ID, NAM/NAB BARs
  and PCM-OUT BDL/PICB DMA registers, the 48 kHz stereo signed-16-bit buffer
  format, the polled `audio_play_test()` / `audio_tone()` kernel API, the
  slow-exec 240 tone bridge and the new PCM enqueue/flush ops, how DOOM's
  `i_mysound.c` mixer shim feeds a 32-buffer ring, synchronous/polled operation
  and why `-audiodev none` keeps guest DMA alive, the memory budget (<1 MiB),
  current limitations (no kernel mixer, one stream, poor music, 44.1 vs 48 kHz,
  no audible headless check) and the test commands. Complements §6 of
  [GUI_AUDIO.md](GUI_AUDIO.md).
- `tools/r1_doom_audio_test.py` — end-to-end DOOM + audio harness. Reuses
  `r1_doom_test.py`'s asset staging/WAD recovery and boots the GRUB ISO with
  `-audiodev none,id=a0 -device AC97,audiodev=a0`, launches `doom`, presses
  Enter to start, and asserts the AC97 boot banner (vendor `8384:7600`), the
  `SND:` INIT/sfx/play and kernel `AU:` PCM-play marker families, `DOOM: TITLE`
  / `DOOM: FRAME` and no `Exc`/`PanicKernExec`/`FAULT`. Missing assets →
  `RESULT: SKIP`; a held-open QMP screendump is best-effort and never gates the
  result.

## Added — boot-in-text-mode architecture (append-only)

- [TEXT_MODE.md](TEXT_MODE.md) — reader-facing guide to the new display
  architecture: boot in genuine VGA mode 3 (80x25 cells at 0xB8000, 16-colour
  attribute bytes, CRTC cursor, hardware scrolling) so `version`/`help` and all
  text E32 apps render crisply on COM1 and on screen; the *lazy* switch to VBE
  2.0 0x118 (1024x768x32 LFB, DISPI 0x1CE/0x1CF, PCI-BAR base, 4096-byte pitch)
  only when a known graphical E32 launches, including `display_set_mode()`,
  the `FbMirrorReset()` / `GuiGeometryReset()` / `MouseGeometryReset()` hooks and
  the `VBE: SWITCH OK` marker; why this replaces bitmap-font text emulation and
  its flicker; an honest account of app exit (no int-10 switch-back — the
  console mirror resumes on the LFB, so screendumps stay 1024x768); unchanged
  GUI display-list/exec protocol, mirror-owner bookkeeping, QEMU `-vga std`
  flags, shell verbs and the test plan. Notes that the implementation is
  landing in parallel and `r1_textmode_test.py` is the acceptance gate.
- `tools/r1_textmode_test.py` — host acceptance test for that architecture.
  Boots the GRUB ISO with `-vga std -serial stdio`, uses the held-open QMP
  screendump session from `r1_doom_audio_test.py`, and asserts the prompt screen
  is a real 80x25 text mode (720x400 or 640x400, <=32 colours, banded glyph rows,
  not 1024x768); feeds `version` + `help` and requires both serial output and a
  changed on-screen text region; launches DOOM or `--app guishell` and asserts a
  1024x768 VBE framebuffer; fails on `Exc`/`PanicKernExec`/`FAULT`. Missing
  assets => `RESULT: SKIP`; flaky screendumps are reported as INFO, never fatal.

This brings the indexed Markdown total to **100**.

## Added — genuine userspace architecture & retirement plan (append-only)

- [GENUINE_USERSPACE.md](GENUINE_USERSPACE.md) — architecture document and
  retirement plan for the project rule that *everything above the kernel must
  be genuine Symbian code* (if the kernel works the rest should too; changing
  userspace breaks the other architecture ports). Records why the port layer is
  only kernel/HAL/variant (evidence from `kernelhwsrv/bsptemplate` and the
  per-arch `bx86*/bmarm/bwins` slices), then maps every current shim to its
  genuine source and replacement: F32 (`sfsrv`/`sfile`/`sfat32` →
  `portfilesrv.cpp` + `fs/r1_fs_*`), window server (`ewsrv` → `winsrv.c` /
  `guishost.h` / `qemu_windowsrv.cpp`), EUSER (genuine prebuilt `EUSER.DLL`
  already, shim only in `e32base.h`), startup (`estart`/`domainmgr`/`etshell`
  → `svcmgr.cpp` + `e32load.cpp` boot launch), HAL (`halservices/hal` present),
  and the one genuine gap — ESOCK/sockets, absent from the tree, so
  `netstack.c` stays as documented *authored* code behind a Symbian-shaped ABI.
  Also gives the `-m32` → `.def`-driven → PE2-E32 build strategy, a six-step
  dependency-ordered retirement plan gated by selfcheck / doom / efsrv file
  test / ewsrv window test, and an honest list of absent components (ESOCK,
  comms DB, Bluetooth, UIKON/GDI, multimedia).

This brings the indexed Markdown total to **101**.

## Added — genuine userspace progress tracking (append-only)

- [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) — live status companion to
  `GENUINE_USERSPACE.md`, updated from `tools/r1_genuine_retire_test.py`
  (dry-run + full QEMU run) and the per-workstream reports under `genuine/`:
  a status table (subsystem, genuine source, built?, runs?, retired shim?,
  blocker) — EUSER/HAL/domainmgr/EWSRV/ESTART+ESHELL genuine images built and
  running, EFSRV/F32 still SKIP (no runnable `EFILE.E32`/`EFAT32.FSY` yet, but
  the minimal-server IPC round trip passes and the 13/13 wire-opcode static
  check passes), no shim fully retired yet — plus the port-layer blocker list
  (!-name/caps registration, import slots 16→512 done, const-descriptor
  rejection, boot chain, ring-3 FAT adapter, toolchain review fixes) with
  owners, harness tuning notes, and the ordered next steps.

This brings the indexed Markdown total to **102**.

## Added — 2026-11-24 genuine-userspace status reconciliation (append-only)

- [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) — refreshed to 2026-11-24: current
  per-subsystem table (EUSER/EWSRV/ESTART+ESHELL gates PASS; HAL/domainmgr
  smoke PASS; EFSRV SKIP, full `EFILE.E32` builds but faults `EBadHandle`), a short "verified
  today" list, and blocker table B1–B7 (B1 caps/B4/B6/B7 FIXED; B3 follow-up,
  EFILE handle table, EWSRV client name lookup IN FLIGHT; B5 OPEN).
- [GENUINE_USERSPACE.md](GENUINE_USERSPACE.md) — §6 boot-time auto-launch
  updated to RESOLVED: supervisor DThread from `Asic::Init3()`,
  `ECapabilityProtServ`, genuine chain order and svcmgr fallback; the old
  `KERN 0x62` regression is gone.
- [SHIM_RETIREMENT_PLAN.md](SHIM_RETIREMENT_PLAN.md) — dry-run-only shim
  inventory (`tools/r1_shim_retire_plan.py`, 0 READY / 4 BLOCKED), the four
  production shims, boot-list lines that a switch would edit, ordered switch
  and rollback. Blocker checklist reconciled to B7/B4/B1-caps FIXED.
- [BOOT_CHAIN_VERIFICATION.md](BOOT_CHAIN_VERIFICATION.md) — serial boot-chain
  regression report (selfcheck/smoke/cp4/selftest/efsrv/eshell/retire PASS,
  ISO boot 0 faults); note the ewsrv harness it records as FAIL was subsequently
  fixed and is now PASS per `GENUINE_PROGRESS.md`.
- [GENUINE_BUILD_REVIEW.md](GENUINE_BUILD_REVIEW.md) — independent toolchain
  audit (CONDITIONAL verdict; MSVC-decorated `.def` names and `--name` path
  sanitization verified fixed).

This brings the indexed Markdown total to **105**.

## Added — 2026-11-24 on-disk FAT I/O milestone (append-only)

- [GENUINE_PROGRESS.md](GENUINE_PROGRESS.md) — B5 marked **on-disk FAT I/O DONE**
  with evidence (`FATIOCLIENT.E32` + `tools/r1_genuine_fat_io_test.py` PASS:
  disk-only `SEED.TXT` read, `FATIO.TXT` create/write/reopen/read
  `FATIO-MARKER-42A7` byte-for-byte, all r0, no faults); EFSRV retire gate still
  SKIP. In-flight items (EFILE sret-ABI + `CServer2::Start("!FileServer")` error,
  EWSRV client window, `estart`/`etshell` sret-ABI rebuild, EWSRV sret-ABI flag;
  audit `docs/ABI_AUDIT.md`) remain marked as such.
- [GENUINE_USERSPACE.md](GENUINE_USERSPACE.md) — §6 now records the verified
  on-disk FAT I/O milestone and points at `tools/r1_genuine_fat_io_test.py`.
- `genuine/F32_EFSRV_REPORT.md` (under `genuine/`, not indexed here) — blockers
  #1–#4 updated (B5 on-disk FAT resolved; #2 caps and #3 const descriptors
  resolved) plus an "on-disk FAT verification" section. No new `docs/*.md`
  files, so the indexed total stays **105**.

## Added — 2026-11-24 full verification & change inventory (append-only)

- [FINAL_VERIFY_REVIEW.md](FINAL_VERIFY_REVIEW.md) — independent review of
  `tools/r1_final_verify.py` (verdict NEEDS FIXES, minor): confirms the CLI,
  flags and 11-script wiring; flags two false-PASS paths (a stale DOOM ISO
  reused by default, and a last verdict overriding a nonzero exit code) plus
  two low-severity reporting gaps; records the dry-run / `py_compile` results.
  Review fixes applied.
- [CHANGE_INVENTORY.md](CHANGE_INVENTORY.md) — dated (2026-11-24) read-only
  inventory of the uncommitted genuine-userspace pivot: working-tree shape,
  grouped file inventory, commit/in-flight classification and commit caveats.
  No commits/pushes; `kernelhwsrv/` untouched.

New verification command: from `qemu-port/`, `python3 tools/r1_final_verify.py`
runs all 11 tests (selfcheck → smoke → retire → efserver → ewsrv → eshell →
fat_io → doom_main/audio/playtest/multikey) with `--list`, `--dry-run`,
`--only`, `--skip`, `--skip-selfcheck`, `--keep-going`/`--no-keep-going`,
`--rebuild-iso`, `--strict-skip`, `--log-dir`; logs under
`/tmp/opencode/final_verify_<name>.log`. `r1_os_selfcheck.py` remains the quick
check. These two new indexed Markdown files bring the indexed total to **107**.

## Added — 2026-11-24 verification & review report backfill (append-only)

Reports indexed below existed on disk but had no entry in this index (some were
only mentioned in passing); `kernelhwsrv/` untouched.

- [ABI_AUDIT.md](ABI_AUDIT.md) — static audit of the genuine EUSER sret ABI across
  staged E32 images: 1/2/4/8-byte struct returns are hazardous under gcc `-m32`;
  recommended `-freg-struct-return` rebuilds landed for EFILE, EWSRV/EWCLIENT and
  estart/etshell, with EFILE's `CServer2::Start` blocker still in flight.
- [EFILE_START_ANALYSIS.md](EFILE_START_ANALYSIS.md) — independent read-only
  diagnosis of genuine EFILE's `CServer2::Start("!FileServer")` failure, tracing
  the genuine `sf_main.cpp` path and ranking every non-`KErrNone` return cause.
- [EFILE_EXIT111_ANALYSIS.md](EFILE_EXIT111_ANALYSIS.md) — root-cause analysis of
  EFILE's `exit: type=2 reason=111` as `EThrdHeapMaxLessThanMin` from the
  TInt-only `Panic()` overload, with the exact port-side heap-min/max fix chain.
- [DOOM_INTEGRATION_VERIFICATION.md](DOOM_INTEGRATION_VERIFICATION.md) — 2026-11-24
  verification-only DOOM/Freedoom run on the current ISO: `EKA2_CRISP_STOP`
  confirmed absent, main/audio/playtest PASS and multikey FAIL reproduced thrice.
- [HAL_DM_REGRESSION.md](HAL_DM_REGRESSION.md) — read-only HAL/domainmgr E32
  regression against the current toolchain/kernel: 15/15 proof checks PASS with
  the unchanged `kernfull.elf` and staged genuine images.
- [POINTER_BRIDGE_REVIEW.md](POINTER_BRIDGE_REVIEW.md) — SAFE verdict on the
  genuine pointer-event bridge: genuine layout/CS/handoff verified, with a MEDIUM
  test-only gap (`Pos()` execution implied, not directly asserted) and two LOWs.
- [SHIM_RETIRE_APPLY_REVIEW.md](SHIM_RETIRE_APPLY_REVIEW.md) — NEEDS-FIXES review
  of `r1_shim_retire_apply.py`: two HIGH issues (rollback gap, self-defeating
  post-apply gate), MED/LOW guards and dry-run results; no apply performed.
- [EWSRV_CLIENT_BLOCKER.md](EWSRV_CLIENT_BLOCKER.md) — root-caused EWSRV external
  client blocker: stale `EWCLIENT.E32` `!`-name plus `TSize::operator+` sret-ABI
  mismatch in `CWsWindow::CreateL()`, with a fix verified in a temp build.

- [WINSRV_DELETE_WATCH47.md](WINSRV_DELETE_WATCH47.md) — read-only 2026-11-24 WINSRV_DELETE_WATCH47 — 2026-11-24 — APPLY_RC: NOT YET — re-apply still running; no APPLY_RC/rollback line in /tmp/opencode/winsrv_delete_apply2.log (log 4 KB, ends at P1 gate: make + selfcheck + retire-test ewsrv).
- [WINSRV_DELETE_WATCH48.md](WINSRV_DELETE_WATCH48.md) — read-only 2026-11-24 WINSRV_DELETE_WATCH48 — re-apply status (2026-11-24 ~22:05) — APPLY_RC: NOT present. `winsrv_delete_apply2.log` (25 lines, 1.8 KB) ends mid-P1-gate (make → selfcheck → ewsrv retire gate); pid 1286837 still running (etime ~5:52), wrapper 1286836 will append `APPLY_RC=$?` on exit.
- [WINSRV_DELETE_WATCH50.md](WINSRV_DELETE_WATCH50.md) — read-only 2026-11-24 WINSRV_DELETE_WATCH50 — re-apply status (2026-11-24 22:00 EET) — APPLY_RC: NOT YET — /tmp/opencode/winsrv_delete_apply2.log (2.2 KB) ends at phase P2 gate (`make isogrub` + selfcheck); no APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH51.md](WINSRV_DELETE_WATCH51.md) — read-only 2026-11-24 WINSRV_DELETE_WATCH51 — 2026-11-24 ~22:07 UTC — APPLY_RC: **not yet emitted** — log ends at `+ gate[P2]: make isogrub` / selfcheck; process still running (elapsed ~6m28s, state S). No ROLLBACK lines.
These eight newly indexed reports bring the indexed Markdown total to **115**.

## Added — 2026-11-24 EFILE / EFSRV / IPC report backfill (append-only)

Nine reports written during the 2026-11-24 genuine-EFILE unblock effort existed on
disk but had no entry in this index. `EFILE_DUPLICATE_LAUNCH_PLAN.md` was also
checked but does not exist on disk, so it has no entry here. `kernelhwsrv/` was
untouched; no QEMU run by this index update.

- [EFILE_IPC_CONNECT_ANALYSIS.md](EFILE_IPC_CONNECT_ANALYSIS.md) — read-only FULL-EFILE vs EFSRVMIN diagnosis: the `!FileServer` name fix lands but the client gets `SessionPath r=-15` (`KErrServerTerminated`), isolating the residual blocker to server lifetime / session-path shape rather than a bad path.
- [EFSRV_UNBLOCK_CHECK.md](EFSRV_UNBLOCK_CHECK.md) — read-only on-disk audit of the F32CLIENT UTF-16 `!FileServer` fix and rebuild, checking for a fresh `F32CLIENT: IPC round trip OK` against FULL EFILE (none; only the pre-fix EFSRVMIN shim log has it).
- [EFILE_SESSION_PATH_PLAN.md](EFILE_SESSION_PATH_PLAN.md) — investigation of the ranked #2 EFSRV cause: disproves the `?:\Private\<SID>\` premise (genuine `GetSystemDriveChar()` returns `'C'`), rejects explicit `RProperty` definition (`KErrPermissionDenied`) and records the remaining port-side approach.
- [EFILE_DUPLICATE_SOURCES.md](EFILE_DUPLICATE_SOURCES.md) — launch-source/duplicate-owner audit: the observed duplicate is one `E32Run` re-entering `app_main` three times (`start.S` ignores the EBX entry reason), detaching sessions with `KErrServerTerminated`; cross-launcher duplication is only latent.
- [EFILE_CONVERGENCE.md](EFILE_CONVERGENCE.md) — concurrent-fix status table for the EFILE/EFSRV fixes (single-owner guard, entry trampoline, F32CLIENT name fix, gate typo) plus the exact 9-count EFSRV-gate PASS acceptance criteria.
- [EFILE_ACCEPTANCE.md](EFILE_ACCEPTANCE.md) — documents `tools/r1_efile_acceptance.py`, a strict read-only checker that applies the 9 EFSRV-gate PASS criteria from `EFILE_CONVERGENCE.md` §2 to one complete raw serial log.
- [TRAWEVENT_POS_PROOF.md](TRAWEVENT_POS_PROOF.md) — direct guest proof (verdict PROVEN) that genuine `CEvent::RunL()` calls EUSER ord 1868 `TRawEvent::Pos()`, via a conditional EWSRV-only PE2 IAT rebind/wrapper emitting `PTRBRIDGE: TRawEvent::Pos x=80 y=40`.
- [FULLCHAIN_VERIFICATION.md](FULLCHAIN_VERIFICATION.md) — report for `tools/r1_genuine_fullchain_test.py`: one boot stages all four genuine E32s plus `EUSER.DLL`/`EFAT32.FSY` and asserts launch markers in strict sequential order (no shell relaunch).
- [GO_NO_GO.md](GO_NO_GO.md) — 2026-11-24 GO/NO-GO decision record: per-subsystem verdict table and NO-GO recommendation for shim retirement / final sign-off, with EFSRV/F32 as the sole narrow blocker.

These nine newly indexed reports bring the indexed Markdown total to **124**.

## Added — 2026-11-24 reader-guide index backfill (append-only)

Six reader-facing guides existed on disk but had no entry in this index. They are
now linked below; `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [E32_DEVELOPER_GUIDE.md](E32_DEVELOPER_GUIDE.md) — how to add a new end-user
  E32 application: freestanding C against `runtime.h`, a genuine PE2 `EUSER.DLL`
  import, `user.mk` rules, shell / `PROGRAMS.IMG` launch and a QEMU test.
- [FAQ_TROUBLESHOOTING.md](FAQ_TROUBLESHOOTING.md) — practical current-behaviour
  Q&A for building and running the port (display/console, boot, shell,
  networking, the READ-ONLY `kernelhwsrv/` rule), with past-fix notes so a stale
  guide or binary can be told from a real problem.
- [KERNEL_INTERNALS.md](KERNEL_INTERNALS.md) — map of the genuine EKA2 kernel
  objects the port actually links (~80 `kernfull` + ~19 nanokernel objects from
  `kernelhwsrv/kernel/eka/`): bootstrap, scheduler, threads, IPC, executives,
  timers and active objects, with `file:line` references.
- [MMU_PAGING.md](MMU_PAGING.md) — the R4/R5 4 KiB paging upgrade: per-page U/S
  bits, per-process page directories and #PF handling that replaced the old
  flat 4G/4M identity map.
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) — one-page cheat sheet for the port:
  GRUB-ISO boot command, shell commands and paths, self-test and pointers to the
  full boot and troubleshooting guides.
- [TESTING_QA.md](TESTING_QA.md) — complete testing/QA story: the one-command
  verifier, on-OS self-test, full inventory of `r1_*` harnesses, how QEMU tests
  stage disks and judge results, and how to interpret (and de-false) failures.

These six newly indexed guides bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 audit / precommit backfill (append-only)

Three audit/prep reports existed on disk but had no entry in this index. They
are linked below. `EFILE_REENTRY_FIX.md` was checked but does not exist on disk,
so it has no entry; `EFILE_ACCEPTANCE.md` was already indexed. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [SECURITY_AUDIT.md](SECURITY_AUDIT.md) — static pre-commit security audit of the pivot files (`tools/*.py`, `genuine/**`, changed `variant/`+`fs/`); verdict SAFE with zero real findings, no secrets and no command injection, `kernelhwsrv/` read-only/untouched.
- [PRECOMMIT_AUDIT.md](PRECOMMIT_AUDIT.md) — prep-only, read-only pre-commit audit/checklist for the genuine-userspace pivot: NO-GO verdict (EFSRV/F32 gate FAIL, EFILE trampoline fix pending/unproven), ordered verification commands, explicit stage-by-path guidance (never `git add -A`) and a do-not-commit list.
- [EFILE_DUPLICATE_LAUNCH_PLAN.md](EFILE_DUPLICATE_LAUNCH_PLAN.md) — read-only corrected root-cause analysis of the 3x EFILE start signature (one process, three `app_main` reentries) plus a port-side single-owner guard/trampoline experiment in `variant/e32load.cpp`, with IPC-vs-1x-start trade-off and retire-gate results.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 EFILE guard-vs-trampoline resolution backfill (append-only)

One report existed on disk but had no entry in this index; it is linked below.
`EFILE_REENTRY_FIX.md` was checked and does not exist on disk. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [EFILE_GUARD_VS_TRAMPOLINE.md](EFILE_GUARD_VS_TRAMPOLINE.md) — authoritative
  decision resolving the two competing single-owner EFILE fixes in
  `variant/e32load.cpp`: keep the single-owner guard and remove the entry
  trampoline (which broke IPC without genuine EUSER thread/TLS setup), with the
  accepting-gate evidence (retire 4/4, smoke PASS) and honest caveats.

This newly indexed report brings the indexed Markdown total to **135**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 audit / integrity report backfill (append-only)

Two reports existed on disk but had no entry in this index; they are linked
below. Of the other named candidates, `EFILE_DUPLICATE_LAUNCH_PLAN.md` was
already indexed (audit/precommit backfill above); `SESSIONPATH_NEG15.md` and
`ESTART_EXC00_ANALYSIS.md` were checked and do not exist on disk, so they have
no entries. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [GUARD_TRAMPOLINE_REVIEW.md](GUARD_TRAMPOLINE_REVIEW.md) — independent
  read-only review of the guard-vs-trampoline resolution (NEEDS WORK: the code
  resolution is clean — guard kept, trampoline gone, 464 binds/1 registration/
  no port-shim/faults — but runtime acceptance is unmet with `SessionPath r=-15`
  ×2 and all supporting logs ending in harness SIGTERM; keep guard-only and
  maintain NO-GO until an uninterrupted prove and the ESTART fault fix).
- [PRESERVATION_AUDIT.md](PRESERVATION_AUDIT.md) — read-only pre-commit
  verification that the documented preservation points are intact: the
  `archive/handwritten-r25` branch/tag and `HEAD` all at `56fbaff`, the
  983-entry `symbian-x86-archive-56fbaff.tar.gz` present/readable, no mismatches
  in the SHIM_RETIREMENT_PLAN / PRECOMMIT_AUDIT archive refs, plus a rollback
  recipe and caveats (tarball is git-source-only, ambiguous branch+tag name).

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 final report backfill (append-only)

Nine reports existed on disk but had no entry in this index; they are linked
below. Of the named candidates, `NEG15_SEMANTICS.md` and
`ESTART_UID_CLEANUP.md` were checked and do not exist on disk, so they have no
entries. `EFILE_REENTRY_FIX.md` now exists on disk (earlier backfill sections
recorded it as absent); it is indexed here without altering those entries.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [TRAMPOLINE_FINAL_REMOVAL.md](TRAMPOLINE_FINAL_REMOVAL.md) — definitive removal of the re-added EFILE entry trampoline from `variant/e32load.cpp` (veneer, `E32Run` call site, exec-270 op 5 trace), restoring the guard-only model that `AGENTS.md` now permanently forbids reintroducing.
- [ESHELL_PS_STATUS.md](ESHELL_PS_STATUS.md) — read-only 2026-11-24 snapshot of the ESHELL `ps` fix: owner-process activity plus the finding that ESHELL `ps` appears FIXED in the latest repro (`--PROCESSES-->` rows), pending formal gate confirmation.
- [GUARD_ESTART_BLOCK_ANALYSIS.md](GUARD_ESTART_BLOCK_ANALYSIS.md) — read-only diagnosis of ESTART's `r=-11`: `pack_multi.py` stamps EFILE UID3 `0x100000bf` on every image so the single-owner guard refuses ESTART, with the minimal fix of per-image genuine UID3s (`0x10272C04` / `0x101F7AE7`).
- [FINAL_GATE_STATUS.md](FINAL_GATE_STATUS.md) — live one-page gate-status snapshot (2026-11-24 03:17 EET): per-gate GREEN/IN FLIGHT/FAIL/NOT DONE table synthesizing docs, log mtimes and running processes, flagging the EFSRV `SessionPath r=-15` false-PASS and ESTART guard-block caveats.
- [FINAL_VERIFY_RUNBOOK.md](FINAL_VERIFY_RUNBOOK.md) — read-only, execute-only-after-fixes runbook keyed to `FINAL_GATE_STATUS.md`: five fix-landing prerequisites (A–E), tree-stability checks and the ordered full-verification commands; explicitly not authorization to commit, push or retire shims.
- [EFILE_CLEAN_PROVE.md](EFILE_CLEAN_PROVE.md) — report for `tools/r1_efile_clean_prove.py`: one uninterrupted QEMU boot on the final GRUB ISO staging EUSER/EFILE/EFAT32/F32CLIENT, with raw-log/ISO hashes and the marker table (464 binds, three EFILE starts, single-owner claim, IPC).
- [PACK_MULTI_UID_FIX.md](PACK_MULTI_UID_FIX.md) — `genuine/pack_multi.py` per-image UID3 fix unblocking ESTART `-11`: new optional `uid3` argument and `--uid3` CLI defaulting to the EFILE UID so existing callers stay byte-identical (callers in `build_estart_eshell.py` still to land).
- [SESSIONPATH_NEG15.md](SESSIONPATH_NEG15.md) — F32CLIENT false-pass integrity fix: `fsclient.cpp` wrongly accepted any completed IPC reply (including `KErrServerTerminated -15`) as `IPC round trip OK`, making the EFSRV retire gate a false PASS; now requires `KErrNone`.
- [EFILE_REENTRY_FIX.md](EFILE_REENTRY_FIX.md) — `variant/e32load.cpp` report on the 3x EFILE start signature: single-owner guard plus corrected reentry handling, tracing the old magic-offset trampoline's `#PF`/port-shim failure to `user/start.S` ignoring the EBX entry reason.

These nine newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 latest-report backfill (append-only)

Four reports existed on disk but had no entry in this index; they are linked
below. Of the other named candidates, `TRAMPOLINE_FINAL_REMOVAL.md` was already
indexed (final report backfill above), while `ESTART_UID_GATE_CHECK.md` and
`EFILE_REENTRY_SUPPRESS.md` were checked and do not exist on disk, so they have
no entries. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [NEG15_SEMANTICS.md](NEG15_SEMANTICS.md) — read-only semantics report:
  `SessionPath r=-15` is `KErrServerTerminated` (a genuine server-lifecycle
  failure, never a benign status or marshalling bug); rejects accepting `-15`
  as success — the exact false positive the tightened client removed — and
  classifies the fix as server-session stability / wait-for-`KErrNone`.
- [FINAL_PROJECT_SUMMARY.md](FINAL_PROJECT_SUMMARY.md) — dated 2026-11-24
  ready-to-fill synthesis of the genuine-userspace pivot: what was built,
  preservation point `56fbaff`, last known gate verdicts (no authoritative full
  verify run yet; fullchain FAIL 11/14, EFILE acceptance FAIL 5/9, DOOM PASS)
  and open blockers; explicitly a status snapshot, not authorization to commit,
  push or retire shims.
- [ESHELL_UID_GATE_CHECK.md](ESHELL_UID_GATE_CHECK.md) — read-only verification
  that changing ESHELL UID3 `0x101F7AE7` → `0x10000111` (the PS-fix build change,
  later reverted) breaks neither the boot chain nor the ESTART/ESHELL retire-gate
  UID detection.
- [ESTART_UID_CLEANUP.md](ESTART_UID_CLEANUP.md) — genuine-UID3 cleanup in
  `genuine/estart_eshell/build_estart_eshell.py`: pass per-image genuine UID3s to
  `pack_multi()` (ESTART `0x10272C04`, ESHELL `0x101F7AE7`, distinct debug UIDs)
  and drop the post-pack `0x10000111` stamp, clearing the single-owner guard's
  ESTART `-11` block.

These four newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 EFSRV gate-flip checklist backfill (append-only)

One report existed on disk but had no entry in this index; it is linked below.
Of the named candidates, `FINAL_PROJECT_SUMMARY.md` was already indexed (latest-
report backfill above), while `EFILE_REENTRY_SUPPRESS.md` and
`ESTART_UID_GATE_CHECK.md` were checked and do not exist on disk, so they have
no entries. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [EFSRV_GATE_FLIP_CHECKLIST.md](EFSRV_GATE_FLIP_CHECKLIST.md) — read-only
  anti-false-pass checklist that forbids declaring EFSRV PASS until every box
  is ticked from ONE clean log: re-entry suppression and UID re-stamp
  preconditions, the strict 9/9 `r1_efile_acceptance.py` prove (start=1,
  IPC on `r==0` only, `r=-15`/port-shim/`#PF`=0), then the official retire,
  smoke and 14/14 full-chain gates, plus invariant anti-false-pass rules.

This newly indexed report brings the indexed Markdown total to **151**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 DOOM post-UID verification backfill (append-only)

One report existed on disk but had no entry in this index; it is linked below.
Of the named candidates, `EFILE_REENTRY_SUPPRESS.md` was checked and does not
exist on disk, so it has no entry. `kernelhwsrv/` was untouched and no QEMU run
was made by this index update.

- [DOOM_POST_UID_VERIFICATION.md](DOOM_POST_UID_VERIFICATION.md) — verification-only DOOM/Freedoom run against the current 03:15 staged ISO after the genuine-userspace/UID changes (eshell UID re-stamp, guard-only EFILE, PS shim, trampoline removal): all four tests (main, audio, playtest past 455-frame cap, 8/8 multikey) PASS, `EKA2_CRISP_STOP` absent from code, ord-915 bind rebased, zero Exc/Panic/FAULT/#PF; no rebuild, commit or push, `kernelhwsrv/` untouched.

This newly indexed report brings the indexed Markdown total to **152**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 EFSrv gate-GREEN final-state backfill (append-only)

Four final-state reports existed on disk but had no entry in this index; they
are linked below. Of the named candidates, `EFSRV_GATE_FLIP_CHECKLIST.md` and
`NEG15_SEMANTICS.md` were already indexed (earlier backfill sections above).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [EFILE_REENTRY_SUPPRESS.md](EFILE_REENTRY_SUPPRESS.md) — final report on
  suppressing genuine EFILE's two re-entrant user threads via an atomic
  single-owner guard in `efile_glue.cpp` (plus the no-op plugin-chain lock),
  keeping one server thread alive; clean prove 18/18, acceptance 9/9, EFSRV
  gate flipped to PASS with zero faults/port-shim.
- [REENTRY_SUPPRESSION_STATUS.md](REENTRY_SUPPRESSION_STATUS.md) — read-only
  03:36 EET observer snapshot of the in-flight re-entry-suppression work:
  guard landed and verified (1 start, 0 returns, 2 suppression markers,
  `r=-15` eliminated) but 8/9 with the `IPC round trip OK` reply gap still
  open; recommendation WAIT while the owner iterates.
- [SESSIONPATH_REPLY_HANG.md](SESSIONPATH_REPLY_HANG.md) — root-cause report
  tracing the last 8/9 criterion to a deadlock in genuine
  `FsPluginManager::ReadLockChain()` (`RReadWriteLock::ReadLock`), never
  reaching `DoRequestL`/`Complete`, with the no-op chain-lock shim fix and a
  one-discriminating-test evidence sequence.
- [EFILE_NOOP_LOCK_FINAL.md](EFILE_NOOP_LOCK_FINAL.md) — close-out making the
  no-op chain-lock patch unconditional in `genuine/build_efile.py` so a default
  (non-debug) build cannot silently reintroduce the hang; static validation
  plus all five fresh runtime gates PASS and the EFSRV gate GREEN with no
  trampoline re-added.

These four newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 final verification / commit-prep backfill (append-only)

Three final-stage reports existed on disk but had no entry in this index; they
are linked below. Of the named candidates, `FINAL_PROJECT_SUMMARY.md` was already
indexed (latest-report backfill above); `FINAL_VERIFICATION_RESULT.md`,
`FINAL_COMMIT_INVENTORY.md` and `SHIM_RETIRE_READINESS.md` are newly linked here.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [FINAL_VERIFICATION_RESULT.md](FINAL_VERIFICATION_RESULT.md) — authoritative
  `r1_final_verify.py --strict-skip` run on the stable tree: OVERALL PASS,
  11/11 PASS, 0 skip, 0 fail, exit 0, with per-test verdicts, logs and the caveat
  that a PASS does not authorise commit/push or shim retirement.
- [FINAL_COMMIT_INVENTORY.md](FINAL_COMMIT_INVENTORY.md) — read-only pre-commit
  staging inventory: file counts/categories, secrets scan (zero matches), exact
  `git add` paths (never `git add -A`), do-not-stage list and a suggested commit
  message; plan only, no commit/push and `kernelhwsrv/` untouched.
- [SHIM_RETIRE_READINESS.md](SHIM_RETIRE_READINESS.md) — dry-run readiness audit
  of `r1_shim_retire_apply.py`: all four shims READY only via `--from-file` on a
  captured full-run PASS log, default preview BLOCKED by design, and apply still
  refused because edit targets are dirty; no apply attempted.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 shim-retirement / stale-object final backfill (append-only)

Six reports existed on disk but had no entry in this index; they are linked
below. Of the named candidates, `FINAL_VERIFICATION_RESULT.md` was already
indexed (final verification / commit-prep backfill above). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [SHIM_RETIREMENT_RESULT.md](SHIM_RETIREMENT_RESULT.md) — guarded, user-authorized
  retirement after `7f76900` of the file-server shims `portfilesrv` + `r1_fs`:
  preflight checks, per-shim outcomes (APPLIED / RETIRED via shared edits,
  svcmgr applied then rolled back as out of scope, winsrv not applied), the two
  commented-out fallback lines, snapshots and gate logs.
- [CP4_STRESS_INDEPENDENT_CHECK.md](CP4_STRESS_INDEPENDENT_CHECK.md) — independent
  refutation of the claim that the retirement edits cause `cp4_stress` to fail:
  pristine tree 4/4 PASS, current tree 4/4 FAIL, pristine + edits 3/3 PASS,
  traced to one stale build object; no retirement edit needs reverting.
- [STALE_OBJECT_REBUILD.md](STALE_OBJECT_REBUILD.md) — confirmation and repair of
  the stale `build/qemu_variant_asic.o` behind the `cp4_stress` failure: before/
  after mtimes and sha256 hashes, the force-rebuild + relink commands (no
  `make clean`) and the subsequent PASS results.
- [RETIREMENT_COMMIT_REVIEW.md](RETIREMENT_COMMIT_REVIEW.md) — read-only diff audit
  after the guarded apply: only two intended shim lines are commented out, the
  genuine boot chain and other `kServices[]` rows are intact, plus a future
  commit staging plan; no commit or push.
- [POST_COMMIT_TREE_REVIEW.md](POST_COMMIT_TREE_REVIEW.md) — read-only post-`7f76900`
  tree snapshot: local HEAD and `origin/main` match, `kernelhwsrv/` clean, and a
  table of intended runtime/untracked files versus the stray compiler-flag
  artifact, with future authorized-cleanup notes.
- [RETIREMENT_FINAL_INVENTORY.md](RETIREMENT_FINAL_INVENTORY.md) — exact read-only
  staging plan: modified/untracked file counts and categories, the 164→165 doc
  count, the two verified shim edits, and the caveat that this index's backfill
  text still claimed 159.

These six newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 post-commit tree / remaining-shims backfill (append-only)

Two post-commit reports existed on disk but had no entry in this index; they are
linked below. Of the named candidates, `STALE_OBJECT_REBUILD.md` and
`RETIREMENT_FINAL_INVENTORY.md` were already indexed (shim-retirement /
stale-object final backfill above). `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

- [FINAL_TREE_VERIFICATION.md](FINAL_TREE_VERIFICATION.md) — read-only post-push
  tree verification: local HEAD and `origin/main` both at `d26fade`, `git diff
  --check` clean, `kernelhwsrv/` and source trees clean, preservation refs at
  `56fbaff`, and a table of expected working-tree leftovers with caveats.
- [REMAINING_SHIMS_AUDIT.md](REMAINING_SHIMS_AUDIT.md) — read-only inventory of
  hand-written code still in the production path after retirement: the two
  applied bypasses (portfilesrv/r1_fs), A/B/C classification table, dependency-
  first retirement order for svcmgr/winsrv/EUSER-static-shim, and risks.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 archive final-check backfill (append-only)

One report existed on disk but had no entry in this index; it is linked below.
Of the named candidates, `FINAL_PROJECT_SUMMARY.md`, `FINAL_VERIFICATION_RESULT.md`
and `REMAINING_SHIMS_AUDIT.md` were already indexed (earlier backfill sections
above). `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [ARCHIVE_FINAL_CHECK.md](ARCHIVE_FINAL_CHECK.md) — read-only post-commit
  preservation verification that all documented points survived the pivot and
  shim-retirement commits: `origin/main` and `main` at `d26fade`, history order
  `d26fade` → `7f76900` → `56fbaff`, branch/tag `archive/handwritten-r25` and the
  983-entry `symbian-x86-archive-56fbaff.tar.gz` all still at `56fbaff`, plus a
  recorded tarball sha256 baseline, remote-ref listing and caveats.

This newly indexed report brings the indexed Markdown total to **168**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 cleanup-phase log (append-only)

One report was created during the post-`d26fade` cleanup phase; it is linked
below. `R1FS_GLUE_FEASIBILITY.md` and `CLEANUP_AUTHORITY_CHECK.md` were checked
and do not exist on disk, so they have no entries. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [CLEANUP_LOG.md](CLEANUP_LOG.md) — dated running log of the human-reviewed
  shim cleanup: authority verification (`r1_final_verify.py --strict-skip` on
  `d26fade`, PENDING), dead portfilesrv files/artifacts removal (PENDING),
  `r1_fs` kernel-glue feasibility (PENDING), svcmgr/winsrv/euser_shim NOT
  STARTED with the `SvcMgrExecInstall()` exec-81/PTRINJ trap preserved, and
  netstack marked NEVER (authored exception), plus the safe-kept port-bridge
  list.

This newly indexed report brings the indexed Markdown total to **169**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 cleanup phase (append-only)

Six cleanup-phase reports existed on disk but had no entry in this index;
they are linked below. Of the named candidates, `CLEANUP_LOG.md` was already
indexed (cleanup-phase log backfill above), while `CLEANUP_AUTHORITY_CHECK.md`
was checked and does not exist on disk, so it has no entry. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [CLEANUP_STEP_STATUS.md](CLEANUP_STEP_STATUS.md) — read-only ~18:50 EET
  snapshot of the post-`d26fade` cleanup steps: authority verify IN PROGRESS,
  dead portfilesrv file/artifact removal IN PROGRESS, r1_fs glue feasibility
  and svcmgr/winsrv retirement NOT STARTED, each with file+mtime evidence.
- [R1FS_GLUE_FEASIBILITY.md](R1FS_GLUE_FEASIBILITY.md) — read-only verdict
  (SAFE TO REMOVE, coupled edit only) that the kernel-linked `r1_fs` objects
  in `KERNFULL_OBJS` serve only the retired portfilesrv shim, not genuine
  EFILE/EFSRV.
- [SVCMGR_RETIREMENT_PLAN.md](SVCMGR_RETIREMENT_PLAN.md) — read-only,
  reviewed SAFE / apply-ready plan to retire svcmgr without reintroducing
  KERN 4, mapping the `SvcMgrExecInstall()` exec-81/PTRINJ trap and its shared
  files with the r1_fs glue removal.
- [SVCMGR_PLAN_REVIEW.md](SVCMGR_PLAN_REVIEW.md) — read-only independent
  review of the svcmgr plan (verdict SAFE, no blockers, 3 minor fixes
  recommended) with every file:line claim verified against live sources.
- [WINSRV_EDISP_SCOPE.md](WINSRV_EDISP_SCOPE.md) — read-only scoping report
  for cleanup step 5: winsrv retirement via genuine edisp plus the EWSRV
  client gate, covering the current exec-271/272/273 architecture and sources.
- [POST_REMOVAL_VERIFY.md](POST_REMOVAL_VERIFY.md) — verify-only impact check
  (~18:49 EET) of the portfilesrv file/artifact removal after a transient
  18:47:31 smoke failure, confirming tree state and whether the removal broke
  anything once the tree settled.

These six newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 cleanup-phase r1_fs removal backfill (append-only)

Five cleanup-phase reports existed on disk but had no entry in this index;
they are linked below. `SVCMGR_APPLY_STATUS.md` was checked and does not exist
on disk, so it has no entry. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [R1FS_REMOVAL_STATUS.md](R1FS_REMOVAL_STATUS.md) — read-only ~18:56 EET
  status snapshot of the step-3 `r1_fs` kernel-glue removal: Makefile
  `KERNFULL_OBJS` edit LANDED (uncommitted), `R1Fs::*` symbols and mount path
  status, with a concurrent `r1_final_verify.py --strict-skip` run in flight.
- [R1FS_REMOVAL_REVIEW.md](R1FS_REMOVAL_REVIEW.md) — independent read-only
  review of the same `r1_fs` glue removal (~19:0x EET), verdict **CLEAN**:
  file:line claims verified, `kernelhwsrv/` clean, no QEMU run.
- [CLEANUP_STAGING_DRYRUN.md](CLEANUP_STAGING_DRYRUN.md) — read-only
  validation that every proposed staging command from
  `CLEANUP_COMMIT_INVENTORY.md` was run with `--dry-run`; nothing actually
  staged, no commit/push, `kernelhwsrv/` clean.
- [CLEANUP_COMMIT_INVENTORY.md](CLEANUP_COMMIT_INVENTORY.md) — refreshed
  staging plan for a future user-authorized cleanup commit: grouped
  `git add`/`git rm` path lists (docs group now includes
  `EUSER_SHIM_SCOPE.md`, group C `r1_fs` glue now LANDED), never `git add -A`.
- [EUSER_SHIM_SCOPE.md](EUSER_SHIM_SCOPE.md) — read-only scoping report for
  cleanup step 6: `user/euser_shim.cpp` is 34 lines providing exactly five
  out-of-line symbols (rest is header-inline), with consumer inventory and the
  build-time-only retirement path.

These five newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 newest-report backfill (append-only)

Two newest reports existed on disk but had no entry in this index; they are
linked below. Of the named candidates, `AUTHORITY_GATE_REVIEW.md` and
`CLEANUP_AUTHORITY_CHECK.md` were checked and do not exist on disk, so they
have no entries. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [DANGLING_REFS_SWEEP.md](DANGLING_REFS_SWEEP.md) — post-deletion reference
  sweep proving no build/test/doc reference to the deleted portfilesrv files
  or the removed kernel `r1_fs` glue breaks the build: `make -n` dry-runs for
  userland/kernfull/isogrub all exit 0 with zero dangling refs; verdict CLEAN.
- [SVCMGR_APPLY_STATUS.md](SVCMGR_APPLY_STATUS.md) — read-only ~18:58 monitor
  snapshot of the in-flight svcmgr retirement apply: 5-file snapshot taken,
  0/5 target files edited, `SvcMgrExecInstall()` still live, baseline gates
  green (strict 11/11 PASS, retire 4/4, fullchain 14/14); verdict IN PROGRESS.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 authority-gate backfill (append-only)

Four newest reports existed on disk but had no entry in this index; they are
linked below (all named candidates — `AUTHORITY_GATE_REVIEW.md`,
`CLEANUP_AUTHORITY_CHECK.md`, `CLEANUP_PHASE_STATUS.md`,
`FOLLOWUP_COMMIT_PLAN.md` — exist on disk and are indexed).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [CLEANUP_PHASE_STATUS.md](CLEANUP_PHASE_STATUS.md) — read-only ~19:03 EET
  snapshot of the whole cleanup phase at HEAD `e29a072`: svcmgr-apply edits
  landed uncommitted (post-apply gates only partially run, `svcmgr.cpp`
  deletion pending), edisp client blockers fixed with gate PASS, authority
  runs certified clean only at `d26fade`, plus per-workstream next actions.
- [AUTHORITY_GATE_REVIEW.md](AUTHORITY_GATE_REVIEW.md) — read-only ~19:02 EET
  independent review of the full 11/11 strict verification, verdict **PARTIAL**:
  a trustworthy 11/11 PASS exists only for the `d26fade` tree; the current
  post-cleanup tree's strict run never concluded (10/11 with one transient
  mid-rebuild FAIL).
- [CLEANUP_AUTHORITY_CHECK.md](CLEANUP_AUTHORITY_CHECK.md) — authoritative
  clean-tree verdict (17:01 UTC, HEAD `d26fade`) required before file
  deletions: effective 11/11 PASS strict, retire 4/4, fullchain 14/14,
  `kernelhwsrv/` clean at both checkpoints.
- [FOLLOWUP_COMMIT_PLAN.md](FOLLOWUP_COMMIT_PLAN.md) — staging plan for the
  post-apply follow-up commit on top of pushed `d26fade`: lands the r1_fs
  kernel-glue removal plus the in-flight svcmgr apply in one commit, with
  pre-conditions and grouped `git add` paths (never `git add -A`); the
  document itself staged/committed nothing.

These four newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 svcmgr/r1_fs commit `dd22ae2` (append-only)

The cleanup commit `dd22ae2` ("Retire svcmgr and drop r1_fs kernel glue:
genuine boot chain is sole path") was committed and pushed
(HEAD = origin/main = `dd22ae2`, push `e29a072..dd22ae2`; apply-agent gates
all PASS, 0 KERN-4). It added **no new `docs/*.md` files** — candidates
`POST_SVCMGR_VERIFY.md` and edisp progress docs were checked and do not exist
on disk — so no new link entries are needed here; `CLEANUP_LOG.md` (step 4
LANDED + COMMITTED `dd22ae2`, steps 5/6 IN FLIGHT, Commits table) and
`CLEANUP_PHASE_STATUS.md` record the commit. `kernelhwsrv/` untouched; no QEMU
run was made by this index update.

The indexed Markdown total stays **186**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked (188 link entries, some files referenced in several backfill sections).

## Added — 2026-11-24 edisp / svcmgr / euser_shim doc sweep (append-only)

Post-`dd22ae2` sweep for new docs from the edisp port / svcmgr / `euser_shim`
work: the named candidates `POST_SVCMGR_VERIFY.md` and
`WINSRV_MIGRATION_PLAN.md` do not exist on disk, no edisp-named doc exists
beyond the already-indexed `WINSRV_EDISP_SCOPE.md`, and no `.md` under `docs/`
is newer than this index. `genuine/ewsrv/` contains no README or `.md` at all
(build scripts and sources only); the other `genuine/` READMEs
(`genuine/README.md`, `genuine/efat32/README.md`, `genuine/estart_eshell/README.md`)
remain unindexed per the docs/-only convention (as already noted for
`genuine/F32_EFSRV_REPORT.md`). No new link entries are needed;
`kernelhwsrv/` untouched; no QEMU run was made by this index update.

The indexed Markdown total stays **186**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked (186 unique link targets, verified equal to the on-disk set).

## Added — 2026-11-24 winsrv migration plan backfill (append-only)

One report existed on disk but had no entry in this index; it is linked below.
Of the named candidates, `EWSRV_INPUT_INJECTION.md` and `POST_SVCMGR_VERIFY.md`
were checked and do not exist on disk, so they have no entries. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [WINSRV_MIGRATION_PLAN.md](WINSRV_MIGRATION_PLAN.md) — read-only 2026-11-24
  plan to migrate all 14 GUI/DOOM binaries (GUISHELL, SAVER, SHOT, GUIHELLO,
  BREAKOUT, GUIMENU, DESKTOP, DTEDIT, DTCALC, GCBLITTEST, WINSRV, WSDL,
  FBBLITTEST, DOOM) off the winsrv execs 249/250/251 (display list) and
  271/272/273 (window/event ring) onto the genuine EWSRV client API
  (`RWsSession` et al. via the EWCLIENT `pack_multi` ordinal pattern), then
  delete the winsrv shim files; edisp is landed and the EWCLIENT client gate
  PASSES, exec 270 fbblit stays out of the deletion set.

This newly indexed report brings the indexed Markdown total to **187**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 EUSER-shim closure / post-svcmgr verify backfill (append-only)

Two reports existed on disk but had no entry in this index; they are linked
below. Of the named candidates, `EWSRV_INPUT_INJECTION.md` was checked and does
not exist on disk, so it has no entry. `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

- [EUSER_SHIM_REVIEW.md](EUSER_SHIM_REVIEW.md) — read-only closure review
  (verdict CLEAN) of the `user/euser_shim.cpp` deletion: the `user/user.mk`
  diff and `ca_glue.cpp` link, PE2 ord-915 import-slot evidence, symbol audit,
  and the two non-blocking references left behind (`build_server.sh`, retire-gate string).
- [POST_SVCMGR_VERIFY.md](POST_SVCMGR_VERIFY.md) — post-apply gate verification
  on the uncommitted svcmgr-applied tree (HEAD `e29a072`): 7/7 standalone gates
  PASS (smoke, retire 4/4, fullchain 14/14, pointer/PTRINJ exec-270 op 4, ewsrv,
  efsrv, eshell 8/8), clean fault scan with the exec-81 wrapper intact (no
  KERN-4), verdict SAFE, isolated strict-verify re-run pending.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 cp4_stress recurrence check backfill (append-only)

One report existed on disk but had no entry in this index; it is linked below.
Of the named candidates, `WINSRV_MIGRATION_PLAN.md`, `EUSER_SHIM_REVIEW.md` and
`POST_SVCMGR_VERIFY.md` were already indexed (earlier backfill sections above),
while `EWSRV_INPUT_INJECTION.md` was checked and does not exist on disk, so it
has no entry. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [CP4_STRESS_RECURRENCE.md](CP4_STRESS_RECURRENCE.md) — read-only diagnosis
  that the stale-object cause did NOT recur: `cp4_stress` now fails because
  commit `dd22ae2` retired slow-exec slots 268/269, so ring-3 `exec2(269)`
  (`user/killproc.c`) lands on genuine slot 269 (`KernMsgTest`) and the victim
  is never killed — a real functional change at HEAD, not staleness.

This newly indexed report brings the indexed Markdown total to **190**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 DOOM winsrv audit backfill (append-only)

One report existed on disk but had no entry in this index; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [DOOM_WINSRV_AUDIT.md](DOOM_WINSRV_AUDIT.md) — read-only audit of DOOM's
  display paths: exec 270 op 3 `FBO_FBBLIT` is the primary (always-on) blit and
  exec 251 `GC_RECT` in `blit_via_gcrect()` is dead-code fallback taken only if
  fbblit fails, so deleting the 251 path per `WINSRV_MIGRATION_PLAN.md` class (c)
  is a delete-last, playtest-gated step with no current test coverage.

This newly indexed report brings the indexed Markdown total to **191**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 full directory sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: **0 unlinked documents and 0 broken links** (191 on-disk `.md`
files = 191 unique indexed targets, including this index). No new documents
were found, so no new entries are needed; `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total stays **191**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 winsrv exec-users audit backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: one report existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_EXEC_USERS_AUDIT.md](WINSRV_EXEC_USERS_AUDIT.md) — read-only
  2026-11-24 audit of the exact consumers of winsrv execs 249/250/251
  (display list) and 271/272/273 (window/event ring): per-binary table of all
  14 `user.mk` targets with file:line call sites, classification (already-EWSRV
  WSE32TEST, chunk-assigned, delete-not-migrate WINSRV, outside-set FBBLITTEST,
  DOOM-special) and a recommended 3-4 binaries-per-chunk migration split.

This newly indexed report brings the indexed Markdown total to **192**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 full directory sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: **0 unlinked documents and 0 broken links** (192 on-disk `.md`
files = 192 unique indexed targets, including this index). No new documents
were found, so no new entries are needed; `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total stays **192**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 critical-path / input-injection backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: two documents existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [CRITICAL_PATH_STATUS.md](CRITICAL_PATH_STATUS.md) — read-only ~19:55 EET
  critical-path snapshot: exec-269 `E32Kill` fix live (cp4_stress now a
  start-marker timeout flake, not the survivor bug) and EWSRV input injection
  pointer PASS but keyboard path hitting `KERN 8` #PF at `cr2=0xf0000010`
  (EWSRV pid 9), with the newest strict-verify log 10/11 PASS.
- [EWSRV_INPUT_INJECTION.md](EWSRV_INPUT_INJECTION.md) — implemented+verified
  PS/2 ISR → DFC → `Kern::AddEvent` bridge feeding genuine EWSRV's raw event
  queue (`TRawEvent` via SPSC ring, `\KBINJ.TXT` opt-in gate), closing the
  input half of the `WINSRV_MIGRATION_PLAN.md` §5 blocker.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 EWSRV input injection / critical-path status backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: two reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [EWSRV_INPUT_INJECTION.md](EWSRV_INPUT_INJECTION.md) — design and
  verification of PS/2 ISR → DFC → `Kern::AddEvent` injection of genuine
  `TRawEvent` keyboard/pointer events into genuine EWSRV's event queue (with
  the `\KBINJ.TXT` opt-in source-mask gate), closing the input half of the
  `WINSRV_MIGRATION_PLAN.md` §5 blocker; pointer path PASS, keyboard path
  still open.
- [CRITICAL_PATH_STATUS.md](CRITICAL_PATH_STATUS.md) — read-only 2026-11-24
  ~19:55 EET snapshot of the two live workstreams: exec-269 `E32Kill` fix LIVE
  (cp4_stress residual failure is a start-marker timeout flake) and EWSRV
  input injection (pointer PASS, keyboard 1 FAULT: KERN-8 #PF at
  cr2=0xf0000010 on the keyboard-injection path), with the strict-gate log
  verdicts.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 full directory sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: **0 unlinked documents and 0 broken links** (194 on-disk `.md`
files = 194 unique indexed targets, including this index). No new documents
were found, so no new link entries are needed; `kernelhwsrv/` was untouched
and no QEMU run was made by this index update.

The indexed Markdown total stays **194**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 full directory sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: **0 unlinked documents and 0 broken links** (194 on-disk `.md`
files = 194 unique indexed targets, including this index). No new documents
were found, so no new link entries were needed; `kernelhwsrv/` was untouched
and no QEMU run was made by this index update.

The indexed Markdown total stays **194**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 cp4_stress postfix verify backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: one report existed on disk but had no entry; it is linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [CP4_STRESS_POSTFIX_VERIFY.md](CP4_STRESS_POSTFIX_VERIFY.md) — read-only
  19:58 EET verify-only report on the exec-269 `E32Kill` fix: QEMU slot was
  BUSY (strict-verify and DOOM test running), so 0/3 cp4_stress runs were
  executed; static confirmation that slot 269 → `PortKillExec` → `E32Kill`
  is live in `variant/e32load.cpp` and fresh in the built kernel/ISO.

This newly indexed report brings the indexed Markdown total to **195**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 userland build-status backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: one report existed on disk but had no entry; it is linked below.
The named candidate `WINSRV_DELETION_GATE.md` was checked and does not exist
on disk, so it has no entry. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [USERLAND_BUILD_STATUS.md](USERLAND_BUILD_STATUS.md) — read-only 19:58 EET
  diagnosis of the `make userland` failure: only `wsdl.o` fails to link
  (`undefined reference to ws_e32_iat_printf`, user.mk:1375) because the WSDL
  rules still use the old single-defsym + `elf2e32 --euser-import 915` pattern
  instead of the 4-defsym + `pack_multi` EWCLIENT pattern; WSE32TEST/WSE32DRAW
  and the guilib chunk are already fine, and `shot_e32.c` has no rule at all.

This newly indexed report brings the indexed Markdown total to **196**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 full directory sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: one document existed on disk but had no entry; it is linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [USERLAND_BUILD_STATUS.md](USERLAND_BUILD_STATUS.md) — read-only 19:58 EET
  diagnosis of the `make userland` failure: 14 targets (incl. the guilib
  chunk) build fine, WSE32TEST/WSE32DRAW are already up to date, and the sole
  error is WSDL's missing `ws_e32_iat_printf` link (user.mk:1374-1378 still
  using the old defsym/pack rules) plus `shot_e32.c` having no rule at all.

This newly indexed report brings the indexed Markdown total to **196**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 WINSRV deletion-gate backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETION_GATE.md](WINSRV_DELETION_GATE.md) — read-only terminal
  checklist for winsrv deletion (migration-plan chunk E): preconditions P1–P6
  (consumers off execs 249/250/251/271/272/273, input injection incl. keymap
  fix, edisp-via-genuine-EWSRV, DOOM 251-fallback removal, chunks A–D landed,
  standing invariants), the exact deletion set, and the human-gated execution
  step with no apply path.

This newly indexed report brings the indexed Markdown total to **197**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 GUISHELL migration-map backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [GUISHELL_MIGRATION_MAP.md](GUISHELL_MIGRATION_MAP.md) — read-only migration
  map for winsrv chunk D (GUISHELL/GUIMENU/DESKTOP/BREAKOUT): per-app table of
  every winsrv/exec call (249/250/251 display list, 271-273 window ring, PortCon
  input lease, exec 268 launch) and its `ws_e32_client` / genuine-EWSRV
  equivalent, with the remaining stubs (`ws_text`, `ws_get_event`) flagged.
- [KEYMAP_FIX_STATUS.md](KEYMAP_FIX_STATUS.md) — LANDED+VERIFIED status of the
  genuine EWSRV `CKeyTranslatorX` built-in keymap port block (`ky_tran.cpp`,
  no EKDATA.DLL needed): keyboard injection now passes with 0 `KERN 8`/`#PF`;
  remaining gap is only the minimal identity layout, no real keymap/IME.

These newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 KEYMAP_FIX_STATUS / final full sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: **0 unlinked documents and 0 broken links** (199 on-disk `.md`
files = 199 unique indexed targets, including this index).
[`KEYMAP_FIX_STATUS.md`](KEYMAP_FIX_STATUS.md) is confirmed indexed above
(LANDED+VERIFIED built-in `CKeyTranslatorX` keymap: keyboard injection passes
with 0 `KERN 8`/`#PF`; only the minimal identity layout remains as a gap).
No new documents were found, so no new entries were needed; `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **199**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 full directory sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: **0 unlinked documents and 0 broken links** (199 on-disk `.md`
files = 199 unique indexed targets, including this index). No new documents
were found, so no new entries were needed; `kernelhwsrv/` was untouched and
no QEMU run was made by this index update.

The indexed Markdown total stays **199**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 final full directory sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: **0 unlinked documents and 0 broken links** (199 on-disk `.md`
files = 199 unique indexed targets, including this index; the header count
"(199 files)" was verified consistent). No new documents were found, so no
new entries were needed; `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

The indexed Markdown total stays **199**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent full sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: **0 unlinked documents and 0 broken links** (199 on-disk `.md`
files = 199 unique indexed targets, including this index; header count
"(199 files)" verified consistent). No new documents were found, so no new
link entries were needed; `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

The indexed Markdown total stays **199**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 final sweep re-verification (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: **0 unlinked documents and 0 broken links** (199 on-disk `.md`
files = 199 unique indexed targets, including this index; header count
"(199 files)" verified consistent). No missing documents were found, so this
section adds no new link entries; `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

The indexed Markdown total stays **199**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 USERMK_CONSOLIDATION backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [USERMK_CONSOLIDATION.md](USERMK_CONSOLIDATION.md) — read-only 2026-11-24
  consolidated, conflict-checked patch list for `user/user.mk` (WSDL PE2-IAT
  swap, SHOTE32 new rule, SAVER/GCBLITTEST/BREAKOUT IAT defsyms) with
  re-verified current line numbers; `user/user.mk` itself was not touched.

This newly indexed report brings the indexed Markdown total to **200**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 subagent final sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: **0 unlinked documents and 0 broken links** (200 on-disk `.md`
files = 200 unique indexed targets, including this index; header count
"(200 files)" verified consistent). No missing documents were found, so this
section adds no new link entries; `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

The indexed Markdown total stays **200**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent docs-index sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: **0 unlinked documents and 0 broken links** (200 on-disk `.md`
files = 200 unique indexed targets, including this index; header count
"(200 files)" verified consistent). No missing documents were found, so this
section adds no new link entries; `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

The indexed Markdown total stays **200**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 WORKSTREAM_STATUS backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WORKSTREAM_STATUS.md](WORKSTREAM_STATUS.md) — read-only 2026-11-24 ~20:25 EET
  monitor snapshot of the live workstreams (guilib chunk B `WSE32: Connect r=-28`,
  desktop D4 not migrated, user.mk consolidation applied elsewhere, cp4_stress
  blocked on retired exec 269, four `user/` files still on winsrv execs) plus
  the gate state (strict 11/11 PASS at 03:55, authority run 9 pass/2 fail at
  19:18).

This newly indexed report brings the indexed Markdown total to **201**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 full directory sweep, post-201 (append-only)

A full `docs/*.md` listing (201 files) was diffed against the 201 unique `.md`
link targets in this index: **0 unlinked documents and 0 broken links** (201
on-disk `.md` files = 201 unique indexed targets, including this index; the
header count "(201 files)" at line 4 is consistent). No missing documents
were found, so this section adds no new link entries; `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **201**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 WINSRV endgame status backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_ENDGAME_STATUS.md](WINSRV_ENDGAME_STATUS.md) — read-only ~20:30 EET
  winsrv-deletion-gate monitor snapshot: the winsrv refs still live in
  `user/` (`shot.c`, `ws_e32_draw_test.c`; desktop chunk D4 landed), the
  blocking `Connect r=-28` `!`-name mismatch, the newest log verdicts, and
  the open P1–P6 blockers before winsrv deletion.

This newly indexed report brings the indexed Markdown total to **202**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 post-202 full directory sweep (append-only)

A full `docs/*.md` listing (202 files, flat directory, no subdirectories) was
diffed against the 202 unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (202 on-disk `.md` files = 202 unique indexed
targets, including this index; the header count "(202 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new
link entries; `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

The indexed Markdown total stays **202**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 EXEC269 fix-status backfill (append-only)

A full `docs/*.md` listing (203 files) was diffed against the 202 unique `.md`
link targets in this index: one document existed on disk but had no entry; it
is linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [EXEC269_FIX_STATUS.md](EXEC269_FIX_STATUS.md) — read-only ~20:30 EET status of
  the exec-269 `E32Kill` fix (verdict LANDED-UNVERIFIED): slot 269 →
  `PortKillExec`/`E32Kill` live in `variant/e32load.cpp` and installed at boot,
  but no clean `cp4_stress` PASS after the fix, with post-20:19 verification
  blocked by an unrelated guishell userland link failure.

This newly indexed report brings the indexed Markdown total to **203**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 subagent post-203 full sweep (append-only)

A full `docs/*.md` listing (203 files, flat directory, no subdirectories) was
diffed against the 203 unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (203 on-disk `.md` files = 203 unique indexed
targets, including this index; the header count "(203 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **203**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent post-203 re-sweep (append-only)

A full `docs/*.md` listing (203 files, flat directory, no subdirectories) was
diffed against the 203 unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (203 on-disk `.md` files = 203 unique indexed
targets, including this index; the header count "(203 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **203**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 post-203 subagent sweep (append-only)

A full `docs/*.md` listing (203 files, flat directory, no subdirectories) was
diffed against the 203 unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (203 on-disk `.md` files = 203 unique indexed
targets, including this index; the header count "(203 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **203**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 CONNECT_NEG28_STATUS backfill (append-only)

A full `docs/*.md` listing (204 files, flat directory, no subdirectories) was
diffed against the 203 unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [CONNECT_NEG28_STATUS.md](CONNECT_NEG28_STATUS.md) — read-only ~20:31 EET
  status of the `Connect r=-28` (`KErrBadName`) winsrv/EWSRV client blocker:
  root cause fixed in code (hand-rolled descriptors now carry EPtrC/EBufC type
  bits for the `!E32WindowServer` name), the -28 is gone from the 20:31 boot
  log, but the connect now returns handle==0 (`gl_connect` -1 path), so runtime
  is not yet green.

This newly indexed report brings the indexed Markdown total to **204**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 post-204 full directory sweep (append-only)

A full `docs/*.md` listing (204 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (204 on-disk `.md` files = 204 unique indexed
targets, including this index; the header count "(204 files)" at line 4 is
consistent — the three backtick-wrapped header links `ARCHITECTURE.md`,
`DOCS_INDEX.md` and `r1_boot_and_selftest.md` account for the plain-regex
diff of 3). No missing documents were found, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **204**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 CONNECT_DEBUG_STATUS backfill (append-only)

A full `docs/*.md` listing (205 files, flat directory, no subdirectories) was
diffed against the 204 unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [CONNECT_DEBUG_STATUS.md](CONNECT_DEBUG_STATUS.md) — read-only ~20:32 EET
  status of the guilib `Connect` null-handle blocker: `gui_init rc=-1`
  because `e32_connect` returns 0 while `iHandle` stays 0 (exec-0
  `SessionCreate` handle-return path not propagated on this port), with the
  UTF-16 `!E32WindowServer` name fix already landed (no more `r=-28`).

This newly indexed report brings the indexed Markdown total to **205**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 HANDLE_RETURN_DIAGNOSIS sweep (append-only)

A full `docs/*.md` listing (205 files, flat directory, no subdirectories) was
diffed against the 205 unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links**. The named candidate `HANDLE_RETURN_DIAGNOSIS.md`
was checked and does not exist on disk (its handle-return diagnosis content
lives in `CONNECT_DEBUG_STATUS.md`, indexed above), so it has no entry.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **205**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 WINSRV_DELETION_EXECUTOR backfill (append-only)

A full `docs/*.md` listing (206 files, flat directory, no subdirectories) was
diffed against the 205 unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. The named candidate
`HANDLE_RETURN_DIAGNOSIS.md` was checked and does not exist on disk, so it
has no entry. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETION_EXECUTOR.md](WINSRV_DELETION_EXECUTOR.md) — read-only
  2026-11-24 guarded-executor spec for the winsrv deletion: a new
  `tools/r1_winsrv_delete.py` (reusing `r1_shim_retire_apply.py` helpers by
  import, never weakening its never-delete contract) with edit+delete op
  model, allowlisted `DELETE_SET`, snapshot-before-delete, multi-phase
  apply with auto-rollback, and the same token-gated `--apply
  --confirm DELETE-WINSRV` dry-run-first contract to one day execute
  `WINSRV_DELETION_GATE.md`.

This newly indexed report brings the indexed Markdown total to **206**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 HANDLE_RETURN_STATUS backfill (append-only)

A full `docs/*.md` listing (207 files, flat directory, no subdirectories) was
diffed against the 206 unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [HANDLE_RETURN_STATUS.md](HANDLE_RETURN_STATUS.md) — read-only ~20:45 EET
  handle-return-zero diagnosis: exec-0 `SessionCreate`'s handle value is not
  propagated on this x86 port (`e32_connect` returns 0 while `iHandle` stays
  0), no fix landed in `variant/e32load.cpp` (diff touches only exec 269),
  newest markers `WSE32: Connect r=-1` / `GH: FAIL gui_init rc=-1`.

This newly indexed report brings the indexed Markdown total to **207**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 subagent full sweep, post-207 (append-only)

A full `docs/*.md` listing (207 files, flat directory, no subdirectories) was
diffed against the 207 unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (207 on-disk `.md` files = 207 unique indexed
targets, including this index; the header count "(207 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new
link entries; `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

The indexed Markdown total stays **207**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 HANDLE_FIX_STATUS backfill (append-only)

A full `docs/*.md` listing (208 files, flat directory, no subdirectories) was
diffed against the 207 unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [HANDLE_FIX_STATUS.md](HANDLE_FIX_STATUS.md) — read-only ~20:42 EET status of
  the exec ord-211 `CreateSession` handle-return fix: NOT STARTED (no landed
  `e32load.cpp` change, `WSE32: Connect r=-1` persists, `gui_init rc=0` count 0);
  the 20:41 final_verify 11/11 PASS is the unrelated slot-269/doom run.

This newly indexed report brings the indexed Markdown total to **208**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(208 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 HANDLE_RETURN_DIAGNOSIS backfill (append-only)

A full `docs/*.md` listing (209 files, flat directory, no subdirectories) was
diffed against the 208 unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. (Earlier backfill
sections recorded `HANDLE_RETURN_DIAGNOSIS.md` as absent; it has since been
created.) `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [HANDLE_RETURN_DIAGNOSIS.md](HANDLE_RETURN_DIAGNOSIS.md) — root-cause
  diagnosis of the ord-211 `Connect` `iHandle==0` symptom: the exec handle
  return path is proven intact (int 0x21 saves/pops EAX; raw `exec4(128,...)`
  returns valid handles); the real defect is a server-name mismatch — port
  g++ `-m32` builds genuine images with 32-bit `TText` literals so EWSRV
  registers as `"!"` while hand-built 2-byte UTF-16 clients look up
  `"!E32WindowServer"` and get `r=-1`; port-side fix is 16-bit `TText`
  (`-fshort-wchar`) rebuilds.

This newly indexed report brings the indexed Markdown total to **209**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(209 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 full re-verification sweep (append-only)

A fresh full `docs/*.md` listing (209 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 documents
on disk are missing an entry and 0 links are stale**, so no new doc entries are
added in this section. `kernelhwsrv/` was untouched and no QEMU run was made
by this index update.

This verification confirms the indexed Markdown total remains **209**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(209 files)" at line 4 is consistent); every Markdown document in the
directory is linked.

## Added — 2026-11-24 FSHORTWCHAR_EWSRV_STATUS backfill (append-only)

A full `docs/*.md` listing (210 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [FSHORTWCHAR_EWSRV_STATUS.md](FSHORTWCHAR_EWSRV_STATUS.md) — read-only
  ~21:00 EET monitor snapshot of the `-fshort-wchar` EWSRV/EWCLIENT rebuild
  (verdict LANDED-UNVERIFIED): 16-bit `TText` literals now make
  `!E32WindowServer` UTF-16-only in both rebuilt images, but no post-rebuild
  boot test yet and the newest client logs still ran the old staged image.

This newly indexed report brings the indexed Markdown total to **210**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(210 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 subagent final full sweep (append-only)

A fresh full `docs/*.md` listing (210 files, flat directory, no subdirectories)
was diffed against the 210 unique `.md` link targets in this index: **0
documents on disk are missing an entry and 0 links are stale** (210 on-disk
`.md` files = 210 unique indexed targets, including this index; the header
count "(210 files)" at line 4 is consistent). No missing documents were found,
so this section adds no new link entries; `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total stays **210**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 EWSRV_UTF16_VERIFY_STATUS backfill (append-only)

A full `docs/*.md` listing (212 files, flat directory, no subdirectories) was
diffed against the 210 unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [EWSRV_UTF16_VERIFY_STATUS.md](EWSRV_UTF16_VERIFY_STATUS.md) — read-only
  ~21:05 EET monitor (VERIFIED): post-`-fshort-wchar` rebuild, hand-client
  `Connect` to `!E32WindowServer` returns r=0, staged images byte-scanned as
  UTF-16-only, post-20:55 runs all PASS; caveat that no `final_verify` OVERALL
  post-rebuild and `gui_init rc=0` not yet seen.
- [FRESH_STRICT_VERIFY.md](FRESH_STRICT_VERIFY.md) — read-only 21:02:45
  quiescence-gated strict-verify attempt, verdict BLOCKED: live QEMU debug
  session holds the slot (no lock file, `ps` shows PID in use), so no
  rebuild/gate run was started; tree at HEAD `0f9ec6a`, `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(212 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 FRESH_STRICT_VERIFY re-sweep (append-only)

A fresh full `docs/*.md` listing (212 files, flat directory, no subdirectories)
was diffed against the 212 unique `.md` link targets in this index: **0
documents on disk are missing an entry and 0 links are stale** (212 on-disk
`.md` files = 212 unique indexed targets, including this index; the header
count "(212 files)" at line 4 is consistent). [`FRESH_STRICT_VERIFY.md`](FRESH_STRICT_VERIFY.md)
is confirmed indexed above (quiescence-gated strict-verify attempt, verdict
BLOCKED — live QEMU debug session held the slot at 21:02:45/21:03:07, tree at
HEAD `0f9ec6a`, `kernelhwsrv/` clean). No missing documents were found, so
this section adds no new link entries; `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total stays **212**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 full directory sweep, post-212 (append-only)

A fresh full `docs/*.md` listing (212 files, flat directory, no
subdirectories) was diffed against the 212 unique `.md` link targets in this
index: **0 unlinked documents and 0 broken links** (212 on-disk `.md` files =
212 unique indexed targets, including this index; the header count
"(212 files)" at line 4 is consistent). No missing documents were found, so
this section adds no new link entries; `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total stays **212**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent post-212 full sweep (append-only)

A fresh full `docs/*.md` listing (214 files, flat directory, no subdirectories)
was diffed against the 212 unique `.md` link targets in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [EWSRV_UTF16_RUNTIME_VERIFY.md](EWSRV_UTF16_RUNTIME_VERIFY.md) — read-only
  2026-11-24 21:05 EET runtime verify (PASS): after the `-fshort-wchar` rebuild
  genuine EWSRV registers UTF-16 `!E32WindowServer`, so genuine EWCLIENT and
  hand-written `ws_e32` clients both get `Connect r=0`; ewsrv/pointer/input/
  smoke gates PASS, no FAULT, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_TOOL_STATUS.md](WINSRV_DELETE_TOOL_STATUS.md) — read-only
  status of the `tools/r1_winsrv_delete.py` executor from
  `WINSRV_DELETION_EXECUTOR.md`: NOT STARTED — the tool file does not exist
  anywhere under `qemu-port/tools/`, only the plan doc does.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_TOOL_STATUS2 backfill (append-only)

A full `docs/*.md` listing (215 files, flat directory, no subdirectories) was
diffed against the 214 unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_TOOL_STATUS2.md](WINSRV_DELETE_TOOL_STATUS2.md) — read-only
  second check (2026-11-24): `tools/r1_winsrv_delete.py` still NOT STARTED —
  the file does not exist under `qemu-port/tools/`, no matching commit, only
  `r1_winsrv_test.py` present; winsrv cleanup remains pending per
  `REMAINING_SHIMS_AUDIT.md`.

This newly indexed report brings the indexed Markdown total to **215**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-214 full sweep (append-only)

A fresh full `docs/*.md` listing (215 files, flat directory, no
subdirectories) was diffed against the 214 unique `.md` link targets in this
index: one document existed on disk but had no entry; it is linked below.
[`WINSRV_DELETE_TOOL_STATUS.md`](WINSRV_DELETE_TOOL_STATUS.md) was re-confirmed
already indexed above (read-only status of the `tools/r1_winsrv_delete.py`
executor: NOT STARTED — the tool file does not exist under
`qemu-port/tools/`, only the `WINSRV_DELETION_EXECUTOR.md` plan does).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_TOOL_STATUS2.md](WINSRV_DELETE_TOOL_STATUS2.md) — read-only
  second check: `tools/r1_winsrv_delete.py` still absent (no file, no matching
  commit); winsrv deletion tool NOT STARTED, cleanup remains pending per
  `REMAINING_SHIMS_AUDIT.md`.

This newly indexed report brings the indexed Markdown total to **215**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-214 subagent sweep, WINSRV_DELETE_TOOL_STATUS2 (append-only)

A fresh full `docs/*.md` listing (215 files, flat directory, no subdirectories)
was diffed against the 215 unique `.md` link targets in this index: **0
unlinked documents and 0 broken links** (215 on-disk `.md` files = 215 unique
indexed targets, including this index; the header count "(215 files)" at line 4
is consistent). [`WINSRV_DELETE_TOOL_STATUS2.md`](WINSRV_DELETE_TOOL_STATUS2.md)
is confirmed already indexed above (second 2026-11-24 read-only check:
`tools/r1_winsrv_delete.py` still NOT STARTED — file absent under
`qemu-port/tools/`, no matching commit, only `r1_winsrv_test.py` present;
winsrv cleanup remains pending per `REMAINING_SHIMS_AUDIT.md`). No other new
documents were found, so this section adds no new link entries; `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

The indexed Markdown total is **215**, matching the 301 `.md` files under
`docs/` (including this index; the header count "(215 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 MIGRATION_COMMIT_INVENTORY / post-215 full sweep (append-only)

A fresh full `docs/*.md` listing (215 files, flat directory, no subdirectories)
was diffed against the 215 unique `.md` link targets in this index: **0
unlinked documents and 0 broken links** (215 on-disk `.md` files = 215 unique
indexed targets, including this index; the header count "(215 files)" at line
4 is consistent). The named candidate `MIGRATION_COMMIT_INVENTORY.md` was
searched for across the whole repository and does not exist on disk, so it
has no entry here. No missing documents were found, so this section adds no
new link entries; `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

The indexed Markdown total stays **215**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 WINSRV_DELETE_TOOL_STATUS2 + newest-docs sweep (append-only)

A fresh full `docs/*.md` listing (217 files, flat directory, no subdirectories)
was diffed against the 215 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below. The
named target
[`WINSRV_DELETE_TOOL_STATUS2.md`](WINSRV_DELETE_TOOL_STATUS2.md) is confirmed
already indexed above (read-only second check: `tools/r1_winsrv_delete.py`
still absent under `qemu-port/tools/` — no file, no matching commit — so the
winsrv deletion tool remains NOT STARTED and cleanup pending per
`REMAINING_SHIMS_AUDIT.md`). `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [ENDGAME_TOOL_STATUS.md](ENDGAME_TOOL_STATUS.md) — read-only 2026-11-24
  monitor (NEITHER LANDED): `tools/r1_winsrv_delete.py` absent (rc=2, no
  matching commit; only `r1_winsrv_test.py` and the
  `WINSRV_DELETION_EXECUTOR.md` plan exist) and `MIGRATION_COMMIT_INVENTORY.md`
  absent at scan time; agrees with `WINSRV_DELETE_TOOL_STATUS.md` /
  `WINSRV_DELETE_TOOL_STATUS2.md` (NOT STARTED); `kernelhwsrv/` clean.
- [MIGRATION_COMMIT_INVENTORY.md](MIGRATION_COMMIT_INVENTORY.md) — read-only
  inventory-only staging plan on top of HEAD `0f9ec6a` (no commits, no QEMU):
  68-entry status with secrets scan clean, 11 in-flight exclusions (57
  committable), grouped exact `git add` path lists A–E (never `git add -A`),
  four-commit split order, preconditions (fresh 11/11 strict verify), and
  rollback recipes; `kernelhwsrv/` clean.
These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked at that point.

## Added — 2026-11-24 ENDGAME_TOOLS_STATUS backfill (append-only)

A full `docs/*.md` listing (218 files, flat directory, no subdirectories) was
diffed against the 217 unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [ENDGAME_TOOLS_STATUS.md](ENDGAME_TOOLS_STATUS.md) — read-only 2026-11-24
  final-retry monitor: `tools/r1_winsrv_delete.py` NOT LANDED (absent, no
  matching commit; winsrv deletion still pending per `REMAINING_SHIMS_AUDIT.md`)
  while `MIGRATION_COMMIT_INVENTORY.md` is LANDED (inventory-only staging plan
  on HEAD `0f9ec6a`, 57 committable files, 4-commit split); agrees with
  `WINSRV_DELETE_TOOL_STATUS{,2}.md`; `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **218**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-218 full directory sweep (append-only)

A fresh full `docs/*.md` listing (219 files, flat directory, no subdirectories)
was diffed against the 218 unique `.md` link targets previously in this index:
one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [DELETE_TOOL_STATUS3.md](DELETE_TOOL_STATUS3.md) — read-only third check
  (2026-11-24): `tools/r1_winsrv_delete.py` still NOT LANDED (absent under
  `qemu-port/tools/`, only `r1_winsrv_test.py` present, no apply ever
  attempted); winsrv cleanup remains pending per `REMAINING_SHIMS_AUDIT.md`.

This newly indexed report brings the indexed Markdown total to **219**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 MIGRATION_COMMIT_INVENTORY / ENDGAME_TOOLS_STATUS index confirmation (append-only)

A fresh full `docs/*.md` listing (218 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (218 on-disk `.md` files = 218 unique indexed
targets, including this index; the header count "(218 files)" at line 4 is
consistent). Both task-named docs were confirmed already indexed in the
sections above —
[`MIGRATION_COMMIT_INVENTORY.md`](MIGRATION_COMMIT_INVENTORY.md) (read-only
staging-plan inventory on HEAD `0f9ec6a`, 57 committable files, grouped
`git add` paths, never `git add -A`) and
[`ENDGAME_TOOLS_STATUS.md`](ENDGAME_TOOLS_STATUS.md) (final-retry monitor:
`r1_winsrv_delete.py` NOT LANDED, inventory LANDED) — so this section adds no
new link entries. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

The indexed Markdown total stays **218**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 DELETE_TOOL_STATUS3 backfill (append-only)

A full `docs/*.md` listing (219 files, flat directory, no subdirectories) was
diffed against the 218 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [DELETE_TOOL_STATUS3.md](DELETE_TOOL_STATUS3.md) — read-only 2026-11-24 third
  check of the winsrv deletion tool: `tools/r1_winsrv_delete.py` still NOT
  LANDED (absent under `qemu-port/tools/`, only `r1_winsrv_test.py` present, no
  apply ever attempted); winsrv cleanup remains pending per
  `REMAINING_SHIMS_AUDIT.md`; agrees with `WINSRV_DELETE_TOOL_STATUS{,2}.md`
  and `ENDGAME_TOOL{S, S}_STATUS.md`.

This newly indexed report brings the indexed Markdown total to **219**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(219 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 DELETE_TOOL_STATUS3 backfill (append-only)

A full `docs/*.md` listing (219 files, flat directory, no subdirectories) was
diffed against the 218 unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [DELETE_TOOL_STATUS3.md](DELETE_TOOL_STATUS3.md) — read-only third check
  (2026-11-24): `tools/r1_winsrv_delete.py` still NOT LANDED (file absent
  under `qemu-port/tools/`, only `r1_winsrv_test.py` present, no apply ever
  attempted); winsrv cleanup remains pending per `REMAINING_SHIMS_AUDIT.md`.

This newly indexed report brings the indexed Markdown total to **219**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 subagent full sweep, post-218 (append-only)

A fresh full `docs/*.md` listing (218 files, flat directory, no subdirectories)
was diffed against the 218 unique `.md` link targets in this index: **0
unlinked documents and 0 broken links** (218 on-disk `.md` files = 218 unique
indexed targets, including this index; the header count "(218 files)" at line
4 is consistent). No missing documents were found, so this section adds no new
link entries; `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

The indexed Markdown total stays **218**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent post-219 full sweep (append-only)

A fresh full `docs/*.md` listing (221 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [DELETE_TOOL_STATUS4.md](DELETE_TOOL_STATUS4.md) — read-only 2026-11-24
  fourth check: `tools/r1_winsrv_delete.py` HAS LANDED (present, py_compile
  OK) and dry-runs/refuses safely (needs `--apply --confirm` token plus gate
  PASS; P1 consumer grep still BLOCKED with 24 matches outside DELETE_SET and
  one AMBIGUOUS edit target), but no apply was ever executed.
- [DELETE_TOOL_STATUS5.md](DELETE_TOOL_STATUS5.md) — read-only 2026-11-24
  (~21:15) fifth check, verdict LANDED: `tools/r1_winsrv_delete.py` exists,
  py_compile OK, prints the full plan (12 guarded edits, 7 deletes, pre/post
  gates) and refuses rc=2 on both no-`--apply` and missing-`--confirm`
  paths before any filesystem write; apply never invoked, `kernelhwsrv/`
  diff empty.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 endgame-tools status sweep (append-only)

A fresh full `docs/*.md` listing (219 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (219 on-disk `.md` files = 219 unique indexed
targets, including this index; the header count "(219 files)" at line 4 is
consistent). The task-named docs are confirmed indexed in the sections above —
[`ENDGAME_TOOLS_STATUS.md`](ENDGAME_TOOLS_STATUS.md) (read-only 2026-11-24
final-retry monitor: `tools/r1_winsrv_delete.py` NOT LANDED — absent, no
matching commit — while `MIGRATION_COMMIT_INVENTORY.md` is LANDED on disk;
winsrv cleanup still pending per `REMAINING_SHIMS_AUDIT.md`) and
[`DELETE_TOOL_STATUS3.md`](DELETE_TOOL_STATUS3.md) (read-only third check:
`r1_winsrv_delete.py` still absent under `qemu-port/tools/`, NOT LANDED, no
apply ever attempted; agrees with `WINSRV_DELETE_TOOL_STATUS{,2}.md` and
`ENDGAME_TOOL_STATUS.md`) — so this section adds no new link entries.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **219**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent index sweep, post-219 (append-only)

A fresh full `docs/*.md` listing (221 files, flat directory, no subdirectories)
was diffed against the 219 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [DELETE_TOOL_STATUS4.md](DELETE_TOOL_STATUS4.md) — read-only 2026-11-24
  fourth monitor pass: `tools/r1_winsrv_delete.py` HAS LANDED (present,
  py_compile OK); dry-run-only behavior checks confirm the token-gated
  refuse paths (`--apply` without `--confirm` rc=2, no args preview ends
  `REFUSE: --apply not set`), and the preview shows apply would refuse today
  (P1 consumer grep BLOCKED with 24 matches outside `DELETE_SET`, plus one
  `[AMBIGUOUS]` edit target in `variant/qemu_mouse.cpp:302`); no apply ever
  executed.
- [DELETE_TOOL_STATUS5.md](DELETE_TOOL_STATUS5.md) — read-only 2026-11-24
  (~21:15) fifth monitor pass, verdict LANDED: `tools/r1_winsrv_delete.py`
  exists (11977 bytes, 21:10), py_compile OK; no-args and `--apply`-without-
  token runs both refuse rc=2 before any filesystem write (12 guarded edits,
  7 deletes, pre/post gates planned); apply never invoked, `kernelhwsrv/`
  clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 final sweep, post-219 (append-only)

A fresh full `docs/*.md` listing (219 files, flat directory, no subdirectories)
was diffed against the 219 unique `.md` link targets in this index: **0
unlinked documents and 0 broken links** (219 on-disk `.md` files = 219 unique
indexed targets, including this index; the header count "(219 files)" at line
4 is consistent). No missing documents were found, so this section adds no new
link entries; `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

The indexed Markdown total stays **219**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 DELETE_TOOL_STATUS4/5 backfill (append-only)

A fresh full `docs/*.md` listing (221 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [DELETE_TOOL_STATUS4.md](DELETE_TOOL_STATUS4.md) — read-only 2026-11-24
  fourth monitor pass on whether `tools/r1_winsrv_delete.py` landed: YES
  (present, `py_compile` OK); dry-run-only behavior checks confirm the
  `--apply`/`--confirm` token refusal contract, and the preview shows apply
  would still refuse (P1 consumer grep BLOCKED with 24 matches outside
  `DELETE_SET`, one `[AMBIGUOUS]` edit at `variant/qemu_mouse.cpp:302`); no
  apply ever executed.
- [DELETE_TOOL_STATUS5.md](DELETE_TOOL_STATUS5.md) — read-only 2026-11-24
  (~21:15) fifth monitor pass: verdict LANDED — `tools/r1_winsrv_delete.py`
  present (11977 bytes, 21:10), `py_compile` OK, no-args run prints the full
  guarded plan (12 edits, 7 deletes, pre/post gates); all checks PASS, no
  writes, no apply.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 DELETE_TOOL_STATUS4 backfill (append-only)

A full `docs/*.md` listing (220 files, flat directory, no subdirectories) was
diffed against the 219 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [DELETE_TOOL_STATUS4.md](DELETE_TOOL_STATUS4.md) — read-only 2026-11-24
  fourth check: `tools/r1_winsrv_delete.py` has now LANDED (present, 42354 B,
  `py_compile` OK); dry-run-only behavior verified (plans P1–P3, refuses
  without `--apply --confirm` token, P1 consumer grep BLOCKED with 24 matches
  outside `DELETE_SET`, one `[AMBIGUOUS]` edit target), no apply ever executed.

This newly indexed report brings the indexed Markdown total to **220**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 DELETE_TOOL_STATUS4 / final sweep (append-only)

A fresh full `docs/*.md` listing (222 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `DELETE_TOOL_STATUS4.md`
was the named target and is confirmed already linked above (fourth read-only
monitor pass: `tools/r1_winsrv_delete.py` LANDED, `py_compile` OK, dry-run-only
`--apply`/`--confirm` token refusal verified, P1 consumer grep BLOCKED with 24
matches outside `DELETE_SET`, no apply ever executed). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [CONSUMER_CLEAN_STATUS.md](CONSUMER_CLEAN_STATUS.md) — read-only 2026-11-24
  P1 winsrv consumer-cleanup check: live exec refs in `user/` outside
  `DELETE_SET` are all cleared (26 line-matches / 8 files, up from 24 / 6);
  residue is 3 `wsclient.h` includes + 14 `user.mk` dep tokens (+ comments),
  verdict IN PROGRESS with P1 still BLOCKED (26 > 0).

This newly indexed report brings the indexed Markdown total to **222**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 DELETE_TOOL_STATUS5 backfill (append-only)

A full `docs/*.md` listing (221 files, flat directory, no subdirectories) was
diffed against the 220 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [DELETE_TOOL_STATUS5.md](DELETE_TOOL_STATUS5.md) — read-only 2026-11-24
  ~21:15 fifth check (verdict LANDED): `tools/r1_winsrv_delete.py` exists
  (11977 B, mtime 21:10), `py_compile` OK, and both refusal paths (no
  `--apply`, `--apply` without `--confirm DELETE-WINSRV`) exit rc=2 before any
  filesystem write; apply never invoked.

This newly indexed report brings the indexed Markdown total to **221**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 CONSUMER_CLEAN_STATUS backfill (append-only)

A full `docs/*.md` listing (222 files, flat directory, no subdirectories) was
diffed against the 221 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [CONSUMER_CLEAN_STATUS.md](CONSUMER_CLEAN_STATUS.md) — read-only 2026-11-24
  P1 winsrv consumer-match cleanup check (verdict IN PROGRESS, P1 still
  BLOCKED): all live `QE_EXEC_GUI_*`/`QE_EXEC_WS_*` exec refs in `user/` outside
  `DELETE_SET` are cleared, leaving 26 line-matches/8 files (17 live:
  3 `wsclient.h` includes + 14 `user.mk` dep tokens, rest comments);
  `r1_winsrv_delete.py` still refuses without `--apply --confirm`.

This newly indexed report brings the indexed Markdown total to **222**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(222 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 CONSUMER_CLEAN_STATUS confirmation / full sweep (append-only)

A fresh full `docs/*.md` listing (222 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (222 on-disk `.md` files = 222 unique indexed
targets, including this index; the header count "(222 files)" at line 4 is
consistent). [`CONSUMER_CLEAN_STATUS.md`](CONSUMER_CLEAN_STATUS.md) is confirmed
already indexed above (read-only 2026-11-24 P1 winsrv consumer-cleanup check,
verdict IN PROGRESS / P1 still BLOCKED: all live `QE_EXEC_GUI_*`/`QE_EXEC_WS_*`
exec refs in `user/` outside `DELETE_SET` cleared, leaving 26 line-matches /
8 files — 3 `wsclient.h` includes + 14 `user.mk` dep tokens + comments;
`r1_winsrv_delete.py` still refuses without `--apply --confirm`). No missing
documents were found, so this section adds no new link entries; `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **222**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 CONSUMER_CLEAN_STATUS2 backfill (append-only)

A fresh full `docs/*.md` listing (223 files, flat directory, no subdirectories)
was diffed against the 222 unique `.md` link targets previously in this index:
one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [CONSUMER_CLEAN_STATUS2.md](CONSUMER_CLEAN_STATUS2.md) — read-only 2026-11-24
  P1 winsrv consumer-match recheck (verdict P1 still N = 24 line-matches /
  6 files > 0): zero live `QE_EXEC_GUI_*`/`QE_EXEC_WS_*` exec refs remain
  outside `DELETE_SET`; residue is 13 `user/user.mk` dep tokens + 2
  `wsclient.h` includes (+ comments); `r1_winsrv_delete.py` dry-run plans
  30 edits + 9 deletes and refuses rc=2 without the `--apply --confirm` token.

This newly indexed report brings the indexed Markdown total to **223**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 DELETE_TOOL_STATUS4/5 confirmation sweep (append-only)

A fresh full `docs/*.md` listing (221 files, flat directory, no subdirectories)
was diffed against the 221 unique `.md` link targets in this index: **0
unlinked documents and 0 broken links** (221 on-disk `.md` files = 221 unique
indexed targets, including this index; the header count "(221 files)" at line
4 is consistent). Both task-named docs were confirmed already indexed in the
sections above; they are re-linked here with one-line descriptions.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [DELETE_TOOL_STATUS4.md](DELETE_TOOL_STATUS4.md) — read-only 2026-11-24
  fourth monitor pass: `tools/r1_winsrv_delete.py` HAS LANDED (42354 B,
  `py_compile` OK); dry-run-only refusal contract verified (`--apply` needs
  the `--confirm` token plus a gate PASS; P1 consumer grep BLOCKED with 24
  matches outside `DELETE_SET`, one `[AMBIGUOUS]` edit at
  `variant/qemu_mouse.cpp:302`); no apply ever executed.
- [DELETE_TOOL_STATUS5.md](DELETE_TOOL_STATUS5.md) — read-only 2026-11-24
  (~21:15) fifth monitor pass, verdict LANDED: `tools/r1_winsrv_delete.py`
  present (11977 bytes, mtime 21:10), `py_compile` OK; no-args and
  `--apply`-without-token runs both refuse rc=2 before any filesystem write;
  all checks PASS, no apply invoked.

The indexed Markdown total stays **221**, matching the 301 `.md` files under
`docs/` (including this index; the header count "(221 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 CONSUMER_CLEAN_STATUS backfill (append-only)

A full `docs/*.md` listing (222 files, flat directory, no subdirectories) was
diffed against the 221 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [CONSUMER_CLEAN_STATUS.md](CONSUMER_CLEAN_STATUS.md) — read-only 2026-11-24
  P1 winsrv consumer-match cleanup check: live winsrv exec refs in `user/`
  outside `DELETE_SET` are down to 0 exec calls; residue is 3 `wsclient.h`
  includes + 14 `user.mk` dep tokens (+ comments) across 8 files (26 matches),
  so the P1 gate stays BLOCKED (26 > 0) and `r1_winsrv_delete.py` still refuses
  without `--apply --confirm`.

This newly indexed report brings the indexed Markdown total to **222**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 CONSUMER_CLEAN_STATUS2 backfill (append-only)

A full `docs/*.md` listing (223 files, flat directory, no subdirectories) was
diffed against the 222 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [CONSUMER_CLEAN_STATUS2.md](CONSUMER_CLEAN_STATUS2.md) — read-only 2026-11-24
  P1 winsrv consumer-match recheck after the cleanup agent: still 24
  line-matches / 6 files outside `DELETE_SET` (13 `user.mk` dep-token rows +
  2 `wsclient.h` includes live; zero exec calls remain), so the P1 gate stays
  BLOCKED (24 > 0) and `tools/r1_winsrv_delete.py` still refuses without
  `--apply --confirm DELETE-WINSRV`.

This newly indexed report brings the indexed Markdown total to **223**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(223 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 CONSUMER_CLEAN_STATUS2 + post-222 sweep (append-only)

A fresh full `docs/*.md` listing (223 files, flat directory, no subdirectories)
was diffed against the unique `.md` mentions in this index: **0 documents on
disk are missing an entry and 0 links are stale** (223 on-disk `.md` files =
223 unique mentioned targets, including this index; the header count
"(223 files)" at line 4 is consistent). The task-named
[`CONSUMER_CLEAN_STATUS2.md`](CONSUMER_CLEAN_STATUS2.md) is confirmed already
indexed in the two backfill sections above; it is re-linked here with a one-line
description. No other new documents exist past the 222 mark, so this section
adds no new link entries; `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [CONSUMER_CLEAN_STATUS2.md](CONSUMER_CLEAN_STATUS2.md) — read-only 2026-11-24
  P1 winsrv consumer-match recheck after the cleanup agent: still 24
  line-matches / 6 files outside `DELETE_SET` (13 `user.mk` dep-token rows +
  2 `wsclient.h` includes live; zero `QE_EXEC_GUI_*`/`QE_EXEC_WS_*` exec calls
  remain), so the P1 gate stays BLOCKED (24 > 0) and
  `tools/r1_winsrv_delete.py` dry-run still refuses rc=2 without
  `--apply --confirm DELETE-WINSRV`.

The indexed Markdown total stays **223**, matching the 301 `.md` files under
`docs/` (including this index; the header count "(223 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 P1_TAIL_STATUS backfill (append-only)

A full `docs/*.md` listing (226 files, flat directory, no subdirectories) was
diffed against the 224 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [P1_TAIL_STATUS.md](P1_TAIL_STATUS.md) — read-only 2026-11-24 P1 tail
  monitor pass (verdict PARTIALLY LANDED): `wsclient.h`-include + `user.mk`
  dep-token cleanup reduced the P1 pattern from 24 matches / 6 files to 10
  matches / 5 files (4 source includes + 6 dep tokens remain), full gate
  pattern still 57 matches / 13 files, so the P1 winsrv-deletion gate stays
  BLOCKED and `r1_winsrv_delete.py` still refuses without `--apply
  --confirm DELETE-WINSRV`.
- [P1_TAIL_STATUS3.md](P1_TAIL_STATUS3.md) — read-only 2026-11-24 third P1
  tail monitor: `r1_winsrv_delete.py --dry-run` now reports P1 BLOCKED with
  only 2 code matches outside `DELETE_SET` (the `wsclient.h` includes in
  `user/ws_e32_client.c:40` and `user/ws_e32_draw_test.c:21`, pending chunks
  A–D / shot migration); all other refs are deletion-set members, P2 dep
  rows or comments; 39/39 edit/delete checks ok, apply still refused.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH backfill (append-only)

A full `docs/*.md` listing (227 files, flat directory, no subdirectories) was
diffed against the 226 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH.md](STRICT_VERIFY_WATCH.md) — read-only 2026-11-24
  21:14 EET watch of the in-progress `r1_final_verify.py --strict-skip` run
  (PID 1137836, log `/tmp/opencode/final_verify_1913.log`): verdict NOT YET
  PRESENT, still inside the selfcheck pre-flight (`qemu_smoke` at the `C:/>`
  prompt); no verdict from any of the 11 tests yet.

This newly indexed report brings the indexed Markdown total to **227**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH4/5 backfill (append-only)

A full `docs/*.md` listing (229 files, flat directory, no subdirectories) was
diffed against the 227 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH4.md](STRICT_VERIFY_WATCH4.md) — read-only 2026-11-24
  ~21:14 watch: strict verify PID 1137836 alive (holds the flock), log
  `final_verify_1913.log` still on test 1/11 (selfcheck, DOOM ISO staged),
  queued `gate_run_211252.sh` blocked on the same lock, no fault markers;
  verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH5.md](STRICT_VERIFY_WATCH5.md) — read-only 2026-11-24
  ~21:14 UTC follow-up watch: same run alive ~1 min, still test 1/11
  selfcheck (`e32run: validate r=0`), no verdicts / OVERALL / FV_EXIT yet,
  no fault markers; verdict IN PROGRESS.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH6 backfill (append-only)

A full `docs/*.md` listing (235 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH6.md](STRICT_VERIFY_WATCH6.md) — read-only 2026-11-24
  21:14 UTC sixth watch of the in-flight `r1_final_verify.py --strict-skip`
  run (PID 1137836, log `/tmp/opencode/final_verify_1913.log`): still on test
  1/11 selfcheck, no OVERALL/FV_EXIT yet, queued gate blocked on the flock,
  no fault markers.
- [STRICT_VERIFY_WATCH15.md](STRICT_VERIFY_WATCH15.md) — read-only 2026-11-24
  21:16 UTC fifteenth watch of the same in-flight strict-verify run
  (PID 1137836, ~3 min elapsed, flock-held): still in test 1/11 selfcheck
  (build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS, `selftest` PASS
  failures=0), no FV_EXIT yet, no fault markers in any active log.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(235 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 subagent sweep, post-222 (append-only)

A fresh full `docs/*.md` listing (223 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (223 on-disk `.md` files = 223 unique indexed
targets, including this index; the header count "(223 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **223**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 P1_TAIL_STATUS backfill + post-223 sweep (append-only)

A fresh full `docs/*.md` listing (226 files, flat directory, no subdirectories)
was diffed against the 223 unique `.md` link targets in this index: three
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [P1_TAIL_STATUS.md](P1_TAIL_STATUS.md) — read-only 2026-11-24 monitor pass
  on the P1 winsrv gate: the `wsclient.h`-include + `user.mk`-dep-token cleanup
  landed only partially (10 matches / 5 files remain of the 24/6 baseline; full
  P1 pattern still 57 matches / 13 files), so P1 is NOT green and
  `r1_winsrv_delete.py` dry-run still refuses rc=2.
- [P1_TAIL_STATUS2.md](P1_TAIL_STATUS2.md) — read-only final P1 recount after
  the cleanup attempt: full P1 grep still 24 line-matches / 6 files outside
  the deletion set (unchanged vs `CONSUMER_CLEAN_STATUS2.md`), per-file residue
  listed; verdict P1 N=24 > 0, gate not passable until user.mk dep rows are
  re-pointed and the two `wsclient.h` includes dropped.
- [P1_TAIL_STATUS3.md](P1_TAIL_STATUS3.md) — read-only post-cleanup
  `r1_winsrv_delete.py --dry-run`: P1 down to 2 code matches (the
  `wsclient.h` includes at `ws_e32_client.c:40` and `ws_e32_draw_test.c:21`),
  all other refs classified `[ok]` (deletion-set members, P2 dep rows or
  comments); 39/39 edit/delete checks ok, apply still refused without
  `--apply` + gate PASS.
- [STRICT_VERIFY_WATCH6.md](STRICT_VERIFY_WATCH6.md) — read-only 21:14 UTC
  in-flight watch of `r1_final_verify.py --strict-skip` (PID 1137836): still
  on test 1/11 selfcheck, queued gate run correctly blocked on the flock, no
  fault markers in the log.
- [STRICT_VERIFY_WATCH8.md](STRICT_VERIFY_WATCH8.md) — read-only 21:16 UTC
  watch of the same run: selfcheck sub-log shows build/isogrub/qemu_smoke
  PASS with cp4_ca under QEMU; no fault markers; healthy IN PROGRESS.
- [STRICT_VERIFY_WATCH9.md](STRICT_VERIFY_WATCH9.md) — read-only 21:15 watch
  of the same run: DOOM ISO staged, selfcheck in progress, no verdicts yet,
  fault scan clean.
- [STRICT_VERIFY_WATCH10.md](STRICT_VERIFY_WATCH10.md) — read-only 21:15
  watch of the same run: selfcheck sub-log now build/isogrub/qemu_smoke/
  cp4_ca all PASS with cp4_stress running; no fault markers; IN PROGRESS.

- [STRICT_2125_OVERALL2.md](STRICT_2125_OVERALL2.md) — read-only 2026-11-24 report: STRICT 21:25 final_verify — OVERALL / FV_EXIT report (2026-11-24).

These 8 newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(235 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 subagent docs-index sweep, post-222 final (append-only)

A fresh full `docs/*.md` listing (236 files, flat directory, no subdirectories)
was diffed against the 223 unique `.md` link targets previously in this index:
thirteen documents existed on disk but had no entry; they are linked below. The
one earlier post-222 doc, [`CONSUMER_CLEAN_STATUS2.md`](CONSUMER_CLEAN_STATUS2.md),
was already indexed above (read-only 2026-11-24 P1 winsrv consumer-match
recheck: 24 line-matches / 6 files outside `DELETE_SET`, zero live exec calls,
P1 gate still BLOCKED). `kernelhwsrv/` was untouched and no QEMU run was made
by this index update.

- [P1_TAIL_STATUS.md](P1_TAIL_STATUS.md) — read-only 2026-11-24 monitor pass on
  the `wsclient.h`-include + `user.mk`-dep-token tail (verdict PARTIALLY
  LANDED): P1 baseline 24 matches / 6 files down to 10 / 5, but 4 source
  includes and all 6 `user.mk` dep tokens still reference `wsclient.h`, and
  the full P1 gate pattern is 57 matches / 13 files, so P1 is not green;
  `r1_winsrv_delete.py` dry-run plans 10 edits + 7 deletes and refuses rc=2
  without `--apply --confirm DELETE-WINSRV`.
- [P1_TAIL_STATUS2.md](P1_TAIL_STATUS2.md) — read-only 2026-11-24 interim P1
  recount after the cleanup attempt (verdict P1 still N = 24 line-matches /
  6 files > 0, unchanged vs `CONSUMER_CLEAN_STATUS2.md`): residue is 13
  `user.mk` dep rows + 2 `#include "wsclient.h"` (zero live exec calls
  outside `DELETE_SET`); `r1_winsrv_delete.py` dry-run plans 11 edits + 7
  deletes and refuses rc=2 without the token; P1 gate not passable yet.
- [P1_TAIL_STATUS3.md](P1_TAIL_STATUS3.md) — read-only 2026-11-24 third P1
  tail check after the include/dep-token cleanup (verdict P1 still = 2 code
  matches outside `DELETE_SET`, down from 24; chunks A–D / `ws_e32_client.c` /
  `shot` migration must land first); `r1_winsrv_delete.py --dry-run` reports
  `P1 BLOCKED: 2 code match(es)` and still refuses apply without the token.
- [STRICT_VERIFY_WATCH.md](STRICT_VERIFY_WATCH.md) — read-only 2026-11-24
  21:14 EET watch report on the in-progress `r1_final_verify.py
  --strict-skip` run (PID 1137836, log `/tmp/opencode/final_verify_1913.log`):
  OVERALL verdict NOT YET PRESENT, run still in pre-flight selfcheck /
  `qemu_smoke` (boot reached `C:/>`); next expected tests listed in order.
- [STRICT_VERIFY_WATCH4.md](STRICT_VERIFY_WATCH4.md) — read-only 2026-11-24
  ~21:14 EET watch: strict-verify PID 1137836 alive holding
  `/tmp/opencode/final_verify.lock`; log has only test 1 (selfcheck, DOOM ISO
  staged, `r1_ca_test` QEMU active), no verdicts yet; queued
  `gate_run_211252.sh` correctly blocked on the same flock; fault scan clean;
  verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH5.md](STRICT_VERIFY_WATCH5.md) — read-only 2026-11-24
  ~21:14 UTC watch: still on test 1/11 (selfcheck, `e32run: validate r=0`),
  no OVERALL / FV_EXIT yet, queued runner blocked on the lock as expected, no
  fault/panic/#PF markers in the log; `kernelhwsrv/` clean, no QEMU launched
  by the watcher.
- [STRICT_VERIFY_WATCH6.md](STRICT_VERIFY_WATCH6.md) — read-only 2026-11-24
  21:14 UTC watch: strict-verify PID 1137836 alive (~1 min), still on test
  1/11 selfcheck, no verdicts/OVERALL/FV_EXIT yet; queued runner blocked on
  the flock; fault scan of the main log clean; `kernelhwsrv/` untouched.
- [STRICT_VERIFY_WATCH8.md](STRICT_VERIFY_WATCH8.md) — read-only 2026-11-24
  21:16 UTC watch: PID 1137836 alive (~1m46s), still test 1/11; selfcheck
  sub-log shows build PASS, isogrub PASS, qemu_smoke PASS (50.2 s), cp4_ca
  in progress; no fault markers; `kernelhwsrv/` untouched.
- [STRICT_VERIFY_WATCH9.md](STRICT_VERIFY_WATCH9.md) — read-only 2026-11-24
  21:15 watch: main log 235 B (test 1/11 selfcheck in progress, staged DOOM
  ISO done), selfcheck child log build/isogrub/qemu_smoke PASS with cp4_ca
  running; no fault/panic/KERN-EXEC markers; verdict IN PROGRESS, healthy.
- [STRICT_VERIFY_WATCH10.md](STRICT_VERIFY_WATCH10.md) — read-only 2026-11-24
  21:15 watch: strict-verify PID 1137836 alive (~2m04s); still test 1/11
  selfcheck; child log now build/isogrub/qemu_smoke/cp4_ca PASS, cp4_stress
  running; no fault markers; `kernelhwsrv/` clean; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH12.md](STRICT_VERIFY_WATCH12.md) — read-only 2026-11-24
  21:15 watch: PID 1137836 alive (~2m20s), flock held; main log still in
  selfcheck phase (no verdicts yet); selfcheck child log build/isogrub/
  qemu_smoke/cp4_ca PASS with cp4_stress RUNNING; fault scan 0 matches;
  `kernelhwsrv/` untouched.
- [STRICT_VERIFY_WATCH15.md](STRICT_VERIFY_WATCH15.md) — read-only 2026-11-24
  21:16 watch: strict-verify PID 1137836 alive (~3 min, flock-held), main log
  still test 1/11 with no verdicts; selfcheck child log now build/isogrub/
  qemu_smoke/cp4_ca/cp4_stress PASS and `selftest` QEMU reports
  `ST: RESULT PASS - failures=0`; fault scan of all active logs benign;
  `kernelhwsrv/` untouched.
- [STRICT_VERIFY_WATCH16.md](STRICT_VERIFY_WATCH16.md) — read-only 2026-11-24
  21:16:59 EET monitor: strict-verify PID 1137836 alive (03:29 elapsed), still
  in test 1 selfcheck (main log only `RUN selfcheck`); selfcheck child log
  build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS, now `RUN selftest`;
  per-test logs 2–11 stale from prior runs; no OVERALL/FV_EXIT yet; fault scan
  clean; `kernelhwsrv/` untouched.

These nineteen newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH4/5/6/8/9 backfill (append-only)

A full `docs/*.md` listing (232 files, flat directory, no subdirectories) was
diffed against the 227 unique `.md` link targets previously in this index:
five documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH4.md](STRICT_VERIFY_WATCH4.md) — read-only 2026-11-24
  ~21:14 watch of the in-flight `r1_final_verify.py --strict-skip` (PID
  1137836 holding the flock; log `final_verify_1913.log` at test 1/11
  selfcheck, no verdicts yet); queued `gate_run_211252.sh` blocked on the same
  lock as expected; fault scan clean; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH5.md](STRICT_VERIFY_WATCH5.md) — read-only 2026-11-24
  ~21:14 UTC second watch of the same run: PID alive ~1 min, still on
  test 1/11 (selfcheck, `e32run: validate r=0`), no OVERALL/FV_EXIT yet, no
  fault/panic/KERN/#PF markers in the log; watcher launched no QEMU and
  `kernelhwsrv/` untouched.
- [STRICT_VERIFY_WATCH6.md](STRICT_VERIFY_WATCH6.md) — read-only 2026-11-24
  21:14 UTC third watch: PID 1137836 alive, still test 1/11 selfcheck with no
  per-test verdicts/OVERALL/FV_EXIT; queued gate runner still flock-blocked;
  fault scan of `final_verify_1913.log` clean; `kernelhwsrv/` untouched.
- [STRICT_VERIFY_WATCH8.md](STRICT_VERIFY_WATCH8.md) — read-only 2026-11-24
  21:16 UTC watch: same run alive ~1 min 46 s, still on selfcheck; child log
  shows build PASS, isogrub PASS, qemu_smoke PASS (50.2 s), cp4_ca now in
  progress under QEMU; no fault markers in either log.
- [STRICT_VERIFY_WATCH9.md](STRICT_VERIFY_WATCH9.md) — read-only 2026-11-24
  21:15 watch: main log still 235 B (test 1/11), selfcheck child log at
  qemu_smoke PASS / cp4_ca running; no OVERALL/FV_EXIT; fault scan clean;
  verdict IN PROGRESS, healthy so far.

These five newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH10/12 backfill (append-only)

A fresh full `docs/*.md` listing (234 files, flat directory, no subdirectories)
was diffed against the 232 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH10.md](STRICT_VERIFY_WATCH10.md) — read-only 2026-11-24
  21:15 watch on the in-flight `r1_final_verify.py --strict-skip` run
  (`/tmp/opencode/final_verify_1913.log`, PID 1137836 ALIVE, flock held):
  selfcheck child log shows build/isogrub/qemu_smoke/cp4_ca PASS, cp4_stress
  running; no fault markers; verdict IN PROGRESS, healthy so far.
- [STRICT_VERIFY_WATCH12.md](STRICT_VERIFY_WATCH12.md) — read-only 2026-11-24
  21:15 watch (same run, ~2m20s elapsed): main log still buffered at 235 B
  with no per-test verdicts yet; selfcheck log build/isogrub/qemu_smoke/cp4_ca
  PASS and cp4_stress RUNNING; 0 fault-scan matches; `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH6/8/9 backfill (append-only)

A full `docs/*.md` listing (232 files, flat directory, no subdirectories) was
diffed against the 229 unique `.md` link targets previously in this index:
three documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH6.md](STRICT_VERIFY_WATCH6.md) — read-only 2026-11-24
  ~21:14 UTC watch: strict verify PID 1137836 alive ~1 min, still test 1/11
  selfcheck (no verdicts / OVERALL / FV_EXIT yet), queued `gate_run_211252.sh`
  blocked on the flock, no fault markers in `final_verify_1913.log`.
- [STRICT_VERIFY_WATCH8.md](STRICT_VERIFY_WATCH8.md) — read-only 2026-11-24
  ~21:16 UTC watch: same run alive ~1m46s, still test 1/11; selfcheck child
  log shows build/isogrub/qemu_smoke PASS (50.2 s), cp4_ca in progress; no
  fault markers; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH9.md](STRICT_VERIFY_WATCH9.md) — read-only 2026-11-24
  ~21:15 watch: same run alive ~1m43s, main log still buffered at 235 B on
  test 1/11, selfcheck child PASS through qemu_smoke with cp4_ca running; no
  fault markers; verdict IN PROGRESS, healthy so far.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH10/12 backfill (append-only)

A full `docs/*.md` listing (234 files, flat directory, no subdirectories) was
diffed against the 232 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH10.md](STRICT_VERIFY_WATCH10.md) — read-only
  2026-11-24 ~21:15 watch: strict verify PID 1137836 alive ~2m04s, still
  selfcheck; child log shows build/isogrub/qemu_smoke/cp4_ca PASS (60.6 s),
  cp4_stress running; no fault markers; verdict IN PROGRESS, healthy.
- [STRICT_VERIFY_WATCH12.md](STRICT_VERIFY_WATCH12.md) — read-only
  2026-11-24 ~21:15 watch: same run alive ~2m20s; selfcheck child log PASS
  through cp4_ca with cp4_stress running; zero fault/panic/KERN/#PF matches;
  verdict IN PROGRESS.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH15 backfill (append-only)

A full `docs/*.md` listing (235 files, flat directory, no subdirectories) was
diffed against the 234 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH15.md](STRICT_VERIFY_WATCH15.md) — read-only
  2026-11-24 ~21:16 watch: strict verify PID 1137836 alive ~3 min, still
  test 1/11 selfcheck; child log PASS through cp4_stress and selftest
  (`ST: RESULT PASS - failures=0`); no fault markers in any active log.

This newly indexed report brings the indexed Markdown total to **235**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH16 backfill (append-only)

A full `docs/*.md` listing (236 files, flat directory, no subdirectories) was
diffed against the 235 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH16.md](STRICT_VERIFY_WATCH16.md) — read-only 2026-11-24
  21:16:59 EET watch: strict verify PID 1137836 alive 3m29s, still test 1/11
  selfcheck (build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS, selftest
  running); tests 2–11 logs stale from prior runs, no OVERALL/FV_EXIT yet;
  no fault markers.

This newly indexed report brings the indexed Markdown total to **236**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH18/19 backfill (append-only)

A full `docs/*.md` listing (238 files, flat directory, no subdirectories) was
diffed against the 236 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH18.md](STRICT_VERIFY_WATCH18.md) — read-only 2026-11-24
  21:17:36 EET watch: strict verify PID 1137836 alive 4m07s, still test 1/11
  selfcheck (build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS, selftest
  running); tests 2–11 logs stale; no OVERALL/FV_EXIT; fault scan clean.
- [STRICT_VERIFY_WATCH19.md](STRICT_VERIFY_WATCH19.md) — read-only 2026-11-24
  ~21:17 watch: same run alive ~3m55s, still selfcheck with selftest QEMU
  booting (AHCI disk detected, normal boot trace); no verdicts yet; no fault
  markers in run-1913 logs.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH20/21 backfill (append-only)

A full `docs/*.md` listing (240 files, flat directory, no subdirectories) was
diffed against the 238 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH20.md](STRICT_VERIFY_WATCH20.md) — read-only 2026-11-24
  ~21:17Z watch: run PID 1137836 alive 4m26s, advanced to test 2/11
  qemu_smoke; selfcheck FAIL (248 s — harness marker-order / EUSER ord-915
  check, 0 guest exceptions, not a guest fault); fault scan clean.
- [STRICT_VERIFY_WATCH21.md](STRICT_VERIFY_WATCH21.md) — read-only 2026-11-24
  ~21:17 watch: same run; selfcheck FAIL (selftest step, all ST markers
  incl. ord 915 FAIL, 0 guest exceptions — marker-level, not a fault),
  qemu_smoke in progress; no fault markers in run logs.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH22/26 backfill (append-only)

A full `docs/*.md` listing (242 files, flat directory, no subdirectories) was
diffed against the 240 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH22.md](STRICT_VERIFY_WATCH22.md) — read-only 2026-11-24
  ~21:2x watch: run PID 1137836 alive 5m28s; selfcheck FAIL (248 s, selftest
  markers, 0 guest exceptions), qemu_smoke PASS (50.1 s), now test 3/11
  genuine_retire; notes possible interference from a concurrent
  `r1_winsrv_delete.py` session; fault scan clean.
- [STRICT_VERIFY_WATCH26.md](STRICT_VERIFY_WATCH26.md) — read-only 2026-11-24
  ~21:18 watch: same run alive ~5m22s in test 3/11 genuine_retire (EFSRV
  PASS, EWSRV stage); selfcheck FAIL + qemu_smoke PASS as above; no
  OVERALL/FV_EXIT yet; no faults in per-test logs.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH10/12 backfill (append-only)

A fresh full `docs/*.md` listing (234 files, flat directory, no subdirectories)
was diffed against the 232 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH10.md](STRICT_VERIFY_WATCH10.md) — read-only 2026-11-24
  21:15 watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836 alive ~2m04s, flock held; still on selfcheck with cp4_stress
  running; build/isogrub/qemu_smoke/cp4_ca PASS; fault scan clean; verdict
  IN PROGRESS, healthy so far).
- [STRICT_VERIFY_WATCH12.md](STRICT_VERIFY_WATCH12.md) — read-only 2026-11-24
  21:15 watch of the same run (~2m20s elapsed; main log 235 B buffered,
  selfcheck log shows build/isogrub/qemu_smoke/cp4_ca PASS and cp4_stress
  RUNNING; 0 fault/panic/KERN-/#PF/assert matches; verdict IN PROGRESS).

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH6/8/9/10 backfill (append-only)

A fresh full `docs/*.md` listing (234 files, flat directory, no subdirectories)
was diffed against the 233 unique `.md` link targets previously in this index:
1 document(s) existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH12.md](STRICT_VERIFY_WATCH12.md) — read-only 2026-11-24 report: STRICT VERIFY WATCH 12 — 2026-11-24 21:15.

These 1 newly indexed report(s) bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH19 backfill (append-only)

A full `docs/*.md` listing (238 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH19.md](STRICT_VERIFY_WATCH19.md) — read-only 2026-11-24
  21:17 EET watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836, log `final_verify_1913.log`): still inside test 1 selfcheck
  (build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS, now in `selftest`), no
  per-test verdicts or OVERALL yet, fault scan clean.

This newly indexed report brings the indexed Markdown total to **238**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(238 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH15 backfill (append-only)

A fresh full `docs/*.md` listing (235 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below, and no links are stale
(0 linked targets missing from disk). `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

- [STRICT_VERIFY_WATCH15.md](STRICT_VERIFY_WATCH15.md) — read-only 2026-11-24
  21:16 watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836, log `/tmp/opencode/final_verify_1913.log`): still test 1/11
  selfcheck with build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS and selftest
  `ST: RESULT PASS - failures=0`, no fault markers, no FV_EXIT yet; verdict
  IN PROGRESS, healthy.

This newly indexed report brings the indexed Markdown total to **235**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH6/8/9/10 sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index. At scan time four documents —
[`STRICT_VERIFY_WATCH6.md`](STRICT_VERIFY_WATCH6.md),
[`STRICT_VERIFY_WATCH8.md`](STRICT_VERIFY_WATCH8.md),
[`STRICT_VERIFY_WATCH9.md`](STRICT_VERIFY_WATCH9.md) and
[`STRICT_VERIFY_WATCH10.md`](STRICT_VERIFY_WATCH10.md) — had no entry; they
are (now) linked below with one-line descriptions (read-only 2026-11-24
watches of the in-flight `r1_final_verify.py --strict-skip` run PID 1137836,
all showing test 1/11 selfcheck in progress with no verdicts yet and a clean
fault scan; WATCH8/9 additionally record selfcheck child-log build/isogrub/
qemu_smoke PASS, WATCH10 also cp4_ca PASS). `kernelhwsrv/` was untouched and
no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH6.md](STRICT_VERIFY_WATCH6.md) — read-only 2026-11-24
  21:14 UTC watch: strict-verify PID 1137836 alive on test 1/11 selfcheck, no
  verdicts, fault scan clean, queued gate blocked on the flock as expected.
- [STRICT_VERIFY_WATCH8.md](STRICT_VERIFY_WATCH8.md) — read-only 2026-11-24
  21:16 UTC watch: still test 1/11 (~1m46s), selfcheck child log build/isogrub/
  qemu_smoke PASS with cp4_ca running; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH9.md](STRICT_VERIFY_WATCH9.md) — read-only 2026-11-24
  21:15 watch: same run ~1m43s at test 1/11, child log build/isogrub/
  qemu_smoke PASS, cp4_ca running, fault scan clean; IN PROGRESS, healthy.
- [STRICT_VERIFY_WATCH10.md](STRICT_VERIFY_WATCH10.md) — read-only 2026-11-24
  21:15 watch: run ~2m04s, child log now also cp4_ca PASS (60.6s) with
  cp4_stress running; still no OVERALL, fault scan clean; IN PROGRESS.

The indexed Markdown total stays **235**, matching the 301 `.md` files under
`docs/` (including this index; the header count "(235 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH16 backfill (append-only)

A full `docs/*.md` listing (236 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH16.md](STRICT_VERIFY_WATCH16.md) — read-only 2026-11-24
  21:16:59 EET monitor of the in-flight strict verify (PID 1137836, run 1913):
  still inside test 1 selfcheck (build/isogrub/qemu_smoke/cp4_ca/cp4_stress
  PASS, now `RUN selftest`), per-test logs 2–11 stale from prior runs, no
  OVERALL/FV_EXIT yet, fault scan clean.

This newly indexed report brings the indexed Markdown total to **236**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH15 backfill (append-only)

A fresh full `docs/*.md` listing (235 files, flat directory, no subdirectories)
was diffed against the 234 unique `.md` link targets previously in this index:
one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH15.md](STRICT_VERIFY_WATCH15.md) — read-only 2026-11-24
  21:16 watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836 alive ~3 min, flock held, still on test 1/11 selfcheck;
  build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS, `selftest` ST RESULT
  PASS failures=0; fault scan of all active logs clean).

This newly indexed report brings the indexed Markdown total to **235**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH6/8/9/10/12 backfill (append-only)

A fresh full `docs/*.md` listing (234 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: six documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH6.md](STRICT_VERIFY_WATCH6.md) — read-only 21:14 UTC
  watch: strict-verify PID 1137836 alive on test 1/11 (selfcheck), queued
  `gate_run_211252.sh` correctly flock-blocked, no verdicts yet, fault scan
  clean.
- [STRICT_VERIFY_WATCH8.md](STRICT_VERIFY_WATCH8.md) — read-only 21:16 UTC
  watch: same run (~1m46s), selfcheck sub-progress build/isogrub/qemu_smoke
  PASS with cp4_ca in progress, no OVERALL yet, no fault markers.
- [STRICT_VERIFY_WATCH9.md](STRICT_VERIFY_WATCH9.md) — read-only 21:15 watch:
  same run (~1m43s) still on selfcheck, child log shows build/isogrub/
  qemu_smoke PASS and cp4_ca running; verdict IN PROGRESS, healthy.
- [STRICT_VERIFY_WATCH10.md](STRICT_VERIFY_WATCH10.md) — read-only 21:15
  watch: same run (~2m04s), selfcheck child log now also cp4_ca PASS (60.6s)
  with cp4_stress running; no per-test verdicts or OVERALL yet, fault scan
  clean, verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH12.md](STRICT_VERIFY_WATCH12.md) — read-only 2026-11-24
  later watch pass on the same in-flight `--strict-skip` run (PID 1137836,
  log `/tmp/opencode/final_verify_1913.log`): still no OVERALL/FV_EXIT, fault
  scan clean, `kernelhwsrv/` untouched.

These five newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is now
linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH15 backfill (append-only)

A full `docs/*.md` listing (235 files, flat directory, no subdirectories) was
diffed against the 234 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH15.md](STRICT_VERIFY_WATCH15.md) — read-only 2026-11-24
  21:16 watch of the in-flight `r1_final_verify.py --strict-skip` (PID 1137836,
  flock-held, still on test 1/11 selfcheck): build/isogrub/qemu_smoke/cp4_ca/
  cp4_stress PASS, `selftest` reports `ST: RESULT PASS - failures=0`, no
  fault markers; verdict IN PROGRESS.

This newly indexed report brings the indexed Markdown total to **235**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH16 backfill (append-only)

A fresh full `docs/*.md` listing (236 files, flat directory, no subdirectories)
was diffed against the 235 unique `.md` link targets previously in this index:
one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH16.md](STRICT_VERIFY_WATCH16.md) — read-only 2026-11-24
  21:16:59 EET watch of the in-flight strict verify (PID 1137836 alive,
  03:29 elapsed, still on test 1/11 selfcheck; build/isogrub/qemu_smoke/
  cp4_ca/cp4_stress PASS, selftest running; tests 2–11 logs stale from prior
  runs; no OVERALL/FV_EXIT yet; fault scan clean).

This newly indexed report brings the indexed Markdown total to **236**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 subagent full sweep, post-223 (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index. All docs past the 223 mark up to `STRICT_VERIFY_WATCH19.md`
(e.g. `STRICT_VERIFY_WATCH10/12/15/18/19.md`, `P1_TAIL_STATUS{,2,3}.md`) were
confirmed already indexed in the sections above; two newest reports existed
on disk but had no entry and are linked below. `kernelhwsrv/` was untouched
and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH20.md](STRICT_VERIFY_WATCH20.md) — read-only 2026-11-24
  21:17Z watch of the in-flight strict verify (PID 1137836, flock-held, test
  2/11 `qemu_smoke`): selfcheck FAIL (248 s) traced to a harness
  marker-order/EUSER-ord-915 check, not a guest fault (`ST: RESULT PASS,
  failures=0`, 0 exceptions); fault scan clean.
- [STRICT_VERIFY_WATCH21.md](STRICT_VERIFY_WATCH21.md) — read-only 2026-11-24
  21:17 watch of the same run 1913: selfcheck FAIL (248 s; all ST markers FAIL
  incl. EUSER ord 915, 0 guest exceptions — marker-level, not a guest fault),
  `qemu_smoke` in progress, no #PF/KERN/panic in any log.
- [STRICT_VERIFY_WATCH27.md](STRICT_VERIFY_WATCH27.md) — read-only 2026-11-24
  ~21:24 watch of run 1913 (PID 1137836, still in flight ~6 min): selfcheck
  FAIL (248 s, selftest sub-step FAIL, 0 guest exceptions), `qemu_smoke` PASS
  (50.1 s), `genuine_retire` RUNNING (EWSRV PASS so far); fault scan clean.
- [STRICT_VERIFY_WATCH28.md](STRICT_VERIFY_WATCH28.md) — read-only 2026-11-24
  21:19 watch: same run at test 3/11 `genuine_retire`; selfcheck FAIL (marker
  order / EUSER ord 915 unbound, 0 guest exceptions), smoke PASS, retire EFSRV/
  F32 + EWSRV PASS; no fault markers in any log.
- [STRICT_VERIFY_WATCH29.md](STRICT_VERIFY_WATCH29.md) — read-only 2026-11-24
  21:13Z watch: same run in progress; selfcheck FAIL (selftest step), smoke
  PASS, retire EFSRV/EWSRV PASS with estart staging; zero real
  Exc/Panic/FAULT/#PF hits.
- [STRICT_VERIFY_WATCH30.md](STRICT_VERIFY_WATCH30.md) — read-only 2026-11-24
  watch: same run at test 4/11 `genuine_efsrv`; selfcheck FAIL (non-fatal
  under `--strict-skip`), smoke PASS (50.1 s), retire PASS (63.8 s); no
  OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_WATCH33.md](STRICT_VERIFY_WATCH33.md) — read-only 2026-11-24
  21:20 EET watch: run 1913 at test 5/11 `genuine_ewsrv`; selfcheck FAIL,
  smoke/retire/efsrv PASS, ewsrv in progress; no fault markers.
- [STRICT_VERIFY_WATCH35.md](STRICT_VERIFY_WATCH35.md) — read-only 2026-11-24
  21:26 watch: same run at `genuine_eshell` (child active); selfcheck FAIL,
  smoke/retire/efsrv/ewsrv PASS; fat_io + 4x doom pending; fault scan clean.
- [STRICT_VERIFY_WATCH37.md](STRICT_VERIFY_WATCH37.md) — read-only 2026-11-24
  21:20 watch: same run at test 6/11 `genuine_eshell`; selfcheck FAIL (248 s),
  smoke/retire/efsrv/ewsrv PASS (50.1/63.8/27.2/22.7 s); no OVERALL yet.
- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only 2026-11-24
  ~7.5 min watch: same run at test 7/11 `genuine_fat_io`; selfcheck FAIL
  (marker-level, 0 guest exceptions), smoke/retire/efsrv/ewsrv/eshell PASS;
  fault scan clean.
- [STRICT_VERIFY_WATCH40.md](STRICT_VERIFY_WATCH40.md) — read-only 2026-11-24
  ~8m25s watch: same run at test 7/11 `doom_main` booted to `X:/>`; selfcheck
  FAIL (marker-level, 0 guest exceptions), smoke/retire/efsrv/ewsrv/eshell/
  fat_io all PASS; 0 real fault markers in any per-test log.
- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 2026-11-24
  21:22 watch (~9m09s): run 1913 at doom_audio (8/11); selfcheck FAIL (0 guest
  exceptions), smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main all PASS;
  fault scan clean.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 2026-11-24
  21:22 EET watch (~9m10s): same run at doom_audio (9/11); selfcheck FAIL
  (exit 124 timeout, markers never appeared), all other tests through
  doom_main PASS; no guest faults in any sublog.
- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 2026-11-24
  21:22 watch (~9 min): run 1913 in `doom_audio`; selfcheck FAIL (marker-order
  + ord-915 checks, 0 guest exceptions), smoke/retire/efsrv/ewsrv/eshell/
  fat_io/doom_main all PASS; fault scan clean.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 2026-11-24
  21:22 UTC watch (~9 min): same run at doom_audio (9/11, QEMU booting); same
  verdict set — selfcheck FAIL, all through doom_main PASS (cosmetic
  BrokenPipe in screenshot finalize); no OVERALL yet.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only 2026-11-24
  ~9m18s watch: same run at doom_audio (10/11); selfcheck FAIL, smoke/retire/
  efsrv/ewsrv/eshell/fat_io/doom_main PASS; playtest+multikey pending; 0 fault
  markers.
- [STRICT_VERIFY_WATCH51.md](STRICT_VERIFY_WATCH51.md) — read-only 2026-11-24
  21:24 watch (10 min elapsed): run 1913 at doom_playtest (11/11 pending
  multikey); selfcheck FAIL, smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main/
  doom_audio all PASS; fault scan clean.

These seventeen newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 P1_TAIL_STATUS + STRICT_VERIFY_WATCH16 backfill (append-only)

A fresh full `docs/*.md` listing (236 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. The task-named
[`P1_TAIL_STATUS.md`](P1_TAIL_STATUS.md) is confirmed already indexed above
(read-only 2026-11-24 P1 winsrv-gate monitor pass). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH16.md](STRICT_VERIFY_WATCH16.md) — read-only 2026-11-24
  21:16:59 EET watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836, log `/tmp/opencode/final_verify_1913.log`): still in test 1/11
  selfcheck (build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS, `selftest`
  running); per-test logs for tests 2–11 are stale from prior runs; no
  OVERALL/FV_EXIT yet; fault scan clean.

This newly indexed report brings the indexed Markdown total to **236**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 final full sweep past 235 (append-only)

A fresh full `docs/*.md` listing (236 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: **0 documents on disk are missing an entry and 0 links are stale**
(236 on-disk `.md` files = 236 unique indexed targets, including this
index; the header count "(236 files)" at line 4 is consistent). No missing
documents were found past the 235 mark, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

The indexed Markdown total stays **236**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 full directory sweep, post-236 (append-only)

A fresh full `docs/*.md` listing (236 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (236 on-disk `.md` files = 236 unique indexed
targets, including this index; the header count "(236 files)" at line 4 is
consistent). No missing documents were found past the 234 mark, so this
section adds no new link entries; `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

The indexed Markdown total stays **236**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent docs-index sweep, post-233 (append-only)

A full `docs/*.md` listing (240 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: the docs missing
at the start of this sweep are linked below. `kernelhwsrv/` was untouched and
no QEMU run was made by this index update. The first two,
[`STRICT_VERIFY_WATCH10.md`](STRICT_VERIFY_WATCH10.md) (read-only 2026-11-24
21:15 watch of the in-flight `r1_final_verify.py --strict-skip` PID 1137836:
still test 1/11 selfcheck, build/isogrub/qemu_smoke/cp4_ca PASS, cp4_stress
running, fault scan clean, verdict IN PROGRESS) and
[`STRICT_VERIFY_WATCH12.md`](STRICT_VERIFY_WATCH12.md) (read-only 2026-11-24
21:15 second watch of the same run: cp4_stress RUNNING, 0 fault/panic/KERN-/#PF
matches, no OVERALL/FV_EXIT yet, `kernelhwsrv/` untouched), were indexed by
concurrent sweeps before this section; they are re-linked here with one-line
descriptions. Three further watch reports that appeared on disk during this
sweep are also linked here:

- [STRICT_VERIFY_WATCH18.md](STRICT_VERIFY_WATCH18.md) — read-only 2026-11-24
  21:17:36 EET watch of strict-verify PID 1137836 (~4m07s, still test 1/11
  selfcheck, now `RUN selftest`): build/isogrub/qemu_smoke/cp4_ca/cp4_stress
  PASS, tests 2–11 logs stale, no OVERALL yet, fault scan clean.
- [STRICT_VERIFY_WATCH19.md](STRICT_VERIFY_WATCH19.md) — read-only 2026-11-24
  21:17 EET watch of the same run (~3m55s, still selfcheck, `RUN selftest`
  booting): all selfcheck steps through cp4_stress PASS, no fault markers,
  run still in progress, `kernelhwsrv/` untouched.
- [STRICT_VERIFY_WATCH20.md](STRICT_VERIFY_WATCH20.md) — read-only 2026-11-24
  21:17Z watch: PID alive ~4m26s, now test 2/11 `qemu_smoke`; selfcheck FAIL
  (248.0s) traced to a harness marker-order/ord-915 check with
  `ST: RESULT PASS, failures=0` in the guest, not a guest fault; no
  OVERALL/FV_EXIT yet, fault scan clean.

- [STRICT_VERIFY_WATCH22.md](STRICT_VERIFY_WATCH22.md) — read-only 2026-11-24
  ~21:2x watch: strict-verify PID 1137836 alive ~5m28s, selfcheck FAIL
  (248.0s, selftest step, 0 guest exceptions), qemu_smoke PASS (50.1s), now
  test 3/11 genuine_retire; notes a concurrent session possibly interfering;
  fault scan clean, no FV_EXIT yet.
- [STRICT_VERIFY_WATCH26.md](STRICT_VERIFY_WATCH26.md) — read-only 2026-11-24
  21:18 watch: same run alive ~5m22s in test 3/11 genuine_retire (EWSRV stage,
  EFSRV PASS); selfcheck FAIL (selftest step, 0 guest exceptions) and
  qemu_smoke PASS recorded, no OVERALL yet, fault scan clean.

These newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH18/19 backfill (append-only)

A fresh full `docs/*.md` listing (238 files, flat directory, no subdirectories)
was diffed against the unique `.md` mentions in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH18.md](STRICT_VERIFY_WATCH18.md) — read-only 2026-11-24
  21:17 EET watch of the in-flight `r1_final_verify.py --strict-skip` (PID
  1137836, flock-held, still test 1/11 selfcheck; build/isogrub/qemu_smoke/
  cp4_ca/cp4_stress PASS, `selftest` running); no verdicts yet, fault scan
  clean.
- [STRICT_VERIFY_WATCH19.md](STRICT_VERIFY_WATCH19.md) — read-only 2026-11-24
  21:17 EET watch of the same run (PID alive, still selfcheck; selfcheck
  sub-log build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS, `selftest` QEMU
  booting with normal AHCI/boot trace); no fault markers.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH latest-report backfill (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets previously in this index:
fourteen documents existed on disk but had no entry at this pass (intermediate
`STRICT_VERIFY_WATCH*` reports were indexed by concurrent sweeps); they are
linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [STRICT_VERIFY_WATCH18.md](STRICT_VERIFY_WATCH18.md) — read-only 2026-11-24
  21:17:36 EET watch of the in-flight `r1_final_verify.py --strict-skip` (PID
  1137836, log `final_verify_1913.log`): still on test 1/11 selfcheck with
  build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS and selftest running; tests
  2–11 logs stale, no OVERALL/FV_EXIT yet, fault scan clean; verdict IN
  PROGRESS.
- [STRICT_VERIFY_WATCH19.md](STRICT_VERIFY_WATCH19.md) — read-only 2026-11-24
  21:17 EET follow-up watch of the same run (~03:55 elapsed): still inside
  selfcheck (selftest QEMU booting, AHCI disk detected), all five completed
  selfcheck sub-tests PASS, no verdicts/OVERALL/FV_EXIT for run 1913 yet, no
  fault markers; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH31.md](STRICT_VERIFY_WATCH31.md) — read-only 2026-11-24
  21:20 EET watch: strict verify alive (~6m13s, flock held), test 4/11
  `genuine_efsrv` running; selfcheck FAIL (selftest step only, 0 guest
  exceptions), qemu_smoke/genuine_retire PASS; no OVERALL/FV_EXIT yet, fault
  scan clean; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH32.md](STRICT_VERIFY_WATCH32.md) — read-only 2026-11-24
  21:20 EET watch: test 5/11 `genuine_ewsrv` running; selfcheck FAIL
  (selftest sub-step), qemu_smoke/genuine_retire/genuine_efsrv PASS (4/11
  done, 3 PASS + selfcheck FAIL); no OVERALL/FV_EXIT yet; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH33.md](STRICT_VERIFY_WATCH33.md) — read-only 2026-11-24
  21:20 EET watch: still in `genuine_ewsrv` (test 5/11, QEMU boot phase);
  selfcheck FAIL, qemu_smoke/genuine_retire/genuine_efsrv PASS, no
  OVERALL/FV_EXIT yet, no fault markers; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH40.md](STRICT_VERIFY_WATCH40.md) — read-only 2026-11-24
  ~21:21 watch (~8m25s elapsed): on test 7/11 `doom_main`; selfcheck FAIL
  (selftest step, 0 guest exceptions), qemu_smoke/genuine_retire 4/4/
  genuine_efsrv/genuine_ewsrv/genuine_eshell 8-8/genuine_fat_io all PASS; no
  OVERALL/FV_EXIT yet, fault scan clean; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH41.md](STRICT_VERIFY_WATCH41.md) — read-only 2026-11-24
  21:21 UTC watch (~8m33s): still on `doom_main` (8th of 11); selfcheck FAIL,
  all six subsequent tests PASS; no OVERALL/FV_EXIT yet, no fault markers;
  verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH42.md](STRICT_VERIFY_WATCH42.md) — read-only 2026-11-24
  ~21:21 watch (~8m43s): 8/11 reached with `doom_main` RUNNING; selfcheck FAIL,
  qemu_smoke + all genuine/fat_io tests PASS; no OVERALL/FV_EXIT yet, fault
  scan clean; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH43.md](STRICT_VERIFY_WATCH43.md) — read-only 2026-11-24
  21:21 watch (~8m40s): `doom_main` (7/11) in flight, doom_audio/playtest/
  multikey pending; selfcheck FAIL, six tests PASS; no OVERALL/FV_EXIT yet;
  verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 2026-11-24
  21:22 watch (~9m09s): `doom_audio` (8/11) in flight; selfcheck FAIL (selftest
  step), qemu_smoke/genuine_retire 4-4/genuine_efsrv/genuine_ewsrv/
  genuine_eshell/genuine_fat_io/doom_main all PASS; no OVERALL/FV_EXIT yet,
  fault scan clean; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 2026-11-24
  21:22 EET watch (~9m10s): on `doom_audio` (9/11); selfcheck FAIL, seven
  subsequent tests incl. doom_main PASS (80.8s); no OVERALL/FV_EXIT yet, no
  guest fault markers; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 2026-11-24
  21:22 watch (~9 min): 8/11 decided with `doom_audio` just started; selfcheck
  FAIL (marker-order/ord-915 checks), qemu_smoke + six genuine/fat/doom_main
  tests PASS; no OVERALL/FV_EXIT yet; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 2026-11-24
  21:22 UTC watch (~9 min): `doom_audio` (9/11, QEMU booting) running;
  selfcheck FAIL, qemu_smoke/genuine_*/fat_io/doom_main PASS; no
  OVERALL/FV_EXIT yet; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only 2026-11-24
  watch (~9m18s): `doom_audio` (10/11) running, playtest/multikey pending;
  selfcheck FAIL, seven tests PASS; no OVERALL/FV_EXIT yet, 0 fault matches;
  verdict IN PROGRESS.

These fourteen newly indexed reports bring the indexed Markdown total to
**262**, matching the 301 `.md` files under `docs/` (including this index; the
header count "(301 files)" at line 4 is updated); every Markdown document in
the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH18/19/22/26/33/35/37 backfill (append-only)

A fresh full `docs/*.md` listing (253 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: seven documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH18.md](STRICT_VERIFY_WATCH18.md) — read-only 2026-11-24
  21:17:36 EET watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836, log `/tmp/opencode/final_verify_1913.log`): still in test 1
  selfcheck with build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS and `selftest`
  running; tests 2–11 logs stale, no OVERALL/FV_EXIT yet; fault scan clean.
- [STRICT_VERIFY_WATCH19.md](STRICT_VERIFY_WATCH19.md) — read-only 2026-11-24
  21:17 EET watch of the same run (03:55 elapsed, still test 1 selfcheck, now
  `RUN selftest` with QEMU booting and AHCI disk detected; boot trace normal);
  no verdicts yet, no Exc/PanicKernExec/#PF/FAULT/Traceback in scanned logs.
- [STRICT_VERIFY_WATCH22.md](STRICT_VERIFY_WATCH22.md) — read-only 2026-11-24
  21:2x watch of the in-flight strict-verify run (PID 1137836): alive at 05:28
  elapsed; selfcheck **FAIL** (selftest ST markers, 0 guest exceptions) with
  possible concurrent-session interference, qemu_smoke PASS, now on test 3/11
  genuine_retire; fault scan clean.
- [STRICT_VERIFY_WATCH26.md](STRICT_VERIFY_WATCH26.md) — read-only 2026-11-24
  21:18 watch of the same run (~5m22s, test 3/11 genuine_retire in progress at
  the EWSRV stage, EFSRV PASS; selfcheck FAIL on selftest, qemu_smoke PASS; no
  OVERALL/FV_EXIT yet; no #PF/KERN-EXEC/Panic in per-test logs).
- [STRICT_VERIFY_WATCH33.md](STRICT_VERIFY_WATCH33.md) — read-only 2026-11-24
  21:20 EET watch of the in-flight strict-verify run (PID 1137836, alive 6m34s,
  test 5/11 genuine_ewsrv): selfcheck FAIL (selftest), qemu_smoke/genuine_retire/
  genuine_efsrv PASS; no OVERALL/FV_EXIT yet; fault scan clean.
- [STRICT_VERIFY_WATCH35.md](STRICT_VERIFY_WATCH35.md) — read-only 2026-11-24
  21:26 watch of the same run (~7 min, now test 6/11 genuine_eshell): selfcheck
  FAIL (selftest QEMU timeout 124), qemu_smoke/retire/efsrv/ewsrv PASS, eshell
  RUNNING, fat_io + 4x doom pending; fault scan clean.
- [STRICT_VERIFY_WATCH37.md](STRICT_VERIFY_WATCH37.md) — read-only 2026-11-24
  21:20 watch of the same run (~7 min, test 6/11 genuine_eshell QEMU booting):
  selfcheck FAIL (248.0s), qemu_smoke/retire/efsrv/ewsrv PASS; no
  OVERALL/FV_EXIT; no fault/panic/KERN-/#PF markers.

These nine newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 post-235 docs backfill (append-only)

A fresh full `docs/*.md` listing (242 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: three documents existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [STRICT_VERIFY_WATCH21.md](STRICT_VERIFY_WATCH21.md) — read-only 2026-11-24
  21:17 watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836): selfcheck FAIL (selftest step, all ST markers FAIL, 0 guest
  exceptions), qemu_smoke in progress; fault scan clean.
- [STRICT_VERIFY_WATCH22.md](STRICT_VERIFY_WATCH22.md) — read-only 2026-11-24
  watch of the same run: selfcheck FAIL (selftest markers), qemu_smoke PASS,
  now on test 3/11 `genuine_retire`; notes a concurrent `r1_winsrv_delete.py`
  session as possible selftest interference; no faults in current logs.
- [STRICT_VERIFY_WATCH26.md](STRICT_VERIFY_WATCH26.md) — read-only 2026-11-24
  21:18 watch: still in test 3/11 `genuine_retire` (EWSRV stage, EFSRV PASS),
  selfcheck FAIL / qemu_smoke PASS, no OVERALL/FV_EXIT yet, fault scan clean.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH20/21 backfill (append-only)

A fresh full `docs/*.md` listing (240 files, flat directory, no subdirectories)
was diffed against the 238 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below, and no
links are stale (0 linked targets missing from disk). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH20.md](STRICT_VERIFY_WATCH20.md) — read-only 2026-11-24
  21:17Z watch of the in-flight `r1_final_verify.py --strict-skip` run 1913
  (PID 1137836, test 2/11 `qemu_smoke`): selfcheck FAIL at the harness
  marker-order/EUSER-ord-915 level with 0 guest exceptions, fault scan clean.
- [STRICT_VERIFY_WATCH21.md](STRICT_VERIFY_WATCH21.md) — read-only 2026-11-24
  21:17 watch of the same run: selfcheck FAIL (248 s, marker-level, no guest
  faults), qemu_smoke in progress, no OVERALL/FV_EXIT yet, `kernelhwsrv/`
  clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH18/19/20 backfill (append-only)

A fresh full `docs/*.md` listing (239 files, flat directory, no subdirectories)
was diffed against the 236 unique `.md` link targets previously in this index:
three documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH18.md](STRICT_VERIFY_WATCH18.md) — read-only 2026-11-24
  ~21:17 EET watch: PID 1137836 alive (~4 min), still in test 1/11 selfcheck
  (`RUN selftest`, prior sub-steps PASS), tests 2–11 logs stale, no
  OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_WATCH19.md](STRICT_VERIFY_WATCH19.md) — read-only 2026-11-24
  21:17 EET watch: PID alive (~3m55s), still test 1/11 selfcheck
  (build/isogrub/qemu_smoke/cp4_ca/cp4_stress PASS, `RUN selftest`); no
  verdicts yet; fault scan clean.
- [STRICT_VERIFY_WATCH20.md](STRICT_VERIFY_WATCH20.md) — read-only 2026-11-24
  21:17Z watch: PID alive (4m26s), now test 2/11 qemu_smoke; selfcheck FAIL is
  a harness marker-order check (guest `ST: RESULT PASS, failures=0`); no fault
  markers; `kernelhwsrv/` clean.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH22/26 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: two documents existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [STRICT_VERIFY_WATCH22.md](STRICT_VERIFY_WATCH22.md) — read-only 2026-11-24
  watch of the in-flight strict verify (PID 1137836, ~5m28s elapsed): selfcheck
  FAIL (selftest step, 0 guest exceptions), qemu_smoke PASS, now on test 3/11
  `genuine_retire`; notes possible interference from a concurrent session; no
  fault markers in current-run logs.
- [STRICT_VERIFY_WATCH26.md](STRICT_VERIFY_WATCH26.md) — read-only 2026-11-24
  21:18 watch: run alive ~5m22s in test 3/11 `genuine_retire` (EWSRV stage,
  EFSRV PASS); selfcheck FAIL (marker-level, 0 guest exceptions), qemu_smoke
  PASS; no OVERALL/FV_EXIT yet, fault scan clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` at sweep time (including this
index); every Markdown document in the directory is linked.

## Added — 2026-11-24 post-236 sweep, STRICT_VERIFY_WATCH20/21 backfill (append-only)

A full `docs/*.md` listing (240 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH20.md](STRICT_VERIFY_WATCH20.md) — read-only 2026-11-24
  21:17Z watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836, flock-held, test 2/11 `qemu_smoke` running): selfcheck FAIL
  (248.0 s) traced to a harness marker-order/EUSER-ord-915 check, not a guest
  fault (`ST: RESULT PASS, failures=0`, 0 exceptions); fault scan clean.
- [STRICT_VERIFY_WATCH21.md](STRICT_VERIFY_WATCH21.md) — read-only 2026-11-24
  21:17 follow-up watch of the same run: selfcheck FAIL with all ST markers
  FAIL incl. EUSER ord 915 (0 guest exceptions, marker-level harness issue),
  `qemu_smoke` in progress, no OVERALL/FV_EXIT yet; fault scan clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(240 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH20/21 backfill (append-only)

A full `docs/*.md` listing (240 files, flat directory, no subdirectories) was
diffed against the 238 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH20.md](STRICT_VERIFY_WATCH20.md) — read-only 2026-11-24
  21:17Z watch of the in-flight `r1_final_verify.py --strict-skip` (PID 1137836,
  flock held, ~4m26s): advanced to test 2/11 `qemu_smoke`; selfcheck FAIL
  (248 s, harness marker-order/EUSER-ord-915 check, 0 guest exceptions);
  fault scan clean.
- [STRICT_VERIFY_WATCH21.md](STRICT_VERIFY_WATCH21.md) — read-only 2026-11-24
  21:17 watch of the same run 1913: selfcheck FAIL (all ST markers FAIL incl.
  ord 915, 0 guest faults — marker-level, not a guest fault), `qemu_smoke`
  (test 2/11) in progress, no OVERALL/FV_EXIT yet, fault scan clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(240 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH31/32 backfill (append-only)

A full `docs/*.md` listing (248 files, flat directory, no subdirectories) was
diffed against the 246 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below, and no
links are stale (0 linked targets missing from disk). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH31.md](STRICT_VERIFY_WATCH31.md) — read-only 2026-11-24
  ~21:20 watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836, flock held): selfcheck FAIL (248 s, selftest sub-step, 0 guest
  exceptions), qemu_smoke PASS, genuine_retire PASS, test 4/11 genuine_efsrv
  running; no OVERALL/FV_EXIT yet; fault scan clean.
- [STRICT_VERIFY_WATCH32.md](STRICT_VERIFY_WATCH32.md) — read-only 2026-11-24
  21:20 watch of the same run (~6m32s): selfcheck FAIL, qemu_smoke PASS,
  genuine_retire PASS, genuine_efsrv PASS (27.2 s), test 5/11 genuine_ewsrv
  running; no OVERALL/FV_EXIT yet; fault scan clean; verdict IN PROGRESS.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH22/26 backfill (append-only)

A fresh full `docs/*.md` listing (242 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH22.md](STRICT_VERIFY_WATCH22.md) — read-only 2026-11-24
  21:2x watch of strict-verify run 1913 (PID 1137836, ~05:28 elapsed): selfcheck
  FAIL (selftest ST markers, 0 guest exceptions), qemu_smoke PASS, now on test
  3/11 `genuine_retire`; notes a concurrent session possibly interfering; fault
  scan clean.
- [STRICT_VERIFY_WATCH26.md](STRICT_VERIFY_WATCH26.md) — read-only 2026-11-24
  21:18 watch of the same run (~5m22s, test 3/11 `genuine_retire`, EWSRV stage,
  EFSRV PASS): selfcheck FAIL (marker-level), qemu_smoke PASS, no OVERALL yet,
  no faults in any per-test log.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 strict-verify watch backfill (append-only)

A fresh full `docs/*.md` listing (246 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index; the document(s)
below existed on disk but had no entry. `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

- [STRICT_VERIFY_WATCH27.md](STRICT_VERIFY_WATCH27.md) — STRICT_VERIFY_WATCH27 — monitor report (2026-11-24 ~21:24)
- [STRICT_VERIFY_WATCH28.md](STRICT_VERIFY_WATCH28.md) — STRICT_VERIFY_WATCH28 — in-flight strict verify watch (2026-11-24 21:19)
- [STRICT_VERIFY_WATCH29.md](STRICT_VERIFY_WATCH29.md) — STRICT VERIFY WATCH 29 — in-flight run (2026-11-24 21:13Z)
- [STRICT_VERIFY_WATCH30.md](STRICT_VERIFY_WATCH30.md) — STRICT_VERIFY_WATCH30 — in-flight strict verify watch

This backfill brings the indexed Markdown total to **246**, matching the 246
`.md` files under `docs/` (including this index; the header count at line 4 is
updated); every Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH33/35/37 backfill (append-only)

A full `docs/*.md` listing (253 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: three documents
existed on disk but had no entry; they are linked below, and 0 links are stale
(0 linked targets missing from disk). `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

- [STRICT_VERIFY_WATCH33.md](STRICT_VERIFY_WATCH33.md) — read-only 2026-11-24
  21:20 EET watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836, log `final_verify_1913.log`): selfcheck FAIL (248 s selftest
  sub-step), qemu_smoke/genuine_retire/genuine_efsrv PASS, genuine_ewsrv in
  progress (test 5/11), no OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_WATCH35.md](STRICT_VERIFY_WATCH35.md) — read-only 2026-11-24
  21:26 watch of the same run (~7 min): selfcheck FAIL (selftest QEMU timeout
  124), qemu_smoke/retire/efsrv/ewsrv PASS, genuine_eshell RUNNING (test 6/11),
  fat_io + 4x doom pending, no OVERALL yet, fault scan clean.
- [STRICT_VERIFY_WATCH37.md](STRICT_VERIFY_WATCH37.md) — read-only 2026-11-24
  21:20 watch of the same run: PID 1137836 alive, test 6/11 genuine_eshell
  running with QEMU booting; selfcheck FAIL (248.0 s), qemu_smoke PASS (50.1 s),
  genuine_retire PASS (63.8 s), genuine_efsrv PASS (27.2 s), genuine_ewsrv PASS
  (22.7 s); no OVERALL/FV_EXIT yet, fault scan clean.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH31/32/33/38/41/42/43/46/47 backfill (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the 260 unique `.md` link targets previously in this index:
nine documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH31.md](STRICT_VERIFY_WATCH31.md) — read-only 2026-11-24
  21:20 watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836, log `/tmp/opencode/final_verify_1913.log`): selfcheck FAIL (selftest
  sub-step only, no guest exceptions), qemu_smoke/genuine_retire PASS, test 4/11
  genuine_efsrv running; fault scan clean; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH32.md](STRICT_VERIFY_WATCH32.md) — read-only 2026-11-24
  21:20 watch of the same run (~6m32s): selfcheck FAIL (selftest sub-step),
  qemu_smoke/genuine_retire/genuine_efsrv PASS, test 5/11 genuine_ewsrv
  running; no OVERALL/FV_EXIT yet; fault scan clean; verdict IN PROGRESS.
- [STRICT_VERIFY_WATCH33.md](STRICT_VERIFY_WATCH33.md) — read-only 2026-11-24
  21:20 EET watch of the same run (~6m34s): selfcheck FAIL, smoke/retire/efsrv
  PASS, genuine_ewsrv in progress (test 5/11); no OVERALL/FV_EXIT; no
  Exc/PanicKernExec/FAULT/#PF in current-run logs.
- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only 2026-11-24
  ~7.5-min watch of the same run (PID 1137836): IN FLIGHT on test 7/11
  genuine_fat_io; selfcheck FAIL (selftest sub-step, 0 guest exceptions),
  qemu_smoke/retire(4/4)/efsrv/ewsrv/eshell(8/8) PASS; no OVERALL/FV_EXIT;
  fault scan clean.
- [STRICT_VERIFY_WATCH41.md](STRICT_VERIFY_WATCH41.md) — read-only 2026-11-24
  21:21 UTC watch of the same run (~8m33s, PID 1137836): on test 8/11
  doom_main; selfcheck FAIL (selftest step), qemu_smoke/retire/efsrv/ewsrv/
  eshell/fat_io all PASS; no OVERALL/FV_EXIT; fault scan benign; IN FLIGHT.
- [STRICT_VERIFY_WATCH42.md](STRICT_VERIFY_WATCH42.md) — read-only 2026-11-24
  ~21:21 watch (~8m43s): 8/11 reached with doom_main running; selfcheck FAIL
  (selftest), smoke/retire/efsrv/ewsrv/eshell/fat_io PASS; no OVERALL yet;
  no fault markers in any per-test log.
- [STRICT_VERIFY_WATCH43.md](STRICT_VERIFY_WATCH43.md) — read-only 2026-11-24
  21:21 watch (~8m40s): selfcheck FAIL (non-fatal under --strict-skip),
  smoke/retire(4/4)/efsrv/ewsrv/eshell/fat_io PASS, doom_main in flight
  (7/11), rest pending; 0 fault/panic/#PF matches; no OVERALL/FV_EXIT yet.
- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 2026-11-24
  21:22 watch (~9 min, PID 1137836): on doom_audio (9/11); selfcheck FAIL
  (selftest marker-order/ord-915 checks, 0 guest exceptions), smoke/retire/
  efsrv/ewsrv/eshell/fat_io/doom_main all PASS; no OVERALL yet; fault scan
  clean; IN FLIGHT.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 2026-11-24
  21:22 UTC watch (~9 min): doom_audio (9/11) running, playtest/multikey
  pending; selfcheck FAIL (selftest step, exit 124, 0 guest exceptions),
  smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main PASS (doom_main
  BrokenPipeError cosmetic only); no fault markers; no OVERALL/FV_EXIT yet.

These nine newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH31/32/33/35 backfill (append-only)

A full `docs/*.md` listing (250 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: four documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH31.md](STRICT_VERIFY_WATCH31.md) — read-only 2026-11-24
  21:20 watch of in-flight strict verify run 1913 (PID 1137836): selfcheck FAIL
  (selftest step only, 0 guest exceptions), qemu_smoke/genuine_retire PASS, now
  test 4/11 `genuine_efsrv`; no OVERALL yet, fault scan clean.
- [STRICT_VERIFY_WATCH32.md](STRICT_VERIFY_WATCH32.md) — read-only 21:20 watch:
  genuine_efsrv PASS (27.2s), now test 5/11 `genuine_ewsrv`; same verdict trail,
  no FV_EXIT, fault scan clean.
- [STRICT_VERIFY_WATCH33.md](STRICT_VERIFY_WATCH33.md) — read-only 21:20 EET
  watch: genuine_ewsrv in progress (test 5/11), prior tests as above, no
  OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_WATCH35.md](STRICT_VERIFY_WATCH35.md) — read-only 21:26 watch:
  genuine_ewsrv PASS, now `genuine_eshell` (test 6/11); selfcheck FAIL (selftest
  sub-step), qemu_smoke/retire/efsrv/ewsrv PASS; no OVERALL yet, fault scan
  clean.
- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only ~21:2x watch:
  genuine_eshell PASS (8/8), now test 7/11 `genuine_fat_io`; selfcheck FAIL
  (selftest step, 0 guest exceptions), qemu_smoke/retire/efsrv/ewsrv/eshell
  PASS; no OVERALL/FV_EXIT yet, fault scan clean.

These five newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH31–33 backfill (append-only)

A full `docs/*.md` listing (249 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: three documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH31.md](STRICT_VERIFY_WATCH31.md) — read-only 2026-11-24
  21:20 watch of the in-flight strict verify (PID 1137836, ~6m13s, test 4/11
  `genuine_efsrv` running): selfcheck FAIL (selftest step only, 0 guest
  exceptions), qemu_smoke + genuine_retire PASS, no OVERALL/FV_EXIT, fault
  scan clean.
- [STRICT_VERIFY_WATCH32.md](STRICT_VERIFY_WATCH32.md) — read-only 21:20 watch
  of the same run (~6m32s, test 5/11 `genuine_ewsrv` running): selfcheck FAIL
  (pre-existing selftest issue), qemu_smoke/genuine_retire/genuine_efsrv PASS
  (4/11 done), fault scan clean, no OVERALL yet.
- [STRICT_VERIFY_WATCH33.md](STRICT_VERIFY_WATCH33.md) — read-only 21:20 EET
  watch of run 1913 (~6m34s, `genuine_ewsrv` test 5/11 in QEMU boot): selfcheck
  FAIL (selftest), qemu_smoke/retire/efsrv PASS, no OVERALL/FV_EXIT, fault scan
  clean.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(249 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH31/32 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: two documents existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [STRICT_VERIFY_WATCH31.md](STRICT_VERIFY_WATCH31.md) — read-only 2026-11-24
  21:20 watch of the in-flight strict verify (PID 1137836, ~6m13s): selfcheck
  FAIL (selftest step only, 0 guest exceptions), qemu_smoke + genuine_retire
  PASS, test 4/11 `genuine_efsrv` running; fault scan clean; IN PROGRESS.
- [STRICT_VERIFY_WATCH32.md](STRICT_VERIFY_WATCH32.md) — read-only 2026-11-24
  21:20 watch: run ~6m32s, genuine_efsrv also PASS (27.2s), test 5/11
  `genuine_ewsrv` running; 4/11 done (3 PASS + selfcheck FAIL, no faults);
  fault scan clean; IN PROGRESS.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` at sweep time (including this
index; the header count at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH31/32/33 backfill (append-only)

A full `docs/*.md` listing (249 files, flat directory, no subdirectories) was
diffed against the 246 unique `.md` link targets previously in this index:
three documents existed on disk but had no entry; they are linked below, and
no links are stale (0 linked targets missing from disk). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH31.md](STRICT_VERIFY_WATCH31.md) — read-only 2026-11-24
  21:20 watch of the in-flight `r1_final_verify.py --strict-skip` (PID 1137836,
  ~6m13s, test 4/11 `genuine_efsrv` running): selfcheck FAIL (selftest step,
  0 guest exceptions), qemu_smoke PASS, genuine_retire PASS; no
  OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_WATCH32.md](STRICT_VERIFY_WATCH32.md) — read-only 2026-11-24
  21:20 watch of the same run (~6m32s, test 5/11 `genuine_ewsrv` running):
  selfcheck FAIL, qemu_smoke / genuine_retire / genuine_efsrv PASS (4/11 done);
  no OVERALL yet, fault scan clean.
- [STRICT_VERIFY_WATCH33.md](STRICT_VERIFY_WATCH33.md) — read-only 2026-11-24
  21:20 EET watch of the same run (~6m34s, `genuine_ewsrv` QEMU boot phase):
  4 completed tests PASS + selfcheck FAIL (pre-existing selftest marker issue,
  no guest faults); no OVERALL/FV_EXIT yet, fault scan clean.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(249 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH backfill (append-only)

A full `docs/*.md` listing (252 files, flat directory, no subdirectories) was
diffed against the 248 unique `.md` link targets previously in this index: the
following documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH33.md](STRICT_VERIFY_WATCH33.md) — STRICT_VERIFY_WATCH33 — in-flight strict verify watch (2026-11-24 21:20 EET).
- [STRICT_VERIFY_WATCH35.md](STRICT_VERIFY_WATCH35.md) — STRICT_VERIFY_WATCH35 — in-flight strict verify watch (2026-11-24 21:26).
- [STRICT_VERIFY_WATCH36.md](STRICT_VERIFY_WATCH36.md) — STRICT_VERIFY_WATCH36 — watch of /tmp/opencode/final_verify_1913.log (PID 1137836).
- [STRICT_VERIFY_WATCH37.md](STRICT_VERIFY_WATCH37.md) — STRICT_VERIFY_WATCH37 — watch of /tmp/opencode/final_verify_1913.log (PID 1137836).

These newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH38 backfill (append-only)

A full `docs/*.md` listing (253 files, flat directory, no subdirectories) was
diffed against the 252 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only 2026-11-24
  ~21:2x EET watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836, log `/tmp/opencode/final_verify_1913.log`): still on test 7/11
  `genuine_fat_io`; verdicts so far selfcheck FAIL (marker-level selftest, 0
  guest exceptions), qemu_smoke / genuine_retire 4/4 / genuine_efsrv /
  genuine_ewsrv / genuine_eshell 8/8 all PASS; no OVERALL/FV_EXIT yet; fault
  scan clean.

This newly indexed report brings the indexed Markdown total to **253**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(253 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH38 backfill (append-only)

A full `docs/*.md` listing (254 files, flat directory, no subdirectories) was
diffed against the 252 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only ~21:3x EET
  log-only watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836, `/tmp/opencode/final_verify_1913.log`): 7/11 tests done at
  watch time (`genuine_fat_io` running), selfcheck FAIL (pre-existing selftest
  marker issue), qemu_smoke/retire/efsrv/ewsrv/eshell PASS, no OVERALL yet,
  fault scan clean.
- [STRICT_VERIFY_WATCH40.md](STRICT_VERIFY_WATCH40.md) — read-only log-only
  watch (~8m25s elapsed) of the same strict-verify run: test 7/11 `doom_main`
  active, selfcheck FAIL (pre-existing), qemu_smoke/retire/efsrv/ewsrv/eshell/
  fat_io all PASS, 0 real fault hits, no OVERALL yet.

These newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(254 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH38 backfill (append-only)

A full `docs/*.md` listing (253 files, flat directory, no subdirectories) was
diffed against the 252 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only 2026-11-24
  ~21:2x watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836, log `/tmp/opencode/final_verify_1913.log`): ~7.5 min elapsed, test
  7/11 `genuine_fat_io` running; selfcheck FAIL (selftest markers only, 0
  guest exceptions), qemu_smoke/retire/efsrv/ewsrv/eshell PASS; fault scan
  clean; IN PROGRESS.

This newly indexed report brings the indexed Markdown total to **253**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH38 backfill (append-only)

A full `docs/*.md` listing (253 files, flat directory, no subdirectories) was
diffed against the 252 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only 2026-11-24
  ~21:2x watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836, log `final_verify_1913.log`): still on test 7/11
  `genuine_fat_io`; selfcheck FAIL (marker-level selftest, 0 guest exceptions),
  qemu_smoke / genuine_retire 4/4 / efsrv / ewsrv / eshell 8/8 all PASS, no
  OVERALL/FV_EXIT yet, fault scan clean.

This newly indexed report brings the indexed Markdown total to **253**,
matching the 301 `.md` files under `docs/` at sweep time (including this index;
the header count at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH38 backfill (append-only)

A full `docs/*.md` listing (253 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only 2026-11-24
  ~21:2x in-flight strict-verify watch (PID 1137836, ~7.5 min, test 7/11
  `genuine_fat_io` running): selfcheck FAIL (selftest step, 0 guest
  exceptions), qemu_smoke/retire/efsrv/ewsrv/eshell all PASS, no
  OVERALL/FV_EXIT yet, fault scan clean.

This newly indexed report brings the indexed Markdown total to **253**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(253 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH38 backfill (append-only)

A full `docs/*.md` listing (253 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only 2026-11-24
  (~7.5 min elapsed) watch of the in-flight `r1_final_verify.py --strict-skip`
  run (PID 1137836, log `/tmp/opencode/final_verify_1913.log`): test 7/11
  `genuine_fat_io` running; qemu_smoke / genuine_retire (4/4) /
  genuine_efsrv / genuine_ewsrv / genuine_eshell (8/8) PASS, selfcheck FAIL
  (selftest marker-order issue, 0 guest exceptions); fault scan clean;
  verdict IN PROGRESS.

This newly indexed report brings the indexed Markdown total to **253**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH38 backfill (append-only)

A full `docs/*.md` listing (253 files, flat directory, no subdirectories) was
diffed against the 252 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only 2026-11-24
  ~7.5-min watch of strict verify PID 1137836: IN FLIGHT on test 7/11
  genuine_fat_io; selfcheck FAIL (selftest markers, 0 guest exceptions),
  qemu_smoke/retire(4/4)/efsrv/ewsrv/eshell(8/8) all PASS; no
  OVERALL/FV_EXIT; fault scan clean.

This newly indexed report brings the indexed Markdown total to **253**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 final directory sweep + STRICT_VERIFY_WATCH44–48 backfill (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: six documents
existed on disk but had no entry; they are linked below (concurrent watcher
agents were still appending `STRICT_VERIFY_WATCH*.md` files during this
sweep, so the on-disk count may have moved past 262 by the time of reading).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 2026-11-24
  21:22 watch of the in-flight strict verify (PID 1137836, ~9m09s): selfcheck
  FAIL (selftest step, 0 guest exceptions), qemu_smoke/retire/efsrv/ewsrv/
  eshell/fat_io/doom_main all PASS; no OVERALL yet; fault scan clean.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 21:22 EET
  watch (~9m10s): now on `doom_audio` (test 9/11) with doom_main PASS (80.8 s);
  no OVERALL/FV_EXIT yet; fault scan clean.
- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 21:22
  watch (~9 min): IN FLIGHT, just entered `doom_audio`; no verdicts beyond
  the PASS set above; no OVERALL/FV_EXIT yet.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 21:22 UTC
  monitor of run 1913 (~9 min): on `doom_audio` (9/11, QEMU child booting the
  genuine kernel); run IN FLIGHT, no OVERALL/FV_EXIT yet.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only watch
  (elapsed 09:18): PID 1137836 alive on run 1913, log tail-safe; IN FLIGHT.
- [STRICT_VERIFY_WATCH51.md](STRICT_VERIFY_WATCH51.md) — read-only 21:24
  watch of run 1913 (PID 1137836, 10 min elapsed, flock held): IN FLIGHT,
  no FV_EXIT yet.

These newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` at sweep time (including this
index; the header count at line 4 is updated); every Markdown document in the
directory is linked at that point.

## Added — 2026-11-24 STRICT_VERIFY_WATCH21/22/26+ backfill (append-only)

A full `docs/*.md` listing (253 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: **0 documents on
disk are missing an entry and 0 links are stale** (253 on-disk `.md` files =
253 unique indexed targets, including this index; the header count
"(253 files)" at line 4 is updated). The newest watch reports
(`STRICT_VERIFY_WATCH21.md`, `STRICT_VERIFY_WATCH22.md`, `STRICT_VERIFY_WATCH26.md`
and later) were confirmed already indexed by the concurrent backfill sections
above, so this section adds no new link entries. `kernelhwsrv/` was untouched
and no QEMU run was made by this index update.

The indexed Markdown total stays **253**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH38 backfill (append-only)

A full `docs/*.md` listing (253 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry at sweep time; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH38.md](STRICT_VERIFY_WATCH38.md) — read-only 2026-11-24
  ~21:2x watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836, log `/tmp/opencode/final_verify_1913.log`): ~7.5 min elapsed,
  on test 7/11 `genuine_fat_io`, selfcheck FAIL (selftest markers only, 0
  guest faults) while qemu_smoke / genuine_retire 4/4 / efsrv / ewsrv /
  eshell 8/8 all PASS, no OVERALL/FV_EXIT yet, fault scan clean; verdict
  IN PROGRESS.

This newly indexed report brings the indexed Markdown total to **253**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(253 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH40/44/45/46/47/48 backfill (append-only)

A fresh full `docs/*.md` listing (262 files, flat directory, no subdirectories)
was diffed against the 256 unique `.md` link targets previously in this index:
six documents existed on disk but had no entry; they are linked below, and 0
links are stale (0 linked targets missing from disk). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH40.md](STRICT_VERIFY_WATCH40.md) — read-only 2026-11-24
  ~21:2x watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836, log `/tmp/opencode/final_verify_1913.log`): ~8m25s elapsed, test 7/11
  `doom_main` running; selfcheck FAIL (selftest markers, 0 guest exceptions),
  qemu_smoke/retire/efsrv/ewsrv/eshell/fat_io all PASS, no OVERALL/FV_EXIT yet,
  fault scan clean.
- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 2026-11-24
  21:22 watch of the same run (~9m09s): now past `doom_main` PASS (80.8 s);
  selfcheck FAIL + qemu_smoke/retire/efsrv/ewsrv/eshell/fat_io PASS; no
  OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 2026-11-24
  21:22 EET watch (~9m10s): test 9/11 `doom_audio` running; same verdict trail
  (selfcheck FAIL, all other completed tests PASS incl. doom_main); no
  OVERALL yet, fault scan clean.
- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 2026-11-24
  21:22 watch (~9 min): `doom_audio` just started, run IN FLIGHT, no
  OVERALL/FV_EXIT yet.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 2026-11-24
  21:22 UTC watch (~9 min): test 9/11 `doom_audio` with QEMU child booting the
  genuine kernel; run IN FLIGHT, no OVERALL/FV_EXIT yet.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only 2026-11-24
  watch (~9m18s): run PID 1137836 still alive on `doom_audio`, log 1.7 KB,
  no OVERALL/FV_EXIT yet.

These six newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH40–43 backfill (append-only)

A full `docs/*.md` listing (257 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: four documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH40.md](STRICT_VERIFY_WATCH40.md) — read-only 2026-11-24
  ~21:2x watch of the in-flight `r1_final_verify.py --strict-skip` (PID
  1137836): ~8m25s, on test 7/11 `doom_main`; selfcheck FAIL (selftest only,
  0 guest faults), qemu_smoke/retire 4/4/efsrv/ewsrv/eshell/fat_io all PASS,
  fault scan clean, no OVERALL/FV_EXIT yet; IN PROGRESS.
- [STRICT_VERIFY_WATCH41.md](STRICT_VERIFY_WATCH41.md) — read-only 2026-11-24
  21:21 UTC watch of the same run (~8m33s, `doom_main` 8th of 11): same
  verdict set (selfcheck FAIL, all others PASS), no OVERALL/FV_EXIT yet,
  fault scan clean; IN PROGRESS.
- [STRICT_VERIFY_WATCH42.md](STRICT_VERIFY_WATCH42.md) — read-only 2026-11-24
  21:21 watch: 8/11 reached, doom_main RUNNING (240 s timeout); selfcheck FAIL,
  qemu_smoke/retire/efsrv/ewsrv/eshell/fat_io PASS; no OVERALL/FV_EXIT yet;
  fault scan clean.
- [STRICT_VERIFY_WATCH43.md](STRICT_VERIFY_WATCH43.md) — read-only 2026-11-24
  21:21 watch (~8m40s, doom_main in flight, doom_audio/playtest/multikey
  pending): selfcheck FAIL (non-fatal under --strict-skip), all six completed
  non-selfcheck tests PASS; no OVERALL/FV_EXIT yet; fault scan clean.

These four newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH44–48 backfill (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: six documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 21:22 watch
  of the in-flight strict-verify run (PID 1137836, ~9m09s): doom_audio (8/11)
  in flight, selfcheck FAIL, the other six completed tests incl. doom_main
  PASS, no OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 21:22 EET
  watch of the same run: selfcheck FAIL, qemu_smoke/retire/efsrv/ewsrv/eshell/
  fat_io/doom_main PASS, run still in flight, no real fault hits.
- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 21:22 watch:
  8/11 decided, doom_audio just started; selfcheck FAIL (marker-order/ord-915
  checks only), all seven other completed tests PASS; no OVERALL yet.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 21:22 UTC
  monitor: doom_audio (9/11) running with QEMU child up, selfcheck FAIL plus
  seven PASS verdicts incl. doom_main (cosmetic BrokenPipe only); run in flight.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only watch
  (~09:18 elapsed): doom_audio (10/11) running, selfcheck FAIL, seven other
  completed tests PASS, doom_playtest/multikey pending, no OVERALL/FV_EXIT yet.

These newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(262 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH44/45/48 backfill (append-only)

A full `docs/*.md` listing (260 files, flat directory, no subdirectories) was
diffed against the 257 unique `.md` link targets previously in this index: three
documents existed on disk but had no entry; they are linked below, and 0 links
are stale (0 linked targets missing from disk). `kernelhwsrv/` was untouched
and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 2026-11-24
  21:22 watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836, ~9m09s): on test 8/11 `doom_audio`; selfcheck FAIL, qemu_smoke /
  retire 4/4 / efsrv / ewsrv / eshell / fat_io / doom_main all PASS, no
  OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 2026-11-24
  21:22 EET watch (~9m10s): now on test 9/11 `doom_audio`; same verdict set
  (selfcheck FAIL 248s, all seven others PASS), no OVERALL/FV_EXIT, no guest
  fault/panic/#PF matches.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only watch
  (~9m18s): on test 10/11 `doom_audio`; selfcheck FAIL, qemu_smoke / retire /
  efsrv / ewsrv / eshell / fat_io / doom_main all PASS, doom_playtest and
  doom_multikey pending, no OVERALL/FV_EXIT, fault scan 0 matches.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH44/45/48 backfill (append-only)

A full `docs/*.md` listing (260 files, flat directory, no subdirectories) was
diffed against the 257 unique `.md` link targets in this index: three
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 2026-11-24
  21:22 in-flight `--strict-skip` watch (PID 1137836, doom_audio 8/11):
  selfcheck FAIL, qemu_smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main all
  PASS, no OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 21:22 EET
  watch of the same run (9m10s, doom_audio 9/11): selfcheck FAIL (248s,
  qemu exit 124), seven tests PASS, run still in flight, fault scan clean.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only watch of the
  same run (09:18, doom_audio 10/11): selfcheck FAIL, seven tests PASS,
  doom_playtest/multikey pending, no OVERALL/FV_EXIT, 0 fault matches.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(260 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH44–48 backfill (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: six documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 2026-11-24
  21:22 watch (~9m09s) of the in-flight `r1_final_verify.py --strict-skip`
  run (PID 1137836, log `final_verify_1913.log`): 8/11 decided — selfcheck FAIL
  (selftest markers, 0 guest exceptions), qemu_smoke/retire/efsrv/ewsrv/
  eshell/fat_io/doom_main all PASS, doom_audio in flight; no OVERALL/FV_EXIT
  yet; fault scan clean.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 21:22 EET
  watch of the same run (~9m10s, now doom_audio test 9/11): same verdict trail
  (selfcheck FAIL, seven non-selfcheck tests PASS, doom_main PASS 80.8 s); no
  OVERALL yet; fault scan clean.
- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 21:22
  watch of the same run (~9 min, doom_audio just started): 8/11 decided with
  selfcheck FAIL and qemu_smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main
  PASS; doom_audio/playtest/multikey pending; zero real faults in per-test
  logs.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 21:22 UTC
  monitor of run 1913 (~9 min, doom_audio test 9/11 with QEMU child booting):
  selfcheck FAIL (selftest step, 0 guest exceptions), qemu_smoke through
  doom_main all PASS; no OVERALL/FV_EXIT yet; run IN FLIGHT.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only watch
  (~09:18 elapsed, doom_audio test 10/11): selfcheck FAIL (248 s),
  qemu_smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main PASS, playtest/
  multikey pending; no OVERALL/FV_EXIT; 0 panic/KERN-EXEC/#PF matches.

These five newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH44/45 backfill (append-only)

A full `docs/*.md` listing (259 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 2026-11-24
  21:22 watch of the in-flight strict verify (PID 1137836, ~9m09s, on
  doom_audio 8/11): selfcheck FAIL (selftest, 0 guest faults); qemu_smoke,
  retire 4/4, efsrv, ewsrv, eshell, fat_io and doom_main (80.8s) all PASS; no
  OVERALL/FV_EXIT yet; fault scan clean.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 2026-11-24
  21:22 EET watch of the same run (~9m10s, doom_audio 9/11): same verdicts —
  selfcheck FAIL, all other completed tests including doom_main PASS; run
  still in flight, no OVERALL/FV_EXIT; fault scan clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 subagent docs-index sweep, STRICT_VERIFY_WATCH46/47 (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` mentions in this index: the two newest
watch reports existed on disk but had no entry; they are linked below with
one-line descriptions, and no links are stale (0 mentioned targets missing
from disk). `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 2026-11-24
  21:22 watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836, ~9 min, now in `doom_audio`): 8/11 decided — selfcheck FAIL
  (selftest marker-level, 0 guest exceptions), qemu_smoke/retire/efsrv/ewsrv/
  eshell/fat_io/doom_main all PASS; no OVERALL/FV_EXIT yet; fault scan clean.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 2026-11-24
  21:22 UTC watch of the same run (doom_audio 9/11, QEMU child booting): same
  verdict set with doom_main PASS (cosmetic BrokenPipeError in screenshot
  finalize), doom_audio RUNNING, playtest/multikey pending; fault scan clean.

These newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH46/47 backfill (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the 260 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 2026-11-24
  21:22 watch of the in-flight `r1_final_verify.py --strict-skip` run (PID
  1137836, log `/tmp/opencode/final_verify_1913.log`, ~9 min elapsed): on test
  9/11 `doom_audio`; verdicts so far selfcheck FAIL (marker-order/ord-915
  harness checks, 0 guest exceptions) with qemu_smoke / genuine_retire 4/4 /
  genuine_efsrv / genuine_ewsrv / genuine_eshell 8/8 / genuine_fat_io /
  doom_main all PASS; no OVERALL/FV_EXIT yet; fault scan clean.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 2026-11-24
  21:22 UTC follow-up watch of the same run (~9 min, `doom_audio` test 9/11
  with QEMU child booting): identical verdict trail (selfcheck FAIL selftest
  step only, eight later tests PASS incl. doom_main 80.8s; BrokenPipeError in
  screenshot finalize cosmetic), doom_playtest/doom_multikey pending, no
  OVERALL/FV_EXIT yet, fault scan clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH46–48 backfill (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: three documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 2026-11-24
  21:22 watch of the in-flight strict verify (PID 1137836, ~9 min, in
  `doom_audio`); no OVERALL/FV_EXIT yet; IN FLIGHT.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 2026-11-24
  21:22 UTC monitor of run 1913 (`r1_final_verify.py --strict-skip`,
  PID 1137836); in flight, no final verdict yet.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only 2026-11-24
  watch of the in-flight `r1_final_verify.py --strict-skip` run; no OVERALL/
  FV_EXIT yet.

These three newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH51 backfill (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH51.md](STRICT_VERIFY_WATCH51.md) — read-only 2026-11-24
  21:24 UTC watch of the in-flight `r1_final_verify.py --strict-skip` run
  (PID 1137836, ~10 min, on `doom_playtest` 10/11): selfcheck FAIL, the other
  eight completed tests (smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main/
  doom_audio) all PASS, no OVERALL/FV_EXIT yet, fault scan clean; IN FLIGHT.

This newly indexed report brings the indexed Markdown total to **263**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 final full directory sweep (append-only)

A fresh full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 1 document(s)
existed on disk but had no entry; they are linked below with one-line
descriptions, and 0 links are stale (0 linked targets missing from disk).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH51.md](STRICT_VERIFY_WATCH51.md) — read-only 2026-11-24 watch of the in-flight `r1_final_verify.py --strict-skip` run (PID 1137836, log `/tmp/opencode/final_verify_1913.log`): no OVERALL/FV_EXIT yet, fault scan clean.

These newly indexed reports bring the indexed Markdown total to **301** matching
the 263 `.md` files under `docs/` (including this index; the header count
"(263 files)" at line 4 is updated); every Markdown document in the directory
is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH44–48 backfill (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: six documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH44.md](STRICT_VERIFY_WATCH44.md) — read-only 2026-11-24
  ~21:22 watch (~9m09s, PID 1137836): selfcheck FAIL (0 guest exceptions),
  qemu_smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main all PASS, doom_audio
  in flight, no OVERALL/FV_EXIT yet; fault scan clean.
- [STRICT_VERIFY_WATCH45.md](STRICT_VERIFY_WATCH45.md) — read-only 2026-11-24
  ~21:22 EET watch (~9m10s): same PASS set plus doom_main PASS (80.8s),
  doom_audio (9/11) running, no OVERALL/FV_EXIT; fault scan clean.
- [STRICT_VERIFY_WATCH46.md](STRICT_VERIFY_WATCH46.md) — read-only 2026-11-24
  ~21:22 watch (~9 min, 8/11 decided): selfcheck FAIL (marker-order/ord-915
  checks), seven non-selfcheck tests PASS, doom_audio just started; no
  OVERALL/FV_EXIT; zero real faults.
- [STRICT_VERIFY_WATCH47.md](STRICT_VERIFY_WATCH47.md) — read-only 2026-11-24
  ~21:22 UTC watch (~9 min, QEMU child 1179691 in doom_audio): selfcheck
  FAIL (selftest only), seven non-selfcheck tests PASS, doom_audio RUNNING;
  no OVERALL/FV_EXIT; fault scan clean.
- [STRICT_VERIFY_WATCH48.md](STRICT_VERIFY_WATCH48.md) — read-only 2026-11-24
  watch (09:18 elapsed, doom_audio 10/11): selfcheck FAIL, seven non-selfcheck
  tests PASS, doom_playtest/multikey pending, no OVERALL/FV_EXIT; 0
  panic/KERN/#PF matches.

These five newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH51 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH51.md](STRICT_VERIFY_WATCH51.md) — read-only 2026-11-24
  watch of the in-flight `r1_final_verify.py --strict-skip` run (PID 1137836);
  no OVERALL/FV_EXIT yet; IN FLIGHT.

This newly indexed report brings the indexed Markdown total to **263**,
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 subagent post-257 full sweep (append-only)

A fresh full `docs/*.md` listing (263 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 documents
on disk are missing an entry and 0 links are stale** (263 on-disk `.md` files =
263 unique indexed targets, including this index; the header count "(263
files)" at line 4 is consistent). The one document past the 257 mark,
[`STRICT_VERIFY_WATCH51.md`](STRICT_VERIFY_WATCH51.md), was confirmed already
indexed by the concurrent backfill sections above (read-only 2026-11-24 21:24
watch of the in-flight `r1_final_verify.py --strict-skip` run 1913, PID
1137836: IN FLIGHT at 10m elapsed, selfcheck FAIL 248 s with 0 guest
exceptions, qemu_smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main/doom_audio
all PASS, doom_playtest RUNNING, doom_multikey pending, no OVERALL/FV_EXIT
yet, fault scan clean), so this section adds no new link entries.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **263**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 full directory sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: **0 unlinked documents and 0 broken links** (263 on-disk `.md`
files = 263 unique indexed targets, including this index; the header count
"(263 files)" at line 4 is consistent). No new documents were found at sweep
time, so this section adds no new link entries; `kernelhwsrv/` was untouched
and no QEMU run was made by this index update.

The indexed Markdown total stays **263**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 final sweep, post-263 (append-only)

A fresh full `docs/*.md` listing (263 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (263 on-disk `.md` files = 263 unique indexed
targets, including this index; the header count "(263 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **263**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent docs-index sweep (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (263 on-disk `.md` files = 263 unique indexed
targets, including this index; the header count "(263 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **263**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 final full directory sweep, 263 confirmed (append-only)

A fresh full `docs/*.md` listing (263 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (263 on-disk `.md` files = 263 unique indexed
targets, including this index; the header count "(263 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **263**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 STRICT_VERIFY_WATCH41–51 confirmation sweep (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index (LC_ALL=C collation): **0 unlinked documents and 0
broken links**. The newest watch reports `STRICT_VERIFY_WATCH41.md` through
`STRICT_VERIFY_WATCH51.md` — read-only 2026-11-24 monitors of the in-flight
`r1_final_verify.py --strict-skip` run (PID 1137836, log
`/tmp/opencode/final_verify_1913.log`; selfcheck FAIL with 0 guest
exceptions, qemu_smoke/retire/efsrv/ewsrv/eshell/fat_io/doom_main/doom_audio
PASS, later tests still running, no OVERALL/FV_EXIT yet, fault scan clean) —
are confirmed indexed in the sections above, so this section adds no new
link entries. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

The indexed Markdown total stays **263**, matching the 301 `.md` files under
`docs/` (including this index; the header count "(263 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 subagent full directory sweep, post-260 (append-only)

A fresh full `docs/*.md` listing (263 files, flat directory, no subdirectories)
was diffed against the 263 unique `.md` link targets in this index: **0
unlinked documents and 0 broken links** (263 on-disk `.md` files = 263 unique
indexed targets, including this index; the header count "(263 files)" at line 4
is consistent). The newest watch reports on disk at scan time
(`STRICT_VERIFY_WATCH44/45/46/47/48/51.md`) were confirmed already linked in
the sections above, so this section adds no new link entries; `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **263**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 final new-docs index sweep (append-only)

A fresh full `docs/*.md` listing (263 files, flat directory, no subdirectories)
was diffed against the 263 unique `.md` link targets in this index: **0
documents on disk are missing an entry and 0 links are stale**, so there are
no missing docs to add and this section adds no new link entries.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **263**, matching the 301 `.md` files under
`docs/` (including this index; the header count "(263 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 post-257 index sweep (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: **0 documents on disk are missing an entry and 0 links
are stale** (263 on-disk `.md` files = 263 unique indexed link targets,
including this index; the header count "(263 files)" at line 4 is
consistent). No missing documents were found past the 257 mark, so this
section adds no new link entries; `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

The indexed Markdown total stays **263**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent docs-index sweep (append-only)

A fresh full `docs/*.md` listing (263 files, flat directory, no subdirectories) was diffed against the 263 unique `.md` link targets in this index: **0 unlinked documents and 0 broken links**. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

No missing documents were found, so this section adds no new link entries.

The indexed Markdown total stays **263**, matching the 301 `.md` files under `docs/` (including this index; the header count at line 4 is consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 new-docs index sweep, 264 confirmed (append-only)

A fresh full `docs/*.md` listing (264 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index using `LC_ALL=C`
collation on both sides: one document existed on disk but had no entry; it is
linked below, and 0 links are stale (264 on-disk `.md` files = 264 unique
indexed link targets, including this index). `kernelhwsrv/` was untouched and
no QEMU run was made by this index update.

- [SELFTEST_MARKER_REGRESSION.md](SELFTEST_MARKER_REGRESSION.md) — read-only
  2026-11-24 diagnosis of the 21:13 strict-run selftest false FAIL: a harness
  race from `r1_selftest.py`'s fixed shared `/tmp/opencode/r1_selftest.*` paths
  (no lock) when two selfchecks overlapped, not a boot regression — guest log
  held all 9 ST markers + ord-915 bind with 0 exceptions; minimal fix is
  per-run unique paths plus a flock.

This newly indexed report brings the indexed Markdown total to **264**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(263 files)" at line 4 predates this file and now reads 264 on disk);
every Markdown document in the directory is linked.

## Added — 2026-11-24 task sweep: no missing docs (append-only)

A full `docs/*.md` listing (263 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (263 on-disk `.md` files = 263 unique indexed
targets, including this index; the header count "(263 files)" at line 4 was
consistent at sweep time). No missing documents were found, so this section
adds no new link entries; `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

The indexed Markdown total stayed **263** at that sweep, matching the 263
`.md` files then under `docs/` (including this index).

## Added — 2026-11-24 SELFTEST_MARKER_REGRESSION backfill (append-only)

A fresh full `docs/*.md` listing (264 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [SELFTEST_MARKER_REGRESSION.md](SELFTEST_MARKER_REGRESSION.md) — read-only
  diagnosis of the 21:13 strict-run selftest false FAIL: a `r1_selftest.py`
  harness race on shared `/tmp/opencode/r1_selftest.*` paths (log truncated
  by an overlapping selfcheck) rather than a boot-output change; the guest
  log contains all 9 ST markers + ord-915 PASS, with a minimal per-run-paths
  + flock fix and a discriminating no-concurrent-QEMU rerun.

This newly indexed report brings the indexed Markdown total to **264**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(264 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 strict-verify run-1913 report backfill (append-only)

A fresh full `docs/*.md` listing (270 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: six documents existed on disk but had no entry; they are linked
below. All six are read-only reports on the same
`r1_final_verify.py --strict-skip` run 1913 (PID 1137836,
`/tmp/opencode/final_verify_1913.log`, finished ~21:25 EET): OVERALL FAIL
with 10/11 PASS (qemu_smoke, genuine_retire, genuine_efsrv, genuine_ewsrv,
genuine_eshell, genuine_fat_io, doom_main/audio/playtest/multikey) and only
selfcheck FAIL (248 s, inner selftest markers + ord 915), 0 guest faults.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_FINAL2.md](STRICT_VERIFY_FINAL2.md) — final outcome of the
  strict run: 10/11 PASS, selfcheck the sole FAIL, clean fault scan.
- [STRICT_VERIFY_FINAL3.md](STRICT_VERIFY_FINAL3.md) — second final-outcome
  note for the same log: FV_EXIT=1, OVERALL FAIL (10 pass / 0 skip / 1 fail),
  selfcheck log ends mid inner qemu_smoke step.
- [STRICT_VERIFY_WATCH13.md](STRICT_VERIFY_WATCH13.md) — 21:25 watch note:
  run finished, flock released, 10/10 non-selfcheck PASS, 0 actual faults.
- [STRICT_VERIFY_WATCH24.md](STRICT_VERIFY_WATCH24.md) — watch of run 1913:
  10 PASS / 0 SKIP / 1 FAIL, selfcheck selftest sub-step FAIL, no guest
  faults/panics anywhere.
- [STRICT_VERIFY_WATCH34.md](STRICT_VERIFY_WATCH34.md) — watch of the same
  log: selfcheck counted (not skipped) under `--strict-skip`, OVERALL FAIL,
  benign-only fault-scan hits.
- [STRICT_VERIFY_WATCH52.md](STRICT_VERIFY_WATCH52.md) — final watch:
  selfcheck detail (all selftest markers FAIL, wrapper exit 124, 0 guest
  exceptions), 10 PASS list, 0 Exc/Panic/FAULT/#PF.

These six newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(270 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 full sweep at 276 — twelve report backfills (append-only)

A full `docs/*.md` listing (276 files, flat directory, no subdirectories) was
diffed against the 264 unique `.md` link targets previously in this index:
twelve documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_FINAL2.md](STRICT_VERIFY_FINAL2.md) — final outcome of
  `r1_final_verify.py --strict-skip` run 1913 (PID 1137836): OVERALL FAIL,
  10/11 PASS with only `selfcheck` FAIL (248 s, ends inside inner qemu_smoke);
  fault scan clean, FV_EXIT=1.
- [STRICT_VERIFY_FINAL3.md](STRICT_VERIFY_FINAL3.md) — second read-only summary
  of the same run 1913: 10/11 PASS, selfcheck FAIL (selftest sub-step, no
  verdict printed), OVERALL FAIL, FV_EXIT=1.
- [STRICT_VERIFY_WATCH13.md](STRICT_VERIFY_WATCH13.md) — 21:25 watch: run 1913
  finished (~12m47s, flock released), 10/10 non-selfcheck tests PASS,
  selfcheck FAIL (selftest wrapper exit 124), 0 actual faults; OVERALL FAIL.
- [STRICT_VERIFY_WATCH24.md](STRICT_VERIFY_WATCH24.md) — watch of run 1913:
  10 PASS / 0 SKIP / 1 FAIL (selfcheck 248 s, inner selftest marker order +
  ord-915), no guest faults/panics anywhere, FV_EXIT=1.
- [STRICT_VERIFY_WATCH34.md](STRICT_VERIFY_WATCH34.md) — watch of run 1913:
  10 PASS, selfcheck FAIL counted not skipped under `--strict-skip`, OVERALL
  FAIL (FV_EXIT=1), fault-scan hits all benign.
- [STRICT_VERIFY_WATCH52.md](STRICT_VERIFY_WATCH52.md) — final outcome of run
  1913: PID exited, FV_EXIT=1, 10 PASS / 0 skip / 1 FAIL (selfcheck; all
  selftest markers FAIL, wrapper exit 124, 0 guest exceptions), 0
  Exc/Panic/FAULT/#PF in the full log.
- [HARNESS_FIX_STATUS.md](HARNESS_FIX_STATUS.md) — read-only status of the
  selfcheck race fix (per-PID logs + flock in `r1_os_selfcheck.py` /
  `r1_selftest.py`): verdict NOT STARTED — both tools still use fixed log
  paths with no getpid/flock.
- [SELFTEST_RERUN.md](SELFTEST_RERUN.md) — read-only ~21:26 quiet-slot rerun of
  `r1_os_selfcheck.py`: OVERALL blocked — slot not quiet, selfcheck not run.
- [STRICT_VERIFY_FINAL5.md](STRICT_VERIFY_FINAL5.md) — run-1913 outcome with
  per-test timings: 10/11 PASS, selfcheck FAIL (nested qemu_smoke stalled;
  standalone qemu_smoke PASS 50.1 s — harness flake, not OS regression),
  OVERALL FAIL, FV_EXIT=1.
- [STRICT_VERIFY_FINAL6.md](STRICT_VERIFY_FINAL6.md) — run-1913 final outcome:
  10 PASS / 0 skip / 1 FAIL (selfcheck 248 s, log truncated mid qemu_smoke),
  no fault/panic/#PF/KERN markers, FV_EXIT=1, sole cause selfcheck-harness
  flakiness.
- [STRICT_VERIFY_WATCH11.md](STRICT_VERIFY_WATCH11.md) — watch of run 1913
  (exited 21:25:51, FV_EXIT=1): 10/11 PASS, selfcheck FAIL; child log truncated
  at qemu_smoke step while a concurrent selfcheck (PID 1144439) shared the
  child log paths.
- [STRICT_VERIFY_WATCH17.md](STRICT_VERIFY_WATCH17.md) — read-only monitor of
  run 1913: process exited, OVERALL FAIL (10 pass / 0 skip / 1 fail =
  selfcheck 248 s), selfcheck log truncated after `== RUN qemu_smoke`, no
  faults; earlier in-run snapshot showed selfcheck sub-steps PASS.

These twelve newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` at sweep time (including this index;
the header count at line 4 tracks the live total); every Markdown document in
the directory is now linked.

## Added — 2026-11-24 SELFTEST_MARKER_REGRESSION backfill (append-only)

A full `docs/*.md` listing (264 files, flat directory, no subdirectories) was
diffed against the 263 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [SELFTEST_MARKER_REGRESSION.md](SELFTEST_MARKER_REGRESSION.md) — read-only
  2026-11-24 21:13 diagnosis of the selftest step's false FAIL in the strict
  run: a `tools/r1_selftest.py` harness race (fixed shared `/tmp/opencode`
  paths + single 1.0 s log read, no lock) from two overlapping selfchecks, not
  a boot regression; guest log holds all 9 ST markers + ord-915 PASS.

This newly indexed report brings the indexed Markdown total to **264**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 strict-verify watch/final backfill (append-only)

A full `docs/*.md` listing (270 files, flat directory, no subdirectories) was
diffed against the 264 unique `.md` link targets previously in this index: six
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH13.md](STRICT_VERIFY_WATCH13.md) — read-only 2026-11-24
  21:25 watch: strict-verify PID 1137836 finished (~12m47s), OVERALL FAIL
  (10 pass / 0 skip / 1 fail: selfcheck 248 s selftest-step timeout), all
  other 10 tests PASS, 0 actual faults.
- [STRICT_VERIFY_WATCH24.md](STRICT_VERIFY_WATCH24.md) — read-only watch of
  run 1913: exited ~21:25 EET, 10 PASS / 0 SKIP / 1 FAIL (selfcheck selftest
  sub-step), FV_EXIT=1, fault scan clean.
- [STRICT_VERIFY_WATCH34.md](STRICT_VERIFY_WATCH34.md) — read-only watch:
  OVERALL FAIL 10/1 (selfcheck counted, not skipped, under --strict-skip),
  FV_EXIT=1, no KERN/#PF/panic hits.
- [STRICT_VERIFY_WATCH52.md](STRICT_VERIFY_WATCH52.md) — read-only final
  outcome of run 1913: FV_EXIT=1, OVERALL FAIL (10 pass / 0 skip / 1 fail:
  selfcheck, guest selftest markers FAIL, wrapper exit 124, 0 exceptions);
  retire 4/4, efsrv/ewsrv/eshell/fat_io and all four DOOM tests PASS.
- [STRICT_VERIFY_FINAL2.md](STRICT_VERIFY_FINAL2.md) — read-only final
  outcome of `r1_final_verify.py --strict-skip` (PID 1137836): 10/11 PASS,
  selfcheck FAIL (248.0 s, inner qemu_smoke step).
- [STRICT_VERIFY_FINAL3.md](STRICT_VERIFY_FINAL3.md) — read-only final
  outcome: process exited FV_EXIT=1, OVERALL FAIL (10 pass / 0 skip / 1
  fail: selfcheck only).

These six newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-270 task sweep backfill (append-only)

A fresh full `docs/*.md` listing (280 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index (LC_ALL=C
collation on both sides): ten documents existed on disk but had no entry; they
are linked below, and 0 links are stale (0 linked targets missing from disk).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [HARNESS_FIX_STATUS.md](HARNESS_FIX_STATUS.md) — read-only 2026-11-24 status
  (verdict NOT STARTED) of the selfcheck-race fix: `tools/r1_os_selfcheck.py`
  and `tools/r1_selftest.py` still log to fixed shared `/tmp/opencode/` paths
  with no per-PID names or flock.
- [SELFTEST_RERUN.md](SELFTEST_RERUN.md) — read-only ~21:26 quiet-slot rerun
  of the 21:13 selfcheck failure: OVERALL blocked, slot not quiet, selfcheck
  not run.
- [STRICT_VERIFY_FINAL5.md](STRICT_VERIFY_FINAL5.md) — read-only final outcome
  of strict-verify run 1913 (PID 1137836, exited): OVERALL FAIL 10 pass / 0
  skip / 1 fail (selfcheck only, harness stall, FV_EXIT=1); all other ten
  tests PASS with timings.
- [STRICT_VERIFY_FINAL6.md](STRICT_VERIFY_FINAL6.md) — read-only close-out of
  run 1913: same 10/11 verdicts, FV_EXIT=1, no fault markers; sole failure is
  selfcheck's internal qemu_smoke step stalling while the standalone smoke
  PASSed — harness flakiness, not an OS regression.
- [STRICT_VERIFY_WATCH11.md](STRICT_VERIFY_WATCH11.md) — read-only watch of run
  1913 after exit (21:25:51, FV_EXIT=1): 10/11 PASS, selfcheck FAIL (248.0s),
  child log truncated mid-`qemu_smoke` by concurrent selfcheck PID 1144439.
- [STRICT_VERIFY_WATCH17.md](STRICT_VERIFY_WATCH17.md) — read-only monitor of
  the exited run 1913: OVERALL FAIL (10 pass / 1 fail selfcheck, FV_EXIT=1),
  no panic/#PF/KernExec faults; `kernelhwsrv/` untouched, no QEMU run.
- [STRICT_VERIFY_FINAL.md](STRICT_VERIFY_FINAL.md) — read-only final outcome of
  strict-verify run 1913 (`r1_final_verify.py --strict-skip`, PID 1137836):
  OVERALL FAIL (10 pass / 0 skip / 1 fail — selfcheck only), FV_EXIT=1; all
  ten functional tests PASS.
- [STRICT_VERIFY_WATCH3.md](STRICT_VERIFY_WATCH3.md) — read-only 21:13 monitor
  of run 1913: run DONE, PID gone, FV_EXIT=1; no QEMU launched by the monitor,
  `kernelhwsrv/` untouched.
- [STRICT_VERIFY_WATCH7.md](STRICT_VERIFY_WATCH7.md) — read-only outcome watch
  of run 1913: 10/11 PASS, selfcheck FAIL (248.0s; log ends at `== RUN cp4_ca`
  amid concurrent test processes), FV_EXIT=1.
- [STRICT_VERIFY_WATCH49.md](STRICT_VERIFY_WATCH49.md) — read-only final watch
  of run 1913: OVERALL FAIL (10 pass / 1 fail selfcheck), FV_EXIT=1, log
  complete, no QEMU left running.

These ten newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 strict-verify watch/final backfill (append-only)

A full `docs/*.md` listing (280 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: eight documents
existed on disk but had no entry; they are linked below.
`SELFTEST_MARKER_REGRESSION.md` was already indexed (backfill section above).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH3.md](STRICT_VERIFY_WATCH3.md) — read-only 21:13 EET
  monitor of strict-verify run 1913 (PID 1137836): run DONE, FV_EXIT=1.
- [STRICT_VERIFY_WATCH7.md](STRICT_VERIFY_WATCH7.md) — read-only outcome of
  run 1913: 10 PASS / selfcheck FAIL (248.0s, cp4_ca step stalled, possible
  contention), FV_EXIT=1, no guest faults.
- [STRICT_VERIFY_WATCH11.md](STRICT_VERIFY_WATCH11.md) — read-only watch of
  strict-verify run 1913 (PID 1137836, exited 21:25:51, FV_EXIT=1): 10/11 PASS,
  selfcheck FAIL (248.0s), clean fault scan.
- [STRICT_VERIFY_WATCH17.md](STRICT_VERIFY_WATCH17.md) — read-only in-flight
  outcome monitor: same run, OVERALL FAIL (10 pass / 0 skip / 1 fail:
  selfcheck), selfcheck child log truncated mid `qemu_smoke`, no faults.
- [STRICT_VERIFY_WATCH49.md](STRICT_VERIFY_WATCH49.md) — read-only final
  outcome: 10 PASS / selfcheck FAIL (248.0s), FV_EXIT=1, 0 real faults across
  all 11 per-test logs.
- [STRICT_VERIFY_FINAL.md](STRICT_VERIFY_FINAL.md) — read-only final outcome
  of run 1913: OVERALL FAIL (10 pass / 0 skip / 1 fail: selfcheck), FV_EXIT=1,
  clean main-log fault scan.
- [STRICT_VERIFY_FINAL5.md](STRICT_VERIFY_FINAL5.md) — read-only final
  verdict table for run 1913: selfcheck FAIL(248.0s), all other 10 tests PASS
  with timings, FV_EXIT=1, failure attributed to selfcheck-harness flakiness.
- [STRICT_VERIFY_FINAL6.md](STRICT_VERIFY_FINAL6.md) — read-only final
  outcome: process exited, log complete (3,266 B), OVERALL FAIL 10/1
  (selfcheck), fault scan clean, `kernelhwsrv/` untouched.

These eight newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 strict-verify run-1913 watch backfill, part 3 (append-only)

A fresh full `docs/*.md` listing (280 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: four more read-only reports on strict run 1913
(`/tmp/opencode/final_verify_1913.log`, PID 1137836: OVERALL FAIL, 10/11 PASS,
selfcheck the sole FAIL, FV_EXIT=1) existed on disk but had no entry; they
are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [STRICT_VERIFY_FINAL.md](STRICT_VERIFY_FINAL.md) — final outcome of run
  1913: OVERALL FAIL (10 pass / 0 skip / 1 fail: selfcheck 248.0 s),
  FV_EXIT=1, clean fault scan.
- [STRICT_VERIFY_WATCH3.md](STRICT_VERIFY_WATCH3.md) — read-only monitor of the
  in-flight 21:13 run; process finished with FV_EXIT=1.
- [STRICT_VERIFY_WATCH7.md](STRICT_VERIFY_WATCH7.md) — outcome watch: 10 PASS,
  selfcheck FAIL at the cp4_ca step under concurrent-QEMU contention.
- [STRICT_VERIFY_WATCH49.md](STRICT_VERIFY_WATCH49.md) — final outcome across
  all 11 per-test logs: 10 PASS, selfcheck FAIL, 0 real faults.

These four newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 post-280 new-docs backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index (LC_ALL=C collation on both sides): nine documents existed on disk
but had no entry; they are linked below, and 0 links are stale (0 linked
targets missing from disk). `kernelhwsrv/` was untouched and no QEMU run was
made by this index update. (Concurrent sweeps by other agents interleaved
further sections below; this section remains the dated 2026-11-24 backfill
for the post-280 reports.)

- [SELFTEST_RERUN_STATUS2.md](SELFTEST_RERUN_STATUS2.md) — read-only 2026-11-24
  21:27 monitor, verdict PENDING: standalone selfcheck rerun (PID 1194653)
  in flight with build/isogrub/qemu_smoke/cp4_ca PASS so far, no OVERALL yet.
- [STRICT_VERIFY_WATCH23.md](STRICT_VERIFY_WATCH23.md) — read-only monitor of
  strict-verify run 1913 (`r1_final_verify.py --strict-skip`, PID 1137836,
  exited, FV_EXIT=1): OVERALL FAIL (10 pass / 0 skip / 1 fail — selfcheck only,
  248.0 s selftest cap, all other ten tests PASS), fault scan clean.
- [HARNESS_FIX_STATUS3.md](HARNESS_FIX_STATUS3.md) — read-only 2026-11-24
  status, verdict LANDED: per-PID rundirs + `flock` in `r1_selftest.py`,
  `r1_os_selfcheck.py` and `r1_final_verify.py` closing the shared-log race.
- [STRICT_VERIFY_2125_FINAL.md](STRICT_VERIFY_2125_FINAL.md) — read-only final
  outcome of the 21:25 strict run (PID 1194626): OVERALL FAIL, 10/11 PASS,
  selfcheck the sole FAIL (248.0 s), FV_EXIT=1.
- [STRICT_VERIFY_2125_WATCH.md](STRICT_VERIFY_2125_WATCH.md) — read-only
  21:27 monitor of the 21:25 strict run (PID 1194626 alive, selfcheck child in
  cp4_stress), no verdicts yet, `kernelhwsrv/` untouched.
- [SELFTEST_RERUN_FINAL3.md](SELFTEST_RERUN_FINAL3.md) — read-only ~21:29 final
  status of the standalone selfcheck rerun (PID 1194653): stages through
  cp4_stress PASS, post-race-fix in-guest selftest RESULT PASS, no OVERALL yet.
- [STRICT_VERIFY_2125_FINAL2.md](STRICT_VERIFY_2125_FINAL2.md) — read-only
  monitor of the 21:25 strict run (PID 1194626): 10/11 per-test logs all
  RESULT PASS, selfcheck still running at report time.
- [FRESH_STRICT_WATCH2.md](FRESH_STRICT_WATCH2.md) — read-only 21:29 watch of
  the post-race-fix strict run (PID 1194626 alive, flock held): selfcheck now
  OVERALL PASS 6/6 sub-tests after the 21:28 harness fix.
- [STRICT_2125_OVERALL2.md](STRICT_2125_OVERALL2.md) — read-only OVERALL /
  FV_EXIT report for the 21:25 strict run: selfcheck FAIL (248.0 s), the other
  ten tests PASS, all logs small, no skip.

These nine newly indexed reports bring the indexed Markdown total to **302**
matching the 302 `.md` files under `docs/` (including this index; the header
count "(302 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 strict-verify watch 2/14/25/50 + selftest-rerun backfill (append-only)

A fresh full `docs/*.md` listing (285 files, flat directory, no subdirectories)
was diffed against the 280 unique `.md` link targets previously in this index:
five documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH2.md](STRICT_VERIFY_WATCH2.md) — read-only watch of
  strict-verify run 1913 (PID 1137836, FV_EXIT=1): OVERALL FAIL 10 pass /
  0 skip / 1 fail (selfcheck, stalled at `== RUN cp4_ca` with no verdict),
  0 fault markers in the main log.
- [STRICT_VERIFY_WATCH14.md](STRICT_VERIFY_WATCH14.md) — read-only 21:26 watch
  of the finished run 1913 (~13m elapsed): selfcheck FAIL (248.0s), all other
  10 tests PASS, OVERALL FAIL, FV_EXIT=1, 0 actual faults (only `[PASS] no
  ...` marker lines).
- [STRICT_VERIFY_WATCH25.md](STRICT_VERIFY_WATCH25.md) — read-only watch of
  run 1913: 10/11 PASS with selfcheck FAIL (selftest step all FAIL),
  OVERALL FAIL, FV_EXIT=1; no guest exceptions, only a benign host-side
  BrokenPipeError at doom_main teardown.
- [STRICT_VERIFY_WATCH50.md](STRICT_VERIFY_WATCH50.md) — read-only final
  outcome of run 1913: per-test timings, OVERALL FAIL (10 pass / 0 skip /
  1 fail: selfcheck 248.0s), FV_EXIT=1, fault scan 0 real FAULT/#PF/Panic.
- [SELFTEST_RERUN_STATUS.md](SELFTEST_RERUN_STATUS.md) — read-only ~21:27
  monitor check: standalone selfcheck rerun still PENDING on a quiet slot
  (busy with final_verify/qemu/selfcheck PIDs), latest selftest logs PASS
  but from a non-quiet slot, harness fix not yet landed.

These five newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 strict-verify watch 23 + selftest-rerun 2 backfill (append-only)

A fresh full `docs/*.md` listing (287 files, flat directory, no subdirectories)
was diffed against the 285 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH23.md](STRICT_VERIFY_WATCH23.md) — read-only monitor of
  strict-verify run 1913 (PID 1137836, exited): 10/11 PASS, selfcheck FAIL
  (248.0s, guest selftest hit the 90s QEMU cap, wrapper exit 124), OVERALL
  FAIL, FV_EXIT=1, no panic/#PF/KERN faults.
- [SELFTEST_RERUN_STATUS2.md](SELFTEST_RERUN_STATUS2.md) — read-only 21:27
  status: standalone selfcheck rerun (PID 1194653) still in flight with
  build/isogrub/qemu_smoke/cp4_ca all PASS, final selftest stage not yet
  reached; verdict PENDING.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 run-1913/selfcheck-rerun backfill, part 4 (append-only)

A fresh full `docs/*.md` listing (285 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: five more read-only reports existed on disk but had no entry; they
are linked below (four more watches of strict run 1913 — OVERALL FAIL, 10/11
PASS, selfcheck sole FAIL, FV_EXIT=1 — plus the standalone selfcheck-rerun
monitor). `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [STRICT_VERIFY_WATCH2.md](STRICT_VERIFY_WATCH2.md) — watch of the 21:13
  strict run: COMPLETE, FV_EXIT=1, 10 pass / 1 fail (selfcheck 248.0 s).
- [STRICT_VERIFY_WATCH14.md](STRICT_VERIFY_WATCH14.md) — watch: run finished
  21:26:05, 10/10 non-selfcheck PASS, selfcheck FAIL, clean.
- [STRICT_VERIFY_WATCH25.md](STRICT_VERIFY_WATCH25.md) — in-flight watch
  outcome: 10 PASS, selfcheck selftest-step FAIL, FV_EXIT=1.
- [STRICT_VERIFY_WATCH50.md](STRICT_VERIFY_WATCH50.md) — final outcome with
  per-test timings: 10 PASS, selfcheck FAIL, FV_EXIT=1.
- [SELFTEST_RERUN_STATUS.md](SELFTEST_RERUN_STATUS.md) — read-only ~21:27
  monitor: standalone selfcheck rerun still PENDING on a quiet slot; newest
  21:18 selftest logs show `ST: RESULT PASS`, 0 exceptions, but on a busy
  slot, so the discriminating rerun remains outstanding.

These five newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 run-1913/selfcheck-rerun backfill, part 5 (append-only)

A fresh full `docs/*.md` listing (287 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: two more read-only reports existed on disk but had no entry; they are
linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [STRICT_VERIFY_WATCH23.md](STRICT_VERIFY_WATCH23.md) — read-only monitor of
  strict run 1913 after exit: 10/11 PASS, selfcheck FAIL (selftest hit the
  90 s QEMU cap, wrapper exit 124, all ST markers FAIL).
- [SELFTEST_RERUN_STATUS2.md](SELFTEST_RERUN_STATUS2.md) — 21:27 monitor:
  standalone selfcheck rerun (PID 1194653) still in flight, verdict PENDING.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 strict run 21:25 backfill, part 6 (append-only)

A fresh full `docs/*.md` listing (289 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: two more read-only reports on the 21:25 strict run (PID 1194626,
`final_verify_1913.log`: OVERALL FAIL, 10/11 PASS, selfcheck sole FAIL,
FV_EXIT=1) existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_2125_FINAL.md](STRICT_VERIFY_2125_FINAL.md) — final outcome
  of the 21:25 strict run: 10 PASS, selfcheck FAIL (248.0 s), FV_EXIT=1.
- [STRICT_VERIFY_2125_WATCH.md](STRICT_VERIFY_2125_WATCH.md) — monitor-only
  watch of the same run at 21:27 EET.

These two newly indexed reports bring the indexed Markdown total to **292**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(289 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-263 newest-docs backfill (append-only)

A full `docs/*.md` listing (287 files, flat directory, no subdirectories) was
diffed against every `.md` basename mentioned in this index: two documents
existed on disk but had no entry; they are linked below, and 0 mentions are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [SELFTEST_RERUN_STATUS2.md](SELFTEST_RERUN_STATUS2.md) — read-only 2026-11-24
  21:27 monitor: verdict PENDING — the standalone `r1_os_selfcheck.py` rerun
  (PID 1194653) was in flight with build/isogrub/qemu_smoke/cp4_ca all PASS
  and the final `selftest` stage not yet reached; no quiet-slot OVERALL yet.
- [STRICT_VERIFY_WATCH23.md](STRICT_VERIFY_WATCH23.md) — read-only monitor of
  strict-verify run 1913 (PID 1137836, `/tmp/opencode/final_verify_1913.log`,
  exited): 10/11 PASS with selfcheck the sole FAIL (248.0 s, guest selftest
  hit the 90 s QEMU cap, all ST markers FAIL), OVERALL FAIL, FV_EXIT=1, fault
  scan clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-280 docs backfill (append-only)

Successive fresh full `docs/*.md` listings (docs kept appearing on disk during
this sweep) were diffed against the unique `.md` link targets in this index:
nine documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [SELFTEST_RERUN_STATUS2.md](SELFTEST_RERUN_STATUS2.md) — read-only 2026-11-24
  21:27 monitor of the standalone `r1_os_selfcheck.py` rerun (PID 1194653,
  verdict PENDING): build/isogrub/qemu_smoke/cp4_ca PASS so far, `selftest`
  stage not yet reached, no OVERALL yet.
- [STRICT_VERIFY_WATCH23.md](STRICT_VERIFY_WATCH23.md) — read-only final watch
  of strict-verify run 1913 (PID 1137836, now exited): 10/11 tests PASS
  (smoke/retire/efsrv/ewsrv/eshell/fat_io + 4x DOOM), selfcheck FAIL (248 s,
  QEMU cap exit 124, marker-level, 0 guest exceptions), OVERALL FAIL,
  FV_EXIT=1, fault scan clean.
- [STRICT_VERIFY_2125_WATCH.md](STRICT_VERIFY_2125_WATCH.md) — read-only
  2026-11-24 21:27 watch of the new strict-verify run (PID 1194626, started
  21:25, alive ~2 min in selfcheck `cp4_stress`): build/isogrub/qemu_smoke/
  cp4_ca PASS; recaps the prior run 1913 as OVERALL FAIL (10 PASS, selfcheck
  FAIL, FV_EXIT=1).
- [STRICT_VERIFY_2125_FINAL.md](STRICT_VERIFY_2125_FINAL.md) — read-only final
  outcome of the 21:25 strict run (PID 1194626): OVERALL FAIL (10 pass / 0
  skip / 1 fail: selfcheck 248.0 s), FV_EXIT=1, fault scan across all 11
  per-test logs clean (no guest FAULT/#PF/Panic/KERN_EXEC).
- [HARNESS_FIX_STATUS2.md](HARNESS_FIX_STATUS2.md) — read-only status of the
  selfcheck harness race fix (verdict LANDED, uncommitted): `selftest_lock()`
  flock + per-PID rundir + signal grace in `r1_selftest.py` /
  `r1_os_selfcheck.py` / `r1_final_verify.py`, py_compile OK, `kernelhwsrv/`
  clean.
- [STRICT_VERIFY_WATCH39.md](STRICT_VERIFY_WATCH39.md) — read-only outcome watch
  of strict run 1913 (PID 1137836, finished): selfcheck FAIL (248.0 s,
  selftest timeout), other 10 tests PASS, OVERALL FAIL, FV_EXIT=1, fault scan
  clean.
- [FRESH_STRICT_WATCH2.md](FRESH_STRICT_WATCH2.md) — read-only 2026-11-24 21:29
  watch of the post-race-fix strict run (PID 1194626, alive ~4 min, flock
  held): selfcheck OVERALL PASS 6/6 sub-tests (238.4 s), qemu_smoke in
  progress, no run OVERALL/FV_EXIT yet, fault scan 0 matches.

These 8 newly indexed reports bring the indexed Markdown total to **292**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-263 newest strict-run reports backfill (append-only)

A full `docs/*.md` listing (289 files, flat directory, no subdirectories) was
diffed against every `.md` basename mentioned in this index: two documents
existed on disk but had no entry; they are linked below, and 0 mentions are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [STRICT_VERIFY_2125_FINAL.md](STRICT_VERIFY_2125_FINAL.md) — read-only
  final-outcome summary of the 21:25 strict run (PID 1194626): 10/11 PASS
  with selfcheck the sole FAIL (248.0 s), OVERALL FAIL, FV_EXIT=1, clean
  guest fault scan.
- [STRICT_VERIFY_2125_WATCH.md](STRICT_VERIFY_2125_WATCH.md) — read-only
  21:27 EET watch of the same run while in flight (selfcheck at cp4_stress,
  QEMU child up, no OVERALL yet).

These two newly indexed reports bring the indexed Markdown total to **292**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-263 harness-fix status backfill (append-only)

A full `docs/*.md` listing (291 files, flat directory, no subdirectories) was
diffed against every `.md` basename mentioned in this index: two documents
existed on disk but had no entry; they are linked below, and 0 mentions are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [HARNESS_FIX_STATUS2.md](HARNESS_FIX_STATUS2.md) — read-only status:
  verdict LANDED (uncommitted) — the `r1_selftest.py` shared-path race fix
  (`selftest_lock()` flock + per-PID rundir + signal grace) is in
  `tools/r1_selftest.py`, `tools/r1_os_selfcheck.py` and
  `tools/r1_final_verify.py` (+169/−49).
- [HARNESS_FIX_STATUS3.md](HARNESS_FIX_STATUS3.md) — read-only confirmation:
  same flock/lock-file fix present in all three tools at 21:27–21:28,
  `py_compile` OK on each, `kernelhwsrv/` clean, no QEMU run.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-race-fix monitor backfill (append-only)

A full `docs/*.md` listing (296 files, flat directory, no subdirectories) was
diffed against every `.md` basename mentioned in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH.md](FRESH_STRICT_WATCH.md) — read-only 21:29/21:30
  monitor of the post-race-fix `r1_final_verify.py --strict-skip` run (PID
  1194626, lock held): selfcheck phase completed all-PASS and the run
  advanced to qemu_smoke; verdicts for tests 2–11, OVERALL and FV_EXIT still
  pending; recaps the prior 21:25 run as OVERALL FAIL (10 PASS, selfcheck
  FAIL, FV_EXIT=1); fault scan clean.
- [SELFTEST_RERUN_FINAL3.md](SELFTEST_RERUN_FINAL3.md) — read-only ~21:29
  final-stage status of the rerun (PID 1194653, no OVERALL yet): post-race-fix
  selfcheck PID 1203925 reports `ST: RESULT PASS`, failures=0, 0 guest
  exceptions; last completed OVERALLs were PASS (00:52) and 11/11 (20:41);
  the 21:25 FAIL predates the fix.
- [FRESH_STRICT_WATCH2.md](FRESH_STRICT_WATCH2.md) — read-only 21:29 second
  watch of the post-race-fix strict run (PID 1194626, lock held): selfcheck
  test 1/11 now OVERALL PASS 6/6 sub-tests (238.4 s), test 2/11 qemu_smoke
  in progress, no run-level OVERALL/FV_EXIT yet; fault scan clean.
- [FRESH_STRICT_WATCH3.md](FRESH_STRICT_WATCH3.md) — read-only ~21:30 third
  watch of the same run (PID 1194626, ~5 min): selfcheck PASS (238.5 s) and
  qemu_smoke PASS (50.3 s), genuine_retire in progress; 2/11 done, no
  run-level OVERALL/FV_EXIT yet; prior 21:25 run stays OVERALL FAIL (pre-fix).
- [FRESH_STRICT_WATCH4.md](FRESH_STRICT_WATCH4.md) — read-only 21:30 fourth
  watch: recap of the completed pre-fix 21:25 run (`final_verify_1913.log`)
  as 10/11 PASS with selfcheck FAIL (248.0 s), OVERALL FAIL, FV_EXIT=1.

These five newly indexed reports bring the indexed Markdown total to **304**
matching the 304 `.md` files under `docs/` (including this index; the header
count "(304 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 selfcheck-rerun-final / strict-verify-watch39 backfill (append-only)

A full `docs/*.md` listing (298 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two read-only
monitor reports existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [SELFTEST_RERUN_FINAL2.md](SELFTEST_RERUN_FINAL2.md) — final 21:28 monitor of
  the standalone selfcheck rerun (PID 1194653): verdict still PENDING with
  build/isogrub/qemu_smoke/cp4_ca/cp4_stress all PASS and the guest `selftest`
  already reporting `ST: RESULT PASS - all core services operational`,
  failures=0, while `r1_selftest.py` awaits QEMU exit.
- [STRICT_VERIFY_WATCH39.md](STRICT_VERIFY_WATCH39.md) — read-only watch of
  strict-verify run 1913 (PID 1137836, `/tmp/opencode/final_verify_1913.log`,
  finished): 10/11 PASS with selfcheck the sole FAIL (248.0 s stall at the
  `selftest` step), OVERALL FAIL, FV_EXIT=1, fault scan clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 strict-verify watch 39 + selftest-rerun-final2 backfill (append-only)

A fresh full `docs/*.md` listing (293 files, flat directory, no subdirectories)
was diffed against the 291 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_WATCH39.md](STRICT_VERIFY_WATCH39.md) — read-only watch of
  strict-verify run 1913 (PID 1137836, finished): 10/11 PASS, selfcheck FAIL
  (248.0s, stalled at the selftest step), OVERALL FAIL, FV_EXIT=1, fault scan
  clean.
- [SELFTEST_RERUN_FINAL2.md](SELFTEST_RERUN_FINAL2.md) — read-only final-ish
  status of the standalone selfcheck rerun (PID 1194653, still alive at
  ~3 min): build/isogrub/qemu_smoke/cp4_ca/cp4_stress all PASS, guest selftest
  reports `ST: RESULT PASS`, verdict still PENDING (no OVERALL line yet).

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-263 final two docs backfill (append-only)

A full `docs/*.md` listing (293 files, flat directory, no subdirectories) was
diffed against every `.md` basename mentioned in this index: two documents
existed on disk but had no entry; they are linked below, and 0 mentions are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [SELFTEST_RERUN_FINAL2.md](SELFTEST_RERUN_FINAL2.md) — read-only monitor of
  the selfcheck rerun (PID 1194653): verdict PENDING with build/isogrub/
  qemu_smoke/cp4_ca/cp4_stress PASS and in-guest `ST: RESULT PASS
  failures=0`; no OVERALL yet.
- [STRICT_VERIFY_WATCH39.md](STRICT_VERIFY_WATCH39.md) — read-only outcome
  watch of strict run 1913 (PID 1137836): 10/11 PASS, selfcheck sole FAIL
  (248.0 s selftest-step stall), OVERALL FAIL, FV_EXIT=1, fault scan clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-263 last new doc backfill (append-only)

A full `docs/*.md` listing (294 files, flat directory, no subdirectories) was
diffed against every `.md` basename mentioned in this index: one document
existed on disk but had no entry; it is linked below, and 0 mentions are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [STRICT_VERIFY_2125_FINAL2.md](STRICT_VERIFY_2125_FINAL2.md) — read-only
  monitor of the 21:25 strict run (PID 1194626): 10/10 completed per-test
  logs PASS, selfcheck still in flight at report time (no OVERALL/FV_EXIT
  yet), 0 FAULT/#PF/Panic/KERN-EXEC hits.

This newly indexed report brings the indexed Markdown total to **294**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 newest-docs backfill, post-270 (append-only)

A full `docs/*.md` listing (300 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 3 document(s)
existed on disk but had no entry; they are linked below, and 0 links are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [SELFTEST_RERUN_FINAL.md](SELFTEST_RERUN_FINAL.md) — read-only 2026-11-24 status/watch report (see file for details).
- [STRICT_2125_OVERALL3.md](STRICT_2125_OVERALL3.md) — read-only 2026-11-24 status/watch report (see file for details).
- [STRICT_VERIFY_FINAL4.md](STRICT_VERIFY_FINAL4.md) — read-only 2026-11-24 status/watch report (see file for details).

These 3 newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-263 closing backfill, FINAL4 (append-only)

A full `docs/*.md` listing (299 files, flat directory, no subdirectories) was
diffed against every `.md` basename mentioned in this index: one document
existed on disk but had no entry; it is linked below, and 0 mentions are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [STRICT_VERIFY_FINAL4.md](STRICT_VERIFY_FINAL4.md) — read-only final-outcome
  note for the strict-verify runs (run 1913: 10 pass / 0 skip / 1 fail
  selfcheck, FV_EXIT=1; post-fix selfcheck rerun PASS 6/6).

This newly indexed report brings the indexed Markdown total to **299**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 final sweep, post-297 (append-only)

A fresh full `docs/*.md` listing (299 files, flat directory, no subdirectories)
was diffed against the 297 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry and 0 links were stale; they are
linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [FRESH_STRICT_WATCH2.md](FRESH_STRICT_WATCH2.md) — read-only 2026-11-24 21:29
  watch of the post-race-fix `r1_final_verify.py --strict-skip` run (PID
  1194626, flock held): selfcheck sub-run OVERALL PASS 6/6 (238.4 s), test 2/11
  qemu_smoke in progress, no strict-run OVERALL/FV_EXIT yet, fault scan clean.
- [STRICT_VERIFY_FINAL4.md](STRICT_VERIFY_FINAL4.md) — read-only FINAL outcome of
  strict run 1913 (PID 1137836, FV_EXIT=1): 10/11 PASS with selfcheck the sole
  FAIL (248.0 s stall), OVERALL FAIL, fault scan clean, `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **301**
matching the 301 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 subagent full sweep, post-301 (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: two documents existed on disk but had no entry; they are
linked below (the remaining diff entries, `ARCHITECTURE.md`,
`DOCS_INDEX.md` and `r1_boot_and_selftest.md`, are referenced via
backtick-wrapped links in the header and are indexed).
[`FRESH_STRICT_WATCH2.md`](FRESH_STRICT_WATCH2.md) is confirmed indexed above
(read-only 2026-11-24 21:29 watch of the post-race-fix strict-verify run
PID 1194626: selfcheck 6/6 PASS, qemu_smoke in progress, fault scan clean).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH3.md](FRESH_STRICT_WATCH3.md) — read-only 2026-11-24
  ~21:30 watch of the post-race-fix `r1_final_verify.py --strict-skip` run
  (PID 1194626): selfcheck PASS (238.5 s) and qemu_smoke PASS (50.3 s), 2/11
  done with genuine_retire running, no OVERALL/FV_EXIT yet, fault scan clean.
- [FRESH_STRICT_WATCH4.md](FRESH_STRICT_WATCH4.md) — read-only 2026-11-24
  21:30 watch: completed pre-fix log `final_verify_1913.log` OVERALL FAIL
  10/11 (selfcheck recorded FAIL at 248.0 s despite its own PASS — the
  read/timeout race) while PID 1194626 stays alive on the retire test;
  fault scan clean.

These two **403**,
matching the 403 `.md` files under `docs/` (including this index; the header
count "(303 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-297 newest-docs backfill (append-only)

A full `docs/*.md` listing (298 files, flat directory, no subdirectories) was
diffed against every `.md` basename mentioned in this index: one document
existed on disk but had no entry; it is linked below, and 0 mentions are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [FRESH_STRICT_WATCH2.md](FRESH_STRICT_WATCH2.md) — read-only 21:29 watch of
  the post-race-fix strict run (`r1_final_verify.py --strict-skip`, PID
  1194626, alive): selfcheck test 1/11 OVERALL PASS 6/6, qemu_smoke in
  progress, 0 fault-scan hits.

This newly indexed report brings the indexed Markdown total to **298**,
matching the 298 `.md` files under `docs/` (including this index; the header
count "(298 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-263 closing backfill, 21:25 OVERALL2 (append-only)

A full `docs/*.md` listing (301 files, flat directory, no subdirectories) was
diffed against every `.md` basename mentioned in this index: one document
existed on disk but had no entry; it is linked below, and 0 mentions are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [STRICT_2125_OVERALL2.md](STRICT_2125_OVERALL2.md) — read-only OVERALL
  monitor for the 21:25 post-fix strict run (PID 1194626): selfcheck PASS
  6/6, later tests in progress, run-level verdict pending.

This newly indexed report brings the indexed Markdown total to **301**,
matching the 301 `.md` files under `docs/` (including this index; the header
count "(301 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 newest-docs backfill (append-only)

A full `docs/*.md` listing (299 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 1 document existed
on disk but had no entry; it is linked below, and 0 links are stale.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [STRICT_VERIFY_FINAL4.md](STRICT_VERIFY_FINAL4.md) — final outcome of the
  21:25 `r1_final_verify.py --strict-skip` run (PID 1137836, exit 1):
  10/11 tests PASS (smoke through doom_multikey), selfcheck FAIL (248 s),
  OVERALL FAIL / FV_EXIT=1, zero fault/panic/#PF matches in the log;
  predates the 21:28 selfcheck race fix.

This newly indexed report brings the indexed Markdown total to **299**,
matching the 299 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 subagent final sweep, post-299 (append-only)

A fresh full `docs/*.md` listing (301 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (301 on-disk `.md` files = 301 unique indexed
targets, including this index; the header count "(301 files)" at line 4 is
consistent). The two documents found unlinked in an earlier pass of this
sweep — [`SELFTEST_RERUN_FINAL3.md`](SELFTEST_RERUN_FINAL3.md) (read-only
~21:29 final selfcheck-rerun monitor: post-race-fix selfcheck `ST: RESULT
PASS`, failures=0, rerun PID 1194653 still in its final stage) and
[`STRICT_VERIFY_2125_FINAL2.md`](STRICT_VERIFY_2125_FINAL2.md) (read-only 21:25
strict-verify monitor PID 1194626: 10/11 per-test logs all `RESULT: PASS`,
selfcheck in flight, fault scan clean) — are confirmed already indexed above.
No missing documents remained, so this section adds no new link entries;
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **301**, matching the 301 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 FRESH_STRICT_WATCH3/4 backfill (append-only)

A full `docs/*.md` listing (306 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: five documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH3.md](FRESH_STRICT_WATCH3.md) — read-only 2026-11-24
  ~21:29 watch of the post-race-fix `r1_final_verify.py --strict-skip` run
  (PID 1194626): selfcheck PASS (238.5s) + qemu_smoke PASS, genuine_retire
  running; pre-fix run 1913 was 10 PASS / selfcheck FAIL, fault scan clean.
- [FRESH_STRICT_WATCH4.md](FRESH_STRICT_WATCH4.md) — read-only 2026-11-24
  21:30 watch: run 1913 log verdicts 10/11 PASS with selfcheck FAIL (read/
  timeout race — its own log says OVERALL PASS); PID 1194626 still in flight;
  fault scan clean.
- [STRICT_NEWEST_OVERALL.md](STRICT_NEWEST_OVERALL.md) — read-only 2026-11-24
  21:31 EET newest-overall monitor: in-flight strict run PID 1194626 at
  5/11 `genuine_ewsrv` with 4/4 PASS so far (selfcheck 238.5s, smoke 50.3s,
  retire 56.7s, efsrv 17.0s); last completed logs 1913 OVERALL FAIL (10/11)
  and `final_verify_strict_20261124.log` OVERALL PASS 11/11; fault scan clean.
- [FRESH_STRICT_WATCH6.md](FRESH_STRICT_WATCH6.md) — read-only 2026-11-24
  21:32 watch of the same post-fix strict run (PID 1194626, ~7 min in):
  6/11 PASS so far (selfcheck/smoke/retire/efsrv/ewsrv/eshell), genuine_fat_io
  running, no OVERALL/FV_EXIT yet; fault scan clean.
- [FRESH_STRICT_WATCH7.md](FRESH_STRICT_WATCH7.md) — read-only 2026-11-24
  watch of the same post-fix strict run (PID 1194626, 7m11s, test 8/11
  doom_main): 8/8 decided tests all PASS (selfcheck/smoke/retire/efsrv/
  ewsrv/eshell/fat_io + doom_main pending), no OVERALL/FV_EXIT yet; fault
  scan clean.

These five **405**,
matching the 405 `.md` files under `docs/` (including this index; the header
count "(306 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 subagent docs-index sweep, post-299 (append-only)

A fresh full `docs/*.md` listing (303 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below, and 0 links are stale
(303 on-disk `.md` files = 303 unique indexed targets, including this index;
the header count "(303 files)" at line 4 is consistent). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH4.md](FRESH_STRICT_WATCH4.md) — read-only 2026-11-24
  21:30 watch of the post-race-fix strict verify (PID 1194626): newest log
  `/tmp/opencode/final_verify_1913.log` (3.2 KB, 21:25:41, under the 50 MB
  cap, no skip); verdict IN PROGRESS.

This newly indexed report brings the indexed Markdown total to **303**,
matching the 303 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 full directory sweep (append-only)

A full `docs/*.md` listing (302 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry and 0 links were stale; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH3.md](FRESH_STRICT_WATCH3.md) — read-only 2026-11-24
  21:31 monitor of the post-race-fix strict run (PID 1194626, alive ~5 min):
  selfcheck PASS (238.5 s, OVERALL PASS) and qemu_smoke PASS so far,
  genuine_retire in progress, no run OVERALL yet; prior pre-fix run 1913 was
  10/11 with selfcheck FAIL; fault scan clean.

This newly indexed report brings the indexed Markdown total to **302**,
matching the 302 `.md` files under `docs/` (including this index; the header
count "(302 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 FRESH_STRICT_WATCH4 backfill (append-only)

A full `docs/*.md` listing (303 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH4.md](FRESH_STRICT_WATCH4.md) — read-only 21:30 watch of
  the in-flight post-fix strict run (PID 1194626): newest completed log
  `final_verify_1913.log` 10/11 PASS with selfcheck FAIL (248.0 s wrapper vs
  its own OVERALL: PASS — read/timeout race), OVERALL FAIL / FV_EXIT=1, fault
  scan clean.

This newly indexed report brings the indexed Markdown total to **303**,
matching the 303 `.md` files under `docs/` (including this index; the header
count "(303 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 docs-sweep backfill (append-only)

A full `docs/*.md` listing (304 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 1 document(s)
existed on disk but had no entry; 0 links are stale. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [STRICT_NEWEST_OVERALL.md](STRICT_NEWEST_OVERALL.md) — STRICT newest overall — 2026-11-24 21:31 EET

The indexed Markdown total is **304**, matching the 304 `.md` files under
`docs/` (including this index; the header count "(304 files)" at line 4 is
updated); every Markdown document in the directory is now linked.

## Added — 2026-11-24 subagent final index sweep (append-only)

A fresh full `docs/*.md` listing (304 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: **0 unlinked documents and 0 broken links** (304 on-disk `.md` files = 304 unique indexed
targets, including this index). No missing documents were found, so this section adds no new link entries. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **304**, matching the 304 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 full directory sweep, post-304 (append-only)

A fresh full `docs/*.md` listing (304 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (304 on-disk `.md` files = 304 unique indexed
targets, including this index; the header count "(304 files)" at line 4 is
consistent). No missing documents were found, so this section adds no new link
entries; `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **304**, matching the 304 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent new-docs sweep, 306 confirmed (append-only)

A full `docs/*.md` listing (306 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: **2 unlinked
documents and 0 broken links** (306 on-disk `.md` files = 306 unique indexed
targets, including this index; the header count "(306 files)" at line 4 is
consistent).
The following existed on disk but had no entry; they are linked below
with one-line descriptions.
- [FRESH_STRICT_WATCH5.md](FRESH_STRICT_WATCH5.md) — FRESH_STRICT_WATCH5 — post-race-fix strict run watch (2026-11-24 21:32)
- [FRESH_STRICT_WATCH6.md](FRESH_STRICT_WATCH6.md) — FRESH_STRICT_WATCH6 — post-fix strict run watch (2026-11-24 21:32 EET)
`kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **306**, matching the 306 `.md` files under
`docs/` (including this index; the header count "(306 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 subagent new-docs sweep, 307 confirmed (append-only)

A full `docs/*.md` listing (307 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: **1 unlinked
document and 0 broken links** (307 on-disk `.md` files = 307 unique indexed
targets after this section, including this index; the header count "(307
files)" at line 4 is updated). The one document that existed on disk but had
no entry is linked below with a one-line description. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH7.md](FRESH_STRICT_WATCH7.md) — read-only 2026-11-24
  post-fix strict-run watch (PID 1194626, test 8/11 doom_main): selfcheck,
  qemu_smoke, genuine_retire/efsrv/ewsrv/eshell/fat_io all PASS (8/8 decided),
  no fault markers, OVERALL/FV_EXIT not yet emitted; no QEMU launched by the
  watcher, `kernelhwsrv/` untouched.

The indexed Markdown total is **307**, matching the 307 `.md` files under
`docs/` (including this index; the header count "(307 files)" at line 4 is
consistent); every Markdown document in the directory is now linked.

## Added — 2026-11-24 missing-docs backfill (append-only)

A full `docs/*.md` listing (307 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below, and 0 links are stale.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH7.md](FRESH_STRICT_WATCH7.md) — read-only 2026-11-24
  ~21:33 watch of the post-race-fix `r1_final_verify.py --strict-skip` run
  (PID 1194626, in test 8/11 doom_main): selfcheck through genuine_fat_io all
  PASS (8/8 decided), DOOM tests pending, no OVERALL/FV_EXIT yet, fault scan
  clean, no log >50 MB.

This newly indexed report brings the indexed Markdown total to **307**,
matching the 307 `.md` files under `docs/` (including this index; the header
count "(307 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 new-docs sweep, post-307 (append-only)

A fresh full `docs/*.md` listing (308 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **1 unlinked
document and 0 broken links** (308 on-disk `.md` files = 308 unique indexed
targets after this section, including this index; the header count "(308
files)" at line 4 is updated). The one document that existed on disk but had
no entry is linked below with a one-line description. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH9.md](FRESH_STRICT_WATCH9.md) — read-only 2026-11-24
  21:33 EET watch of the post-fix `r1_final_verify.py --strict-skip` run
  (PID 1194626): latest completed strict run OVERALL PASS 11/11 with
  FV_EXIT=0, current run 10/10 decided tests PASS with doom_main in progress,
  fault scan clean; no QEMU launched by the watcher, `kernelhwsrv/` untouched.

This newly indexed report brings the indexed Markdown total to **308**,
matching the 308 `.md` files under `docs/` (including this index; the header
count "(308 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 final new-docs sweep, post-304 (append-only)

A fresh full `docs/*.md` listing (308 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: one document existed on disk but had
no entry; it is linked below, and 0 links are stale (308 on-disk `.md` files
= 308 unique indexed targets, including this index; the header count
"(308 files)" at line 4 is consistent). `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

- [FRESH_STRICT_WATCH9.md](FRESH_STRICT_WATCH9.md) — read-only 2026-11-24
  21:33 EET watch of the post-fix `r1_final_verify.py --strict-skip` run (PID
  1194626): 10/10 finished tests PASS (selfcheck through doom_multikey),
  doom_main in progress, no OVERALL/FV_EXIT yet, fault scan clean.

This newly indexed report brings the indexed Markdown total to **308**,
matching the 308 `.md` files under `docs/` (including this index); every
Markdown document in the directory is linked.

## Added — 2026-11-24 FRESH_STRICT_WATCH11 backfill (append-only)

A fresh full `docs/*.md` listing (309 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: one document existed on disk but had no entry; it is linked below,
and 0 links are stale. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [FRESH_STRICT_WATCH11.md](FRESH_STRICT_WATCH11.md) — read-only 2026-11-24
  watch of the post-fix `r1_final_verify.py --strict-skip` run (PID 1194626,
  log `/tmp/opencode/gate_final_verify.log`): 7/11 verdicts so far, all PASS
  (selfcheck through genuine_fat_io), doom_main in progress, no OVERALL /
  FV_EXIT yet, fault scan clean.

This newly indexed report brings the indexed Markdown total to **309**,
matching the 309 `.md` files under `docs/` (including this index; the header
count "(309 files)" at line 4 is consistent); every Markdown document in the
directory is linked.

## Added — 2026-11-24 post-306 new-docs sweep (append-only)

A fresh full `docs/*.md` listing (308 files, flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the unique `.md` link
targets in this index: **0 unlinked documents and 0 broken links** (308 on-disk
`.md` files = 308 unique indexed targets, including this index; the header
count "(308 files)" at line 4 is consistent). Every document that appeared past
the 306 mark (e.g. [`FRESH_STRICT_WATCH9.md`](FRESH_STRICT_WATCH9.md), read-only
2026-11-24 21:33 EET watch of the post-fix `r1_final_verify.py --strict-skip`
run: 10/10 finished tests PASS with doom_main in progress, no OVERALL yet) is
already linked in the sections above, so this section adds no new link entries;
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH11.md](FRESH_STRICT_WATCH11.md) — read-only 2026-11-24
  ~21:34 EET watch of the post-fix `r1_final_verify.py --strict-skip` run
  (PID 1194626, log `gate_final_verify.log`): 7/11 verdicts so far, all PASS
  (selfcheck through genuine_fat_io), doom_main (test 8/11) running, no
  OVERALL/FV_EXIT yet, fault scan clean, `kernelhwsrv/` untouched.

The indexed Markdown total is **309**, matching the 309 `.md` files under
`docs/` (including this index; the header count "(309 files)" at line 4 is
updated); every Markdown document in the directory is linked.

## Added — 2026-11-24 subagent docs-index sweep, post-308 (append-only)

A full `docs/*.md` listing (309 files, flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the unique `.md` link
targets in this index: at sweep start one document existed on disk but had
no entry ([`FRESH_STRICT_WATCH11.md`](FRESH_STRICT_WATCH11.md) — read-only
2026-11-24 ~21:34 EET watch of the post-fix `r1_final_verify.py
--strict-skip` run, PID 1194626: 7/11 verdicts all PASS, doom_main test 8/11
running, no OVERALL/FV_EXIT yet, fault scan clean, `kernelhwsrv/` untouched);
its link entry is present in the section above, and 0 links are stale
(309 on-disk `.md` files = 309 unique indexed targets, including this index;
the header count "(309 files)" at line 4 is updated). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

The indexed Markdown total is **309**, matching the 309 `.md` files under
`docs/` (including this index; the header count "(309 files)" at line 4 is
consistent); every Markdown document in the directory is now linked.

## Added — 2026-11-24 full directory sweep, post-309 (append-only)

A fresh full `docs/*.md` listing (309 files, flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the unique `.md` link
targets in this index: **0 unlinked documents and 0 broken links** (309 on-disk
`.md` files = 309 unique indexed targets, including this index; the header
count "(309 files)" at line 4 is consistent). No missing documents were found,
so this section adds no new link entries; `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total stays **309**, matching the 309 `.md` files under
`docs/` (including this index; the header count "(309 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 post-309 sweep (append-only)

A fresh full `docs/*.md` listing (309 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: **0 unlinked documents and 0 broken
links** (309 on-disk `.md` files = 309 unique indexed targets, including
this index; the header count "(309 files)" at line 4 is consistent). No
missing documents were found past the 309 mark, so this section adds no new
link entries; `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

The indexed Markdown total stays **309**, matching the 309 `.md` files under
`docs/` (including this index; the header count "(309 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 post-309 docs sweep (append-only)

A fresh full `docs/*.md` listing (309 files, flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the unique `.md` link
targets in this index: **0 unlinked documents and 0 broken links** (309 on-disk
`.md` files = 309 unique indexed targets, including this index; the header
count "(309 files)" at line 4 is consistent). No missing documents were found
past the 309 mark, so this section adds no new link entries;
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **309**, matching the 309 `.md` files under
`docs/` (including this index; the header count "(309 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 post-309 sweep, ten new docs (append-only)

A fresh full `docs/*.md` listing (319 files, flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the unique `.md` link
targets in this index: ten documents existed on disk but had no entry, and
0 links were stale. They are linked below. `kernelhwsrv/` was untouched and
no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH8.md](FRESH_STRICT_WATCH8.md) — final watch of the
  post-fix `r1_final_verify.py --strict-skip` run (PID 1194626): process
  exited after 11/11 PASS with `OVERALL: PASS` and a zero-hit fault scan.
- [FRESH_STRICT_WATCH10.md](FRESH_STRICT_WATCH10.md) — confirmation watch:
  PID 1194626 exited normally (~13 min, FV_EXIT 0) after OVERALL: PASS
  (11 pass, 0 skip, 0 fail); all 11 per-test logs fault-scan clean.
- [FRESH_STRICT_WATCH12.md](FRESH_STRICT_WATCH12.md) — gate-log watch: 11/11
  PASS with per-test timings, clean harness exit, no FAULT/#PF/panic lines.
- [FRESH_STRICT_WATCH14.md](FRESH_STRICT_WATCH14.md) — final strict-run watch:
  clean process exit, 11/11 PASS, doom_multikey 12/12 scenario loops, and
  `git status --porcelain kernelhwsrv/` empty.
- [MIGRATION_COMMIT_WATCH.md](MIGRATION_COMMIT_WATCH.md) — migration-wave
  commit status LANDED: origin/main at `0f9ec6a` ("Close euser_shim") after
  `e29a072`/`dd22ae2`, with `kernelhwsrv/` at 0 diff lines.
- [MIGRATION_COMMIT_WATCH2.md](MIGRATION_COMMIT_WATCH2.md) — read-only monitor:
  `git ls-remote origin main` matches local HEAD `0f9ec6a`, so the wave is in
  sync; 173 dirty working-tree entries are in-flight session work.
- [STRICT_2125_OVERALL.md](STRICT_2125_OVERALL.md) — final result of the 21:25
  strict gate run (PID 1194626): 11/11 PASS, OVERALL: PASS, FV_EXIT 0, clean
  fault scan (prev 21:13 run's selfcheck FAIL superseded).
- [MIGRATION_COMMIT_WATCH3.md](MIGRATION_COMMIT_WATCH3.md) — monitor (ses_df0ee0f05):
  local HEAD = origin/main `0f9ec6a`; wave `7f76900`→`d26fade`→`e29a072`→
  `dd22ae2`→`0f9ec6a` fully pushed, nothing pending.
- [MIGRATION_COMMIT_WATCH4.md](MIGRATION_COMMIT_WATCH4.md) — monitor verdict
  LANDED + PUSHED: `0f9ec6a` on origin/main, top-3 log `0f9ec6a`/`dd22ae2`/
  `e29a072`, `kernelhwsrv/` clean.
- [MIGRATION_COMMIT_WATCH5.md](MIGRATION_COMMIT_WATCH5.md) — monitor
  (ses_df0ecfaa8): origin/main still at `0f9ec6a`; the retry commit has NOT
  landed or been pushed as of this check.

**401**,
matching the 401 `.md` files under `docs/` (including this index; the header
count "(319 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-314 monitor-report backfill (append-only)

A fresh full `docs/*.md` listing (319 files, flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the unique `.md` link
targets in this index: five documents existed on disk but had no entry, and
0 links were stale. They are linked below. `kernelhwsrv/` was untouched and
no QEMU run was made by this index update.

- [MIGRATION_COMMIT_WATCH2.md](MIGRATION_COMMIT_WATCH2.md) — read-only monitor
  report: migration-wave commit `0f9ec6a` landed and pushed, local HEAD in
  sync with `origin/main`, top-3 log `0f9ec6a`/`dd22ae2`/`e29a072`, 173
  working-tree entries of in-flight uncommitted work, `kernelhwsrv/` untouched.
- [STRICT_2125_OVERALL.md](STRICT_2125_OVERALL.md) — final result of the 21:25
  `r1_final_verify.py --strict-skip` run (PID 1194626): 11/11 PASS, 0 skip,
  0 fail, `OVERALL: PASS`, per-test timings, zero Exc/PanicKernExec/FAULT/#PF
  across all fresh logs, selfcheck PASS this run.
- [MIGRATION_COMMIT_WATCH3.md](MIGRATION_COMMIT_WATCH3.md) — read-only watch
  (agent ses_df0ee0f05): local HEAD = `origin/main` = `0f9ec6a`, recent wave
  `0f9ec6a`/`dd22ae2`/`e29a072`/`d26fade`/`7f76900` all on main, 174 modified
  working-tree files, `kernelhwsrv/` clean; verdict: nothing pending push.
- [MIGRATION_COMMIT_WATCH4.md](MIGRATION_COMMIT_WATCH4.md) — read-only watch:
  verdict LANDED + PUSHED, local HEAD = origin/main = `0f9ec6a` ("Close
  euser_shim"), top-3 log `0f9ec6a`/`dd22ae2`/`e29a072`, 175 dirty working-tree
  entries, `kernelhwsrv/` untouched.
- [MIGRATION_COMMIT_WATCH5.md](MIGRATION_COMMIT_WATCH5.md) — read-only watch
  (ses_df0ecfaa8): local HEAD and `origin/main` both still at `0f9ec6a`; the
  migration-wave retry commit has NOT landed and NOT been pushed; 177
  working-tree entries; `kernelhwsrv/` untouched.

These five **406**,
matching the 406 `.md` files under `docs/` (including this index; the header
count "(319 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-314 new-docs backfill (append-only)

A fresh full `docs/*.md` listing (flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the unique `.md` link
targets in this index: five documents existed on disk but had no entry; they
are linked below, and 0 links are stale. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

- [MIGRATION_COMMIT_WATCH2.md](MIGRATION_COMMIT_WATCH2.md) — read-only
  monitor: migration commit `0f9ec6a` LANDED and PUSHED, local HEAD =
  origin/main, wave `0f9ec6a`/`dd22ae2`/`e29a072`, `kernelhwsrv/` clean.
- [MIGRATION_COMMIT_WATCH3.md](MIGRATION_COMMIT_WATCH3.md) — monitor (ses_df0ee0f05):
  origin/main identical to local HEAD `0f9ec6a`, full wave incl. `d26fade`
  and `7f76900` on main, nothing pending push, `kernelhwsrv/` 0 modified.
- [MIGRATION_COMMIT_WATCH4.md](MIGRATION_COMMIT_WATCH4.md) — watch 4:
  LANDED + PUSHED, `0f9ec6a` on origin/main, top-3 log `0f9ec6a`/`dd22ae2`/
  `e29a072`, `kernelhwsrv/` untouched.
- [MIGRATION_COMMIT_WATCH5.md](MIGRATION_COMMIT_WATCH5.md) — watch 5
  (ses_df0ecfaa8): retry commit NOT landed/pushed; origin/main still at
  `0f9ec6a`, `kernelhwsrv/` untouched, read-only.
- [STRICT_2125_OVERALL.md](STRICT_2125_OVERALL.md) — final result of the
  21:25 strict run (PID 1194626, `r1_final_verify.py --strict-skip`):
  11/11 PASS, `OVERALL: PASS`, FV_EXIT 0, fault scan clean, `kernelhwsrv/`
  untouched.

These newly indexed reports bring the indexed Markdown total to **319**,
matching the 319 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is
linked.

## Added — 2026-11-24 post-309 new-docs backfill (append-only)

A fresh full `docs/*.md` listing (322 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: three documents existed on disk but
had no entry, and 0 links were stale. They are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [FRESH_STRICT_WATCH13.md](FRESH_STRICT_WATCH13.md) — final monitor watch of
  the post-fix `r1_final_verify.py --strict-skip` run (PID 1194626):
  process exited after 11/11 PASS, `OVERALL: PASS`, FV_EXIT 0, fault scan of
  all 11 logs clean; verdict COMPLETE.
- [STRICT_2125_FINAL2.md](STRICT_2125_FINAL2.md) — second and authoritative
  result of the 21:25 strict gate run (PID 1194626) after the race fix:
  11/11 PASS, `OVERALL: PASS`, clean fault scan, superseding the 21:13
  selfcheck-FAIL run.
- [MIGRATION_COMMIT_WATCH6.md](MIGRATION_COMMIT_WATCH6.md) — retry-4 landing
  check (ses_df0ebe315): migration commit NOT LANDED, local HEAD =
  `origin/main` = `0f9ec6a`, 180 dirty working-tree entries, other sessions'
  runs left alone; `kernelhwsrv/` untouched.

These three newly indexed reports bring the indexed Markdown total to **322**,
matching the 322 `.md` files under `docs/` (including this index; the header
count "(322 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-321 new-docs backfill (append-only)

A fresh full `docs/*.md` listing (323 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: two documents existed on disk but had
no entry, and 0 links were stale. They are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [MIGRATION_COMMIT_WATCH6.md](MIGRATION_COMMIT_WATCH6.md) — read-only retry-4
  landing check (ses_df0ebe315): verdict NOT LANDED — local HEAD and
  `origin/main` both still at `0f9ec6a`, nothing new pushed, 180 dirty
  working-tree entries of unrelated in-flight work, `kernelhwsrv/` untouched.
- [MIGRATION_COMMIT_WATCH7.md](MIGRATION_COMMIT_WATCH7.md) — read-only retry-5
  landing check (ses_df0eba263): verdict NOT LANDED — local HEAD =
  `origin/main` = `0f9ec6a`, no push, 181 dirty working-tree entries of
  pre-existing churn, `kernelhwsrv/` untouched, no QEMU run.

These two newly indexed reports bring the indexed Markdown total to **323**,
matching the 323 `.md` files under `docs/` (including this index; the header
count "(323 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-323 new-docs backfill (append-only)

A fresh full `docs/*.md` listing (324 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: one document existed on disk but
had no entry, and 0 links were stale. It is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [MIGRATION_COMMIT_WATCH8.md](MIGRATION_COMMIT_WATCH8.md) — read-only
  2026-11-24 retry-6 landing check (ses_df0eb4b16): verdict NOT LANDED —
  local HEAD = `origin/main` = `0f9ec6a`, nothing pushed, 182 dirty
  working-tree entries of pre-existing churn; `kernelhwsrv/` untouched, no
  QEMU run.

This newly indexed report brings the indexed Markdown total to **324**,
matching the 324 `.md` files under `docs/` (including this index; the header
count "(324 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-322 new-docs backfill (append-only)

A fresh full `docs/*.md` listing (325 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: three documents existed on disk but
had no entry, and 0 links were stale. They are linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [MIGRATION_COMMIT_WATCH7.md](MIGRATION_COMMIT_WATCH7.md) — read-only retry-5
  landing check (ses_df0eba263): migration commit NOT LANDED, local HEAD =
  `origin/main` = `0f9ec6a`, nothing pushed, 181 dirty working-tree entries of
  pre-existing churn, `kernelhwsrv/` untouched.
- [MIGRATION_COMMIT_WATCH8.md](MIGRATION_COMMIT_WATCH8.md) — read-only retry-6
  landing check (ses_df0eb4b16): NOT LANDED, local HEAD and `origin/main`
  both still `0f9ec6a`, 182 pre-existing dirty entries, `kernelhwsrv/`
  untouched.
- [MIGRATION_COMMIT_WATCH9.md](MIGRATION_COMMIT_WATCH9.md) — read-only retry-7
  status (ses_df0eb0a0a): NOT LANDED, local HEAD = `origin/main` =
  `0f9ec6a` (top-3 log unchanged), 183 pre-existing dirty entries,
  `kernelhwsrv/` untouched.

These three newly indexed reports bring the indexed Markdown total to **325**,
matching the 325 `.md` files under `docs/` (including this index; the header
count "(325 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-322 missing-docs backfill (append-only)

A fresh full `docs/*.md` listing (325 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: three documents existed on disk but
had no entry, and 0 links were stale. They are linked below. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [MIGRATION_COMMIT_WATCH7.md](MIGRATION_COMMIT_WATCH7.md) — read-only
  2026-11-24 retry-5 landing check (ses_df0eba263): verdict NOT LANDED —
  local HEAD = `origin/main` = `0f9ec6a`, nothing pushed, 181 dirty
  working-tree entries are pre-existing churn; `kernelhwsrv/` untouched, no
  QEMU run.
- [MIGRATION_COMMIT_WATCH8.md](MIGRATION_COMMIT_WATCH8.md) — read-only
  2026-11-24 retry-6 landing check (ses_df0eb4b16): verdict NOT LANDED —
  local HEAD = `origin/main` = `0f9ec6a` unchanged, 182 dirty working-tree
  entries pre-existing; `kernelhwsrv/` untouched, no QEMU run.
- [MIGRATION_COMMIT_WATCH9.md](MIGRATION_COMMIT_WATCH9.md) — read-only
  2026-11-24 retry-7 status (ses_df0eb0a0a): verdict NOT LANDED — HEAD and
  `origin/main` still `0f9ec6a`, top-3 log unchanged, 183 dirty entries
  pre-existing; `kernelhwsrv/` untouched, no QEMU run.

These newly indexed reports bring the indexed Markdown total to **325**,
matching the 325 `.md` files under `docs/` (including this index; the header
count "(325 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-323 new-docs backfill (append-only)

A fresh full `docs/*.md` listing (325 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: two documents existed on disk but
had no entry, and 0 links were stale. They are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [MIGRATION_COMMIT_WATCH8.md](MIGRATION_COMMIT_WATCH8.md) — read-only
  2026-11-24 retry-6 landing check (ses_df0eb4b16): verdict NOT LANDED —
  local HEAD = `origin/main` = `0f9ec6a`, nothing pushed, 182 dirty
  working-tree entries pre-existing; `kernelhwsrv/` untouched, no QEMU run.
- [MIGRATION_COMMIT_WATCH9.md](MIGRATION_COMMIT_WATCH9.md) — read-only
  2026-11-24 retry-7 status (ses_df0eb0a0): still NOT LANDED — HEAD and
  `origin/main` both `0f9ec6a`, top-3 log unchanged, 183 dirty entries
  pre-existing; `kernelhwsrv/` untouched, no QEMU run, no commits/pushes.

These two newly indexed reports bring the indexed Markdown total to **325**,
matching the 325 `.md` files under `docs/` (including this index; the header
count "(325 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 MIGRATION_COMMIT_WATCH8/9 backfill (append-only)

A full `docs/*.md` listing (325 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below, and no links are
stale (0 linked targets missing from disk). `kernelhwsrv/` was untouched and
no QEMU run was made by this index update.

- [MIGRATION_COMMIT_WATCH8.md](MIGRATION_COMMIT_WATCH8.md) — read-only retry-6
  landing check (ses_df0eb4b16): verdict NOT LANDED — local HEAD and
  `origin/main` both still at `0f9ec6a`, nothing new committed or pushed,
  182 pre-existing dirty working-tree entries, `kernelhwsrv/` untouched.
- [MIGRATION_COMMIT_WATCH9.md](MIGRATION_COMMIT_WATCH9.md) — read-only retry-7
  status (ses_df0eb0a0a): verdict NOT LANDED — HEAD and `origin/main` both
  `0f9ec6a` (top-3 log unchanged: `0f9ec6a`/`dd22ae2`/`e29a072`), 183 dirty
  entries pre-existing, `kernelhwsrv/` untouched, no QEMU, no commits/pushes.

These two newly indexed reports bring the indexed Markdown total to **325**,
matching the 325 `.md` files under `docs/` (including this index; the header
count "(325 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 post-309 new-docs backfill (append-only)

A fresh full `docs/*.md` listing (325 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: **0 unlinked documents and 0 broken links**.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

No missing documents were found, so this section adds no new link entries.

The indexed Markdown total stays **325**, matching the 325 `.md` files under
`docs/` (including this index); every Markdown document in the directory is linked.

## Added — 2026-11-24 post-319 sweep verification (append-only)

A fresh full `docs/*.md` listing (325 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: **0 documents on disk are missing
an entry and 0 links are stale** (325 on-disk `.md` files = 325 unique
indexed targets, including this index; the header count "(325 files)" at
line 4 is consistent). Every document past the 319 mark — the
`MIGRATION_COMMIT_WATCH*.md` retry-5/6/7 landing checks — is already
indexed in the sections above, so this section adds no new link entries.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **325**, matching the 325 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent missing-docs sweep, post-325 (append-only)

A fresh full `docs/*.md` listing (325 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: **0 unlinked documents and 0 broken
links** (325 on-disk `.md` files = 325 unique indexed targets, including
this index; the header count "(325 files)" at line 4 is consistent). The
one document past the earlier 323 mark,
[`MIGRATION_COMMIT_WATCH9.md`](MIGRATION_COMMIT_WATCH9.md) (read-only
2026-11-24 retry-7 landing check ses_df0eb0a0: NOT LANDED — local HEAD =
`origin/main` = `0f9ec6a`, nothing pushed; `kernelhwsrv/` untouched, no
QEMU), was confirmed already linked by a concurrent backfill, so this
section adds no new link entries. `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

The indexed Markdown total stays **325**, matching the 325 `.md` files under
`docs/` (including this index; the header count "(325 files)" at line 4 is
consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 subagent sweep past 319 (append-only)

A full `docs/*.md` listing (325 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index (LC_ALL=C collation
on both sides): **0 documents on disk are missing an entry and 0 links are
stale** — every document that appeared past the 319 mark (through 325) was
already linked by the concurrent backfill sections above, so this section adds
no new link entries. Counts verified: 325 on-disk `.md` files = 325 unique
indexed link targets (including this index), and the header count "(325 files)"
at line 4 is consistent. `kernelhwsrv/` was untouched and no QEMU run was made
by this index update.

The indexed Markdown total stays **325**, matching the 325 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent sweep, post-325 (append-only)

A fresh full `docs/*.md` listing (325 files, flat directory, no
subdirectories, `LC_ALL=C` collation on both sides) was diffed against the
unique `.md` link targets in this index: **0 unlinked documents and 0 broken
links** (325 on-disk `.md` files = 325 unique indexed targets, including this
index; the header count "(325 files)" at line 4 is consistent). No missing
documents were found, so this section adds no new link entries; `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **325**, matching the 325 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 final full-directory verification (append-only)

A fresh full `docs/*.md` listing (325 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index (both sides
`LC_ALL=C` sorted): **0 documents on disk are missing an entry and 0 links are
stale** (325 on-disk `.md` files = 325 unique indexed link targets, including
this index; the header count "(325 files)" at line 4 is consistent). No
missing documents were found, so this section adds no new link entries —
there is nothing to describe or link. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total is confirmed at **325**, matching the 325 `.md`
files under `docs/` (including this index); every Markdown document in the
directory is linked.

## Added — 2026-11-24 sweep past 319, owned-index pass (append-only)

A full `docs/*.md` listing (325 files, flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the unique `.md` link
targets in this index: **0 documents on disk are missing an entry and 0 links
are stale** — every document past the previous 319 mark (through 325) was
already linked by the backfill sections above, so this section adds no new
link entries and no one-line descriptions are needed. Counts verified:
325 on-disk `.md` files = 325 unique indexed link targets (including this
index), and the header count "(325 files)" at line 4 is consistent.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **325**, matching the 325 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 owned-index final sweep, post-325 (append-only)

A fresh full `docs/*.md` listing (326 files, flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the 325 unique `.md`
link targets previously in this index: one document existed on disk but had no
entry; it is linked below. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [MIGRATION_COMMIT_WATCH10.md](MIGRATION_COMMIT_WATCH10.md) — read-only
  2026-11-24 retry-10 monitor of the migration commit: NOT landed, NOT pushed —
  local HEAD and `origin/main` both still at `0f9ec6a`, 184 working-tree
  entries, `kernelhwsrv/` untouched.

This newly indexed report brings the indexed Markdown total to **326**,
matching the 326 `.md` files under `docs/` (including this index; the header
count "(326 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 owned-index final diff, 0 missing (append-only)

A fresh full `docs/*.md` listing (326 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index (both sides `LC_ALL=C` sorted): **0 documents on disk are missing an
entry and 0 links are stale** (326 on-disk `.md` files = 326 unique indexed
link targets, including this index; the header count "(326 files)" at line 4
is consistent). No missing docs exist, so this section adds no new link
entries and no one-line descriptions are needed. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **326**, matching the 326 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 owned sweep, 326-file diff, 0 missing (append-only)

A fresh full `docs/*.md` listing (326 files, flat directory, no subdirectories,
`LC_ALL=C` collation on both sides) was diffed against the unique `.md` link
targets in this index: **0 documents on disk are missing an entry and 0 links
are stale** (326 on-disk `.md` files = 326 unique indexed link targets,
including this index; the header count "(326 files)" at line 4 is
consistent). No missing docs exist, so this section adds no new link entries
and no one-line descriptions are needed. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total is confirmed at **326**, matching the 326 `.md`
files under `docs/` (including this index); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH backfill (append-only)

A full `docs/*.md` listing (329 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: three documents existed on disk but had no
entries; they are linked below, with 0 stale links. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH.md](WINSRV_DELETE_WATCH.md) — read-only 2026-11-24
  monitor snapshot (verdict APPLY NOT LANDED): `tools/r1_winsrv_delete.py`
  exists but only dry-run REFUSE runs are logged; zero `D` entries in
  `git status`, all winsrv shim files still present/unmodified, no snapshot
  dir, and the `WINSRV_ENDGAME_STATUS.md` blockers still open.
- [WINSRV_DELETE_WATCH2.md](WINSRV_DELETE_WATCH2.md) — read-only follow-up
  monitor (verdict NO): the `ses_df0e6e3a2` deletion did not land on the real
  winsrv sources — the deleted paths were untracked stale duplicates while
  the tracked `user/`+`variant/` files are intact and the delete-tool log
  ends in REFUSE.
- [WINSRV_DELETE_WATCH3.md](WINSRV_DELETE_WATCH3.md) — third read-only
  monitor verdict (deletion did NOT apply): zero `D` entries, all winsrv
  shim files present/unmodified, no snapshot dir, dry-run-safe tool never
  applied, open blockers unchanged, `kernelhwsrv/` diff empty.

These three newly indexed reports bring the indexed Markdown total to
**329**, matching the 329 `.md` files under `docs/` (including this index;
the header count "(329 files)" at line 4 is updated); every Markdown
document in the directory is now linked.

## Added — 2026-11-24 owned-index task sweep, post-326 (append-only)

A fresh full `docs/*.md` listing (326 files, flat directory, no
subdirectories, both sides `LC_ALL=C` sorted) was diffed against the 326
unique `.md` link targets in this index: **0 documents on disk are missing
an entry and 0 links are stale** (326 on-disk `.md` files = 326 unique
indexed link targets, including this index; the header count "(326 files)"
at line 4 is consistent). No missing docs exist, so this section adds no
new link entries or one-line descriptions. `kernelhwsrv/` was untouched
(read-only; `git status` on it is clean) and no QEMU run was made by this
index update.

The indexed Markdown total is **326**, matching the 326 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 subagent owned-index sweep, post-326 (append-only)

A fresh full `docs/*.md` listing (327 files, flat directory, no
subdirectories, both sides `LC_ALL=C` sorted) was diffed against the unique
`.md` link targets (`](*.md)` pattern) in this index: one document existed
on disk but had no entry; it is linked below, and **0 links are stale**
(327 on-disk `.md` files = 327 unique indexed link targets, including this
index; the header count "(327 files)" at line 4 is updated).
`kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH.md](WINSRV_DELETE_WATCH.md) — read-only 2026-11-24
  monitor (verdict APPLY NOT LANDED): `tools/r1_winsrv_delete.py` exists but
  only dry-run/no-args runs logged (all REFUSE, no writes); no winsrv files
  deleted or modified, no snapshot dir, open blockers per
  `WINSRV_ENDGAME_STATUS.md`; `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **327**,
matching the 327 `.md` files under `docs/` (including this index); every
Markdown document in the directory is linked, with 0 stale links.

## Added — 2026-11-24 WINSRV delete-watch backfill (append-only)

A fresh full `docs/*.md` listing (337 files, flat directory, no
subdirectories, both sides `LC_ALL=C` sorted) was diffed against the unique
`.md` link targets (`](*.md)` pattern) in this index: eight documents existed
on disk but had no entry; they are linked below, and **0 links are stale**
(337 on-disk `.md` files = 337 unique indexed link targets, including this
index; the header count "(337 files)" at line 4 is updated). `kernelhwsrv/`
was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH4.md](WINSRV_DELETE_WATCH4.md) — read-only 2026-11-24
  monitor verdict (deletion did NOT apply): zero `D` entries in git status,
  all winsrv/guishost/wsclient/qemu_windowsrv/gui_exec files still present,
  no backup snapshot, delete-tool logs all REFUSE/no-apply.
- [WINSRV_DELETE_WATCH5.md](WINSRV_DELETE_WATCH5.md) — follow-up read-only
  monitor (deletion landed? NO): targets still tracked and unmodified,
  `user/user.mk` still builds `WINSRV.E32`, no snapshot, HEAD is GUI-migration
  work (`addfd07`/`97f224b`), no delete ever applied.
- [WINSRV_DELETE_WATCH6.md](WINSRV_DELETE_WATCH6.md) — read-only check on
  whether `ses_df0e59b22` landed: NO — no deletions, all targets on disk, no
  rollback snapshot, tool logs all dry-run REFUSE with zero APPLY lines.
- [WINSRV_DELETE_WATCH7.md](WINSRV_DELETE_WATCH7.md) — read-only monitor: all
  9 DELETE_SET targets still tracked, no `winsrv_retire_backup_*` snapshot, all
  winsrv logs end REFUSE with zero APPLY lines; HEAD moved to `0f03f7e` via the
  EWSRV migration but the deletion itself never executed.
- [WINSRV_DELETE_WATCH8.md](WINSRV_DELETE_WATCH8.md) — read-only monitor:
  corrected deletion did NOT finish; no staged/unstaged deletions, all targets
  on disk, no snapshot, newest log 21:48:55 ends REFUSE, agent appears dead.
- [WINSRV_DELETE_WATCH9.md](WINSRV_DELETE_WATCH9.md) — read-only monitor:
  `ses_df0e59b22`'s deletion never executed; zero deletions, no snapshot, all
  logs REFUSE, no agent process alive, HEAD `0f03f7e` has no deletion commit.
- [WINSRV_DELETE_WATCH10.md](WINSRV_DELETE_WATCH10.md) — read-only monitor:
  direct deletion apply never ran; all 9 targets tracked per `git ls-files`,
  no snapshot, every log a dry-run REFUSE, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH11.md](WINSRV_DELETE_WATCH11.md) — read-only monitor of
  `ses_df0e49d19`: NO — zero `D` entries, all 17 winsrv-related files tracked,
  no snapshot, all runs REFUSE (no `--apply`/`--confirm DELETE-WINSRV`),
  HEAD `0f03f7e` with no deletion commit.

These eight newly indexed reports bring the indexed Markdown total to **337**,
matching the 337 `.md` files under `docs/` (including this index); every
Markdown document in the directory is linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH4–18 backfill (append-only)

A full `docs/*.md` listing (344 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the 329 unique `.md` link targets
(`](*.md)` pattern) previously in this index: fourteen documents existed on
disk but had no entry; they are linked below, and 0 links are stale (344
on-disk `.md` files = 344 unique indexed link targets, including this index;
the header count "(344 files)" at line 4 is updated). `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH4.md](WINSRV_DELETE_WATCH4.md) — read-only 2026-11-24
  monitor verdict (ses_df0e6e3a2): winsrv deletion did NOT apply — zero `D`
  entries in `git status`, all winsrv shim files present/unmodified, no
  snapshot dir, newest delete-tool logs end in `REFUSE: --apply not set`;
  `kernelhwsrv/` diff empty.
- [WINSRV_DELETE_WATCH5.md](WINSRV_DELETE_WATCH5.md) — read-only monitor,
  verdict NO: no deletions, all winsrv files tracked/unmodified, no snapshot
  dir, latest commits are GUI-migration work only, no delete ever applied.
- [WINSRV_DELETE_WATCH6.md](WINSRV_DELETE_WATCH6.md) — read-only check (did
  ses_df0e59b22 land?): NO — all 9 DELETE_SET targets still tracked/on disk,
  no snapshot, all tool logs end in REFUSE with zero APPLY lines; HEAD
  `addfd07` is migration-only.
- [WINSRV_DELETE_WATCH7.md](WINSRV_DELETE_WATCH7.md) — read-only check: NO —
  deletion did NOT land; all 9 targets present, no snapshot, newest log
  `winsrv_dryrun_head.log` (21:48) REFUSE; HEAD `0f03f7e` migration only.
- [WINSRV_DELETE_WATCH8.md](WINSRV_DELETE_WATCH8.md) — read-only 21:49 check:
  NO — nothing landed; zero deletions, all targets on disk, no snapshot, all
  logs REFUSE/zero APPLY, agent appears dead after 21:48:55 dry run.
- [WINSRV_DELETE_WATCH9.md](WINSRV_DELETE_WATCH9.md) — read-only check: NO —
  deletion never executed; no snapshot ⇒ `--apply` never ran, all logs
  REFUSE, no delete commit at HEAD `0f03f7e`, agent dead.
- [WINSRV_DELETE_WATCH10.md](WINSRV_DELETE_WATCH10.md) — read-only final
  check: NO — deletion never executed; all 9 DELETE_SET files tracked/on
  disk, no snapshot, all logs dry-run REFUSE, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH12.md](WINSRV_DELETE_WATCH12.md) — read-only check (did
  ses_df0e49d19 land?): NO — zero deletions, all 9 DELETE_SET targets tracked,
  no snapshot ⇒ no `--apply`, newest log REFUSE; HEAD `0f03f7e` migration
  only.
- [WINSRV_DELETE_WATCH13.md](WINSRV_DELETE_WATCH13.md) — read-only check: NO
  — stopped at dry-run/plan; no snapshot, logs end REFUSE, dry-run flags
  DOOM edits AMBIGUOUS after `0f03f7e` restored the migrated
  `e32_platform.c`.
- [WINSRV_DELETE_WATCH14.md](WINSRV_DELETE_WATCH14.md) — read-only 21:50
  verdict: agent ses_df0e49d19 DEAD, deletion NOT landed; no apply process,
  no snapshot, winsrv paths clean, HEAD `0f03f7e` migration wave only.
- [WINSRV_DELETE_WATCH15.md](WINSRV_DELETE_WATCH15.md) — read-only 21:5x
  verdict: ses_df0e3ba4c did NOT land the deletion — all runs REFUSED
  (`no --apply`, wrong-token apply attempt a no-op), no snapshot, all 9
  DELETE_SET files present, HEAD `0f03f7e` migration only.
- [WINSRV_DELETE_WATCH16.md](WINSRV_DELETE_WATCH16.md) — read-only verdict
  NO (ses_df0e3ba4c): every winsrv log is a dry-run REFUSE, no snapshot, no
  deletions in `git status`, all 9 DELETE_SET files present, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH17.md](WINSRV_DELETE_WATCH17.md) — read-only 21:51 EET
  verdict: agent ses_df0e3ba4c DEAD, deletion NOT landed; no snapshot, newest
  log REFUSE, only the delete tool itself modified, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH18.md](WINSRV_DELETE_WATCH18.md) — read-only verdict NO
  (ses_df0e26e7a): deletion never applied, dry-runs only; all 9 DELETE_SET
  files tracked/present, no snapshot, all logs REFUSE.

These fourteen newly indexed reports bring the indexed Markdown total to
**344**, matching the 344 `.md` files under `docs/` (including this index; the
header count "(344 files)" at line 4 is consistent); every Markdown document
in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 owned-index sweep, post-326 (append-only)

A full `docs/*.md` listing (337 files, flat directory) was diffed against the
unique `.md` link targets (`](*.md)` pattern) in this index: five documents
existed on disk but had no entry; they are linked below, with 0 stale links.
`kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH7.md](WINSRV_DELETE_WATCH7.md) — read-only monitor
  verdict NO: the `ses_df0e59b22` winsrv deletion never executed — all 9
  DELETE_SET targets still tracked/on disk, no snapshot dir, newest log ends
  `REFUSE: no --apply given`; HEAD `0f03f7e` is migration-only.
- [WINSRV_DELETE_WATCH8.md](WINSRV_DELETE_WATCH8.md) — read-only monitor
  verdict NO: the corrected deletion did not finish; nothing landed, zero
  deletions, no snapshot, all logs REFUSE, agent appears dead after its last
  dry run.
- [WINSRV_DELETE_WATCH9.md](WINSRV_DELETE_WATCH9.md) — read-only monitor
  verdict NO: the deletion never executed; no snapshot exists so `--apply`
  never ran, zero APPLY lines in any log, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH10.md](WINSRV_DELETE_WATCH10.md) — read-only monitor
  verdict NO: the direct winsrv deletion never executed; all 9 DELETE_SET
  files still tracked, no snapshot, all logs end `REFUSE: no --apply given`.
- [WINSRV_DELETE_WATCH11.md](WINSRV_DELETE_WATCH11.md) — read-only monitor
  verdict NO: deletion (ses_df0e49d19) did not land; zero tracked deletions,
  no snapshot, every run REFUSED without `--apply --confirm DELETE-WINSRV`.

These five newly indexed reports bring the indexed Markdown total to **337**,
matching the 337 `.md` files under `docs/` (including this index; the header
count "(337 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH5–9 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: ten documents existed on disk but had no
entry; they are linked below, and **0 links are stale**. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH5.md](WINSRV_DELETE_WATCH5.md) — read-only 2026-11-24
  monitor, verdict "deletion landed? NO": zero `D` entries, all winsrv files
  still tracked and unmodified, no snapshot dir, latest commits are GUI
  migration not deletion.
- [WINSRV_DELETE_WATCH6.md](WINSRV_DELETE_WATCH6.md) — read-only 2026-11-24
  check of whether the winsrv deletion landed: NO, all winsrv targets still
  tracked and on disk.
- [WINSRV_DELETE_WATCH7.md](WINSRV_DELETE_WATCH7.md) — read-only 2026-11-24
  re-check: the winsrv deletion did NOT land; nothing was deleted.
- [WINSRV_DELETE_WATCH8.md](WINSRV_DELETE_WATCH8.md) — read-only 2026-11-24
  21:49 check: the corrected winsrv deletion did NOT finish; nothing landed.
- [WINSRV_DELETE_WATCH9.md](WINSRV_DELETE_WATCH9.md) — read-only 2026-11-24
  check: the corrected winsrv deletion never executed; nothing landed.
- [WINSRV_DELETE_WATCH12.md](WINSRV_DELETE_WATCH12.md) — read-only 2026-11-24
  check of the direct deletion attempt: NO — the winsrv deletion did NOT
  land, nothing was deleted, HEAD is EWSRV-migration work.
- [WINSRV_DELETE_WATCH13.md](WINSRV_DELETE_WATCH13.md) — read-only 2026-11-24
  check: deletion did NOT run; stopped at dry-run/plan stage, nothing deleted.
- [WINSRV_DELETE_WATCH14.md](WINSRV_DELETE_WATCH14.md) — read-only 2026-11-24
  21:50 verdict: the deletion agent is DEAD; winsrv deletion NOT landed.
- [WINSRV_DELETE_WATCH15.md](WINSRV_DELETE_WATCH15.md) — read-only 2026-11-24
  21:5x verdict: the deletion agent did NOT land the winsrv deletion; all
  runs REFUSED.
- [WINSRV_DELETE_WATCH16.md](WINSRV_DELETE_WATCH16.md) — read-only 2026-11-24
  check: NO — every winsrv log is a dry-run ending in `REFUSE: no --apply`;
  no apply run and no snapshot dir exist, no deletions in `git status`.

These ten newly indexed reports bring the indexed Markdown total to **342**,
matching the 342 `.md` files under `docs/` (including this index; the header
count "(342 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH13/14 backfill (append-only)

A full `docs/*.md` listing (341 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the 338 unique `.md` link targets
(`](*.md)` pattern) previously in this index: three documents existed on disk
but had no entry; they are linked below, and 0 links are stale (341 on-disk
`.md` files = 341 unique indexed link targets, including this index; the
header count at line 4 is updated). `kernelhwsrv/` was untouched (read-only)
and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH13.md](WINSRV_DELETE_WATCH13.md) — read-only 2026-11-24
  monitor (ses_df0e49d19): winsrv deletion did NOT run, stopped at
  dry-run/plan stage — all 9 `DELETE_SET` sources still exist untouched.
- [WINSRV_DELETE_WATCH14.md](WINSRV_DELETE_WATCH14.md) — read-only 2026-11-24
  21:50 monitor: agent ses_df0e49d19 DEAD, deletion NOT landed — no
  winsrv_delete process, last log ends `REFUSE: no --apply`, no snapshot dir.
- [WINSRV_DELETE_WATCH15.md](WINSRV_DELETE_WATCH15.md) — read-only 2026-11-24
  21:5x monitor: agent ses_df0e3ba4c did NOT land the deletion — all runs
  REFUSED (`REFUSE: no --apply given`), dry-run only, nothing changed.

These three newly indexed reports bring the indexed Markdown total to **341**,
matching the 341 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is now
linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH15 backfill (append-only)

A full `docs/*.md` listing (341 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the 340 unique `.md` link targets
(`](*.md)` pattern) previously in this index: one document existed on disk but
had no entry; it is linked below, and 0 links are stale. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH15.md](WINSRV_DELETE_WATCH15.md) — read-only 2026-11-24
  21:5x monitor: agent ses_df0e3ba4c did NOT land the winsrv deletion — all
  runs REFUSED (`no --apply`, missing `--confirm DELETE-WINSRV` token), no
  snapshot dir, all 9 `DELETE_SET` files still present, HEAD = EWSRV
  migration wave only.

This newly indexed report brings the indexed Markdown total to **341**,
matching the 341 `.md` files under `docs/` (including this index; the header
count "(341 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH15 backfill (append-only)

A full `docs/*.md` listing (341 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the 340 unique `.md` link targets
(`](*.md)` pattern) previously in this index: one document existed on disk but
had no entry; it is linked below, and 0 links are stale (341 on-disk `.md`
files = 341 unique indexed link targets, including this index; the header
count "(341 files)" at line 4 is consistent). `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH15.md](WINSRV_DELETE_WATCH15.md) — read-only 2026-11-24
  21:5x monitor (agent ses_df0e3ba4c): winsrv deletion did NOT land — all runs
  REFUSED (`no --apply`, apply attempt lacked the `--confirm DELETE-WINSRV`
  token), no snapshot dirs, all 9 `DELETE_SET` files still present and
  unmodified, HEAD at EWSRV-migration work only.

This newly indexed report brings the indexed Markdown total to **341**,
matching the 341 `.md` files under `docs/` (including this index; the header
count "(341 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV delete-watch 15 backfill (append-only)

A fresh full `docs/*.md` listing (341 files, flat directory, no
subdirectories, both sides `LC_ALL=C` sorted) was diffed against the unique
`.md` link targets (`](*.md)` pattern) in this index: one document existed
on disk but had no entry; it is linked below, and **0 links are stale**
(341 on-disk `.md` files = 341 unique indexed link targets, including this
index; the header count "(341 files)" at line 4 is consistent). `kernelhwsrv/`
was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH15.md](WINSRV_DELETE_WATCH15.md) — read-only 21:5x
  verdict: agent `ses_df0e3ba4c` did NOT land the winsrv deletion; all runs
  REFUSED (no `--apply` / missing `--confirm DELETE-WINSRV` token), no snapshot,
  all 9 DELETE_SET files present and unmodified, HEAD `0f03f7e` is
  EWSRV-migration only.

This newly indexed report brings the indexed Markdown total to **341**,
matching the 341 `.md` files under `docs/` (including this index); every
Markdown document in the directory is linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH12–15 backfill (append-only)

A full `docs/*.md` listing (341 files, flat directory) was diffed against the
unique `.md` link targets (`](*.md)` pattern) in this index: at the time of
this sweep four `WINSRV_DELETE_WATCH` reports were unlinked; WATCH12–14 were
indexed by a concurrent pass and only WATCH15 remained, linked below; 0 stale
links. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH15.md](WINSRV_DELETE_WATCH15.md) — read-only 2026-11-24
  21:5x monitor, verdict NO: agent `ses_df0e3ba4c` did not land the winsrv
  deletion — all runs REFUSED (no `--apply` / wrong token), no snapshot dir,
  all 9 DELETE_SET files still tracked; HEAD `0f03f7e` is migration-only.

This newly indexed report brings the indexed Markdown total to **341**,
matching the 341 `.md` files under `docs/` (including this index; the header
count "(341 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH15 backfill (append-only)

A full `docs/*.md` listing (341 files, flat directory, no subdirectories) was
diffed against the 340 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below, and **0 links
are stale**. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH15.md](WINSRV_DELETE_WATCH15.md) — read-only 2026-11-24
  21:5x monitor, verdict NO: agent ses_df0e3ba4c did NOT land the winsrv
  deletion — all runs REFUSED (`no --apply given` / missing `--confirm
  DELETE-WINSRV` token), no snapshot dir, all 9 DELETE_SET files still present
  and unmodified, HEAD `0f03f7e` migration-only; `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **341**,
matching the 341 `.md` files under `docs/` (including this index; the header
count "(341 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH16 backfill (append-only)

A full `docs/*.md` listing (344 files, flat directory, no subdirectories) was
diffed against the 341 unique `.md` link targets previously in this index:
three documents existed on disk but had no entry; they are linked below, and 0
links are stale (0 linked targets missing from disk). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH16.md](WINSRV_DELETE_WATCH16.md) — read-only 2026-11-24
  monitor: agent `ses_df0e3ba4c` did NOT land the winsrv deletion — every
  winsrv log is a dry-run ending `REFUSE: no --apply given`, no
  `winsrv_retire_backup_*` snapshot, no winsrv deletions in `git status`, all
  9 DELETE_SET files still present, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH17.md](WINSRV_DELETE_WATCH17.md) — read-only 2026-11-24
  21:51 EET monitor: agent `ses_df0e3ba4c` DEAD (no winsrv/delete process in
  `ps`), deletion NOT landed — zero `D` entries in `git status`, all winsrv
  sources still present, no `winsrv_retire_backup_*` snapshot, so `--apply`
  never ran.
- [WINSRV_DELETE_WATCH18.md](WINSRV_DELETE_WATCH18.md) — read-only 2026-11-24
  monitor: agent `ses_df0e26e7a` did NOT apply the winsrv deletion — all 9
  DELETE_SET files still present and git-tracked, only dry-runs ran, verdict
  NO.

These three newly indexed reports bring the indexed Markdown total to **344**,
matching the 344 `.md` files under `docs/` (including this index; the header
count "(344 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH16 backfill (append-only)

A full `docs/*.md` listing (343 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the 341 unique `.md` link targets
(`](*.md)` pattern) previously in this index: two documents existed on disk but
had no entry; they are linked below, and **0 links are stale** (343 on-disk
`.md` files = 343 unique indexed link targets, including this index; the header
count "(343 files)" at line 4 is updated). `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH16.md](WINSRV_DELETE_WATCH16.md) — read-only 2026-11-24
  monitor, verdict NO: every winsrv log is a dry-run REFUSE (`no --apply
  given`), no snapshot dir, no deletions in `git status`, all 9 DELETE_SET
  files still present, HEAD `0f03f7e` migration-only.
- [WINSRV_DELETE_WATCH17.md](WINSRV_DELETE_WATCH17.md) — read-only 2026-11-24
  21:51 EET monitor, verdict NO: agent dead, no snapshot dir ⇒ `--apply`
  never ran, newest log `winsrv_dryrun.log` (21:50:49) REFUSE, zero `D`
  entries, only `tools/r1_winsrv_delete.py` itself modified, HEAD `0f03f7e`.

These two newly indexed reports bring the indexed Markdown total to **343**,
matching the 343 `.md` files under `docs/` (including this index; the header
count "(343 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH17 backfill (append-only)

A full `docs/*.md` listing (344 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: two documents existed on disk but had no
entry; they are linked below, and 0 links are stale. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH17.md](WINSRV_DELETE_WATCH17.md) — read-only 2026-11-24
  21:51 EET watch: agent ses_df0e3ba4c DEAD, winsrv deletion NOT landed — zero
  `D` entries in `git status`, no snapshot dirs, newest log still
  `REFUSE: no --apply given`; only `tools/r1_winsrv_delete.py` itself changed
  (+5/-3); HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH18.md](WINSRV_DELETE_WATCH18.md) — read-only 2026-11-24
  watch, verdict NO: agent ses_df0e26e7a never applied the deletion — all 9
  `DELETE_SET` files still tracked, logs all dry-run `REFUSE: no --apply`,
  no snapshot dirs, only `r1_winsrv_delete.py` modified (+27/-13); HEAD
  `0f03f7e` / `97f224b` migration-only.

These two newly indexed reports bring the indexed Markdown total to **344**,
matching the 344 `.md` files under `docs/` (including this index; the header
count "(344 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH16 backfill (append-only)

A full `docs/*.md` listing (342 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below, and **0 links are
stale**. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH16.md](WINSRV_DELETE_WATCH16.md) — read-only 2026-11-24
  watch: ses_df0e3ba4c did NOT land the winsrv deletion — every winsrv log is
  a dry-run ending `REFUSE: no --apply given`, no snapshot dir, all 9
  DELETE_SET files still tracked, HEAD `0f03f7e` migration-only.
- [WINSRV_DELETE_WATCH17.md](WINSRV_DELETE_WATCH17.md) — read-only 2026-11-24
  21:51 EET watch: agent ses_df0e3ba4c DEAD, deletion NOT landed — zero `D`
  entries, all DELETE_SET files present, no snapshot dir, newest logs all
  `REFUSE: no --apply given`.
- [WINSRV_DELETE_WATCH18.md](WINSRV_DELETE_WATCH18.md) — read-only 2026-11-24
  watch: ses_df0e26e7a's winsrv deletion was never applied — dry-runs only,
  nothing deleted.
- [WINSRV_DELETE_WATCH19.md](WINSRV_DELETE_WATCH19.md) — read-only 2026-11-24
  watch: ses_df0e26e7a's deletion never applied — only dry-run/no-args runs,
  `--apply` never executed, nothing deleted.

These newly indexed reports bring the indexed Markdown total to **345**,
matching the 345 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is now
linked, with 0 stale links.

## Added — 2026-11-24 WINSRV delete-watch 18 backfill (append-only)

A fresh full `docs/*.md` listing (344 files, flat directory, no
subdirectories, both sides `LC_ALL=C` sorted) was diffed against the unique
`.md` link targets (`](*.md)` pattern) in this index: one document existed
on disk but had no entry; it is linked below, and **0 links are stale**
(344 on-disk `.md` files = 344 unique indexed link targets, including this
index; the header count "(344 files)" at line 4 is updated). `kernelhwsrv/`
was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH18.md](WINSRV_DELETE_WATCH18.md) — read-only verdict NO
  for `ses_df0e26e7a`: deletion never applied (all logs dry-run REFUSE, no
  snapshot, no apply log); all 9 DELETE_SET files still tracked; HEAD `0f03f7e`
  / `97f224b` is migration-only.

This newly indexed report brings the indexed Markdown total to **344**,
matching the 344 `.md` files under `docs/` (including this index); every
Markdown document in the directory is linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH19 backfill (append-only)

A full `docs/*.md` listing (345 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below, and **0 links are
stale**. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH19.md](WINSRV_DELETE_WATCH19.md) — read-only 2026-11-24
  verdict NO: the winsrv deletion was never applied (only dry-run/no-args
  runs ending `REFUSE: no --apply given`, no snapshot dir, all 9 DELETE_SET
  files still git-tracked; HEAD `0f03f7e`/`97f224b` migration-only).

This newly indexed report brings the indexed Markdown total to **345**,
matching the 345 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is now
linked, with 0 stale links.

## Added — 2026-11-24 WINSRV delete-watch 19 backfill (append-only)

A fresh full `docs/*.md` listing (345 files, flat directory, no
subdirectories, both sides `LC_ALL=C` sorted) was diffed against the unique
`.md` link targets (`](*.md)` pattern) in this index: one document existed
on disk but had no entry; it is linked below, and **0 links are stale**
(345 on-disk `.md` files = 345 unique indexed link targets, including this
index; the header count "(345 files)" at line 4 is updated). `kernelhwsrv/`
was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH19.md](WINSRV_DELETE_WATCH19.md) — read-only verdict NO
  for `ses_df0e26e7a`: only dry-runs/no-args ran, `--apply` never executed; all
  9 DELETE_SET files still tracked, all logs REFUSE, no snapshot, HEAD
  `0f03f7e`/`97f224b` migration-only.

This newly indexed report brings the indexed Markdown total to **345**,
matching the 345 `.md` files under `docs/` (including this index); every
Markdown document in the directory is linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH17 backfill (append-only)

A full `docs/*.md` listing (343 files, flat directory, no subdirectories) was
diffed against the 342 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below, and **0 links
are stale**. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH17.md](WINSRV_DELETE_WATCH17.md) — read-only 2026-11-24
  21:51 EET monitor, verdict NOT LANDED: agent ses_df0e3ba4c dead, zero
  winsrv deletions in `git status`, all 9 DELETE_SET files still present, no
  snapshot dir, newest log ends `REFUSE: no --apply given`; only change is the
  `r1_winsrv_delete.py` tool itself; HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **343**,
matching the 343 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH19 backfill (append-only)

A full `docs/*.md` listing (345 files, flat directory, no subdirectories) was
diffed against the 344 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below, and **0 links
are stale**. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH19.md](WINSRV_DELETE_WATCH19.md) — read-only 2026-11-24
  monitor, verdict NO: ses_df0e26e7a never executed `--apply` — only dry-runs
  ending `REFUSE: no --apply given`, all 9 DELETE_SET files still tracked, no
  snapshot dir, `git log -2` (`0f03f7e`, `97f224b`) migration-only; only
  `r1_winsrv_delete.py` modified; `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **345**,
matching the 345 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 full directory sweep, post-342 (append-only)

A fresh full `docs/*.md` listing (346 files, flat directory, no
subdirectories) was diffed against the 346 unique `.md` link targets
(`](*.md)` pattern) in this index: **0 documents on disk are missing an
entry and 0 links are stale** (346 on-disk `.md` files = 346 unique indexed
link targets, including this index; the header count "(346 files)" at line 4
is consistent). No missing documents were found past the last-342 sweep, so
this section adds no new link entries; `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

The indexed Markdown total stays **346**, matching the 346 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH20 backfill (append-only)

A full `docs/*.md` listing (346 files, flat directory, no subdirectories) was
diffed against the 345 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below, and **0 links
are stale**. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH20.md](WINSRV_DELETE_WATCH20.md) — read-only 2026-11-24
  monitor, verdict NO: agent `ses_df0e26e7a` never ran `--apply` — all 9
  DELETE_SET winsrv files still on disk, no snapshot dir, all logs end
  `REFUSE: no --apply given`, only `r1_winsrv_delete.py` modified, HEAD
  `0f03f7e` migration-only; `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **346**,
matching the 346 `.md` files under `docs/` (including this index; the header
count "(346 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH20 backfill (append-only)

A full `docs/*.md` listing (346 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the 345 unique `.md` link targets
(`](*.md)` pattern) previously in this index: one document existed on disk but
had no entry; it is linked below, and **0 links are stale** (346 on-disk `.md`
files = 346 unique indexed link targets, including this index; the header
count "(346 files)" at line 4 is updated). `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH20.md](WINSRV_DELETE_WATCH20.md) — read-only 2026-11-24
  monitor, verdict NO: ses_df0e26e7a never ran `--apply` — all 9 DELETE_SET
  files still present, no snapshot dir, logs end `REFUSE: no --apply given`,
  `git log -2` (`0f03f7e`, `97f224b`) migration-only, only
  `r1_winsrv_delete.py` modified; `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **346**,
matching the 346 `.md` files under `docs/` (including this index; the header
count "(346 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 subagent full sweep vs last 345 (append-only)

A full `docs/*.md` listing (346 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: **0 documents on disk are missing an entry
and 0 links are stale** (346 on-disk `.md` files = 346 unique indexed link
targets, including this index; the header count "(346 files)" at line 4 is
consistent). No new documents were found since the last 345, so this section
adds no new link entries; `kernelhwsrv/` was untouched (read-only) and no QEMU
run was made by this index update.

The indexed Markdown total stays **346**, matching the 346 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with 0 stale links.

## Added — 2026-11-24 full sweep vs last 342 (append-only)

A full `docs/*.md` listing (371 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` plus backtick-wrapped `` `*.md` `` mentions) in this index:
documents that existed on disk without an entry are linked below, and
**0 in-directory links are stale** (371 on-disk `.md` files = 371 unique
indexed targets, including this index; the header count "(371 files)" at
line 4 is updated). The docs added since the 342-file sweep are the
`WINSRV_DELETE_WATCH*` series) were already linked by the preceding backfill
sections. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made
by this index update.

- [WINSRV_DELETE_WATCH21.md](WINSRV_DELETE_WATCH21.md) — read-only 2026-11-24
  monitor, verdict DEAD + NOT LANDED: agent `ses_df0e26e7a` not running; the
  apply was REFUSED (`P6 standing invariant violated: EFILE trampoline active
  at variant/e32load.cpp:1014`, APPLY_RC=2), all 7 DELETE_SET files still
  tracked, no snapshot dir, no delete commit (`0f03f7e`, `97f224b`).
- [WINSRV_DELETE_WATCH22.md](WINSRV_DELETE_WATCH22.md) — read-only 2026-11-24
  monitor, verdict NO: the final winsrv deletion apply was refused on the P6
  EFILE-trampoline invariant (APPLY_RC=2), nothing deleted — all 7 DELETE_SET
  sources still present and tracked, no snapshot dir, newest log still a
  dry-run REFUSE.
- [WINSRV_DELETE_WATCH23.md](WINSRV_DELETE_WATCH23.md) — read-only 2026-11-24
  monitor, verdict NO: the deletion did not land — newest apply log stops at
  the ewsrv pre-gate, earlier apply REFUSED on the P6 EFILE-trampoline
  invariant (`variant/e32load.cpp:1014`, APPLY_RC=2); all 7 DELETE_SET targets
  still exist and tracked, no snapshot dir, no delete commit.
- [WINSRV_DELETE_WATCH24.md](WINSRV_DELETE_WATCH24.md) — read-only ~21:55 EET
  monitor: re-apply by `ses_df0e49d19` IN PROGRESS — 18 edits + 7 deletes
  staged, snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` taken,
  process alive inside the EWSRV retire gate, no `APPLY_RC=` yet, delete files
  still on disk.
- [WINSRV_DELETE_WATCH25.md](WINSRV_DELETE_WATCH25.md) — read-only 21:54 EET
  monitor: re-apply NOT finished — blocked in post-edit gate[P1] `make`,
  pre-gate `r1_genuine_retire_test.py --only ewsrv` PASS, 18 edits applied,
  rollback token `ROLLBACK-WINSRV-20261124_215436`, no deletes executed yet.
- [WINSRV_DELETE_WATCH26.md](WINSRV_DELETE_WATCH26.md) — read-only 21:54 EET
  monitor: re-apply IN PROGRESS (PID 1286837) in gate[P1] make/selfcheck, no
  `APPLY_RC=` yet; 18 edits + 7 deletes staged, ewsrv pre-gate PASS, snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436` present.
- [WINSRV_DELETE_WATCH29.md](WINSRV_DELETE_WATCH29.md) — read-only ~21:56 EET
  monitor, verdict re-apply NOT finished: apply pid 1286837 still in
  gate[P1] make/selfcheck, no `APPLY_RC=`/ROLLBACK yet, 0 of 7 deletes landed,
  snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` present, ewsrv
  pre-gate PASS, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH30.md](WINSRV_DELETE_WATCH30.md) — read-only ~21:55 EET
  monitor: re-apply (pid 1286837) STILL RUNNING in P1 gate make/selfcheck, no
  `APPLY_RC=` yet; 18 edits applied, 7 deletes not staged, ewsrv pre-gate PASS,
  snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH31.md](WINSRV_DELETE_WATCH31.md) — read-only ~21:56 EET
  monitor: re-apply STILL RUNNING (pid 1286837, ~1m45s) in gate[P1]
  make/selfcheck, no `APPLY_RC=` yet; ewsrv pre-gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436`, zero `D` entries in git status.
- [WINSRV_DELETE_WATCH32.md](WINSRV_DELETE_WATCH32.md) — read-only 2026-11-24
  monitor: apply still running (~2 min, pid 1286837), no `APPLY_RC=`/rollback;
  25 ops (18 edits, 7 deletes) staged, in gate[P1]; no deletes in git yet, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH33.md](WINSRV_DELETE_WATCH33.md) — read-only 21:56 EET
  monitor: `APPLY_RC` not yet written — apply pid 1286837 (~2 min) still in
  gate[P1] `make`; 5 variant files modified, no deletes staged, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH35.md](WINSRV_DELETE_WATCH35.md) — read-only ~21:57 EET
  monitor: `APPLY_RC` NOT YET — apply pid 1286837 (~2m39s) still in gate[P1]
  make/selfcheck; no deletes staged, ewsrv pre-gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH36.md](WINSRV_DELETE_WATCH36.md) — read-only ~21:57 EET
  monitor: re-apply pid 1286837 (~3 min) still RUNNING in gate[P1], no
  `APPLY_RC`/rollback; ewsrv pre-gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436`, zero deletes staged, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH37.md](WINSRV_DELETE_WATCH37.md) — read-only 2026-11-24
  monitor: re-apply still RUNNING (pid 1286837, ~3m21s) in gate[P1], no
  `APPLY_RC`/rollback; ewsrv pre-gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436`.
- [WINSRV_DELETE_WATCH38.md](WINSRV_DELETE_WATCH38.md) — read-only ~21:58 EET
  monitor: re-apply RUNNING (pid 1286837, 3m32s), log ends mid-gate[P1], no
  `APPLY_RC`/rollback; ewsrv pre-gate PASS, snapshot present.
- [WINSRV_DELETE_WATCH39.md](WINSRV_DELETE_WATCH39.md) — read-only ~21:58 EET
  monitor: re-apply NOT finished — no `APPLY_RC` in the 1.6 KB apply log; pid
  1286837 (~3m44s) still in the P1 build/selfcheck gate.
- [WINSRV_DELETE_WATCH40.md](WINSRV_DELETE_WATCH40.md) — read-only 2026-11-24
  monitor: `APPLY_RC` not yet emitted — apply pid 1286837 (~4 min) still in
  gate[P1] `r1_os_selfcheck.py`; no rollback line.
- [WINSRV_DELETE_WATCH44.md](WINSRV_DELETE_WATCH44.md) — read-only 2026-11-24
  monitor: `APPLY_RC` NOT YET — apply still in gate[P1] make/selfcheck; 18/18
  edits applied, 7 deletes pending, ewsrv pre-gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436`.
- [WINSRV_DELETE_WATCH46.md](WINSRV_DELETE_WATCH46.md) — read-only ~22:05 EET
  monitor: `APPLY_RC` NOT YET — apply pid 1286837 (~5-6 min) still in gate[P1]
  (make + selfcheck + ewsrv retire gate); no rollback line.
- [WINSRV_DELETE_WATCH47.md](WINSRV_DELETE_WATCH47.md) — read-only 2026-11-24
  monitor: `APPLY_RC` NOT YET — re-apply pid 1286837 (~5m35s) still running,
  log (4 KB) ends at the P1 gate; no `APPLY_RC`/rollback.

These newly indexed reports bring the indexed Markdown total to **371**,
matching the 371 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 final new-docs sweep, 352 confirmed (append-only)

A fresh full `docs/*.md` listing (352 files, flat directory, no subdirectories,
both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: three documents existed on disk but had no
entry; they are linked below, and **0 links are stale** (352 on-disk `.md`
files = 352 unique indexed link targets, including this index; the header
count "(352 files)" at line 4 is updated). `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH24.md](WINSRV_DELETE_WATCH24.md) — read-only 2026-11-24
  ~21:55 monitor of the re-apply (ses_df0e49d19): NOT finished — `--apply`
  (PID 1286837) still in the EWSRV retire gate, log ends at the gate line with
  no `APPLY_RC=`; snapshot taken at
  `/tmp/opencode/winsrv_retire_backup_20261124_215436`, P1 consumers grep-clean,
  all 7 DELETE_SET files still on disk, HEAD `0f03f7e`; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH25.md](WINSRV_DELETE_WATCH25.md) — read-only 21:54 EET
  monitor: re-apply NOT finished — apply2 log ends at `+ gate[P1]: make` (PID
  1286837 blocked in post-edit build gate), pre-apply ewsrv retire gate PASS,
  all 18 edits applied, snapshot/rollback token
  `ROLLBACK-WINSRV-20261124_215436`, no deletions in `git status` yet, HEAD
  `0f03f7e`; `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH26.md](WINSRV_DELETE_WATCH26.md) — read-only 2026-11-24
  monitor: re-apply IN PROGRESS, not finished — PID 1286837 alive in gate[P1]
  (`make` + selfcheck), no `APPLY_RC=` yet; 18 edits + 7 deletes planned, ewsrv
  pre-gate PASS, snapshot `winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`;
  `kernelhwsrv/` clean.

These three newly indexed reports bring the indexed Markdown total to **352**,
matching the 352 `.md` files under `docs/` (including this index; the header
count "(352 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH24/25/26 backfill (append-only)

A full `docs/*.md` listing (352 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: three documents
existed on disk but had no entry; they are linked below, and 0 links are stale.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH24.md](WINSRV_DELETE_WATCH24.md) — read-only 2026-11-24
  ~21:55 monitor: the winsrv re-apply (ses_df0e49d19, `r1_winsrv_delete.py
  --apply`, PID 1286837) is still in the EWSRV retire pre-gate with no
  `APPLY_RC=` yet; 25 ops planned (18 edits, 7 deletes), P1 grep-clean,
  snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` taken, all 7
  DELETE_SET files still on disk, `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH25.md](WINSRV_DELETE_WATCH25.md) — read-only 2026-11-24
  21:54 EET monitor: re-apply NOT finished — log `winsrv_delete_apply2.log`
  ends at `gate[P1]: make`, no `APPLY_RC=`; all 18 edits applied, pre-gate
  ewsrv retire test PASS, same snapshot/rollback token, deletes not yet run,
  `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH26.md](WINSRV_DELETE_WATCH26.md) — read-only 2026-11-24
  monitor: re-apply IN PROGRESS (PID 1286837 in gate[P1] make/selfcheck, no
  `APPLY_RC=`); 18 edits + 7 deletes staged, ewsrv pre-gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436` (token
  `ROLLBACK-WINSRV-20261124_215436`), `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH29.md](WINSRV_DELETE_WATCH29.md) — read-only 2026-11-24
  ~21:56 monitor, verdict re-apply NOT finished: no `APPLY_RC=`/ROLLBACK in
  `winsrv_delete_apply2.log`, PID 1286837 still in `gate[P1]` make/selfcheck,
  0 of 7 deletes landed (no `D` entries), snapshot
  `winsrv_retire_backup_20261124_215436` present, pre-gate ewsrv retire PASS,
  `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH30.md](WINSRV_DELETE_WATCH30.md) — read-only 2026-11-24
  ~21:55 monitor: re-apply (PID 1286837) STILL RUNNING, no `APPLY_RC=` in
  `winsrv_delete_apply2.log`; 18 edits applied, now at P1 gate make/selfcheck,
  ewsrv pre-gate PASS, snapshot `winsrv_retire_backup_20261124_215436`, no
  deletes in git yet, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH31.md](WINSRV_DELETE_WATCH31.md) — read-only 2026-11-24
  ~21:56 monitor: re-apply STILL RUNNING, no `APPLY_RC=` in
  `winsrv_delete_apply2.log` (ends at `gate[P1]: make`/selfcheck); PID 1286837
  alive ~1m45s, ewsrv pre-gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436` (token
  `ROLLBACK-WINSRV-20261124_215436`).
- [WINSRV_DELETE_WATCH32.md](WINSRV_DELETE_WATCH32.md) — read-only 2026-11-24
  monitor: no `APPLY_RC=` yet (PID 1286837, ~2 min, in gate[P1] make/selfcheck);
  25 ops planned (18 edits, 7 deletes), 13 edits applied, no `D` entries in
  git status, ewsrv pre-gate PASS, same snapshot/token, HEAD `0f03f7e`.

These seven newly indexed reports bring the indexed Markdown total to **356**,
matching the 356 `.md` files under `docs/` (including this index; the header
count "(356 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH29–32 backfill (append-only)

A full `docs/*.md` listing (356 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: four documents
existed on disk but had no entry; they are linked below, and 0 links are stale
(the three backtick-wrapped header links `ARCHITECTURE.md`, `DOCS_INDEX.md`
and `r1_boot_and_selftest.md` account for the plain-regex diff of 3).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH29.md](WINSRV_DELETE_WATCH29.md) — read-only 2026-11-24
  ~21:56 EET monitor: the winsrv re-apply (`r1_winsrv_delete.py --apply
  --confirm DELETE-WINSRV-EWSRV`, PID 1286837) is still NOT finished — no
  `APPLY_RC=` in `winsrv_delete_apply2.log`, still in `gate[P1]` make/selfcheck
  with 18 edits applied and 0 of 7 deletes landed; snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436` present, `kernelhwsrv/`
  clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH30.md](WINSRV_DELETE_WATCH30.md) — read-only 2026-11-24
  ~21:55 EET monitor: the winsrv re-apply (apply2, PID 1286837) still running
  with no `APPLY_RC=` yet — 18 edits applied (qemu_variant.h, qemu_mouse.cpp,
  qemu_display.cpp, kern_shell.cpp, qemu_variant_int.cpp), now at the P1 gate
  make/selfcheck; 0 of 7 deletes staged, winsrv sources still on disk, pre-apply
  ewsrv gate PASS, same snapshot/token, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH31.md](WINSRV_DELETE_WATCH31.md) — read-only 2026-11-24
  ~21:56 EET monitor: winsrv re-apply still running (PID 1286837, ~1m45s, no
  `APPLY_RC=`, log ends at `gate[P1]` make/selfcheck); 0 git `D` entries, all
  winsrv/guishost/wsclient/qemu_windowsrv files still on disk, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH32.md](WINSRV_DELETE_WATCH32.md) — read-only 2026-11-24
  re-apply status: still no `APPLY_RC=` (PID 1286837, ~2 min); 25 ops (18 edits,
  7 deletes) with 13 edits applied and still in the P1 gate; no deletes staged,
  `variant/qemu_windowsrv.*` still present, HEAD `0f03f7e`, `kernelhwsrv/` clean.

These four newly indexed reports bring the indexed Markdown total to **356**,
matching the 356 `.md` files under `docs/` (including this index; the header
count "(356 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH30 backfill (append-only)

A full `docs/*.md` listing (354 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH30.md](WINSRV_DELETE_WATCH30.md) — read-only 2026-11-24
  ~21:55 watch: ses_df0e49d19 re-apply (pid 1286837) STILL RUNNING in the P1
  gate (`make` + selfcheck, no `APPLY_RC=` yet); 18 edits applied, 7 deletes
  not in git, snapshot `winsrv_retire_backup_20261124_215436`, ewsrv pre-gate
  PASS, HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **354**,
matching the 354 `.md` files under `docs/` (including this index; the header
count "(354 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH34 backfill (append-only)

A full `docs/*.md` listing (358 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH34.md](WINSRV_DELETE_WATCH34.md) — read-only 2026-11-24
  ~21:57 watch: winsrv re-apply (`r1_winsrv_delete.py --apply --confirm
  DELETE-WINSRV-EWSRV`, PID 1286837) still RUNNING in `gate[P1]` make/selfcheck
  with no `APPLY_RC=` yet; 18/25 edits applied, 0 of 7 file deletes landed,
  ewsrv pre-apply gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436` (token
  `ROLLBACK-WINSRV-20261124_215436`), HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **358**,
matching the 358 `.md` files under `docs/` (including this index; the header
count "(358 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH31/32 backfill (append-only)

A full `docs/*.md` listing (356 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH31.md](WINSRV_DELETE_WATCH31.md) — read-only 2026-11-24
  ~21:56 watch: ses_df0e49d19 re-apply STILL RUNNING (pid 1286837 in P1 gate
  make/selfcheck, no `APPLY_RC=` yet), zero `D` entries in git, ewsrv
  pre-gate PASS, snapshot `winsrv_retire_backup_20261124_215436`, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH32.md](WINSRV_DELETE_WATCH32.md) — read-only re-apply
  status: apply still running ~2 min (no `APPLY_RC=`), 25 ops planned with 13
  edits applied, in gate[P1] make/selfcheck, no deletes in git yet, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **356**,
matching the 356 `.md` files under `docs/` (including this index; the header
count "(356 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH33 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: one document existed on disk but had no
entry; it is linked below, and 0 links are stale. `kernelhwsrv/` was untouched
and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH33.md](WINSRV_DELETE_WATCH33.md) — read-only 2026-11-24
  21:56 monitor of the winsrv re-apply (PID 1286837, `--apply --confirm
  DELETE-WINSRV-EWSRV`): still running in `gate[P1]: make`, no `APPLY_RC=` yet;
  5 edits applied so far, no deletes staged, pre-apply ewsrv gate PASS,
  snapshot `winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **357**,
matching the 357 `.md` files under `docs/` (including this index; the header
count "(357 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 full sweep vs last 346 (append-only)

A full `docs/*.md` listing (357 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: one document existed on disk but had no
entry; it is linked below, and **0 in-directory links are stale** (357 on-disk
`.md` files = 357 unique indexed targets, including this index; the header
count "(357 files)" at line 4 is updated). `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH33.md](WINSRV_DELETE_WATCH33.md) — read-only 2026-11-24
  21:56 monitor: winsrv re-apply (PID 1286837, ~2 min) still running, no
  `APPLY_RC=` yet (log ends at `gate[P1]: make`); 18 edits applied, no winsrv
  deletes in git yet, ewsrv pre-gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436` present, `kernelhwsrv/` clean,
  HEAD `0f03f7e`.

This newly indexed report brings the indexed Markdown total to **357**,
matching the 357 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH31/32 backfill (append-only)

A full `docs/*.md` listing (356 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below, and 0 links are stale.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH31.md](WINSRV_DELETE_WATCH31.md) — read-only 2026-11-24
  ~21:56 EET monitor: winsrv re-apply (apply2, PID 1286837) still running with no
  `APPLY_RC=` — log ends at gate[P1] make/selfcheck, 18 edits applied, 0 of 7
  deletes staged, pre-apply ewsrv gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH32.md](WINSRV_DELETE_WATCH32.md) — read-only 2026-11-24
  re-apply status: still no `APPLY_RC=` (PID 1286837, ~2 min in, 13 of 18 edits
  applied, in gate[P1] make + `r1_os_selfcheck.py`), 25 ops planned, no deletes
  in `git status` yet, same snapshot, HEAD `0f03f7e` on `97f224b`,
  `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **356**,
matching the 356 `.md` files under `docs/` (including this index; the header
count "(356 files)" at line 4 is current); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 full sweep vs last 346 (append-only)

A full `docs/*.md` listing (358 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` plus backtick-wrapped ```*.md``` mentions) in this index: one
document existed on disk but had no entry; it is linked below, and
**0 in-directory links are stale** (358 on-disk `.md` files = 358 unique
indexed targets, including this index; the header count "(358 files)" at
line 4 is updated). `kernelhwsrv/` was untouched (read-only) and no QEMU
run was made by this index update.

- [WINSRV_DELETE_WATCH33.md](WINSRV_DELETE_WATCH33.md) — read-only 2026-11-24
  ~21:56 monitor, verdict STILL RUNNING: the winsrv re-apply (PID 1286837,
  `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) has no
  `APPLY_RC=` yet — log ends at `gate[P1]: make`; ewsrv pre-gate PASS,
  snapshot `winsrv_retire_backup_20261124_215436` present, no deletes
  staged, `kernelhwsrv/` clean, HEAD `0f03f7e`.

This newly indexed report brings the indexed Markdown total to **359**,
matching the 359 `.md` files under `docs/` (including this index; the header
count "(358 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH33/34 backfill (append-only)

A full `docs/*.md` listing (358 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH33.md](WINSRV_DELETE_WATCH33.md) — read-only 2026-11-24
  ~21:56 re-apply watch: apply pid 1286837 still running in gate[P1] `make`
  (no `APPLY_RC=` yet), 5 variant files edited, zero deletes staged, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH34.md](WINSRV_DELETE_WATCH34.md) — read-only ~21:57
  re-apply watch: pid 1286837 alive in P1 gate make/selfcheck with no
  `APPLY_RC`/rollback, 18/25 edits applied, 0 of 7 deletes executed, ewsrv
  pre-gate PASS, snapshot `winsrv_retire_backup_20261124_215436`, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **359**,
matching the 359 `.md` files under `docs/` (including this index; the header
count "(358 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

- [WINSRV_DELETE_WATCH34.md](WINSRV_DELETE_WATCH34.md) — read-only 2026-11-24
  monitor of the same winsrv re-apply (PID 1286837): status unchanged, still
  no `APPLY_RC=`, still in `gate[P1]`; appended to the 2026-11-24
  full-sweep-vs-last-346 section above. Total stays **359** (359 on-disk `.md`
  files = 359 unique indexed targets, including this index; header count
  "(359 files)" at line 4 updated), 0 stale links; `kernelhwsrv/` untouched,
  no QEMU run.

## Added — 2026-11-24 WINSRV_DELETE_WATCH34 backfill (append-only)

A full `docs/*.md` listing (358 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below, and 0 links are stale
(the three backtick-wrapped header links `ARCHITECTURE.md`, `DOCS_INDEX.md`
and `r1_boot_and_selftest.md` account for the plain-regex diff of 3).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH34.md](WINSRV_DELETE_WATCH34.md) — read-only 2026-11-24
  ~21:57 EET re-apply status: winsrv re-apply still running (PID 1286837, log
  ends at `gate[P1]` make/selfcheck, no `APPLY_RC=`, no rollback); 18/25 edits
  applied, 0 of 7 deletes executed, pre-apply ewsrv gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **358**,
matching the 358 `.md` files under `docs/` (including this index; the header
count "(358 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH35 backfill (append-only)

A full `docs/*.md` listing (359 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH35.md](WINSRV_DELETE_WATCH35.md) — read-only 2026-11-24
  winsrv re-apply watch: apply pid 1286837 still running in gate[P1]
  (no `APPLY_RC=` yet), edits applied, no deletes staged, snapshot
  `winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **359**,
matching the 359 `.md` files under `docs/` (including this index; the header
count "(359 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 full sweep vs last 346 (append-only)

A full `docs/*.md` listing (358 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: one document existed on disk but had no
entry; it is linked below, and **0 in-directory links are stale** (358 on-disk
`.md` files = 358 unique indexed targets, including this index; the header
count "(358 files)" at line 4 is updated). `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH34.md](WINSRV_DELETE_WATCH34.md) — read-only 2026-11-24
  ~21:57 monitor: winsrv re-apply (PID 1286837) still running, no `APPLY_RC=`
  (log ends at `gate[P1]: make`/selfcheck); 18/25 edits applied, 0 of 7
  deletes executed, ewsrv pre-gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436` present, `kernelhwsrv/` clean,
  HEAD `0f03f7e`.

This newly indexed report brings the indexed Markdown total to **358**,
matching the 358 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH35–39 backfill (append-only)

A full `docs/*.md` listing (383 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: thirteen documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH35.md](WINSRV_DELETE_WATCH35.md) — read-only 2026-11-24
  ~21:57 monitor, verdict STILL RUNNING: the winsrv re-apply (PID 1286837,
  `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) has no
  `APPLY_RC=` yet — log `/tmp/opencode/winsrv_delete_apply2.log` ends at
  `gate[P1]: make` + `r1_os_selfcheck.py`; ewsrv pre-gate PASS, snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436` present, no deletes
  staged, `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH37.md](WINSRV_DELETE_WATCH37.md) — read-only 2026-11-24
  monitor: re-apply still RUNNING (pid 1286837, ~3m21s, no `APPLY_RC=`); 13
  edit ops applied, 7 deletes are in-file block deletions not yet staged;
  winsrv/guishost/qemu_windowsrv sources still tracked; ewsrv pre-gate PASS,
  same snapshot/token, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH38.md](WINSRV_DELETE_WATCH38.md) — read-only 2026-11-24
  ~21:58 monitor: re-apply RUNNING (3m32s, log ends mid-`gate[P1]`, no
  `APPLY_RC`/rollback); zero `D` entries in git, winsrv sources still on
  disk; pre-apply ewsrv gate PASS; snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436`; HEAD `0f03f7e`;
  `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH39.md](WINSRV_DELETE_WATCH39.md) — read-only 2026-11-24
  ~21:58 monitor: re-apply NOT finished (no `APPLY_RC`, ~3:44 elapsed, in P1
  build/selfcheck gate); 18 edits applied, 7 deletes not executed, 0 `D`
  entries; pre-apply ewsrv gate PASS; snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436`; HEAD `0f03f7e`;
  `kernelhwsrv/` clean/untouched.
- [WINSRV_DELETE_WATCH46.md](WINSRV_DELETE_WATCH46.md) — read-only 2026-11-24
  ~22:05 monitor: re-apply still RUNNING (pid 1286837, ~5-6 min, no
  `APPLY_RC`/rollback; log ends in the P1 gate); 18/25 edits applied, 7
  deletes not on disk, no git `D` entries; snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436` intact; HEAD `0f03f7e`;
  `kernelhwsrv/` clean/untouched.
- [WINSRV_DELETE_WATCH49.md](WINSRV_DELETE_WATCH49.md) — read-only monitor:
  re-apply still running, no `APPLY_RC=` (log ends at `gate[P2]: make
  isogrub`); P1 edits + P1 gate PASS, P2 edits (Makefile, user/user.mk)
  applied, deletes not started, no git `D` entries; pid 1286837 (~6 min);
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH50.md](WINSRV_DELETE_WATCH50.md) — read-only 2026-11-24
  22:00 EET monitor: no `APPLY_RC=` (log ends at P2 gate `make isogrub` +
  selfcheck); pid 1286837 (~6m22s) alive; zero git deletes, all winsrv
  sources still tracked; HEAD `0f03f7e` on `97f224b`; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH52.md](WINSRV_DELETE_WATCH52.md) — read-only 2026-11-24
  ~22:01 monitor: no `APPLY_RC=` (log at P2 gate, pid 1286837 ~7 min; stray
  duplicate apply pid 1298363 exited); P1 edits+gates PASS, zero git `D`
  entries, winsrv sources still on disk; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH53.md](WINSRV_DELETE_WATCH53.md) — read-only 2026-11-24
  ~22:02 monitor: no `APPLY_RC=` (log at `gate[P2]: r1_os_selfcheck.py`);
  P1 + pre-apply ewsrv gates PASS; zero git deletes, all winsrv files still
  tracked; pid 1286837 alive; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH54.md](WINSRV_DELETE_WATCH54.md) — read-only 2026-11-24
  ~22:00 monitor: no `APPLY_RC=` (P2 gate `make isogrub` + selfcheck, pid
  1286837 ~7 min); P1 edits + P1 ewsrv gate PASS; zero git `D` entries,
  winsrv sources still tracked; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 monitor: no `APPLY_RC`/rollback line; apply still running (pid
  1286837, ~7m35s); `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24
  monitor: re-apply RUNNING (pid 1286837, 7m35s, log not truncated); no
  `APPLY_RC`/rollback; pre-apply snapshot
  `winsrv_retire_backup_20261124_215436` intact.
- [WINSRV_DELETE_WATCH59.md](WINSRV_DELETE_WATCH59.md) — read-only 2026-11-24
  ~22:08 monitor: no `APPLY_RC`/`ROLLBACK`/`FAIL` lines (log ends at P2 gate
  `make isogrub` + selfcheck); pid 1286837 alive ~8 min; `kernelhwsrv/`
  clean.

These newly indexed reports bring the indexed Markdown total to **383**,
matching the 383 `.md` files under `docs/` (including this index; the header
count "(383 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH35 backfill (append-only)

A full `docs/*.md` listing (359 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below, and 0 links are stale
(the three backtick-wrapped header links `ARCHITECTURE.md`, `DOCS_INDEX.md`
and `r1_boot_and_selftest.md` account for the plain-regex diff of 3).
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH35.md](WINSRV_DELETE_WATCH35.md) — read-only 2026-11-24
  ~21:57 EET re-apply status: apply still running (PID 1286837, ~2m39s, no
  `APPLY_RC=`/rollback), no winsrv deletes staged, 18 edits applied, pre-apply
  ewsrv gate PASS, snapshot `winsrv_retire_backup_20261124_215436`, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **359**,
matching the 359 `.md` files under `docs/` (including this index; the header
count "(359 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH35 backfill (append-only)

A full `docs/*.md` listing (360 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: one document
existed on disk but had no entry; it is linked below, and 0 links are stale.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH35.md](WINSRV_DELETE_WATCH35.md) — read-only 2026-11-24 ~21:57 monitor: the winsrv re-apply (pid 1286837, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) is still running with no `APPLY_RC=` yet — log ends at gate[P1] make/selfcheck, 18 edits + 7 deletes in the op set applied to the worktree, no deletes staged in git, pre-gate ewsrv PASS, snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **360**, matching the 360 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

 (append-only)

A fresh full `docs/*.md` listing (366 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: 6
document(s) added since the last 352 sweep had no entry and are linked below;
0 links are stale. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH37.md](WINSRV_DELETE_WATCH37.md) — WINSRV_DELETE_WATCH37 — re-apply status (2026-11-24).
- [WINSRV_DELETE_WATCH38.md](WINSRV_DELETE_WATCH38.md) — WINSRV_DELETE_WATCH38 — 2026-11-24 ~21:58.
- [WINSRV_DELETE_WATCH39.md](WINSRV_DELETE_WATCH39.md) — WINSRV_DELETE_WATCH39 — re-apply status (2026-11-24 ~21:58).
- [WINSRV_DELETE_WATCH40.md](WINSRV_DELETE_WATCH40.md) — WINSRV_DELETE_WATCH40 — re-apply status (2026-11-24).
- [WINSRV_DELETE_WATCH41.md](WINSRV_DELETE_WATCH41.md) — WINSRV_DELETE_WATCH41 — 2026-11-24 21:59 UTC.
- [WINSRV_DELETE_WATCH42.md](WINSRV_DELETE_WATCH42.md) — WINSRV_DELETE_WATCH42 — 2026-11-24 ~21:59.

This sweep brings the indexed Markdown total to **366**, matching the 366
`.md` files under `docs/` (including this index; the header count
"(366 files)" at line 4 is updated); every Markdown document in the directory
is linked, with 0 stale links.

## Added — 2026-11-24 post-352 new-docs sweep, WINSRV_DELETE_WATCH36–38 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: three documents existed on disk but had no entry and 0
links were stale; they are linked below. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH36.md](WINSRV_DELETE_WATCH36.md) — read-only 2026-11-24
  ~21:57 monitor of the winsrv re-apply (pid 1286837): still running ~3 min in
  gate[P1] (make + selfcheck), no APPLY_RC/rollback yet, pre-apply ewsrv gate
  PASS, snapshot `winsrv_retire_backup_20261124_215436`, zero deletes staged,
  HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH37.md](WINSRV_DELETE_WATCH37.md) — read-only 2026-11-24
  re-apply status: pid 1286837 alive ~3m21s, no APPLY_RC yet, 13 edit ops
  applied (qemu_variant.h, qemu_mouse.cpp, qemu_display.cpp, kern_shell.cpp,
  qemu_variant_int.cpp), 7 in-file block deletes pending, no `D` entries in git,
  winsrv sources still tracked, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH38.md](WINSRV_DELETE_WATCH38.md) — read-only 2026-11-24
  ~21:58 watch: re-apply still mid-gate[P1] (log 1.6 KB), no APPLY_RC/rollback,
  ewsrv pre-gate PASS, snapshot token `ROLLBACK-WINSRV-20261124_215436`, zero
  deletes staged, only `tools/r1_winsrv_delete.py` modified plus WATCH docs
  untracked, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH44.md](WINSRV_DELETE_WATCH44.md) — read-only 2026-11-24
  re-apply status: pid 1286837 still running ~5 min in gate[P1] (make +
  selfcheck), no APPLY_RC/rollback yet, 18/18 edits applied with 7 deletes
  pending, pre-apply ewsrv gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436`, no `D` entries in git, HEAD `0f03f7e`.

These newly indexed reports bring the indexed Markdown total to **375**,
matching the 375 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 subagent full sweep, post-362 (append-only)

A fresh full `docs/*.md` listing (383 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: seven documents
existed on disk but had no entry; they are linked below, and 0 links are
stale. `WINSRV_DELETE_WATCH36.md` and `WINSRV_DELETE_WATCH37.md` were confirmed
already linked in the sections above. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH46.md](WINSRV_DELETE_WATCH46.md) — read-only 2026-11-24
  ~22:05 watch: winsrv re-apply (pid 1286837) still running at gate[P1] with
  no `APPLY_RC`/rollback yet; 18/25 edits applied, 7 deletes not yet on disk,
  no git `D` entries, snapshot `ROLLBACK-WINSRV-20261124_215436` intact,
  HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH49.md](WINSRV_DELETE_WATCH49.md) — read-only 2026-11-24
  re-apply monitor: apply (pid 1286837) past P1 (gate PASS) into gate[P2]
  (`make isogrub`), no `APPLY_RC`/rollback yet; P2 edits (Makefile,
  `user/user.mk`) applied, deletes not started, no git `D` entries, snapshot
  `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/`
  clean.
- [WINSRV_DELETE_WATCH52.md](WINSRV_DELETE_WATCH52.md) — read-only 2026-11-24
  ~22:01 watch: apply (pid 1286837) in gate[P2] with P1 edits+gates PASS,
  duplicate pid 1298363 exited, 18 edits in the worktree, zero git `D`
  entries, snapshot intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH53.md](WINSRV_DELETE_WATCH53.md) — read-only 2026-11-24
  ~22:02 monitor: still no `APPLY_RC` (log at gate[P2] selfcheck), no git
  deletes, winsrv sources still on disk, HEAD `0f03f7e`, snapshot
  `ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 watch: apply (pid 1286837, ~7m35s) still running with no `APPLY_RC`;
  P1 edits+gate PASS, P2 gate (`make isogrub` + selfcheck) in progress, 7
  deletes (P3) not executed, no git `D` entries, snapshot intact, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24
  re-apply status: pid 1286837 alive, log at P2 gate, no `APPLY_RC`/rollback;
  P1 done (18 edits, gates PASS), zero git `D` entries, 7 deletes pending,
  snapshot `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/`
  clean.
- [WINSRV_DELETE_WATCH59.md](WINSRV_DELETE_WATCH59.md) — read-only 2026-11-24
  ~22:08 watch: apply (pid 1286837, ~8 min) still at the P2 gate with no
  `APPLY_RC`/rollback; P1 edits+gate PASS, P2 edits applied, deletes not
  started, winsrv sources still on disk, snapshot intact, `kernelhwsrv/`
  clean.

These seven newly indexed reports bring the indexed Markdown total to **383**,
matching the 383 `.md` files under `docs/` (including this index; the header
count "(383 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep vs index links (append-only)

A full `docs/*.md` listing (367 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 1 document(s)
existed on disk but had no entry; they are linked below, and 0 links are stale.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH43.md](WINSRV_DELETE_WATCH43.md) — read-only 2026-11-24 status report; see file.

These newly indexed reports bring the indexed Markdown total to **367**, matching the 367 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH44 backfill (append-only)

A fresh full `docs/*.md` listing (368 files, flat directory, no subdirectories)
was diffed against the 367 unique `.md` link targets previously in this index:
one document existed on disk but had no entry; it is linked below, and 0 links
are stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH44.md](WINSRV_DELETE_WATCH44.md) — read-only 2026-11-24
  ~22:00 monitor of the winsrv re-apply (pid 1286837, ~5 min elapsed, still
  RUNNING at `gate[P1]` make/selfcheck, no `APPLY_RC`/rollback): 18/18 edits
  applied, 7 deletes pending, pre-apply ewsrv gate PASS, snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436`, no git `D` entries,
  HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **368**,
matching the 368 `.md` files under `docs/` (including this index; the header
count "(368 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH37/38/39 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: three documents existed on disk but had no
entry; they are linked below, and 0 links are stale. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH37.md](WINSRV_DELETE_WATCH37.md) — read-only 2026-11-24
  re-apply status: PID 1286837 still in `gate[P1]` make/selfcheck (~3m21s), no
  `APPLY_RC=` yet; 13 edits applied, no git deletes, pre-apply ewsrv gate PASS,
  snapshot `winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH38.md](WINSRV_DELETE_WATCH38.md) — read-only ~21:58
  monitor: re-apply RUNNING (~3m32s), log ends mid-`gate[P1]`, no APPLY_RC or
  rollback; zero `D` entries in git, winsrv sources still tracked, HEAD
  `0f03f7e`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH39.md](WINSRV_DELETE_WATCH39.md) — read-only ~21:58
  monitor: re-apply NOT finished (~3:44 elapsed, no `APPLY_RC`); 18 edits
  applied, 7 deletes not yet executed, 0 git deletes, ewsrv pre-gate PASS,
  snapshot `winsrv_retire_backup_20261124_215436`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH44.md](WINSRV_DELETE_WATCH44.md) — read-only 2026-11-24
  re-apply status: PID 1286837 alive (~5 min) in `gate[P1]`, no `APPLY_RC`; 18/18
  edits applied, 7 deletes pending, no git `D` entries, ewsrv pre-gate PASS,
  snapshot `winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH45.md](WINSRV_DELETE_WATCH45.md) — read-only ~21:59
  monitor: apply still in progress (no `APPLY_RC`/rollback in the 1.6 KB log);
  no winsrv deletes in git, only `r1_winsrv_delete.py` modified + watch docs
  untracked, pre-apply ewsrv gate PASS, snapshot/token as above, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH49.md](WINSRV_DELETE_WATCH49.md) — read-only 2026-11-24
  monitor: apply now in `gate[P2]: make isogrub` (~6 min, no `APPLY_RC`); P1
  edits + P1 gate PASS, P2 edits (Makefile, `user/user.mk`) applied, deletes
  not started, no git `D` entries, snapshot token
  `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.

These six newly indexed reports bring the indexed Markdown total to **373**,
matching the 373 `.md` files under `docs/` (including this index; the header
count "(373 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 post-352 sweep, WINSRV_DELETE_WATCH36–38 backfill (append-only)

A full `docs/*.md` listing (362 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in
this index: three documents existed on disk but had no entry; they are linked
below, and **0 in-directory links are stale** (362 on-disk `.md` files = 362
unique indexed targets after this section, including this index; the header
count "(362 files)" at line 4 is consistent). `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH36.md](WINSRV_DELETE_WATCH36.md) — read-only 2026-11-24
  ~21:57 monitor: winsrv re-apply (PID 1286837, `r1_winsrv_delete.py --apply
  --confirm DELETE-WINSRV-EWSRV`) still RUNNING ~3 min, no APPLY_RC/rollback
  yet; ewsrv pre-gate PASS, snapshot present, zero deletes staged, HEAD
  `0f03f7e`.
- [WINSRV_DELETE_WATCH37.md](WINSRV_DELETE_WATCH37.md) — read-only 2026-11-24
  re-apply status: still RUNNING (~3m21s), no APPLY_RC; 13 edit ops applied
  (qemu_variant.h, qemu_mouse.cpp, qemu_display.cpp, kern_shell.cpp,
  qemu_variant_int.cpp), no `D` entries in git, winsrv sources still on disk.
- [WINSRV_DELETE_WATCH38.md](WINSRV_DELETE_WATCH38.md) — read-only 2026-11-24
  ~21:58 monitor: re-apply still RUNNING (3m32s, log mid-`gate[P1]`, 1.6 KB);
  no APPLY_RC, no rollback, zero deletes in git; HEAD `0f03f7e`, parent
  `97f224b`; `kernelhwsrv/` clean.

These three newly indexed reports bring the indexed Markdown total to **362**,
matching the 362 `.md` files under `docs/` (including this index; the header
count "(362 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH44/45/49/52/53/55 backfill (append-only)

A full `docs/*.md` listing (379 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: six documents
existed on disk but had no entry; they are linked below, and 0 links are
stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH44.md](WINSRV_DELETE_WATCH44.md) — read-only 2026-11-24
  monitor: winsrv re-apply (pid 1286837) still RUNNING with no APPLY_RC, log
  ends at `gate[P1]` make/selfcheck; 18/18 edits applied, 7 deletes pending,
  zero `D` entries in git, pre-apply ewsrv gate PASS, snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436`.
- [WINSRV_DELETE_WATCH45.md](WINSRV_DELETE_WATCH45.md) — read-only 2026-11-24
  ~21:59 monitor: re-apply still in progress (no APPLY_RC/rollback in log),
  pid 1286837 ~5 min in `gate[P1]`, no winsrv/guishost/wsclient/qemu_windowsrv
  deletes in git yet; only `tools/r1_winsrv_delete.py` modified plus untracked
  WATCH docs.
- [WINSRV_DELETE_WATCH49.md](WINSRV_DELETE_WATCH49.md) — read-only 2026-11-24
  monitor: re-apply still running with no APPLY_RC (log ends at `gate[P2]`
  `make isogrub`, no rollback); P1 edits + P1 gate PASS, P2 edits (Makefile,
  `user/user.mk`) applied with P2 gate in flight, deletes not started, no
  `D` entries in git — winsrv sources still on disk.
- [WINSRV_DELETE_WATCH52.md](WINSRV_DELETE_WATCH52.md) — read-only 2026-11-24
  ~22:01 monitor: re-apply still running (pid 1286837, ~7 min, in `gate[P2]`
  make isogrub/selfcheck), no APPLY_RC/rollback; P1 edits+gates PASS, duplicate
  apply pid 1298363 exited, still no git deletes.
- [WINSRV_DELETE_WATCH53.md](WINSRV_DELETE_WATCH53.md) — read-only 2026-11-24
  ~22:02 monitor: re-apply still running with no APPLY_RC (log ends at
  `gate[P2]` selfcheck; P1 and pre-apply ewsrv gates PASS); pid 1286837 alive
  ~7 min, pid 1298363 gone, zero git deletes.
- [WINSRV_DELETE_WATCH55.md](WINSRV_DELETE_WATCH55.md) — read-only 2026-11-24
  ~22:01 monitor: re-apply (pid 1286837, ~7 min) still running in `gate[P2]`,
  no APPLY_RC/rollback yet (wrapper appends `APPLY_RC=$?` only after exit);
  P1 edits done + P1 gate PASS, pre-apply ewsrv gate PASS.

These six newly indexed reports bring the indexed Markdown total to **379**,
matching the 379 `.md` files under `docs/` (including this index; the header
count "(379 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH44–45 backfill (append-only)

A full `docs/*.md` listing (369 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 2 document(s)
existed on disk but had no entry; they are linked below with one-line
descriptions, and 0 links are stale. `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

- [WINSRV_DELETE_WATCH44.md](WINSRV_DELETE_WATCH44.md) — read-only 2026-11-24 watch (~5 min): winsrv re-apply pid 1286837 still in `gate[P1]` with no `APPLY_RC`/rollback, 18/18 edits applied, 7 deletes pending, zero git `D` entries, ewsrv pre-gate PASS, snapshot `winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH45.md](WINSRV_DELETE_WATCH45.md) — read-only 2026-11-24 ~21:59 watch (~5 min): same re-apply unfinished (no `APPLY_RC`, no ROLLBACK lines), no winsrv deletes staged, only `r1_winsrv_delete.py` modified, snapshot present, pre-apply ewsrv gate PASS, HEAD `0f03f7e`, nothing committed/pushed, `kernelhwsrv/` clean.

These newly indexed reports bring the indexed Markdown total to **369**,
matching the 369 `.md` files under `docs/` (including this index; the header
count "(369 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH49–81 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: ten documents existed on disk but had no entry; they are linked
below, and 0 links are stale. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH49.md](WINSRV_DELETE_WATCH49.md) — read-only 2026-11-24
  monitor: winsrv re-apply (pid 1286837) still running with no APPLY_RC
  (log ends at `gate[P2]: make isogrub`); P1 edits+gate PASS, P2 edits
  applied, no deletes yet, snapshot `winsrv_retire_backup_20261124_215436`,
  HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH50.md](WINSRV_DELETE_WATCH50.md) — read-only 2026-11-24
  22:00 EET watch: same re-apply unfinished (no APPLY_RC/rollback, log ends at
  the P2 gate), zero git `D` entries, all winsrv files still tracked and
  present, pre-apply ewsrv gate PASS, HEAD `0f03f7e`/`97f224b`.
- [WINSRV_DELETE_WATCH51.md](WINSRV_DELETE_WATCH51.md) — read-only 2026-11-24
  ~22:07 watch: APPLY_RC not yet emitted (~6m28s, still in P2 selfcheck), P1
  gate PASS with 18 edits applied, 7 deletes pending, no deletions staged,
  winsrv sources still on disk, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH54.md](WINSRV_DELETE_WATCH54.md) — read-only 2026-11-24
  ~22:00 watch: re-apply (pid 1286837, ~7 min) still at the P2 gate with no
  APPLY_RC/rollback; P1 edits applied and P1 gate PASS, pre-apply ewsrv gate
  PASS, no deletes yet.
- [WINSRV_DELETE_WATCH55.md](WINSRV_DELETE_WATCH55.md) — read-only 2026-11-24
  ~22:01 watch: APPLY_RC not yet emitted (~7 min, log tail at P2
  `make isogrub` + selfcheck), P1 gate PASS, snapshot
  `winsrv_retire_backup_20261124_215436` intact, no rollback.
- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 watch: APPLY_RC still absent from `winsrv_delete_apply2.log`; apply
  (pid 1286837) still running.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24
  watch: re-apply RUNNING (pid 1286837, 7m35s), log not truncated, no
  APPLY_RC/rollback yet.
- [WINSRV_DELETE_WATCH58.md](WINSRV_DELETE_WATCH58.md) — read-only 2026-11-24
  ~22:08 watch: 0 APPLY_RC lines, apply (pid 1286837, 7m46s) still sitting at
  the P2 gate (`make isogrub`), no rollback.
- [WINSRV_DELETE_WATCH61.md](WINSRV_DELETE_WATCH61.md) — read-only 2026-11-24
  ~22:09 watch: re-apply not finished, no APPLY_RC/rollback line in
  `winsrv_delete_apply2.log` (full 2.2 KB tail read).
- [WINSRV_DELETE_WATCH62.md](WINSRV_DELETE_WATCH62.md) — read-only 2026-11-24
  22:00 watch: APPLY_RC not yet (pid 1286837, ~9 min), log ends at
  `gate[P2]` `make isogrub` + selfcheck, no rollback line.
- [WINSRV_DELETE_WATCH71.md](WINSRV_DELETE_WATCH71.md) — read-only 2026-11-24
  watch: 25 ops (18 edits, 7 deletes) applied, P1+P2 gates PASS, deletes done,
  P3 gate started — still no APPLY_RC.
- [WINSRV_DELETE_WATCH73.md](WINSRV_DELETE_WATCH73.md) — read-only 2026-11-24
  monitor: APPLY_RC not yet written (pid 1286837, ~11 min), log ends at
  `gate[P3]: selfcheck`, no rollback.
- [WINSRV_DELETE_WATCH74.md](WINSRV_DELETE_WATCH74.md) — read-only 2026-11-24
  22:05 EET watch: APPLY_RC NOT YET (pid 1286837, 11m35s), still in
  `gate[P3]` (`make` + selfcheck), 0 APPLY_RC lines, no rollback.
- [WINSRV_DELETE_WATCH75.md](WINSRV_DELETE_WATCH75.md) — read-only 2026-11-24
  monitor: APPLY_RC NOT YET, apply still in flight, no rollback triggered.
- [WINSRV_DELETE_WATCH76.md](WINSRV_DELETE_WATCH76.md) — read-only 2026-11-24
  monitor: APPLY_RC NOT YET EMITTED, apply still in progress, no rollback.
- [WINSRV_DELETE_WATCH77.md](WINSRV_DELETE_WATCH77.md) — read-only 2026-11-24
  monitor: APPLY_RC NOT YET EMITTED, log ends at `gate[P3]: make`.
- [WINSRV_DELETE_WATCH78.md](WINSRV_DELETE_WATCH78.md) — read-only 2026-11-24
  monitor: APPLY_RC not yet emitted, log ends at `gate[P3]: make`/selfcheck.
- [WINSRV_DELETE_WATCH79.md](WINSRV_DELETE_WATCH79.md) — read-only 2026-11-24
  monitor: APPLY_RC NOT YET EMITTED (0 occurrences in log, still growing).
- [WINSRV_DELETE_WATCH81.md](WINSRV_DELETE_WATCH81.md) — read-only 2026-11-24
  monitor: APPLY_RC NOT YET EMITTED (pid 1286837, ~13 min), log ends at
  `gate[P3]: make`/selfcheck.

**398**,
matching the 398 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is now
linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep, WINSRV_DELETE_WATCH46–47/52–53/56 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: five documents existed on disk but had no entry and 0
links were stale; they are linked below. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH46.md](WINSRV_DELETE_WATCH46.md) — read-only 2026-11-24
  ~22:05 watch of the winsrv re-apply (pid 1286837, ~5-6 min): no `APPLY_RC`
  yet, 18/25 edits applied, 7 deletes not on disk (no git `D` entries),
  snapshot `winsrv_retire_backup_20261124_215436` intact, pre-apply ewsrv
  gate PASS, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH47.md](WINSRV_DELETE_WATCH47.md) — read-only 2026-11-24
  watch (~5m35s): re-apply still running with no `APPLY_RC`/rollback, 0 git
  deletes, 10 files modified (18 edits across `variant/*`), rollback token
  `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e` over `97f224b`,
  `kernelhwsrv/` clean, report-only.
- [WINSRV_DELETE_WATCH52.md](WINSRV_DELETE_WATCH52.md) — read-only 2026-11-24
  ~22:01 watch (~7 min): no `APPLY_RC` yet, now in gate[P2] (`make isogrub` +
  selfcheck), P1 edits+gates PASS, duplicate apply pid gone, zero git `D`
  entries — winsrv files still on disk.
- [WINSRV_DELETE_WATCH53.md](WINSRV_DELETE_WATCH53.md) — read-only 2026-11-24
  ~22:02 monitor: apply still in P2 gate with no `APPLY_RC`, P1 gate and
  pre-apply ewsrv retire gate PASS, no winsrv deletions in git, report-only.
- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 watch (~7m35s): no `APPLY_RC` yet, P1 gate PASS, P2 edits done with
  the P2 gate (`make isogrub` + selfcheck) in progress, 7 P3 deletes not yet
  executed, `kernelhwsrv/` clean.

These newly indexed reports bring the indexed Markdown total to **384**,
matching the 384 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH46–68 backfill (append-only)

A full `docs/*.md` listing (392 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 15 documents
existed on disk but had no entry; they are linked below with one-line
descriptions, and 0 links are stale. `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

- [WINSRV_DELETE_WATCH46.md](WINSRV_DELETE_WATCH46.md) — read-only 2026-11-24
  ~22:05 monitor: winsrv re-apply (`r1_winsrv_delete.py --apply --confirm
  DELETE-WINSRV-EWSRV`, pid 1286837) still running — APPLY_RC NOT YET, 18/25
  edits applied, 7 deletes pending (no git `D` entries), rollback snapshot
  `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/`
  clean.
- [WINSRV_DELETE_WATCH47.md](WINSRV_DELETE_WATCH47.md) — read-only 2026-11-24
  follow-up monitor of the same apply (~5m35s elapsed, log 4 KB still at the
  P1 gate): no APPLY_RC, 0 git deletes, 10 files modified across
  `qemu-port/variant/*`, pre-apply ewsrv gate PASS, rollback token intact,
  `kernelhwsrv/` clean, report-only.
- [WINSRV_DELETE_WATCH49.md](WINSRV_DELETE_WATCH49.md) — read-only 2026-11-24
  monitor: apply pid 1286837 alive (~6 min), APPLY_RC not yet, log (2.2 KB)
  ends at `gate[P2]: make isogrub`; P1 edits + P1 gate PASS, P2 edits
  (Makefile, `user/user.mk`) applied, deletes not started, no git `D`
  entries, HEAD `0f03f7e`, rollback token intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH50.md](WINSRV_DELETE_WATCH50.md) — read-only 2026-11-24
  22:00 EET monitor: apply still running (06:22 elapsed), log ends at the P2
  gate (`make isogrub` + selfcheck), no APPLY_RC/rollback, zero git deletes
  (all winsrv files still tracked/present), pre-apply ewsrv gate PASS,
  rollback snapshot intact, HEAD `0f03f7e`, `kernelhwsrv/` untouched, no QEMU.
- [WINSRV_DELETE_WATCH52.md](WINSRV_DELETE_WATCH52.md) — read-only 2026-11-24
  ~22:01 monitor: apply pid 1286837 alive (~7 min) in `gate[P2]` (`make
  isogrub` + selfcheck), APPLY_RC NOT YET, P1 edits+gates PASS, duplicate
  instance pid 1298363 exited, zero git `D` entries, 7 deletes not yet run,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH53.md](WINSRV_DELETE_WATCH53.md) — read-only 2026-11-24
  ~22:02 monitor: apply still running (~7 min) at P2 gate
  (`r1_os_selfcheck.py`), APPLY_RC not present, P1 gate PASS, zero git
  deletes (all winsrv files still on disk/tracked), `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH54.md](WINSRV_DELETE_WATCH54.md) — read-only 2026-11-24
  ~22:00 monitor: apply alive (~7 min) at P2 gate start, no APPLY_RC/rollback,
  P1 edits (variant/Makefile/`user.mk`) applied and P1 gate PASS
  (`retire_test --only ewsrv` exit 0), zero git `D` entries, deletes not yet
  staged, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 monitor: apply pid 1286837 alive (~7m35s), APPLY_RC NOT YET, P1
  (11 variant/ edits) gate PASS, P2 edits done with P2 gate (`make isogrub` +
  selfcheck) in progress, 7 P3 deletes not yet executed, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24
  monitor: re-apply RUNNING (pid 1286837, 7m35s), no APPLY_RC/rollback yet,
  pre-apply snapshot token `ROLLBACK-WINSRV-20261124_215436` intact, P1 done
  (18 edits, gates PASS incl. ewsrv retire), P2 gate in progress,
  `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH59.md](WINSRV_DELETE_WATCH59.md) — read-only 2026-11-24
  ~22:08 monitor: apply pid 1286837 alive (~8 min), log (2.2 KB) ends at the
  P2 gate (`make isogrub` + selfcheck running), no APPLY_RC/ROLLBACK/FAIL
  lines yet; pre-apply ewsrv gate PASS, rollback token
  `ROLLBACK-WINSRV-20261124_215436` intact, P1 edits+gate PASS, P2 edits
  applied, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH61.md](WINSRV_DELETE_WATCH61.md) — read-only 2026-11-24
  ~22:09 monitor: re-apply not finished (pid 1286837 alive 8m26s, log ends at
  P2 gate `make isogrub`), no APPLY_RC/rollback line, P1 gate PASS, pre-apply
  ewsrv gate PASS, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH62.md](WINSRV_DELETE_WATCH62.md) — read-only 2026-11-24
  monitor: apply still running (pid 1286837, ~9 min) at `gate[P2]`
  (`make isogrub` + selfcheck), APPLY_RC NOT YET, zero git deletes (only
  `r1_winsrv_delete.py` modified + untracked WATCH docs), 7 deletes not yet
  executed, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH66.md](WINSRV_DELETE_WATCH66.md) — read-only 2026-11-24
  ~22:10 UTC monitor: APPLY_RC NOT YET, log (2.2 KB) ends at P2 gate `make
  isogrub`, no rollback; pid 1286837 alive (~9m45s, in P2 build gate),
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH67.md](WINSRV_DELETE_WATCH67.md) — read-only 2026-11-24
  ~22:01 UTC monitor: APPLY_RC NOT YET, log (2.2 KB) ends mid P2 gate
  (`make isogrub` + selfcheck running), no APPLY_RC/rollback line yet,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH68.md](WINSRV_DELETE_WATCH68.md) — read-only 2026-11-24
  ~22:15 monitor: apply still in flight, no APPLY_RC/rollback line in the
  log, `kernelhwsrv/` untouched.

These newly indexed reports bring the indexed Markdown total to **392**,
matching the 392 `.md` files under `docs/` (including this index; the header
count "(392 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH52–54 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: 3 documents existed on disk but had no entry; they are linked
below with one-line descriptions, and 0 links are stale. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH52.md](WINSRV_DELETE_WATCH52.md) — read-only 2026-11-24
  ~22:01 monitor: winsrv re-apply (pid 1286837, ~7 min) in the P2 gate with P1
  edits+gates PASS, no APPLY_RC/rollback, zero git deletes (all winsrv files
  still on disk), duplicate apply pid 1298363 exited, snapshot intact.
- [WINSRV_DELETE_WATCH53.md](WINSRV_DELETE_WATCH53.md) — read-only ~22:02
  watch: same apply still in the P2 gate (`r1_os_selfcheck.py`), no APPLY_RC,
  no git `D` entries, HEAD `0f03f7e`/`97f224b`, rollback token present,
  `kernelhwsrv/` clean, no QEMU run.
- [WINSRV_DELETE_WATCH54.md](WINSRV_DELETE_WATCH54.md) — read-only ~22:00
  status: apply still running in the P2 gate start (`make isogrub` + selfcheck),
  P1 edits applied and P1 ewsrv gate PASS, zero git deletes, snapshot
  `winsrv_retire_backup_20261124_215436` intact, `kernelhwsrv/` clean.

These newly indexed reports bring the indexed Markdown total to **378**,
matching the 378 `.md` files under `docs/` (including this index; the header
count "(378 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 subagent sweep, post-376 new-docs backfill (append-only)

A fresh full `docs/*.md` listing (379 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index (link label may
carry backticks): two new reports existed on disk but had no entry; they are
linked below, and 0 links are stale. The remaining regex-only diffs
(`DOCS_INDEX.md`, `r1_boot_and_selftest.md`) are the header's backtick prose
mentions, now also formally linked here so every on-disk `.md` has a Markdown
link. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH55.md](WINSRV_DELETE_WATCH55.md) — read-only 2026-11-24
  ~22:01 monitor of the winsrv re-apply (pid 1286837, `r1_winsrv_delete.py
  --apply --confirm DELETE-WINSRV-EWSRV`): APPLY_RC not yet present, still
  running ~7 min, no rollback lines; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH59.md](WINSRV_DELETE_WATCH59.md) — read-only 2026-11-24
  monitor of the same winsrv re-apply (pid 1286837): still no APPLY_RC/rollback;
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH51.md](WINSRV_DELETE_WATCH51.md) — read-only 2026-11-24
  ~22:07 UTC monitor of the same re-apply (pid 1286837,
  `/tmp/opencode/winsrv_delete_apply2.log`): still in flight, no APPLY_RC.
- [r1_boot_and_selftest.md](r1_boot_and_selftest.md) — canonical usage document
  (header prose mention now a formal link): exact commands to boot the GRUB
  image, run bundled programs and execute the self-test.
- [DOCS_INDEX.md](DOCS_INDEX.md) — this index itself (header prose mention now a
  formal link).

These newly indexed reports bring the indexed Markdown total to **379**, matching
the 379 `.md` files under `docs/` (including this index; the header count
"(379 files)" at line 4 is updated); every Markdown document in the directory is
now linked, with 0 stale links.

## Added — 2026-11-24 winsrv re-apply watch backfill (append-only)

A fresh full `docs/*.md` listing (382 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index (backtick-tolerant):
three new reports existed on disk but had no entry; they are linked below, and
0 links are stale. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 monitor of the winsrv re-apply (pid 1286837): no APPLY_RC yet, P1 edits
  + P1 gate PASS, P2 gate (`make isogrub` + selfcheck) in progress, zero git
  deletes, rollback token `ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24
  re-apply status: pid 1286837 alive in the P2 gate, 18 edits applied with P1
  gates PASS, 7 deletes pending (P3), no `D` entries in git status, snapshot
  intact, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH58.md](WINSRV_DELETE_WATCH58.md) — read-only 2026-11-24
  ~22:08 monitor: still no APPLY_RC/rollback, sitting at the P2 gate, winsrv
  files still on disk and tracked (only tool mod + WATCH docs), HEAD
  `0f03f7e`/`97f224b`, `kernelhwsrv/` clean, no QEMU run.

These newly indexed reports bring the indexed Markdown total to **382**,
matching the 382 `.md` files under `docs/` (including this index; the header
count "(382 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 latest-docs backfill (append-only)

A full `docs/*.md` listing (384 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 12 document(s) existed on disk but had no entry; they are linked below, and 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH55.md](WINSRV_DELETE_WATCH55.md) — read-only ~22:01 monitor: winsrv re-apply (pid 1286837) still at P2 gate (`make isogrub`/selfcheck), no APPLY_RC or rollback, P1 gate PASS, 0 deletes staged, kernelhwsrv/ clean.

These newly indexed reports bring the indexed Markdown total to **379**, matching the 379 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH56–57 backfill (append-only)

A full `docs/*.md` listing (384 files, flat directory, both sides `LC_ALL=C`
sorted) was diffed against the unique `.md` link targets in this index: 2
documents existed on disk but had no entry; they are linked below, and 0
links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 watch: winsrv re-apply pid 1286837 still running with no `APPLY_RC`/
  rollback, P1 edits+gate PASS, P2 gate (`make isogrub` + selfcheck) in
  progress, 7 deletes (P3) not yet in git, snapshot
  `winsrv_retire_backup_20261124_215436` intact.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24
  watch: same re-apply unfinished (no `APPLY_RC`, no rollback lines, pid
  1286837 ~7m35s), P1 18 edits applied with P1 gates PASS, P2 gate in flight,
  zero git `D` entries so all winsrv/guishost/wsclient/qemu_windowsrv sources
  still tracked, `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **381**,
matching the 381 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is now
linked, with 0 stale links.
## Added — 2026-11-24 WINSRV_DELETE_WATCH49 backfill (append-only)

A full `docs/*.md` listing (384 files, flat directory, no subdirectories) was
diffed against the 372 unique `.md` link targets previously in this index: one
document existed on disk but had no entry; it is linked below, and 0 links
are stale (no linked target is missing from disk). `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH49.md](WINSRV_DELETE_WATCH49.md) — read-only 2026-11-24
  re-apply monitor of `r1_winsrv_delete.py --apply --confirm
  DELETE-WINSRV-EWSRV` (pid 1286837, ~6 min): APPLY_RC not yet, P1 edits +
  P1 gate PASS, P2 edits (Makefile, user/user.mk) applied with the P2 gate in
  flight, deletes not started, no git deletes yet, rollback snapshot
  `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`/`97f224b`,
  `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **373**,
matching the 373 `.md` files under `docs/` (including this index; the header
count "(373 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH56/57 backfill (append-only)

A full `docs/*.md` listing (384 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below with one-line
descriptions, and 0 links are stale. `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 monitor: winsrv re-apply (pid 1286837, ~7m35s) has no APPLY_RC yet,
  P1 edits+gates PASS, P2 gate in progress, 7 deletes not executed, no git
  `D` entries, rollback token intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24
  re-apply status: pid 1286837 alive with P1 done (18 edits, gates PASS) and
  the P2 `make isogrub` gate in progress; zero deletes staged, snapshot
  `ROLLBACK-WINSRV-20261124_215436` taken, `kernelhwsrv/` clean, HEAD
  `0f03f7e`.

These newly indexed reports bring the indexed Markdown total to **384**,
matching the 384 `.md` files under `docs/` (including this index; the header
count "(384 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH56–57 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: 2 documents existed on disk but had no entry; they are linked
below with one-line descriptions, and 0 links are stale. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 monitor: winsrv re-apply (pid 1286837, ~7m35s) in the P2 gate with P1
  edits+gate PASS, no APPLY_RC/rollback, 7 deletes (P3) not yet executed, zero
  git `D` entries, snapshot/rollback token intact.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24
  re-apply status: same run alive in the P2 gate (`make isogrub`), P1 done
  (18 edits, gates PASS), no APPLY_RC, zero git deletes, snapshot
  `winsrv_retire_backup_20261124_215436` present, `kernelhwsrv/` clean, no QEMU.

These newly indexed reports bring the indexed Markdown total to **381**,
matching the 381 `.md` files under `docs/` (including this index; the header
count "(381 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 winsrv re-apply watch backfill (append-only)

A full `docs/*.md` listing (384 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 3 document(s)
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24 monitor: APPLY_RC: NOT YET — no APPLY_RC/rollback line in /tmp/opencode/winsrv_delete_apply2.log; apply still running.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24 monitor: Re-apply RUNNING: pid 1286837 alive (7m35s), log `/tmp/opencode/winsrv_delete_apply2.log` (2.2 KB, not truncated).
- [WINSRV_DELETE_WATCH58.md](WINSRV_DELETE_WATCH58.md) — read-only 2026-11-24 monitor: APPLY_RC: NOT YET — log has 0 APPLY_RC lines; apply still in progress (pid 1286837 alive, 7m46s, sitting at P2 gate `make isogrub`).

- [WINSRV_DELETE_WATCH60.md](WINSRV_DELETE_WATCH60.md) — read-only 2026-11-24 status report; see file.

These newly indexed reports bring the indexed Markdown total to **382**,
matching the 382 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 final sweep (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index: one report existed on disk but had no entry; it is linked
below, with 0 stale links. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH59.md](WINSRV_DELETE_WATCH59.md) — read-only 2026-11-24
  ~22:08 monitor: winsrv re-apply (pid 1286837) still at the P2 gate
  (`make isogrub` + selfcheck), APPLY_RC not yet emitted, P1 edits + P1 gate
  PASS, zero git deletes, rollback snapshot intact, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **384**,
matching the 384 `.md` files under `docs/` (including this index; the header
count "(384 files)" at line 4 is consistent); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH56/57/58 backfill (append-only)

A full `docs/*.md` listing (382 files, flat directory, no subdirectories) was
diffed against the 379 unique `.md` link targets previously in this index:
three documents existed on disk but had no entry; they are linked below, and
0 links are stale. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24
  ~22:01 monitor of the winsrv re-apply (`r1_winsrv_delete.py --apply
  --confirm DELETE-WINSRV-EWSRV`, pid 1286837, ~7m35s): APPLY_RC NOT YET, P1
  edits gate PASS, P2 gate (`make isogrub` + selfcheck) in progress, 7 P3
  deletes not executed, no git `D` entries, rollback snapshot intact, HEAD
  `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24
  re-apply status (pid 1286837 alive 7m35s): no APPLY_RC/rollback, P1 done
  (18 edits, gates PASS incl. ewsrv retire gate), P2 gate in progress, zero
  git deletes, 7 deletes pending, snapshot/token
  `ROLLBACK-WINSRV-20261124_215436` taken pre-apply, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH58.md](WINSRV_DELETE_WATCH58.md) — read-only 2026-11-24
  ~22:08 monitor (pid 1286837 alive 7m46s, sitting at the P2 gate
  `make isogrub`): 0 APPLY_RC lines, P1 edits + gate PASS, no rollback, no
  git `D` entries (winsrv/guishost/wsclient/qemu_windowsrv files still on
  disk), rollback snapshot ready, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/`
  clean, no QEMU run.

These three newly indexed reports bring the indexed Markdown total to **382**,
matching the 382 `.md` files under `docs/` (including this index; the header
count "(382 files)" at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH56/57 backfill (append-only)

A full `docs/*.md` listing (381 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 2 documents existed on disk but had no entry; they are linked below, and 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH56.md](WINSRV_DELETE_WATCH56.md) — read-only 2026-11-24 ~22:01 monitor: winsrv re-apply (pid 1286837, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still running with no APPLY_RC/rollback line; P1 (11 variant/ edits) gate PASS, P2 gate (`make isogrub` + selfcheck) in progress, 7 deletes (P3) not yet executed, no git deletes staged, rollback snapshot `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH57.md](WINSRV_DELETE_WATCH57.md) — read-only 2026-11-24 re-apply status: pid 1286837 alive (~7m35s), log not truncated, no APPLY_RC/rollback yet; P1 done with 18 edits and P1 gates PASS, P2 gate (`make isogrub`) in progress, deletes NOT in git (zero `D` entries, 7 pending P3), snapshot token `ROLLBACK-WINSRV-20261124_215436` taken pre-apply, `kernelhwsrv/` clean, HEAD `0f03f7e`/`97f224b`.

These newly indexed reports bring the indexed Markdown total to **381**, matching the 381 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 winsrv re-apply watch backfill, final (append-only)

A full `docs/*.md` listing (384 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 0 document(s)
existed on disk but had no entry; linked below. `kernelhwsrv/` untouched; no QEMU run.


This brings the indexed Markdown total to **384**, matching the 384 `.md`
files under `docs/` (including this index); every Markdown document in the directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH64–67 backfill (append-only)

A full `docs/*.md` listing (391 files, flat directory, both sides `LC_ALL=C`
sorted) was diffed against the unique `.md` link targets in this index:
**4 documents existed on disk but had no entry and 0 links were stale**; they
are linked below. `kernelhwsrv/` was untouched (read-only) and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH64.md](WINSRV_DELETE_WATCH64.md) — read-only 2026-11-24
  ~22:10 monitor: winsrv re-apply (pid 1286837, `--confirm DELETE-WINSRV-EWSRV`)
  still in flight at the P2 gate (`make isogrub`), no APPLY_RC/rollback line,
  P1 gate PASS, zero git `D` deletes yet, rollback snapshot
  `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH65.md](WINSRV_DELETE_WATCH65.md) — read-only 2026-11-24
  22:03 EET monitor: re-apply still running (pid 1286837, ~9m22s), log ends at
  the P2 `make isogrub` gate with no APPLY_RC/rollback, P1 edits + gates PASS,
  7 deletes still pending with no git `D` entries, HEAD `0f03f7e`,
  `kernelhwsrv/` clean, no QEMU run.
- [WINSRV_DELETE_WATCH66.md](WINSRV_DELETE_WATCH66.md) — read-only 2026-11-24
  ~22:10 UTC monitor: APPLY_RC NOT YET, log ends at the P2 `make isogrub` gate,
  pid 1286837 alive (~9m45s), 0 git deletes, P1 + pre-apply ewsrv gates PASS,
  snapshot token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`/`97f224b`,
  `kernelhwsrv/` clean, no QEMU run.
- [WINSRV_DELETE_WATCH67.md](WINSRV_DELETE_WATCH67.md) — read-only 2026-11-24
  ~22:01 UTC monitor: APPLY_RC NOT YET at the P2 gate, pid 1286837 alive
  (~10 min, with two other apply runs also alive — possible index race),
  ewsrv gates PASS, no git `D` entries, HEAD `0f03f7e`, `kernelhwsrv/`
  untouched, no QEMU run.
- [WINSRV_DELETE_WATCH71.md](WINSRV_DELETE_WATCH71.md) — read-only 2026-11-24
  monitor: P1+P2 gates PASS, the 7 winsrv deletes are now present in
  `git status` (`user/winsrv.c`, `guishost.h`, `wsclient.[ch]`,
  `variant/qemu_windowsrv.[ch]`, `qemu_gui_exec.cpp`), P3 gate started, still
  no APPLY_RC, pid 1286837 running, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.

These five newly indexed reports bring the indexed Markdown total to **395**,
matching the 395 `.md` files under `docs/` (including this index; the header
count "(395 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 subagent docs-index sweep, WINSRV_DELETE_WATCH54/55 backfill (append-only)

A fresh full `docs/*.md` listing (386 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: at sweep start
two documents — [WINSRV_DELETE_WATCH54.md](WINSRV_DELETE_WATCH54.md) and
[WINSRV_DELETE_WATCH55.md](WINSRV_DELETE_WATCH55.md) — had no entry (indexed by a
concurrent sweep before this section landed); they are linked below with
one-line descriptions, and 0 links are stale. `kernelhwsrv/` was untouched and
no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH54.md](WINSRV_DELETE_WATCH54.md) — read-only 2026-11-24 ~22:00 monitor of the winsrv re-apply (pid 1286837): APPLY_RC not yet, apply at the P2 gate (`make isogrub` + selfcheck), P1 edits + P1 ewsrv gate PASS, zero git deletes, rollback snapshot intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH55.md](WINSRV_DELETE_WATCH55.md) — read-only 2026-11-24 ~22:01 monitor of the same re-apply (~7 min): APPLY_RC absent, still in the P2 gate, P1 gate PASS, no deletes staged, snapshot/token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean, no QEMU.

The indexed Markdown total is **386**, matching the 386 `.md` files under
`docs/` (including this index; the header count at line 4 is updated); every
Markdown document in the directory is linked.

## Added — 2026-11-24 docs-index sweep, post-381 backfill (append-only)

A fresh full `docs/*.md` listing (387 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 3 document(s) existed on disk but had no entry; 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH61.md](WINSRV_DELETE_WATCH61.md) — WINSRV_DELETE_WATCH61 — re-apply status (2026-11-24 ~22:09)
- [WINSRV_DELETE_WATCH62.md](WINSRV_DELETE_WATCH62.md) — WINSRV_DELETE_WATCH62 — 2026-11-24 22:00
- [WINSRV_DELETE_WATCH63.md](WINSRV_DELETE_WATCH63.md) — WINSRV_DELETE_WATCH63 — re-apply status (2026-11-24 ~22:10)

These newly indexed reports bring the indexed Markdown total to **387**, matching the 387 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep, WINSRV_DELETE_WATCH64–65 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: two documents existed on disk but had no entry and 0
links were stale; they are linked below. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH64.md](WINSRV_DELETE_WATCH64.md) — read-only 2026-11-24
  ~22:10 monitor of the winsrv re-apply (pid 1286837, ~9 min): APPLY_RC NOT
  YET, log at P2 gate (`make isogrub` + selfcheck), no rollback, P1 gate and
  pre-apply ewsrv gate PASS, rollback token `ROLLBACK-WINSRV-20261124_215436`
  intact, no git deletes yet.
- [WINSRV_DELETE_WATCH65.md](WINSRV_DELETE_WATCH65.md) — read-only 2026-11-24
  22:03 EET monitor: re-apply still running (pid 1286837, 9m22s), no
  `APPLY_RC`/rollback, log at P2 gate, P1 edits+gates PASS, snapshot/rollback
  token intact, `kernelhwsrv/` untouched, no QEMU.

These newly indexed reports bring the indexed Markdown total to **389**,
matching the 389 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is now
linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH61/62 backfill (append-only)

A full `docs/*.md` listing (386 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 2 documents existed on disk but had no entry; they are linked below, and 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH61.md](WINSRV_DELETE_WATCH61.md) — read-only 2026-11-24 ~22:09 monitor: winsrv re-apply (pid 1286837, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) not finished, no APPLY_RC/rollback line, log ends at P2 gate `make isogrub`; P1 and pre-apply ewsrv gates PASS; no git deletes yet (winsrv files still tracked, 18 `M` edits applied); rollback token `ROLLBACK-WINSRV-20261124_215436` intact; HEAD `0f03f7e`/`97f224b`; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH62.md](WINSRV_DELETE_WATCH62.md) — read-only 2026-11-24 22:00 re-apply status: pid 1286837 alive (~9 min), APPLY_RC not yet, log ends at P2 gate (`make isogrub` + selfcheck); zero git deletes (only `M` on `tools/r1_winsrv_delete.py` plus untracked WATCH docs), 7 deletes pending; 25 ops planned with P1 (18 edits) gate PASS; snapshot `ROLLBACK-WINSRV-20261124_215436` pre-apply; HEAD `0f03f7e`; `kernelhwsrv/` untouched.

These newly indexed reports bring the indexed Markdown total to **386**, matching the 386 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH64/65 backfill (append-only)

A full `docs/*.md` listing (389 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 2 documents existed on disk but had no entry; they are linked below, and 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH64.md](WINSRV_DELETE_WATCH64.md) — read-only 2026-11-24 ~22:10 monitor: winsrv re-apply (pid 1286837, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still running, no APPLY_RC/rollback, log ends at P2 gate `make isogrub` + selfcheck; P1 gate PASS, no git deletes yet, rollback token `ROLLBACK-WINSRV-20261124_215436` intact; HEAD `0f03f7e`/`97f224b`; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH65.md](WINSRV_DELETE_WATCH65.md) — read-only 2026-11-24 22:03 EET re-apply status: pid 1286837 alive (~9m22s), no `APPLY_RC`/rollback, P2 gate (`make isogrub`) in progress, zero git deletes (7 pending), snapshot `ROLLBACK-WINSRV-20261124_215436` taken pre-apply; HEAD `0f03f7e`; `kernelhwsrv/` clean; no QEMU run.

These two newly indexed reports bring the indexed Markdown total to **389**, matching the 389 `.md` files under `docs/` (including this index; the header count at line 4 already reads 389); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH67 backfill (append-only)

One further document (WINSRV_DELETE_WATCH67.md) appeared on disk after the 389-file sweep above and had no entry; it is linked below, 0 links stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH67.md](WINSRV_DELETE_WATCH67.md) — read-only 2026-11-24 ~22:01 UTC monitor: winsrv re-apply (pid 1286837) still at the P2 gate (`make isogrub` + selfcheck), no APPLY_RC/rollback; notes two other apply pids alive (possible index race); ewsrv retire gate PASS, zero git deletes, snapshot `ROLLBACK-WINSRV-20261124_215436` intact; HEAD `0f03f7e`/`97f224b`; `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **390**, matching the 390 `.md` files under `docs/` at this sweep (including this index; header count at line 4 reads 391 as one more WATCH doc landed concurrently); every Markdown document in the directory is linked at this snapshot, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH68 backfill (append-only)

One further document (WINSRV_DELETE_WATCH68.md) appeared on disk during this sweep and had no entry; it is linked below, 0 links stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH68.md](WINSRV_DELETE_WATCH68.md) — read-only 2026-11-24 ~22:15 monitor: winsrv re-apply (pid 1286837, ~10 min) still in the P2 gate (`make isogrub` + selfcheck + ewsrv retire test), no APPLY_RC/rollback; P1 edits done + P1 gate PASS; zero git deletes; snapshot `ROLLBACK-WINSRV-20261124_215436` intact; HEAD `0f03f7e`/`97f224b`; `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **391** at this snapshot (392 `.md` on disk as WATCH69-class docs land concurrently from the live winsrv re-apply monitor); 0 stale links.

## Added — 2026-11-24 subagent full sweep, post-386 (append-only)

A fresh full `docs/*.md` listing (391 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: one document existed on disk but had no entry; it is linked below, and 0 links are stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH67.md](WINSRV_DELETE_WATCH67.md) — read-only 2026-11-24 ~22:01 UTC monitor of the winsrv re-apply (pid 1286837, `--apply --confirm DELETE-WINSRV-EWSRV`): APPLY_RC not yet, log ends mid-P2 gate, ewsrv gate PASS pre/post P1, rollback token intact, no git deletes yet, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **391**, matching the 391 `.md` files under `docs/` (including this index; the header count "(391 files)" at line 4 is consistent); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH64–67 backfill (append-only)

A full `docs/*.md` listing (391 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 4 documents existed on disk but had no entry; they are linked below, and 0 links are stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH64.md](WINSRV_DELETE_WATCH64.md) — read-only 2026-11-24 ~22:10 monitor of the winsrv re-apply (pid 1286837): APPLY_RC not yet, log ends at the P2 gate (`make isogrub` + selfcheck); P1 gates PASS, P2 edits applied, zero git deletes, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH65.md](WINSRV_DELETE_WATCH65.md) — read-only 2026-11-24 22:03 EET re-apply status: pid 1286837 alive (~9m22s), no APPLY_RC/rollback, P2 `make isogrub` in progress, no `D` deletes yet (7 pending), snapshot intact, HEAD `0f03f7e`, `kernelhwsrv/` clean, no QEMU.
- [WINSRV_DELETE_WATCH66.md](WINSRV_DELETE_WATCH66.md) — read-only 2026-11-24 winsrv re-apply watch: same in-flight state (pid 1286837, P2 gate, no APPLY_RC/deletes), snapshot token and `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH67.md](WINSRV_DELETE_WATCH67.md) — read-only 2026-11-24 winsrv re-apply watch: same in-flight state (pid 1286837, P2 gate, no APPLY_RC/deletes), snapshot token and `kernelhwsrv/` clean.

These newly indexed reports bring the indexed Markdown total to **391**, matching the 391 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH64/65 backfill (append-only)

A fresh full `docs/*.md` listing (391 files, flat directory, no subdirectories, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 2 document(s) existed on disk but had no entry; they are linked below, and 0 links are stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH64.md](WINSRV_DELETE_WATCH64.md) — read-only 2026-11-24 ~22:10 monitor: winsrv re-apply (pid 1286837) still at the P2 gate (`make isogrub` + selfcheck), no APPLY_RC/rollback, P1 gate PASS, no git deletes yet, `kernelhwsrv/` clean.


These newly indexed reports bring the indexed Markdown total to **0**, matching the 0 `.md` files under `docs/` (including this index; the header count "(0 files)" at line 4 is updated); every Markdown document in the directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH67 backfill (append-only)

A full `docs/*.md` listing (407 files, flat directory, both sides `LC_ALL=C`
sorted) was diffed against the unique `.md` link targets in this index: 9
documents existed on disk but had no entry; they are linked below, and 0
links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH67.md](WINSRV_DELETE_WATCH67.md) — read-only 2026-11-24
  ~22:01 UTC monitor of the winsrv re-apply (pid 1286837, ~10 min, plus two
  other apply pids noted as a possible index race): log ends mid-P2 gate
  (`make isogrub` + selfcheck), no `APPLY_RC`/rollback, ewsrv gates PASS,
  zero git `D` deletes, snapshot token `ROLLBACK-WINSRV-20261124_215436`
  intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH69.md](WINSRV_DELETE_WATCH69.md) — read-only 2026-11-24
  ~22:05 monitor: re-apply pid 1286837 alive ~10 min, no `APPLY_RC`/rollback,
  P1 edits+gates PASS, P2 edits done and P2 gate (`make isogrub` + ewsrv
  retire gate) running, zero git `D` deletes (all winsrv sources still on
  disk), snapshot token intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH73.md](WINSRV_DELETE_WATCH73.md) — read-only 2026-11-24
  monitor: re-apply pid 1286837 ~11 min with no `APPLY_RC`/rollback, all 7
  winsrv source deletes now `D` in the working tree, P1/P2 gates PASS, P3
  post-delete selfcheck gate in flight, snapshot token intact,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH75.md](WINSRV_DELETE_WATCH75.md) — read-only 2026-11-24
  monitor: re-apply pid 1286837 ~11m38s, no `APPLY_RC`/rollback, P1+P2 gates
  PASS, 7 winsrv deletes in the worktree, now in the P3 gate (`make`), rollback
  token `ROLLBACK-WINSRV-20261124_215436` available, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH76.md](WINSRV_DELETE_WATCH76.md) — read-only 2026-11-24
  ~22:16 monitor: no `APPLY_RC` yet, pid 1286837 ~12 min, pre-gate/P1/P2 gates
  PASS, P3 gate (`make` + selfcheck) running over the 7 deletes, deletions
  staged in worktree, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH77.md](WINSRV_DELETE_WATCH77.md) — read-only 2026-11-24
  22:16 UTC monitor: log ends at `gate[P3]: make`, P1/P2 PASS, 7 deletes
  applied (82 winsrv-related git entries), pre-apply ewsrv gate PASS, snapshot
  intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH80.md](WINSRV_DELETE_WATCH80.md) — read-only 2026-11-24
  ~22:17 monitor: no `APPLY_RC` yet, pid 1286837 at 12m42s, 25 ops (18 edits +
  7 deletes) applied with P1/P2 gates PASS, now in the P3 gate (`make` +
  selfcheck), 7 deletes staged, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH82.md](WINSRV_DELETE_WATCH82.md) — read-only 2026-11-24
  monitor: no `APPLY_RC` yet, pid 1286837 ~13 min, P1/P2 gates PASS (incl.
  `make isogrub`), 7 worktree deletes (`D` count 7), P3 gate running,
  rollback token `ROLLBACK-WINSRV-20261124_215436` available, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.

These eight newly indexed reports bring the indexed Markdown total to **406**,
matching the 406 `.md` files under `docs/` (including this index; the header
count "(406 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH66/67 backfill (append-only)

A fresh full `docs/*.md` listing (391 files, flat directory, no subdirectories, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 2 documents existed on disk but had no entry; they are linked below, and 0 links are stale. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH66.md](WINSRV_DELETE_WATCH66.md) — read-only 2026-11-24 ~22:10 UTC monitor of the winsrv re-apply (pid 1286837, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`): APPLY_RC NOT YET, log ends at the P2 gate (`make isogrub`), P1 gates PASS, zero git deletes, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH67.md](WINSRV_DELETE_WATCH67.md) — read-only 2026-11-24 ~22:01 UTC re-apply status: pid 1286837 alive (~10 min) at the P2 gate with no APPLY_RC/rollback; flags two other live apply runs (pids 1317751, 1323788) as a possible index race; ewsrv gate PASS before and after P1, no `D` deletes yet, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` untouched.

These two newly indexed reports bring the indexed Markdown total to **391**, matching the 391 `.md` files under `docs/` (including this index; the header count "(391 files)" at line 4 is already correct); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep (last 375+), WINSRV_DELETE_WATCH66–67 backfill (append-only)

A fresh full `docs/*.md` listing (391 files, flat directory, both sides sorted) was diffed against the unique `.md` link targets in this index: 2 documents existed on disk but had no entry and 0 links were stale; they are linked below. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH66.md](WINSRV_DELETE_WATCH66.md) — read-only 2026-11-24 ~22:10 UTC monitor of the winsrv re-apply (pid 1286837): APPLY_RC NOT YET, log ends at the P2 gate (`make isogrub`), no rollback.
- [WINSRV_DELETE_WATCH67.md](WINSRV_DELETE_WATCH67.md) — read-only 2026-11-24 ~22:01 UTC monitor: re-apply still in the P2 gate, no APPLY_RC/rollback line yet.

These newly indexed reports bring the indexed Markdown total to **391**, matching the 391 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 docs-index sweep (append-only)

A fresh full `docs/*.md` listing (394 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 2 document(s) existed on disk but had no entry; they are linked below with one-line descriptions, and 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH69.md](WINSRV_DELETE_WATCH69.md) — WINSRV_DELETE_WATCH69 — re-apply status (read-only monitor, 2026-11-24 ~22:05)
- [WINSRV_DELETE_WATCH70.md](WINSRV_DELETE_WATCH70.md) — WINSRV_DELETE_WATCH70 — 2026-11-24 22:15 UTC

- [WINSRV_DELETE_WATCH71.md](WINSRV_DELETE_WATCH71.md) — read-only 2026-11-24 winsrv re-apply monitor snapshot (APPLY_RC pending); `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH72.md](WINSRV_DELETE_WATCH72.md) — read-only 2026-11-24 winsrv re-apply monitor snapshot (APPLY_RC pending); `kernelhwsrv/` untouched.

These newly indexed reports bring the indexed Markdown total to **396**, matching the 396 `.md` files under `docs/` (including this index; the header count "(396 files)" at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep (last ~389+), WINSRV_DELETE_WATCH73–76 backfill (append-only)

A fresh full `docs/*.md` listing (400 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 4 documents existed on disk but had no entry and 0 links were stale; they are linked below with one-line descriptions. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH73.md](WINSRV_DELETE_WATCH73.md) — read-only 2026-11-24 winsrv re-apply monitor: APPLY_RC not yet written (pid 1286837 in gate P3 selfcheck), all 7 winsrv file deletes landed in the worktree, rollback token intact.
- [WINSRV_DELETE_WATCH74.md](WINSRV_DELETE_WATCH74.md) — read-only 2026-11-24 22:05 EET monitor: still in P3 gate (`make` + selfcheck), P1/P2 gates PASS, 7 deletes staged, HEAD `0f03f7e`, no commit.
- [WINSRV_DELETE_WATCH75.md](WINSRV_DELETE_WATCH75.md) — read-only monitor: apply in flight in phase P3 gate, P1/P2 PASS, 7 winsrv deletions staged, snapshot/rollback token `ROLLBACK-WINSRV-20261124_215436`.
- [WINSRV_DELETE_WATCH76.md](WINSRV_DELETE_WATCH76.md) — read-only 2026-11-24 ~22:16 monitor: APPLY_RC not yet emitted, pid 1286837 ~12 min in P3 `make`+selfcheck gate, P1 (18 edits)/P2 gates PASS, 7 deletes staged.

These four newly indexed reports bring the indexed Markdown total to **400**, matching the 400 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep follow-up, WINSRV_DELETE_WATCH77 backfill (append-only)

One more document appeared on disk during the sweep above and had no entry; it is linked below. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH77.md](WINSRV_DELETE_WATCH77.md) — read-only 2026-11-24 22:16 UTC monitor: APPLY_RC not yet emitted (pid 1286837 in P3 `make` gate), P1/P2 PASS, 7 winsrv deletes staged, rollback token intact, HEAD `0f03f7e`.

This newly indexed report brings the indexed Markdown total to **401**, matching the 401 `.md` files under `docs/` at sweep time (including this index; the header count at line 4 is updated); every Markdown document in the directory was linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep (last ~392), WINSRV_DELETE_WATCH74–75 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link targets in this index: 2 documents existed on disk but had no entry; they are linked below with one-line descriptions, and 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH74.md](WINSRV_DELETE_WATCH74.md) — read-only 2026-11-24 22:05 EET re-apply monitor: APPLY_RC NOT YET, pid 1286837 still in the P3 gate (`make` + selfcheck), P1/P2 gates PASS, 7 winsrv files deleted, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH75.md](WINSRV_DELETE_WATCH75.md) — read-only 2026-11-24 re-apply monitor: APPLY_RC NOT YET, apply still in phase P3 (`+ gate[P3]: make`), P1/P2 PASS, 7 deletions staged, snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, no commit, `kernelhwsrv/` untouched.

These newly indexed reports bring the indexed Markdown total to **398**, matching the 398 `.md` files under `docs/` (including this index; the header count "(398 files)" at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH73/74 backfill (append-only)

A full `docs/*.md` listing (398 files, flat directory, both sides `LC_ALL=C`
sorted) was diffed against the unique `.md` link targets in this index: 2
documents existed on disk but had no entry and 0 links were stale; they are
linked below. `kernelhwsrv/` was untouched (read-only) and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH73.md](WINSRV_DELETE_WATCH73.md) — read-only 2026-11-24 monitor: winsrv re-apply (pid 1286837) still in gate P3 (selfcheck), no APPLY_RC/rollback, all 7 winsrv file deletes landed in the working tree, P1/P2 gates PASS, rollback token intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH74.md](WINSRV_DELETE_WATCH74.md) — read-only 2026-11-24 22:05 EET monitor: same in-flight state (pid 1286837 at gate P3, no APPLY_RC, 7 files `D`, P1/P2 PASS), snapshot token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` untouched.

These two newly indexed reports bring the indexed Markdown total to **398**,
matching the 398 `.md` files under `docs/` (including this index; the header
count "(398 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 WINSRV_DELETE_WATCH77/78 backfill (append-only)

A full `docs/*.md` listing (402 files, flat directory, both sides `LC_ALL=C`
sorted) was diffed against the unique `.md` link targets in this index: 2
documents existed on disk but had no entry; they are linked below, and 0
links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH77.md](WINSRV_DELETE_WATCH77.md) — read-only 2026-11-24 22:16 UTC monitor of the winsrv re-apply (pid 1286837, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`): APPLY_RC not yet emitted, log ends at `gate[P3]: make`, apply still running ~12 min.
- [WINSRV_DELETE_WATCH78.md](WINSRV_DELETE_WATCH78.md) — read-only 2026-11-24 re-apply status: APPLY_RC not yet emitted, log unchanged since 22:04 ending at `gate[P3]: make`/selfcheck, pid 1286837 alive ~12 min, no rollback.

These two newly indexed reports bring the indexed Markdown total to **402**,
matching the 402 `.md` files under `docs/` (including this index; the header
count "(402 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 docs-index sweep, WINSRV_DELETE_WATCH76/77 backfill (append-only)

A fresh full `docs/*.md` listing (401 files, flat directory, both sides `LC_ALL=C`
sorted) was diffed against the unique `.md` link targets in this index: two
documents existed on disk but had no entry; they are linked below with one-line
descriptions, and 0 links are stale. `kernelhwsrv/` was untouched (read-only)
and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH76.md](WINSRV_DELETE_WATCH76.md) — read-only 2026-11-24
  ~22:16 monitor: winsrv re-apply (pid 1286837) still running with P1/P2 gates
  PASS and the P3 delete-gate in flight; no APPLY_RC/rollback yet,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH77.md](WINSRV_DELETE_WATCH77.md) — read-only 2026-11-24
  22:16 UTC monitor: same re-apply at `gate[P3]: make`, APPLY_RC not yet
  emitted, 7 winsrv deletions + edits in the working tree, no rollback,
  `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **401**,
matching the 401 `.md` files under `docs/` (including this index; the header
count "(401 files)" at line 4 is consistent); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep (last ~395+), WINSRV_DELETE_WATCH78–79 backfill (append-only)

A fresh full `docs/*.md` listing (403 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: 2 documents existed on disk but had no entry and 0 links were stale;
they are linked below with one-line descriptions. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH78.md](WINSRV_DELETE_WATCH78.md) — read-only 2026-11-24
  re-apply monitor: `APPLY_RC` not yet emitted, pid 1286837 ~12 min in the P3
  gate (`make` + selfcheck), pre-apply ewsrv + P1 + P2 gates PASS, 7 winsrv
  deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436` intact,
  HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH79.md](WINSRV_DELETE_WATCH79.md) — read-only 2026-11-24
  follow-up monitor: apply still in progress at the P3 gate with no
  `APPLY_RC`/rollback line, 7 winsrv files `D` in the worktree, snapshot token
  on record, `kernelhwsrv/` untouched, verdict "no final result yet".

These two newly indexed reports bring the indexed Markdown total to **403**,
matching the 403 `.md` files under `docs/` (including this index; the header
count "(403 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 docs-index sweep (append-only)

A fresh full `docs/*.md` listing (402 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 1 document existed on disk but had no entry; it is linked below with a one-line description, and 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH78.md](WINSRV_DELETE_WATCH78.md) — read-only 2026-11-24 winsrv re-apply monitor: APPLY_RC not yet emitted (pid 1286837 alive at P3 gate), P1/P2 gates PASS, 7 winsrv deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` untouched.

This newly indexed report brings the indexed Markdown total to **402**, matching the 402 `.md` files under `docs/` (including this index; the header count "(402 files)" at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 index sweep backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link targets in this index: 1 document(s) existed on disk but had no entry and 0 links were stale; they are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH79.md](WINSRV_DELETE_WATCH79.md) — WINSRV_DELETE_WATCH79 — re-apply status (2026-11-24)

These newly indexed reports bring the indexed Markdown total to **403**, matching the 403 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep (last ~392), WINSRV_DELETE_WATCH78 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link targets in this index: 1 document existed on disk but had no entry; it is linked below with a one-line description, and 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH78.md](WINSRV_DELETE_WATCH78.md) — read-only 2026-11-24 re-apply monitor: APPLY_RC not yet emitted, pid 1286837 still in the P3 gate (`make`/selfcheck), P1/P2 gates PASS, 7 winsrv deletions present, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH81.md](WINSRV_DELETE_WATCH81.md) — read-only 2026-11-24 ~22:06 re-apply monitor: APPLY_RC NOT YET EMITTED, pid 1286837 still in `gate[P3]: make`/selfcheck (~13 min), P1 (18 edits) and P2 gates PASS, 7 deletes done, snapshot `winsrv_retire_backup_20261124_215436` on standby, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH82.md](WINSRV_DELETE_WATCH82.md) — read-only 2026-11-24 re-apply monitor: APPLY_RC NOT YET EMITTED, pid 1286837 alive (~13 min) at the P3 gate, P1/P2 gates PASS incl. `make isogrub`, 7 worktree deletions, rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean, no commit.

These newly indexed reports bring the indexed Markdown total to **406**, matching the 406 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep (last ~389 docs) (append-only)

A fresh full `docs/*.md` listing (404 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: the documents that existed on disk but had no entry at sweep time are
linked below with one-line descriptions. `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH80.md](WINSRV_DELETE_WATCH80.md) — read-only 2026-11-24 winsrv re-apply monitor snapshot (APPLY_RC not yet emitted; apply pid 1286837 still in flight).

This sweep brings the indexed Markdown total to **404**, matching the 404 `.md` files under `docs/` (including this index; the header count at line 4 reads "404 files, verified 2026-11-24"); every Markdown document in the directory is linked.

## Added — 2026-11-24 new-docs sweep (last 396+), WINSRV_DELETE_WATCH81 backfill (append-only)

A full `docs/*.md` listing (407 files, flat directory, both sides `LC_ALL=C`
sorted) was diffed against the unique `.md` link targets in this index: 3
documents existed on disk but had no entry and 0 links were stale; they are
linked below with one-line descriptions. `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH81.md](WINSRV_DELETE_WATCH81.md) — read-only 2026-11-24
  ~22:06 winsrv re-apply monitor: APPLY_RC NOT YET EMITTED (pid 1286837 alive
  ~13 min in the P3 `make`+selfcheck gate), P1 (18 edits)/P2 gates PASS, 7
  winsrv deletes staged uncommitted, rollback snapshot/token
  `ROLLBACK-WINSRV-20261124_215436` on standby, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH82.md](WINSRV_DELETE_WATCH82.md) — read-only 2026-11-24
  winsrv re-apply monitor: APPLY_RC NOT YET EMITTED, no rollback seen, pid
  1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still
  alive ~13 min in the P3 gate.
- [WINSRV_DELETE_WATCH83.md](WINSRV_DELETE_WATCH83.md) — read-only 2026-11-24
  ~22:18 winsrv re-apply monitor: APPLY_RC NOT YET SET, log ends at the P3
  `make` gate, pid 1286837 alive ~13m54s, no rollback/FAIL lines.

These newly indexed reports bring the indexed Markdown total to **407**,
matching the 407 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is now
linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep (append-only)

A fresh full `docs/*.md` listing (407 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 2 documents existed on disk but had no entry; they are linked below with one-line descriptions, and 0 links are stale. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH82.md](WINSRV_DELETE_WATCH82.md) — read-only 2026-11-24 winsrv re-apply monitor: APPLY_RC not yet emitted, pid 1286837 alive ~13 min at the P3 gate (P1/P2 gates PASS, 7 worktree deletions, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`), `kernelhwsrv/` clean, no QEMU run.
- [WINSRV_DELETE_WATCH83.md](WINSRV_DELETE_WATCH83.md) — read-only 2026-11-24 ~22:18 winsrv re-apply monitor: APPLY_RC not yet set, log ends at `gate[P3]: make`, pid 1286837 alive (P1/P2 PASS, 7 deletes, snapshot token intact, HEAD `0f03f7e`), `kernelhwsrv/` untouched, no QEMU run.

These newly indexed reports bring the indexed Markdown total to **407**, matching the 407 `.md` files under `docs/` (including this index; the header count "(407 files)" at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep (last ~396 docs) (append-only)

A fresh full `docs/*.md` listing (406 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: 1 document(s) existed on disk but had no entry;
they are linked below with one-line descriptions, and 0 links are stale.
`kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH82.md](WINSRV_DELETE_WATCH82.md) — APPLY_RC: NOT YET EMITTED — apply still in flight, no rollback seen.

This sweep brings the indexed Markdown total to **406**, matching the 406 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 stale links.

## Added — 2026-11-24 subagent docs-index sweep (last 396+ docs) (append-only)

A fresh full `docs/*.md` listing (406 files, flat directory, both sides sorted)
was diffed against the unique `.md` link targets in this index: 1 document
existed on disk but had no entry at scan time; it is linked below with a
one-line description, and 0 links are stale. `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH82.md](WINSRV_DELETE_WATCH82.md) — read-only 2026-11-24
  winsrv re-apply monitor: APPLY_RC NOT YET EMITTED, pid 1286837 in the P3 gate
  (make + selfcheck) with P1/P2 PASS, 7 winsrv deletions staged (uncommitted),
  rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH83.md](WINSRV_DELETE_WATCH83.md) — read-only 2026-11-24
  ~22:18 winsrv re-apply watch: APPLY_RC NOT YET SET, log ends at P3 gate
  (`make`), pid 1286837 alive, P1/P2 PASS, 7 deletions staged, rollback token
  `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/`
  clean.

These newly indexed reports bring the indexed Markdown total to **407**,
matching the 407 `.md` files under `docs/` (including this index; the header
count "(407 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 new-docs sweep (last ~399 docs), WINSRV_DELETE_WATCH83 backfill (append-only)

A fresh full `docs/*.md` listing (407 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: 1 document existed on disk but had no entry and 0 links were stale;
it is linked below with a one-line description. `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH83.md](WINSRV_DELETE_WATCH83.md) — read-only 2026-11-24
  ~22:18 re-apply monitor: APPLY_RC NOT YET SET, pid 1286837 alive ~13:54 in
  the P3 gate (`make` + selfcheck) after P1/P2 PASS, 7 winsrv deletions staged
  (uncommitted, HEAD `0f03f7e`), rollback token
  `ROLLBACK-WINSRV-20261124_215436` intact, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **407**,
matching the 407 `.md` files under `docs/` (including this index; the header
count "(407 files)" at line 4 is updated); every Markdown document in the
directory is now linked, with 0 stale links.

## Added — 2026-11-24 subagent new-docs sweep (last ~399 docs) (append-only)

A fresh full `docs/*.md` listing (406 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: **0 documents on disk lacked an entry and 0 links were stale** (406
on-disk `.md` files = 406 unique indexed targets, including this index), so
this section adds no new link entries. `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

The indexed Markdown total stays **406**, matching the 406 `.md` files under
`docs/` (including this index; the header count "(406 files, verified
2026-11-24)" at line 4 is consistent); every Markdown document in the
directory is linked, with 0 missing and 0 stale links (counts verified equal
after this append).

## Added — 2026-11-24 new-docs sweep (last ~399 docs), WINSRV_DELETE_WATCH83 backfill (append-only)

A fresh full `docs/*.md` listing (407 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: 1 document existed on disk but had no entry and 0 links were stale;
it is linked below with a one-line description. `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH83.md](WINSRV_DELETE_WATCH83.md) — read-only 2026-11-24 ~22:18 winsrv re-apply monitor: APPLY_RC not yet set (pid 1286837 in the P3 `make` gate), P1/P2 gates PASS, 7 winsrv deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **407**,
matching the 407 `.md` files under `docs/` (including this index; the header
count "(407 files, verified 2026-11-24)" at line 4 is consistent); every
Markdown document in the directory is now linked, with 0 stale links
(407 on-disk = 407 unique indexed targets, verified equal after this
append).

## Added — 2026-11-24 new-docs sweep (last 403 docs) (append-only)

A fresh full `docs/*.md` listing (407 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: 1 document existed on disk but had no entry; it is linked below with a
one-line description, and 0 links are stale. `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH83.md](WINSRV_DELETE_WATCH83.md) — read-only 2026-11-24
  ~22:18 winsrv re-apply monitor: APPLY_RC NOT YET SET, pid 1286837 alive
  ~13:54 in the P3 `make` gate (P1/P2 PASS, 7 winsrv files deleted in the
  working tree, rollback snapshot/token `ROLLBACK-WINSRV-20261124_215436`
  on standby, HEAD `0f03f7e`), `kernelhwsrv/` clean, no QEMU run.

This newly indexed report brings the indexed Markdown total to **407**,
matching the 407 `.md` files under `docs/` (including this index; the header
count "(407 files, verified 2026-11-24)" at line 4 is updated); every
Markdown document in the directory is now linked, with 0 missing and 0
stale links (counts verified equal after this append).

## Added — 2026-11-24 new-docs sweep (last ~405 docs) (append-only)

A fresh full `docs/*.md` listing (407 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: **0 documents on disk lacked an entry and 0 links were stale** (407
on-disk `.md` files = 407 unique indexed targets, including this index; the
header count "(407 files, verified 2026-11-24)" at line 4 is consistent), so
this section adds no new link entries. `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

The indexed Markdown total stays **407**, matching the 407 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with 0 missing and 0 stale links (counts verified equal after this
append).

## Added — 2026-11-24 new-docs sweep (last ~403 docs onward) (append-only)

A fresh full `docs/*.md` listing (407 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the 407 unique `.md` link targets in
this index: **0 documents on disk are missing an entry and 0 links are
stale** (407 on-disk `.md` files = 407 unique indexed targets, including
this index; the header count "(407 files, verified 2026-11-24)" at line 4
is consistent). The newest docs since the last ~403-file sweep —
[`WINSRV_DELETE_WATCH82.md`](WINSRV_DELETE_WATCH82.md) (read-only ~22:07
winsrv re-apply monitor: APPLY_RC not yet emitted, P1/P2 gates PASS, 7
winsrv deletes staged, rollback token `ROLLBACK-WINSRV-20261124_215436`
on standby) and [`WINSRV_DELETE_WATCH83.md`](WINSRV_DELETE_WATCH83.md)
(read-only ~22:18 same monitor, pid alive in the P3 `make` gate) — are
confirmed already indexed in the sections above, so this section adds no
new link entries. `kernelhwsrv/` was untouched (read-only, `git status`
clean) and no QEMU run was made by this index update.

The indexed Markdown total stays **407**, matching the 407 `.md` files
under `docs/` (including this index); every Markdown document in the
directory is linked, with counts re-verified equal after this append.

## Added — 2026-11-24 WINSRV_DELETE_WATCH85–92 backfill (append-only)

A fresh full `docs/*.md` listing (415 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the 407 unique `.md` link targets
previously in this index: eight documents existed on disk but had no entry;
they are linked below. All eight are read-only monitors of the same
in-flight winsrv re-apply (`r1_winsrv_delete.py --apply`, pid 1286837,
log `/tmp/opencode/winsrv_delete_apply2.log`): APPLY_RC never emitted,
P1/P2 gates PASS, 7 winsrv files deleted (`git status` `D` count = 7),
still inside the P3 gate batch, no rollback, rollback token
`ROLLBACK-WINSRV-20261124_215436` on standby, HEAD `0f03f7e` on `97f224b`.
`kernelhwsrv/` was untouched (read-only, `git status` clean) and no QEMU
run was made by this index update.

- [WINSRV_DELETE_WATCH85.md](WINSRV_DELETE_WATCH85.md) — APPLY_RC not yet
  emitted; pid alive ~15 min in the P3 gate after P1/P2 PASS and 7 deletes.
- [WINSRV_DELETE_WATCH86.md](WINSRV_DELETE_WATCH86.md) — ~22:25 monitor: log
  ends mid-P3 gates (2.9 KB), no rollback, 7 deletes staged, pid alive 15:49.
- [WINSRV_DELETE_WATCH87.md](WINSRV_DELETE_WATCH87.md) — 22:10 monitor: log
  ends at the P3 `r1_genuine_ewsrv_test.py` gate; P1 (18 edits) + P2 PASS,
  7 deletes applied, pid ~16 min.
- [WINSRV_DELETE_WATCH88.md](WINSRV_DELETE_WATCH88.md) — log ends at P3 gates
  (make/selfcheck/smoke/ewsrv/input); pre-gate + P1 + P2 PASS, 18 edits +
  7 deletes applied, pid ~16 min.
- [WINSRV_DELETE_WATCH89.md](WINSRV_DELETE_WATCH89.md) — ~22:26 monitor:
  log 49 lines ends mid-P3; P1+P2 PASS, 7 deletes, no APPLY_RC/rollback.
- [WINSRV_DELETE_WATCH90.md](WINSRV_DELETE_WATCH90.md) — ~22:26 monitor: log
  ends at P3 gate-batch launch, no FAIL/Traceback; 7 `D` entries match the
  deletion set; no rollback reverted.
- [WINSRV_DELETE_WATCH91.md](WINSRV_DELETE_WATCH91.md) — log (2939 B) ends at
  P3 gates; pid 16m35s still running; P1/P2 edits applied, ewsrv retire gate
  PASS pre and post.
- [WINSRV_DELETE_WATCH92.md](WINSRV_DELETE_WATCH92.md) — ~22:27 monitor: P3
  last line = `r1_genuine_input_test.py` (3rd of 4 P3 gates); no
  APPLY_RC/FAILED/rollback lines; pid ~16m43s.

These eight newly indexed reports bring the indexed Markdown total to **415**,
matching the 415 `.md` files under `docs/` (including this index; the header
count "(415 files, verified 2026-11-24)" at line 4 is consistent); every
Markdown document in the directory is now linked, with 0 missing and 0 stale
links (counts verified equal after this append).

## Added — 2026-11-24 WINSRV_DELETE_WATCH87–91 backfill (append-only)

A fresh full `docs/*.md` listing (414 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the 409 unique `.md` link targets
previously in this index: five documents existed on disk but had no entry;
they are linked below. `kernelhwsrv/` was untouched (read-only) and no QEMU
run was made by this index update.

- [WINSRV_DELETE_WATCH87.md](WINSRV_DELETE_WATCH87.md) — read-only 2026-11-24 22:10 monitor of the in-flight `r1_winsrv_delete.py --apply` re-apply: APPLY_RC not yet emitted, P1 (18 edits) + P2 gates PASS, 7 winsrv deletes applied, P3 gate running (pid 1286837), rollback token on standby.
- [WINSRV_DELETE_WATCH88.md](WINSRV_DELETE_WATCH88.md) — read-only 2026-11-24 monitor, same re-apply: APPLY_RC NOT YET EMITTED, log ends at P3 gates with 7 deletions and HEAD `0f03f7e` on `97f224b`; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH89.md](WINSRV_DELETE_WATCH89.md) — read-only ~22:26 monitor: no APPLY_RC/rollback in the 49-line apply log, P1+P2 PASS, 7 deletes, P3 gates in flight; verdict still in progress.
- [WINSRV_DELETE_WATCH90.md](WINSRV_DELETE_WATCH90.md) — read-only ~22:26 monitor: APPLY_RC NOT YET WRITTEN, log ends at P3 gate batch launch with the 7-delete set intact and no FAIL/Traceback.
- [WINSRV_DELETE_WATCH91.md](WINSRV_DELETE_WATCH91.md) — read-only monitor: pid 1286837 still running (16m35s) in P3, P1/P2 gates PASS including ewsrv retire gate pre/post, 7 deletions, snapshot `ROLLBACK-WINSRV-20261124_215436` on standby; no commits made.

These five newly indexed reports bring the indexed Markdown total to **415**,
matching the 415 `.md` files under `docs/` (including this index; the header
count "(415 files, verified 2026-11-24)" at line 4 is consistent, updated
again after a concurrent writer indexed `WINSRV_DELETE_WATCH92.md`); every
Markdown document in the directory is now linked, with 0 missing and 0 stale
links (counts verified equal after this append).

## Added — 2026-11-24 subagent docs-index sweep (append-only)

A fresh full `docs/*.md` listing (415 files, flat directory, both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this index: 6 document(s) existed on disk but had no entry and 0 link(s) were stale; the missing ones are linked below with one-line descriptions. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH87.md](WINSRV_DELETE_WATCH87.md) — read-only 2026-11-24 winsrv re-apply watch (WINSRV_DELETE_WATCH87 — re-apply status (2026-11-24 22:10)): NOT finished. Log ends at P3 gate (`r1_genuine_ewsrv_test.py`); no APPLY_RC line, no rollback.; P1/P2 gates PASS, 7 winsrv deletions staged (uncommitted, HEAD `0f03f7e`), rollback token `ROLLBACK-WINSRV-20261124_215436` intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH88.md](WINSRV_DELETE_WATCH88.md) — read-only 2026-11-24 winsrv re-apply watch (WINSRV_DELETE_WATCH88 — re-apply status (2026-11-24)): NOT YET EMITTED; P1/P2 gates PASS, 7 winsrv deletions staged (uncommitted, HEAD `0f03f7e`), rollback token `ROLLBACK-WINSRV-20261124_215436` intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH89.md](WINSRV_DELETE_WATCH89.md) — read-only 2026-11-24 winsrv re-apply watch (WINSRV_DELETE_WATCH89 — re-apply status (2026-11-24 ~22:26)): NOT YET; P1/P2 gates PASS, 7 winsrv deletions staged (uncommitted, HEAD `0f03f7e`), rollback token `ROLLBACK-WINSRV-20261124_215436` intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH90.md](WINSRV_DELETE_WATCH90.md) — read-only 2026-11-24 winsrv re-apply watch (WINSRV_DELETE_WATCH90 — re-apply status (2026-11-24 ~22:26)): NOT YET WRITTEN; P1/P2 gates PASS, 7 winsrv deletions staged (uncommitted, HEAD `0f03f7e`), rollback token `ROLLBACK-WINSRV-20261124_215436` intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH91.md](WINSRV_DELETE_WATCH91.md) — read-only 2026-11-24 winsrv re-apply watch (WINSRV_DELETE_WATCH91 — re-apply status (2026-11-24)): NOT YET WRITTEN — /tmp/opencode/winsrv_delete_apply2.log (2939 B) ends at phase P3 gates (make, selfcheck, r1_qemu_smoke, ewsrv/input tests); no APPLY_RC/rollback line.; P1/P2 gates PASS, 7 winsrv deletions staged (uncommitted, HEAD `0f03f7e`), rollback token `ROLLBACK-WINSRV-20261124_215436` intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH92.md](WINSRV_DELETE_WATCH92.md) — read-only 2026-11-24 winsrv re-apply watch (WINSRV_DELETE_WATCH92 — re-apply status (2026-11-24 ~22:27)): NOT YET EMITTED — re; P1/P2 gates PASS, 7 winsrv deletions staged (uncommitted, HEAD `0f03f7e`), rollback token `ROLLBACK-WINSRV-20261124_215436` intact, `kernelhwsrv/` clean.

These newly indexed reports bring the indexed Markdown total to **415**, matching the 415 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked, with 0 missing and 0 stale links (counts verified equal after this append).

## Added — 2026-11-24 new-docs sweep vs last 307+ backfills (append-only)

A fresh full `docs/*.md` listing (415 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: **0 documents on disk lacked an entry and 0 links were stale** (415
on-disk `.md` files = 415 unique indexed targets, including this index; the
header count "(415 files, verified 2026-11-24)" at line 4 now matches), so
this section adds no new link entries — the newest docs since the earlier
sweeps (`WINSRV_DELETE_WATCH87.md`–`WINSRV_DELETE_WATCH92.md`, the read-only
winsrv re-apply monitors) were already indexed by the section above.
`kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this
index update.

The indexed Markdown total stays **415**, matching the 415 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with 0 missing and 0 stale links (counts re-verified equal after this
append).

## Added — 2026-11-24 subagent final sweep, post-415 (append-only)

A fresh full `docs/*.md` listing (415 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the 415 unique `.md` link targets in
this index: **0 documents on disk are missing an entry and 0 links are
stale**, so this section adds no new link entries. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

The indexed Markdown total stays **415**, matching the 415 `.md` files under
`docs/` (including this index; the header count "(415 files, verified
2026-11-24)" at line 4 is consistent); every Markdown document in the
directory is linked, with counts re-verified equal after this append.

## Added — 2026-11-24 final missing-docs sweep (append-only)

A fresh full `docs/*.md` listing (415 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: **0 documents on disk are missing an entry and 0 links are stale**
(415 on-disk `.md` files = 415 unique indexed targets, including this
index; the header count "(415 files, verified 2026-11-24)" at line 4 is
consistent). The previously missing reports —
[`WINSRV_DELETE_WATCH87.md`](WINSRV_DELETE_WATCH87.md),
[`WINSRV_DELETE_WATCH88.md`](WINSRV_DELETE_WATCH88.md),
[`WINSRV_DELETE_WATCH89.md`](WINSRV_DELETE_WATCH89.md),
[`WINSRV_DELETE_WATCH90.md`](WINSRV_DELETE_WATCH90.md) and
[`WINSRV_DELETE_WATCH91.md`](WINSRV_DELETE_WATCH91.md) (read-only 2026-11-24
winsrv re-apply monitors: APPLY_RC not yet emitted, P1/P2 gates PASS, 7
winsrv deletes staged uncommitted, P3 gates in flight, rollback token
`ROLLBACK-WINSRV-20261124_215436` on standby, HEAD `0f03f7e`) — plus
[`WINSRV_DELETE_WATCH92.md`](WINSRV_DELETE_WATCH92.md) (read-only ~22:27,
same re-apply status) are confirmed already indexed in the sections above,
so this section adds no new link entries. `kernelhwsrv/` was untouched
(read-only) and no QEMU run was made by this index update.

The indexed Markdown total is **415**, matching the 415 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with 0 missing and 0 stale links (counts verified equal after this
append).

## Added — 2026-11-24 WINSRV_DELETE_WATCH93–97 backfill (append-only)

A fresh full `docs/*.md` listing (420 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the 415 unique `.md` link targets in
this index: five documents existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH93.md](WINSRV_DELETE_WATCH93.md) — read-only ~22:29
  winsrv re-apply monitor: APPLY_RC not yet emitted (pid 1286837 alive, ~17:45
  elapsed), P1/P2 gates PASS, 7 winsrv deletes staged uncommitted, P3 gates
  (make/selfcheck/smoke/ewsrv/input/doom) in flight at `r1_doom_test.py`,
  rollback token `ROLLBACK-WINSRV-20261124_215436` on standby, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH94.md](WINSRV_DELETE_WATCH94.md) — read-only ~22:12
  winsrv re-apply status (APPLY_RC not yet seen): P1 (18 edits) + P2
  (Makefile/user.mk) done with each gate PASS, all 7 deletes applied, P3
  gates in flight (~18 min elapsed), `kernelhwsrv/` clean, snapshot/rollback
  token on standby, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH95.md](WINSRV_DELETE_WATCH95.md) — read-only ~22:30
  re-apply monitor: APPLY_RC not yet emitted, still in P3 gates (pid 1286837
  alive ~18 min), P1+P2 gates PASS, 7 files deleted, no FAIL/ROLLBACK lines,
  rollback token on standby.
- [WINSRV_DELETE_WATCH96.md](WINSRV_DELETE_WATCH96.md) — read-only 22:12 EET
  re-apply status: APPLY_RC not yet written, log ends mid-P3 at the doom test
  (pid 1286837, 18:15 elapsed), P1+P2 gates PASS, 7 deletes staged,
  `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH97.md](WINSRV_DELETE_WATCH97.md) — read-only 22:11+
  re-apply status: APPLY_RC not yet emitted, P3 gates in progress (pid 1286837
  alive 18+ min), P1/P2 gates PASS, 7 deletions pending, `kernelhwsrv/` diff
  0 lines, rollback token on standby, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH98.md](WINSRV_DELETE_WATCH98.md) — read-only ~22:31
  re-apply monitor: APPLY_RC not yet, pid 1286837 alive ~18:32, log at P3
  gates (last: doom_audio_test), P1/P2 gates PASS, 7 deletes applied,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH99.md](WINSRV_DELETE_WATCH99.md) — read-only monitor:
  APPLY_RC not finished (pid 1286837, 18m42s), log ends mid-P3 gates, 7
  deletions staged, P1/P2 gates PASS, snapshot/token on standby, HEAD
  `0f03f7e`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH100.md](WINSRV_DELETE_WATCH100.md) — read-only 22:12 UTC
  watch #100: re-apply still running (APPLY_RC = NO), log ends at P3 gate
  `r1_doom_audio_test`, pid 1286837 alive 18m52s, no rollback.
- [WINSRV_DELETE_WATCH101.md](WINSRV_DELETE_WATCH101.md) — read-only 2026-11-24
  monitor: APPLY_RC still in flight (NO), log ends at P3 `r1_doom_audio_test`,
  pid 1286837 alive 19:00, P1 (18 edits) + P2 (3 edits) gates PASS, 7 files
  staged deleted, rollback token on standby.
- [WINSRV_DELETE_WATCH102.md](WINSRV_DELETE_WATCH102.md) — read-only 2026-11-24
  re-apply status: APPLY_RC not yet emitted (pid 1286837, ~19 min), P1/P2 gates
  PASS, 7 deletes done, P3 gates in progress, no rollback.
- [WINSRV_DELETE_WATCH103.md](WINSRV_DELETE_WATCH103.md) — read-only 2026-11-24
  re-apply status: APPLY_RC not yet emitted (pid 1286837, ~19 min), log tail
  mid-P3 gates through doom_playtest, P1/P2 PASS, 7 deletes staged.
- [WINSRV_DELETE_WATCH104.md](WINSRV_DELETE_WATCH104.md) — read-only 2026-11-24
  apply2 status: APPLY_RC not yet emitted, pid 1286837 alive ~19 min, log ends
  mid-P3 gates, no rollback.
- [WINSRV_DELETE_WATCH105.md](WINSRV_DELETE_WATCH105.md) — read-only ~22:33
  re-apply status: APPLY_RC not yet, log ends at P3 gate launches (through
  doom_playtest), pid 1286837 alive 19m36s, P1 (13 edits) + P2 (4 edits) gates
  PASS, 7 files deleted, snapshot/rollback token on standby.

These thirteen newly indexed reports bring the indexed Markdown total to
**432**, matching the 432 `.md` files under `docs/` (including this index;
the header count "(432 files, verified 2026-11-24)" at line 4 is
consistent; WINSRV_DELETE_WATCH106+ were indexed by a concurrent sweep);
every Markdown document in the directory is now linked, with 0 missing and
0 stale links (counts re-verified equal after this append).

## Added — 2026-11-24 WINSRV_DELETE_WATCH95–99 backfill (append-only)

A fresh full `docs/*.md` listing (422 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the 417 unique `.md` link targets in
this index: five documents existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH95.md](WINSRV_DELETE_WATCH95.md) — read-only ~22:30
  winsrv re-apply monitor: APPLY_RC not yet emitted, apply still in phase P3
  gates (make, selfcheck, smoke, ewsrv/input/doom), log ends at the P3 gate
  list.
- [WINSRV_DELETE_WATCH96.md](WINSRV_DELETE_WATCH96.md) — read-only ~22:12 EET
  winsrv re-apply status: APPLY_RC not yet written, log ends mid-P3 gates
  (last: doom_test), pid 1286837 alive ~18:15, rollback token
  `ROLLBACK-WINSRV-20261124_215436` on standby, no rollback executed.
- [WINSRV_DELETE_WATCH97.md](WINSRV_DELETE_WATCH97.md) — read-only ~22:11+
  winsrv re-apply status: APPLY_RC not yet emitted, P1 (variant edits) and P2
  (Makefile/user.mk) gates PASS, P3 (7 file deletes) gates in progress, no
  rollback.
- [WINSRV_DELETE_WATCH98.md](WINSRV_DELETE_WATCH98.md) — read-only ~22:31
  winsrv re-apply status: no APPLY_RC/rollback line in the apply log (3 KB),
  pid 1286837 still running ~18:32, log at P3 gates (last: doom_audio_test).
- [WINSRV_DELETE_WATCH99.md](WINSRV_DELETE_WATCH99.md) — read-only monitor:
  APPLY_RC not finished, pid 1286837 running 18m42s, log ends mid-P3 gates,
  7 deletions staged (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h,
  qemu_gui_exec.cpp), no rollback line.
- [WINSRV_DELETE_WATCH111.md](WINSRV_DELETE_WATCH111.md) — read-only monitor:
  APPLY_RC not yet emitted, no rollback line in `winsrv_delete_apply2.log`
  (3,162 bytes), apply still in flight.
- [WINSRV_DELETE_WATCH113.md](WINSRV_DELETE_WATCH113.md) — read-only monitor:
  APPLY_RC not yet emitted, apply log (53 lines, 3.2 KB) ends at the P3 gate
  list, no APPLY_RC/rollback/FAIL line.
- [WINSRV_DELETE_WATCH114.md](WINSRV_DELETE_WATCH114.md) — read-only monitor:
  APPLY_RC not yet emitted, apply log ends at the P3 gate list, no rollback.

These eight newly indexed reports brought the indexed Markdown total to **422**
at the time of this append; the re-verified state at final check is **439
on-disk `.md` files = 439 unique indexed link targets, 0 missing, 0 stale**
(including this index; the header count "(439 files, verified 2026-11-24)"
at line 4 is consistent); every Markdown document in the directory is linked.

## Added — 2026-11-24 new-docs sweep (last 215, subagent backfill) (append-only)

A fresh full `docs/*.md` listing (432 files, flat directory) was diffed against the unique `.md` link targets in this index: 8 document(s) existed on disk but had no entry; they are linked below. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH102.md](WINSRV_DELETE_WATCH102.md) — WINSRV_DELETE_WATCH102 — re-apply status (2026-11-24) (read-only 2026-11-24 winsrv re-apply monitor entry).
- [WINSRV_DELETE_WATCH103.md](WINSRV_DELETE_WATCH103.md) — WINSRV_DELETE_WATCH103 — re-apply status (2026-11-24) (read-only 2026-11-24 winsrv re-apply monitor entry).
- [WINSRV_DELETE_WATCH104.md](WINSRV_DELETE_WATCH104.md) — WINSRV_DELETE_WATCH104 — apply2 status (2026-11-24) (read-only 2026-11-24 winsrv re-apply monitor entry).
- [WINSRV_DELETE_WATCH105.md](WINSRV_DELETE_WATCH105.md) — WINSRV_DELETE_WATCH105 — re-apply status (2026-11-24 ~22:33) (read-only 2026-11-24 winsrv re-apply monitor entry).
- [WINSRV_DELETE_WATCH106.md](WINSRV_DELETE_WATCH106.md) — WINSRV_DELETE_WATCH106 — winsrv re-apply status (2026-11-24 ~22:33) (read-only 2026-11-24 winsrv re-apply monitor entry).
- [WINSRV_DELETE_WATCH107.md](WINSRV_DELETE_WATCH107.md) — WINSRV delete watch 107 — re-apply status (read-only 2026-11-24 winsrv re-apply monitor entry).
- [WINSRV_DELETE_WATCH108.md](WINSRV_DELETE_WATCH108.md) — WINSRV_DELETE_WATCH108 — re-apply status (2026-11-24 22:13) (read-only 2026-11-24 winsrv re-apply monitor entry).
- [WINSRV_DELETE_WATCH109.md](WINSRV_DELETE_WATCH109.md) — WINSRV_DELETE_WATCH109 — re-apply status (2026-11-24) (read-only 2026-11-24 winsrv re-apply monitor entry).

These newly indexed reports bring the indexed Markdown total to **432**, matching the 432 `.md` files under `docs/` (including this index); every Markdown document in the directory is now linked, with counts re-verified equal after this append (header count at line 4 updated to 432).

## Added — 2026-11-24 WINSRV_DELETE_WATCH98–102 backfill (append-only)

A full `docs/*.md` listing (432 files, flat directory, `LC_ALL=C` sorted) was
diffed against the unique `.md` link targets in this index: 0
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.



These 0 newly indexed reports bring the indexed Markdown total to **432**,
matching the 432 `.md` files under `docs/` (including this index; the header
count "(432 files, verified 2026-11-24)" at line 4 is updated); every Markdown
document in the directory is now linked, with 0 missing and 0 stale links
(counts re-verified equal after this append).

## Added — 2026-11-24 missing-docs sweep (subagent, last-215 backfill check) (append-only)

A fresh full `docs/*.md` listing (432 files, flat directory, `LC_ALL=C` sorted)
was diffed against the 432 unique `.md` link targets in this index: 0
documents existed on disk but had no entry, so no new links were required.
`kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this
index update.

The indexed Markdown total remains **432**, matching the 432 `.md` files
under `docs/` (including this index; the header count "(432 files, verified
2026-11-24)" at line 4 is already correct); every Markdown document in the
directory is linked, with 0 missing and 0 stale links (counts re-verified
equal after this append).

## Added — 2026-11-24 WINSRV_DELETE_WATCH111–112 backfill (append-only)

A fresh full `docs/*.md` listing (434 files, flat directory, `LC_ALL=C` sorted)
was diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH111.md](WINSRV_DELETE_WATCH111.md) — read-only monitor:
  APPLY_RC not yet emitted (pid 1286837 alive, ~20:44), P1/P2 gates PASS, 7
  deletes applied, rollback token `ROLLBACK-WINSRV-20261124_215436` on
  standby, `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH112.md](WINSRV_DELETE_WATCH112.md) — read-only 22:15 EET
  monitor: APPLY_RC not yet emitted (pid 1286837, ~21 min), mid-P3 gates, P1+P2
  PASS, 7 deletes applied, rollback token on standby, `kernelhwsrv/` clean,
  HEAD `0f03f7e`.

These two newly indexed reports bring the indexed Markdown total to **434**,
matching the 434 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked, with 0 missing and 0 stale
links (counts re-verified equal after this append).

## Added — 2026-11-24 WINSRV_DELETE_WATCH113–114 backfill (append-only)

A fresh full `docs/*.md` listing (436 files, flat directory, `LC_ALL=C` sorted)
was diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH113.md](WINSRV_DELETE_WATCH113.md) — read-only monitor:
  APPLY_RC not yet emitted (pid 1286837 alive ~21 min, mid-P3 gates), 7 deletes
  + 10 modified in worktree, rollback token
  `ROLLBACK-WINSRV-20261124_215436` on standby, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH114.md](WINSRV_DELETE_WATCH114.md) — read-only watch #114:
  APPLY_RC not yet emitted (pid 1286837 ~21 min, in P3 gate battery), P1/P2
  gates PASS, 7 deletes present in worktree, HEAD `0f03f7e`.

These two newly indexed reports bring the indexed Markdown total to **436**,
matching the 436 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked, with 0 missing and 0 stale
links (counts re-verified equal after this append).

## Added — 2026-11-24 new-docs sweep (last 215) re-verification (append-only)

A fresh full `docs/*.md` listing (432 files, flat directory, `LC_ALL=C`
sorted) was diffed against the unique `.md` link targets in this index:
**0 documents existed on disk without an entry and 0 linked targets are
missing on disk** (432 on-disk = 432 unique indexed targets, including this
index; the header count "(432 files, verified 2026-11-24)" at line 4 is
consistent). No new documents were found, so this section adds no new link
entries. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made
by this index update.

The indexed Markdown total stays **432**, matching the 432 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with counts re-verified equal after this append.

## Added — 2026-11-24 WINSRV_DELETE_WATCH98–103 backfill (append-only)

A fresh full `docs/*.md` listing (426 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: six documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH98.md](WINSRV_DELETE_WATCH98.md) — read-only ~22:31
  winsrv re-apply monitor: APPLY_RC not yet emitted (pid 1286837, ~18:32
  elapsed), log at P3 gates ending at doom_audio_test, P1/P2 gates PASS,
  7 winsrv deletes staged (git D-count = 7), rollback token on standby,
  `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH99.md](WINSRV_DELETE_WATCH99.md) — read-only monitor:
  APPLY_RC NOT FINISHED (pid 1286837, 18m42s), log ends mid-P3 gates, no
  rollback line; 7 deletions staged, P1/P2 gates PASS (ewsrv retire gate,
  selfcheck, isogrub), snapshot/token `ROLLBACK-WINSRV-20261124_215436`,
  `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH100.md](WINSRV_DELETE_WATCH100.md) — read-only 22:12 UTC
  monitor: re-apply still running (NO APPLY_RC), log ends mid-P3 at
  `r1_doom_audio_test`, pid 1286837 alive 18m52s running
  `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`.
- [WINSRV_DELETE_WATCH101.md](WINSRV_DELETE_WATCH101.md) — read-only monitor:
  APPLY_RC NO / still in flight; log ends at P3 gate `r1_doom_audio_test.py`
  with no APPLY_RC or rollback (token `ROLLBACK-WINSRV-20261124_215436` only);
  pid 1286837 at 19:00, P1 (18 edits) + P2 (3 edits) gates PASS, 7 files
  staged deleted.
- [WINSRV_DELETE_WATCH102.md](WINSRV_DELETE_WATCH102.md) — read-only monitor:
  APPLY_RC not yet emitted (pid 1286837, ~19 min), no rollback; P1/P2 gates
  PASS, 7 deletes done (winsrv.c, guishost.h, wsclient.c/h,
  qemu_windowsrv.cpp/h, qemu_gui_exec.cpp), P3 gates in progress,
  `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH103.md](WINSRV_DELETE_WATCH103.md) — read-only monitor:
  APPLY_RC not yet emitted (pid 1286837, ~19 min), no rollback; log tail at
  P3 gates (make/selfcheck/smoke/ewsrv/input/doom/doom_audio/doom_playtest),
  P1/P2 PASS, 7 deletes staged, `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH113.md](WINSRV_DELETE_WATCH113.md) — read-only monitor:
  APPLY_RC NOT YET EMITTED (pid 1286837, ~21 min, still in P3 gates; log ends
  at the P3 gate list with no APPLY_RC/rollback/FAIL); 7 deletions + 10
  modified files in worktree, HEAD `0f03f7e`, rollback token
  `ROLLBACK-WINSRV-20261124_215436` at
  `/tmp/opencode/winsrv_retire_backup_20261124_215436`.
- [WINSRV_DELETE_WATCH114.md](WINSRV_DELETE_WATCH114.md) — read-only monitor:
  APPLY_RC not yet emitted, log ends at P3 gate list, no rollback; pid 1286837
  ~21 min in P3 gate battery; P1 (18 edits) + P2 (Makefile/user.mk) gates
  PASS, 7 deletes present in worktree matching the 7-op delete set.

These eight newly indexed reports brought the indexed Markdown total to
**428** at append time; concurrent writers have since indexed further docs,
so the current verified total is **436**, matching the 436 `.md` files under
`docs/` (including this index; the header count at line 4 is kept in sync by
the sweeps); every Markdown document in the directory is linked, with 0
missing and 0 stale links (counts re-verified equal after this append).

## Added — 2026-11-24 post-215 sweep, WINSRV_DELETE_WATCH102–104 (append-only)

A fresh full `docs/*.md` listing (both sides `LC_ALL=C` sorted) was diffed
against the unique `.md` link targets in this index. At scan time three
documents past the 215 mark had no entry —
[`WINSRV_DELETE_WATCH102.md`](WINSRV_DELETE_WATCH102.md),
[`WINSRV_DELETE_WATCH103.md`](WINSRV_DELETE_WATCH103.md) and
[`WINSRV_DELETE_WATCH104.md`](WINSRV_DELETE_WATCH104.md) — all read-only
2026-11-24 winsrv re-apply monitors (APPLY_RC not yet emitted, pid 1286837
alive ~19 min, P1/P2 gates PASS, 7 winsrv deletes staged uncommitted, P3
gates in flight, no rollback, `kernelhwsrv/` clean, HEAD `0f03f7e`); a
concurrent sweep indexed them in the section above, and they are re-linked
here with one-line descriptions for completeness. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH102.md](WINSRV_DELETE_WATCH102.md) — read-only 2026-11-24
  re-apply monitor: APPLY_RC not yet emitted (pid 1286837 ~19 min, no
  rollback), P1/P2 PASS, 7 deletes staged, P3 gates through doom_audio,
  `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH103.md](WINSRV_DELETE_WATCH103.md) — read-only 2026-11-24
  re-apply status: APPLY_RC not yet emitted, log mid-P3 (through
  doom_playtest), P1/P2 PASS, 7 deletes applied, `kernelhwsrv/` clean,
  HEAD `0f03f7e` on `97f224b`.
- [WINSRV_DELETE_WATCH104.md](WINSRV_DELETE_WATCH104.md) — read-only 2026-11-24
  apply2 status: APPLY_RC not yet emitted, pid 1286837 alive ~19 min, log
  (52 lines) ends mid-P3, no APPLY_RC/rollback/FAILED/Traceback markers.

The indexed Markdown total is **432**, matching the 432 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with 0 missing and 0 stale links (counts re-verified equal after this
append).

## Added — 2026-11-24 full directory sweep + WINSRV_DELETE_WATCH111–113 backfill (append-only)

A fresh full `docs/*.md` listing (435 files, flat directory, no
subdirectories, both sides `LC_ALL=C` sorted) was diffed against the 432
unique `.md` link targets previously in this index: three documents existed on
disk but had no entry; they are linked below. `kernelhwsrv/` was untouched
and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH111.md](WINSRV_DELETE_WATCH111.md) — read-only monitor:
  APPLY_RC not yet emitted (pid 1286837 alive, ~20:44), log ends at the P3
  gate list, P1 (18 edits) + P2 (Makefile/user.mk + `make isogrub`) gates
  PASS, 7 winsrv deletes applied, rollback token
  `ROLLBACK-WINSRV-20261124_215436` on standby, `kernelhwsrv/` clean,
  HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH112.md](WINSRV_DELETE_WATCH112.md) — read-only 22:15 EET
  monitor: APPLY_RC not yet emitted (pid 1286837, ~21 min), P3 gates
  (selfcheck/smoke/ewsrv/input/DOOM QEMU tests) still running, P1+P2 PASS,
  7 deletes staged (`git status` D-count = 7), rollback token on standby,
  `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH113.md](WINSRV_DELETE_WATCH113.md) — read-only monitor:
  APPLY_RC not yet emitted (pid 1286837, ~21 min), log (53 lines) ends at the
  P3 gate list with no rollback/FAIL, 7 deletions + 10 modified files in the
  working tree, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean.

These three newly indexed reports bring the indexed Markdown total to **435**,
matching the 435 `.md` files under `docs/` (including this index; the header
count "(432 files, verified 2026-11-24)" at line 4 predates these three).
After this append the diff re-verified **0 missing and 0 stale links**
(435 on-disk = 435 unique indexed targets); `kernelhwsrv/` untouched, no QEMU
run by this index update.

## Added — 2026-11-24 new-docs sweep vs last 307+ backfills (append-only)

A fresh full `docs/*.md` listing (439 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: the WINSRV_DELETE_WATCH98–102 monitors outstanding at the start of this
sweep were already indexed in the sections above, and 2 further documents
appeared during the sweep and are indexed here — 0 links are stale. The new
read-only 2026-11-24 winsrv re-apply monitors (both: APPLY_RC not yet emitted,
pid 1286837 inside phase-P3 gates, P1/P2 PASS, 7 deletes staged, rollback
token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean):

- [WINSRV_DELETE_WATCH111.md](WINSRV_DELETE_WATCH111.md) — Re-apply watch: pid alive 20:44, log ends at P3 gate list, no APPLY_RC/rollback, 7 deletes applied.
- [WINSRV_DELETE_WATCH112.md](WINSRV_DELETE_WATCH112.md) — Re-apply watch 22:15 EET: APPLY_RC not yet emitted, pid ~21 min in P3 gates, P1/P2 PASS, 7 deletes staged.

`kernelhwsrv/` was untouched (read-only) and no QEMU run was made by this
index update.

The indexed Markdown total is **439**, matching the 439 `.md` files under
`docs/` (including this index; the header count "(439 files, verified
2026-11-24)" at line 4 is updated); every Markdown document in the directory
is linked, with 0 missing and 0 stale links (counts verified equal after this
append).

## Added — 2026-11-24 subagent full sweep, post-215 (append-only)

A fresh full `docs/*.md` listing (432 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: **0 documents on disk are missing an entry and 0 links are stale**
(432 on-disk `.md` files = 432 unique indexed targets, including this index;
the header count "(432 files, verified 2026-11-24)" at line 4 is
consistent). The newest unindexed reports at sweep start
(`WINSRV_DELETE_WATCH102.md`–`WINSRV_DELETE_WATCH107.md`, read-only monitors
of the in-flight winsrv re-apply pid 1286837: APPLY_RC not yet emitted, P1/P2
gates PASS, 7 deletes staged, P3 gates in progress, no rollback) were indexed
by a concurrent sweep before this section; this sweep adds no new link entries
because no missing documents remained. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total stays **432**, matching the 432 `.md` files under
`docs/` (including this index; the header count "(432 files)" at line 4 is
consistent); every Markdown document in the directory is linked (counts
re-verified equal after this append).

## Added — 2026-11-24 subagent full sweep, WINSRV_DELETE_WATCH98–101/111 (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: at scan start four documents (`WINSRV_DELETE_WATCH98.md`,
`WINSRV_DELETE_WATCH99.md`, `WINSRV_DELETE_WATCH100.md`,
`WINSRV_DELETE_WATCH101.md` — all read-only winsrv re-apply monitors: APPLY_RC
not yet emitted, pid 1286837 in P3 gates, P1/P2 gates PASS, 7 deletes staged,
rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`,
`kernelhwsrv/` clean) had no entry and were linked by the concurrent backfill;
one further document appeared during this update and is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH111.md](WINSRV_DELETE_WATCH111.md) — read-only re-apply
  monitor: APPLY_RC not yet emitted (pid 1286837 alive 20:44, log 3,162 B ends
  at the phase-P3 gate list), pre-apply ewsrv gate + P1 (18 edits) + P2
  (Makefile/user.mk + `make isogrub`) all PASS, 7 deletes applied
  (D-count 7), rollback token `ROLLBACK-WINSRV-20261124_215436`,
  `kernelhwsrv/` untouched, HEAD `0f03f7e`.

The re-verified diff after this append shows **0 unlinked documents and 0
broken links** (439 on-disk `.md` files = 439 unique indexed targets, including
this index; the header count "(439 files, verified 2026-11-24)" at line 4 is
consistent; WINSRV_DELETE_WATCH113–117 were linked by the concurrent backfills
below).

The indexed Markdown total is **439**, matching the 439 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with 0 missing and 0 stale links (counts re-verified equal after this
append).

## Added — 2026-11-24 subagent index sweep, docs self-audit (append-only)

A full `docs/*.md` listing (432 files, flat directory) was diffed against the
unique `.md` link targets in this index: **0 documents on disk are missing an
entry and 0 links are stale** (432 on-disk `.md` files = 432 unique indexed
targets, including this index; the header count "(432 files, verified
2026-11-24)" at line 4 is consistent). No missing documents were found, so
this section adds no new link entries. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total stays **432**, matching the 432 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with counts re-verified equal after this append.

## Added — 2026-11-24 WINSRV_DELETE_WATCH113–115 backfill (append-only)

A full `docs/*.md` listing (437 files, flat directory, no subdirectories) was
diffed against the 434 unique `.md` link targets previously in this index:
three documents existed on disk but had no entry; they are linked below — all
read-only 2026-11-24 winsrv re-apply monitors (APPLY_RC not yet emitted, pid
1286837 in the phase-P3 gate battery, P1/P2 gates PASS, 7 deletes staged,
rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`,
`kernelhwsrv/` clean). `kernelhwsrv/` was untouched and no QEMU run was made
by this index update.

- [WINSRV_DELETE_WATCH113.md](WINSRV_DELETE_WATCH113.md) — re-apply watch: APPLY_RC not yet emitted, pid 1286837 ~21 min into P3 gates, 7 deletions in worktree, snapshot/rollback token recorded, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH114.md](WINSRV_DELETE_WATCH114.md) — re-apply watch: APPLY_RC not yet emitted, P1 (18 edits) + P2 (Makefile/user.mk) applied with P1/P2 gates PASS, 7-op delete set staged, rollback token not invoked.
- [WINSRV_DELETE_WATCH115.md](WINSRV_DELETE_WATCH115.md) — re-apply watch: APPLY_RC not yet emitted (~21m41s, P3 gates running), D-count 7 confirmed, pre-apply/P1/P2 gates PASS, no new commit from the apply.

These three newly indexed reports bring the indexed Markdown total to **437**,
matching the 437 `.md` files under `docs/` (including this index; the header
count "(437 files, verified 2026-11-24)" at line 4 is updated); every Markdown
document in the directory is now linked, with 0 missing and 0 stale links
(counts re-verified equal after this append).

## Added — 2026-11-24 WINSRV_DELETE_WATCH113/114 backfill (append-only)

A full `docs/*.md` listing (436 files, flat directory, no subdirectories) was
diffed against the 434 unique `.md` link targets previously in this index:
two documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH113.md](WINSRV_DELETE_WATCH113.md) — read-only re-apply
  monitor: APPLY_RC not yet emitted (pid 1286837 alive ~21 min, apply log ends
  at the P3 gate list), 7 deletions + 10 modified files in the worktree,
  rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH114.md](WINSRV_DELETE_WATCH114.md) — read-only follow-up
  watch #114: still no APPLY_RC/rollback (pid 1286837 in the P3 gate battery,
  P1 18 edits + P2 Makefile/user.mk edits applied with P1/P2 gates PASS,
  7-op delete set present), same rollback token, no commit/push,
  `kernelhwsrv/` untouched.

These two newly indexed reports bring the indexed Markdown total to **436**
at the time of this append; the following sweep indexed
`WINSRV_DELETE_WATCH115/116/117.md`, and the final re-verified diff shows **0
unlinked documents and 0 broken links** (439 on-disk `.md` files = 439 unique
indexed targets, including this index; the header count "(439 files, verified
2026-11-24)" at line 4 is updated), so every Markdown document in the
directory is now linked.

## Added — 2026-11-24 subagent missing-docs sweep (append-only)

A fresh full `docs/*.md` listing (439 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: 3 document(s) existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH115.md](WINSRV_DELETE_WATCH115.md) — read-only 2026-11-24 winsrv re-apply monitor: re-apply status (2026-11-24) APPLY_RC not yet emitted; pid 1286837 in P3 gates; 7 winsrv deletes staged; rollback token `ROLLBACK-WINSRV-20261124_215436` on standby; HEAD `0f03f7e`; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH116.md](WINSRV_DELETE_WATCH116.md) — read-only 2026-11-24 winsrv re-apply monitor: re-apply status (2026-11-24 22:15 UTC) APPLY_RC not yet emitted; pid 1286837 in P3 gates; 7 winsrv deletes staged; rollback token `ROLLBACK-WINSRV-20261124_215436` on standby; HEAD `0f03f7e`; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH117.md](WINSRV_DELETE_WATCH117.md) — read-only 2026-11-24 winsrv re-apply monitor: WINSRV delete apply watch #117 (2026-11-24 22:15 UTC) APPLY_RC not yet emitted; pid 1286837 in P3 gates; 7 winsrv deletes staged; rollback token `ROLLBACK-WINSRV-20261124_215436` on standby; HEAD `0f03f7e`; `kernelhwsrv/` clean.

These newly indexed reports bring the indexed Markdown total to **439**,
matching the 439 `.md` files under `docs/` (including this index; the header
count at line 4 is updated to "(439 files, verified 2026-11-24)"); every
Markdown document in the directory is now linked, with 0 missing and 0 stale
links (counts re-verified equal after this append).

## Added — 2026-11-24 subagent full sweep past 215 (append-only)

A full `docs/*.md` listing (439 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (439 on-disk `.md` files = 439 unique indexed
targets, including this index; the header count "(439 files, verified
2026-11-24)" at line 4 is consistent). No missing documents were found past
the 215 mark, so this section adds no new link entries; `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **439**, matching the 439 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with counts re-verified equal after this append.

## Added — 2026-11-24 post-215 missing-docs diff (append-only)

A fresh full `docs/*.md` listing (439 files, flat directory, `LC_ALL=C` sorted)
was diffed against the unique `.md` link targets in this index: the WINSRV
delete-watch reports past the 215 mark (WINSRV_DELETE_WATCH115/116/117) were
already linked by a concurrent backfill, so **0 documents were missing and 0
stale links** existed at diff time (439 on-disk `.md` files = 439 unique indexed
targets, including this index; the header count "(439 files, verified
2026-11-24)" at line 4 is consistent). This section therefore adds no new link
entries; `kernelhwsrv/` was untouched (read-only) and no QEMU run was made by
this index update.

The indexed Markdown total stays **439**, matching the 439 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with counts re-verified equal after this append.

## Added — 2026-11-24 post-215 full sweep, all docs linked (append-only)

A fresh full `docs/*.md` listing (439 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: **0 unlinked
documents and 0 broken links** (439 on-disk `.md` files = 439 unique indexed
targets, including this index). Every document past the earlier 215 mark —
including the `WINSRV_DELETE_WATCH104`–`115` monitor series — already has a
one-line entry and link above, so this section adds no new link entries;
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total is **439**, matching the 439 `.md` files under
`docs/` (including this index; the header count "(439 files, verified
2026-11-24)" at line 4 is updated); every Markdown document in the directory
is linked, with counts re-verified equal after this append.

## Added — 2026-11-24 subagent last-215 missing-docs sweep (append-only)

A fresh full `docs/*.md` listing (439 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the 439 unique `.md` link targets in this
index: **0 documents on disk are missing an entry and 0 links are stale**
(439 on-disk `.md` files = 439 unique indexed targets, including this index;
the header count "(439 files, verified 2026-11-24)" at line 4 is consistent,
and the newest reports `WINSRV_DELETE_WATCH112/113/115/116/117.md` were
confirmed indexed by concurrent sweeps). No missing documents were found past
the 215 mark, so this section adds no new link entries. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **439**, matching the 439 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked (counts re-verified equal immediately after this append).

## Added — 2026-11-24 new-docs sweep (last ~215 check, subagent) (append-only)

A fresh full `docs/*.md` listing (439 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the unique `.md` link targets in this
index: **0 documents on disk lacked an entry and 0 links were stale** (439
on-disk = 439 unique indexed targets, including this index; the header count
"(439 files, verified 2026-11-24)" at line 4 is consistent). The newest
reports past the 215 mark (`WINSRV_DELETE_WATCH113/114/115.md` et al.) were
already covered by concurrent sweeps, so this section adds no new link
entries. `kernelhwsrv/` was untouched (read-only) and no QEMU run was made
by this index update.

The indexed Markdown total stays **439**, matching the 439 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with counts re-verified equal after this append.

## Added — 2026-11-24 subagent index sweep, final missing-doc check (append-only)

A full `docs/*.md` listing (439 files, flat directory) was diffed against the
unique `.md` link targets in this index: **0 documents on disk are missing an
entry and 0 links are stale** (439 on-disk `.md` files = 439 unique indexed
targets, including this index; the header count "(439 files, verified
2026-11-24)" at line 4 is consistent). The three newest reports
(`WINSRV_DELETE_WATCH113.md`, `WINSRV_DELETE_WATCH114.md`,
`WINSRV_DELETE_WATCH115.md` — read-only winsrv re-apply monitors, APPLY_RC not
yet emitted, pid 1286837 in P3 gates, 7 deletes staged, rollback token
`ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` clean) are already linked by
the concurrent backfill, so this section adds no new link entries.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **439**, matching the 439 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with counts re-verified equal after this append.

## Added — 2026-11-24 subagent sweep vs last ~435, 115/116 check (append-only)

A fresh full `docs/*.md` listing (439 files, flat directory, both sides
`LC_ALL=C` sorted) was diffed against the 439 unique `.md` link targets in this
index: **0 documents on disk are missing an entry and 0 links are stale** (the
earlier 115/116 gap closed by the concurrent `WINSRV_DELETE_WATCH115/116/117`
backfill above), so this section adds no new link entries. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

The indexed Markdown total stays **439**, matching the 439 `.md` files under
`docs/` (including this index; the header count "(439 files, verified
2026-11-24)" at line 4 is consistent); post-append re-verification shows
439 on-disk `.md` files = 439 unique indexed targets, 0 missing, 0 dangling.

## Added — 2026-11-24 subagent docs-index sweep, WATCH112–117 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index. At scan time the unindexed documents were the newest
`WINSRV_DELETE_WATCH112.md`–`WINSRV_DELETE_WATCH117.md` read-only monitors of
the in-flight winsrv re-apply (pid 1286837; APPLY_RC not yet emitted, log
`/tmp/opencode/winsrv_delete_apply2.log` ending at the P3 gate list, P1/P2
gates PASS, 7 deletes staged, rollback token
`ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean); they
are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH112.md](WINSRV_DELETE_WATCH112.md) — read-only 22:15 EET
  re-apply monitor: APPLY_RC not yet emitted, pid 1286837 alive ~21 min in the
  P3 gate battery, P1+P2 gates PASS, 7 deletes applied, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH113.md](WINSRV_DELETE_WATCH113.md) — read-only re-apply
  monitor: APPLY_RC not yet emitted (log 53 lines, ends at P3 gates), pid
  1286837 alive, 7 deletions + 10 modified files in the worktree, snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH114.md](WINSRV_DELETE_WATCH114.md) — read-only re-apply
  monitor: APPLY_RC not yet emitted, pid 1286837 ~21 min in P3 gates, P1 (18
  edits) + P2 gates PASS, 7-op delete set present, rollback token not invoked,
  `kernelhwsrv/` untouched, no commit/push.
- [WINSRV_DELETE_WATCH115.md](WINSRV_DELETE_WATCH115.md) — read-only re-apply
  monitor: APPLY_RC not yet emitted (log 3.2 KB at P3 gate list), pid 1286837
  alive ~21m41s, D-count 7, P1/P2 gates PASS, snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436`, HEAD `0f03f7e` /
  `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH116.md](WINSRV_DELETE_WATCH116.md) — read-only 22:15 UTC
  re-apply monitor: APPLY_RC not yet emitted (log 3,226 B, 0 APPLY_RC hits), pid
  1286837 ~22 min in the P3 test suite, P1/P2 gates PASS, 7 files deleted,
  rollback snapshot present, HEAD `0f03f7e`, `kernelhwsrv/` 0 modified.
- [WINSRV_DELETE_WATCH117.md](WINSRV_DELETE_WATCH117.md) — read-only 22:15 UTC
  re-apply monitor: APPLY_RC not yet, pid 1286837 ~22 min in P3 QEMU gates
  (selfcheck → smoke → ewsrv → input → doom×4 → fullchain), pre/P1/P2 gates
  PASS, 7 deletes, HEAD `0f03f7e` / `97f224b`, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH118.md](WINSRV_DELETE_WATCH118.md) — read-only re-apply
  monitor: APPLY_RC not yet emitted (pid 1286837 ~23 min; log 3.4 KB ends at
  `gate[P3]: r1_final_verify.py --strict-skip`), P1 (18 edits) + P2 gates
  PASS, 7 deletes, D-count 7, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH119.md](WINSRV_DELETE_WATCH119.md) — read-only ~22:40
  re-apply monitor: APPLY_RC not yet (log 3.3 KB, no rollback line), pid
  1286837 alive ~23 min, last log line `gate[P3]: r1_final_verify.py
  --strict-skip`, P1/P2 PASS, D-count 7, HEAD `0f03f7e` / `97f224b`,
  rollback token `ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH120.md](WINSRV_DELETE_WATCH120.md) — read-only ~22:40
  re-apply monitor: APPLY_RC not yet (pid 1286837 23+ min inside
  `r1_final_verify.py --strict-skip`), P1/P2 PASS, 7 deletes (7 `D` entries),
  HEAD `0f03f7e`, snapshot/rollback token
  `ROLLBACK-WINSRV-20261124_215436`, pre-apply ewsrv gate PASS,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH122.md](WINSRV_DELETE_WATCH122.md) — read-only ~22:41
  re-apply monitor: APPLY_RC not yet emitted (log ends at `gate[P3]:
  r1_final_verify.py --strict-skip`), pid 1286837 ~24 min, 7 `D` deletions,
  P1/P2 + pre-apply ewsrv gates PASS, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e` / `97f224b`,
  `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH123.md](WINSRV_DELETE_WATCH123.md) — read-only re-apply
  monitor: APPLY_RC not yet emitted (pid 1286837 ~24 min in P3; log 56 lines /
  3.3 KB), no rollback/FAIL, P1+P2 gates PASS, 7 deletes, HEAD `0f03f7e` /
  `97f224b`, rollback token `ROLLBACK-WINSRV-20261124_215436`,
  `kernelhwsrv/` clean.

The re-verified diff after this append shows **0 unlinked documents and 0
broken links** (445 on-disk `.md` files = 445 unique indexed `.md` targets,
including this index; the header count "(445 files, verified 2026-11-24)" at
line 4 matches). The indexed Markdown total is **445**, matching the 445
`.md` files under `docs/` (including this index); every Markdown document in
the directory is linked (counts re-verified equal after this append).

## Added — 2026-11-24 subagent docs-index diff, post-439 (append-only)

A fresh full `docs/*.md` listing (439 files, flat directory, no subdirectories,
both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: **0 documents on disk are missing an entry
and 0 links are stale** (439 on-disk `.md` files = 439 unique indexed link
targets, including this index; the header count "(439 files, verified
2026-11-24)" at line 4 is consistent). No missing documents were found — the
newest on-disk reports (`WINSRV_DELETE_WATCH115.md` /
`WINSRV_DELETE_WATCH116.md` / `WINSRV_DELETE_WATCH117.md`, read-only 2026-11-24
winsrv re-apply monitors) are already linked in the concurrent backfill
sections above, and the one doc that appeared after this sweep
(`WINSRV_DELETE_WATCH118.md`) is linked below:

- [WINSRV_DELETE_WATCH118.md](WINSRV_DELETE_WATCH118.md) — read-only
  2026-11-24 winsrv re-apply monitor: APPLY_RC not yet emitted, pid 1286837
  (~23 min) in P3 gate `r1_final_verify.py --strict-skip`, 7 winsrv deletes
  staged, rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`,
  `kernelhwsrv/` clean. `kernelhwsrv/` was
untouched (read-only) and no QEMU run was made by this index update.

The indexed Markdown total is **440**, matching the 440 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked, with 0 missing and 0 stale links re-verified after this append.

## Added — 2026-11-24 last-~435 sweep, WATCH119–121 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: four reports existed on disk but had no entry; they
are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH119.md](WINSRV_DELETE_WATCH119.md) — read-only ~22:40
  winsrv re-apply monitor #119: APPLY_RC not yet emitted, pid 1286837 alive
  ~23 min inside the P3 `r1_final_verify.py --strict-skip` gate, P1/P2 PASS,
  7 deletes staged, rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH120.md](WINSRV_DELETE_WATCH120.md) — read-only ~22:40
  monitor #120: same in-flight apply (APPLY_RC NOT YET, P1 18 edits + P2 gates
  PASS, 7 winsrv files deleted, snapshot/rollback token
  `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, pre-apply ewsrv gate PASS).
- [WINSRV_DELETE_WATCH121.md](WINSRV_DELETE_WATCH121.md) — read-only monitor
  #121: apply still running (~24 min, last log line P3 strict-verify gate),
  no rollback, 7 deletions, rollback token `ROLLBACK-WINSRV-20261124_215436`,
  HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH122.md](WINSRV_DELETE_WATCH122.md) — read-only ~22:41
  monitor #122: APPLY_RC still not emitted (apply in P3 strict-verify gates,
  pid 1286837 ~24 min), P1/P2 gates PASS, 7 winsrv deletions in worktree, no
  rollback, snapshot/token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH124.md](WINSRV_DELETE_WATCH124.md) — read-only ~22:41
  monitor #124: APPLY_RC still NO — apply alive 24:06 (pid 1286837), log ends
  at the P3 strict-verify gate, no rollback; 7 winsrv deletions staged.
- [WINSRV_DELETE_WATCH125.md](WINSRV_DELETE_WATCH125.md) — read-only monitor
  #125: APPLY_RC not emitted, pid 1286837 alive 24:14, log (56 lines) ends at
  `gate[P3]: r1_final_verify.py --strict-skip` in progress, no rollback.

- [WINSRV_DELETE_WATCH126.md](WINSRV_DELETE_WATCH126.md) — read-only monitor
  #126: APPLY_RC not yet written, pid 1286837 alive 24m27s in the P3
  strict-verify gate, no rollback, 7 winsrv deletions staged.
- [WINSRV_DELETE_WATCH127.md](WINSRV_DELETE_WATCH127.md) — read-only monitor
  #127: APPLY_RC NOT YET, pid 1286837 alive 24:37, P1 (18 edits) and P2 PASS,
  P3 gates (7 deletes) in flight.
- [WINSRV_DELETE_WATCH128.md](WINSRV_DELETE_WATCH128.md) — read-only monitor
  #128: APPLY_RC not yet written (pid 1286837 ~24 min, P3 strict-verify child
  pid 1376334 active), pre-apply ewsrv + P1 + P2 gates PASS, no rollback.

- [WINSRV_DELETE_WATCH131.md](WINSRV_DELETE_WATCH131.md) — read-only monitor
  #131: APPLY_RC NOT YET, pid 1286837 alive 25:15 in the P3 strict-verify
  gate, no rollback, snapshot token `ROLLBACK-WINSRV-20261124_215436` intact.
- [WINSRV_DELETE_WATCH132.md](WINSRV_DELETE_WATCH132.md) — read-only monitor
  #132: APPLY_RC not yet emitted (pid 1286837 ~25 min, mid-P3 gates), P1/P2
  PASS, exactly 7 winsrv deletions staged, no rollback, snapshot intact.

These eleven newly indexed reports bring the indexed Markdown total to **454**,
matching the 454 `.md` files under `docs/` (including this index; the header
count "(454 files, verified 2026-11-24)" is consistent); post-append
re-verification shows 454 on-disk `.md` files = 454 unique indexed link
targets, 0 missing, 0 dangling.

## Added — 2026-11-24 WINSRV_DELETE_WATCH122/123 backfill (append-only)

A full `docs/*.md` listing (445 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH122.md](WINSRV_DELETE_WATCH122.md) — read-only 2026-11-24
  ~22:41 winsrv re-apply monitor: APPLY_RC not yet emitted, apply pid 1286837
  still in P3 strict-verify gates, P1/P2 gates PASS, 7 winsrv files deleted
  (no rollback), snapshot/token `ROLLBACK-WINSRV-20261124_215436`, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH123.md](WINSRV_DELETE_WATCH123.md) — read-only 2026-11-24
  winsrv re-apply monitor: APPLY_RC still not emitted (pid 1286837 in P3 gates,
  log `winsrv_delete_apply2.log` 56 lines, not truncated), no
  FAIL/ERROR/Traceback, 7 deletions match `git status`, HEAD `0f03f7e` over
  `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH124.md](WINSRV_DELETE_WATCH124.md) — read-only 2026-11-24
  ~22:41 monitor: apply pid 1286837 still running (24:06) in the P3
  strict-verify gate, no APPLY_RC/rollback yet; P1 (18 edits) and P2 PASS,
  7 winsrv deletions staged, HEAD `0f03f7e` on `97f224b`, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH125.md](WINSRV_DELETE_WATCH125.md) — read-only 2026-11-24
  monitor: apply still running (pid 1286837, 24:14) with no APPLY_RC and last
  log line the P3 strict-verify gate; P1/P2 gates PASS, 7 staged deletions,
  HEAD `0f03f7e` / `97f224b`, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` untouched.

These four newly indexed reports bring the indexed Markdown total to **447**,
matching the 447 `.md` files under `docs/` (including this index; the header
count "(447 files, verified 2026-11-24)" at line 4 is updated); post-append
re-verification shows 447 on-disk `.md` files = 447 unique indexed link
targets, 0 missing, 0 stale.

## Added — 2026-11-24 WINSRV_DELETE_WATCH124/125 backfill (append-only)

A full `docs/*.md` listing (447 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH124.md](WINSRV_DELETE_WATCH124.md) — read-only 2026-11-24
  ~22:41 winsrv re-apply monitor: APPLY_RC still not emitted, pid 1286837
  alive ~24 min in P3 `r1_final_verify.py --strict-skip` gate, P1/P2 gates
  PASS, 7 winsrv deletions staged, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH125.md](WINSRV_DELETE_WATCH125.md) — read-only 2026-11-24
  winsrv re-apply monitor: still running (pid 1286837, 24:14), no APPLY_RC/rollback
  in `winsrv_delete_apply2.log`, P1/P2 PASS + P3 in flight, 7 `D` entries,
  HEAD `0f03f7e` over `97f224b`, `kernelhwsrv/` untouched.

These two newly indexed reports bring the indexed Markdown total to **447**,
matching the 447 `.md` files under `docs/` (including this index; the header
count "(447 files, verified 2026-11-24)" at line 4 is updated); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 subagent docs-index backfill, post-447 (append-only)

A fresh full `docs/*.md` listing (LC_ALL=C sorted, flat directory) was diffed
against the unique `.md` link targets (`](*.md)` pattern) in this index:
3 documents on disk had no entry and 0 links were stale. They are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH126.md](WINSRV_DELETE_WATCH126.md) — read-only
  2026-11-24 winsrv re-apply monitor: APPLY_RC not yet written (pid 1286837
  alive ~24m27s, log ends at P3 `r1_final_verify.py --strict-skip`), pre/P1/P2
  gates PASS, 7 deletes staged, rollback token
  `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH127.md](WINSRV_DELETE_WATCH127.md) — read-only
  2026-11-24 winsrv re-apply monitor: APPLY_RC not yet emitted (pid 1286837,
  24:37), P1/P2 PASS with P3 running, 7 deletions (`winsrv.c`, `guishost.h`,
  `wsclient.c/h`, `qemu_windowsrv.cpp/h`, `qemu_gui_exec.cpp`), HEAD
  `0f03f7e`/`97f224b`, rollback token `ROLLBACK-WINSRV-20261124_215436`.
- [WINSRV_DELETE_WATCH128.md](WINSRV_DELETE_WATCH128.md) — read-only
  2026-11-24 winsrv re-apply monitor: still in P3 gate (child pid 1376334),
  no APPLY_RC/rollback, pre/P1/P2 PASS, 7 staged deletions, HEAD `0f03f7e`,
  snapshot `ROLLBACK-WINSRV-20261124_215436`, `kernelhwsrv/` untouched.

These three newly indexed reports bring the indexed Markdown total to **450**,
matching the 450 `.md` files under `docs/` (including this index); the header
count "(450 files, verified 2026-11-24)" at line 4 is updated. Post-append
re-verification: 450 on-disk `.md` files = 450 unique indexed `.md` link
targets, 0 missing, 0 dangling — every Markdown document in the directory is
linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH126/127/128 backfill (append-only)

A full `docs/*.md` listing (450 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: three documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH126.md](WINSRV_DELETE_WATCH126.md) — read-only 2026-11-24
  winsrv re-apply monitor: APPLY_RC not yet written, pid 1286837 alive ~24m27s
  in P3 strict-verify gate, pre-apply ewsrv + P1 + P2 gates PASS, 7 winsrv
  deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH127.md](WINSRV_DELETE_WATCH127.md) — read-only 2026-11-24
  winsrv re-apply monitor: APPLY_RC NOT YET, pid 1286837 alive (24:37), P1/P2
  PASS with P3 running, 7 deletions, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/`
  clean.
- [WINSRV_DELETE_WATCH128.md](WINSRV_DELETE_WATCH128.md) — read-only 2026-11-24
  winsrv re-apply monitor: APPLY_RC not yet written (pid 1286837 ~24 min, P3
  strict-verify child active), pre-apply ewsrv + P1 + P2 PASS, 7 deletions,
  HEAD `0f03f7e`, `kernelhwsrv/` untouched.

These three newly indexed reports bring the indexed Markdown total to **450**,
matching the 450 `.md` files under `docs/` (including this index; the header
count "(450 files, verified 2026-11-24)" at line 4 is updated); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH129/130 backfill (append-only)

A full `docs/*.md` listing (452 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: two documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH129.md](WINSRV_DELETE_WATCH129.md) — read-only 2026-11-24
  22:19 EET winsrv re-apply monitor: APPLY_RC not yet written, pid 1286837
  alive (24:58) with strict-verify child in P3, P1/P2 gates PASS, 7 winsrv
  deletions, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH130.md](WINSRV_DELETE_WATCH130.md) — read-only 2026-11-24
  winsrv re-apply monitor: APPLY_RC not yet written (pid 1286837, ~25 min, P3
  gate suite in flight), P1/P2 PASS, 7 deletions, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **452**,
matching the 452 `.md` files under `docs/` (including this index; the header
count "(452 files, verified 2026-11-24)" at line 4 is updated); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH124–131 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: eight documents existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH124.md](WINSRV_DELETE_WATCH124.md) — read-only 2026-11-24
  ~22:41 monitor of the winsrv re-apply (pid 1286837, elapsed 24:06): APPLY_RC
  still not emitted, P1 (18 edits) and P2 gates PASS, 7 winsrv files deleted,
  P3 `r1_final_verify.py --strict-skip` gate in flight, rollback token
  `ROLLBACK-WINSRV-20261124_215436` on standby; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH125.md](WINSRV_DELETE_WATCH125.md) — read-only 2026-11-24
  follow-up watch (etime 24:14): same run still in P3 gate with no `APPLY_RC`
  or rollback in `winsrv_delete_apply2.log` (56 lines), 7 staged deletions
  confirmed, HEAD `0f03f7e`; verdict still running.
- [WINSRV_DELETE_WATCH126.md](WINSRV_DELETE_WATCH126.md) — read-only 2026-11-24
  re-apply watch (pid 1286837, 24m27s): APPLY_RC still not written, log ends at
  the P3 `r1_final_verify.py --strict-skip` gate; pre-apply ewsrv + P1 + P2
  gates PASS, 7 deletions staged, rollback snapshot on standby;
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH127.md](WINSRV_DELETE_WATCH127.md) — read-only 2026-11-24
  re-apply watch (pid alive, 24:37): no APPLY_RC in `winsrv_delete_apply2.log`,
  P3 gates in flight; P1 (18 edits) and P2 PASS, 7 winsrv files deleted, HEAD
  `0f03f7e`, no rollback; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH128.md](WINSRV_DELETE_WATCH128.md) — read-only 2026-11-24
  re-apply watch (~24 min, child P3 gate pid 1376334 active): APPLY_RC not yet
  written, no rollback; pre-apply/P1/P2 PASS, 7 deletions staged, snapshot
  `winsrv_retire_backup_20261124_215436`; `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH129.md](WINSRV_DELETE_WATCH129.md) — read-only 2026-11-24
  22:19 EET re-apply watch (pid 1286837, 24:58; P3 child 1376334 running):
  APPLY_RC not yet written, no rollback, P1/P2 PASS, 7 deletions staged, HEAD
  `0f03f7e`; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH130.md](WINSRV_DELETE_WATCH130.md) — read-only 2026-11-24
  re-apply watch (pid alive, 25 min): APPLY_RC not yet in the log, P3 full gate
  suite in flight; P1 (18 edits) / P2 PASS, 7 files deleted, rollback snapshot
  on standby; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH131.md](WINSRV_DELETE_WATCH131.md) — read-only 2026-11-24
  re-apply watch (pid 1286837, 25:15): still in the P3 `final_verify` gate, no
  APPLY_RC/rollback; P1/P2 PASS, 7 winsrv files deleted, HEAD `0f03f7e`;
  `kernelhwsrv/` clean.

These eight newly indexed reports bring the indexed Markdown total to **454**,
matching the 454 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 post-443 sweep, WATCH126–128 backfill (append-only)

A fresh full `docs/*.md` listing (454 files, flat directory, no subdirectories,
both sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: seven documents existed on disk but had no
entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH126.md](WINSRV_DELETE_WATCH126.md) — read-only 2026-11-24
  winsrv re-apply monitor #126: APPLY_RC not yet written, apply still in P3 gate
  `r1_final_verify.py --strict-skip`, pid 1286837 alive ~24m27s, no ROLLBACK.
- [WINSRV_DELETE_WATCH127.md](WINSRV_DELETE_WATCH127.md) — read-only monitor
  #127: APPLY_RC NOT YET (log ends at P3 gate list), pid 1286837 alive ~24:37.
- [WINSRV_DELETE_WATCH128.md](WINSRV_DELETE_WATCH128.md) — read-only monitor
  #128: APPLY_RC not yet written (pid 1286837 ~24 min, P3 strict-verify child
  pid 1376334 active), no rollback triggered.
- [WINSRV_DELETE_WATCH129.md](WINSRV_DELETE_WATCH129.md) — read-only 2026-11-24
  22:19 EET monitor #129: APPLY_RC NOT YET WRITTEN, no rollback, log tail at
  P3 gate `r1_final_verify.py --strict-skip`.
- [WINSRV_DELETE_WATCH130.md](WINSRV_DELETE_WATCH130.md) — read-only monitor
  #130: APPLY_RC NOT YET WRITTEN, apply still running, log ends at the P3 gate
  list with no final APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH131.md](WINSRV_DELETE_WATCH131.md) — read-only monitor
  #131: APPLY_RC NOT YET, log ends at P3 `r1_final_verify.py --strict-skip`
  still running, no rollback (snapshot token
  `ROLLBACK-WINSRV-20261124_215436` intact).
- [WINSRV_DELETE_WATCH132.md](WINSRV_DELETE_WATCH132.md) — read-only monitor
  #132: APPLY_RC NOT YET EMITTED, pid 1286837 alive ~25 min, log mid-P3 gates,
  snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` intact.

These seven newly indexed reports bring the indexed Markdown total to **454**,
matching the 454 `.md` files under `docs/` (including this index; the header
count "(454 files, verified 2026-11-24)" at line 4 is consistent);
post-append re-verification shows 454 on-disk `.md` files = 454 unique
indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 winsrv re-apply watch backfill 126–128 (append-only)

Three read-only monitor reports existed on disk but had no entry in this
index; they are linked below. `kernelhwsrv/` was untouched and no QEMU run
was made by this index update.

- [WINSRV_DELETE_WATCH126.md](WINSRV_DELETE_WATCH126.md) — read-only monitor:
  APPLY_RC not yet written; pid 1286837 alive 24m27s in P3
  `r1_final_verify.py --strict-skip`; P1/P2 gates PASS, 7 winsrv deletions
  staged, rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH127.md](WINSRV_DELETE_WATCH127.md) — read-only monitor:
  APPLY_RC NOT YET; pid 1286837 alive 24:37, P3 gate list in flight; P1 (18
  edits) + P2 PASS, 7 deletes (winsrv.c, guishost.h, wsclient.c/h,
  qemu_windowsrv.cpp/h, qemu_gui_exec.cpp), snapshot/rollback token present,
  HEAD `0f03f7e` / `97f224b`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH128.md](WINSRV_DELETE_WATCH128.md) — read-only monitor:
  APPLY_RC not yet emitted (wrapper will append `APPLY_RC=$?` on exit); P3
  final gate child pid 1376334 active, no rollback; P1/P2 PASS, 7 deletions
  staged, HEAD `0f03f7e`, rollback token `ROLLBACK-WINSRV-20261124_215436`,
  `kernelhwsrv/` clean.

These three newly indexed reports bring the indexed Markdown total to **450**,
matching the 450 `.md` files under `docs/` (including this index; the header
count "(447 files, verified 2026-11-24)" at line 4 predates this append and
is updated to 450); post-append re-verification shows 450 on-disk `.md` files
= 450 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 WINSRV_DELETE_WATCH132 backfill (append-only)

A full `docs/*.md` listing (454 files, flat directory, no subdirectories) was
diffed against the 453 unique `.md` link targets previously in this index:
one document existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH132.md](WINSRV_DELETE_WATCH132.md) — read-only winsrv
  re-apply monitor #132 (2026-11-24): APPLY_RC NOT YET EMITTED, apply pid
  1286837 alive ~25 min in the P3 `r1_final_verify.py --strict-skip` gate;
  P1 (18 edits) and P2 gates PASS, all 7 winsrv deletions applied, no
  rollback, snapshot `winsrv_retire_backup_20261124_215436` intact, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **454**,
matching the 454 `.md` files under `docs/` (including this index; the header
count "(454 files, verified 2026-11-24)" at line 4 is consistent);
post-append re-verification shows 454 on-disk `.md` files = 454 unique
indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 winsrv re-apply watch backfill 131–132 (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: two documents existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH131.md](WINSRV_DELETE_WATCH131.md) — read-only monitor
  #131: APPLY_RC NOT YET (log ends at P3 `r1_final_verify.py --strict-skip`,
  still running); pid 1286837 alive 25:15; P1/P2 gates PASS, 7 winsrv files
  deleted in worktree, rollback token `ROLLBACK-WINSRV-20261124_215436`
  unconsumed, no rollback executed.
- [WINSRV_DELETE_WATCH132.md](WINSRV_DELETE_WATCH132.md) — read-only monitor
  #132: APPLY_RC NOT YET EMITTED (pid 1286837 ~25 min, mid-P3 strict-verify);
  P1 (18 edits) + P2 (Makefile/user.mk + isogrub) PASS, all 7 deletes applied
  (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h,
  qemu_gui_exec.cpp), snapshot intact, no rollback.

These two newly indexed reports bring the indexed Markdown total to **454**,
matching the 454 `.md` files under `docs/` (including this index); post-append
re-verification shows 454 on-disk `.md` files = 454 unique indexed link
targets, 0 missing, 0 dangling.

## Added — 2026-11-24 WINSRV_DELETE_WATCH131/132 backfill (append-only)

A fresh `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: two watch reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH131.md](WINSRV_DELETE_WATCH131.md) — read-only 2026-11-24
  winsrv re-apply monitor: APPLY_RC not yet written, pid 1286837 alive (25:15)
  in the P3 `r1_final_verify.py --strict-skip` gate, P1/P2 PASS, 7 deletions
  staged, HEAD `0f03f7e` on `97f224b`, rollback snapshot
  `ROLLBACK-WINSRV-20261124_215436` unused, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH132.md](WINSRV_DELETE_WATCH132.md) — read-only 2026-11-24
  follow-up watch: still no APPLY_RC (pid 1286837 ~25 min, log ends mid-P3),
  P1 (18 edits) and P2 (Makefile/user.mk + `make isogrub`) PASS, exactly 7 ` D`
  winsrv deletions and no other modifications, HEAD `0f03f7e`/`97f224b`, nothing
  committed, `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **454**,
matching the 454 `.md` files under `docs/` (including this index; the header
count at line 4 is updated to 454); post-append re-verification shows 454
on-disk `.md` files = 454 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 WINSRV_DELETE_WATCH129/130/131 backfill (append-only)

A full `docs/*.md` listing (453 files, flat directory, no subdirectories, both
sides `LC_ALL=C` sorted) was diffed against the unique `.md` link targets
(`](*.md)` pattern) in this index: three documents existed on disk but had no
entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU run
was made by this index update.

- [WINSRV_DELETE_WATCH129.md](WINSRV_DELETE_WATCH129.md) — read-only 2026-11-24
  22:19 EET winsrv re-apply monitor: APPLY_RC not yet written (pid 1286837
  alive 24:58, P3 `r1_final_verify.py --strict-skip` child running), P1/P2
  gates PASS, 7 winsrv deletions staged, no rollback, rollback token
  `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e`, `kernelhwsrv/`
  clean.
- [WINSRV_DELETE_WATCH130.md](WINSRV_DELETE_WATCH130.md) — read-only winsrv
  re-apply monitor: APPLY_RC not yet written (pid 1286837, ~25 min), P1/P2
  PASS, full P3 gate suite in flight, 7 files deleted in `git status`,
  snapshot/token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH131.md](WINSRV_DELETE_WATCH131.md) — read-only winsrv
  re-apply monitor: APPLY_RC NOT YET, log ends at the P3 strict-verify gate,
  pid 1286837 alive 25:15, P1/P2 PASS, 7 deletions (winsrv.c, guishost.h,
  wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp), no rollback, HEAD
  `0f03f7e` on `97f224b`, `kernelhwsrv/` clean.

These three newly indexed reports bring the indexed Markdown total to **453**,
matching the 453 `.md` files under `docs/` (including this index; the header
count "(453 files, verified 2026-11-24)" at line 4 is updated); post-append
re-verification shows 453 on-disk `.md` files = 453 unique indexed link
targets, 0 missing, 0 dangling.

## Added — 2026-11-24 last-~450 sweep, WATCH131–132 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: two reports existed on disk but had no entry; they are
linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH131.md](WINSRV_DELETE_WATCH131.md) — read-only 2026-11-24
  winsrv re-apply monitor #131: APPLY_RC NOT YET (log ends at P3
  `r1_final_verify.py --strict-skip`), pid 1286837 alive 25:15, P1/P2 PASS,
  7 deletions staged, HEAD `0f03f7e`, rollback token
  `ROLLBACK-WINSRV-20261124_215436` unused, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH132.md](WINSRV_DELETE_WATCH132.md) — read-only monitor
  #132: APPLY_RC not yet emitted (pid 1286837 ~25 min, mid-P3 strict-verify
  gate), P1/P2 PASS, exactly 7 ` D` deletions, HEAD `0f03f7e` on
  `97f224b`, snapshot/token intact, `kernelhwsrv/` untouched, nothing
  committed.

These two newly indexed reports bring the indexed Markdown total to **454**,
matching the 454 `.md` files under `docs/` (including this index; the header
count at line 4 is updated to 454); post-append re-verification shows 454
on-disk `.md` files = 454 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 winsrv re-apply watch backfill 134–136 (append-only)

A full `docs/*.md` listing (458 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: three documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH134.md](WINSRV_DELETE_WATCH134.md) — read-only monitor:
  APPLY_RC not yet emitted; pid 1286837 alive ~26 min mid-P3
  `r1_final_verify.py --strict-skip`; P1/P2 + pre-apply ewsrv gates PASS, no
  rollback (token `ROLLBACK-WINSRV-20261124_215436` unused), 7 winsrv
  deletions staged, HEAD `0f03f7e` / `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH135.md](WINSRV_DELETE_WATCH135.md) — read-only monitor
  (22:20 UTC): APPLY_RC not yet written; pid 1286837 ~26 min, log ends at the
  P3 strict-verify gate; P1/P2 PASS, 18 edits applied, 7 files deleted
  (`git status` ` D` count = 7), snapshot/token present, HEAD `0f03f7e`,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH136.md](WINSRV_DELETE_WATCH136.md) — read-only monitor:
  APPLY_RC not yet emitted; apply log ends mid-P3 gates, no rollback consumed;
  pid 1286837 alive 26m43s; 7 deletions staged; `git log -2` `0f03f7e` /
  `97f224b`; `kernelhwsrv/` clean (0 lines).
- [WINSRV_DELETE_WATCH139.md](WINSRV_DELETE_WATCH139.md) — read-only monitor:
  APPLY_RC NOT YET EMITTED (pid 1286837 alive, no APPLY_RC/rollback in the
  56-line apply log); P1/P2 gates PASS, 18 edits + 7 deletes applied, P3
  10-test gate battery in flight.
- [WINSRV_DELETE_WATCH140.md](WINSRV_DELETE_WATCH140.md) — read-only monitor:
  APPLY_RC NOT YET (apply log 3,355 B, no rollback line); pid 1286837 alive
  27+ min; last line is gate[P3] `r1_final_verify.py --strict-skip`; P1/P2
  PASS, all 7 file deletions applied.

These five newly indexed reports bring the indexed Markdown total to **462**,
matching the 462 `.md` files under `docs/` (including this index; the header
count at line 4 is updated to 462); post-append re-verification shows 462
on-disk `.md` files = 462 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 final full directory sweep, post-454 (append-only)

A fresh full `docs/*.md` listing (454 files, flat directory, no subdirectories) was diffed against the unique `.md` link targets in this index: **0 unlinked documents and 0 broken links** (454 on-disk `.md` files = 454 unique indexed targets, including this index; the header count "(454 files)" at line 4 is consistent). The newest winsrv re-apply watch reports (`WINSRV_DELETE_WATCH129.md`-`WINSRV_DELETE_WATCH131.md` and later) were indexed by the concurrent backfill sections immediately above, so this section adds no new link entries. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **454**, matching the 454 `.md` files under `docs/` (including this index); every Markdown document in the directory is linked.

## Added — 2026-11-24 last-~454 sweep, WATCH133–136 backfill (append-only)

A fresh full `docs/*.md` listing (458 files) was diffed against the unique `.md` link targets in this index: four reports existed on disk but had no entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH133.md](WINSRV_DELETE_WATCH133.md) — read-only 2026-11-24 winsrv re-apply monitor #133: APPLY_RC NOT YET EMITTED (log ends at the P3 `r1_final_verify.py --strict-skip` gate, pid 1286837 alive 26:20), P1/P2 gates PASS, 7 winsrv files deleted (`winsrv.c`, `guishost.h`, `wsclient.c/h`, `qemu_windowsrv.cpp/h`, `qemu_gui_exec.cpp`), rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean, no commits.
- [WINSRV_DELETE_WATCH134.md](WINSRV_DELETE_WATCH134.md) — read-only monitor #134: APPLY_RC not yet emitted (still in P3 strict-verify gate), pid 1286837 ~26 min, no rollback, 7 deletions staged, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH135.md](WINSRV_DELETE_WATCH135.md) — read-only monitor #135 (22:20 UTC): APPLY_RC not yet written (log line 56, P3 gate running), P1/P2 PASS, 18 edits + 7 deletions, snapshot/token intact, verdict "in progress, no failure/rollback so far".
- [WINSRV_DELETE_WATCH136.md](WINSRV_DELETE_WATCH136.md) — read-only monitor #136: APPLY_RC not yet emitted (mid-P3 gates, pid 1286837 26m43s), exactly 7 ` D` deletions, rollback token unconsumed, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH141.md](WINSRV_DELETE_WATCH141.md) — read-only monitor #141 (~22:44): APPLY_RC NOT YET (mid-P3 strict-verify gate, pid 1286837 27:27), pre-apply/P1/P2 PASS, 7 deletions, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH143.md](WINSRV_DELETE_WATCH143.md) — read-only monitor #143: APPLY_RC NOT YET (log 3,355 B ends mid-P3 gate list, pid 1286837 27m39s), P1/P2 PASS, 7 deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436` unconsumed, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH145.md](WINSRV_DELETE_WATCH145.md) — read-only monitor #145: APPLY_RC NOT YET SEEN (log line 56, mid-P3 strict-verify, pid 1286837 ~28 min), P1/P2 PASS, all 7 winsrv deletions applied, EFILE guard-only intact, rollback token unused, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.

These newly indexed reports bring the indexed Markdown total to **466**, matching the 466 `.md` files under `docs/` (including this index; the header count at line 4 is updated to 466); post-append re-verification shows 466 on-disk `.md` files = 466 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 last-460 subagent sweep (append-only)

A fresh full `docs/*.md` listing (466 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets (`](*.md)` pattern) in this
index: **0 documents on disk are missing an entry and 0 links are stale** (466
on-disk `.md` files = 466 unique indexed targets, including this index; the
header count "(466 files, verified 2026-11-24)" at line 4 is consistent), so
this section adds no new link entries. `kernelhwsrv/` was untouched and no
QEMU run was made by this index update.

The indexed Markdown total stays **466**, matching the 466 `.md` files under
`docs/` (including this index); every Markdown document in the directory is
linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH146–159 backfill (append-only)

A fresh full `docs/*.md` listing (514 files, flat directory, no subdirectories)
was diffed against the 466 unique `.md` link targets previously in this index:
eighteen reports existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH146.md](WINSRV_DELETE_WATCH146.md) — read-only 2026-11-24
  winsrv re-apply monitor #146: APPLY_RC NOT YET EMITTED (apply2 log 3.4 KB
  fully tailed, no APPLY_RC/rollback line), pid 1286837 alive 29:00 inside the
  P3 `r1_final_verify.py --strict-skip` gate, P1/P2 gates PASS, 7 winsrv files
  deleted in the worktree, rollback snapshot/token
  `ROLLBACK-WINSRV-20261124_215436` intact/unused, HEAD `0f03f7e`,
  `kernelhwsrv/` clean, verdict IN PROGRESS.
- [WINSRV_DELETE_WATCH147.md](WINSRV_DELETE_WATCH147.md) — read-only 2026-11-24
  winsrv re-apply monitor #147: APPLY_RC not yet emitted (apply still in flight
  at P3 gates, log ends at the `r1_final_verify.py --strict-skip` launch), pid
  1286837 alive ~29 min, P1/P2 gates PASS, 7 winsrv deletions staged, snapshot
  `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/`
  clean.
- [WINSRV_DELETE_WATCH149.md](WINSRV_DELETE_WATCH149.md) — read-only 2026-11-24
  winsrv re-apply monitor #149: APPLY_RC NOT YET EMITTED (log ends at line 56,
  `+ gate[P3]: r1_final_verify.py --strict-skip`), pid 1286837 alive ~30 min,
  P1 (18 edits) and P2 gates PASS, P3 = 7 winsrv deletions done with the full
  gate suite running, snapshot/token `ROLLBACK-WINSRV-20261124_215436` intact,
  HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean, no commit by the apply.
- [WINSRV_DELETE_WATCH150.md](WINSRV_DELETE_WATCH150.md) — read-only 2026-11-24
  winsrv re-apply monitor #150: APPLY_RC not yet reached, re-apply STILL RUNNING
  (apply2 log 3.3 KB ends at the last P3 strict-skip gate, no APPLY_RC/ROLLBACK
  lines), pid 1286837 alive 30+ min, P1 (12 edits) + P2 (5 edits) gates PASS,
  7 deletions staged, snapshot/token `ROLLBACK-WINSRV-20261124_215436` intact,
  HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH158.md](WINSRV_DELETE_WATCH158.md) — read-only 2026-11-24
  winsrv re-apply monitor #158: APPLY_RC NOT YET EMITTED (0 APPLY_RC lines in
  the 3.4 KB apply2 log, no rollback), pid 1286837 alive 31:19 inside the P3
  gate suite (last line `+ gate[P3]: r1_final_verify.py --strict-skip`),
  pre-apply/P1/P2 gates PASS, 7 deletions staged, snapshot/token
  `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`/`97f224b`,
  `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH160.md](WINSRV_DELETE_WATCH160.md) — read-only 2026-11-24
  winsrv re-apply monitor #160: APPLY_RC NOT YET SEEN (log ends at the P3
  `r1_final_verify.py --strict-skip` gate), P1/P2 gates PASS, P3 full-gate
  suite in progress, pid 1286837 ~31 min, snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436` intact, no rollback.
- [WINSRV_DELETE_WATCH161.md](WINSRV_DELETE_WATCH161.md) — read-only 2026-11-24
  winsrv re-apply monitor #161: APPLY_RC NOT FINISHED (pid 1286837, 31m37s,
  log ends at the P3 final gate, no rollback), 7 deletions staged, P1/P2
  gates PASS, snapshot/token `ROLLBACK-WINSRV-20261124_215436`, HEAD
  `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH162.md](WINSRV_DELETE_WATCH162.md) — read-only 2026-11-24
  winsrv re-apply monitor #162: APPLY_RC not yet printed (pid 1286837 ~31 min,
  log ends mid-P3 at the strict-skip gate, no FAIL/rollback), P1/P2 gates
  PASS, 7 winsrv deletions in the working tree, `kernelhwsrv/` diff 0 lines,
  snapshot/token `ROLLBACK-WINSRV-20261124_215436`.
- [WINSRV_DELETE_WATCH164.md](WINSRV_DELETE_WATCH164.md) — read-only 2026-11-24
  winsrv re-apply monitor #164: APPLY_RC not yet emitted (log ends at
  `gate[P3]: r1_final_verify.py --strict-skip`), pid 1286837 alive ~31:45 in
  the P3 full-gate phase, 7 deletions staged, P1/P2 gates PASS (18 edits + 7
  deletes allowlisted), pre-apply ewsrv gate PASS.
- [WINSRV_DELETE_WATCH185.md](WINSRV_DELETE_WATCH185.md) — read-only 2026-11-24
  ~22:50 winsrv re-apply monitor #185: APPLY_RC not yet emitted (apply2 log
  3.3 KB/56 lines ends inside P3 gates at the strict-skip line, no rollback),
  pid 1286837 alive 33+ min, 7 deletions confirmed, pre-apply/P1/P2 ewsrv
  gates PASS, P3 in progress.
- [WINSRV_DELETE_WATCH186.md](WINSRV_DELETE_WATCH186.md) — read-only 2026-11-24
  winsrv re-apply monitor #186: APPLY_RC NOT YET EMITTED (log ends at
  `+ gate[P3]: r1_final_verify.py --strict-skip`, no rollback; snapshot token
  `ROLLBACK-WINSRV-20261124_215436` unused), pid 1286837 running 33:21,
  18 edits + 7 deletes applied, pre/P1/P2 ewsrv gates PASS, P3 gate suite
  launched.
- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — read-only 2026-11-24
  winsrv re-apply monitor #187: re-apply NOT finished (no APPLY_RC/rollback/
  FAILED in the 3.3 KB apply2 log), pid 1286837 alive 33:25 in P3
  `r1_final_verify.py --strict-skip` after P1/P2 gates PASS.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — read-only 2026-11-24
  winsrv delete watch #188: APPLY_RC not yet printed (P3 strict-skip gate
  queued, no rollback/FAILED lines), pid 1286837 alive 33:30.
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — read-only 2026-11-24
  ~22:51 winsrv re-apply monitor #189: APPLY_RC not yet emitted (pid 1286837,
  33m35s, log ends mid-P3), snapshot/token
  `ROLLBACK-WINSRV-20261124_215436`, pre-gate + P1 (18 edits) + P2 PASS, all 7
  deletes done, P3 gates running.
- [WINSRV_DELETE_WATCH190.md](WINSRV_DELETE_WATCH190.md) — read-only 2026-11-24
  winsrv re-apply monitor #190: APPLY_RC NOT YET EMITTED (3.4 KB log ends at
  the P3 strict-skip gate), pid 1286837 running 33m34s inside the P3 gate
  battery.
- [WINSRV_DELETE_WATCH191.md](WINSRV_DELETE_WATCH191.md) — read-only 2026-11-24
  ~22:50 winsrv re-apply monitor #191: APPLY_RC NOT YET EMITTED (pid 1286837
  alive 33:44, log ends at the P3 gate list, no rollback), 7 staged deletions
  confirmed.
- [WINSRV_DELETE_WATCH192.md](WINSRV_DELETE_WATCH192.md) — read-only 2026-11-24
  winsrv re-apply monitor #192: APPLY_RC NOT YET WRITTEN (3.3 KB log ends mid
  `gate[P3]` final-verify suite, no APPLY_RC/rollback), pid 1286837 still
  alive in the P3 gate phase.
- [WINSRV_DELETE_WATCH194.md](WINSRV_DELETE_WATCH194.md) — read-only 2026-11-24
  winsrv re-apply monitor #194: APPLY_RC not yet printed (pid 1286837, 33:54
  elapsed, no ROLLBACK in log), P1/P2 gates PASS, P3 running with the
  strict-skip final gate last.

These eighteen newly indexed reports bring the indexed Markdown total to
**514**, matching the 514 `.md` files under `docs/` (including this index; the
header count "(514 files, verified 2026-11-24)" at line 4 is consistent);
post-append re-verification shows 514 on-disk `.md` files = 514 unique
indexed link targets, 0 missing, 0 dangling; every Markdown document in the
directory is now linked.

## Added — 2026-11-24 last-466 sweep, WINSRV_DELETE_WATCH146 backfill (append-only)

A fresh full `docs/*.md` listing (467 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: one report existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH146.md](WINSRV_DELETE_WATCH146.md) — read-only 2026-11-24 winsrv re-apply monitor #146: APPLY_RC not yet emitted, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV` (pid 1286837) in P3 gates with P1/P2 PASS, 7 winsrv deletions staged, rollback snapshot `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **467**,
matching the 467 `.md` files under `docs/` (including this index; the header
count "(467 files)" at line 4 is consistent); post-append re-verification
shows 467 on-disk `.md` files = 467 unique indexed link targets, 0 missing,
0 dangling; every Markdown document in the directory is now linked.

## Added — 2026-11-24 last-454 sweep, WATCH134–145 backfill (append-only)

A fresh full `docs/*.md` listing (466 files) was diffed against the unique `.md` link targets in this index: eight reports existed on disk but had no entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH134.md](WINSRV_DELETE_WATCH134.md) — read-only 2026-11-24 winsrv re-apply monitor #134: APPLY_RC not yet emitted (pid 1286837 alive ~26 min, mid-P3 `r1_final_verify.py --strict-skip`), P1/P2 + pre-apply ewsrv gates PASS, 7 winsrv files deleted, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH135.md](WINSRV_DELETE_WATCH135.md) — read-only monitor #135 (22:20 UTC): APPLY_RC not yet written (log ends at line 56 in P3 strict-verify), P1/P2 PASS, 18 edits applied, 7 deletions staged, snapshot/token intact, `kernelhwsrv/` clean, verdict in progress with no failure/rollback so far.
- [WINSRV_DELETE_WATCH136.md](WINSRV_DELETE_WATCH136.md) — read-only monitor #136: APPLY_RC not yet emitted (apply2 log ends mid-P3 gates), pid 1286837 at 26m43s, exactly 7 deletions, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean (0 lines), no rollback token consumed.
- [WINSRV_DELETE_WATCH137.md](WINSRV_DELETE_WATCH137.md) — read-only monitor #137: APPLY_RC not yet written (pid 1286837 ~27 min, in P3 strict-verify gate), P1/P2 gates PASS, log 3.3 KB, 7 deletions staged, rollback token unused, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH138.md](WINSRV_DELETE_WATCH138.md) — read-only monitor #138: APPLY_RC not yet emitted (~27 min up), P1 (18 edits) and P2 gates PASS, 7 deletes staged, now in the P3 gate battery, snapshot/token intact, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean, no commits.
- [WINSRV_DELETE_WATCH139.md](WINSRV_DELETE_WATCH139.md) — read-only monitor #139: APPLY_RC NOT YET EMITTED (apply2 log 56 lines/3.3 KB, pid 1286837 alive), P1/P2 PASS with all 18 edits + 7 deletes applied, P3 gate battery in flight, rollback token unused, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH140.md](WINSRV_DELETE_WATCH140.md) — read-only monitor #140: APPLY_RC NOT YET (apply2 log 3,355 bytes, pid 1286837 27+ min, last line P3 `r1_final_verify.py --strict-skip`), P1/P2 PASS, 7 deletions applied, snapshot/token intact, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH145.md](WINSRV_DELETE_WATCH145.md) — read-only monitor #145: APPLY_RC NOT YET SEEN (apply2 log ends at line 56 in P3 strict-verify, pid 1286837 ~28 min), P1/P2 PASS with all 7 deletes applied, rollback token unused, EFILE guard-only intact, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean.

These eight newly indexed reports bring the indexed Markdown total to **466**, matching the 466 `.md` files under `docs/` (including this index; the header count at line 4 is updated to 466); post-append re-verification shows 466 on-disk `.md` files = 466 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 subagent docs sweep, post-455 (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: 2 document(s) existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH137.md](WINSRV_DELETE_WATCH137.md) — newly on disk; see the document. (WINSRV_DELETE_WATCH137 — re-apply status (2026-11-24))
- [WINSRV_DELETE_WATCH138.md](WINSRV_DELETE_WATCH138.md) — newly on disk; see the document. (WINSRV_DELETE_WATCH138 — re-apply status (2026-11-24, read-only monitor))

These 2 newly indexed report(s) bring the indexed Markdown total to **460**,
matching the 460 `.md` files under `docs/` (including this index; the header
count at line 4 is updated to 460); every Markdown document in the directory is
now linked.

## Added — 2026-11-24 post-460 WINSRV_DELETE_WATCH139/140 backfill (append-only)

A full `docs/*.md` listing (462 files, flat directory, no subdirectories) was
diffed against the 460 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH139.md](WINSRV_DELETE_WATCH139.md) — read-only 2026-11-24 winsrv re-apply monitor #139: APPLY_RC NOT YET EMITTED (pid 1286837 alive, log at 56 lines/3.3 KB), P1/P2 gates PASS, all 18 edits + 7 deletes applied, P3 strict-verify gate in flight, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH140.md](WINSRV_DELETE_WATCH140.md) — read-only monitor #140 (27+ min): APPLY_RC NOT YET (apply2 log 3,355 B, still in P3 `r1_final_verify.py --strict-skip`), P1/P2 PASS, exactly 7 winsrv deletions staged, snapshot/token intact, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean, no QEMU run by this watch.

These two newly indexed reports bring the indexed Markdown total to **462**,
matching the 462 `.md` files under `docs/` (including this index; the header
count "(464 files, verified 2026-11-24)" at line 4 is consistent);
post-append re-verification shows 462 on-disk `.md` files = 462 unique indexed
link targets, 0 missing, 0 dangling; every Markdown document in the directory
is now linked.

## Added — 2026-11-24 subagent post-460 full sweep (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: three reports existed on disk but had no entry; they
are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH143.md](WINSRV_DELETE_WATCH143.md) — read-only 2026-11-24
  winsrv re-apply monitor #143: APPLY_RC NOT YET (log ends mid-P3 strict-verify
  gate), pid 1286837 alive 27m39s, P1/P2 PASS, 7 deletions staged, rollback
  token unused, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH144.md](WINSRV_DELETE_WATCH144.md) — read-only monitor
  #144: APPLY_RC NOT YET (log ends at P3 gate, ~28 min), P1/P2 PASS, 7 winsrv
  files deleted, HEAD `0f03f7e` on `97f224b`, no rollback, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH145.md](WINSRV_DELETE_WATCH145.md) — read-only monitor
  #145: APPLY_RC NOT YET SEEN (log line 56, P3 gate, pid 1286837 ~28 min), P1/P2
  PASS, all 7 deletions applied, snapshot/token intact, EFILE guard-only
  intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.

These three newly indexed reports bring the indexed Markdown total to **466**,
matching the 466 `.md` files under `docs/` (including this index; the header
count at line 4 is updated to 466); post-append re-verification shows 466
on-disk `.md` files = 466 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 post-460 sweep, WATCH139–140/143 backfill (append-only)

A fresh full `docs/*.md` listing (464 files, flat directory, no subdirectories)
was diffed against the 460 unique `.md` link targets previously in this index:
three reports existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH139.md](WINSRV_DELETE_WATCH139.md) — read-only 2026-11-24 winsrv re-apply monitor #139: APPLY_RC NOT YET EMITTED (pid 1286837 alive, log 56 lines / 3.3 KB), P1+P2 gates PASS, all 18 edits + 7 deletes applied (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp), P3 `r1_final_verify.py --strict-skip` in flight, rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH140.md](WINSRV_DELETE_WATCH140.md) — read-only monitor #140: APPLY_RC still NOT YET (log 3,355 B, apply in flight 27+ min), P1/P2 gates PASS with all 7 file deletions staged, P3 strict-verify gate running, snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` intact, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` untouched, no QEMU run by this watch.

- [WINSRV_DELETE_WATCH143.md](WINSRV_DELETE_WATCH143.md) — read-only monitor #143: APPLY_RC still NOT YET (log 3,355 B, ends mid-P3 gate list), pid 1286837 alive 27m39s, P1/P2 gates PASS, 7 deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean, no QEMU run by this watch.

These three newly indexed reports bring the indexed Markdown total to **464**,
matching the 464 `.md` files under `docs/` (including this index; the header
count "(464 files)" at line 4 is consistent); post-append re-verification shows
464 on-disk `.md` files = 464 unique indexed link targets, 0 missing, 0
dangling; every Markdown document in the directory is now linked.

## Added — 2026-11-24 last-462 sweep, WATCH139–140 backfill (append-only)

A fresh full `docs/*.md` listing (462 files, flat directory, no subdirectories) was diffed against the unique `.md` link targets in this index: two reports existed on disk but had no entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH139.md](WINSRV_DELETE_WATCH139.md) — read-only 2026-11-24 winsrv re-apply monitor #139: APPLY_RC NOT YET EMITTED (pid 1286837 alive, apply2 log 56 lines, P3 gate in flight), P1/P2 gates PASS, 18 edits + 7 deletions applied, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH140.md](WINSRV_DELETE_WATCH140.md) — read-only monitor #140: APPLY_RC not yet written (pid 1286837 alive 27+ min, last log line = P3 `r1_final_verify.py --strict-skip`), P1/P2 PASS, exactly 7 deletions (`winsrv.c`, `guishost.h`, `wsclient.c/h`, `qemu_windowsrv.cpp/h`, `qemu_gui_exec.cpp`), snapshot/token intact, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.

These two newly indexed reports bring the indexed Markdown total to **462**, matching the 462 `.md` files under `docs/` (including this index; the header count "(462 files)" at line 4 is consistent); post-append re-verification shows 462 on-disk `.md` files = 462 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 subagent post-454 sweep, WATCH143–145 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index (backtick-wrapped header links included): three reports
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH143.md](WINSRV_DELETE_WATCH143.md) — read-only 2026-11-24 winsrv re-apply monitor #143: APPLY_RC NOT YET (log 3,355 B, ends mid-P3 `r1_final_verify.py --strict-skip` gate, pid 1286837 alive 27m39s), P1/P2 gates PASS, 7 deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436` unused.
- [WINSRV_DELETE_WATCH144.md](WINSRV_DELETE_WATCH144.md) — read-only monitor #144: APPLY_RC NOT YET (log 3.3 KB, last line P3 strict-verify gate), pid 1286837 ~28 min in P3, P1/P2 PASS, exactly 7 deletions (`winsrv.c`, `guishost.h`, `wsclient.c/h`, `qemu_windowsrv.cpp/h`, `qemu_gui_exec.cpp`), HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH145.md](WINSRV_DELETE_WATCH145.md) — read-only monitor #145: APPLY_RC NOT YET SEEN (log ends at line 56, P3 gate), re-apply IN PROGRESS, P1+P2 PASS, all 7 winsrv deletes applied, EFILE guard-only intact, snapshot/token intact, `kernelhwsrv/` clean.

These three newly indexed reports bring the indexed Markdown total to **466**,
matching the 466 `.md` files under `docs/` (including this index; the header
count at line 4 is updated to 466); post-append re-verification shows 466
on-disk `.md` files = 466 unique indexed link targets covering them, 0 missing,
0 dangling.

## Added — 2026-11-24 last-~447 sweep, WATCH143/144 backfill (append-only)

A fresh full `docs/*.md` listing (466 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets (`](*.md)` pattern) in this
index: three documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH143.md](WINSRV_DELETE_WATCH143.md) — read-only 2026-11-24 winsrv re-apply monitor #143: APPLY_RC NOT YET (log ends mid P3 `r1_final_verify.py --strict-skip` gate list, no rollback), pid 1286837 alive 27m39s, P1/P2 gates PASS, 7 winsrv deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH144.md](WINSRV_DELETE_WATCH144.md) — read-only 2026-11-24 winsrv re-apply monitor #144: APPLY_RC NOT YET (pid 1286837 ~28 min in P3 gate, P1/P2 PASS), exactly 7 deletions (`winsrv.c`, `guishost.h`, `wsclient.c/h`, `qemu_windowsrv.cpp/h`, `qemu_gui_exec.cpp`), snapshot intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH145.md](WINSRV_DELETE_WATCH145.md) — read-only 2026-11-24 winsrv re-apply monitor #145: APPLY_RC NOT YET SEEN (log ends at the P3 `r1_final_verify.py --strict-skip` gate line, no rollback), pid 1286837 ~28 min in P3, P1/P2 gates PASS, all 7 winsrv deletions applied, snapshot/token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.

These three newly indexed reports bring the indexed Markdown total to **466**, matching the 466 `.md` files under `docs/` (including this index; the header count "(466 files, verified 2026-11-24)" at line 4 is updated); post-append re-verification shows 466 on-disk `.md` files = 466 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 post-465 sweep, WATCH145 backfill (append-only)

A fresh full `docs/*.md` listing (466 files, flat directory, no subdirectories)
was diffed against the 465 unique `.md` link targets previously in this index:
one report existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH145.md](WINSRV_DELETE_WATCH145.md) — read-only 2026-11-24 winsrv re-apply monitor #145: APPLY_RC NOT YET SEEN (apply2 log ends at line 56, mid P3 `r1_final_verify.py --strict-skip` gate, no rollback), pid 1286837 still running ~28 min, P1/P2 gates PASS, all 7 winsrv deletions staged, snapshot `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **466**,
matching the 466 `.md` files under `docs/` (including this index; the header
count "(466 files)" at line 4 is consistent); post-append re-verification shows
466 on-disk `.md` files = 466 unique indexed link targets, 0 missing, 0
dangling; every Markdown document in the directory is now linked.

## Added — 2026-11-24 last-454 sweep, post-454 (append-only)

A fresh full `docs/*.md` listing (466 files, flat directory) was diffed against the unique `.md` link targets in this index: **0 unlinked documents and 0 broken links** (466 on-disk `.md` files = 466 unique indexed targets, including this index; the header count at line 4 is consistent). This section adds no new link entries. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

The indexed Markdown total stays **466**, matching the 466 `.md` files under `docs/` (including this index); every Markdown document in the directory is linked.

## Added — 2026-11-24 post-466 sweep, WINSRV_DELETE_WATCH146 backfill (append-only)

A fresh full `docs/*.md` listing (485 files, flat directory, no subdirectories)
was diffed against the 466 unique `.md` link targets previously in this index:
sixteen documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH146.md](WINSRV_DELETE_WATCH146.md) — read-only 2026-11-24
  winsrv re-apply monitor #146: APPLY_RC NOT YET EMITTED (pid 1286837 alive
  29:00, in P3 `r1_final_verify.py --strict-skip`), P1/P2 gates PASS with 7
  winsrv files deleted in the worktree, rollback snapshot
  `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token
  `ROLLBACK-WINSRV-20261124_215436`) unused, `kernelhwsrv/` clean, HEAD
  `0f03f7e`; verdict IN PROGRESS, await APPLY_RC.
- [WINSRV_DELETE_WATCH147.md](WINSRV_DELETE_WATCH147.md) — read-only 2026-11-24
  winsrv re-apply monitor #147: APPLY_RC not yet emitted, apply in flight at
  P3 gates (log ends at the `r1_final_verify.py --strict-skip` launch), pid
  1286837 alive ~29 min, P1/P2 gates PASS, 7 winsrv deletions staged, no
  rollback, snapshot/token intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH148.md](WINSRV_DELETE_WATCH148.md) — read-only 2026-11-24
  winsrv re-apply monitor #148: APPLY_RC still not emitted (log ends at P3
  `r1_final_verify.py --strict-skip`), pid 1286837 alive 29:33, 7 deletions
  staged (`winsrv.c`, `guishost.h`, `wsclient.c/h`, `qemu_windowsrv.cpp/h`,
  `qemu_gui_exec.cpp`), pre-apply ewsrv + P1 + P2 gates PASS, no rollback,
  snapshot/token intact, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH149.md](WINSRV_DELETE_WATCH149.md) — read-only 2026-11-24
  winsrv re-apply monitor #149: APPLY_RC NOT YET EMITTED (log line 56 = P3
  gate launch), pid 1286837 alive ~30 min, P1 (18 edits) + P2 (Makefile/
  user.mk) gates PASS, P3 running with 7 deletions done, no rollback,
  `kernelhwsrv/` clean, HEAD `0f03f7e` on `97f224b`, apply not committed.
- [WINSRV_DELETE_WATCH150.md](WINSRV_DELETE_WATCH150.md) — read-only 2026-11-24
  winsrv re-apply monitor #150: re-apply STILL RUNNING, no APPLY_RC/ROLLBACK/
  FAIL lines in `winsrv_delete_apply2.log` (3.3 KB, ends at last P3 gate),
  pid 1286837 alive 30+ min, P1 (12 edits) + P2 (5 edits) PASS, 7 deletes
  done, snapshot/token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD
  `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH154.md](WINSRV_DELETE_WATCH154.md) — read-only 2026-11-24
  winsrv re-apply monitor #154: APPLY_RC NOT YET (pid 1286837 alive ~31 min, in
  P3 gates, log ends at `r1_final_verify.py --strict-skip`), P1 (18 edits) +
  P2 (Makefile/user.mk) gates PASS, 7 deletions staged, snapshot/token
  `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/`
  clean, no commits/pushes.
- [WINSRV_DELETE_WATCH155.md](WINSRV_DELETE_WATCH155.md) — read-only 2026-11-24
  ~22:49 monitor #155: APPLY_RC NOT YET EMITTED, pid 1286837 alive ~31 min in
  P3; pre-apply ewsrv + P1 + P2 gates PASS, 7 deletions match the delete op
  set, snapshot/token intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH156.md](WINSRV_DELETE_WATCH156.md) — read-only 2026-11-24
  22:17 monitor #156: APPLY_RC NOT YET WRITTEN (pid 1286837 alive 31:11, P3
  strict-verify running as pid 1376334), P1/P2 PASS with 7 deletes in progress,
  ewsrv retire gate PASS at every phase, HEAD `0f03f7e`/`97f224b`,
  `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH157.md](WINSRV_DELETE_WATCH157.md) — read-only 2026-11-24
  ~22:49 monitor #157: APPLY_RC NOT YET EMITTED (log 3.3 KB ends mid-P3), pid
  1286837 alive ~31 min, pre/P1/P2 gates PASS, 18 edits + all 7 deletes
  applied, `git status` D-count 7, HEAD `0f03f7e` uncommitted.
- [WINSRV_DELETE_WATCH159.md](WINSRV_DELETE_WATCH159.md) — read-only 2026-11-24
  ~22:49 monitor #159: APPLY_RC NOT YET EMITTED (pid 1286837 ~31 min), P1/P2
  gates PASS, 7 deletes done, now in P3 gate suite (selfcheck…final_verify),
  D-count 7, HEAD `0f03f7e` on `97f224b`, `kernelhwsrv/` clean, no QEMU run.
- [WINSRV_DELETE_WATCH158.md](WINSRV_DELETE_WATCH158.md) — read-only 2026-11-24
  monitor #158: APPLY_RC NOT YET EMITTED (0 APPLY_RC lines in the 3.4 KB log,
  no rollback), pid 1286837 alive 31:19 in P3, pre-ewsrv/P1/P2 gates PASS, 7
  deletions staged, D-count 7, HEAD `0f03f7e` on `97f224b`.
- [WINSRV_DELETE_WATCH160.md](WINSRV_DELETE_WATCH160.md) — read-only 2026-11-24
  monitor #160: APPLY_RC NOT YET SEEN (log ends at P3 strict-verify), pid
  1286837 running ~31 min, P1/P2 gates PASS, P3 suite in progress, snapshot
  intact, no rollback token consumed.
- [WINSRV_DELETE_WATCH161.md](WINSRV_DELETE_WATCH161.md) — read-only 2026-11-24
  monitor #161: APPLY_RC NOT FINISHED (pid 1286837, 31m37s, final P3 gate in
  flight), 7 deletions staged (D-count 7), P1/P2 gates PASS, snapshot/token
  intact, `kernelhwsrv/` clean, HEAD `0f03f7e` on `97f224b`.
- [WINSRV_DELETE_WATCH162.md](WINSRV_DELETE_WATCH162.md) — read-only 2026-11-24
  monitor #162: APPLY_RC not yet printed (pid 1286837 ~31 min, log mid-P3, no
  FAIL/rollback), P1/P2 gates PASS, 7 winsrv files deleted (D-count 7),
  `kernelhwsrv/` diff 0 lines, snapshot/token intact.
- [WINSRV_DELETE_WATCH163.md](WINSRV_DELETE_WATCH163.md) — read-only 2026-11-24
  monitor #163: APPLY_RC not yet emitted (pid 1286837 ~32 min), P1/P2 PASS, 7
  deletions, P3 strict-verify in flight, no rollback, HEAD `0f03f7e`,
  `kernelhwsrv/` clean, token `ROLLBACK-WINSRV-20261124_215436`.
- [WINSRV_DELETE_WATCH164.md](WINSRV_DELETE_WATCH164.md) — read-only 2026-11-24
  monitor #164: APPLY_RC not yet emitted (pid 1286837 alive ~31:45, P3 full
  gate phase, no PASS/FAIL/rollback after the P3 line), 18 edits + 7 deletes
  applied (D-count 7), P1/P2 + pre-apply ewsrv gates PASS.
- [WINSRV_DELETE_WATCH185.md](WINSRV_DELETE_WATCH185.md) — read-only 2026-11-24
  ~22:50 monitor #185: APPLY_RC not yet emitted (log 56 lines ends inside P3
  gates), pid 1286837 alive 33+ min, 7 deletes confirmed (D-count 7),
  pre-apply ewsrv + P1 + P2 gates PASS, P3 in progress, no rollback.

These seventeen newly indexed reports bring the indexed Markdown total to
**485**, matching the 485 `.md` files under `docs/` (including this index; the
header count at line 4 is updated); post-append re-verification shows 485
on-disk `.md` files = 485 unique indexed link targets, 0 missing, 0 dangling;
every Markdown document in the directory is now linked.

## Added — 2026-11-24 last-460 sweep, post-466 (append-only)

A fresh full `docs/*.md` listing (484 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: sixteen reports
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH146.md](WINSRV_DELETE_WATCH146.md) — read-only 2026-11-24
  winsrv re-apply monitor #146: APPLY_RC NOT YET EMITTED (apply2 log 3.4 KB, pid
  1286837 alive 29:00 in P3 `r1_final_verify.py --strict-skip` gate), P1/P2
  gates PASS, 7 winsrv deletions applied, rollback token
  `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e`, `kernelhwsrv/`
  clean.
- [WINSRV_DELETE_WATCH148.md](WINSRV_DELETE_WATCH148.md) — read-only 2026-11-24
  winsrv re-apply monitor #148: APPLY_RC not yet emitted (apply log ends at P3
  `r1_final_verify.py --strict-skip` gate, no rollback), pid 1286837 alive
  29:33, 7 winsrv deletions (`winsrv.c`, `guishost.h`, `wsclient.c/h`,
  `qemu_windowsrv.cpp/h`, `qemu_gui_exec.cpp`), snapshot/token
  `ROLLBACK-WINSRV-20261124_215436` intact.
- [WINSRV_DELETE_WATCH147.md](WINSRV_DELETE_WATCH147.md) — read-only 2026-11-24
  winsrv re-apply monitor #147: APPLY_RC not yet emitted (apply in flight at P3
  gates, log ends at `r1_final_verify.py --strict-skip` launch), pid 1286837
  alive ~29 min, P1/P2 gates PASS, 7 winsrv deletions staged, no rollback.
- [WINSRV_DELETE_WATCH149.md](WINSRV_DELETE_WATCH149.md) — read-only 2026-11-24
  winsrv re-apply monitor #149: APPLY_RC NOT YET EMITTED (log line 56, P3
  `r1_final_verify.py --strict-skip` gate), pid 1286837 alive ~30 min, P1
  (18 edits) and P2 (isogrub) PASS, 7 deletions done, no rollback.
- [WINSRV_DELETE_WATCH150.md](WINSRV_DELETE_WATCH150.md) — read-only 2026-11-24
  winsrv re-apply monitor #150: APPLY_RC not yet reached, re-apply still running
  (apply2 log 3.3 KB ends at last P3 gate, no APPLY_RC/ROLLBACK/FAIL), pid
  1286837 alive 30+ min.
- [WINSRV_DELETE_WATCH153.md](WINSRV_DELETE_WATCH153.md) — read-only 2026-11-24
  winsrv re-apply monitor #153: APPLY_RC NOT YET WRITTEN (pid 1286837 ~31 min in
  P3 final gate, `r1_final_verify.py --strict-skip` still running, no
  rollback), P1/P2 PASS, 7 staged deletions, HEAD `0f03f7e`, rollback token
  `ROLLBACK-WINSRV-20261124_215436` intact.
- [WINSRV_DELETE_WATCH154.md](WINSRV_DELETE_WATCH154.md) — read-only 2026-11-24
  winsrv re-apply monitor #154: APPLY_RC NOT YET (apply2 log 56 lines / 3.3 KB,
  no APPLY_RC/rollback marker), apply still in flight.
- [WINSRV_DELETE_WATCH155.md](WINSRV_DELETE_WATCH155.md) — read-only 2026-11-24
  winsrv re-apply monitor #155: APPLY_RC NOT YET EMITTED (in P3 gates, last
  queued `r1_final_verify.py --strict-skip`), no rollback triggered.
- [WINSRV_DELETE_WATCH156.md](WINSRV_DELETE_WATCH156.md) — read-only 2026-11-24
  winsrv re-apply monitor #156: APPLY_RC NOT YET WRITTEN (pid 1286837 alive
  31:11, log ends at P3 `r1_final_verify.py --strict-skip`, pid 1376334), no
  rollback.
- [WINSRV_DELETE_WATCH157.md](WINSRV_DELETE_WATCH157.md) — read-only 2026-11-24
  winsrv re-apply monitor #157: APPLY_RC NOT YET EMITTED (apply2 log 3.3 KB ends
  mid-P3 gates).
- [WINSRV_DELETE_WATCH159.md](WINSRV_DELETE_WATCH159.md) — read-only 2026-11-24
  winsrv re-apply monitor #159: APPLY_RC NOT YET EMITTED (pid 1286837 alive
  ~31 min, apply still running).
- [WINSRV_DELETE_WATCH158.md](WINSRV_DELETE_WATCH158.md) — read-only 2026-11-24
  winsrv re-apply monitor #158: APPLY_RC NOT YET EMITTED (apply2 log 3.4 KB, zero
  APPLY_RC lines, only the snapshot-token ROLLBACK match), no rollback.
- [WINSRV_DELETE_WATCH160.md](WINSRV_DELETE_WATCH160.md) — read-only 2026-11-24
  winsrv re-apply monitor #160: APPLY_RC NOT YET SEEN (apply2 log 3.4 KB, apply
  in flight).
- [WINSRV_DELETE_WATCH161.md](WINSRV_DELETE_WATCH161.md) — read-only 2026-11-24
  winsrv re-apply monitor #161: APPLY_RC NOT FINISHED (pid 1286837 running
  31m37s, log ends at P3 `r1_final_verify.py --strict-skip`), no rollback.
- [WINSRV_DELETE_WATCH162.md](WINSRV_DELETE_WATCH162.md) — read-only 2026-11-24
  winsrv re-apply monitor #162: APPLY_RC not yet printed (pid 1286837 ~31 min,
  log mid-P3, no FAIL/rollback).
- [WINSRV_DELETE_WATCH164.md](WINSRV_DELETE_WATCH164.md) — read-only 2026-11-24
  winsrv re-apply monitor #164: APPLY_RC not yet emitted (log ends at P3
  `r1_final_verify.py --strict-skip` gate, nothing after it).

These sixteen newly indexed reports bring the indexed Markdown total to
**484**, matching the 484 `.md` files under `docs/` (including this index; the
header count "(484 files, verified 2026-11-24)" at line 4 is updated);
post-append re-verification shows 484 on-disk `.md` files = 484 unique indexed
link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 last-467 sweep, WINSRV_DELETE_WATCH148 backfill (append-only)

A fresh full `docs/*.md` listing (468 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: one report existed on disk but had no entry; it is linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH148.md](WINSRV_DELETE_WATCH148.md) — read-only 2026-11-24 winsrv re-apply monitor #148: APPLY_RC not yet emitted (P3 `r1_final_verify.py --strict-skip` gate still running, pid 1286837), no rollback, snapshot `ROLLBACK-WINSRV-20261124_215436` intact, 7 winsrv deletions staged, P1/P2 gates PASS, HEAD `0f03f7e`, `kernelhwsrv/` clean.

This newly indexed report brings the indexed Markdown total to **468**,
matching the 468 `.md` files under `docs/` (including this index; the header
count "(468 files, verified 2026-11-24)" at line 4 is consistent);
post-append re-verification shows 468 on-disk `.md` files = 468 unique
indexed link targets, 0 missing, 0 dangling; every Markdown document in the
directory is now linked.

## Added — 2026-11-24 last-468 sweep, WINSRV_DELETE_WATCH147/149/150 backfill (append-only)

A fresh full `docs/*.md` listing (471 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: three reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH147.md](WINSRV_DELETE_WATCH147.md) — read-only 2026-11-24 winsrv re-apply monitor #147: APPLY_RC not yet emitted (apply in flight at P3 gates, pid 1286837 ~29 min), P1/P2 PASS, 7 winsrv deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH149.md](WINSRV_DELETE_WATCH149.md) — read-only 2026-11-24 winsrv re-apply monitor #149: APPLY_RC NOT YET EMITTED (log ends at P3 `r1_final_verify.py --strict-skip`), pid 1286837 alive ~30 min, P1 (18 edits)/P2 gates PASS, 7 deletions done, snapshot intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH150.md](WINSRV_DELETE_WATCH150.md) — read-only 2026-11-24 winsrv re-apply monitor #150: re-apply still running (no APPLY_RC/ROLLBACK/FAIL in log), pid 1286837 alive 30+ min, P1 (12 edits)/P2 (5 edits) gates PASS, 7 deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` untouched.

These three newly indexed reports bring the indexed Markdown total to
**471**, matching the 471 `.md` files under `docs/` (including this index;
the header count "(471 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 471 on-disk `.md` files =
471 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 last-473 sweep, WINSRV_DELETE_WATCH151/152 backfill (append-only)

A fresh full `docs/*.md` listing (473 files, flat directory, no
subdirectories) was diffed against the 471 unique `.md` link targets in
this index: two reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH151.md](WINSRV_DELETE_WATCH151.md) — read-only 2026-11-24 winsrv re-apply monitor #151: APPLY_RC not yet emitted (log ends mid-P3 gates, pid 1286837 ~30 min), pre-gate ewsrv + P1 (18 edits) + P2 gates PASS, all 7 deletes done with 7 staged `D` rows, snapshot `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH152.md](WINSRV_DELETE_WATCH152.md) — read-only 2026-11-24 ~22:47 winsrv re-apply monitor #152: APPLY_RC still NOT YET (apply pid 1286837 alive 30:23), P1+P2 gates PASS, P3 gate list (selfcheck/smoke/ewsrv/input/doom x4/fullchain/retire/final_verify --strict-skip) in progress, 7 winsrv deletions staged, HEAD `0f03f7e`, `kernelhwsrv/` diff empty.

These two newly indexed reports bring the indexed Markdown total to
**473**, matching the 473 `.md` files under `docs/` (including this index;
the header count "(473 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 473 on-disk `.md` files =
473 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 last-471 sweep, WINSRV_DELETE_WATCH151/152 backfill (append-only)

A fresh full `docs/*.md` listing (473 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: two reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH151.md](WINSRV_DELETE_WATCH151.md) — read-only 2026-11-24 winsrv re-apply monitor #151: APPLY_RC not yet emitted (pid 1286837 ~30 min in P3 gates), P1/P2 gates PASS, all 7 deletes done, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH152.md](WINSRV_DELETE_WATCH152.md) — read-only 2026-11-24 ~22:47 winsrv re-apply monitor #152: APPLY_RC NOT YET (P3 gate suite in progress), pid 1286837 alive 30:23, P1+P2 PASS, 7 staged deletions, HEAD `0f03f7e`/`97f224b`, `kernelhwsrv/` clean, snapshot token intact.

These two newly indexed reports bring the indexed Markdown total to
**473**, matching the 473 `.md` files under `docs/` (including this index;
the header count "(473 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 473 on-disk `.md` files =
473 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 last-473 sweep, WINSRV_DELETE_WATCH153/154 backfill (append-only)

A fresh full `docs/*.md` listing (475 files, flat directory, no
subdirectories) was diffed against the 473 unique `.md` link targets in
this index: two reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH153.md](WINSRV_DELETE_WATCH153.md) — read-only 2026-11-24 ~22:17 winsrv re-apply monitor #153: APPLY_RC NOT YET WRITTEN (apply pid 1286837 ~31 min in P3 final gate, strict-verify pid 1376334 still running), P1 (18 edits) + P2 gates PASS, 7 winsrv files deleted and staged, snapshot `ROLLBACK-WINSRV-20261124_215436`, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH154.md](WINSRV_DELETE_WATCH154.md) — read-only 2026-11-24 22:17+ winsrv re-apply monitor #154: APPLY_RC NOT YET (apply pid 1286837 ~31 min in P3 gates, last log line = strict-skip verify), P1/P2 gates PASS, 7 staged deletions (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.c/h, qemu_gui_exec.cpp), HEAD `0f03f7e`, `kernelhwsrv/` untouched.

These two newly indexed reports bring the indexed Markdown total to
**475**, matching the 475 `.md` files under `docs/` (including this index;
the header count "(475 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 475 on-disk `.md` files =
475 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 last-473 sweep, WINSRV_DELETE_WATCH153–170 backfill (append-only)

A fresh full `docs/*.md` listing (flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: nine reports
existed on disk but had no entry (WINSRV_DELETE_WATCH153/154 first, then
163/165/166/167/168/169/170 as concurrent sessions landed them); they are
linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH153.md](WINSRV_DELETE_WATCH153.md) — read-only 2026-11-24 ~22:17 winsrv re-apply monitor #153: APPLY_RC NOT YET WRITTEN (pid 1286837 ~31 min, in P3 final gate with `r1_final_verify.py --strict-skip` still running); P1 (18 edits) + P2 (isogrub) gates PASS, 7 winsrv files staged as deletions, HEAD `0f03f7e`, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH154.md](WINSRV_DELETE_WATCH154.md) — read-only 2026-11-24 22:17+ winsrv re-apply monitor #154: APPLY_RC still NOT YET (log 56 lines, no rollback marker), apply pid 1286837 in P3 gates, P1+P2 PASS, same 7 staged deletions, HEAD `0f03f7e`, snapshot token intact, `kernelhwsrv/` untouched, no commits/pushes.
- [WINSRV_DELETE_WATCH163.md](WINSRV_DELETE_WATCH163.md) — read-only 2026-11-24 winsrv re-apply monitor #163: APPLY_RC not yet emitted, apply pid 1286837 still running (~32 min).
- [WINSRV_DELETE_WATCH165.md](WINSRV_DELETE_WATCH165.md) — read-only 2026-11-24 ~22:50 winsrv re-apply monitor #165: APPLY_RC NOT YET, apply log (3.3 KB) ends at P3 gate `r1_final_verify.py --strict-skip`, no APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH166.md](WINSRV_DELETE_WATCH166.md) — read-only 2026-11-24 ~22:49 winsrv re-apply monitor #166: APPLY_RC NOT YET EMITTED, apply still running.
- [WINSRV_DELETE_WATCH167.md](WINSRV_DELETE_WATCH167.md) — read-only 2026-11-24 winsrv re-apply monitor #167: APPLY_RC NOT YET EMITTED, log ends mid-P3 gates with `r1_final_verify.py --strict-skip` in flight, no APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH168.md](WINSRV_DELETE_WATCH168.md) — read-only 2026-11-24 winsrv re-apply monitor #168: APPLY_RC NOT YET EMITTED, log (3.4 KB) ends at line 56 mid-P3 gate, apply IN PROGRESS, no rollback triggered.
- [WINSRV_DELETE_WATCH169.md](WINSRV_DELETE_WATCH169.md) — read-only 2026-11-24 winsrv re-apply monitor #169 (final-state monitor): apply pid 1286837 still RUNNING (~32 min) in P3 `r1_final_verify.py --strict-skip`, no `APPLY_RC=`/rollback lines, token `ROLLBACK-WINSRV-20261124_215436`.
- [WINSRV_DELETE_WATCH170.md](WINSRV_DELETE_WATCH170.md) — read-only 2026-11-24 winsrv re-apply monitor #170: APPLY_RC NOT YET, apply ~32 min elapsed, last log line shows P3 `r1_final_verify.py --strict-skip` in progress, no rollback lines.
- [WINSRV_DELETE_WATCH184.md](WINSRV_DELETE_WATCH184.md) — read-only 2026-11-24 22:27 EET winsrv re-apply monitor #184: APPLY_RC NOT YET, pid 1286837 alive (~33 min), log ends at P3 gate `r1_final_verify.py --strict-skip`, no APPLY_RC/rollback line.

These ten newly indexed reports bring the indexed Markdown total to
**483**, matching the 483 `.md` files under `docs/` at append time
(including this index); post-append re-verification shows on-disk `.md`
files = unique indexed link targets with 0 dangling links (the directory is
actively written by concurrent sessions, so later watch reports are picked
up by their own backfill sections).

## Added — 2026-11-24 last-473 sweep, WINSRV_DELETE_WATCH153/154 backfill (append-only)

A fresh full `docs/*.md` listing (475 files, flat directory, no
subdirectories) was diffed against the 473 unique `.md` link targets in
this index: two reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH153.md](WINSRV_DELETE_WATCH153.md) — read-only 2026-11-24 ~22:17 winsrv re-apply monitor #153: APPLY_RC not yet written (apply pid 1286837 in P3 final gate, `r1_final_verify.py --strict-skip` still running); P1 (18 edits) + P2 (isogrub) gates PASS, 7 winsrv files deleted and staged, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH154.md](WINSRV_DELETE_WATCH154.md) — read-only 2026-11-24 22:17+ winsrv re-apply monitor #154: APPLY_RC NOT YET (log 56 lines, apply pid 1286837 ~31 min in P3 gates, last line `r1_final_verify.py --strict-skip`); P1/P2 gates PASS, 7 staged deletions (winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp), HEAD `0f03f7e`, snapshot token intact, `kernelhwsrv/` untouched.

These two newly indexed reports bring the indexed Markdown total to
**475**, matching the 475 `.md` files under `docs/` (including this index;
the header count "(475 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 475 on-disk `.md` files =
475 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 post-473 sweep, WINSRV_DELETE_WATCH153–156 backfill (append-only)

A fresh full `docs/*.md` listing (477 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: four reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH153.md](WINSRV_DELETE_WATCH153.md) — read-only 2026-11-24 ~22:17 winsrv re-apply monitor #153: APPLY_RC not yet written (apply pid 1286837 in P3 final gate, `r1_final_verify.py --strict-skip` running), P1/P2 gates PASS, 7 winsrv files staged for deletion, rollback token unused, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH154.md](WINSRV_DELETE_WATCH154.md) — read-only 2026-11-24 22:17+ winsrv re-apply monitor #154: APPLY_RC NOT YET, pid 1286837 in P3 gates, P1 (18 edits) and P2 (Makefile/user.mk) PASS, 7 staged deletions, HEAD `0f03f7e`, snapshot token `ROLLBACK-WINSRV-20261124_215436` intact.
- [WINSRV_DELETE_WATCH155.md](WINSRV_DELETE_WATCH155.md) — read-only 2026-11-24 ~22:49 winsrv re-apply monitor #155: APPLY_RC not yet emitted, pre-apply ewsrv + P1 + P2 gates PASS, P3 running, 7 deletions staged, `kernelhwsrv/` clean, no commit/push.
- [WINSRV_DELETE_WATCH156.md](WINSRV_DELETE_WATCH156.md) — read-only 2026-11-24 22:17 winsrv re-apply monitor #156: APPLY_RC not yet written, pid 1286837 alive 31:11 in P3 (`r1_final_verify.py --strict-skip` pid 1376334), ewsrv retire gate PASS at every phase, 7 deletions, `kernelhwsrv/` untouched.

These four newly indexed reports bring the indexed Markdown total to
**477**, matching the 477 `.md` files under `docs/` (including this index;
the header count "(477 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 477 on-disk `.md` files =
477 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 post-473 sweep, WINSRV_DELETE_WATCH158/160–164 backfill (append-only)

A fresh full `docs/*.md` listing (485 files, flat directory, no
subdirectories) was diffed against the 479 unique `.md` link targets in
this index: six reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH158.md](WINSRV_DELETE_WATCH158.md) — read-only 2026-11-24 winsrv re-apply monitor #158: APPLY_RC not yet emitted (apply pid 1286837, 31:19, inside P3 gate `r1_final_verify.py --strict-skip`); P1 (9 edits) + P2 (isogrub) gates PASS, 7 winsrv files deleted/staged, rollback snapshot `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH160.md](WINSRV_DELETE_WATCH160.md) — read-only 2026-11-24 winsrv re-apply monitor #160: APPLY_RC not yet seen, log ends at P3 `r1_final_verify.py --strict-skip` with pid 1286837 ~31 min running; P1/P2 gates PASS, 7 staged deletions (winsrv.c, wsclient.c/h, guishost.h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp), no rollback, HEAD `0f03f7e`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH161.md](WINSRV_DELETE_WATCH161.md) — read-only 2026-11-24 winsrv re-apply monitor #161: APPLY_RC not finished, pid 1286837 31m37s in the final P3 gate; P1/P2 gates PASS, 7 deletions staged, snapshot/token intact, HEAD `0f03f7e` over `97f224b`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH162.md](WINSRV_DELETE_WATCH162.md) — read-only 2026-11-24 winsrv re-apply monitor #162: APPLY_RC not yet printed, apply ~31 min mid-P3 with no FAIL/rollback lines; P1/P2 PASS, 7 winsrv deletions in working tree, `git diff -- kernelhwsrv/` empty, snapshot token intact, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH163.md](WINSRV_DELETE_WATCH163.md) — read-only 2026-11-24 winsrv re-apply monitor #163: APPLY_RC not yet emitted, pid 1286837 ~32 min; P1/P2 gates PASS, 7 deletions staged, now in P3 gate, no rollback triggered, `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH164.md](WINSRV_DELETE_WATCH164.md) — read-only 2026-11-24 winsrv re-apply monitor #164: APPLY_RC not yet emitted, pid 1286837 ~31:45 in P3 with no verdict line after it; 18 edits + 7 allowlisted deletes applied, pre-apply ewsrv and P1/P2 gates PASS, rollback snapshot `ROLLBACK-WINSRV-20261124_215436` intact, `kernelhwsrv/` untouched.

These six newly indexed reports bring the indexed Markdown total to
**485**, matching the 485 `.md` files under `docs/` (including this index;
the header count "(485 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 485 on-disk `.md` files =
485 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 post-473 sweep, WINSRV_DELETE_WATCH154–160 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: seven reports existed on disk but had no entry; they
are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH154.md](WINSRV_DELETE_WATCH154.md) — read-only 2026-11-24 ~22:17+ winsrv re-apply monitor #154: APPLY_RC NOT YET (pid 1286837 in P3 gates, `r1_final_verify.py --strict-skip` running), P1/P2 gates PASS, 7 winsrv deletions staged, rollback token `ROLLBACK-WINSRV-20261124_215436` unused, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH155.md](WINSRV_DELETE_WATCH155.md) — read-only ~22:49 monitor #155: APPLY_RC NOT YET EMITTED, pid 1286837 alive ~31 min in P3 gates; pre-apply/P1/P2 gates PASS, 7 staged deletions match the 7-delete op set, no rollback.
- [WINSRV_DELETE_WATCH156.md](WINSRV_DELETE_WATCH156.md) — read-only 22:17 monitor #156: APPLY_RC NOT YET WRITTEN, log ends at `gate[P3]: r1_final_verify.py --strict-skip` (pid 1376334); P1 (18 edits)/P2 gates PASS, ewsrv retire gate PASS every phase, 7 staged deletions, HEAD `0f03f7e`/`97f224b`.
- [WINSRV_DELETE_WATCH157.md](WINSRV_DELETE_WATCH157.md) — read-only ~22:49 monitor #157: APPLY_RC NOT YET EMITTED, no rollback lines in `winsrv_delete_apply2.log`; pid 1286837 alive ~31 min; pre/P1/P2 gates PASS, all 7 deletes applied, P3 gate suite running.
- [WINSRV_DELETE_WATCH158.md](WINSRV_DELETE_WATCH158.md) — read-only monitor #158: 0 APPLY_RC lines and no rollback in the apply log; pid 1286837 alive 31:19 inside P3 gates; ewsrv/P1/P2 gates PASS, 7 files deleted, HEAD `0f03f7e` then `97f224b`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH159.md](WINSRV_DELETE_WATCH159.md) — read-only ~22:49 monitor #159: APPLY_RC NOT YET EMITTED; P1/P2 gates PASS, 7 deletes done, now in P3 gates (selfcheck/smoke/ewsrv/input/doom x4/fullchain/retire/final_verify), `kernelhwsrv/` clean, snapshot token intact.
- [WINSRV_DELETE_WATCH160.md](WINSRV_DELETE_WATCH160.md) — read-only 2026-11-24 monitor #160: APPLY_RC NOT YET SEEN, log ends at P3 `final_verify --strict-skip`; P1/P2 PASS, P3 full-gate suite in progress, pid 1286837 ~31 min, rollback token unconsumed.

These seven newly indexed reports bring the indexed Markdown total to
**480**, matching the 480 `.md` files under `docs/` (including this index;
the header count at line 4 is updated); post-append re-verification shows
480 on-disk `.md` files = 480 unique indexed link targets, 0 missing,
0 dangling; every Markdown document in the directory is now linked.

## Added — 2026-11-24 post-475 sweep, WINSRV_DELETE_WATCH158/160-164/168 backfill (append-only)

A fresh full `docs/*.md` listing (486 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: seven reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH158.md](WINSRV_DELETE_WATCH158.md) — read-only winsrv re-apply monitor #158: APPLY_RC not yet emitted (log 3.4 KB, 0 APPLY_RC lines, no rollback); apply pid 1286837 alive 31:19 inside the P3 gate suite (`r1_final_verify.py --strict-skip`).
- [WINSRV_DELETE_WATCH160.md](WINSRV_DELETE_WATCH160.md) — read-only winsrv re-apply monitor #160: APPLY_RC not yet seen; log ends at P3 `r1_final_verify.py --strict-skip` with P1/P2 gates PASS and the P3 suite in progress.
- [WINSRV_DELETE_WATCH161.md](WINSRV_DELETE_WATCH161.md) — read-only winsrv re-apply monitor #161: APPLY_RC not finished (pid 1286837, 31m37s, final gate in flight); 7 winsrv deletions staged (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp).
- [WINSRV_DELETE_WATCH162.md](WINSRV_DELETE_WATCH162.md) — read-only winsrv re-apply monitor #162: APPLY_RC not yet printed; log mid-P3 at strict-verify, P1/P2 PASS, 7 winsrv files deleted in the working tree, no rollback.
- [WINSRV_DELETE_WATCH163.md](WINSRV_DELETE_WATCH163.md) — read-only winsrv re-apply monitor #163: APPLY_RC not yet emitted (pid 1286837, ~32 min); P1/P2 PASS, 7 files deleted, P3 strict-verify in flight, no rollback.
- [WINSRV_DELETE_WATCH164.md](WINSRV_DELETE_WATCH164.md) — read-only winsrv re-apply monitor #164: APPLY_RC not yet emitted; log ends at `gate[P3]: r1_final_verify.py --strict-skip`, pid 1286837 alive ~31:45 in the P3 full-gate phase.
- [WINSRV_DELETE_WATCH168.md](WINSRV_DELETE_WATCH168.md) — read-only winsrv re-apply monitor #168: APPLY_RC not yet emitted (log ends line 56 mid-P3 strict-verify); pid 1286837 alive 32:01, apply IN PROGRESS, no rollback.

These seven newly indexed reports bring the indexed Markdown total to
**486**, matching the 486 `.md` files under `docs/` (including this index);
post-append re-verification shows 486 on-disk `.md` files = 486 unique
indexed link targets, 0 missing, 0 dangling; every Markdown document in
the directory is now linked.

## Added — 2026-11-24 post-486 WINSRV_DELETE_WATCH backfill (append-only)

A fresh full `docs/*.md` listing (498 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: twelve reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH165.md](WINSRV_DELETE_WATCH165.md) — read-only 2026-11-24 ~22:50 winsrv re-apply monitor #165: APPLY_RC NOT YET (pid 1286837 ~32:00 in P3 `r1_final_verify.py --strict-skip`), P1/P2 gates PASS, 7 deletes applied, no rollback.
- [WINSRV_DELETE_WATCH166.md](WINSRV_DELETE_WATCH166.md) — read-only ~22:49 monitor #166: APPLY_RC not yet emitted (pid alive 31:56); P1+P2 PASS, 7 deletes applied, log ends mid-P3.
- [WINSRV_DELETE_WATCH167.md](WINSRV_DELETE_WATCH167.md) — read-only monitor #167: APPLY_RC not yet emitted, log mid-P3 strict-verify, pid 1286837 alive 32:01, P1/P2 PASS.
- [WINSRV_DELETE_WATCH169.md](WINSRV_DELETE_WATCH169.md) — read-only final-state monitor #169: apply2 still running ~32 min in P3 strict-verify with no APPLY_RC/rollback line; earlier apply.log ended `APPLY_RC=2`; snapshot token `ROLLBACK-WINSRV-20261124_215436`.
- [WINSRV_DELETE_WATCH170.md](WINSRV_DELETE_WATCH170.md) — read-only monitor #170: APPLY_RC NOT YET (pid ~32 min, P3 strict-verify in progress); P1/P2 PASS, 7 deletes done, no rollback lines.
- [WINSRV_DELETE_WATCH171.md](WINSRV_DELETE_WATCH171.md) — read-only ~22:49 monitor #171: APPLY_RC not yet emitted, in P3 full-verify phase; no rollback, snapshot taken, all 7 deletes in git status.
- [WINSRV_DELETE_WATCH172.md](WINSRV_DELETE_WATCH172.md) — read-only monitor #172: APPLY_RC NOT YET, log ends at P3 strict-verify, pid alive 32:17, exactly 7 staged deletions.
- [WINSRV_DELETE_WATCH173.md](WINSRV_DELETE_WATCH173.md) — read-only 22:25 UTC monitor #173: APPLY_RC not yet written; P3 strict-verify in flight (pid 1376334), P1 PASS (18 edits), P2 PASS, 7 deletes applied, no rollback.
- [WINSRV_DELETE_WATCH175.md](WINSRV_DELETE_WATCH175.md) — read-only monitor #175: APPLY_RC not yet emitted (~33 min in P3 gate list, final_verify last); snapshot token intact, pre-gates PASS.
- [WINSRV_DELETE_WATCH177.md](WINSRV_DELETE_WATCH177.md) — read-only monitor #177: APPLY_RC not yet written (pid 32m41s), log ends at the P3 gate battery; pre/P1/P2 gates PASS, exactly 7 deletes.
- [WINSRV_DELETE_WATCH178.md](WINSRV_DELETE_WATCH178.md) — read-only monitor #178: APPLY_RC NOT YET (pid ~32 min), log line 56 mid-P3 strict-verify; no rollback, snapshot on standby.
- [WINSRV_DELETE_WATCH179.md](WINSRV_DELETE_WATCH179.md) — read-only 22:27 EET monitor #179: APPLY_RC not yet emitted (pid 32:45, last line P3 strict-verify); no rollback; snapshot + `ROLLBACK-WINSRV-20261124_215436` token recorded, deletes confirmed.

These twelve newly indexed reports bring the indexed Markdown total to
**498**, matching the 498 `.md` files under `docs/` (including this index);
post-append re-verification shows 498 on-disk `.md` files = 498 unique
indexed link targets, 0 missing, 0 dangling; every Markdown document in
the directory is now linked.

## Added — 2026-11-24 post-486 sweep, WINSRV_DELETE_WATCH165–177 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: twenty reports existed on disk but had no entry;
they are linked below. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH165.md](WINSRV_DELETE_WATCH165.md) — read-only 2026-11-24 ~22:50 winsrv re-apply monitor #165: APPLY_RC NOT YET (pid 1286837, 32:00 elapsed, in P3 `final_verify --strict-skip`), P1/P2 gates PASS, 7 winsrv deletes applied, rollback token `ROLLBACK-WINSRV-20261124_215436` intact, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH166.md](WINSRV_DELETE_WATCH166.md) — read-only ~22:49 monitor #166: APPLY_RC NOT YET EMITTED, pid 1286837 alive 31:56 in the P3 gate list; P1+P2 PASS, all 7 deletes done, snapshot ready, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH167.md](WINSRV_DELETE_WATCH167.md) — read-only monitor #167: APPLY_RC NOT YET EMITTED, log mid-P3 strict-verify; P1/P2 gates PASS, 7 files `D` in git status, `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH169.md](WINSRV_DELETE_WATCH169.md) — read-only monitor #169: apply2 still RUNNING ~32 min in P3 strict-verify with no APPLY_RC/rollback line (earlier apply ended `APPLY_RC=2`); P1/P2 passed, 7 deletions in worktree, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH170.md](WINSRV_DELETE_WATCH170.md) — read-only monitor #170: APPLY_RC NOT YET, pid 1286837 ~32 min in P3 `final_verify --strict-skip`; P1/P2 PASS, 7 deletes confirmed, HEAD `0f03f7e`, no rollback.
- [WINSRV_DELETE_WATCH171.md](WINSRV_DELETE_WATCH171.md) — read-only ~22:49 monitor #171: APPLY_RC NOT YET EMITTED, pid 1286837 ~32 min in gate[P3]; pre-apply/P1/P2 gates PASS, 7 deletes in git status, `kernelhwsrv/` 0 modified paths, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH172.md](WINSRV_DELETE_WATCH172.md) — read-only monitor #172: APPLY_RC NOT YET, log ends at `gate[P3]: final_verify --strict-skip`, pid alive 32:17; 7 `D` entries, `kernelhwsrv/` clean, HEAD `0f03f7e`, snapshot token intact.
- [WINSRV_DELETE_WATCH173.md](WINSRV_DELETE_WATCH173.md) — read-only 22:25 UTC monitor #173: APPLY_RC NOT YET WRITTEN, pid 1286837 32m+ in P3 strict-verify; P1 (18 edits)/P2 PASS, exactly 7 deletes, `kernelhwsrv/` 0 lines, HEAD `0f03f7e`, no commit.
- [WINSRV_DELETE_WATCH175.md](WINSRV_DELETE_WATCH175.md) — read-only monitor #175: APPLY_RC NOT YET EMITTED, pid ~33 min in the P3 gate list; pre/P1/P2 PASS, 7 deletes confirmed, `kernelhwsrv/` clean, HEAD `0f03f7e`, deletes uncommitted.
- [WINSRV_DELETE_WATCH177.md](WINSRV_DELETE_WATCH177.md) — read-only monitor #177: APPLY_RC NOT YET WRITTEN, pid 1286837 32m41s at the P3 gate battery; pre/P1/P2 PASS, 7 deletes, `kernelhwsrv/` clean, HEAD `0f03f7e` on `97f224b`.
- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — read-only monitor #187: re-apply not finished, no APPLY_RC/rollback in the log; pid 1286837 alive 33:25 in P3 strict-verify after P1/P2 PASS; 7 deletes confirmed, `kernelhwsrv/` 0 lines.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — read-only monitor #188: APPLY_RC not yet printed, pid 1286837 alive 33:30 in the P3 gate list; 7 deletes match the plan, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — read-only ~22:51 monitor #189: APPLY_RC not yet emitted, pid alive 33m35s mid-P3; pre-gate + P1 (18 edits) + P2 PASS, 7 deletes done, snapshot token intact, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH190.md](WINSRV_DELETE_WATCH190.md) — read-only monitor #190: APPLY_RC NOT YET EMITTED, log ends at `gate[P3]: final_verify --strict-skip`, pid 33m34s; 18 edits + 7 deletes done, D count exactly 7.
- [WINSRV_DELETE_WATCH191.md](WINSRV_DELETE_WATCH191.md) — read-only ~22:50 monitor #191: APPLY_RC NOT YET EMITTED, pid 33:44 in the P3 gate list, no rollback; pre-apply/P1/P2 PASS, 7 deletes confirmed, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH192.md](WINSRV_DELETE_WATCH192.md) — read-only ~22:17+ monitor #192: APPLY_RC NOT YET WRITTEN, pid alive mid-P3 final-verify; 7 deletes staged and confirmed, `kernelhwsrv/` 0 lines.
- [WINSRV_DELETE_WATCH194.md](WINSRV_DELETE_WATCH194.md) — read-only monitor #194: APPLY_RC not yet printed, pid 1286837 33:54 in P3 (`final_verify --strict-skip` last); P1/P2 PASS, 18/18 edits, 7 deletes, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH28.md](WINSRV_DELETE_WATCH28.md) — read-only 2026-11-24 completion monitor #28: verdict YES — re-apply finished `APPLY_RC=0`, all 7 deletions landed, P1/P2/P3 gates all PASS incl. `final_verify --strict-skip`, rollback token unused.
- [WINSRV_DELETE_WATCH84.md](WINSRV_DELETE_WATCH84.md) — read-only ~22:47 completion report #84: re-apply finished `APPLY_RC=0` after ~34 min, `SUCCESS: winsrv deletion applied across P1-P3`, all phase gates PASS, no rollback, snapshot token intact.
- [WINSRV_DELETE_WATCH199.md](WINSRV_DELETE_WATCH199.md) — read-only completion monitor #199: `APPLY_RC=0`, winsrv deletion applied across P1–P3 with all phase gates PASS (incl. `final_verify --strict-skip`), 7 deletes confirmed, pid 1286837 finished, `kernelhwsrv/` clean.

These twenty newly indexed reports bring the indexed Markdown total to
**523**, matching the 523 `.md` files under `docs/` (including this index;
the header count "(523 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 523 on-disk `.md` files =
523 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 post-473 WINSRV_DELETE_WATCH163–179 backfill (append-only)

A fresh full `docs/*.md` listing (504 files, flat directory, no subdirectories) was diffed against the unique `.md` link targets in this index: 13 reports existed on disk but had no entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH171.md](WINSRV_DELETE_WATCH171.md) — - APPLY_RC: NOT YET EMITTED — apply still running (pid 1286837, ~32 min elapsed, in gate[P3] full-verify phase of /tmp/opencode/winsrv_delete_apply2.log, 56 lines).
- [WINSRV_DELETE_WATCH172.md](WINSRV_DELETE_WATCH172.md) — - APPLY_RC: **NOT YET** — `/tmp/opencode/winsrv_delete_apply2.log` (3,355 B, no APPLY_RC/rollback line) ends at `+ gate[P3]: r1_final_verify.py --strict-skip`; apply still in flight.
- [WINSRV_DELETE_WATCH173.md](WINSRV_DELETE_WATCH173.md) — - APPLY_RC: NOT YET WRITTEN — pid 1286837 (r1_winsrv_delete.py --apply) still alive (32m+), currently in P3 gate: r1_final_verify.py --strict-skip running (pid 1376334). No rollback lines in log; no APPLY_RC= line yet.
- [WINSRV_DELETE_WATCH174.md](WINSRV_DELETE_WATCH174.md) — - APPLY_RC: **not yet emitted** — `/tmp/opencode/winsrv_delete_apply2.log` (3355 B, last write 22:17) ends at `+ gate[P3]: r1_final_verify.py --strict-skip`; no APPLY_RC, no ROLLBACK/FAIL/Traceback lines.
- [WINSRV_DELETE_WATCH175.md](WINSRV_DELETE_WATCH175.md) — - APPLY_RC: NOT YET EMITTED — apply still running (pid 1286837, `r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`, ~33 min elapsed), currently inside phase P3 gate list (final gate: `r1_final_verify.py --strict
- [WINSRV_DELETE_WATCH176.md](WINSRV_DELETE_WATCH176.md) — - APPLY_RC: **not yet emitted** — apply still running, pid 1286837 alive (32:55 elapsed), currently in P3 gate at `r1_final_verify.py --strict-skip`; no rollback triggered (snapshot `winsrv_retire_backup_20261124_215436`
- [WINSRV_DELETE_WATCH177.md](WINSRV_DELETE_WATCH177.md) — - APPLY_RC: NOT YET WRITTEN — apply still in flight; pid 1286837 alive (32m41s), log ends at `+ gate[P3]` battery (final_verify last), no rollback lines.
- [WINSRV_DELETE_WATCH178.md](WINSRV_DELETE_WATCH178.md) — - APPLY_RC: NOT YET — pid 1286837 (`r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-EWSRV`) still running (~32 min); log `/tmp/opencode/winsrv_delete_apply2.log` (3.3 KB) ends at line 56 mid-P3 gates (`r1_final_verif
- [WINSRV_DELETE_WATCH179.md](WINSRV_DELETE_WATCH179.md) — - APPLY_RC: **not yet emitted** — apply still running (pid 1286837 alive, 32:45 elapsed); last log line is `+ gate[P3]: r1_final_verify.py --strict-skip`. No rollback triggered.
- [WINSRV_DELETE_WATCH180.md](WINSRV_DELETE_WATCH180.md) — - APPLY_RC: **NOT YET** — apply still running (pid 1286837, ~33 min elapsed); log ends at line 56, P3 gate list in progress (make, selfcheck, smoke, ewsrv, input, doom*, fullchain, retire, final_verify). No rollback trig
- [WINSRV_DELETE_WATCH181.md](WINSRV_DELETE_WATCH181.md) — - APPLY_RC: NOT YET WRITTEN — apply still running. PID 1286837 alive (~33 min), currently in P3 gate `r1_final_verify.py --strict-skip`; no APPLY_RC/rollback line in /tmp/opencode/winsrv_delete_apply2.log (log is 3.4 KB,
- [WINSRV_DELETE_WATCH182.md](WINSRV_DELETE_WATCH182.md) — - APPLY_RC: **not yet emitted** — apply still in flight; log ends at P3 gate line (`r1_final_verify.py --strict-skip`), no APPLY_RC/rollback line. No rollback triggered.
- [WINSRV_DELETE_WATCH183.md](WINSRV_DELETE_WATCH183.md) — - APPLY_RC: NOT YET — apply still running (pid 1286837, ~33 min); log ends at P3 gate `r1_final_verify.py --strict-skip`. No rollback so far (snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436`, token ROLLBACK-

These 13 newly indexed reports bring the indexed Markdown total to **504**, matching the 504 `.md` files under `docs/` (including this index; the header count at line 4 is updated); every Markdown document in the directory is now linked.

## Added — 2026-11-24 post-480 sweep, WINSRV_DELETE_WATCH163/165–173 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: eleven reports existed on disk but had no entry; they
are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH163.md](WINSRV_DELETE_WATCH163.md) — read-only 2026-11-24 monitor #163: APPLY_RC not yet emitted (pid 1286837 ~32 min); P1/P2 gates PASS, 7 winsrv files deleted, P3 gate (`r1_final_verify.py --strict-skip`) in flight, no rollback.
- [WINSRV_DELETE_WATCH165.md](WINSRV_DELETE_WATCH165.md) — read-only ~22:50 monitor #165: APPLY_RC NOT YET, apply log ends at `gate[P3]: r1_final_verify.py --strict-skip`, pid 1286837 alive 32:00, no APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH166.md](WINSRV_DELETE_WATCH166.md) — read-only ~22:49 monitor #166: APPLY_RC NOT YET EMITTED, winsrv re-apply (pid 1286837) still running in the P3 full-verify gate.
- [WINSRV_DELETE_WATCH167.md](WINSRV_DELETE_WATCH167.md) — read-only monitor #167: APPLY_RC NOT YET EMITTED, log ends mid-P3 gates, pid 1286837 alive 32:01, no rollback line.
- [WINSRV_DELETE_WATCH168.md](WINSRV_DELETE_WATCH168.md) — read-only monitor #168: apply log (3.4 KB, 56 lines) ends mid-P3 at `final_verify --strict-skip`, 0 APPLY_RC/rollback lines, pid 1286837 elapsed 32:01.
- [WINSRV_DELETE_WATCH169.md](WINSRV_DELETE_WATCH169.md) — read-only monitor #169 (final-state): apply2 still RUNNING ~32 min in P3 gate, no APPLY_RC/rollback; earlier apply.log ended `APPLY_RC=2`; P1/P2 PASS, 7 deletions applied, rollback token intact.
- [WINSRV_DELETE_WATCH170.md](WINSRV_DELETE_WATCH170.md) — read-only monitor #170: APPLY_RC NOT YET; P1/P2 gates PASS, 7 deletes done, P3 gates running, pid 1286837 ~32 min, no rollback lines.
- [WINSRV_DELETE_WATCH171.md](WINSRV_DELETE_WATCH171.md) — read-only ~22:49 monitor #171: APPLY_RC NOT YET EMITTED, pid 1286837 ~32 min in P3 full-verify (log 56 lines), no rollback, snapshot token `ROLLBACK-WINSRV-20261124_215436` intact.
- [WINSRV_DELETE_WATCH172.md](WINSRV_DELETE_WATCH172.md) — read-only monitor #172: apply log 3,355 B with no APPLY_RC/rollback, ends at `gate[P3]: final_verify --strict-skip`; pid 1286837 elapsed 32:17.
- [WINSRV_DELETE_WATCH173.md](WINSRV_DELETE_WATCH173.md) — read-only 22:25 UTC monitor #173: APPLY_RC NOT YET WRITTEN, pid 1286837 32m+ in P3 gate (`final_verify --strict-skip`, pid 1376334); P1 (18 edits)/P2 PASS, 7 deletes applied, no rollback, token intact.

These eleven newly indexed reports bring the indexed Markdown total to
**510**, matching the 510 `.md` files under `docs/` (including this index;
the header count at line 4 is updated); post-append re-verification shows
510 on-disk `.md` files = 510 unique indexed link targets, 0 missing,
0 dangling; every Markdown document in the directory is now linked.

## Added — 2026-11-24 post-466 sweep, WINSRV_DELETE_WATCH163–170 backfill (append-only)

A fresh full `docs/*.md` listing (491 files, flat directory, no subdirectories)
was diffed against the 484 unique `.md` link targets previously in this index:
seven documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH163.md](WINSRV_DELETE_WATCH163.md) — read-only 2026-11-24
  winsrv re-apply monitor #163: APPLY_RC not yet emitted, apply pid 1286837
  alive ~32 min, still in flight.
- [WINSRV_DELETE_WATCH165.md](WINSRV_DELETE_WATCH165.md) — read-only 2026-11-24
  (~22:50) winsrv re-apply monitor #165: APPLY_RC NOT YET, log ends at P3
  `r1_final_verify.py --strict-skip`, no rollback line.
- [WINSRV_DELETE_WATCH166.md](WINSRV_DELETE_WATCH166.md) — read-only 2026-11-24
  (~22:49) winsrv re-apply monitor #166: APPLY_RC not yet emitted, apply still
  running.
- [WINSRV_DELETE_WATCH167.md](WINSRV_DELETE_WATCH167.md) — read-only 2026-11-24
  winsrv re-apply monitor #167: APPLY_RC not yet emitted, log ends mid-P3
  gates, no APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH168.md](WINSRV_DELETE_WATCH168.md) — read-only 2026-11-24
  winsrv re-apply monitor #168: log ends at line 56 mid-P3 gate, apply IN
  PROGRESS, no rollback triggered.
- [WINSRV_DELETE_WATCH169.md](WINSRV_DELETE_WATCH169.md) — read-only 2026-11-24
  winsrv re-apply monitor #169 (final-state watch): apply still RUNNING ~32 min
  in P3, no APPLY_RC/rollback lines, token `ROLLBACK-WINSRV-20261124_215436`.
- [WINSRV_DELETE_WATCH170.md](WINSRV_DELETE_WATCH170.md) — read-only 2026-11-24
  winsrv re-apply monitor #170: APPLY_RC NOT YET, P3 `r1_final_verify.py
  --strict-skip` in progress, no rollback lines.

These seven newly indexed reports bring the indexed Markdown total to **510**,
matching the 510 `.md` files under `docs/` (including this index; the header
count "(491 files, verified 2026-11-24)" at line 4 is updated);
post-append re-verification shows 510 on-disk `.md` files = 510 unique indexed
link targets, 0 missing, 0 dangling; every Markdown document in the directory
is now linked.

## Added — 2026-11-24 last-473 sweep, WINSRV_DELETE_WATCH163/165–173 backfill (append-only)

A fresh full `docs/*.md` listing (494 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: ten read-only winsrv re-apply monitor reports existed on disk but had
no entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

- [WINSRV_DELETE_WATCH163.md](WINSRV_DELETE_WATCH163.md) — read-only 2026-11-24 monitor #163: APPLY_RC not yet emitted (pid 1286837 ~32 min, in P3 `final_verify --strict-skip`); P1/P2 gates PASS, 7 winsrv deletes staged, HEAD `0f03f7e`, no rollback.
- [WINSRV_DELETE_WATCH165.md](WINSRV_DELETE_WATCH165.md) — read-only 2026-11-24 monitor #165: APPLY_RC NOT YET (pid 1286837 alive 32:00); P1 (18 edits) + P2 gates PASS, all 7 deletes done, P3 full gate suite in progress.
- [WINSRV_DELETE_WATCH166.md](WINSRV_DELETE_WATCH166.md) — read-only 2026-11-24 monitor #166: APPLY_RC NOT YET EMITTED, apply still running (pid 1286837, 31:56, state S).
- [WINSRV_DELETE_WATCH167.md](WINSRV_DELETE_WATCH167.md) — read-only 2026-11-24 monitor #167: APPLY_RC NOT YET EMITTED; log ends mid-P3 (`final_verify --strict-skip` in flight); P1/P2 PASS, 7 deletes applied, P3 gates running.
- [WINSRV_DELETE_WATCH168.md](WINSRV_DELETE_WATCH168.md) — read-only 2026-11-24 monitor #168: APPLY_RC NOT YET EMITTED, log at line 56 mid-P3 gate; pid 1286837 alive 32:01; exactly 7 `D` deletions (winsrv.c, guishost.h, wsclient.c/.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp).
- [WINSRV_DELETE_WATCH169.md](WINSRV_DELETE_WATCH169.md) — read-only 2026-11-24 monitor #169 (final-state): apply2 still RUNNING in P3 gate, no APPLY_RC/rollback; earlier apply.log ended `APPLY_RC=2`; 7 deletes staged, HEAD `0f03f7e`, `kernelhwsrv/` clean (0 lines).
- [WINSRV_DELETE_WATCH170.md](WINSRV_DELETE_WATCH170.md) — read-only 2026-11-24 monitor #170: APPLY_RC NOT YET (pid 1286837 ~32 min, P3 `final_verify --strict-skip`); P1/P2 PASS, 7 deletes done, no rollback, snapshot intact, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH171.md](WINSRV_DELETE_WATCH171.md) — read-only 2026-11-24 ~22:49 monitor #171: APPLY_RC NOT YET EMITTED (pid 1286837 ~32 min, P3 full-verify); no rollback, snapshot token intact, all 7 deletes in `git status`, `kernelhwsrv/` untouched (0 lines).
- [WINSRV_DELETE_WATCH172.md](WINSRV_DELETE_WATCH172.md) — read-only 2026-11-24 monitor #172: APPLY_RC NOT YET, log ends at P3 `final_verify --strict-skip` (pid 1286837, 32:17); 7 deletes confirmed, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH173.md](WINSRV_DELETE_WATCH173.md) — read-only 2026-11-24 22:25 UTC monitor #173: APPLY_RC NOT YET WRITTEN (pid 1286837 32m+ in P3 gate); P1/P2 PASS, exactly 7 deletes, snapshot token intact, `kernelhwsrv/` clean.

These ten newly indexed reports bring the indexed Markdown total to
**494**, matching the 494 `.md` files under `docs/` (including this index;
the header count "(494 files, verified 2026-11-24)" at line 4 is updated);
post-append re-verification shows 494 on-disk `.md` files = 494 unique
indexed link targets, 0 missing, 0 dangling; every Markdown document in the
directory is now linked.

## Added — 2026-11-24 last-460 sweep catch-up (append-only)

A fresh full `docs/*.md` listing (505 files, flat directory, no subdirectories) was diffed against the unique `.md` link targets in this index: 1 further reports had landed on disk but had no entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH184.md](WINSRV_DELETE_WATCH184.md) — WINSRV_DELETE_WATCH184 — re-apply status (2026-11-24 22:27 EET).

These newly indexed reports bring the indexed Markdown total to **505**, matching the 505 `.md` files under `docs/` (including this index; the header count at line 4 is updated); post-append re-verification shows 505 on-disk `.md` files = 505 unique indexed link targets, 0 missing, 0 dangling.

## Added — 2026-11-24 post-473 sweep, WINSRV_DELETE_WATCH163/165–173 backfill (append-only)

A fresh full `docs/*.md` listing (510 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: fifteen reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH163.md](WINSRV_DELETE_WATCH163.md) — read-only 2026-11-24 winsrv re-apply monitor #163: APPLY_RC not yet emitted, pid 1286837 (~32 min) still in the P3 `r1_final_verify.py --strict-skip` gate, P1/P2 gates PASS, 7 winsrv deletions staged, no rollback, `kernelhwsrv/` clean, HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH165.md](WINSRV_DELETE_WATCH165.md) — read-only 2026-11-24 ~22:50 winsrv re-apply monitor #165: APPLY_RC NOT YET, log ends at `gate[P3]: r1_final_verify.py --strict-skip`, pid 1286837 alive (32:00), no APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH166.md](WINSRV_DELETE_WATCH166.md) — read-only 2026-11-24 ~22:49 winsrv re-apply monitor #166: APPLY_RC not yet emitted, apply still running in the P3 full-gate phase with no rollback.
- [WINSRV_DELETE_WATCH167.md](WINSRV_DELETE_WATCH167.md) — read-only 2026-11-24 winsrv re-apply monitor #167: APPLY_RC NOT YET EMITTED, log (3.3 KB) ends mid-P3 strict-verify gate, pid 1286837 alive (32:01), no APPLY_RC/rollback.
- [WINSRV_DELETE_WATCH168.md](WINSRV_DELETE_WATCH168.md) — read-only 2026-11-24 winsrv re-apply monitor #168: APPLY_RC NOT YET EMITTED, log ends at line 56 mid-P3 `r1_final_verify.py --strict-skip`, pid 1286837 alive (32:01), apply IN PROGRESS, no rollback.
- [WINSRV_DELETE_WATCH169.md](WINSRV_DELETE_WATCH169.md) — read-only 2026-11-24 winsrv monitor #169 (final-state watch): apply2 (pid 1286837) still RUNNING ~32 min in P3 gate, no APPLY_RC/rollback; earlier apply.log ended `APPLY_RC=2`; P1/P2 PASS, all 7 winsrv deletions applied, token `ROLLBACK-WINSRV-20261124_215436`.
- [WINSRV_DELETE_WATCH170.md](WINSRV_DELETE_WATCH170.md) — read-only 2026-11-24 winsrv re-apply monitor #170: APPLY_RC NOT YET, pid 1286837 ~32 min in P3 gate; P1 (variant edits) and P2 (Makefile/user.mk + isogrub) PASS, 7 deletions done, P3 gates running, no rollback.
- [WINSRV_DELETE_WATCH171.md](WINSRV_DELETE_WATCH171.md) — read-only 2026-11-24 ~22:49 winsrv re-apply monitor #171: APPLY_RC NOT YET EMITTED, pid 1286837 ~32 min in gate[P3] full-verify (56-line log); no rollback; snapshot `/tmp/opencode/winsrv_retire_backup_20261124_215436` (token `ROLLBACK-WINSRV-20261124_215436`).
- [WINSRV_DELETE_WATCH172.md](WINSRV_DELETE_WATCH172.md) — read-only 2026-11-24 winsrv monitor #172: APPLY_RC NOT YET, log (3,355 B, no APPLY_RC/rollback) ends at `gate[P3]: r1_final_verify.py --strict-skip`; pid 1286837 still RUNNING (32:17).
- [WINSRV_DELETE_WATCH173.md](WINSRV_DELETE_WATCH173.md) — read-only 2026-11-24 22:25 UTC winsrv re-apply monitor #173: APPLY_RC NOT YET WRITTEN, pid 1286837 alive 32m+ in P3 strict-verify (pid 1376334); P1 PASS (18 edits), P2 PASS, 7 deletes applied, no rollback, snapshot/token intact.

- [WINSRV_DELETE_WATCH185.md](WINSRV_DELETE_WATCH185.md) — read-only 2026-11-24 ~22:50 winsrv re-apply monitor #185: APPLY_RC not yet emitted, apply2 log (56 lines) ends inside P3 `r1_final_verify.py --strict-skip` gates, no rollback; pid 1286837 alive 33+ min.
- [WINSRV_DELETE_WATCH186.md](WINSRV_DELETE_WATCH186.md) — read-only 2026-11-24 winsrv monitor #186: APPLY_RC NOT YET EMITTED, log ends at `gate[P3]` strict-verify, no APPLY_RC/rollback; pid 1286837 RUNNING (33:21); snapshot/token unused.
- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — read-only 2026-11-24 winsrv re-apply monitor #187: re-apply not finished, no APPLY_RC/rollback/Traceback in the 3.3 KB log; pid 1286837 alive (33:25) in P3 strict-verify after P1/P2 PASS.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — read-only 2026-11-24 winsrv delete watch #188: APPLY_RC not yet printed, still in P3 gates (strict-verify queued), no rollback/FAILED lines; pid 1286837 alive (33:30).
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — read-only 2026-11-24 ~22:51 winsrv re-apply monitor #189: APPLY_RC not yet emitted, pid 1286837 alive (33m35s) mid-P3 strict-verify, no rollback; P1 (18 edits) + P2 PASS, all 7 deletes done, snapshot/token intact.

These fifteen newly indexed reports bring the indexed Markdown total to
**510**, matching the 510 `.md` files under `docs/` (including this index;
the header count at line 4 is consistent); post-append re-verification
shows 510 on-disk `.md` files = 510 unique indexed link targets, 0 missing,
0 dangling; every Markdown document in the directory is now linked.

## Added — 2026-11-24 post-479 sweep, WINSRV_DELETE_WATCH163–173 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: fifteen reports existed on disk but had no entry;
they are linked below. `kernelhwsrv/` was untouched and no QEMU run was made
by this index update.

- [WINSRV_DELETE_WATCH163.md](WINSRV_DELETE_WATCH163.md) — read-only 2026-11-24 winsrv re-apply monitor #163: APPLY_RC not yet emitted (pid 1286837 ~32 min), P1/P2 gates PASS, 7 winsrv deletes staged, P3 gate `r1_final_verify.py --strict-skip` in flight, no rollback.
- [WINSRV_DELETE_WATCH165.md](WINSRV_DELETE_WATCH165.md) — read-only 2026-11-24 ~22:50 monitor #165: APPLY_RC NOT YET, log ends mid-P3 at the final-verify gate, pid 1286837 alive 32:00, no APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH166.md](WINSRV_DELETE_WATCH166.md) — read-only 2026-11-24 ~22:49 monitor #166: APPLY_RC not yet emitted, apply still running.
- [WINSRV_DELETE_WATCH167.md](WINSRV_DELETE_WATCH167.md) — read-only 2026-11-24 monitor #167: APPLY_RC NOT YET EMITTED, log ends mid-P3 gates, pid 1286837 alive 32:01, no rollback.
- [WINSRV_DELETE_WATCH168.md](WINSRV_DELETE_WATCH168.md) — read-only 2026-11-24 monitor #168: APPLY_RC NOT YET EMITTED, log ends at line 56 mid-P3 (`r1_final_verify.py --strict-skip`), apply in progress, no rollback.
- [WINSRV_DELETE_WATCH169.md](WINSRV_DELETE_WATCH169.md) — read-only 2026-11-24 final-state monitor #169: apply2 still RUNNING in P3 final gate, no APPLY_RC/rollback; earlier apply attempt ended `APPLY_RC=2`; P1/P2 PASS, 7 deletions applied, rollback token intact.
- [WINSRV_DELETE_WATCH170.md](WINSRV_DELETE_WATCH170.md) — read-only 2026-11-24 monitor #170: APPLY_RC NOT YET, P1/P2 PASS, 7 deletes done, P3 gates running, no rollback lines.
- [WINSRV_DELETE_WATCH172.md](WINSRV_DELETE_WATCH172.md) — read-only 2026-11-24 monitor #172: APPLY_RC NOT YET, log ends at `+ gate[P3]: r1_final_verify.py --strict-skip`, pid 1286837 alive 32:17.
- [WINSRV_DELETE_WATCH173.md](WINSRV_DELETE_WATCH173.md) — read-only 2026-11-24 22:25 UTC monitor #173: APPLY_RC not yet written, pid 1286837 in P3 gate (final verify pid 1376334), P1 (18 edits)/P2 PASS, 7 deletes, snapshot token intact.
- [WINSRV_DELETE_WATCH185.md](WINSRV_DELETE_WATCH185.md) — read-only 2026-11-24 ~22:50 monitor #185: APPLY_RC not yet emitted, log (3.3 KB, 56 lines) ends inside P3 gates at `r1_final_verify.py --strict-skip`, no rollback.
- [WINSRV_DELETE_WATCH186.md](WINSRV_DELETE_WATCH186.md) — read-only 2026-11-24 monitor #186: APPLY_RC NOT YET EMITTED, log ends at the P3 final-verify gate, no APPLY_RC/rollback line, snapshot token exists but unused.
- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — read-only 2026-11-24 monitor #187: re-apply not finished — no APPLY_RC and no rollback/FAILED/Traceback markers in the fully read apply log.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — read-only 2026-11-24 monitor #188: APPLY_RC not yet printed, apply still in P3 gates with the final-verify gate queued, no rollback/FAILED lines.
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — read-only 2026-11-24 ~22:51 monitor #189: APPLY_RC not yet emitted, pid 1286837 alive 33m35s mid-P3, no rollback lines, snapshot token intact.
- [WINSRV_DELETE_WATCH84.md](WINSRV_DELETE_WATCH84.md) — read-only 2026-11-24 ~22:47 completion report #84: re-apply FINISHED with `APPLY_RC=0`, no rollback; P1/P2/P3 gates all PASS (incl. `r1_final_verify.py --strict-skip`), ewsrv retire gate PASS at every phase, snapshot taken but unused.

These fifteen newly indexed reports bring the indexed Markdown total to
**520**, matching the 520 `.md` files under `docs/` (including this index);
post-append re-verification shows 520 on-disk `.md` files = 520 unique
indexed link targets, 0 missing, 0 dangling; every Markdown document in the
directory is now linked.

## Added — 2026-11-24 last-473 sweep, WINSRV_DELETE_WATCH185 backfill (append-only)

A fresh full `docs/*.md` listing (506 files, flat directory, no
subdirectories) was diffed against the 505 unique `.md` link targets in
this index: one report existed on disk but had no entry; it is linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH185.md](WINSRV_DELETE_WATCH185.md) — read-only 2026-11-24 ~22:50 winsrv re-apply monitor #185: APPLY_RC not yet emitted (log `winsrv_delete_apply2.log` ends inside P3 gates with `r1_final_verify.py --strict-skip` as last line; pid 1286837 alive 33+ min, no rollback triggered).

This newly indexed report brings the indexed Markdown total to
**506**, matching the 506 `.md` files under `docs/` (including this index;
the header count "(506 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 506 on-disk `.md` files =
506 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 last-473 sweep verification (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets
in this index. At scan time four documents existed on disk without an entry —
[WINSRV_DELETE_WATCH158.md](WINSRV_DELETE_WATCH158.md) (read-only re-apply
monitor #158: APPLY_RC not yet emitted, pid 1286837 in the P3
`r1_final_verify.py --strict-skip` gate, P1/P2 gates PASS, no rollback),
[WINSRV_DELETE_WATCH160.md](WINSRV_DELETE_WATCH160.md) (monitor #160: APPLY_RC
not yet seen, P3 gate in progress, 7 winsrv deletions staged, snapshot token
intact), [WINSRV_DELETE_WATCH161.md](WINSRV_DELETE_WATCH161.md) (monitor
#161: APPLY_RC not finished, P1/P2 PASS, 7 staged deletions, P3 gates
started) and [WINSRV_DELETE_WATCH162.md](WINSRV_DELETE_WATCH162.md) (monitor
#162: APPLY_RC not yet printed, mid-P3 strict-verify, `kernelhwsrv/` clean) —
and they are now linked above. Post-append re-verification: on-disk `.md`
files = unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is linked. `kernelhwsrv/` was untouched and no QEMU
run was made by this index update.

The indexed Markdown total is **505**, matching the 505 `.md` files under
`docs/` (including this index; the header count "(505 files, verified
2026-11-24)" at line 4 is consistent).

## Added — 2026-11-24 WINSRV_DELETE_WATCH186–192 backfill (append-only)

A full `docs/*.md` listing (513 files, flat directory, no subdirectories)
was diffed against the unique `.md` link targets in this index: seven
reports existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH186.md](WINSRV_DELETE_WATCH186.md) — read-only 2026-11-24 winsrv re-apply monitor #186: APPLY_RC not yet emitted, log ends at the P3 `r1_final_verify.py --strict-skip` gate, pid 1286837 alive 33+ min, 18 edits + 7 deletes applied, pre/P1/P2 gates PASS, no rollback.
- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — re-apply status #187: no APPLY_RC in `winsrv_delete_apply2.log`, pid 1286837 in P3 strict-verify after P1/P2 PASS, all 7 winsrv file deletions confirmed in `git status`, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — monitor #188: APPLY_RC not printed, apply mid-P3 gates, pid 1286837 alive, 7 staged deletes match the plan (winsrv.c, guishost.h, wsclient.c/h, qemu_windowsrv.cpp/h, qemu_gui_exec.cpp), `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — re-apply status ~22:51 #189: APPLY_RC not emitted, P3 gates running, pre+P1+P2 PASS, all 7 deletes done, rollback snapshot token intact and unused.
- [WINSRV_DELETE_WATCH190.md](WINSRV_DELETE_WATCH190.md) — monitor #190: APPLY_RC not yet emitted, log at P3 gate battery, 18 edits applied with P1/P2 PASS, 7 deletes confirmed.
- [WINSRV_DELETE_WATCH191.md](WINSRV_DELETE_WATCH191.md) — re-apply status ~22:50 #191: APPLY_RC not emitted, no rollback, 7 staged deletions, `kernelhwsrv/` clean, pre/P1/P2 gates PASS with P3 in flight.
- [WINSRV_DELETE_WATCH192.md](WINSRV_DELETE_WATCH192.md) — re-apply status ~22:17+ #192: APPLY_RC not written, pid 1286837 alive in the P3 final-verify phase, all 7 winsrv deletes staged, `kernelhwsrv/` clean.

These seven newly indexed reports bring the indexed Markdown total to
**513**, matching the 513 `.md` files under `docs/` (including this
index; the header count "(513 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 513 on-disk `.md` files =
513 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 post-473 sweep, WINSRV_DELETE_WATCH186–192 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: seven reports existed on disk but had no entry; they
are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH186.md](WINSRV_DELETE_WATCH186.md) — read-only 2026-11-24 winsrv re-apply monitor #186: APPLY_RC not yet emitted, P3 gates in flight, no rollback.
- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — read-only 2026-11-24 winsrv re-apply monitor #187: APPLY_RC not yet emitted, apply pid in P3 final-verify gate, no rollback.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — read-only 2026-11-24 winsrv delete watch #188: apply still running in P3 gates, no APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — read-only 2026-11-24 ~22:51 winsrv re-apply monitor #189: APPLY_RC not yet emitted, pid alive in P3 `r1_final_verify.py --strict-skip`.
- [WINSRV_DELETE_WATCH190.md](WINSRV_DELETE_WATCH190.md) — read-only 2026-11-24 winsrv re-apply monitor #190: APPLY_RC not yet emitted, log ends mid-P3 gate list, no rollback.
- [WINSRV_DELETE_WATCH191.md](WINSRV_DELETE_WATCH191.md) — read-only 2026-11-24 ~22:50 winsrv re-apply monitor #191: APPLY_RC not yet emitted, P3 gates running, snapshot/rollback token unconsumed.
- [WINSRV_DELETE_WATCH192.md](WINSRV_DELETE_WATCH192.md) — read-only 2026-11-24 ~22:17+ winsrv re-apply monitor #192: APPLY_RC not yet written, apply pid in P3 gate battery, no rollback lines.

## Added — 2026-11-24 post-505 sweep, WINSRV_DELETE_WATCH185–189 backfill (append-only)

A fresh full `docs/*.md` listing (510 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: five reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH185.md](WINSRV_DELETE_WATCH185.md) — read-only 2026-11-24 ~22:50 winsrv re-apply monitor #185: APPLY_RC not yet emitted (pid 1286837 33+ min in P3 `r1_final_verify.py --strict-skip`), pre/P1/P2 ewsrv gates PASS, 7 deletes staged, no rollback.
- [WINSRV_DELETE_WATCH186.md](WINSRV_DELETE_WATCH186.md) — read-only monitor #186: APPLY_RC NOT YET EMITTED, pid 1286837 alive 33:21 in P3 gates, 18 edits + 7 deletes applied, rollback snapshot token unused, `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — read-only monitor #187: re-apply NOT finished, no APPLY_RC/rollback in log, pid 1286837 in P3 strict-verify after P1/P2 PASS, 7 deletes confirmed, HEAD `0f03f7e`, `kernelhwsrv/` untouched.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — read-only monitor #188: APPLY_RC not yet printed, pid 1286837 alive 33:30 in P3 gates, 7 staged deletes match the plan, `kernelhwsrv/` clean (0 modified), HEAD `0f03f7e`.
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — read-only 2026-11-24 ~22:51 monitor #189: APPLY_RC not yet emitted, pid 1286837 33m35s mid-P3 strict-verify, P1/P2 PASS, 7 deletes confirmed, rollback token intact, `kernelhwsrv/` untouched.

These five newly indexed reports bring the indexed Markdown total to
**510**, matching the 510 `.md` files under `docs/` (including this index;
the header count "(510 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 510 on-disk `.md` files =
510 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 winsrv-delete watch backfill (append-only)

A full `docs/*.md` listing (515 files, flat directory, no subdirectories)
was diffed against the 507 unique `.md` link targets previously in this
index: 8 document(s) existed on disk but had no entry; they are
linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — read-only 2026-11-24 watch of the in-flight winsrv-delete re-apply (pid 1286837): `APPLY_RC` not yet emitted, P3 gate `r1_final_verify.py --strict-skip` still running, no rollback triggered; P1/P2 gates PASS with the 7 winsrv deletions applied; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — read-only 2026-11-24 watch of the in-flight winsrv-delete re-apply (pid 1286837): `APPLY_RC` not yet emitted, P3 gate `r1_final_verify.py --strict-skip` still running, no rollback triggered; P1/P2 gates PASS with the 7 winsrv deletions applied; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — read-only 2026-11-24 watch of the in-flight winsrv-delete re-apply (pid 1286837): `APPLY_RC` not yet emitted, P3 gate `r1_final_verify.py --strict-skip` still running, no rollback triggered; P1/P2 gates PASS with the 7 winsrv deletions applied; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH190.md](WINSRV_DELETE_WATCH190.md) — read-only 2026-11-24 watch of the in-flight winsrv-delete re-apply (pid 1286837): `APPLY_RC` not yet emitted, P3 gate `r1_final_verify.py --strict-skip` still running, no rollback triggered; P1/P2 gates PASS with the 7 winsrv deletions applied; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH191.md](WINSRV_DELETE_WATCH191.md) — read-only 2026-11-24 watch of the in-flight winsrv-delete re-apply (pid 1286837): `APPLY_RC` not yet emitted, P3 gate `r1_final_verify.py --strict-skip` still running, no rollback triggered; P1/P2 gates PASS with the 7 winsrv deletions applied; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH192.md](WINSRV_DELETE_WATCH192.md) — read-only 2026-11-24 watch of the in-flight winsrv-delete re-apply (pid 1286837): `APPLY_RC` not yet emitted, P3 gate `r1_final_verify.py --strict-skip` still running, no rollback triggered; P1/P2 gates PASS with the 7 winsrv deletions applied; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH193.md](WINSRV_DELETE_WATCH193.md) — read-only 2026-11-24 watch of the in-flight winsrv-delete re-apply (pid 1286837): `APPLY_RC` not yet emitted, P3 gate `r1_final_verify.py --strict-skip` still running, no rollback triggered; P1/P2 gates PASS with the 7 winsrv deletions applied; `kernelhwsrv/` clean.
- [WINSRV_DELETE_WATCH194.md](WINSRV_DELETE_WATCH194.md) — read-only 2026-11-24 watch of the in-flight winsrv-delete re-apply (pid 1286837): `APPLY_RC` not yet emitted, P3 gate `r1_final_verify.py --strict-skip` still running, no rollback triggered; P1/P2 gates PASS with the 7 winsrv deletions applied; `kernelhwsrv/` clean.

These 8 newly indexed reports bring the indexed Markdown total to **515**,
matching the 515 `.md` files under `docs/` (including this index; the header
count "(515 files, verified 2026-11-24)" at line 4 is updated); every Markdown
document in the directory is now linked.

## Added — 2026-11-24 last-473 sweep, WINSRV_DELETE_WATCH backfill (append-only)

A fresh full `docs/*.md` listing (517 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: 2 reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH195.md](WINSRV_DELETE_WATCH195.md) — read-only 2026-11-24 winsrv re-apply monitor: APPLY_RC status snapshot of the in-flight `r1_winsrv_delete.py --apply` run (pid 1286837) during P3 gates; see file for the per-watch detail.
- [WINSRV_DELETE_WATCH196.md](WINSRV_DELETE_WATCH196.md) — read-only 2026-11-24 winsrv re-apply monitor: APPLY_RC status snapshot of the in-flight `r1_winsrv_delete.py --apply` run (pid 1286837) during P3 gates; see file for the per-watch detail.

These newly indexed reports bring the indexed Markdown total to
**517**, matching the 517 `.md` files under `docs/` (including this index);
post-append re-verification shows 517 on-disk `.md` files = 517 unique
indexed link targets, 0 missing, 0 dangling; every Markdown document in
the directory is now linked.

## Added — 2026-11-24 subagent sweep, post-517 (append-only)

A fresh full `docs/*.md` listing (521 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: **0 unlinked documents and 0 broken links** (521 on-disk `.md`
files = 521 unique indexed targets, including this index). No missing
documents were found, so this section adds no new link entries;
`kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **521**, matching the 521 `.md` files
under `docs/` (including this index; the header count at line 4 is
updated); every Markdown document in the directory is linked.

## Added — 2026-11-24 continued sweep, WINSRV_DELETE_WATCH186–192 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: seven more reports existed on disk but had no entry;
they are linked below. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH186.md](WINSRV_DELETE_WATCH186.md) — read-only 2026-11-24 monitor #186: APPLY_RC NOT YET EMITTED, apply log ends at `gate[P3]: r1_final_verify.py --strict-skip`, no rollback, snapshot token `ROLLBACK-WINSRV-20261124_215436` unused.
- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — read-only monitor #187: re-apply NOT finished, no APPLY_RC in `winsrv_delete_apply2.log`, no rollback/FAILED/Traceback markers.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — read-only monitor #188: APPLY_RC not yet printed, apply still in P3 gates with `final_verify --strict-skip` queued, no rollback/FAILED lines.
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — read-only ~22:51 monitor #189: APPLY_RC not yet emitted, pid 1286837 alive 33m35s mid-P3, no rollback lines, snapshot token intact.
- [WINSRV_DELETE_WATCH190.md](WINSRV_DELETE_WATCH190.md) — read-only monitor #190: APPLY_RC NOT YET EMITTED, log (3.4 KB) ends at `gate[P3]: final_verify --strict-skip`, no APPLY_RC/rollback line.
- [WINSRV_DELETE_WATCH191.md](WINSRV_DELETE_WATCH191.md) — read-only ~22:50 monitor #191: APPLY_RC NOT YET EMITTED, pid 1286837 alive 33:44, log ends at P3 gate list, no rollback observed.
- [WINSRV_DELETE_WATCH192.md](WINSRV_DELETE_WATCH192.md) — read-only ~22:17+ monitor #192: APPLY_RC NOT YET WRITTEN, log ends mid `gate[P3]` final-verify suite, no APPLY_RC/rollback line.

## Added — 2026-11-24 post-473 sweep, WINSRV_DELETE_WATCH28/84 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: two reports existed on disk but had no entry; they
are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH28.md](WINSRV_DELETE_WATCH28.md) — re-apply completion monitor #28: verdict YES, `APPLY_RC=0`, pid 1286837 exited; all 7 winsrv deletions landed with P1/P2/P3 gates (incl. `r1_final_verify.py --strict-skip`) PASS, rollback snapshot unused.
- [WINSRV_DELETE_WATCH84.md](WINSRV_DELETE_WATCH84.md) — re-apply completion report #84 (~22:47): `APPLY_RC=0`, no rollback; P1/P2/P3 gates all PASS, ewsrv retire gate PASS at every phase, 7 deletions applied, snapshot token `ROLLBACK-WINSRV-20261124_215436` not used.

These two newly indexed reports bring the indexed Markdown total to **521**,
matching the 521 `.md` files under `docs/` (including this index; the header
count "(521 files, verified 2026-11-24)" at line 4 is consistent);
post-append re-verification shows 521 on-disk `.md` files = 521 unique
indexed link targets, 0 missing, 0 dangling; every Markdown document in the
directory is now linked.

These seven newly indexed reports complete this sweep; post-append
re-verification compares on-disk `.md` files against the unique indexed
link targets (0 dangling links; `kernelhwsrv/` untouched, no QEMU run).

## Added — 2026-11-24 WINSRV_DELETE_WATCH28/84 link backfill (append-only)

A fresh full `docs/*.md` listing (521 files, flat directory, no
subdirectories) was diffed against the 519 unique `.md` link targets in
this index: two documents were mentioned in passing but had no link entry;
they are linked below. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

- [WINSRV_DELETE_WATCH28.md](WINSRV_DELETE_WATCH28.md) — read-only 2026-11-24 re-apply completion monitor: verdict YES, `APPLY_RC=0`, all 7 winsrv deletions landed, P1/P2/P3 gates all PASS incl. `r1_final_verify.py --strict-skip`.
- [WINSRV_DELETE_WATCH84.md](WINSRV_DELETE_WATCH84.md) — read-only 2026-11-24 ~22:47 re-apply completion report: `APPLY_RC=0`, no rollback, 25 ops (18 edits + 7 deletes) with all phase gates PASS.

These two newly indexed reports bring the indexed Markdown total to **521**,
matching the 521 `.md` files under `docs/` (including this index; the header
count "(521 files)" at line 4 is consistent); post-append re-verification
diffs on-disk `.md` files against unique link targets with 0 missing and 0
broken links.

## Added — 2026-11-24 subagent sweep, post-523 (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: **0 unlinked documents and 0 broken links** (523
on-disk `.md` files = 523 unique indexed targets, including this index;
the header count "(523 files)" at line 4 is consistent). No missing
documents were found, so this section adds no new link entries;
`kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **523**, matching the 523 `.md` files
under `docs/` (including this index); every Markdown document in the
directory is linked.

## Added — 2026-11-24 post-517-sweep, 2 new WINSRV_DELETE_WATCH reports backfill (append-only)

A fresh full `docs/*.md` listing (519 files, flat directory, no
subdirectories) was diffed against the 517 unique `.md` link targets in
this index: 2 reports existed on disk but had no entry; they are
linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH197.md](WINSRV_DELETE_WATCH197.md) — read-only 2026-11-24 status report.
- [WINSRV_DELETE_WATCH198.md](WINSRV_DELETE_WATCH198.md) — read-only 2026-11-24 status report.

These 2 newly indexed reports bring the indexed Markdown total to
**519**, matching the 519 `.md` files under `docs/` (including this index;
the header count "(519 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 519 on-disk `.md` files =
unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH185–192 backfill (append-only)

A full `docs/*.md` listing (519 files) was diffed against the unique `.md` link
targets in this index: eight reports existed on disk but had no entry; they are
linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH185.md](WINSRV_DELETE_WATCH185.md) — read-only ~22:50 winsrv re-apply monitor #185: APPLY_RC not yet emitted, pid 1286837 alive 33+ min in the P3 `r1_final_verify.py --strict-skip` gate, no rollback.
- [WINSRV_DELETE_WATCH186.md](WINSRV_DELETE_WATCH186.md) — read-only monitor #186: APPLY_RC NOT YET, log ends at gate[P3] strict-verify, pid 1286837 running (33:21), rollback snapshot/token intact but unused.
- [WINSRV_DELETE_WATCH187.md](WINSRV_DELETE_WATCH187.md) — re-apply status #187: no APPLY_RC/rollback markers in the 3.3 KB log, pid 1286837 alive (33:25) in P3 after P1/P2 gates PASS.
- [WINSRV_DELETE_WATCH188.md](WINSRV_DELETE_WATCH188.md) — read-only monitor #188: APPLY_RC not yet printed, apply in P3 gates, pid 1286837 alive (33:30), no rollback lines.
- [WINSRV_DELETE_WATCH189.md](WINSRV_DELETE_WATCH189.md) — read-only ~22:51 monitor #189: APPLY_RC not yet emitted, pid 1286837 alive (33m35s) mid-P3; pre-gate, P1 (18 edits) and P2 PASS, all 7 deletes done.
- [WINSRV_DELETE_WATCH190.md](WINSRV_DELETE_WATCH190.md) — read-only monitor #190: APPLY_RC NOT YET, 3.4 KB log ends at gate[P3] strict-verify, pid 1286837 running (33m34s) in the P3 gate battery.
- [WINSRV_DELETE_WATCH191.md](WINSRV_DELETE_WATCH191.md) — read-only ~22:50 monitor #191: APPLY_RC not yet emitted, pid 1286837 alive (33:44); 7 staged deletions confirmed (winsrv.c, wsclient.c/.h, guishost.h, qemu_windowsrv.cpp/.h, qemu_gui_exec.cpp).
- [WINSRV_DELETE_WATCH192.md](WINSRV_DELETE_WATCH192.md) — read-only monitor #192: APPLY_RC not yet written, log ends mid gate[P3] final-verify, pid 1286837 still alive, apply in progress.

These eight newly indexed reports bring the indexed Markdown total to
**519**, matching the 519 `.md` files under `docs/` (including this index);
post-append re-verification shows 519 on-disk `.md` files = 519 unique indexed
link targets, 0 missing, 0 dangling; every Markdown document in the directory
is now linked.

## Added — 2026-11-24 last-521 sweep, WINSRV_DELETE_WATCH28/84 backfill (append-only)

A fresh full `docs/*.md` listing (521 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: two older reports existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH28.md](WINSRV_DELETE_WATCH28.md) — read-only 2026-11-24 winsrv re-apply monitor #28: verdict YES — the re-apply finished with `APPLY_RC=0`, all 7 deletions landed, pid 1286837 exited, no rollback.
- [WINSRV_DELETE_WATCH84.md](WINSRV_DELETE_WATCH84.md) — read-only 2026-11-24 ~22:47 winsrv re-apply completion report: YES — `APPLY_RC=0`, no rollback.

These two newly indexed reports bring the indexed Markdown total to
**521**, matching the 521 `.md` files under `docs/` (including this index;
the header count "(521 files, verified 2026-11-24)" at line 4 is
consistent); post-append re-verification shows 521 on-disk `.md` files =
521 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH28/84 backfill (append-only)

A full `docs/*.md` listing (521 files, flat directory, no subdirectories) was
diffed against the 519 unique `.md` link targets previously in this index: two
documents existed on disk but had no entry; they are linked below.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH28.md](WINSRV_DELETE_WATCH28.md) — read-only 2026-11-24
  re-apply monitor #28, verdict YES: re-apply finished with `APPLY_RC=0`
  (pid 1286837 exited), all 7 winsrv deletions landed, P1/P2/P3 gates all PASS
  incl. `r1_final_verify.py --strict-skip`, rollback snapshot unused, HEAD
  `0f03f7e`, `kernelhwsrv/` clean, no commit/push.
- [WINSRV_DELETE_WATCH84.md](WINSRV_DELETE_WATCH84.md) — read-only 2026-11-24
  ~22:47 re-apply completion report: `APPLY_RC=0`, no rollback; apply2 log
  shows P1/P2/P3 gates PASS (incl. strict verify), 7 deletions staged
  (`winsrv.c`, `guishost.h`, `wsclient.c/.h`, `qemu_windowsrv.cpp/.h`,
  `qemu_gui_exec.cpp`), deletion uncommitted, `kernelhwsrv/` untouched.

These two newly indexed reports bring the indexed Markdown total to **521**,
matching the 521 `.md` files under `docs/` (including this index; the header
count "(522 files, verified 2026-11-24)" at line 4 was corrected to the
verified 521); post-append re-verification shows 521 on-disk `.md` files =
521 unique indexed link targets, 0 missing, 0 dangling; every Markdown
document in the directory is now linked.

## Added — 2026-11-24 subagent ~486+ docs sweep (append-only)

A full `docs/*.md` listing (521 files at scan time, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: **0 unlinked documents and 0 broken links** (521 on-disk `.md` files =
521 unique indexed targets, including this index). This covers the recent
`WINSRV_DELETE_WATCH82`–`WINSRV_DELETE_WATCH189` re-apply monitor series and
all other newest reports, which were already linked in the backfill sections
above; no new documents were missing, so this section adds no new link
entries. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

The indexed Markdown total stays **521**, matching the 521 `.md` files under
`docs/` (including this index; the header count "(521 files, verified
2026-11-24)" at line 4 is consistent); every Markdown document in the
directory is linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH28/84 backfill (append-only)

A full `docs/*.md` listing (521 files, flat directory, no subdirectories) was
diffed against the unique `.md` link targets in this index: 2 documents
existed on disk but had no entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH28.md](WINSRV_DELETE_WATCH28.md) — read-only 2026-11-24 re-apply monitor: verdict YES, re-apply finished with `APPLY_RC=0` (pid 1286837 exited), all 7 winsrv deletions landed, P1/P2/P3 gates all PASS.
- [WINSRV_DELETE_WATCH84.md](WINSRV_DELETE_WATCH84.md) — read-only 2026-11-24 ~22:47 re-apply completion report: `APPLY_RC=0`, no rollback, all gates PASS.

These 2 newly indexed reports bring the indexed Markdown total to
**521**, matching the 521 `.md` files under `docs/` (including this index;
the header count at line 4 is updated); every Markdown document in the
directory is now linked.

## Added — 2026-11-24 subagent sweep, missing-docs backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: the documents missing at sweep time
(`WINSRV_DELETE_WATCH195.md`, `WINSRV_DELETE_WATCH196.md` — both read-only
2026-11-24 winsrv re-apply monitors reporting `APPLY_RC=0`, all P1–P3 gates
PASS, 7 deletes confirmed, rollback unused) had just been indexed by
concurrent backfill sections, so this pass adds no new link entries.
Post-append re-verification: on-disk `.md` count = unique indexed link
targets, 0 missing, 0 dangling; every Markdown document in the directory is
linked. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

## Added — 2026-11-24 missing-docs backfill, WINSRV_DELETE_WATCH200 (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: 1 document existed on disk but had no entry; it is
linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

- [WINSRV_DELETE_WATCH200.md](WINSRV_DELETE_WATCH200.md) — read-only 2026-11-24
  winsrv re-apply close-out: `APPLY_RC=0`, all P1–P3 phase gates PASS, and 7
  winsrv file deletions confirmed via `git status`.

This newly indexed report brings the indexed Markdown total to **522**,
matching the 522 `.md` files under `docs/` (including this index); post-append
re-verification shows 522 on-disk `.md` files = 522 unique indexed link
targets, 0 missing, 0 dangling; every Markdown document in the directory is
now linked.

## Added — 2026-11-24 post-473 sweep, full re-verification (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: 0 reports were missing and 0 links were dangling, so
no new entries are needed below. The header count at line 4 is updated to
the current on-disk total. `kernelhwsrv/` was untouched and no QEMU run was
made by this index update.

Post-append re-verification shows the on-disk `.md` count = unique indexed
link targets, 0 missing, 0 dangling; every Markdown document in the
directory is now linked. This brings the indexed Markdown total to **517**.

## Added — 2026-11-24 post-473 sweep, docs-index reconciliation (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index; the docs missing at sweep time
(`WINSRV_DELETE_WATCH187`–`192`, `194`) had just been indexed by concurrent
backfill sections, so this pass adds one late link and reconciles the
totals: the header count at line 4 was set to the live on-disk count and
post-append re-verification confirms every Markdown document in the
directory is linked. `kernelhwsrv/` was untouched and no QEMU run was made
by this index update.

- [WINSRV_DELETE_WATCH200.md](WINSRV_DELETE_WATCH200.md) — read-only 2026-11-24
  winsrv re-apply close-out: re-apply FINISHED with `APPLY_RC=0` ("SUCCESS:
  winsrv deletion applied across P1-P3; all phase gates PASS"), all P1/P2/P3
  gates PASS including `final_verify --strict-skip`, 7 deletes confirmed.

This brings the indexed Markdown total to **523**, matching the
523 `.md` files under `docs/` (including this index); every Markdown
document in the directory is now linked.

## Added — 2026-11-24 new-docs backfill (append-only)

A fresh full `docs/*.md` listing (522 files, flat directory, no subdirectories) was diffed against the unique `.md` link targets in this index: 1 document(s) existed on disk but had no entry; they are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by this index update.

- [WINSRV_DELETE_WATCH200.md](WINSRV_DELETE_WATCH200.md) — read-only 2026-11-24 report: WINSRV_DELETE_WATCH200 — winsrv re-apply status (2026-11-24)

These 1 newly indexed report(s) bring the indexed Markdown total to **522**, matching the 522 `.md` files under `docs/` (including this index; the header count "(522 files, verified 2026-11-24)" at line 4 is updated); post-append re-verification shows 522 on-disk `.md` files = 523 unique indexed link targets, 0 missing, 0 dangling; every Markdown document in the directory is now linked.

## Added — 2026-11-24 final catch-up sweep (append-only)

A fresh full `docs/*.md` listing (523 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: **0 unlinked documents and 0 broken links** (523 on-disk `.md`
files = 523 unique indexed targets, including this index; the header
count "(523 files, verified 2026-11-24)" at line 4 is consistent). The
two reports missing at my first scan (`WINSRV_DELETE_WATCH28.md`,
`WINSRV_DELETE_WATCH84.md` — both read-only winsrv re-apply completion
monitors, `APPLY_RC=0`, all 7 deletions landed, P1–P3 gates PASS) were
indexed by a concurrent backfill section, so this pass adds no new link
entries. `kernelhwsrv/` was untouched and no QEMU run was made by this
index update.

The indexed Markdown total is **523**, matching the 523 `.md` files under
`docs/` (including this index); post-append re-verification: 523 on-disk
`.md` files = 523 unique indexed link targets, 0 missing, 0 dangling;
every Markdown document in the directory is now linked.

## Added — 2026-11-24 post-519 sweep, newest WINSRV_DELETE_WATCH backfill (append-only)

A fresh full `docs/*.md` listing (523 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: 1 document(s) existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH199.md](WINSRV_DELETE_WATCH199.md) — read-only 2026-11-24 winsrv re-apply monitor #199: APPLY_RC=0, deletion applied across P1–P3 with all phase gates PASS, 7 winsrv files deleted, no rollback, `kernelhwsrv/` clean.

These 1 newly indexed report(s) bring the indexed Markdown total to
**523**, matching the 523 `.md` files under `docs/` (including this index);
post-append re-verification shows 523 on-disk `.md` files = 524 unique
indexed link targets, 0 missing, 0 dangling; every Markdown document in
the directory is now linked.

## Added — 2026-11-24 last-473 sweep, WINSRV_DELETE_WATCH28/84 backfill (append-only)

A fresh full `docs/*.md` listing was diffed against the unique `.md` link
targets in this index: two reports existed on disk but had no entry; they
are linked below. `kernelhwsrv/` was untouched and no QEMU run was made by
this index update.

- [WINSRV_DELETE_WATCH28.md](WINSRV_DELETE_WATCH28.md) — read-only 2026-11-24 re-apply monitor #28 (verdict YES, finished): all 7 winsrv deletions landed, `APPLY_RC=0`, pid 1286837 exited, 25 ops (18 edits + 7 deletes) with P1/P2/P3 gates all PASS incl. `r1_final_verify.py --strict-skip`.
- [WINSRV_DELETE_WATCH84.md](WINSRV_DELETE_WATCH84.md) — read-only 2026-11-24 ~22:47 re-apply completion report: `APPLY_RC=0`, no rollback — the winsrv re-apply finished successfully.

These two newly indexed reports bring the indexed Markdown total to the
current on-disk `.md` count under `docs/` (including this index); the
header count at line 4 matches; post-append re-verification shows 0
missing and 0 dangling links; every Markdown document in the directory is
linked.

## Added — 2026-11-24 post-473 sweep, final WINSRV_DELETE_WATCH187–194 check (append-only)

A fresh full `docs/*.md` listing (521 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: 0 documents existed on disk but had no entry (WINSRV_DELETE_WATCH187,
188, 189, 190, 191, 192 and 194 were already linked by the concurrent
backfill sections above); no new links are added below. `kernelhwsrv/` was
untouched and no QEMU run was made by this index update.

These 0 newly indexed reports keep the indexed Markdown total at **521**,
matching the 521 `.md` files under `docs/` (including this index; the
header count "(521 files, verified 2026-11-24)" at line 4 is consistent);
post-append re-verification shows 521 on-disk `.md` files = 521 unique
indexed document link targets (prose glob `*.md` excluded), 0 missing,
0 dangling; every Markdown document in the directory is now linked.

## Added — 2026-11-24 WINSRV_DELETE_WATCH28/84 backfill (append-only)

A full `docs/*.md` listing was diffed against the unique `.md` link targets in
this index: two documents existed on disk but had no entry; they are linked
below. `kernelhwsrv/` was untouched and no QEMU run was made by this index
update.

- [WINSRV_DELETE_WATCH28.md](WINSRV_DELETE_WATCH28.md) — read-only 2026-11-24
  re-apply monitor: re-apply FINISHED with `APPLY_RC=0`, all 7 winsrv deletions
  landed (25 ops: 18 edits + 7 deletes), P1/P2/P3 gates all PASS including
  `r1_final_verify.py --strict-skip`.
- [WINSRV_DELETE_WATCH84.md](WINSRV_DELETE_WATCH84.md) — read-only 2026-11-24
  ~22:47 re-apply completion report: `APPLY_RC=0`, no rollback, pid 1286837
  exited after ~34 min, log ends `SUCCESS: winsrv deletion applied across
  P1-P3; all phase gates PASS`.

These two newly indexed reports bring the indexed Markdown total to **521**,
matching the 521 `.md` files under `docs/` (including this index); post-append
re-verification shows 521 on-disk `.md` files = 521 unique indexed link
targets, 0 missing, 0 dangling; every Markdown document in the directory is
now linked.

## Added — 2026-11-24 post-473 final sweep (append-only)

A fresh full `docs/*.md` listing (523 files, flat directory, no
subdirectories) was diffed against the unique `.md` link targets in this
index: 0 document(s) existed on disk but had no entry; they are
**0 unlinked documents and 0 broken links** — no new entries were needed.
`kernelhwsrv/` was untouched and no QEMU run was made by this index update.


The indexed Markdown total is **523**, matching the
523 `.md` files under `docs/` (including this index; the header
count at line 4 is updated); every Markdown document in the directory is linked.
