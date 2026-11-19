# R0-QA — Test and documentation inventory (report only)

Card: **R0-QA** (cheaper tier) from `docs/parallel_work.md`.
Base: commit `fa62323` + the current uncommitted working-tree changes.
Scope: inventory of tests, slow/flaky cases, fixture collisions and stale doc
claims, **without changing test semantics**. No code or test behavior was
modified; every proposed edit below is for the integrator to choose.

Deliverable status: this file is the entire deliverable.

## 1. Test inventory (as of this writing)

`tools/test_stable.py` runs `make`, then these 15 stages sequentially, each
with its own logs in `/tmp/opencode/stabilization/<name>.log`, `timeout=300`
per stage, and preserves each check's exit status:

| Stage | What it exercises | Host-side deps | Notes |
| --- | --- | --- | --- |
| build | `make` (kernels + ISOs) | gcc/as/ld, make, grub/ISO toolchain | fails fast: rest skipped |
| test_e32_relocs | host-only E32 reloc validation — no QEMU | `tools/e32read.py` | fastest; no disk |
| test_args | cmdline passing, monitor BSS readback | QEMU, mtools, unix monitor socket | uses `args-test.sock` |
| test_dll | DLL import resolution via EUSER.DLL | QEMU, mtools | writes fixed names `dll-euser.dll`, `dll-imp.e32` |
| test_data_reloc | initialized-data reloc, ring-3 readback | QEMU, mtools, monitor socket, **repeated `sleep`s** | poll/sleep heavy |
| test_userland | compiled ECHO, genuine cmdline copy | QEMU, monitor, samples build | real sleeps on purpose (NTick) |
| test_ipc | genuine IPC across ring-3 processes | QEMU, mtools | |
| test_cli | DOS-style shell commands (~40 cases) | QEMU, mtools (`mdir`) | sets RTC to 2027; year-agnostic `b"20"` check |
| test_cli_diskfull | failed COPY /Y preserves destination | QEMU, mtools | direct FAT bit-twiddling to simulate full disk |
| test_crash | fault attribution via reaper, shell survival | QEMU | |
| test_conc | server+clients+shell concurrency | QEMU | |
| test_interact | console lease, typed input, sleep/clock/random | QEMU | known-racy ordering (see docstrings) |
| test_sdk | scaffold → SDK build → boot → run | QEMU, mtools, **5 SDK example builds** | slowest-read stage noted in §2 |
| test_samples | build all six samples, run on OS | QEMU, mtools, sdk samples | |
| test_wait | blocking wait live/timeout/exit records | QEMU | line 31: "wait blocks the full timeout" |
| test_chunk | private + cross-process global chunks | QEMU | |

Not wired into the suite: `tools/test_xhci_keyboard.py` (bring-up probe;
~17 s of hard-coded sleeps; documented in README §USB). It is deliberately,
or at least currently, excluded from `CHECKS`; its status should be stated
explicitly in the docs (see §4-P7).

Host tooling the tests require but `tools/check_env.sh` does **not** verify:
`mtools` (`mcopy`, `mdir`, `mtype`), `qemu-system-i386` (verified), grub/ISO
tools (via `make`), `python3` ≥3.x. A missing `mtools` makes 14 of 15 stages
fail confusingly at mcopy/mkdisk time.

## 2. Slow stages / flakiness risks (observed + structural)

Timing evidence (single sequential run of `python3 tools/test_stable.py` on
the dirty working tree, no concurrency; all 16 stages PASS):

| Stage | Wall time (s) |
| --- | --- |
| build (incremental `make`) | 1 (logs show kernels/ISO already current) |
| test_e32_relocs | <1 (host-only) |
| test_args | 36 |
| test_dll | 34 |
| test_data_reloc | 37 |
| test_userland | 40 |
| test_ipc | 36 |
| test_cli | 43 |
| test_cli_diskfull | 35 |
| test_crash | 33 |
| test_conc | 37 |
| test_interact | 32 |
| test_sdk | 36 |
| test_samples | 35 |
| test_wait | 35 |
| test_chunk | 35 |
| **total** | **506 s ≈ 8.5 min** |

Every QEMU stage costs a uniform ~32–43 s (dominated by boot + monitor/regex
setup), so the 300 s per-stage timeout is comfortable on this host; the flaky
risks in §2 below are about fixed `sleep`s, not timeouts. Also note the build
stage ran incrementally in 1 s: a full `make` from clean is much heavier, so
timing comparisons on other machines should qualify that. Full logs:
`/tmp/opencode/stabilization/*.log`.

Structural risks visible in the code (no behavioral change proposed):

1. **`wait_for` re-reads the whole log file every 0.2 s** (`test_cli.py:13–22`,
   used by almost every QEMU stage). Cost grows with log length per iteration;
   harmless at current sizes, but scaling `test_cli` cases or verbose boot logs
   will slow every stage quadratically. Cheap future fix (not semantics):
   incremental `seek(pos)` reads, which the function already returns.
2. **`test_sdk`** rebuilds the scaffolder app and **5 SDK example apps on every
   run** with no incremental build reuse. On this host it remained within
   budget (36 s), but on slower hosts it stays the most likely stage to hit
   the 300 s timeout because its compound workload is not dominated by the
   uniform QEMU boot cost.
3. **Fixed timeouts with `time.sleep` polling**: `test_data_reloc`,
   `test_xhci_keyboard`, `test_args` (monitor `sleep(0.5)`,
   `settimeout(2)`), `test_sdk` (`sleep(0.3/0.2)` before typed commands). These
   embed wall-clock assumptions of a roughly-working host; under heavy machine
   load the typed-command sleeps are the flakiest paths.
4. **`test_interact`**: its own comments (lines 38–40) acknowledge
   nondeterministic ordering after the NTick fix; it therefore waits for
   markers rather than asserting order — correct, but fragile to any future
   reordering of console output.
5. **`test_wait`** intentionally burns its full timeout on the live-wait case;
   this is by design ("wait blocks the full timeout") but is dead time in the
   suite every run.
6. **Stale monitor sockets**: tests create unix monitor sockets with
   `server=on,wait=off` at fixed paths (`args-test.sock`, data_reloc's MON).
   After an aborted/killed run, a leftover socket file at the same path makes
   the next run fail at QEMU startup with a bind error rather than a useful
   message. All fixed-path artifacts are under `/tmp/opencode` and are
   overwritten/reused silently otherwise.
7. **Environmental date dependence**: `test_cli` sets the emulated RTC to an
   explicit 2027 date (deterministic), but the first assertion on the initial
   `DATE` reads the host-backed clock and only checks the `"20"` prefix —
   fine until year 2100, noted for completeness.

## 3. Fixture/path collisions (concurrency map)

Everything lives under `/tmp/opencode` with hard-coded names; the suite is
safe **only when run sequentially in one slot**, consistent with
`parallel_work.md` "Isolation rules". Exact collision map:

| Artifact | Writer stage | Collision if two agents/worktrees run in parallel |
| --- | --- | --- |
| `/tmp/opencode/cli-test.img`, `cli-test.log` | test_cli **and** test_cli_diskfull (imports `DISK/LOG` from `test_cli`) | same names, two QEMUs on one writable image |
| `/tmp/opencode/{args,dll,ipc,crash,conc,interact,sdk,samples,wait,chunk}-test.{img,log}` | respective stage | same name across worktrees |
| `*.sock` monitor sockets | test_args (`args-test.sock`), test_dll (`dll-test.sock`), test_data_reloc, test_xhci_keyboard | stale-socket startup failure; cross-agent crosstalk if two bind the same path |
| `/tmp/opencode/stabilization/*.log` | test_stable only (shared dir) | two suites interleave logs |
| `/tmp/opencode/SdkTmp/`, `sdkhello.txt` | test_sdk | scaffold destroyed mid-build by another run |
| `/tmp/opencode/dll-euser.dll`, `dll-imp.e32`, `reloc-validate.e32`, `cli-old.txt`, `cli-new.txt`, `env_test.c/o` (check_env) | respective helpers | host-side fixture files with fixed names |
| `build/kernfull.elf` | every QEMU stage reads it; `make` writes it | running individual stages after a `make` in *another* worktree races the binary |

The highest-value configurability gap for the R1 wave (e.g. R1-FS-TEST):
`-kernel build/kernfull.elf` and the `/tmp/opencode` root are hard-coded in
every module; making `ROOT`/`DISK`/`LOG` overridable by env var would let
agents run in parallel without touching test semantics. This is a proposal
only, requiring integrator approval since `test_cli.py` is a shared hotspot.

## 4. Stale or inconsistent documentation claims (proposed edits)

All numbers below verified against the current tree. Every claim marked
"→ fix" is a text-only edit; nothing changes what is tested.

- **P1. `qemu-port/README.md` line 6–7 (status header)**: says "runs build + 12
  QEMU suites". The suite has **build + 15** (see README line 761, which is
  already correct). → fix the header to "build + 15 QEMU suites" or, better,
  drop the count from the header and reference §22 so it can go stale in one
  place only.
- **P2. `qemu-port/README.md` §5 (lines 133–135)**: "build + 12 QEMU
  regressions (relocs, args, DLL, data-relocs, userland, IPC, CLI, disk-full,
  crash, concurrency, interactive, SDK)" — same staleness; the list is missing
  samples, wait, chunk. → replace with a pointer to §22's list, or update.
- **P3. `docs/full_os_checklist.md` line 236 (Q-01)**: "(`test_stable.py`
  9/9) — DONE". The count is stale (16 stages today). → make it
  stage-count-agnostic, e.g. "`test_stable.py` all-green — DONE", and forbid
  hardcoded counts in the checklist ground rules (G4 area).
- **P4. `docs/manual_test_order.md` line 15**: "All sixteen stages passed in
  the current stabilization pass" — count is consistent (build + 15 = 16), but
  the claim ages instantly with each new stage. → reword to point at the
  latest stabilization log instead of asserting a count.
- **P5. `docs/manual_test_order.md` line 146 (stray manual results)**: a
  hand-pasted raw line (`Okay... 1. Pass 2.Pass 3.Pass 4.Fail 5.Pass 6.Idunno
  7.Pass 8.Fail 9.Pass-ish 10. Fail 11.Pass ...`) sits at the end of the
  otherwise-polished doc. It contradicts §1–14's tone, mixes confusing step
  numbering with the actual (14-step) manual list, and records FAILs for steps
  4, 8, 10 that the owner reported but nothing tracks further. → move to
  `docs/hw_tests.md` or a dated owner-report file (`docs/history/`), and note
  the unresolved failed steps (4: ECHO spacing, 8: chunk/heap stage, 10: fault
  attribution) as open owner follow-ups anywhere they are tracked.
- **P6. `qemu-port/README.md` status/date**: header says "Status
  (2026-10-14)"; if the integrator redeems today's state, update the date
  whenever the next doc pass lands (low priority, consistency only).
- **P7. `tools/test_xhci_keyboard.py` status undocumented in checklist**: G4
  says every feature lands with a test wired into `test_stable.py`; USB
  keyboard is tested by a probe script that is *not* wired in. → either state
  in the checklist/Q-section that USB input is probe-tested only (and why), or
  wire it in. Doc-only clarification is sufficient today.
- **P8. `tools/check_env.sh` vs real test deps**: env script checks
  gcc/as/ld/qemu/make but not `mtools`. → add `command -v mcopy` (and grub
  ISO tools used by `make`) checks. This is script behavior addition, not
  test semantics; integrator-owned change.
- **P9. Untracked suite members**: `tools/test_chunk.py`, `test_samples.py`,
  `test_wait.py` and `docs/parallel_work.md`, `docs/project_roadmap.md` are
  **untracked** while `test_stable.py` (modified) references them. A fresh
  clone/worktree at the current HEAD has a suite that fails immediately on
  missing check files. → integrator should commit these with the baseline
  before dispatching other cards (this is exactly the "commit the baseline
  yourself" rule in parallel_work.md §Before-starting, item 1).

## 5. Positive findings (worth preserving)

- Exit-status fidelity in `test_stable.py` (no early `|| true`, log per stage,
  build failure short-circuits) is exactly right for an integration gate.
- Every QEMU test uses disposable, freshly-`mkdisk.py seeded` images; none
  touch a persistent/shared image.
- Doc "Boundaries, not completed services" section of
  `docs/manual_test_order.md` is impressively honest and matches README §22's
  "Still out" list — keep that pattern for R1 docs.
- Client-server VM teardown is consistent (`terminate → wait 3 → kill`) across
  tests; no orphaned QEMUs observed in the full sequential run.
- RTC test uses a fixed forward-looking date, avoiding local-date flakiness.

## 6. Proposed doc/test update list (for integrator decision)

Text-only, no semantics:
1. Apply P1–P4 (test-count and status-header fixes) in one doc-commit.
2. Apply P5 (move stray manual results into `docs/history/` or a dedicated
   owner report; note open fails). Nothing else re-records owner manual
   results elsewhere, so this is where the FAILs currently live.
3. Apply P7 (one sentence in checklist §16 on USB probe status).
4. Reconsider P9 before dispatching any parallel worktree (blocker-grade for
   the multi-agent plan).

Requires code-touching decision (separate slots, owner = integrator):
5. P8 mtools/mkisofs additions in `check_env.sh`.
6. Optional env-var configurability of `ROOT/DISK/LOG/`kernel path in
   `test_cli.py` + subtests to unlock parallel agents without semantic change.

## 7. What this card did / did not do

- Ran the stabilization suite once (`python3 tools/test_stable.py`) to obtain
  the timing/flakiness evidence in §2; no test semantics changed afterwards.
- Did not edit any test, build file or doc beyond creating this report (owned
  path per card: a report in `docs/`).
- Did not run manual steps, hardware tests, or QEMU concurrently with anything
  else; single reserved suite run.

## 8. Handoff

```text
Card / base commit / owned branch: R0-QA / fa62323 + dirty tree / report only
Contract revision and dependencies: none (R0; informational)
What changed (paths): docs/qa_inventory_R0.md (new) — nothing else
Tests run: python3 tools/test_stable.py — 16/16 PASS, 506 s total, logs in
  /tmp/opencode/stabilization/; per-stage timings in §2
Manual step drafted/updated: none (documentation-only card)
Compatibility and failure limits: n/a (no code)
Blocked assumptions: suite run was against the dirty working tree, not a
  committed baseline; see P9
Recommended integration order and next owner: integrator applies P1–P4/P7 doc
  fixes, resolves P9 baseline commit, then treats P8/configurability as small
  owned slices before dispatching R1 wave
```
