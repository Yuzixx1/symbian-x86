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
