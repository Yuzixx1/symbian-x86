# R1 decision record — owner sign-off needed at policy level only

Date: 2026-10-28. This file exists so you do not need to review technical
ordinals, MMPs, or toolchain patches. Decide at the policy level:
**approve this plan, ask for changes, or say stop.**

## Decisions workers already made (for your awareness, not your approval)

| Decision | Who made it | Evidence |
|---|---|---|
| R1-B proof apps proposed: `t_cactw.exe` (scheduler) + `fhserver.exe` (file I/O) | Card 30 worker | Read from upstream `kernelhwsrs`, verified zero-capability MMPs |
| R1 file service host: `portfilesrv.exe`, B0–B4 phases, `FsRegisterThread` once | Card 31 worker | Verified against genuine `sexec.cpp` / `sutils.cpp` |
| Genuine G heap/scheduler runs in QEMU | Card 25 | Stable suite 15/15 PASS, asset-backed G PASS |
| Shell supervisor stack raised `0 → 0x8000` | Card 25 | Eliminated `KERN 0x62` fault |
| Console HAL/EKDATA assets staged (not console acceptance) | Card 26 | Byte-identical images, three EKDATA exports verified |

## What needs your high-level sign-off (one decision, not technical)

### Approve R1-B proof app selection?

The worker proposes two unchanged upstream apps:
- **`t_cactw.exe`** — proves active scheduling (CActiveScheduler/Timer)
- **`fhserver.exe`** — proves file read/write (RFs/RFile via EFSRV)

Both are unprivileged (zero capabilities), both come from upstream source. The worker recorded a build plan and import evidence.

**Your choice:** Yes, use these as the R1-B proof apps / No, pick different ones / Stop R1-B here and declare R1 complete at QEMU-ready only.

If yes, the next steps require:
1. Resolving a toolchain blocker (no local bx86gcc EFSRV def/import library — documented in `r1_toolchain_research_v0.md`)
2. Building `portfilesrv.exe` host (card 31 declarations exist, not yet built)
3. Running the file-service regression in QEMU (card 32)
4. Your physical-PC smoke test (card 33 — you, on the reference PC)

### Or declare R1 complete at QEMU-ready?

If you do not want to pursue R1-B unchanged-app compatibility, R1 can stop here:
- Genuine heap + scheduler proven in QEMU ✅
- Stable suite 15/15 PASS ✅
- File service source complete but not guest-tested ⚠️
- Physical-PC smoke test not executed ❌

R1 would be QEMU-ready but **not milestone complete** per `project_roadmap.md`.

## What you do NOT need to decide

- bx86 vs bx86gcc ordinals
- MMP file contents
- Petran/Raptor build deviations D1–D8
- Import library deficiencies
- Console route details for `t_cactw`

Those are worker/coordinator problems. They are documented in the reports linked above.

## Where this lives

This file: `qemu-port/docs/r1_decision_record_v1.md`

No commits or pushes were made. The tree is unchanged except for the implementation files already integrated (cards 25–29).
