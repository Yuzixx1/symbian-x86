# Card 34 — R1-B toolchain blocker: bx86gcc EFSRV def/import library

## Status
Offered, not launched. Research only — no target build, no QEMU, no guest execution.

## Owned paths
New `qemu-port/docs/r1_toolchain_blocker_v1.md` only. Read `r1_toolchain_research_v0.md`, `r1_build_repro_v0.md`, `r1_build_repeat_v1.md`, `r1_sdk_route_research_v0.md`.

## Problem (from card 30 evidence)
Card 30 built `t_cactw.exe` and `fhserver.exe` at build level via Raptor 2.15.3 / MinGW GCC 3.4.5 under Wine / sbs 2.15.3, but the **local bx86gcc EFSRV def/import library is missing**. Import ordinals were verified against `bx86gcc/euseru.def` for EUSER and `eabi/efsrvu.def` for EFSRV, but the actual import-library artifacts for EFSRV were not confirmed present in the local toolchain. Deviations D1–D8 from `r1_build_repro_v0.md` (Raptor source patches D1/D2, fabricated variant header D5, genexec D8) still require audit.

## Task
Determine a build path for unchanged upstream x86 proof apps (`t_cactw.exe`, `fhserver.exe` per card 30 approval):

1. Audit `/home/yuzi/.cache/sybian-r1/r1-build-v1/` and `r1-build-v1-repeat/` for EFSRV import library / def files present in clean build artifacts.
2. Audit pinned providers under `/home/yuzi/.cache/sybian-r1/` for `eabi/efsrvu.def` and corresponding `.lib`/import library.
3. If missing, determine whether new local provisioning is needed (and document exact inputs/hashes), or whether the clean-build artifacts are sufficient for linking proof apps.
4. Verify no bx86/bx86gcc ordinal mixing: EUSER via `bx86gcc/euseru.def`, EFSRV via `eabi/efsrvu.def`, never bx86 MSVC defs spliced in.
5. Record exact missing inputs, present inputs, and recommended build path for card 35 onward.

## Deliverable
Report `qemu-port/docs/r1_toolchain_blocker_v1.md` with:
- Present/missing EFSRV def/import library inputs and hashes
- D1–D8 audit status (accepted/rejected/needs worker correction)
- Recommended build path for `t_cactw`/`fhserver` linking
- Any coordinator decisions needed

## Constraints
- No target build, no QEMU, no guest execution.
- No upstream edits, no commits/pushes.
- Isolated home-backed worktree; preserve all prior build roots.
- Label source evidence vs proposals.

## Exclusivity
No shared files touched. Independent of card 35.
