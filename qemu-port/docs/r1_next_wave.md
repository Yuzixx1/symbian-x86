# Dispatch — implementation contracts, fixture, independent build confirmation

Card **34 completed** (EFSRV verified). Card **35 substantially complete** —
`portfilesrv.exe` built as PEi-i386 (6240 bytes), zero-caps ProtServ,
EUSER+EFSRV imports verified. Card **36 completed** — petran E32 conversion
succeeded; E32 portfilesrv image (884 bytes, sha d91ebcf2...) verified with
UIDs/SID/VID/caps and import audit. Card **32 offered** — QEMU file-service
regression with exclusive slot. Historical dispatch details follow
in [`r1_implementation_dispatch_v1.md`](r1_implementation_dispatch_v1.md).
Two continuing and two freed strong workers; no QEMU slot allocated. Earlier
status paragraphs below are historical, not live assignments.

2026-10-23 latest: cards 1/2/4 also returned and reviewed. New active offers
are cards 8–10 below; do not relaunch completed earlier cards. Card 7 remains
offered/not confirmed unless owner reports launch. No QEMU allocated.

2026-10-23 live update: cards 1/2 drafting, card 4 incomplete; cards 3/5/6
returned and reviewed. Do not relaunch completed builds/diagnostics. New bounded
source card 7 below is offered to a freed strong worker. Prior briefs remain
for provenance and active ownership; no mandatory batch barrier.

Status: **offered, not confirmed launched**, 2026-10-22. Previous wave returned.
Recommended: four strong workers on cards 1–4; add a fifth strong worker for
card 5 if useful. Optional cheaper diagnostic card 6 is independent. No fixed
three-model limit. Return early for review; no mandatory wave barrier.

## Common instructions

Read latest `r1_research_reconciliation_v0.md`, roadmap and parallel-work rules.
Use isolated worktrees with current dirty baseline/reports explicitly available;
old HEAD alone is insufficient. Record actual provenance. Upstream read-only.
No commits/pushes, redistribution, system installs or deletion of old scratch.
No agent spawning. Own only listed new outputs. Do not edit shared ABI/matrix,
Makefile, loader/kernel/FS code or another worker's outputs. No E32 execution,
QEMU or hardware; no test slot allocated. No independent ABI freeze. Return
blockers to coordinator, not owner. Label source/byte evidence vs proposals.

## 1 — R1-LOADER-CONTRACT (strong; critical path)

Own `qemu-port/docs/r1_loader_contract_candidate_v1.md`. Produce an
implementation-ready candidate, not more inventory, using new CLEAN-BUILD
images and startup/heap/loader reports plus coordinator corrections.

Specify genuine PE2/V validation, section/relocation/export representations at
each stage, decorated name/UID/version matching, absent-export rejection,
validated entry+12 identity stamp, genuine dependency init/teardown and H0
startup. Trace actual kernel create-info copyback where necessary; do not add
bases twice. Preserve legacy pilot behavior as a separate selected regime.

Choose and justify a minimal architecture supporting both initial kernel-
launched fixtures and future genuine RLibrary/RProcess !Loader calls. State
whether proposed user loader service reuses upstream F32 loader code or uses
a port-owned compatible provider; include protocol ownership and typed IPC
boundaries, not an unspecified kernel magic bridge. Full F32 is not implicitly
authorized. Identify exact port files/functions and sequence bounded code cards.
Give failure rollback/refcounts, DLL data policy, security configuration and
test/manual assertions. If a necessary decision cannot be made from evidence,
present bounded alternatives, not a runtime-before-code gate. No implementation.

## 2 — R1-FS-DESIGN (strong; critical path)

Own `qemu-port/docs/r1_fs_design_candidate_v1.md`. Turn adopted FILE-CONTRACT-
REVIEW into implementation-ready transport/operation/FAT interfaces. Choose
and justify where the compatible !FileServer runs and how genuine typed IPC,
thread ownership, client panic, async message lifetime and death cleanup work.
Do not silently claim it is the full upstream file server.

Specify exact C/C++ interface sketches, owned-file split, errors/partial I/O,
share/position state, async queue locking/completion, offset FAT allocation and
flush semantics. Preserve PortFile snapshot/disk-full behavior separately.
Use enabled RFile64 framing and adopted SetSize qualification. Pin baseline
security/cache/plugin/media assumptions. Include staged implementation cards,
six-driver regression map, two-boot persistence and disposable-PC manual steps.
Do not choose loader internals: declare required loader services as interfaces.
No implementation or runtime. Ten source-reviewed flags are not runtime passes.

## 3 — R1-CONSOLE-DESIGN (strong)

Own `qemu-port/docs/r1_console_design_candidate_v1.md`. Convert feasibility
findings into a bounded genuine econs/ewsrv bring-up design and file-owned
implementation cards. Close source-resolvable unknowns (UIDs, domaincli init,
security flags, linked VGA mapping, HAL/P&S) instead of requiring runtime for
all. Specify HAL tables/display mapping/keymap/event registration and required
loader/FS interfaces. Distinguish idle input from broken event registration.

Resolve conditional ESHELL/TestBootSequence path with a legitimate documented
test-boot configuration or staged genuine ESHELL requirements. Do not implement
or assume approval for a stub ESHELL, fake success flag, private console or
write-only server. If real scope tradeoff remains, give concrete alternatives
to coordinator. Preserve owner console choice; custom serial R1-A fixtures may
precede it but cannot satisfy t_cactw/console acceptance. No builds this card.

## 4 — R1-FIXTURE (strong; source implementation only)

Own new directory `qemu-port/tests/r1a_fixture/` only. Create a labelled custom
zero-capability fixture source and matching MMP/bld.inf plus README, using the
matching genuine EUSER/EFSRV SDK APIs, not PortFile/PortCon or replacement
runtime. No upstream/shared-code edits. **No build yet**; do not alter scratch
toolchains. Fixture source is authorized; loader/FS implementation is not.

Implement bounded first positive modes: heap/CBase/leave/trap checks, genuine
CActive timer scheduling, and RFs/RFile + small RFile64 lifecycle/byte/EOF/Flush
checks. Use RDebug diagnostics, not RTest/console, for this early custom mode.
Use explicit deterministic markers and errors; keep service launch privileged
and outside zero-capability app. Preseeded disposable paths, no system paths.
No intentional child panic/race/giant-file tests in this first fixture.
README maps modes to adopted requirements, SDK imports/build command, pending
dependencies, expected markers and proposed manual procedure. Clearly mark
uncompiled/unrun status. Don't claim R1-B or visible-console success. Optional
host source checks allowed, but no executable guest tests or stable-suite wiring
until build/loader interfaces are reviewed. Return compile risks explicitly.

## 5 — R1-BUILD-REPEAT (strong; optional independent confirmation)

Own `qemu-port/docs/r1_build_repeat_v1.md`. Authorize one additional cold build
in fresh `/home/yuzi/.cache/sybian-r1/r1-build-v1-repeat`, private Wine prefix,
new epoc32 and temporary dirs there. Verify unused destination/quota first;
stop on conflict. Preserve v1 and all /tmp evidence. Use reviewed v1 inputs,
variant, fixed harness and explicit M: dlltool mapping; record all live hashes,
commands and stage statuses. Reuse pinned tool binaries but not target-generated
headers/objects. No new patches or undocumented retries. Stop at first failure.

Compare v1 and repeat raw hashes plus narrowly normalized timestamps/checksums/
known source path strings. Explain normalization by format/offset; do not mask
code/import/config differences. Check full exports/imports/headers/RFile64 bodies,
triage warnings that could invalidate results. Output one clear comparison,
not official SDK equivalence. No E32 execution/QEMU. Root-cause diagnostics may
run separately; do not change mapping policy mid-build to chase it.

## 6 — R1-DLLTOOL-DIAGNOSTIC (optional cheaper worker)

Own `qemu-port/docs/r1_dlltool_diagnostic_v0.md`. Bounded host-only reproducer
matrix for Wine dlltool spawning as.exe: path length, spaces/dot component,
Z: versus explicit drive and --as override. Read existing logs/binaries; use
own home-backed scratch/prefix, never active build roots/prefixes. No target
build, upstream/tool patch or system changes. Report observed cause only if
isolated; otherwise document workaround and negative results. Not a prerequisite
for loader/FS contracts. No cleanup of other workers' files.

## 7 — R1-CONSOLE-DATA-SOURCE (strong; source implementation)

Follow Common instructions except source changes explicitly authorized here.
Own new `qemu-port/r1-console/hal/`, `qemu-port/r1-console/keymap/` and
`qemu-port/r1-console/README.md` only. No shared Makefile/variant edits,
toolchain changes, target builds, E32/QEMU or system installation.

Read adopted console candidate and latest coordinator rulings. Implement
port-owned HAL config.hcf/values.hda and keymap.cpp/ekdata.mmp/bld.inf using
genuine upstream table/generator/export formats. Select immutable KeyboardIndex
0, display mode 0, one VGA text screen; derive CPU/page/tick values from actual
port constants. Keep unsupported functions unsupported; no copied ARM defaults
or guessed values. Stage policy must name EKDATA.DLL/EKDATA.00 with genuine
UID2/UID3 and fixed exports. No private console/translator implementation.

Provide ordinary US-layout keys/modifiers/arrows with explicit Symbian scan-code
mapping, ordinary-key/no-power-domain profile and missing-key boundaries.
Reuse licensed upstream table material only with provenance/notices preserved;
do not edit upstream or claim distribution clearance. Avoid wholesale unrelated
template copying. Include a small host-only structural checker for table bounds,
duplicates, required keys/exports/config values if useful; no guest test claims.

README: exact future generation/build/staging recipe, required genuine HAL
sources, expected interfaces, provenance and unresolved integration constraints.
Specify future QEMU assertions/manual steps for HAL values, key translation and
layout alias load; no feature is complete until integrated stable-suite coverage
and manual verification. Stop on an unresolved required constant rather than
inventing it. Coordinate questions through main assistant; do not touch active
loader/FS/fixture files. Return standard handoff with checks run and unbuilt status.

## 8 — R1-G-FORMAT (strong; host-tested implementation)

Own new `qemu-port/loader/e32g_format.{h,cpp}` and new
`qemu-port/tests/e32g_format/` only. Implement portable bounded immutable
G parser using loader candidate §3 and latest coordinator corrections. No
existing loader/generator/CRC changes; no kernel graph/materializer/service
implementation, Makefile or stable-suite edits. Host compilation/tests allowed,
not target build or E32/QEMU execution. New module must be freestanding-friendly,
without STL/exceptions or host pointers embedded in serialized formats.

Provide explicit byte/range views and error outputs, parsed image/reloc/import/
export views and decorated identity extraction; standard polynomial CRC, UID
checksum, variable V/PE2 header, correct relocation section extents/counts,
export-hole validation and checked arithmetic. Support legitimate relocation
types 1000/2000/3000; separate unsupported profile from corruption. Parse image
security metadata without overriding it. Document exact module API in owned
tests README, distinguishing link/run/file offsets. Do not implement search or
security enforcement policy prematurely.

Host tests must exercise actual compiled C++ parser, not just a Python mirror:
all four immutable v1 images (hash-check), CRC index-205 regression, correct
relocation-size boundary, malformed semantic mutations with CRC recomputed,
overflow/truncation/name/bitmap/entry cases and legitimate existing relocation
kinds. Do not use pilot generator as sole oracle. Generate synthetic vectors
independently and compare relevant upstream rules. Explicit fixture/scratch paths,
no shared logs. Record commands/results. Preserve upstream and artifact hashes.
Document future QEMU/stable wiring and manual assertions; host passes do not
make a feature QEMU-ready. Stop on an interface conflict rather than inventing
graph/kernel integration. No blanket ABI freeze.

## 9 — R1-SERVICE-CLOSURE (strong; short critical-path contract)

Own `qemu-port/docs/r1_service_boundary_v1.md` only. Reconcile loader/FS/console
candidates into exact shared host and bootstrap interfaces; no broad inventory.
Latest coordinator rulings override security-off and kill/restart assumptions.

Specify portfilesrv.exe process image, !FileServer/!Loader thread ownership,
initial unregistered FS-only versus registered loader phase, security metadata/
trusted bootstrap policy, effective sysbin search and actual E32Loader handle/
client-thread transfer. Preserve genuine FsRegisterThread permanence; map death
tests to noncritical nonregistered transport fixture versus production failure.
Define snapshot API, FAT-channel authorization and creation/registration order
without an interval granting unintended clients coarse loader authority.
Pin exact C++ declarations shared by graph/object adapters and services, ownership,
errors/rollback, common file list and implementation sequence. Account for first
fixture's static EFSRV dependency and no-writable-DLL limit. Determine which
remaining candidate claims block first bootstrap vs later dynamic/console work.
Resolve proposals from source; return genuine residual choices to coordinator.
No implementation, builds, runtime or independent freeze. Completion target is
one concrete boundary document ready for coordinator integration, not another
open-ended requirements catalogue.

## 10 — R1-FIXTURE-BUILD (strong; one isolated build/import audit)

Own new `qemu-port/docs/r1_fixture_build_v1.md` only. Authorize one fixture
build attempt in unused `/home/yuzi/.cache/sybian-r1/r1-fixture-build-v1` with
private prefix/tmp/epoc32; check quota/destination first. Preserve all prior
roots, outputs and source fixture. May provision byte-verified copies of pinned
v1 tools/headers/libs/startup objects and reviewed harness/drive mapping: this
is an **incremental fixture build**, not a third cold SDK build. Record reused
inputs/hashes and exact host/variant. No new patches/banner bypasses.

Scan repo for UID3 E1A0F001 collisions before build; report conflicts, don't
silently assign a new identity. Build submitted source using genuine EUSER/
EFSRV/SDK startup; inspect actual UIDs/SID/VID/caps (NONE), PE2/V header,
imports and every required ordinal in v1 providers, data/BSS/stamp/reloc facts,
command-line API and RDebug output imports. No execution. No changing other
workers' files to make it compile: stop on first compiler/link blocker with
exact proposed minimal fixture fix, for coordinator/source-owner review.

Archive exact argv/status/warnings and hashes; update only owned report, not
fixture README/source or shared docs. State all modes require EFSRV binding.
Capture compile warnings affecting allocation/TRAP/async/overloads. Target build
success proves no heap/FS/scheduler/runtime behavior. No QEMU or hardware.

## 11 — R1-G-CORE (strong; implementation + fake-adapter tests)

Own new `qemu-port/loader/e32g_plan.{h,cpp}`, `e32g_load.{h,cpp}`,
`r1_image_source.h`, `r1_object_adapter.h`, and
`qemu-port/tests/e32g_core/` only. Do not edit format files or shared kernel/
build/docs. Read latest reconciliation, actual parser API and service boundary.

Implement bounded immutable graph planning, identity/version/capability matching,
dependency order/cycle rejection, and transaction materialization/rollback using
fake object/source adapters. Convert service sketches into complete portable
interfaces including allocation, copy/fixup, dependencies, finalization and
publication state—not declarations with unimplemented magic. No kernel/user
adapter or !Loader IPC implementation in this card. Support first bootstrap:
EXE data/BSS, zero-writable-data DLLs, genuine PE2 export/relocation semantics,
validated veneer/stamp and fail-closed G. Standard headers/fixed-width types,
C++98-compatible shared code, no STL/exceptions/runtime allocator assumption.
Coordinate parser changes only through coordinator; format worker owns it.

Make allocation/storage ownership explicit via bounded caller storage or an
injected allocator; no giant automatic stack arrays. Carry parsed security
unchanged. Plans retain snapshots until commit/abort; distinguish link/load/run
and data template, reference transfer, stillborn failure. No security-off policy.
Test actual core with nonidentity bases, synthetic code/data targets, all v1
bindings plus fixture closure, zero/absent imports, constraints/budgets/cycles,
stamp conflicts and failure at each adapter boundary. Assert ledger/snapshot
baseline and no publication on failure. Explicit unique scratch paths.

Publish exact core-header handoff **as soon as stable** for coordinator review,
then finish tests. Kernel adapter can be assigned after that checkpoint rather
than waiting for whole batch. Host compilation/tests allowed, no E32/QEMU,
target builds, commits or upstream changes. README records manual/future guest
assertions and unsupported boundaries; fake-adapter pass is not kernel success.

## 12 — R1-FORMAT-COMPAT (strong; independent review + bounded implementation)

Own existing new `loader/e32g_format.{h,cpp}` and `tests/e32g_format/` only.
Preserve all baseline/user work outside those paths. Independently review bounds,
profile checks and accessors, including degenerate names/section overlaps and
relocation/export marker cases. Do not assume 78 vectors prove full parity.

Make shared format module compile with matching GCC 3.4.5 C++98 without changing
API semantics or requiring a newer user compiler. C++11 aggregate/zero-init syntax
must not leak into shared core. Validate strict host C++98 freestanding compile
and actual pinned MinGW compile-only in private home-backed scratch/prefix;
copy tools as needed, do not mutate SDK roots/prefixes. No target link/E32 run.
Retain modern host test support and all 78+API+200 cases; add independent review
regressions for real defects, not speculative loosening. Sanitizers only if
already available, no installs. Document exact changes/compile evidence and any
remaining issues in owned README. Keep public header signatures stable; request
coordinator approval before an interface change affecting graph worker.

## 13 — R1-CONSOLE-DATA-BUILD (strong; one isolated build/audit)

Own new `qemu-port/docs/r1_console_data_build_v1.md` only. One authorized
incremental HAL/EKDATA build attempt in unused home-backed
`/home/yuzi/.cache/sybian-r1/r1-console-data-build-v1`, private prefix/tmp/SDK.
Check unused destination/quota. Reuse hash-verified reviewed SDK/tool inputs
without generated target objects; preserve all old roots. Copy console source
and read-only upstream references into reviewed layout; do not change main source.
Use real upstream HAL generator, adopted immutable data and genuine eabi defs.

Build HAL and EKDATA; audit headers, security/UID/export/import identities,
generated table values, DLL data/BSS and aliases EKDATA.DLL/EKDATA.00 as
byte-identical staged copies. Explicitly report whether writable-data DLLs need
later loader extension; do not alter libraries to force zero-data profile.
Inspect ordinary-key table compilation/exports, no translation runtime claim.
Stop on first compile/link blocker and report exact minimal fix; no new upstream
patch, NOSTRICTDEF, private translator or source rewrite. Own only report; no
shared Makefile edits, QEMU/E32, additional cold SDK build or retry authorized.
