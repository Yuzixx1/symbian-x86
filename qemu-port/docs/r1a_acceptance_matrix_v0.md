# R1-A acceptance matrix / v0 — custom upstream-toolchain RFs/RFile fixture

Status: **draft acceptance matrix; NOT a frozen contract; no tests executed**.
Date: 2026-10-21. Card: **R1-A-TEST-MATRIX** (cheaper, next-wave dispatch
`r1_next_wave.md` Agent 3). Port base `fa62323` + uncommitted workspace docs.
Upstream ref: `0c320865` (as cited by `r1_wire_research_v0.md`). Owned
artifact: this report only. No code, no tests run, no build, no QEMU slot
use, no shared-manual or executable edits.

Scope: a **bounded, source-referenced** acceptance matrix for the owner-
decided R1-A checkpoint — a *port-authored, zero-capability* file fixture
built with the **matching upstream x86 toolchain** against genuine
EUSER/EFSRV (roadmap §"R1 — R1-A functional checkpoint", lines 119–147).
This is **not** R1-B evidence: nothing here replaces or demos the
unchanged-upstream-app gate (`r1_research_reconciliation_v0.md` "owner
chooses split R1"; roadmap line 74: "A port-authored fixture cannot replace
the upstream R1-B proof").

Evidence shorthand used per case:
- **W** = `r1_wire_research_v0.md` (source-derived wire/behavior contract, v0
  — flagged before v1 freeze),
- **FM** = `fat_paths_R1.md` (port FAT inventory, not an approved design),
- **R** = `project_roadmap.md` (acceptance wording),
- **Q** = `qa_inventory_R0.md` (existing regression/slot facts),
- **V** = `r1_research_reconciliation_v0.md` (owner decisions).

Every case below is a *specification to be implemented after the R1-A ABI
freeze*, one QEMU regression each plus one shared manual step, per ground
rule G4 (`full_os_checklist.md`). Uncertainty flags appear as **[REVIEW-n]**
and are collected in §6.

## 0. Invariants and boundaries for all cases

1. **Existing regressions are preserved, not replaced.** All R1-A tests run
   in addition to the current 16-stage stable suite (Q: 16/16, 506 s, dirty
   tree). PortFile's snapshot semantics and the disk-full preservation
   behavior (`test_cli_diskfull.py`) stay pinned: the R1 service is new
   code; PortFile migration is "gradual" per roadmap lines 184–186 and
   **must not be assumed or pre-broken** by this matrix (V, FAT-model
   distinction bullet). No case requires editing PortFile.
2. **Backend error codes are not the RFs contract.** Today's internal map
   (`FM §4`: KErrTooBig/NotSupported/Corrupt/NoMemory...) is deliberately
   mapped to upstream RFs-visible completions listed in **W §7**; cases cite
   W's upstream table, not FM's.
3. **Isolation.** Each case uses its own fresh disposable image and log under
   `/tmp/opencode/r1a-m/caseNN.img|log|sock` (Q §3 pattern); never two QEMUs
   on one writable image (Q §3); `test_stable.py` stays the single shared
   slot's suite. One case = one killed-VM-torn state may be reused by its
   labeled "recovery" step only.
4. **Size claims** are always "tested to size X" (roadmap 153–159); no
   near-4-GiB capability claim is authorized by this matrix.
5. **Crash-during-write integrity is best effort** (roadmap 123–137): cases
   verify *documented survival*, not corruption-freedom; recovery procedure
   in §5. `Flush` ≠ atomicity/power-loss guarantee (W §7 closing note).
6. Separation duty: file assertions (§2) ≠ active-scheduler proof (§2-S) ≠
   R1-B historical compatibility (§7). A case may note scheduling
   *interactions* but its pass/fail must not depend on scheduler evidence.

## 1. Fixture prerequisites (dependency list, scheduled elsewhere)

| Dependency | Gate | If absent |
| --- | --- | --- |
| matched x86 toolchain + genuine EUSER/EFSRV artifacts | R1-BUILD-REPRO / R1-TOOLCHAIN | no case runs |
| genuine loader import binding of upstream-built E32 | R1-LOADER (loader hardening) | cases start at impl-block |
| RFs/RFile service with session/subsession IPC | R1-FS | no case runs |
| offset-capable FAT backend (>64 KiB, size/metadata ops) | R1-FAT (post freeze) | >1, >2, >3-trunc fail |
| genuine request-status/wait/panic transport | R1-EUSER / R1-STARTUP | *-panic, *-async, S-cases fail |

No case depends on PortFile code (invariant 1) but current fixtures **do**
depend on the shell exec path staying as in `startup_paths_R1.md` until the
loader changeover lands — record that baseline in each handoff.

## 2. The acceptance matrix

Completion codes are upstream RFs results per W §4/§7; descriptors are typed
per W §3. Every "file" case runs **synchronously** unless its ID says async.

### F-1 Create + basic write (roadmap: "creates, writes")

- **API:** `RFs::Connect` → `RFile::Create(path, mode)` with write mode.
- **Evidence:** W §4 (Create / 31, in16 + mode, new handle to slot 3);
  W §2 (session transport, version 2.0.4011); also: Create forces write
  server-side (W §6, do not attempt as read-only).
- **Pass:** KErrNone; then `Size()` = bytes written so far; handle is
  session-numeric, slot-3 integer (capture raw frames — W §8 vector 1).
- **Unique assets:** `case01.img` fresh `mkdisk.py seeded 64`; target name
  deliberately absent before the case starts (mount is the only prerequisite).
- **Deps:** toolchain, R1-FS, EUSER. **[REVIEW-1]** handle Width/capacity
  check for pkg32 reply; W flags descriptor capacity check as
  config-dependent (sthread.cpp note).

### F-2 Write ≥ 65537 bytes, explicit Flush, Close, reopen, byte verify

(The roadmap's headline functional demo; W §8 vector 2 verbatim intent.)

- **API:** RFile::Write (35, in8, length, -1=cur) loop to 65537 bytes;
  `RFile::Flush` (39), `RFile::Close` (28 **not 27** — W §1 correction);
  reopen with RFile::Open (30); readback loop; SHA-256 both sides.
- **Evidence:** W §8 vector 2; W §7 (Flush waits for in-flight writes +
  cache flush; Close error reporting invisible — **use explicit Flush**
  before Close to observe errors); FM §3 (both 64 KiB ceilings are
  PortFile/exec-only: RFile path is unaffected, but the *backend* must not
  be implemented with either cap).
- **Pass:** each Write completes KErrNone (chunked loop, not one 65537-byte
  wire request — client decides chunking); after reopen `Size()=65537`;
  readback byte-compare identical; exactly 65,537 bytes persisted.
- **Unique assets:** `case02.img` ~96 KiB slack for 64 KiB+ overhead; log
  hex-verified readback; SHA-256 in report.
- **Deps:** adds R1-FAT offset backend — the first case that cannot run on
  PortFile semantics. **[REVIEW-2]** Confirm Flush-vs-Close error
  observability on an actual built EFSRV before pinning (W §7 caveat).

### F-3 Seek semantics (Start/Current/End) and position update

- **API:** `RFile::Seek(mode, pos)` with 8-byte output package;
  positioned `Read(pos=10, len=3)` / `Write` at explicit positions.
- **Evidence:** W §6: modes Address=0/Start=1/Current=2/End=3; Current/End
  clamp; Start accepts beyond-EOF position (no reply package write);
  ordinary write beyond EOF clamps to size (NO POSIX hole creation);
  explicit-position Read *updates* the shared position (10→13 example).
  Positioned public calls with negative position panic FSCLIENT 19
  (client-side, before wire) — separate case N-3.
- **Pass:** Start to size+10 ⇒ KErrNone; one-byte Write appends at size+1
  (file grew by 1, **no hole**); Current/End over-size clamp to [0,size];
  positioned Read at 10 of length 3 leaves subsequent Current-position
  Read starting at 13.
- **Unique assets:** `case03.img` seeded.
- **[REVIEW-3]** Start-beyond-EOF followed by a limited-write (the file
  grew by one byte with no hole above) is exactly where the FAT backend
  could disagree with upstream clamping (zero-fill vs clamp); the wire
  candidate says clamp-and-append (W §6). Strong review must confirm from
  a real EFSRV trace once built, not from design intent alone.

### F-4 Size op and SetSize truncate/grow

- **API:** `RFile::Size(TInt&)` — **8-byte** reply despite 32-bit public
  param (W §1 #2, W §4 `0x10028`); `RFile::SetSize` (41).
- **Evidence:** W §4; W §7 error table: write past `0x7fffffff` ⇒ TooBig
  non-big-file; negative SetSize ⇒ client panic FSCLIENT 20 (upstream
  documented; separately N-5).
- **Pass:** Size after F-2 reopen returns 65537; SetSize(1024) then Size()
  = 1024 (truncation); SetSize growth behavior left **[REVIEW-4]** —
  upstream position-dependence of growth fill is documented unsettled
  ("public comment versus share/backend handling", W closing).
- **Unique assets:** `case04.img` (which must be an image where the
  truncating SetSize lands on its own file, not a volume-low state).
  Assert only Size here, not truncated-content guarantees.

### F-5 Empty / EOF reads, over-capacity public Read (client-side)

- **API:** zero-length public Read (no IPC — client fills descriptor),
  Read at EOF, oversized-descriptor read ⇒ KErrOverflow client-side.
- **Evidence:** W §6 end of section (client-side overloads) + W §7 EOF row
  (success, length 0). Do **not** assert KErrEof at EOF (W §1 #5).
- **Pass:** Read at EOF completes 0 with empty descriptor;
  over-capacity public Read returns Overflow without server round-trip
  (bounded: cross-check with N-3/N-4 to prove
  client-side path).
- **Unique assets:** none (share case02 image) or `case05.img`.
- **Deps:** EUSER genuine request/wrap + client library. **[REVIEW-5]**
  platform-security diagnostic config affects const/width rejection
  (W §3): pick one pinned configuration and record it on the fixture
  report.

### E-1 Missing file / missing path / bad name (roadmap: "reports a missing-file error")

- **API:** RFile::Open absent leaf, missing parent dir, wildcard/absent
  extension; **expected upstream codes: NotFound(-1), PathNotFound(-12),
  BadName** per W §7 rows 1–3 (source `sl_mnt.cpp`). Then
  SetSessionPath to legal-but-nonexistent dir ⇒ success; a file op there
  ⇒ PathNotFound (W §8 vector 5). Default session path after connect is
  `\<system-drive>:\Private\<SID>\` — do **not** hardcode C:\ (W §2).
- **Pass:** distinct codes per failure class, not an all-NotFound map
  (precedence still reviewed — **[REVIEW-6]**).

### E-2 Pre-existing snapshot-regression compatibility check (in-suite)

- Not a new test: run the entire 16-stage stable suite against the combined
  tree after R1-FS integration — per `parallel_work.md` "Isolation and
  integration rules" (a green branch test is not a green integration) —
  so it gates PortFile/disk-full preservation (invariant 1). Named
  separately so integration review cannot skip it.

### M-1 Sharing table (transposed order both ways)

- Full share matrix (X/R/A/W × existing X/R/A/W) per W §6 table incl.
  promotion/demotion from W⇒R after closes; incompatible = `InUse(-14)`,
  R-with-write = `Argument(-6)` **before** share check
  (W §7). ReadersOnly-existing + new write attempt = KErrInUse.
- **Unique assets:** `case-m1.img` w/ known-size file; both open orders;
  promotion-visible-Size check (write path promotes readers).
- **[REVIEW-7]** whether the transposed/W-domain matrix rows above can be
  collapsed for the fixture (completeness vs minimal-hard-failure bias) is
  a strong R1-FS review call, not to be inferred here.

### M-2 Two sessions, session-relative paths

- **API:** two RFs sessions, each SetSessionPath to different dirs; open a
  relative name per session; set nonexistent-but-legal dir, file op there
  → PathNotFound. Session-vs-object/lifeline correctness: a numerical
  handle from one session must NOT resolve from another (bad handle =
  panic path N-1, not an ordinary error).
- **Evidence:** W §5 (per-session object index, `sf_utl.cpp:595-612`),
  W §2 (session path semantics).

### N-1 Negative: forged/wrong-type/cross-session subsession handle

- **Pass:** client panic category `Panic`, reason BadHandle(-8) — NOT an
  ordinary error; server + other clients keep running; shell survives.
  **[REVIEW-8]**: distinguish EUSER panic-vs-KErrBadHandle release
  semantics before implementation; if genuine panic transport is not
  implemented yet, the case is *blocked*, not simplified, per §1 table.

### N-2 Negative: capacity/overflow — oversized file writes

- Write past 0x7fffffff legacy ceiling on a non-big-file share: TooBig
  (W §7 row "write across it"). No RFile64 claim (W §4 note). A test
  reaching exactly the legacy cap in QEMU time is infeasible; instead
  record **tested-to-X**: verify at modest sizes, and record SetSize
  refusal behavior as implemented (strong review to choose the probe
  size). **[REVIEW-9]**.

### N-3/4 Negative: public client-side rejection paths

- **N-3 (negative public position):** positioned `Read`/`Write` with a
  negative position must panic FSCLIENT 19 *client-side before sending*
  (W §7 row; `cl_file.cpp:980-995,1300-1312`); the server must never see
  the request. Proof of absence: a paired A-1-style request that would have
  been reachable never changes server state (observable via another reader).
- **N-4 wire-level negative length:** negative *requested length* on the
  raw wire slot ⇒ `Argument(-6)` server-side (W §7 "Negative requested
  length"). This is a raw-transport fixture case (W §8 vector 1 framing),
  not a public-API case — mark as requiring the W §8 "raw transport
  fixture for malformed messages" harness before it can run.
- Known open item: two negative-position rows in F-3's prose and W §3
  config-dependence note collapse here — **[REVIEW-8b]** pick which
  configuration rows the frozen contract carries.

### N-5 Negative: negative SetSize

- Negative `SetSize` ⇒ positive `ESizeNegative` leave converted to client
  panic FSCLIENT 20 (upstream documented, W §7 row; not an ordinary
  completion). Cross-session survival asserted like N-1.
- **Deps:** same as N-1 (genuine panic transport).

### A-1 Async read, explicit cancel, exactly-once completion

- **API:** open shares with async-all mode (`EFileReadAsyncAll`, 0x400);
  queue two reads with distinct statuses; targeted `ReadCancel` (120,
  raw status address or NULL = all reads); writer grows file to satisfy
  the surviving read.
- **Pass:** cancelled read completes Cancel(-3) **exactly once**; cancel
  message itself returns None; buffer/statuses live until completion;
  queued read completes after growth; then cancel-all. Repeat with cancel
  racing completion (W §8 vector 7).
- **Deps:** EUSER request-status identity, loader, service async queue.
  **[REVIEW-10]** lifecycle/status-aliasing pitfalls come from
  implementation time; do not frame a fake status.

### A-2 Server death / restart with pending requests

- Kill server with pending read ⇒ `ServerTerminated(-15)`; reconnect to a
  freshly started service and repeat the E-1/F-2 loop (W §8 vector 7 tail).
  Assert: kernel-side detach, not leak of session object; shell + Type is
  responsive after both kills; recovery per §5 — the killed-state image
  is disposed, not reused (`case-a2.img`).
- **Separate manual step requirement:** no tooling here asserts restart —
  draft PC steps §4 include both kills as owner-visible stops.

### P-1 Fresh-boot persistence (manual + optional automated pre-check)

- Primary form is the owner step (§4 items 2/6): fresh boot, rerun the
  fixture's persistence-only mode, byte-compare F-2's 65537 bytes, read
  EOF line, photograph. A QEMU re-boot persistence pre-check may automate
  the same argument on `case-p1.img` before the owner day; it gates nothing
  alone. "Flush = requested I/O issued" wording applies (roadmap
  123–137; W §7). **Do not** assume the R1 FAT backend power-loss
  behavior beyond the documented best-effort scope.

### S-1 (dedicated) Active-scheduler fixture — **separate**, per §0.6

- Prefer unchanged upstream `t_cactw` if it builds (roadmap 119–124);
  otherwise a port-authored scheduler fixture (recorded *not as unchanged-
  app evidence*). Assertions: at least one pending request completes on an
  active object, `CActive::RunL`-driven, wait/leave/panic-free through N-1
  fixture concurrently. **Out of this matrix's file-assertion scope**;
  entry point is a pointer to the scheduler card, not a case spec.

## 3. Matrix coverage vs brief checklist

| Brief item | Case(s) | Cover |
| --- | --- | --- |
| create/write | F-1, F-2 | true |
| explicit flush/close/reopen | F-2 | true |
| byte verification | F-2 | true |
| seek/size | F-3, F-4 | true |
| empty EOF | F-5 | true |
| missing-file/path errors | E-1 | true |
| >64-KiB boundaries | F-2 (+N-2 ceiling) | true |
| sharing/multiple clients | M-1, M-2 | true |
| cancel/completion | A-1 | true |
| server death/restart | A-2 | true |
| fresh-boot persistence | P-1 (owner §4; optional automated pre-check) | true (manual-gated) |
| scheduler proof separate | S-1 | pointer |

## 4. Draft physical-PC steps (owner only; no execution claims)

1. Boot the combined ISO (`make` default: GRUB image) on the reference PC;
   shell responsive; log to `r1a-manual-<date>.txt` (or photographed).
2. Launch the R1-A file fixture (typed name; verify no EXEC verb needed,
   startup_paths_R1 §1 still holds or note its change). Read F-2 output:
   per-byte verification and SHA line visible.
3. Introduce missing-file/path name variants by typing a fixture arg
   (designed-in, not debugging): E-1 codes printed per attempt.
4. Launch sharing/async case (M-1/A-1) once service-side PC build exists;
   growth + cancel messages visible; fixture exits cleanly.
5. Kill/restart the file service (A-2); shell input/output still live.
6. Cold reboot; rerun a persistence-only argument of the fixture; compare
   bytes (P-1). Photograph the EOF and missing-file output lines.
7. No destructive stress, no power-loss simulation on the reference disk;
   media used is the disposable QEMU-image-volume only.

## 5. Recovery / safety notes (not claims)

- All images are disposable; a torn case N-2/A-2 image must be recreated
  (`mkdisk.py seeded`) not repaired — no fsck exists anywhere in the port
  (FM §5). If a shared/volume fixture is ever reused, first a FAT1↔FAT2
  divergence check is required via host `mtools`; host detection alone
  proves nothing about guest behavior (per the reconciliation caveat on FM).
- `Flush` semantics = "requested I/O issued," not an atomic-write/power-loss
  guarantee (roadmap 126–137; W §7) — that exact wording must appear
  anywhere persistence is claimed (F-2, P-1).
- Any R1 effective-lifetime claim needs backend review against FM §5
  crash-window table *before* strength claims are adopted; per
  reconciliation, FM ≠ approved design and guest-side FAT-mirror detection
  is unproven — this matrix does not pin it.

## 6. Strong-review flag list (do not resolve by inference)

1. pkg32 handle-reply capacity/descriptor config dependence (W §3 note).
2. Flush-vs-Close observability on a built EFSRV (W §7/§8, v2 gate).
3. Start-beyond-EOF + subsequent Write growth semantics vs FAT backend
   model (§2/ F-3 [REVIEW-3]).
4. SetSize growth fill/position behavior (unsettled upstream, F-4 [REVIEW-4]).
5. Public zero-length/oversize Read client-side paths vs raw-wire probes
   (F-5 [REVIEW-5], W §6 close).
6. Error-precedence mapping overall; no all-NotFound collapses (E-1
   [REVIEW-6], W §7 note).
7. Share-table completeness incl. async-all share-transpose edge (M-1
   [REVIEW-7], W §6 matrix).
8. Panic-vs-error boundary for bad subsession handles (N-1 [REVIEW-8]).
9. "Tested to size X" probe sizes for the 0x7fffffff ceiling (N-2 [REVIEW-9]).
10. Async status lifetime / cancel-race framing (A-1 [REVIEW-10], W §7 tail).

## 7. Explicitly out of scope (R1-B and later)

- Unchanged upstream app compatibility (`t_cactw`/`FHServer` and any file
  app) — R1-B gate, not this fixture (V split-R1 decision).
- Packed TEntry/RDir, GetDir sorting, notifications, plugins, handle
  transfer — flagged in W closing as future wire-table expansion if used.
- PortFile redesign/migration order (roadmap "migrate gradually"), MMU
  (R2), GUI/interrupt layers (R3+), hardware-verified claims.

## 8. Handoff

```text
Card / base commit / owned branch: R1-A-TEST-MATRIX / fa62323 + workspace
  docs / research-doc only
Contract revision and dependencies: W v0 (not frozen); R1-A impl gates in
  §1; blocking freeze = R1-A ABI v1
What researched/changed: this report only
  (docs/r1a_acceptance_matrix_v0.md)
Tests run: none (doc-only card; invariant 1 keeps the existing suite gate
  in force — suite last known 16/16, dirty tree, Q)
Manual step drafted/updated: §4 draft steps, no execution claims
Compatibility and failure limits: §0 invariants; "tested to size X" rule;
  not R1-B evidence
Blocked assumptions/requests: [REVIEW-1..10] above; error-mapping decisions
  owed to R1-FS; toolchain artifacts unverified — cases cannot claim
  unblocked status
Recommended integration order and next owner: strong review of §6 flags +
  F-case cull/additions → R1-ABI v1 draft → R1-FS/FAT/EUSER/LOADER
  implementation wave (slot rules per parallel_work.md)
```
