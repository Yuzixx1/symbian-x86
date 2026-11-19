# R1 runtime dispatch — cards 27–29

All cards 22–26 returned. 2026-10-28; these are offers, not launched agents.
Strong workers recommended. Use isolated home-backed worktrees with explicit
current dirty baseline. No upstream changes, commits/pushes, installs,
redistribution or cleanup. Only card 27 owns the serialized QEMU slot; no other
VM suite may run concurrently. No physical hardware authorization here.

## 27 — R1-FS-CHANNEL-MAKEFILE (strong; exclusive QEMU/stable integration)

Own `qemu-port/Makefile`, `variant/r1_fs_channel.{h,cpp}`, `variant/qemu_variant.h`
storage declarations only, `fs/r1_fs_*.{h,cpp}`, `tests/r1_fs_channel/`, and new
`docs/r1_fs_channel_integration_v1.md`. Wire card-18/card-23 source into
`Makefile` under an explicit guard so default `make` builds all ISOs unchanged
without assets; the service is host-visible only after bootstrap integrator
pins/authenticates `portfilesrv.exe` at B0/B2 (separate card-29 policy).

Authenticate the pinned service `DProcess` object on open, transfer and every
request; SID/name/capabilities alone do not grant access. Keep new-key cleanup
on failed reply copyout, deterministic response fields, generation-bearing keys,
separate transport/storage quarantine, and uncertain-mutation failure closed.
Recheck `ThreadRawRead/Write` preconditions and channel lifetime against genuine
EKA kernel source. No `E32Loader` executive via channel. No sysbin path or raw
kernel object in wire protocol.

Compile private kernel channel unit; run host suites (`wire bounds`,
`state and cleanup`, `adapter`) and portable core suite. Then run `tools/test_stable.py`
sequentially in the authorized slot with assets absent first (expect `SKIP`
test_r1_g_boot unchanged) and legacy FS/channel service smoke visible through
the serial shell only if the card-29 host launch policy exists—otherwise mark
service smoke as not-run. Existing 15 checks must remain PASS; no test weakening.
Capture exact timings, skips and any fault transcript. Return early if a blocker
appears. No physical hardware, commits or pushes.

## 28 — R1-CONSOLE-G-MANIFEST (strong; host/build only)

Own `tools/stage_r1_g.py` edits, `qemu-port/r1-console/hal/hal_asset.manifest`,
`qemu-port/docs/r1_console_staging_v1.md`, and new
`qemu-port/docs/r1_console_g_manifest_v1.md`. Wire accepted HAL/EKDATA v3 hashes
into stage source so asset staging copies byte-identical `HAL.DLL`,
`EKDATA.DLL` and `EKDATA.00` from the verified staging directory. Default builds
remain asset-free; no redistribution permission.

Verify manifest hash match against card 24/26 evidence before writing. Document
exact sysbin placement, immutable image, UID/SID/caps and zero-data requirements
for future G manifest. No HAL/EKDATA logic changes, no rebuild/link, no QEMU,
no console-closure claim. Stop and report any evidence mismatch.

## 29 — R1-PRIVILEGED-HOST-POLICY (strong; source/review only)

Own new `qemu-port/docs/r1_privileged_host_policy_v1.md` and reviewed shared
host/bootstrap declarations. Resolve how `portfilesrv.exe` becomes the trusted
registered host without removing the bootstrap zero-cap-root guard.

Specify: pinned image provisioning/hash verification, single-process `!FileServer`
and later `!Loader` thread ownership, B0/B2/B3/B4 phase gating from
`r1_service_boundary_v1.md`, `UserSvr::FsRegisterThread` exactly once by the FS
owner thread, process/thread system-permanent flags from genuine
`sexec.cpp`, explicit death-is-failure-not-restart after registration, and the
minimum trusted-host capabilities justified from source (not all caps). Define
how the G kernel adapter binds the static EUSER/EFSRV fixture imports before B3
without `!Loader`, and how dynamic library publication remains `KErrNotSupported`
until genuine client-handle rollback is implemented and reviewed.

No implementation, build, QEMU or runtime. Return concrete declarations and the
exact remaining decisions for coordinator integration; no broad architecture
inventory. No commits/pushes. No QEMU slot allocated.
