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

These six newly indexed guides bring the indexed Markdown total to **131**,
matching the 131 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 audit / precommit backfill (append-only)

Three audit/prep reports existed on disk but had no entry in this index. They
are linked below. `EFILE_REENTRY_FIX.md` was checked but does not exist on disk,
so it has no entry; `EFILE_ACCEPTANCE.md` was already indexed. `kernelhwsrv/`
was untouched and no QEMU run was made by this index update.

- [SECURITY_AUDIT.md](SECURITY_AUDIT.md) — static pre-commit security audit of the pivot files (`tools/*.py`, `genuine/**`, changed `variant/`+`fs/`); verdict SAFE with zero real findings, no secrets and no command injection, `kernelhwsrv/` read-only/untouched.
- [PRECOMMIT_AUDIT.md](PRECOMMIT_AUDIT.md) — prep-only, read-only pre-commit audit/checklist for the genuine-userspace pivot: NO-GO verdict (EFSRV/F32 gate FAIL, EFILE trampoline fix pending/unproven), ordered verification commands, explicit stage-by-path guidance (never `git add -A`) and a do-not-commit list.
- [EFILE_DUPLICATE_LAUNCH_PLAN.md](EFILE_DUPLICATE_LAUNCH_PLAN.md) — read-only corrected root-cause analysis of the 3x EFILE start signature (one process, three `app_main` reentries) plus a port-side single-owner guard/trampoline experiment in `variant/e32load.cpp`, with IPC-vs-1x-start trade-off and retire-gate results.

These three newly indexed reports bring the indexed Markdown total to **134**,
matching the 134 `.md` files under `docs/` (including this index); every
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
matching the 135 `.md` files under `docs/` (including this index); every
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

These two newly indexed reports bring the indexed Markdown total to **137**,
matching the 137 `.md` files under `docs/` (including this index); every
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

These nine newly indexed reports bring the indexed Markdown total to **146**,
matching the 146 `.md` files under `docs/` (including this index); every
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

These four newly indexed reports bring the indexed Markdown total to **150**,
matching the 150 `.md` files under `docs/` (including this index); every
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
matching the 151 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.

## Added — 2026-11-24 DOOM post-UID verification backfill (append-only)

One report existed on disk but had no entry in this index; it is linked below.
Of the named candidates, `EFILE_REENTRY_SUPPRESS.md` was checked and does not
exist on disk, so it has no entry. `kernelhwsrv/` was untouched and no QEMU run
was made by this index update.

- [DOOM_POST_UID_VERIFICATION.md](DOOM_POST_UID_VERIFICATION.md) — verification-only DOOM/Freedoom run against the current 03:15 staged ISO after the genuine-userspace/UID changes (eshell UID re-stamp, guard-only EFILE, PS shim, trampoline removal): all four tests (main, audio, playtest past 455-frame cap, 8/8 multikey) PASS, `EKA2_CRISP_STOP` absent from code, ord-915 bind rebased, zero Exc/Panic/FAULT/#PF; no rebuild, commit or push, `kernelhwsrv/` untouched.

This newly indexed report brings the indexed Markdown total to **152**,
matching the 152 `.md` files under `docs/` (including this index); every
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

These four newly indexed reports bring the indexed Markdown total to **156**,
matching the 156 `.md` files under `docs/` (including this index); every
Markdown document in the directory is now linked.
