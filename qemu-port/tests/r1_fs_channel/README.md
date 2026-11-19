# R1-FS-FAT-CHANNEL private handoff (card 23)

Worktree: `/home/yuzi/.cache/sybian-r1/r1-fs-core-worktree`. Uncommitted, not
integrated or launched. No service registration, target link, IPC execution,
QEMU run or hardware result is claimed.

## Owned sources

- `fs/r1_fs_channel.h` — fixed wire `Packet`, `Transport`, portable `BackendCore`.
- `fs/r1_fs_channel.cpp` — `BackendCore` implementation + `Encode`.
- `fs/r1_fs_channel_eka.h` / `fs/r1_fs_channel_eka.cpp` — EKA `ChannelBackend`.
- `fs/r1_fs_channel_state.h` — portable `ChannelState` + `RunRequest`.
- `fs/r1_fs_wire_logic.h` — `ValidPath`, `ValidRequest`, `PrepareResponse`,
  `StorageUncertain`, `MalformedSuccess`, `MalformedTransfer`, `Uncertain`.
- `variant/r1_fs_channel.{h,cpp}` — kernel LDD (pin/revoke/install).
- `fs/r1_fs_core.mmp` — proposes the two user translation units.

## Card 23 corrections

- Failed-Open-reply cleanup: `Dispatch(Open)` may publish a key before the
  reply is copied out. `RunRequest` closes only the newly acquired,
  generation-matched key when the post-dispatch authorization check or the user
  copy fails. The original transport/authorization error is preserved; no
  directory entry is rolled back.
- Deterministic response fields: `PrepareResponse` clears `status/done/size/
  stable/quarantined` (and Open's `slot/generation`) before dispatch, so a
  client-supplied value can never survive as a valid reply.
- Whole-device quarantine dominates path-only poison and is never cleared by
  close/reopen/disconnect; `broken_` (transport/protocol) is separate from
  `quarantined_` (storage).
- Unsupported operations (`ReplaceExisting`, async read/cancel, non-ASCII,
  >4096 transfers, >32-bit offsets) remain explicitly rejected.

## Reproduce bounded evidence

From the worktree:

```
g++ -std=c++98 -Wall -Wextra -Werror -pedantic -Iqemu-port/fs \
  qemu-port/tests/r1_fs_channel/wire_test.cpp -o /tmp/opencode/r1-fs-wire-test
/tmp/opencode/r1-fs-wire-test

g++ -std=c++98 -Wall -Wextra -Werror -pedantic -Iqemu-port/fs \
  qemu-port/tests/r1_fs_channel/state_test.cpp -o /tmp/opencode/r1-fs-state-test
/tmp/opencode/r1-fs-state-test

g++ -std=c++98 -Wall -Wextra -Werror -pedantic -Iqemu-port/fs \
  qemu-port/tests/r1_fs_channel/adapter_test.cpp qemu-port/fs/r1_fs_channel.cpp \
  -o /tmp/opencode/r1-fs-adapter-test
/tmp/opencode/r1-fs-adapter-test
```

Expected:
- `R1 private channel wire bounds PASS`
- `R1 channel state and cleanup PASS`
- `R1 channel adapter PASS`

The previous portable FS core suite still passes. Kernel LDD compile is
compile-only against actual EKA headers and card 18's read-only header; no
runtime IPC/authorization is proven by it. See
`qemu-port/docs/r1_fs_channel_review_v1.md`.
