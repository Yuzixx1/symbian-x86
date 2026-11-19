# R1-FS-CHANNEL-HARDEN review v1 (card 23)

## Scope and ownership
Card 23 continues in the card-21 worktree. Owned files: `qemu-port/fs/r1_fs_channel.{h,cpp}`,
`qemu-port/fs/r1_fs_channel_eka.{h,cpp}`, `qemu-port/fs/r1_fs_channel_state.h`,
`qemu-port/fs/r1_fs_wire_logic.h`, `qemu-port/variant/r1_fs_channel.{h,cpp}`,
`qemu-port/tests/r1_fs_channel/`, and this document. Narrow alignment edits to
`fs/r1_fs_backend.h` and `fs/r1_fs_core.h` only (error enum moved to the backend
header). No storage/card-18 files, shared headers or Makefile changes beyond the
MMP source list.

## Failed-Open-reply cleanup
`Dispatch(Open)` allocates a slot and generation before the reply is copied out.
`RunRequest` (the orchestration actually used by the kernel LDD) records
`newKey` only when `op==Open && status==Success`. If the post-dispatch
authorization recheck fails or `Kern::ThreadRawWrite` fails, it calls
`ChannelState::AbandonNewKey` under `UserFilesLock`. `AbandonNewKey` closes only
the exact slot+generation; a reused slot has a different generation, so a stale
reply cannot close a live key. The directory entry created by `Open` is not
rolled back; the original error is preserved.

## Deterministic response fields
`PrepareResponse` (called by `RunRequest` after `ValidRequest`) clears
`status/done/size/stable/quarantined` and, for Open, `slot/generation`. No
client-supplied output field can survive as a valid reply.

## 32-bit wire layout
`Packet` is 4276 bytes: status at offset 48, path at 52, data at 180. Static
asserts in `r1_fs_channel.h` (GCC) and runtime asserts in `wire_test.cpp`
enforce this. The same header is compiled by the host tests, the MinGW 32-bit
user translation unit and the kernel `-m32` translation unit, so all three agree.

## Kernel lifetime / CS / destruction audit
- `DoCreate`/`Create` run in a critical section with the kernel unlocked;
  `Authorized` now uses an atomic pointer load/store instead of `LockSystem`,
  so it is safe with `UserFilesLock` held and with the system lock state of
  the caller.
- `Request` allocates the packet under `ThreadEnterCS` (matching genuine
  `btracex.cpp`), reads the user packet, then calls `RunRequest`, which never
  holds `UserFilesLock` across `ThreadRawRead/Write`.
- `~Channel` runs from `K::ObjDelete` in thread context with the kernel
  unlocked and a critical section held; it takes `UserFilesLock` and releases
  every token. `R1Fat::Close` is a token release only.
- `R1FsChannelInstall` matches genuine `DoLogicalDeviceLoad`: on failure it
  calls `Close(0)` on the device; on success it does not touch the device.
- `R1FsChannelPin`/`Revoke` use `DObject::Open/Close` under a critical
  section and an atomic pointer swap; revoke is documented prepublication-only.

## EFSRV framing review
Ordinary `RFile` position is scalar slot 2; `RFile64` position is an 8-byte
descriptor marked by the slot-2 flag. Seek output is slot 2, FileSize output
slot 0, create reply slot 3. Multi-chunk reads/writes cap each `RMessage2`
transfer to the chunk and offset it by bytes consumed; zero-length and partial
EOF are supported. Async queue and cancellation remain explicitly unsupported.

## Explicitly unsupported
`ReplaceExisting`, async read/cancel, non-ASCII paths, transfers above 4096
bytes, offsets/sizes above 32 bits, sysbin mutation, loader authority.

## Evidence
- Host: `R1 private channel wire bounds PASS`, `R1 channel state and cleanup
  PASS`, `R1 channel adapter PASS`.
- Previous user/kernel compile-only objects remain valid for the unchanged
  EKA plumbing; the LDD change is compile-only and was not runtime-tested.

## Card 25 — integration into main (2026-10-28)
The hardened source was copied into the main tree unchanged; it is deliberately
not wired into `Makefile`, so default/ISO builds and the stable suite are
unaffected:

- `fs/r1_fs_channel.{h,cpp}`, `fs/r1_fs_channel_eka.{h,cpp}`,
  `fs/r1_fs_channel_state.h`, `fs/r1_fs_wire_logic.h`
- `variant/r1_fs_channel.{h,cpp}` (private kernel unit)
- `tests/r1_fs_channel/` (wire/state/adapter host tests)
- `fs/r1_fs_backend.h`, `fs/r1_fs_core.h`, `fs/r1_fs_core.cpp` (hardened
  versions; error enum now lives in the backend header)

One portability edit was required by the genuine kernel build: in
`fs/r1_fs_channel_state.h` the `size`/`after` locals use
`R1FsChannelSize`, which is `TUint32` (`unsigned long`) under `__IN_KERNEL__`
and `uint32_t` otherwise. Without it, the kernel `TUint32&` size out-parameter
cannot bind to a `uint32_t` local. `done` stays `uint32_t` because `Read`/
`Write` take `TUint&`. Values are unchanged for host builds.

Verification on this tree:
- Kernel unit compiles with the exact `KERN_FLAGS`/`KERN_DEFS`/`KERN_INCLUDES`
  used for `variant/e32load.cpp` plus `-I./inc -I./build/export` (`-m32`,
  genuine kernel headers); no errors.
- Host tests rebuilt from source and re-run: `R1 private channel wire bounds
  PASS`, `R1 channel state and cleanup PASS`, `R1 channel adapter PASS`
  (`adapter_test` links `fs/r1_fs_channel.cpp`).
- `python3 tools/test_stable.py`: build + 15 checks PASS, G honestly
  `SKIP` without assets and `PASS` with staged assets (see
  `docs/r1_g_runtime_v1.md`).

The channel is still compile/host-tested only: no EFSRV/LDD runtime wiring, no
guest exercise, and no file-mode, publication or security-bypass behaviour.
