# R1-FS-CHANNEL-MAKEFILE — card 27 integration

2026-10-28. Card 27 owns `qemu-port/Makefile`, `variant/r1_fs_channel.{h,cpp}`,
`variant/qemu_variant.h` storage declarations only, `fs/r1_fs_*.{h,cpp}`,
`tests/r1_fs_channel/`, and this document. No upstream changes, commits/pushes,
installs, redistribution or physical hardware. Card 27 owns the serialized QEMU
slot; no other VM suite ran concurrently.

## What was wired into Makefile

An explicit, off-by-default guard so plain `make` builds all ISOs unchanged
without the channel:

```
# R1 private FS channel — explicit opt-in, off by default.
R1_FS_CHANNEL ?= 0
R1_FS_CHANNEL_OBJ :=
ifeq ($(strip $(R1_FS_CHANNEL)),1)
R1_FS_CHANNEL_OBJ := build/r1_fs_channel.o
build/r1_fs_channel.o: variant/r1_fs_channel.cpp variant/r1_fs_channel.h \
 fs/r1_fs_channel.h fs/r1_fs_channel_state.h fs/r1_fs_wire_logic.h \
 variant/r1_fat_offset.h | build
	$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/r1_fs_channel.cpp -o $@
endif
KERNFULL_OBJS += $(R1_FS_CHANNEL_OBJ)
```

- Default `make` (guard off): `build/r1_fs_channel.o` is not built or linked;
  `kernfull.elf` and `symbian-grub.iso` are byte-identical to the pre-card-27
  build (8634368 bytes), rc=0.
- `make R1_FS_CHANNEL=1 build/r1_fs_channel.o`: compiles the private kernel
  channel unit (`variant/r1_fs_channel.cpp`) with the exact genuine kernel
  flags (`KERN_FLAGS`/`KERN_DEFS`/`KERN_INCLUDES`, `-m32`, `__IN_KERNEL__`):
  rc=0, 0 errors (only pre-existing kernel-header `-Winvalid-offsetof`
  warnings).

The channel is deliberately **not linked** into a default `kernfull.elf`:
`R1Fat::*` backend calls are declared but not defined in this tree, so a guest
link would fail on undefined `R1Fat::Open/Size/Read/Write/Resize/Flush/Close`.
Guest service registration (`portfilesrv.exe` as trusted pinned host) and the
R1Fat backend are out of scope for card 27 and require card-29 host launch
policy plus separate backend integration.

## Files integrated (copied unchanged from card-18/card-23 source)

- `fs/r1_fs_channel.{h,cpp}` — fixed wire `Packet`/`Transport`, `BackendCore`
- `fs/r1_fs_channel_eka.{h,cpp}` — EKA `ChannelBackend`
- `fs/r1_fs_channel_state.h` — portable `ChannelState` + `RunRequest`; uses
  `R1FsChannelSize` (`TUint32` in-kernel, `uint32_t` host) for `size`/`after`
  locals so the kernel `TUint32&` size out-parameter binds; `done` stays
  `uint32_t`
- `fs/r1_fs_wire_logic.h` — `ValidPath`/`ValidRequest`/`PrepareResponse`/
  `StorageUncertain`/`MalformedTransfer`
- `variant/r1_fs_channel.{h,cpp}` — kernel LDD: `R1FsChannelPin/Revoke/Install`,
  generation-bearing keys, new-key cleanup on failed reply copyout
- `tests/r1_fs_channel/` — wire/state/adapter host tests
- Hardened replacements: `fs/r1_fs_backend.h`, `fs/r1_fs_core.h`,
  `fs/r1_fs_core.cpp`

## Verification

Kernel unit compile: rc=0.

Host suites (strict `-std=c++98 -Wall -Wextra -Werror -pedantic -I fs`):
- `R1 private channel wire bounds PASS`
- `R1 channel state and cleanup PASS`
- `R1 channel adapter PASS` (links `fs/r1_fs_channel.cpp`)

Portable core suite:
- `R1-FS-CORE portable decoder/state PASS`
  (`fs/r1_fs_core.cpp` + `tests/r1_fs_core/core_test.cpp`)

Stable suite in the authorized serialized QEMU slot, **assets absent first**:
- Wall clock: 17:52:38–18:01:25 (~8m47s), 2026-10-28
- build: PASS (asset-free default build)
- Existing 15 checks: all PASS
  (e32_relocs, args, dll, data_reloc, userland, ipc, cli, cli_diskfull, crash,
  conc, interact, sdk, samples, wait, chunk)
- `SKIP test_r1_g_boot: R1_G_ASSET_DIR not set` — honest skip, never counted
  as a pass; no test weakening
- `grep 'FAULT:\|Exc '` guest transcripts: not applicable (assets absent,
  G not executed in guest)
- Logs: `/tmp/opencode/stable_run_card27.log` and
  `/tmp/opencode/stabilization/*.log` (copied to
  `/home/yuzi/.cache/sybian-r1/r1-card27-stable-20261028/`)

## Service smoke — NOT-RUN

Card 27 spec: run legacy FS/channel service smoke through the serial shell
only if the card-29 host launch policy exists; otherwise mark not-run.

Card 29 (`R1-PRIVILEGED-HOST-POLICY`) is **source/review only**:
`qemu-port/docs/r1_privileged_host_policy_v1.md` declares the intended
`portfilesrv.exe` trusted-host policy but states "No implementation, build,
QEMU or runtime" and allocates no QEMU slot. No implemented host launch
policy or service registration exists in this tree. Therefore the FS/channel
service smoke is **NOT-RUN**, not skipped-as-pass and not executed.

No guest QEMU run beyond the asset-absent stable suite; no leftover
`qemu-system`/`test_stable.py` processes.

## Tree state

Uncommitted (no commits/pushes): `qemu-port/Makefile` (guard wiring), new
`qemu-port/docs/r1_fs_channel_integration_v1.md`, plus previously integrated
`fs/r1_fs_*`, `variant/r1_fs_channel.*`, `tests/r1_fs_channel/*` and
hardened `fs/r1_fs_backend.h`, `fs/r1_fs_core.{h,cpp}` from card 25.
