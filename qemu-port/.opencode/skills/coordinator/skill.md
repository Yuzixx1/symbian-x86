---
name: coordinator
description: Coordinates work dispatch and collection across secondary chats using shared .sybian/ state.
---

# Coordinator Skill

Load this skill in the **main (coordinator) chat**. It owns `.sybian/` state and
drives secondary worker chats through files on disk — no manual message routing.

## Shared state layout (`.sybian/`)

```
.sybian/
├── coordinator/
│   ├── state.json          # wave, card statuses, deps, qemu lock
│   ├── waves/              # wave-N.json
│   ├── cards/              # card-X.json (briefs)
│   └── handoffs/           # card-X.md (archived worker reports)
├── workers/
│   ├── inbox/              # coordinator drops card-X.brief.md here
│   └── outbox/             # workers drop card-X.handoff.md here
└── qemu-slot.lock          # holds card ID currently using QEMU
```

Everything lives on disk, so a context reset in the main chat does not lose
state.

## Commands

Invoke these exactly as written. The skill translates them into
`python3 .sybian/bin/coord <subcommand> ...` calls under the hood.

### `/coord dispatch <card-id> <tier> <owned-path> [owned-path...]`

Writes a card brief to `workers/inbox/card-X.brief.md`, records the card in
`coordinator/state.json`, and writes `coordinator/cards/card-X.json`.

- `<tier>` must be `strong` or `weak`.
- `<owned-path>` is one or more exact file/folder paths the worker owns.

Example:
```
/coord dispatch card-A strong abi/spec.json abi/encoder.ts
```

### `/coord collect`

Scans `workers/outbox/` for `*.handoff.md` files, reads each handoff, updates
the card status in `state.json`, and moves the handoff to
`coordinator/handoffs/` (archive).

Run this after workers report `/work submit`.

### `/coord wave <n>`

Starts a new wave `<n>`. Checks each card's dependencies; cards whose
dependencies are all `complete` are marked ready/dispatched for the new wave.

### `/coord status`

Prints the current wave, the QEMU lock holder, and every card's status/tier.

### `/coord ask <question>`

Pings the owner for a real decision. The skill flags questions touching ABI,
proof apps, hardware tests, QEMU lock, or blockers as "real decision" and
interrupts the owner; routine questions are noted as batchable.

In a live chat this uses the `question` tool to prompt the owner.

### `/coord docs`

Syncs the current state into `coordinator/planning.md` (human-readable planning
markdown).

## Owner interruption policy

Only interrupt the owner for real decisions:
- ABI changes
- Proof-app selection
- Hardware tests
- Blockers

Routine status collection does not interrupt the owner.

## Context reset safety

All state is in `.sybian/`. If the main chat loses context, re-run
`/coord status` to rebuild the picture from disk; no brief text needs to be
re-typed.
