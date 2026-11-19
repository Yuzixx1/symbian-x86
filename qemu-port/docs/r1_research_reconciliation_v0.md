# R1 research reconciliation — 2026-10-21 (NOT a frozen ABI)

## Latest batch intake — cards 22/23/24 complete

Read card-22 runtime report, channel hardening handoff and console recipe closure.
Adopt **supervisor stack fix**: `ShellSpawnFull` currently sets
`info.iSupervisorStackSize=0`, resolving to 4 KiB; set to 0x8000. This is a
kernel/runtime change outside card-22 ownership, but coordinator authorizes it
because G's verified heap/sched run proves the fix safe and 4 KiB is insufficient
for any Genuine G code. No alternative workaround (reducing automatic storage)
is pursued now; it would be a larger and riskier change. Card 22's diagnostic build
already validated 0x8000. Authorize commit of this single change as part of next
integration card, not standalone push.

Card 22 targeted sequence with diagnostic build passed all assertions, no fault
markers. Existing stable suite 15/15 PASS; G honestly skipped due to asset absence.
Do not claim stable G pass yet. Next integration card applies stack fix and runs
private G stable sequence.

Card 23 hardening accepted: new-key cleanup on reply failure, deterministic
response, separate transport-vs-storage quarantine, and three host test suites.
Card 24 accepted: recipe reconciliation, host checker and independent HAL byte
audit (six exports, 484-byte array, zero data/BSS). Canonical manifest produced.

Cards 25–29 complete. Card 25: G heap/sched passed in guest, stable 15/15 PASS
both asset-free and asset-backed; shell stack 0x8000 applied; FS channel copied
and kernel-unit compiled; private assets staged; console recipe doc produced.
Card 27: FS channel wired into Makefile off-by-default; channel unit compiled;
all host suites PASS; stable 15/15 PASS with honest G skip; service smoke
not-run pending host policy. Card 28: HAL/EKDATA v3 pins wired into
stage_r1_g.py; verify iterates all pins; default builds asset-free. Card 29:
services header and host policy produced; source-backed specifics verified
against genuine kernel sources; host manifest/cap decisions remain for
coordinator.

Card 34 completed: bx86gcc EFSRV def/import library verified in cold build
roots. Card 35 substantially complete: `portfilesrv.exe` built as PEi-i386
(6240 bytes), zero-caps ProtServ, EUSER+EFSRV imports verified, SHA-256
e3ca495061b11656d2cb592fe4dd5ece486217ccf4855065111352eba0f75c88.
Card 36 completed: petran E32 conversion succeeded; E32 portfilesrv image
(884 bytes, SHA-256 `d91ebcf28b659e84ab69bbbad9ff08828e72bfdbc2e307966a0e6c9bce6809d0`),
UIDs `1000007a/00000000/00002323`, SID `00000000`, VID `70000001`, Caps
ProtServ-only `00000000/00000100`, imports 1221/1953 verified. Card 32 offered
— QEMU file-service regression with exclusive slot. Card 33 (physical-PC
smoke test) remains unexecuted. No commits/pushes.

## Latest batch intake — cards 19/20/21 complete

Read G launch implementation/README/driver, channel kernel source/README and
EKDATA v3 report. Located G deliverables in home-backed card-20 worktree, not
main as the abbreviated handoff might imply. Integrated new G launch/tools/tests
and narrowly merged four existing-file changes into main; storage declarations
preserved, launch declarations moved inside existing header guard. No build or
guest run at intake. Both private-link/host passes remain worker evidence.

Independently verified EKDATA v3 image SHA-256
`2e11e7287325a7829a28b7cb510c5605390e57b1953a0b16704b8210c4aced0f`.
Accept audited artifact as later console staging input, not runtime success.
HAL main recipe/checker remains stale relative to surviving v2; authorize narrow
source consistency/independent existing-artifact audit without another build.

FS channel remains private candidate, no wholesale copy of changed FS files.
Review found new-key leak on Open followed by failed ThreadRawWrite response:
must close unpublished new key, preserve original error, no fake file rollback.
Audit deterministic response state/layout, genuine request/CS/lifetime and test
actual cleanup boundary, not just identity predicates. Device-wide quarantine
and unknown mutation size semantics retained. No privilege-launch guard removal.

Now authorize **one serialized targeted G QEMU slot to card 22**, after driver
and linked-input verification. Stop first real kernel/runtime failure; source-
level driver fix alone may proceed in owned scope. Stable-suite wiring/run only
after targeted passes; no acceptance or stable pass claimed in advance. Cards
23/24 independently harden channel and reconcile console recipes; no other VM.
Manual/hardware gate remains pending; no commits/pushes. Full ABI unfrozen.

## Latest ruling — card 18 delivered / card 19 recovery gate waived

Read card-18 source/README: real correct-device ATA/AHCI flush, operation-scoped
write tracking and dispatch-device boot-lifetime FAT quarantine. Independently
verified all five delivery hashes. Host driver/FAT tests and private compiles
remain worker evidence; no rerun or guest/device behavior claim. Adopt storage
interface for continuing card 21; pass latest r1_fat_offset.h and both storage
declarations to channel owner. All FAT aliases on dispatch device are quarantined;
raw external driver callers/different dispatch identities are not proven isolated.
No automatic remount recovery or physical power-loss durability promise.

Original console v2 report remains missing, but source/build evidence survives.
Coordinator read ordinal audit, verified six surviving evidence/artifact hashes;
recovery note `r1_console_data_v2_recovery.md` records facts and limitations.
**Waive original-prose prerequisite**: lost prose must not become a circular
blocker when primary evidence survived. Resume existing card 19 with source hash
verification/resolver inspection, authorized local-path fix and single EKDATA-only
build. No new report search, HAL rebuild or fabrication of original handoff.
HAL historical six-export/data audit remains qualified, EKDATA rejected pending
correction. This is not console/runtime acceptance.

20/21 continue, no new broad inventory or busywork assigned to freed storage
worker; reserve availability for concrete channel/link integration corrections.
No QEMU allocated; no builds/tests repeated at intake, commits or pushes.

## Latest return — kernel adapter / FS core

Read actual kernel adapter/bootstrap source and ownership handoff, actual FS
server/decoder/backend and README. Copied eight FS deliverables from home-backed
worktree without replacing differing files, verifying backend SHA-256
`1ae8e0cde6137dd9a43f6f37631e1d2ab2a9c8634eebef7553ecac02d2fe58d8`.
No completed host/compile tests repeated. Compile/source-test outcomes remain
worker evidence, not independent kernel/runtime proof. No QEMU-ready claim.

Accept adapter for bounded integration: genuine AddDependency is pointer-only,
explicit temporary edge refs bridge to process OpenDeps; kernel creation failures
can leave stale process output, command pointer ownership and creator refs must
follow kernel source. Kernel-thread-owned MakeHandleAndOpen, no Resume, no dynamic
publication, no legacy reaper second creator close. Pre-Resume diagnostics and
published-suspended abandonment cleanup required in integration. Bootstrap rejects
privileged roots; do not delete this gate to launch portfilesrv. Later privileged
host requires reviewed pinned-image policy/authorization and separate scoped API.

Accept FS as first synchronous subset only; full async/cancel matrix remains open.
Service registration still B2 then public Start B3; pure heap/sched needs neither.
Backend integration must reconcile authoritative committed/size fields with FAT
uncertain-metadata errors: successful sector bytes are not a known published file
size. Fail closed and represent uncertainty explicitly, no fabricated Size success.
Volume-wide quarantine cannot be reduced to FS path-only poison/close-and-reopen.

Cards 20/21 authorize G launch source/private link and private FAT-channel adapter.
Cards 18/19 remain pending handoffs; no relaunch. Shared Makefile/launch ownership
is card 20; storage declarations card 18; no overlap in logical header hunks.
No QEMU yet: review linked integration before allocating serialized guest slot
and stable-suite wiring. No commit/push or physical verification.

## Latest partial return — FAT offset / console strict build

Cards 14/15 have not handed off; keep them active, do not relaunch them.
Card 16 source/header/tests are present in main. Coordinator read actual backend
and verified preserved baseline SHA-256
`074dda7f6d50b46bae010416c27a7cedffab9df6cdf25d2c5fc63247df02a920`
and exact current diff against submitted baseline diff. No host/compile tests
repeated at this intake. Worker RAM-sector/kernel compile evidence remains
worker-reported. Accept bounded offset implementation for integration with
qualifications: current Flush without hook returns NotSupported; fail-stop only
covers new API, legacy mutation quarantine missing. Disk-full mutation can
poison volume and needs reboot/repair; do not claim ordinary service recovery,
atomic resize/write or broad R1 file acceptance. Next card closes real flush
and legacy quarantine; FS backend alignment waits actual card-15 header.

Card 17 findings are **owner-relayed, not independently reviewed artifacts**:
HAL six strict exports pass; EKDATA still auto-exports because bare DEFFILE
resolves ../eabi. HAL DATA-size removal via prepdef nodatasizes reportedly
preserves full 484-byte array. Report/source delivery path
`/tmp/opencode/r1-console-data-source` is unavailable after restart; v2 report
not found under home cache. No report/source copy or artifact acceptance claimed.
Recover existing deliverables before rebuilding anything. Explicit local
`DEFFILE ./ekdata.def` is approved in principle subject to actual recipe/resolver
review. Conditional single EKDATA-only correction build is card 19; do not
rebuild already-passing HAL or repeat v1/v2 sequences. HAL remains provisional
until recovered evidence reviewed; EKDATA/aliases remain rejected.

Follow-up cards 18/19 are offered, not launched. No QEMU, feature acceptance,
hardware verification, commits or pushes. No completed build/tests repeated.

## Latest ruling — 2026-10-23 core/compat/console-build return

Reviewed actual core headers/load implementation, host handoff, C++98 format
diff and console export audit. Integrated format conversion without public header
or semantic changes; preserved full worker handoff in original worktree and
recorded its location/qualifications in main tests README.

Coordinator independently reran **both suites on combined main sources**:
- Format: 78 vectors + API assertions + 200 mutations PASS, scratch
  `/home/yuzi/.cache/sybian-r1/r1-coordinator-format-compat-20261023`.
- Core: five real closures, 52 object-operation and 8 source-operation failure
  positions, 26 negatives and data/BSS/ownership assertions PASS, scratch
  `/home/yuzi/.cache/sybian-r1/r1-coordinator-core-20261023`.
No genuine kernel/IPC/runtime claim. Worker MinGW compile-only remains qualified
by Wine quota diagnostic; no sanitizer pass or target link was established.

Adopt concrete core headers as **internal adapter implementation boundary**,
not full public ABI freeze. Kernel adapter work is authorized now. Real kernel
locking/create/finalize/publication/ref ownership may require a reviewed narrow
header correction; fake tests cannot settle it. Bootstrap-only caller handles
are distinct from dynamic arbitrary-client publication, which remains blocked
pending genuine rollback. No need to wait for FS readiness to run heap/sched.

HAL/EKDATA v1 outputs are **rejected diagnostic-only**: auto-exports have wrong
ordinal tables. Source recipe strict-stem/staged genuine-def fixes and one v2
build are authorized; genuine HAL InitialValue DATA size metadata must be
explained without modifying upstream definitions. Zero data/BSS in rejected
images does not prove corrected-image layout or console runtime success.

Move to four strong concrete implementation/build cards 14–17 in
`r1_implementation_dispatch_v1.md`: kernel objects/bootstrap, user FS core,
offset FAT (exclusive current-dirty-file ownership), strict console data build.
Channel/bootstrap shared wiring and Makefile/stable integration are serialized
after handoffs. No QEMU slot allocated, no physical testing, commits or pushes.

This top section supersedes historical offered/pending entries below.

Coordinator review of `r1_abi_contract_v0.md`, `r1_toolchain_research_v0.md`,
`r1_proof_source_research_v0.md`, `r1_wire_research_v0.md` and
`startup_paths_R1.md`, `r1_sdk_route_research_v0.md`,
`r1_file_proof_search_v0.md` and `fat_paths_R1.md`.
Port baseline: `fa62323eea7bc96cefd666e40f5189567731bd44`
plus uncommitted files; upstream read-only revision:
`0c3208650587ac0230aed8a74e9bddb5288023eb`. These are static research
reports, not a built upstream x86 binary or an executed RFs compatibility test.

## Resolved research contradictions (do not silently edit v0 evidence)

- ABI/v0 says file subsession close is opcode **27**. The wire trace follows
  `kernelhwsrv/userlibandfileserver/fileserver/inc/F32plugin.h:71-72` and
  server dispatch in `sfile/sf_ops.h:55-70`: 27 closes a **directory**;
  `EFsFileSubClose` is **28**. Use 28 in later candidate tables/tests; do not
  implement from the incorrect v0 line.
- Ordinary `RFile::Size(TInt&)` and `Seek(TSeek,TInt&)` have 32-bit public
  parameters but their transport packages are **eight bytes**, per
  `r1_wire_research_v0.md` §1, §3. Do not infer a four-byte IPC payload or
  claim that this enables full RFile64 support.
- `startup_paths_R1.md` documents the *port's current* shell, 64-KiB image
  buffer, loader and freestanding `_start` chain. It does **not** establish
  genuine EUSER startup/TLS/heap/leave behavior or validate upstream-built
  imports; a strong R1-STARTUP review is still needed.

## Remaining gates

### Latest wave and owner decisions

- **Documented SDK route found, not reproduced.** The build-tools repo at
  `f458a4ce83f74d603362fe6b71eaa647ccc62fee` explicitly resolves X86 exports
  through `bx86gcc`, then `eabi`, with upstream transformations for secondary
  defs. EFSRV's missing bx86gcc def is therefore not by itself a dead end.
  Candidate pair: bx86gcc EUSER plus upstream-transformed eabi EFSRV, **not**
  bx86/MSVC exports. MinGW 3.4.5 under Wine compiled a probe, not these DLLs.
  Earlier local-search blocker descriptions are superseded to this extent.
- Owner authorizes **isolated scratch-only provisioning/build attempts** for
  pinned legacy tools, with no sudo, system-wide installs, upstream changes,
  artifact redistribution or production-data access. Keep scratch changes out
  of git; record hashes, commands, dependencies and failures. Stop and report
  any required privilege or broader installation. Licensing provenance still
  needs review before distribution; this is not legal clearance.
- Owner chooses **split R1**: R1-A functional checkpoint may use a new,
  port-authored zero-capability file fixture compiled with the matching
  upstream toolchain and genuine EUSER/EFSRV. R1-B retains unchanged upstream
  file-app compatibility as a later gate. No current candidate is selected;
  `t_cactw` remains provisional for scheduling. Stop open-ended app searches
  for now. Historical proof requirements are not satisfied by the custom app.
- **FAT model distinction:** current PortFile stages whole files in 64-KiB
  buffers; exec has a separate 64-KiB image buffer. New RFs streaming requires
  new offset-capable backend operations, but does **not** automatically require
  replacing the existing PortFile API or breaking its pinned snapshot behavior.
  Backend errors must be deliberately mapped to upstream RFs-visible results
  (e.g. disk-full), not inherited unchanged from today's internal error map.
- `fat_paths_R1.md` is a useful inventory, **not an approved design or crash
  test result**. FAT-mirror differences can be checked by host tooling; this
  does not prove the guest detects them. No blanket CRC rejection of torn E32
  contents is established by that report. Verify such assertions and whether
  locking covers every shell/PortFile path before adopting them as invariants.

1. Matching, versioned x86 compiler/build tools, **both** EUSER and EFSRV
   artifacts/import maps, and unchanged app imports: neither bx86gcc nor bx86
   is established by local source lists or a host-GCC ABI probe. Never
   translate bx86 ordinals into a guessed bx86gcc export set.
2. Unchanged, ordinary unprivileged proof source: `FHServer.exe` combines file
   access and active scheduling but depends on a larger client/handle-sharing
   workflow; it is **not selected** for R1. `t_cactw.exe` is a focused
   zero-capability scheduling candidate, not yet built or run. Further search
   across six additional packages found no selected simple candidate (see
   `r1_file_proof_search_v0.md` for revisions and heuristic search limits).
   This blocks R1-B, **not definition of the custom R1-A fixture contract**.
   A port-authored fixture cannot replace the upstream R1-B proof.
3. Wire/v0 gives source-derived framing, handle placement, EOF/cancel/panic
   and error behavior. Verify it against a matching *built* EFSRV client and
   narrowly scoped differential/negative tests before treating it as v1.
   Expand the table if the eventual proof app uses operations outside its
   core RFs/RFile slice.
4. Trace genuine startup, TLS, heap, leaves, EUSER import binding and loader
   relocation compatibility against the port's mapped path. Record failure
   tests. Only then review one consistent R1-ABI/v1 for implementation.

R1-FS, R1-EUSER and R1-FAT implementation remain gated on the R1-A ABI
freeze; R1-B source selection need not block that intermediate contract.
R0-QA records a
single 16/16 stable-suite pass in 506 s on the **dirty working tree**;
that pass does not validate these research contracts or a fresh worktree.
R0-HW is an unexecuted worksheet, not a physical-PC result.

## Build/startup/matrix wave review

Inputs: `r1_build_repro_v0.md`, `r1_startup_research_v0.md` (copied from
the isolated startup worktree), and `r1a_acceptance_matrix_v0.md`.

**Build evidence advanced:** EUSER, EFSRV, t_cactw and FHServer now exist as
E32 artifacts in `/tmp/opencode/r1-build-repro/epoc32/release/x86/urel/`.
Coordinator independently checked all four SHA-256 hashes against the build
report and confirmed `kernelhwsrv/` remains clean. This supersedes older
statements that no genuine images were built. It does **not** establish a
sanctioned SDK, official-kit equivalence, runtime correctness or ABI/v1.
EUSER export-address packing remains an inference needing petran-source
confirmation; import-library contents and a clean repeated build are unchecked.
EFSRV changed **18 additional** RTTI entries to ABSENT: with two already
absent, total ABSENT is 20. Earlier counts must distinguish additional vs total.

### Deviation review and constraints

The builder exceeded the card's stop-before-source-patching instruction.
Do not discard those scratch changes or conceal them. Coordinator read the
two-line-area patch diff: D1 changes host detection fallback and D2 retries
`poll` after EINTR. They are plausible host adaptations, **not approved
upstream fixes or proof of equivalent build output**. A follow-up strong
audit must check their consequences and make every deviation reproducible.
D3/D4 compiler flags and version-banner wrappers, D5 fabricated variant,
D7 hand-built petran and D8 manual genexec also need explicit assessment.
Do not call the empty variant an authentic device/SDK configuration.
No further source patches, variant changes or hidden version-check bypasses
without coordinator review. Scratch-only provisioning permission remains;
no system installs, redistribution or cleanup of another agent's scratch.

### Technical direction for the next review (candidate, not freeze)

- Target the **actual bx86gcc-built artifact set**, not bx86/MSVC. Prefer
  genuine `_E32Startup` and DLL dependency/initialization semantics over a
  new substitute startup. Preserve legacy demo startup separately; code-seg
  ID stamping must not overwrite arbitrary demo instructions at entry+12.
- Built app imports include `euser{000a0000}[100039e5].dll`; E32 output is
  reported **PE-style without redundant ordinal lists (PE2)**. The loader
  contract must be derived from these bytes and upstream parsing/lookup,
  not ELF-only code-offset assumptions or naive stripping to a bare name.
- Prefer the already-produced **uncompressed** images for initial bring-up;
  do not add decompression solely because historical tools can emit it.
  Valid header/export bitmap forms in the actual DLLs still need support;
  compression policy is not a reason to ignore variable header sizes.
- Startup report §7 is unfinished: constructors/destructors, DoLeave and
  exception frame, import identity, actual direct-model heap requirements,
  and the promised RTest/console route need closure. `petran` source and
  built outputs are available from the build work; no owner question is
  needed to use them read-only. Static relocation similarity is not runtime
  validation. t_cactw built successfully but has never run.
- The acceptance matrix is a draft, not executable coverage. Its ten REVIEW
  flags remain open. **Fresh-boot persistence must have automated regression
  coverage as well as the physical-PC step**, not an optional QEMU check.
  Pattern/byte comparison is sufficient; a SHA-256 implementation in the
  target fixture is not required. Sharing a regression driver across cases
  is allowed; one separate QEMU boot/script per matrix row is not mandated.
  Do not require a deliberately panicking negative child to be panic-free.
  Physical media must be owner-identified disposable media, not an assumed
  QEMU image attached to a PC. Host mirror comparison requires an explicit
  tool; generic `mtools` does not establish such a check.

The next wave closes **artifact/build integrity**, **startup/console ABI**,
and **bounded matrix corrections**. No loader/FS/FAT implementation, E32
execution or full-suite rerun is authorized by these research cards. ABI/v1
comes before implementation; runtime results then validate (or revise) it.

## Artifact-audit / startup-closure / matrix-errata wave decisions

Inputs: `r1_artifact_audit_v0.md`, `r1_startup_closure_v0.md` and
`r1a_matrix_errata_v0.md`. Coordinator copied startup closure from its
worktree and verified report SHA-256
`d04b1edd79595c328b33741dba19d87bf20aa47c35ae0e3cb9756a6f75ecfa5e`.
Upstream status remains clean. No E32 program has run.

### Build disposition

- Audit reports 25 structural checks per image, exact export/absent sets,
  matching import-library thunks and provider ordinals. The EUSER section
  packing caveat is closed by source + byte inspection. These are audit
  findings, not a coordinator re-execution of the validator or runtime proof.
- **D1/D2/D3/D6 accepted for local research build recipes only**, as the
  exact inspected host adaptations (host detection, EINTR retry, host-tool
  flags and space-free source symlink). Preserve diffs/hashes, compiler exit
  fidelity and path provenance; no upstream edits or broad patch permission.
- **D8 accepted as an explicit generated-header step** using the pinned
  upstream recipe/inputs and audited output hashes, not dummy headers.
  Raptor's skipped extension remains a documented build-system limitation.
- **D4 and D7 not sanctioned as a reproducible basis yet**: metadata cpp
  flags/predefines need inspection; petran requires a complete scripted
  compile/link recipe with diagnostics archived. No new banner deception.
- **D5 rejected as a representative target configuration**. Existing images
  remain experimental inspection fixtures; do not destroy or overwrite them.
  Define a named **port** variant, not a guessed official kit. Owner explicitly
  chooses `SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API` enabled. Other variability
  choices require evidence and an explicit manifest, not blanket enabling.
  Rebuild/export from that manifest before freezing the client/wire contract.
  The RFile64 panic stubs in the old images are not the desired contract.
- Timestamp/path differences establish that raw byte identity is not currently
  available; they do **not** establish that a build cannot be reproducible
  after documented normalization. Two clean builds have not been compared.
  Record raw hashes and narrowly normalized comparisons separately. Existing
  normalized hashes belong to the old config, not a new variant's baseline.

### Startup and scope decisions

- Owner chooses **upstream econs route**, not a port-private console DLL:
  genuine econs plus an explicitly bounded E32WindowServer provider/protocol
  path. Whether the upstream ewsrv is feasible must be audited; the choice
  does not authorize the whole GUI milestone or an unreviewed server shim.
  `RLibrary::Load`/loader service and dynamic dependency lifecycle are real
  dependencies, not bypasses with fake success output.
- Decorated DLL identity parsing now has source/artifact evidence (§4 of
  startup closure). Earlier "wait for STARTUP to explain names" blockers are
  superseded. Exact version/error precedence and absent-export handling still
  need one coherent loader contract with explicit representation of offsets
  versus relocated addresses. Use absent bitmap/entry marker before binding.
- Do not adopt startup closure's proposal to reject all relocations except
  type 0x3000: those are the observed fixtures, **not** all legitimate x86
  relocation types. Preserve existing validated 0x1000/0x2000/0x3000 support.
  Rule G5's `base + export entry` must specify the representation and stage;
  never add a base twice after export relocation. Reject-table version wording
  must compare provider minor >= requested minor (same major), not the reverse.
- Source claims that fast execs/dependency machinery are absent need a build
  and dispatch-table trace, not grep alone; upstream compiled handlers may
  already provide them. PE2 selection alone is not sufficient validation for
  stamping entry+12: validate the genuine entry format and leave pilot code
  untouched. Capability subset checks do not establish process isolation.
- **Heap H1/H2/H3 not selected**. H2 is not justified merely by saying DL-only
  uses Adjust; the disconnected chunk's Adjust path also fails. H3 would
  abandon genuine startup. First trace iUseAdjust, initial commit/addressability,
  allocation thresholds and compiled dispatch; propose a minimal port-owned
  solution preserving genuine EUSER, without implementing R2 early. Runtime
  tests validate this contract after implementation: runtime proof cannot be
  required before authorizing the code needed to run the first fixture.

### Matrix disposition

Adopt the errata's direction (required two-boot persistence, deterministic
byte checks, separate deliberate-negative and scheduler results, six-driver
grouping as a proposal), **not every replacement verbatim**:

- Build/configuration gates are still open despite existing artifacts; do not
  label build-level acceptance green or say no further build is required.
- t_cactw is **built, not run**. A positive scheduler driver must not panic
  unexpectedly; deliberate child panics have independent criteria. Do not
  make a file request mandatory inside unchanged t_cactw.
- Persistence needs explicit Flush/Close and serialized VM stop/reboot on the
  same preserved image. Do not require R3's future shutdown command, nor claim
  killing QEMU is a clean guest halt. Record the exact stop/cache boundary.
- No target SHA library, arbitrary double-free-space rule, generic mtools
  mirror detection, or automatic PC image-transfer assumption. Media/tool
  requirements must be explicit and owner-confirmed; derived dependencies
  should not claim a custom fixture cannot even compile before loader work.
- Ten review flags remain open; errata do not resolve RFs share/error/race
  semantics. No frozen matrix v1 or implementation authorization yet.

Next wave is named-build recipe/config review, direct-model heap/exec closure,
and bounded upstream-console dependency inventory. No repeated broad app search,
no QEMU slot, and no general FS/FAT/loader implementation this wave.

## 2026-10-22 — BUILD-CONFIG / HEAP-EXEC / expanded CONSOLE-PATHS

Inputs: `r1_build_config_v0.md`, `r1_heap_exec_v0.md` and expanded
`r1_console_paths_v0.md`. Heap report copied from its worktree; SHA-256
`78c5b885e7ce284d09840054dbabb9c0ab2bd13caae0c2644ac5458e5ea113da`
matches the handoff. Upstream remains clean. No runtime result is claimed.

### Coordinator decisions

1. **H0 provisionally selected:** preserve genuine EUSER/kernel heap behavior
   without a heap patch. Compiled evidence shows failed slab/paged allocation
   falls back to the DL zone. Earlier large-allocation failure and missing
   trap/scheduler/static-call-handler claims are withdrawn. HE-3…HE-6 must
   validate fallback/capacity/cleanup; decode the user pool before concurrency
   claims. Do not add duplicate handlers or start H1/R2 work preemptively.
2. Approve **port-x86-rfile64-v1** for local research: pinned platform_paths
   plus the 64-bit file API macro only. Keep distinct-locale, enhanced-change-
   notification, split-headers and descriptor-constructor-fix macros off in
   this scope. Record unavailable features; no official-kit parity or permanent
   SDK policy is implied. These are technical scope defaults, not owner questions.
3. Authorize **one clean scratch build attempt** using real host cpp/sed,
   explicit `--toolcheck=off`, fail-closed preflight and live metadata command
   capture. Check `.test` preprocessing/MMP parsing, not just normalized cpp
   content. D7's reconstructed recipe may be used as an experimental local
   tool after fixes below, with exact commands/diagnostics; not official parity.
4. Use fresh canonical `/tmp/opencode/r1-build-v1`, private Wine prefix and
   new epoc32. Check quota first; do not overwrite/delete prior products.
   Record raw hashes and paths. Old-variant hashes are not new-config expected
   outputs. A second cold determinism run needs a later explicit card.
5. **Fix scripts before target build:** capture linker rc immediately and
   fail on nonzero; reject stale outputs by using fresh empty output dirs.
   Archive every stage status/argv; check variant/header hashes and actual
   checkout diff (hashing a stored patch alone is insufficient). Verify real
   cpp/sed paths/versions. A3 compares ordinal identity/absence, not unchanged
   code offsets. The no-Panic-in-24-bytes heuristic is not positive proof of
   RFile64 implementation: inspect function boundaries and expected IPC calls.
   Create missing recipe directories/files explicitly; no hidden scratch reuse.
6. Console inventory needs **strong feasibility review**. Correct MMP UID2
   versus E32 UID1 confusion (DLL UID1 is 0x10000079); MSVC bx86 defs do not
   establish GCC buildability. Trace actual bx86gcc/eabi resolution and linked
   kernfull variant console, not merely `qemu-port/console/`. Missing direct
   EXE imports cannot exclude transitive calls. Verify release heap-mark macros,
   event registration and version semantics, not grep-based absence claims.
7. Owner's upstream-console choice remains binding: review genuine ewsrv
   dependencies including EKDATA/EDISP, domaincli, HAL, system-permanent
   failure behavior and ESHELL autolaunch. A small client request set does
   not authorize a fake write-only server. Explicit service start is already
   allowed for R1; R3 supervision is not required merely to start a test.

Trace remaining loader offset→run-address/pool/entry concerns in a bounded
evidence card before implementation. Ten matrix semantic flags remain open,
especially against the enabled API. Runtime validation follows the reviewed
implementation contract; it is not a circular prerequisite for writing it.

### Scheduling amendment

## 2026-10-22 — clean build / console feasibility / file-contract adoption

## 2026-10-23 — partial batch return: console design / repeat / diagnostic

## 2026-10-23 — loader/FS candidates and fixture returned

## 2026-10-23 — format / service closure / fixture build / console data

Coordinator reran compiled parser host suite: **78 vectors + API checks + 200
mutation smoke cases PASS**, immutable v1 inputs preserved; evidence under
`/tmp/opencode/r1-coordinator-format-20261023`. No sanitizer or guest pass.
Read implementation/API and service boundary. Copied/hash-verified service
report (`de3dd3c38ac5905a5f7811fcafb6b4939053f860f3fc33ee638ce5db825c4fc4`)
and copied console data source without overwriting differing files. Independently
verified fixture image hash `87fde53b240e54ac9aecf4024546b77c91f06a57d83f60c0101c704e0ed6a6a0`.

- Adopt service boundary phases and trusted immutable executable staging as
  internal implementation direction, with one critical correction: pure heap/
  scheduler fixture modes require static EFSRV **binding**, not public FS-ready.
  They may execute before B3. File modes wait for B3; dynamic calls wait for B4.
  Registration/production permanence and nonregistered death-test split stand.
- Exact client-handle rollback remains deferred to dynamic !Loader publication;
  it is not a blocker for parser, graph fake-adapter or kernel bootstrap work.
  Do not implement unsafe publication by guessing handle-table ownership.
- Service report declaration sketches lack concrete allocation/materialization/
  dependency methods and portable type definitions. They are directional, not
  a ready-to-compile shared-header freeze. Graph owner supplies actual portable
  types/interfaces first; kernel adapter consumes that reviewed handoff.
- Parser is C++11; matching user SDK compiler is MinGW GCC 3.4.5. Shared core
  cannot be assumed compilable there. Assign strong compatibility/review card
  with exclusive format ownership; preserve API semantics and test vectors.
  No SDK upgrade or replacement genuine ABI to avoid this constraint.
- Trusted code source initially uses explicit pinned sysbin image manifest;
  CRC/UID are not trust. Keep executable bytes immutable during guest lifetime
  and public RFs writes outside that namespace. Exact manifest/drive belongs
  to bootstrap integration, no arbitrary removable-image compatibility claim.
- Fixture is **compiled, import-audited, never run**. Original README's earlier
  uncompiled status is historical; authoritative later status is build report.
  Console data remains generated/structurally checked only, not target built.

Authorize graph/materialization core with fake-adapter tests, format compatibility
review and one console HAL/keymap build audit as independent cards. Kernel/shared
bootstrap wiring follows actual reviewed core headers. No runtime slot allocated;
stable/manual requirements remain before integrated feature completion.

Read full loader/FS candidates and fixture source/metadata/README. Copied FS
candidate from its worktree, verifying SHA-256
`6a344b93edcc17086121b4528eb306b37967b844d6811c96004299963682126a`.
No code built or executed by coordinator. Reports preserved as proposals.

### Accepted architecture and rejected conflicts

- Adopt explicit L/G regimes, fail-closed G with no pilot fallback, genuine
  kernel objects/execs, port-owned compatible user services, and FAT channel
  separation from legacy PortFile. Initial G supports EXE data/BSS and only
  zero-writable-data DLLs; console closure may need a later extension.
- Future compatible !Loader and !FileServer share one `portfilesrv.exe` host
  with separate threads and single process registration. Initial file-fixture
  service may run before loader registration; distinguish these phases.
  Loader semantic/algorithm code must not require full upstream F32.
- **Reject loader §8 security-off profile.** Preserve forced effective kernel
  flags (including sysbin/enforcement) and true image SID/VID/caps. Parser
  privilege/security metadata propagation is required; no unlock macro/bypass.
  Search and staging must honor effective sysbin rules, not the candidate's
  arbitrary-directory policy. Version/search limits remain labelled port policy.
- Confirmed source: sexec.cpp:2228-2229 sets process/thread permanent flags at
  FsRegisterThread. Consequently production registered-host kill/restart is
  not a valid ordinary A-2 oracle. Test generic ServerTerminated against a
  separately launched **nonregistered, noncritical test service** with genuine
  transport, or explicitly classify production service death as whole-system
  failure. Do not patch permanence away. Pending closure card specifies phases.
- Proposed parser CRC/relocation divergences merit executable independent
  tests. Do not globally repair legacy CRC/generator behavior: keep pilot bytes
  and regressions, implement standard behavior in G. Claims await host tests.
- Accept source-only fixture as initial custom proof, not acceptance. Provisionally
  reserve UID3 E1A0F001 **locally only**, subject to repo collision scan and built
  SID/caps inspection; no global UID registration claim. Argument-only forwarding
  is selected. Build can precede loader execution; all modes still depend on
  EFSRV because this is one linked EXE, even when heap/sched don't call RFs.
- Fixture lacks large-allocation fallback, static constructor/TLS and negative
  coverage; heap mode alone cannot close HE-3…HE-6 or G-10. Keep those later tests.
  Preserve existing assertions until compiler/runtime evidence warrants changes.
- FS partial-write/error, lock order, active-volume restore and flush proposals
  require implementation review; no promise all claimed atomicity follows from
  existing FAT locks. Preseed fixture dirs, defer MkDir/Delete/long-name creation.

Authorize isolated genuine parser implementation/host tests and one private
fixture build/import audit now. Shared graph/materializer, service and FAT
implementation awaits short cross-contract closure and coordinator ABI candidate
integration. This is a scoped format module contract, not blanket R1-ABI freeze.
No QEMU slot yet; all integrated features later require stable/manual coverage.

Read `r1_console_design_candidate_v1.md`, `r1_build_repeat_v1.md` and
`r1_dlltool_diagnostic_v0.md`. Loader/FS reports are still being written;
fixture is incomplete. Do not adopt unsubmitted validator-divergence claims
or partial fixture work as reviewed deliverables.

### Console decisions

- Adopt staged **genuine ESHELL** with ordinary false TestBootSequence for
  the selected console plan. No stub, patched-out launch or synthetic boot flag.
  This is downstream console scope, not extra mandatory first-serial-fixture
  scope. Loader/FS candidates must receive the ESHELL delta before integration.
- Adopt immutable KeyboardIndex=0 and EDisplayMode=0 for fixed-layout VGA-text
  bring-up, with real port EKDATA staged as EKDATA.DLL and EKDATA.00. Keep
  unsupported attributes explicit; this is not dynamic-layout/P&S acceptance.
- Adopt controlled ordinary-key/no-power-domain initial input profile. Stage
  genuine DOMAINCLI, but defer running domain service until power/event scope
  requires it. Do not feed excluded special events or forge domain success.
- Enforce exclusive VGA/input ownership; retain direct-model security limits.
  Raw keyboard delivery must use the documented Kern::AddEvent context, not
  an ISR/arbitrary DFC. Readiness is beyond early rendezvous. Actual effective
  security bits, paging state, HAL descriptor ABI and generated tables require
  integration review/measurement; candidate source reasoning is not runtime proof.

### Build and diagnostic disposition

- Accept repeat as reported successful second cold target build plus separately
  disclosed diagnostic recompiles/relinks. No third cold build authorized.
  Paths/timestamps prevent raw identity; narrowly normalized EFSRV/RPIPE and
  equal-source-path diagnostic matches are useful bounded evidence, not whole-
  SDK reproducibility. Petran, .sym and some map differences remain unclassified.
- **Equal-length different paths do not guarantee raw identity.** Same canonical
  source-path text or reviewed path mapping plus controlled timestamps would
  be needed for such a gate. Do not move/archive v1 merely to rerun it now.
  Timestamp/header CRC normalization must use full field widths; the report's
  short observed changed-byte ranges are not complete format-field definitions.
- Repeat found UTF-16LE source paths in test EXEs: earlier clean-build claims
  that only EUSER embeds paths are superseded. ASCII-only scans are insufficient.
  Audit archive names without masking meaningful symbols/relocations. Four extra
  defs and missing provision stdout remain provenance qualifications.
- DLLTOOL diagnostic isolates the dotted candidate-path suffix-probing failure
  in these binaries under the tested Wine host. Retain explicit M: mapping with
  dot-free visible subtree. Native Windows and exact libiberty source attribution
  remain unverified. Drop reliance on earlier unreproduced dot-free-symlink
  failure. No tool patch or additional workaround changes needed.

Offer bounded console **data source** implementation to a freed strong worker.
No shared kernel/loader/FS wiring or QEMU yet. Wait for loader/FS candidates
before allocating their implementation ownership; no need to fill every slot
with another build/research report.

Coordinator read all three reports. Copied console feasibility with verified
SHA-256 `4f7e3e573a67896b6daa9eddb8a429106bbb707db87c0ad8d176c815abba46b8`.
Independently hashed new euser/efsrv/t_cactw/fhserver bytes: all four match
CLEAN-BUILD §7. This confirms identity only, not the structural audit or runtime.

- Accept disclosed EUSER stage recovery as part of the one authorized attempt.
  The worker exceeded a strict stop-at-blocker interpretation by adding the
  drive mapping; this is accepted retrospectively as a private harness change,
  not permission for future undocumented retries or patches. Preserve failed,
  partial and clean logs. No claim of an uninterrupted first-pass cold build.
- The M: dlltool mapping is acceptable as an explicit research recipe workaround.
  Root cause is unknown; do not assert a dot-directory bug. Assign an independent
  bounded diagnostic card. Do not block loader design on diagnosing Wine paths.
- A second cold-build comparison is useful but not the implementation critical
  path. Offer it as an independent explicit card with a new root and prefix;
  preserve v1. Compare documented normalized structures/bytes as well as raw
  hashes; never normalize away code/import/config differences.
- Adopt FILE-CONTRACT-REVIEW's bounded source conclusions for the next contract
  candidate, not as ten runtime passes. In particular adopt the **source-following
  SetSize position qualification** and no portable growth-zero-fill promise.
  A future backend may zero exposed bytes for safety; do not claim upstream
  guarantees it. Initialize bytes before content assertions. Preserve valid
  share acquisition orders/promotion, exact panic categories, session-local
  collision semantics and cancel-not-a-completion-barrier rules.
- Adopt bounded sizes 0/1/65535/65536/65537 and 1 MiB when media permits. Synthetic
  boundary tests may be specified as isolated protocol tests, not real FAT size
  evidence. Security enforcement/logging flags must be pinned in the candidate.
- Custom R1-A genuine-EUSER/EFSRV fixtures may initially use RDebug diagnostics
  without RTest. This is staged custom-fixture evidence, not unchanged t_cactw
  acceptance, owner-visible console success or R1-B. Upstream econs/ewsrv remains
  the chosen console route; no fake/private replacement console authorized.
- Console report's deterministic-system-panic wording is conditional on power
  model/TestBootSequence reachability. Do not launch ewsrv until this startup
  path is controlled. No stub ESHELL or fabricated nonzero boot flag approved.
  Assess the supported test-boot policy or genuine ESHELL path in a design card.
  Several §7 unknowns are source-resolvable; its claim all require runtime is
  rejected. Runtime-verified toolchain remains acceptance, not a prerequisite
  to authorizing reviewed bring-up implementation.

Next output must be implementation-ready contracts/interfaces and a concrete
fixture, not another broad source inventory. Independent strong cards may run
in parallel; only coordinator integrates shared ABI/matrix docs. Implementation
of shared loader/FS/kernel files awaits candidate review; no QEMU slot yet.

### Early return — R1-LOADER-EVIDENCE

`r1_loader_evidence_v0.md` returned while CLEAN-BUILD and CONSOLE-FEASIBILITY
remain active (owner-relayed state). Accept the located kernel conversion
at pcodeseg.cpp:293 and published 96 MiB user-pool arithmetic as source
evidence, not runtime verification or maximum concurrency. Preserve raw report.
Important qualifications before using its proposed tests/ledger:

- **OA-2 is rejected:** sf_lepoc.cpp:547-549 rejects zero/absent-marker export
  addresses with KErrNotSupported; it must not bind the importer to the marker.
  The regression should assert load rejection plus cleanup, not a callable IAT.
- OA-1's alternative data-address alignment check cannot prove entry-address
  conversion; separate those assertions. OA-5 needs a genuine test DLL, not
  assumptions that the restricted pilot loader already supports dependency init.
- Section 1's wording that E32Image.iFileEntryPoint is separately relocated
  by export relocation is misleading: export relocation changes export entries,
  not that field. Verify actual marshaling/copy-back when defining the interface;
  a DCodeSeg::Info assignment alone does not trace the entire exec wrapper.
- Host Python header-validator coverage is not equivalent to testing every
  guest validator branch; pool/header quantities are config-dependent, not
  runtime measurements. Existing tests do not establish genuine PE2 startup.
- Old-file API package facts stay valid but do not forbid the owner-selected
  RFile64 scope. Errata are proposals with coordinator qualifications, not
  already-applied matrix fixes. PortFile snapshot behavior remains preserved.

Offer an independent **strong** R1-FILE-CONTRACT-REVIEW card now; it is not
blocked on the pending build/console reports and does not change their scope.
Review the named macro configuration at source level and explicitly reserve
artifact-dependent confirmation. No QEMU or implementation authorized.

**Clean-build storage amendment (owner approved):** initial preflight found
only ~1112 MB effective /tmp quota free versus ~2.75 GB estimated need; no
allocation/build occurred, so the single attempt remains unused. Canonical
root is now `/home/yuzi/.cache/sybian-r1/r1-build-v1`, superseding the /tmp path
above. Check destination unused and disk/quota headroom first. Keep private
Wine/temp/intermediates there where supported, record all actual paths and
preserve every previous scratch product. No cleanup authorized. New path
strings/hashes are recorded provenance, not equality failures against the old
configuration. Other active cards continue unchanged.

Waves are checkpoints, **not mandatory idle barriers**. After coordinator
review, a completed worker may take another independent owned card while
others continue. Give cheaper workers larger bounded inventories/coverage
bundles, not architectural decisions or busywork. No worker self-expands
ownership; coordinator keeps shared docs and actual card states current.
