# Parallel AI work plan

Read this with [`project_roadmap.md`](project_roadmap.md) (milestones and
acceptance gates), [`full_os_checklist.md`](full_os_checklist.md) (what exists),
and [`manual_test_order.md`](manual_test_order.md) (current manual checks).
This is a task-routing and handoff plan, **not** permission to implement every
future feature at once. Start with R0/R1, then revise this board as contracts
and hardware facts become known.

## Coordinator and owner interface

**The planner/coordinator AI in the owner's main conversation is the sole
technical intake and dispatch point.** The owner forwards other agents'
reports (or makes their files available), receives ready-to-paste assignments
in waves of three, answers only project-policy questions raised by the
coordinator, reviews/commits/pushes changes, and performs physical-PC checks
when a gate requires them. The owner is **not** expected to arbitrate ABI
disagreements, read every source report, or negotiate directly with workers.
The coordinator cannot automatically see another agent's conversation or
worktree: the owner must relay a handoff and/or make its report available.

After each wave, the coordinator should:

1. Read **all completed reports and diffs**, confirm their actual paths/base
   revision, distinguish source evidence from inference, and mark unfinished
   agents *in progress* rather than assuming their partial status is final.
2. Reconcile contradictions and document the decision or remaining blocker
   in the coordinator's planning docs. Do **not** freeze an interface because
   multiple reports repeat an unverified assumption. Check with upstream
   evidence or issue another bounded research card.
3. Update the wave ledger below and, when justified, roadmap/checklist/manual
   docs. Only the coordinator edits shared planning documents during a wave;
   worker research reports stay attributed and can be corrected by follow-up.
4. Ask the owner any genuine scope/legal/hardware trade-off through this
   conversation's **question tool**, with a recommended choice and consequences.
   If a worker is blocked by technical uncertainty, it records the question
   in its handoff **to the coordinator**, not directly to the owner. The
   coordinator answers from evidence, assigns follow-up research, or escalates
   just the policy choice to the owner.
5. Return **three ready-to-paste card briefs** for the next wave (strong/cheap
   tiers, exact outputs and owned paths), or explicitly explain why a slot
   should remain idle. Do not imply agents have been launched, files merged,
   suites passed, or contracts frozen without evidence.

Workers should not send the owner unfiltered requests for everyday technical
decisions. They should finish their bounded task, label unknowns and wait for
the next coordinated wave. If immediate clarification is essential to avoid
unsafe or destructive action, **stop work and report the blocker**; do not
silently pick an ABI, erase a disk, or change scope. This coordination role
persists across waves and future sessions: read this section before assigning
or accepting work.

**No mandatory idle barrier:** a completed worker may receive an independent
card after coordinator review while others continue. Cheaper workers may
receive larger bounded inventory/coverage bundles; no self-assigned scope
or taking another worker's files. Waves remain integration checkpoints.

### Current wave ledger (planning state; update from completed handoffs)

| Card | State | Evidence / next coordinator action |
| --- | --- | --- |
| R1-ABI | Report returned, **not frozen** | `r1_abi_contract_v0.md`; matching binary imports and proof sources unresolved. |
| R1-TOOLCHAIN | Local investigation returned; follow-up found documented route | `r1_toolchain_research_v0.md` local findings superseded in part by SDK-ROUTE; no genuine EUSER/EFSRV artifacts built yet. Do not mix bx86/bx86gcc ordinals. |
| R0-HW | Worksheet returned; **no PC verification** | `hw_worksheet.md`; non-isohybrid USB warning reflected in `hw_tests.md`. Owner records actual reference-PC observations later. |
| R0-QA | Report returned; single 16/16 pass on dirty tree | `qa_inventory_R0.md` §2/§8 records 506 s and shared log paths. Baseline commit and later combined-tree rerun remain due; no claim of a clean-worktree pass. |
| R1-PROOF-SOURCE | Research returned, proof **unselected** | `r1_proof_source_research_v0.md`; FHServer plus client is too broad for a default R1 choice; `t_cactw` is an unbuilt scheduler candidate. |
| R1-WIRE | Research returned, **not frozen** | `r1_wire_research_v0.md` is copied from the isolated worktree; file close=28 (ABI/v0 wrongly says 27), Size/Seek use 8-byte packages. See `r1_research_reconciliation_v0.md`. |
| R1-STARTUP-PATHS | Inventory returned, strong review pending | `startup_paths_R1.md` maps the current port startup chain, not genuine upstream EUSER startup compatibility. |
| R1-SDK-ROUTE | Returned; documented route, **not reproduced** | `r1_sdk_route_research_v0.md`; Raptor explicitly falls back to transformed eabi EFSRV exports. Scratch-only provisioning/build attempt authorized by owner. |
| R1-FILE-PROOF-SEARCH | Returned; **no proof selected** | `r1_file_proof_search_v0.md` copied from isolated worktree; six external packages searched, not exhaustive. Owner split R1-A custom functional fixture from later R1-B unchanged-app gate; further searches paused. |
| R1-FAT-PATHS | Inventory returned; design review still due | `fat_paths_R1.md`; separate data/exec 64-KiB limits. Coordinator qualifications in `r1_research_reconciliation_v0.md`; not a mandate to replace legacy PortFile or inherit backend errors unchanged. |
| R1-BUILD-REPRO | Returned; experimental E32 artifacts built, **never executed** | `r1_build_repro_v0.md`; coordinator verified four image hashes. D1–D8 need audit, including unapproved scratch source patches, fabricated variant, banner wrappers and manual tools/header generation. |
| R1-STARTUP | Returned; source trace **incomplete** | `r1_startup_research_v0.md` copied to main docs; genuine entry/TLS/dependency gaps identified. Artifact identity, constructors/traps/heap and RTest/console path remain open. |
| R1-A-TEST-MATRIX | Draft returned; ten review flags open | `r1a_acceptance_matrix_v0.md`; coordinator corrections include required automated fresh-boot persistence, no target SHA requirement, and realistic disposable PC media. |
| R1-ARTIFACT-AUDIT | Returned; structural/import audit passed, configuration rejected as representative | `r1_artifact_audit_v0.md`; section packing/libs closed. D5 disables RFile64; D4/D7 still need review; no two-build reproducibility test. |
| R1-STARTUP-CLOSURE | Returned; candidate contract needs corrections | `r1_startup_closure_v0.md` copied/hash-checked. Decorated names traced; heap/exec and console dependencies still open. See coordinator qualifications, not blanket worker freeze claims. |
| R1-A-MATRIX-ERRATA | Returned; corrections accepted in principle, not verbatim | `r1a_matrix_errata_v0.md`; stale t_cactw/build-green and clean-halt assumptions still need correction. Ten semantic review flags open. |
| R1-BUILD-CONFIG | Returned; research configuration approved | `r1_build_config_v0.md`; single clean attempt authorized, real cpp/sed + toolcheck-off policy; script/preflight fixes required first. |
| R1-HEAP-EXEC | Returned; H0 provisionally selected | `r1_heap_exec_v0.md` copied/hash-checked; exec handlers linked, heap DL fallback traced. Runtime/pool checks due. |
| R1-CONSOLE-PATHS | Expanded inventory returned; strong review due | `r1_console_paths_v0.md`; correct UID interpretation/transitive-call reasoning and trace linked port path before adopting claims. |
| R1-CLEAN-BUILD | Returned; named-config build succeeded with disclosed recovery | Four image hashes independently verified; EUSER recovery accepted, not a second cold build. No runtime proof. |
| R1-CONSOLE-FEASIBILITY | Returned; copied/hash-verified | Genuine route is staged work; no ESHELL stub/boot-flag bypass approved. |
| R1-LOADER-EVIDENCE | Returned; accepted with coordinator corrections | Entry conversion/pool evidence useful; OA-2 absent-export binding assertion rejected. See reconciliation. |
| R1-FILE-CONTRACT-REVIEW | Returned; bounded source findings adopted | Source-following SetSize qualification, package/panic/share/cancel corrections accepted for candidate; runtime checks remain. |
| R1-LOADER-CONTRACT / R1-FS-DESIGN | Returned; architecture accepted with conflicts corrected | FS report copied/hash-verified; security-off profile rejected, registered-host permanence requires phase/test separation. |
| R1-FIXTURE | Returned, source only | Uncompiled/unrun; local UID reservation subject to collision/artifact checks. |
| R1-G-FORMAT | Returned; coordinator host rerun passed | 78 vectors + API + 200 smoke cases; C++11/user GCC 3.4.5 compatibility needs correction/review. No guest run. |
| R1-SERVICE-CLOSURE | Returned; copied/hash-verified, adopted with qualifications | Pure heap/sched may precede FS readiness; concrete core headers and dynamic client-handle rollback still due. |
| R1-FIXTURE-BUILD | Returned; compiled/import-audited | Image hash independently checked, zero caps, 72 present imports; never executed. |
| R1-CONSOLE-DESIGN | Returned; bounded plan adopted | Genuine ESHELL, immutable fixed-layout HAL profile and ordinary-key/no-domain first scope; runtime/integration still due. |
| R1-BUILD-REPEAT | Returned; bounded comparison accepted | Second cold target build plus diagnostics, not raw/whole-SDK reproducibility; no third build allocated. |
| R1-DLLTOOL-DIAGNOSTIC | Returned; workaround evidence accepted | Dotted candidate suffix-probing failure isolated under tested Wine; keep explicit drive mapping. |
| R1-CONSOLE-DATA-SOURCE | Returned; copied into main | Host generation/structure only, target build pending. |
| R1-G-CORE | Returned; reviewed, coordinator host rerun PASS | Five closures, 52 object/8 source failure positions, 26 negatives; kernel adapter offered. |
| R1-FORMAT-COMPAT | Returned; reviewed/integrated | C++98 conversion; coordinator 78/API/200 rerun PASS. Worker MinGW compile-only has Wine quota diagnostic. |
| R1-CONSOLE-DATA-BUILD | Returned; exports REJECTED | HAL/EKDATA linked but wrong ordinals; diagnostic-only. Strict-definition correction authorized. |
| R1-KERNEL-ADAPTER | Returned; source/ownership review | Genuine compiled adapter; kernel-thread handle/suspended; runtime CS/ref safety unverified. Integration card 20 offered. |
| R1-FS-CORE | Returned; copied/backend hash verified | Portable state + genuine compile-only bridge; synchronous bounded subset, no backend/queue. Channel card 21 offered. |
| R1-FAT-OFFSET | Returned; source reviewed, baseline/diff verified | Worker compiled RAM tests/kernel compile pass; flush hook and legacy quarantine pending, no coordinator test rerun. |
| R1-CONSOLE-STRICT-BUILD | Returned via owner; evidence recovery pending | HAL six ordinals reportedly pass; EKDATA rejected. Delivery /tmp worktree unavailable; no independent v2 review/copy. |
| R1-STORAGE-FLUSH-QUARANTINE | Returned; reviewed source/delivery hashes | Real ATA/AHCI flush + boot-lifetime dispatch-device FAT quarantine; worker host/compile passes, no runtime. Worker available for integration fixes. |
| R1-EKDATA-LOCAL-DEF | Recovery stop resolved; continuation offered | Original prose missing, surviving evidence sufficient; coordinator recovery note waives gate. Resume one EKDATA-only correction/build, no HAL rebuild. |
| R1-G-BOOT-INTEGRATE / R1-FS-FAT-CHANNEL | Offered, not confirmed launched | Cards 20/21; private G link and channel implementation; shared-header storage/launch edits separate; no QEMU. |
| R1-EKDATA-LOCAL-DEF (latest) | Returned; v3 report read/image hash verified | Exactly three genuine exports worker-audited, no guest; HAL recipe reconciliation remains separate. |
| R1-G-BOOT-INTEGRATE (latest) | Returned; source integrated from private worktree | Both links worker-pass; G launch wired in main preserving storage header declarations. No runtime yet. |
| R1-FS-FAT-CHANNEL (latest) | Returned; reviewed candidate remains private | Failed Open reply copyout needs key cleanup, response validation and genuine lifetime review before copy. |
| R1-G-STABLE-INTEGRATION | Returned; completed | G heap/sched passed in guest, stable 15/15 both configs, stack fix applied, FS channel copied/kernel-unit compiled. |
| R1-CONSOLE-STAGING | Returned; completed | HAL/EKDATA assets staged byte-identical, manifest produced, no console-closure claim. |
| R1-FS-CHANNEL-MAKEFILE | Returned; completed | Makefile wired off-by-default, kernel channel unit compiled, host suites pass, stable 15/15 PASS with honest G skip; service smoke not-run pending card 29 host policy. |
| R1-CONSOLE-G-MANIFEST | Returned; completed | HAL/EKDATA v3 pins wired into stage_r1_g.py; verify() iterates all pins; default builds asset-free; no mismatch. |
| R1-PRIVILEGED-HOST-POLICY | Returned; completed | Services header + policy doc produced; source-backed specifics verified against genuine kernel sources; no build/runtime/QEMU. Remaining host manifest/cap/phase-ack decisions noted. |
| R1-TOOLCHAIN-BLOCKER (34) | Returned; completed | EFSRV def/import library verified in cold build roots (efsrv.lib from eabi/efsrvu.def, no bx86/efsrvu.def by design). Key ordinals present/non-ABSENT. No bx86/bx86gcc mixing. |
| R1-PORTFILESRV-BUILD (35) | Substantially complete | portfilesrv.exe built as PEi-i386 (6240 bytes), zero-caps ProtServ, EUSER+EFSRV imports verified. Waiting on card 36 for E32 conversion. No QEMU, no registration. |
| R1-PETRAN-E32-CONVERT (36) | Returned; completed | E32 portfilesrv image (884 bytes, sha d91ebcf2...), UIDs 1000007a/00000000/00002323, SID 0, VID 70000001, Caps ProtServ-only, imports 1221/1953 verified. Blocks nothing further. |
| R1-FILE-REGRESSION (32) | Offered, not launched | Card 32; QEMU file-service regression with exclusive slot; wires test_stable.py; waits on card 36 E32 image (now available). |
| R1-OWNER-SMOKE (33) | Offered, not launched | Card 33; owner physical-PC smoke test; waits on card 32. |

On new handoffs update this table before proposing another wave. Research
status and verification limits are summarized in `r1_research_reconciliation_v0.md`.

## Before starting multiple agents

1. **Owner authorizes integration; coordinator reviews it.** The current
   workspace has uncommitted work; do
   not ask another agent to reset it, clean it, or branch from an older HEAD
   expecting to see it. First review/commit the desired baseline yourself.
   Agents should use separate worktrees or independent clones at that *same*
   commit. Do not run several coding agents in one working tree. The upstream
   `kernelhwsrv/` is currently ignored rather than tracked as a submodule:
   a new worktree will **not** contain it automatically. Arrange a read-only
   upstream checkout accessible at the expected `kernelhwsrv/` path in each
   worktree (or a local symlink to the existing checkout, if appropriate),
   verify the source revision, and never commit the checkout/symlink. A clean
   public clone is not buildable until the upstream dependency is explicitly
   provisioned; do not let agents misdiagnose that as a code regression.
2. Give each agent **one card ID below**, a branch/worktree, and a list of
   owned paths. One agent owns each shared file at a time. No one changes
   `kernelhwsrv/` (read-only reference) or publishes binaries, releases,
   licenses, or commits/pushes on your behalf unless separately authorized.
3. A **strong model** is for reverse-engineering upstream contracts, x86 ABI,
   security/memory architecture, FAT mutation/crash cases, IPC concurrency,
   translator design and cross-subsystem integration. A **cheaper model** is
   for bounded inventory, scripts, tests against an agreed interface, docs,
   reproducible examples and hardware-report templates. Tier means task
   difficulty, not a named provider or a guarantee of correctness. A cheap
   agent must flag unclear contracts to the coordinator, not guess kernel behavior.
4. All agents must state assumptions, evidence, changed paths, tests run and
   remaining risks. Research may end in a design note with **no code**. An
   implementation needs a QEMU regression and a manual step (or a clearly
   labeled blocked draft), and must not declare a milestone complete without
   the owner's physical-PC report. Keep default `make` rebuilding the ISOs;
   the workspace path contains a space, so use the existing build conventions.

## Isolation and integration rules

- Give each worktree its **own disk/log names** under `/tmp/opencode`, e.g.
  `/tmp/opencode/agent-r1-fs-a.img`. Existing `test_*.py` scripts hardcode
  some paths and `test_stable.py` writes a shared stabilization log directory:
  **do not run the existing suites concurrently** unless a test has actually
  been made configurable and supplied with unique paths. Reserve a single
  host-level QEMU/stable-suite slot; never point two QEMUs at one writable
  image. Stagger heavy builds too. Research agents need no build slot.
- Shared hotspots: `qemu-port/Makefile`, `user/user.mk`, `tools/test_stable.py`,
  `variant/kern_shell.cpp`, `variant/e32load.cpp`, `variant/fat32.cpp`,
  `variant/genuine_glue.cpp`, `user/runtime.[ch]`, `user/ipc.[ch]`, and the
  roadmap/checklist/manual docs. Assign an **integration owner** for edits to
  these; others propose a patch or test in their own branch and identify the
  desired interface. Never have two agents rewrite the same hotspot in
  parallel. A change touching two cards needs an interface agreement first.
- Integrate one coherent slice at a time: review diff against current baseline,
  build, run its targeted regression, then `python3 tools/test_stable.py` from
  `qemu-port/`. After merge, update the checklist/manual steps and rerun the
  suite on the combined tree in the reserved test slot; a green branch test
  is not a green integration.
  If one task depends on another, rebase/merge **after** its dependency lands
  and rerun tests. Record the commit and test log in the handoff.
- Do not silently change an upstream ABI to make a fixture pass. Mark a
  compatibility assumption as *unverified* until checked against genuine
  source and, where possible, an upstream-built binary. Keep QEMU-ready and
  hardware-verified as separate labels.

## Work cards: R0 and R1

The cards are small enough to hand to one AI each. “Can overlap” means
research/test development on separate branches; it does **not** mean their
kernel edits can all be merged concurrently. The R1 proof and its required
file-service scope are defined in `project_roadmap.md`.

| Card | Model | Deliverable and owned area | Starts after / merge gate |
| --- | --- | --- | --- |
| **R0-HW** | Cheaper | Inspect existing `docs/hw_tests.md`; draft a **non-destructive** VGA-only BIOS/PS2/IDE-or-unknown smoke sheet and a hardware facts form. No hardware claims. Own a new hardware worksheet only. | Now; owner supplies results at major gates. |
| **R0-QA** | Cheaper | Inventory current tests, slow/flaky cases, fixture collisions, stale documentation claims; propose precise doc/test updates without changing test semantics. Own a report in `docs/`. | Now; integrator chooses changes. |
| **R1-ABI** | **Strong** | Existing `r1_abi_contract_v0.md` is research, **not frozen**. Integrator reviews the focused research cards below and issues v1 only when imports, wire framing, startup and proof candidates are evidenced. No upstream edits. | v0 returned; **v1 contract freeze required** before R1-FS/R1-EUSER implementation. |
| **R1-TOOLCHAIN** | **Strong** | Time-box bx86gcc first: locate/reproduce toolchain and matching EUSER **and EFSRV** x86 import metadata, check ordinal/calling convention/startup with built artifacts. If unavailable, report evidence and compare bx86 feasibility; do not mix export sets or invent one. Own a new research report. | Now, parallel with other *research* cards; feeds R1-ABI/v1. |
| **R1-PROOF-SOURCE** | **Strong** | Find/build viable **unchanged, unprivileged** upstream-source x86 app(s). Prefer one file read/write + active scheduling app; two separate unchanged apps are acceptable. Audit full source/MMP/library/capability/console graph. Do not modify proof source to fit current port. Own a new research report. | Now; feeds R1-ABI/v1. |
| **R1-WIRE** | **Strong** | Trace `RSubSessionBase`, `TIpcArgs`, descriptors, masked function IDs and actual server dispatch for RFs/RFile; produce a versioned request/response table and error/share/async semantics with source evidence. No service code yet. Own a new research report. | Now; feeds R1-ABI/v1 and R1-FS. |
| **R1-STARTUP** | **Strong** | Trace genuine process startup, TLS, heap, leaves, EUSER import resolution and x86 loader relocation requirements; identify port gaps and test vectors. Own a new research report. | Now; feeds R1-ABI/v1, R1-EUSER and R1-LOADER. |
| **R1-SDK-ROUTE** | **Strong** | Find a reproducible **versioned and legally usable** matching upstream x86 build/SDK route for both EUSER and EFSRV; establish real artifact/import evidence if locally available, otherwise exact acquisition/build blockers. Own one new report; no binary redistribution or synthesized exports. | Now; follows R1-TOOLCHAIN/v0, feeds ABI/v1. |
| **R1-FILE-PROOF-SEARCH** | **Strong** | Search other available upstream packages/legitimate source indexes for an unchanged zero-capability file read/write candidate simpler than FHServer. Trace its full MMP/source/dependency graph; pair with unbuilt `t_cactw` only provisionally. Own one new report. | Now; follows R1-PROOF-SOURCE/v0, feeds ABI/v1. |
| **R1-FAT-PATHS** | Cheaper | Inventory the existing **port** FAT read/write/seek/flush, directory and 64-KiB limit call paths and test coverage with exact file/line evidence. Identify unanswered questions for R1-FAT, **without proposing or implementing FAT concurrency/crash algorithms**. Own one new report. | Now, independent research; strong R1-FAT design still awaits the ABI freeze. |
| **R1-BUILD-REPRO** | **Strong** | Attempt documented upstream x86 toolchain/header/tool provisioning and minimal genuine builds in isolated scratch; report artifacts/import maps or first reproducible blocker. Own new report only in repo; scratch tools/build products never committed. | Owner authorized scratch-only work; no sudo/system installs, upstream edits, redistribution or QEMU. |
| **R1-A-TEST-MATRIX** | Cheaper | Draft a source-referenced R1-A acceptance matrix for a custom upstream-toolchain file fixture, using existing wire evidence without inventing an ABI. Own one new report; no executable test/source or shared-manual edits yet. | Now; strong review and ABI freeze precede implementation. |
| **R1-BUILD-CONFIG** | **Strong** | Specify named port variant with owner-selected RFile64 enabled, audit metadata preprocessing, script petran provenance and an exact future clean-build recipe. Own new report only; no target rebuild or unreviewed source changes this card. | D1/D2/D3/D6/D8 approved narrowly for local research; D4/D7 and variant manifest await review. |
| **R1-HEAP-EXEC** | **Strong** | Trace actual compiled direct-model heap and fast-exec/dependency dispatch, close conditional heap claims and propose minimal port-owned genuine-EUSER bring-up design/test vectors. Own new report; no kernel changes. | Now; before selecting H1/H2/H3 or freezing startup ABI. |
| **R1-CONSOLE-PATHS** | Cheaper | Inventory genuine econs/ewsrv/loader-service sources, MMP dependencies and protocol requests needed by RTest/t_cactw. Separate full server graph from minimum exercised calls. Own new report; no replacement-console design. | Owner chose upstream console route; strong feasibility review follows inventory. |
| **R1-FAT** | **Strong** | Design and implement FAT backend needs for general offset read/write/seek, metadata/error handling and >64 KiB. Own `variant/fat32.cpp` plus a dedicated regression *only after* R1-ABI's app contract is known. Document interrupted-write risks and disposable-image recovery. | Contract freeze; integrate before R1-FS large-file paths. Do not claim near-4-GiB support without a recorded manual test. |
| **R1-FS** | **Strong** | RFs/RFile-compatible service: server sessions, multi-client file lifecycle, sharing/errors, paths and persistence, using existing genuine IPC and FAT backend. Own new service files; coordinate all IPC/runtime/FAT edits with integrator. | R1-ABI; staged after backend interfaces. Dedicated QEMU regression plus manual step; not upstream F32. |
| **R1-EUSER** | **Strong** | Genuine EUSER/CBase build/link/import ABI and active-object timer/cancel scheduling. Own new port-side build/veneer files; request exclusive ownership before editing loader, build or runtime hotspots. | R1-ABI; can be researched while FAT/FS work proceeds; integrate after interfaces are stable. |
| **R1-LOADER** | **Strong** | Loader/reaper failure-path hardening and import/ordinal validation for upstream-built EUSER app. Own `variant/e32load.cpp` **exclusively** during its slot; add negative E32 and cleanup tests. | R1-ABI; coordinate EUSER changes, integrate before proof. |
| **R1-FS-TEST** | Cheaper with strong review | Build black-box RFs scenarios (multi-client, missing file, reopen/EOF, >64 KiB boundaries, crash/reboot persistence) from the **frozen** service API; disposable QEMU images only. Own new `tools/test_*.py`/fixtures; propose `test_stable.py` wiring to integrator. | After contract and runnable service; no ad-hoc ABI invention. |
| **R1-PROOF** | **Strong** | R1-A: build clearly labelled custom zero-capability file fixture with matching upstream toolchain and genuine EUSER/EFSRV; demonstrate full file/scheduler acceptance. R1-B: unchanged upstream-source compatibility app(s), no source pruning. Keep provenance and claims separate. | R1-A ABI, FS/EUSER/LOADER integration; suite + PC gate for each checkpoint. R1-A alone is not overall R1 completion. |
| **R1-DOC** | Cheaper | Track accurate compatibility claims, upstream artifact provenance and file-size test matrix. Draft docs from merged evidence; **do not choose a license or redistribute binaries**. | Can run alongside research; publish docs only after proof data is verified. |

**Original post-ABI/v0 wave (historical, reports now returned):** R1-TOOLCHAIN, R1-PROOF-SOURCE,
R1-WIRE and R1-STARTUP as separate **strong-model research** jobs in isolated
worktrees; R0-HW/R0-QA are cheaper-model work independent of them. If budget
is tight, prioritize **R1-TOOLCHAIN and R1-PROOF-SOURCE first**, because no
proof/contract can be frozen without a feasible binary path. These jobs are
**not** R1-FS, R1-EUSER or R1-FAT implementation. The integrator checks their
evidence against `r1_abi_contract_v0.md`, records any unresolved blockers and
issues a single reviewed `R1-ABI/v1` contract. Only then assign exclusive
code ownership for the implementation wave. Paying several agents to build
conflicting RFs implementations before that gate is likely to waste work.
For **current** assignments use the wave ledger and `r1_next_wave.md`, not
these historical examples. R1-A's custom-fixture contract can be frozen
without selecting the later R1-B unchanged file app; build/startup/wire
evidence and strong review are still required.

## Later cards (do not start as shared-kernel implementation yet)

| Roadmap gate | Strong model work | Cheaper model work | Parallelism condition |
| --- | --- | --- | --- |
| **R2 memory** | Non-PAE layout, genuine memory-model implications, CR3/context switching, user-copy/guard/fault containment; one architecture owner. | Black-box bad-pointer/stack/chunk tests and source-API inventory. | Begin designs after R1 contract; merge paging only after integrated R1 baseline. Never two MMU implementations. |
| **R3 usability** | Service supervisor/restart policy, console focus/session architecture and foreground job/lifecycle semantics. | SDK app/install docs, console interaction tests, shutdown worksheet. | Agree IPC/session/job interface before shell and console implementations diverge. |
| **R4 loader/network** | DLL/security ABI and NIC/socket architecture with QEMU packet tests. | Hardware inventory, sample networking apps, protocol test harnesses. | Networking follows stable file/console services; one owner for shared loader/IRQ code. |
| **R5 ARM** | ARMv6 ARM+Thumb/VFP dynamic translation, code cache, x86/ARM ABI and containment review; separate ARMv7/NEON stage. | Documented instruction vectors, differential harness, provenance inventory. | Research can overlap R1–R3; **no** third-party binaries until R2 containment. Do not merge speculative kernel translator code into stable path. |
| **R6 GUI** | Window/focus/graphics design after core gates. | App UX mockups and test-plan drafting. | No implementation before the roadmap's service prerequisites. |

## Copy-paste briefs for other AI sessions

Replace bracketed fields. Paste **one card per agent**; include the actual
base commit/branch and owned paths. The AI should not treat roadmap items as
already implemented. If its work uncovers a blocker, it reports it instead
of editing someone else's files.

**Shortest dispatch message** (then paste the appropriate brief below):

> Read `qemu-port/docs/parallel_work.md` and take card **[ID]** only.
> Work from commit **[hash]** in your own worktree **[path]**. Model tier:
> **[strong/cheaper]**. Owned files: **[paths]**. Stop at the listed
> dependency gate; send the handoff format from that document. Do not touch
> the read-only upstream tree, another card's files or shared test images.

Example dispatches from the current baseline: give a strong AI
**R1-TOOLCHAIN** to settle the matching bx86gcc EUSER/EFSRV binary route and
another strong AI **R1-PROOF-SOURCE** to find unchanged proof apps. Give a
cheaper AI **R0-HW** for the non-destructive Pentium 4 worksheet. Do not
assign **R1-FS** implementation until `R1-ABI/v1` has been reviewed and
frozen.

### Research card (good for R1-ABI; use a strong model)

> Work in `[separate worktree]` at commit `[hash]`. Read
> `qemu-port/docs/parallel_work.md` (card **[ID]**), `project_roadmap.md`,
> `full_os_checklist.md` and relevant upstream files in `kernelhwsrv/`
> **read-only**. Your assignment is **[narrow question]**. Own only
> **[new report path]**. Give file/line evidence, a proposed interface,
> unknowns, and executable acceptance tests. Do not implement outside the
> owned paths, touch shared build/test wiring, or claim hardware verification.
> Stop, record the blocker for the coordinator and avoid inventing an ABI
> if an upstream assumption cannot be confirmed. Return a short handoff:
> findings, paths changed, verification, dependencies.

### Implementation card (strong; test-card can use cheaper model)

> Work in `[separate worktree]` at commit `[hash]`. Read
> `qemu-port/docs/parallel_work.md` card **[ID]** and
> `qemu-port/docs/project_roadmap.md` gate **[R#]**. Your frozen interface is
> `[contract/revision]`. You exclusively own `[paths]`; `[shared paths]`
> require integrator approval before edits. Implement **[one bounded slice]**.
> Add a focused QEMU regression and draft the corresponding manual step;
> do not change `kernelhwsrv/`. Use unique `/tmp/opencode/[agent]` disk/log
> paths; never reuse another QEMU's writable image. Run `[targeted command]`
> and, if feasible, `python3 tools/test_stable.py` from `qemu-port/`.
> Report exact commands/results and any tests not run. No commit/push/release
> or physical-hardware claim without separate authorization. Hand off the
> diff, interface changes, known failures and next dependency.

### Integration review (strong; one at a time)

> Review card **[ID]** against `[base/current commit]` and its frozen
> contract. Inspect diffs for upstream changes, ABI drift, shared-path
> conflicts, tests and manual steps; do not discard other work. Integrate
> only this slice, run the targeted test and the **combined** full stable
> suite, then record pass/fail with logs. Mark **QEMU-ready** only if the
> roadmap gate's automated criteria pass; reserve **hardware-verified** for
> the owner's physical-PC report. Summarize remaining blockers and update
> the task board/checklist accordingly.

## Task handoff format

Each agent should end with:

```text
Card / base commit / owned branch:
Contract revision and dependencies:
What changed (paths) or researched (source evidence):
Tests run (exact commands, result, log paths):
Manual step drafted/updated:
Compatibility and failure limits:
Blocked assumptions or cross-card interface requests:
Recommended integration order and next owner:
```

If a subtask has no independent testable artifact or shares the same fragile
kernel function with another, **do not parallelize that implementation**.
Split research, test design and documentation instead, then let one owner
make the code change.
