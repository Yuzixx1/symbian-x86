# Overnight runs — definition and rules (2026-11-25)

## What an overnight run IS
A continuous session of T3 delegated tasks that keeps working while the user is
away. It is **not** a background shell job and **not** a daemon. It starts only
on an explicit control phrase and **does not end when a wave finishes** — it
keeps chaining the next scoped wave until `OVERNIGHT STOP` (2026-11-25 user
directive: "when I say overnight start, you do stuff til I say overnight stop").

## Control phrases (exact)
These are the only commands that control an overnight run. They must appear as
their own line/message; do not infer one from casual wording like "go ahead".

| Phrase | Effect |
|---|---|
| `OVERNIGHT START` | Start (or resume after PAUSE). Launch 4+ delegated small-chunk tasks. Keep going across wave boundaries until STOP. |
| `OVERNIGHT STATUS` | Report current state without changing anything: task IDs, HEAD, gates, ISO, `kernelhwsrv/` cleanliness. |
| `OVERNIGHT STOP` | Cancel all running delegated tasks with `task_cancel`, run the final gate check, report state. No commits unless explicitly authorized by the user. |
| `OVERNIGHT PAUSE` | Stop launching replacements; let in-flight tasks finish; do not start new ones. RESUME with `OVERNIGHT START`. |

Synonyms accepted on a standalone line: `go overnight` = `OVERNIGHT START`;
`stop overnight` = `OVERNIGHT STOP`. Nothing else starts or stops a run.
Wave completion is NOT a stop condition.

## Start
- Only on one of the exact control phrases above. Never auto-start.
- Launch **4+** `delegate_task` calls, `mode:"async"`, on
  `acpRegistry_mistral_vibe` / `deepseek-v4-1-flash-1017` (`thinking=high`).
  Split work into small, independently verifiable chunks so more agents can run
  at once (2026-11-25 user directive). The four already in flight at the time of
  that change were launched on `mistral-large-4-0`; replacements use DeepSeek.
- Each brief gets **exclusive ownership of named files/paths** and carries the
  standing constraints: `kernelhwsrv/` is READ-ONLY; do not commit/push unless
  explicitly authorized by the user; do not
  run `make clean`; QEMU serialized through `.sybian/qemu-slot.lock` with an
  isolated re-run before believing an exit-124 FAIL.
- First-wave briefs: `docs/PLAN_OVERNIGHT_WAVE1.md` — P0 baseline/freeze,
  P1 guarded shim deletion + G12, P3 FreeDM SKIP-clean scaffold, P4-CI.
  P2 is OUT OF SCOPE; P5 is deferred.

## While running
- **Never poll or sleep.** Wait for completion notifications only.
- When a task finishes, immediately launch a replacement on non-overlapping
  files to keep 4 in flight.
- Mistral first; HY4 fallback only after an actual failure, per launch.
  A task dying on budget/network is a task failure — relaunch the identical brief.
- **No commits or pushes** (decision D4) **unless explicitly authorized by the
  user**: absent that authorization, everything stays in the working tree.
- No WATCH archive move (gated on a landed commit), no Wolf3D/shareware, no
  asset downloads, no CD-R boot, no kernel-hook edits.
- No questions to the user while away; safe defaults apply.

## End
- The ONLY terminator is `OVERNIGHT STOP` (or `stop overnight`). Wave completion,
  idle agents, or a green gate run is NOT an end condition: plan and launch the
  next scoped wave (e.g. wave 2: G12 implementation, P5 provenance, remaining
  orphaned shims) and keep replacing finished agents so useful work continues.
- `OVERNIGHT PAUSE` freezes launches without killing in-flight work; a later
  `OVERNIGHT START` resumes the run.
- Cancel any still-running tasks with `task_cancel` — never
  `t3_thread_interrupt` on a `childThreadId`.
- Final check: `git log --oneline -1`, `git status --short`, `git status --short
  kernelhwsrv/`, strict gate result, ISO timestamp.
- Report: HEAD, working-tree diff summary, gate result, ISO, `kernelhwsrv/`
  clean. No commits or pushes unless explicitly authorized by the user.

## What an overnight run is NOT — background jobs are SCRAPPED
A single shell/`systemd-run` job started in the background is a **background
run**, not an overnight run. That idea was scrapped on 2026-11-25 because RAM
is the limiting resource: 15 GiB total, ~12 GiB used, ~2.7 GiB available,
8 GiB zram swap full. Top consumers are desktop apps: `quickshell` ~3 GiB,
Firefox ~3 GiB across processes, T3 Code ~1.2 GiB, Discord ~0.8 GiB.
The one capped verification attempt failed immediately (`systemd-run` rejected
`Nice=19`) and was not retried. Do not start background jobs under current RAM
pressure.

## Decisions in force
D1 GUI retired / P2 out of scope; D2 FreeDM/BSD-CC0 PWAD on `DOOM.E32`;
D3 exec-270-only fbblit; D4 no overnight commits unless explicitly authorized by
the user; D5 per-test WAD swap;
D6 authored netstack permanent. See `docs/PLAN_DECISIONS_RESOLVED.md`.

2026-11-25: recovered `freedoom1.wad` is staged git-ignored at
`qemu-port/wads/` (root `.gitignore`) for wave durability; P3 FreeDM remains
SKIP pending user drop-off; never commit the WAD.
