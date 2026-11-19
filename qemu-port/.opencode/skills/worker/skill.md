---
name: worker
description: Claims cards, submits handoffs, and manages the QEMU slot lock from a secondary chat.
---

# Worker Skill

Load this skill in **each secondary (worker) chat**. Workers read briefs from the
shared inbox, execute work, write handoffs to the shared outbox, and coordinate
the QEMU slot lock — all through files on disk.

## Shared state layout (`.sybian/`)

```
.sybian/
├── coordinator/
│   ├── state.json
│   ├── cards/              # card-X.json (briefs, updated by workers)
│   └── handoffs/
├── workers/
│   ├── inbox/              # card-X.brief.md (coordinator drops briefs here)
│   └── outbox/             # card-X.handoff.md (workers drop reports here)
└── qemu-slot.lock          # holds card ID currently using QEMU
```

## Commands

Invoke these exactly as written. The skill translates them into
`python3 .sybian/bin/work <subcommand> ...` calls under the hood.

### `/work claim <card-id>`

Reads the brief from `workers/inbox/card-X.brief.md`, prints it, and marks the
card `in-progress` in `coordinator/state.json` and `coordinator/cards/card-X.json`.

Example:
```
/work claim card-A
```

Prerequisite: the coordinator must have dispatched the card (brief present in
`workers/inbox/`).

### `/work submit <handoff.md>`

Writes the handoff report to `workers/outbox/card-X.handoff.md` (normalized
filename) and marks the card `complete` in state.

The handoff file must follow this format:

```
Card ID: card-X
Status: complete | blocked | partial
What changed: <file paths>
Pass/Fail: <result with evidence>
Commands/RC: <exact commands and return codes>
Blocker: <if any, exact error>
Next card: <suggested follow-up>
```

After submitting, tell the coordinator to run `/coord collect` to archive the
handoff and finalize state.

Example:
```
/work submit /tmp/card-A.handoff.md
```

### `/work qemu-acquire`

Tries to grab the QEMU slot lock (`sybian/qemu-slot.lock`). Fails with a non-zero
exit if another card already holds the lock.

### `/work qemu-release`

Releases the QEMU slot lock.

## Workflow (per card)

1. Coordinator runs `/coord dispatch <card-id> <tier> <paths>`.
2. Worker runs `/work claim <card-id>` and reads the brief.
3. Worker executes the work, respecting the brief's Constraints.
4. Worker writes a handoff and runs `/work submit <handoff.md>`.
5. (Optional, for hardware/QEMU work) Worker runs `/work qemu-acquire` before
   testing and `/work qemu-release` after.
6. Coordinator runs `/coord collect` to read the handoff and archive it.

## Constraints (default, from card brief)

- Do not modify proof apps under `apps/proof/`.
- Do not run hardware tests unless explicitly assigned.
- Do not change the QEMU lock file directly (use qemu-acquire/qemu-release).
