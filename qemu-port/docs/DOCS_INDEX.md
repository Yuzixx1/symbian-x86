# Documentation index — `qemu-port/docs/`

This is the comprehensive index of every Markdown document in this directory
(92 files). For a top-down description of how the OS actually works, start with
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
