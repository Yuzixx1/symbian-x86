# R1-A matrix errata / v0 — proposed corrections to `r1a_acceptance_matrix_v0.md`

Status: **errata report; the original matrix was NOT edited** (card rule:
"proposed text corrections by section, without editing the original").
Date: 2026-10-21. Card: **R1-A-MATRIX-ERRATA** (cheaper,
`r1_next_wave.md` Agent 3, current wave). Basis read: the current matrix
(`r1a_acceptance_matrix_v0.md`), `project_roadmap.md` (R1-A acceptance
wording), `r1_research_reconciliation_v0.md`, and the build handoff
`r1_build_repro_v0.md`; startup/console/heap prerequisites cross-read from
`r1_startup_research_v0.md`. No code, tests, builds, QEMU runs, or other
docs changed; deliverable is this file.

Each erratum: **[ID]**, section, the defect, and the exact proposed
replacement text (blockquoted) the integrator may apply in a matrix v1.
Defect classes the card names: stale artifact status (A), persistence
coverage misclassification (B), SHA-library requirement (C),
panic/scheduler conflation (D), media/mirror generic assumptions (E),
review-flag drift (F).

## A. Artifact status is stale (the build wave succeeded)

A1 — §1 dependency table, row 1 ("matched x86 toolchain + genuine
EUSER/EFSRV artifacts … | no case runs") and the handoff line "toolchain
artifacts unverified — cases cannot claim unblocked status" predate
`r1_build_repro_v0.md`, which *did* build the artifacts.

Proposed replacement for that row:

> | genuine EUSER/EFSRV **runtime** proof (loader resolves
>   `{000a0000}[100039e5]`-style import strings, relocation semantics, entry
>   frame) | R1-STARTUP + R1-LOADER | cases blocked at run level, *not*
>   at build level — build-level artifacts exist in scratch
>   (`r1_build_repro_v0.md` §Artifacts, sha256-pinned) |

Proposed replacement for the handoff line:

> Blocked assumptions/requests: §6 flags; error-mapping decisions owed to
> R1-FS; build-level EUSER/EFSRV artifacts exist (scratch, unredistributed)
> but run-level behavior is unverified — fixture cases remain blocked
> until the loader/startup gates close, not until someone builds again.

A2 — §1 trailing note "record that baseline in each handoff" must gain the
build report's lines 96–100 facts: import names carry the module version
and owner UID, so *loading* upstream-built images already depends on
R1-STARTUP/loader string matching; the matrix's F-cases must not be
scheduled against the port's bare-name IAT conventions (`startup_paths_R1.md`
step 5).

A3 — case deps that say "toolchain" (F-1 deps, §1 table) should read
"genuine EUSER/EFSRV artifacts (built; run-unverified) + R1-LOADER
artifact resolution" — reflecting that the missing piece is runtime
validation, not compilation.

## B. Fresh-boot QEMU persistence is a required regression, not "manual-gated"

B1 — matrix P-1 currently: "primary form is the owner step … the QEMU
re-boot persistence pre-check … gates nothing alone" and coverage row
"true (manual-gated)". The roadmap acceptance says "**A fresh boot
verifies persisted bytes**" as part of both the automated and owner
checklists, and the card's brief states the regression requirement. Also
"invariant 3" (fresh disposable image per case) conflicts with preserving
an image across two boots.

Proposed replacement for case P-1:

> ### P-1 Fresh-boot persistence (required automated regression + separate
> owner confirmation)
>
> - **Required regression coverage:** the case writes the F-2 65,537-byte
>   deterministic pattern, flushes/closes, **halts the VM cleanly**, boots
>   the *same preserved image* a second time, reopens the file, verifies
>   every byte and the EOF zero-length read, and compares the byte pattern
>   before and after reboot (W §8 vector 2 wording). Fail/timeout there
>   fails the case; this is part of "R1-A QEMU-ready", not owner-only
>   evidence.
> - **Separate owner confirmation:** the owner's reference-PC boot +
>   launch + byte comparison is what grants "hardware-verified" (roadmap),
>   and it is a **separate label** from the QEMU pass — the QEMU pass does
>   not certify the PC and vice versa.
> - **Image rule:** this is the only case whose image is reused across two
>   boots (record `case-p1.img` intent in the report); it is still never
>   written by two runnable VMs at once (sequential suite rule holds).

Proposed coverage-row replacement:

> | fresh-boot persistence | P-1 (required QEMU regression; owner PC separate) | true |

B2 — invariant 3's last sentence ("a killed-VM-torn state may be reused by
its labeled recovery step only") should explicitly add:

> Exception: P-1 reuses one image across a clean-reboot pair by design;
> reuse-after-*failure* is still disposal-recreate.

## C. Deterministic pattern verification — no SHA requirement on target code

C1 — F-2 API line "readback loop; SHA-256 both sides", asset line
"SHA-256 in report", and §4 step 2 "per-byte verification and SHA line
visible" add a hashing dependency to fixture code, which the brief
excludes.

Proposed replacements (F-2):

> - **API:** RFile::Write (35, in8, length, -1=cur) loop writing a
>   **deterministic byte pattern** (e.g. byte at offset i = (i mod 251))
>   to 65,537 bytes; `RFile::Flush` (39), `RFile::Close` (28 not 27 — W §1
>   correction); reopen with RFile::Open (30); readback loop asserting the
>   same pattern byte-for-byte.
> - **Pass criteria:** byte-pattern compare identical across
>   write/flush/close/reopen/read-back; chunking is per-write
>   (65,537 split into chunks); exactly 65,537 bytes persisted. If a
>   digest is useful for the *report log*, compute it host-side from the
>   log dump (optional, never in target code).

C2 — §4 draft PC steps, item 2, replace "per-byte verification and SHA line
visible" with:

>    per-byte pattern-verification output visible (digest optional in the
>    report log only).

C3 — wherever a digest appears elsewhere (F-2 assets line, P-1), keep
byte-pattern wording as the pass criterion and add:

> The fixture relies on deterministic byte patterns, not on any hashing
> library present in target code.

## D. Panic fixtures and scheduler proof are separate (do not conflate)

D1 — case S-1 currently bundles "wait/leave/panic-free through N-1 fixture
concurrently" into the scheduler assertions — that couples deliberate
negative-child behavior with scheduler pass/fail, which the brief forbids.

Proposed replacement for S-1:

> ### S-1 Active-scheduler fixture — separate driver, separate assertions
>
> - Scheduling assertions only: at least one pending request completes on
>   an active object, `CActive::RunL`-driven, with at least one remaining
>   file request completing during scheduler activity. Panic behavior is
>   NOT part of this case's success/failure.
> - Deliberate negative-child panic (case N-1, plus N-3 client-side
>   panics) runs as its **own** negative driver in its own boot/process
>   slot; its criteria are "the server, surviving clients, shell survive."
>   No assertion in the panic driver depends on scheduler output, and no
>   assertion in S-1 depends on panic output.
> - Prefer unchanged upstream `t_cactw` if feasible (unbuilt candidate per
>   `r1_file_proof_search_v0.md`/reconciliation); otherwise a port-authored
>   scheduler fixture recorded *not* as unchanged-app evidence.

D2 — the coverage table's "scheduler proof separate | S-1 | pointer" row
should become "true (separate driver)" once the fixture lands; the
errata-matrix grouping (§G) already schedules the panic driver and S-1 in
different runs.

## E. Media/mirror assumptions replaced with accurate requirements

E1 — §5 first bullet currently mandates a "FAT1↔FAT2 divergence check is
required via host `mtools`" for any reused image. Per the reconciliation
(host detection proves nothing about the guest), that generic check is
downgraded to an *optional aid*; plus the media wording should name the
requirement on the fixture side.

Proposed replacement for that bullet:

> - Disposable images only; recreate (`tools/mkdisk.py seeded`) what a
>   failed/torn case touched. Where a case deliberately produces
>   FAT-mirror divergence, a host-side `mtools`/mirror comparison is an
>   **optional report aid**, never a pass criterion — guest-side detection
>   is an implementation question, not an accepted R1 invariant.
> - Volume/media requirements the fixture actually has: exactly one
>   mounted writable FAT32 volume per case (via the R1-A RFs mount), plus
>   double free space for the largest case size (F-2's 65,537 bytes plus
>   metadata); no other media assumptions (no CD/second-drive
>   dependencies) and destructive operations never target a reference
>   host disk.

E2 — §4 PC-steps preamble should state the transfer path precisely:
programs/media reach the PC disk by the owner's existing copy flow (e.g.
`mkdisk.py seeded` + `mcopy` for the test volume), so the matrix must not
assume an ISO-installed volume or a net-boot of the reference PC.

## F. Keep exactly ten REVIEW flags

F1 — the matrix's §6 lists ten flags, but §2 negatives add "[REVIEW-8b]"
(choosing which config rows the contract carries). That is flag drift —
do not create an eleventh flag; instead:

Proposed replacement (N-3/4 bullet):

> - Known open item: the negative-position rows and the W §3
>   config-dependence note collapse into **[REVIEW-8]** (panic-vs-error
>   boundary). Its scope is broadened to include which descriptor/panic
>   rows the frozen contract carries; §6 stays at ten flags.

F2 — flag #2's wording should reflect that a built EFSRV now exists in
scratch: change "on a built EFSRV (v2 gate)" evidence source to
"on the scratch-built EFSRV (practice gate; still not a frozen ABI)".

## G. Dependency/coverage + regression grouping (errata edition)

Bounded dependency summary (post-A corrections):

| Dependency | State per current reports | Blocking effect on cases |
|---|---|---|
| genuine EUSER/EFSRV/`petran` artifacts | **exist** (scratch, sha256-pinned; build-repro v0) | none at build level |
| loader artifact resolution (import-name strings, relocations, entry frame) | unverified; needs R1-STARTUP/LOADER on real images | all F/M/E/N/P cases blocked at run level |
| genuine startup: heap/TLS/leaves/build path (`r1_startup_research_v0.md` §1–3) | port gap documented ("no genuine SetupThreadHeap path", "no code-seg-ID handling") | fixture cannot be built *using genuine EUSER semantics* until closed (fixture prerequisite) |
| console/print output route | genuine route unresolved (startup "RTest" dependency path question) | fixture visible-output claim pending; port debug-transport fallback is not genuine console |
| R1-FS service + R1-FAT offset backend | not implemented (contracts await ABI/v1) | all file cases blocked |
| QEMU regression slot / suite | stable suite 16/16 recent (dirty tree), sequential rule | always respected |

Errata-authoritative coverage (matrix cases unchanged otherwise):

| Brief-scenario group | Cases | Coverage statement |
|---|---|---|
| create/write + flush/close/reopen + byte verification | F-1, F-2 | required QEMU regression |
| seek/size, EOF | F-3, F-4, F-5 | required QEMU regression |
| missing-file/path errors | E-1 | required QEMU regression |
| >64-KiB boundary | F-2 size + N-2 ceiling probes | required, with per-report "tested to X" |
| sharing/multiple clients | M-1, M-2 | required QEMU regression |
| cancel/completion | A-1 | required QEMU regression |
| server death/restart | A-2 | required QEMU regression |
| persistence | P-1 | **required QEMU regression** (same image across two boots) + separate owner PC confirmation |
| scheduler proof | S-1 | separate driver (D1) |
| deliberate panics | N-1, N-3, N-4, N-5 | separate negative driver (D1) |

Proposed **regression grouping** (sequential suite rule preserved; one
case group runs at a time; images stay unique per group):

1. **Driver-A "file lifecycle"** — F-1, E-1, F-3, F-4, F-5 sub-scenarios
   sharing one fresh `drivera.img` and boot; asserts grouped pass with
   per-subscenario logs; the >64-KiB F-2 write belongs here too
   (`drivera.img` sized accordingly).
2. **Driver-B "sharing"** — M-1 (+M-2), fresh `driverb.img`; one boot.
3. **Driver-C "async + service failure"** — A-1 then A-2 (kill/restart),
   fresh `driverc.img`; explicit ordering, no parallel VM.
4. **Driver-D "negatives"** — N-1, N-3, N-4, N-5 (deliberate child panics),
   fresh `driverd.img`; NOTHING from Driver-C files asserted here and
   vice versa (D1).
5. **Driver-E "persistence regression"** — boot 1 (write pattern, clean
   halt) → boot 2 (verify) on preserved `drivere.img` (B1).
6. **S-1 scheduler driver** — separate (S-1 as revised in D1);
   scheduler-only artifacts, own image.

The grouping is a proposal for the integrator to adopt or redistribute in
the matrix v1; it does not change any case's pass criteria.

## Handoff

```text
Card / base commit / owned branch: R1-A-MATRIX-ERRATA / fa62323 + workspace
  docs / research-doc only
Contract revision and dependencies: none frozen; consumes matrix v0,
  build-repro v0, startup research v0, roadmap/reconciliation
What researched/changed: this errata report only
  (docs/r1a_matrix_errata_v0.md); original matrix untouched
Tests run: none (doc-only card)
Manual step drafted/updated: none; §E clarifies media wording only
Compatibility and failure limits: corrections preserve every original pass
  criterion except the corrected ones (P-1 status, SHA removal, S-1/N
  separation); ten review flags retained (F1)
Blocked assumptions or cross-card interface requests: integrator must
  reconcile §A (artifact/run-level status) and §B (persistence rule) with
  the matrix before R1-ABI/v1; the fixture's console/heap prerequisites
  are owned by the startup card (cross-referenced, not re-owned)
Recommended integration order and next owner: apply corrections A–F into
  matrix v1, adopt/adjust §G grouping, then proceed with the review-flag
  gate (now ten) and the R1-A implementation wave
```
