# R1-FIXTURE: port-authored R1-A source fixture

**UNCOMPILED / UNRUN.** No E32 image was built or executed for this card. This
is custom R1-A source, **not unchanged-app R1-B evidence**, not visible-console
success, and not acceptance-matrix completion. Output uses genuine
`RDebug::Printf`, not RTest, PortCon, PortFile or a replacement runtime.

Baseline: port `fa62323eea7bc96cefd666e40f5189567731bd44`, existing dirty
workspace on `main`; no dedicated owned branch was created. Only this directory
is owned by this card. Upstream reference:
`kernelhwsrv` `0c3208650587ac0230aed8a74e9bddb5288023eb`.

## Invocation and assets

The launcher must pass the **argument string only** to `User::CommandLine`:

| Argument string | Independent gate / adopted requirement subset |
| --- | --- |
| `heap` | Genuine heap, CBase construction/destruction, `new(ELeave)`, cleanup-stack unwinding, explicit leave/TRAP, release allocation-cell balance |
| `sched` | S-1 bounded custom positive: CTimer/CActive completion reaches RunL once via installed CActiveScheduler; no file dependency |
| `file D` | F-1/F-2 sizes 0, 1, 65535, 65536, 65537; Flush/Close/reopen/byte compare; bounded F-3 seek/position; F-4 shrink/regrow discriminator and retained prefix; F-5 EOF/zero/over-capacity read; E-1 absent leaf/parent; async-read positive only |
| `file1m D` | Same plus explicit opt-in 1048576-byte file; requires provisioned free space |
| `rfile64 D` | REVIEW-9 enabled RFile64 API/package smoke check on 200 bytes, truncated to 100; no large-file capability claim |
| `persist D` | Required second-boot F-2/P-1 deterministic read-only comparison of the five boundary files |
| `persist1m D` | Same six-file verification after `file1m D` |

`D` is a **placeholder drive letter**, not a fixed system drive. Supply the
letter assigned by the harness to a confirmed disposable writable volume.
Preseed `<drive>:\R1TEST\A\` and `<drive>:\R1TEST\B\`. Each mode explicitly
sets and checks its session path; it never creates directories or launches
services. No private/system directory is used. Filenames are 8.3-safe:

* A: `DATA0.BIN` .. `DATA4.BIN`, optional `DATA5.BIN`, and `SEEK.BIN`.
* B: `SMALL64.BIN`.
* `ABSENT.BIN` and `NOEXIST\ABSENT.BIN` must remain absent in A.

Creation uses `Create`, not Replace/Delete. A repeat writer run against old
files deliberately fails (typically AlreadyExists). Reset the **disposable
image through the harness**, not by adding deletion to this fixture. Persistence
mode neither creates nor resizes files. A file-mode failure can leave partial
files: no rollback or manifest is promised. Accept persistence only when paired
with a fully successful first-boot log and the same preserved image.

Byte `i` is `i % 251`, generated and compared in bounded 1024-byte chunks.
No target SHA library is needed. Newly grown bytes have **no zero-fill assertion**.
The SetSize discriminator sets position 150, shrinks to 100, regrows to 200,
then seeds distinct bytes at 100/150 through a separate compatible share before
reading the original share without an intervening position-changing call.
This deliberately adopts the reviewed implementation's retained-position rule,
not the conflicting immediate-clamp API comment. EOF is success plus zero
length, never KErrEof. Every Write and explicit Flush result is checked;
void Close cannot certify durability. A successful async read with
`User::WaitForRequest` is **not** A-1 cancel/exactly-once/race coverage.

Contract references: `../../docs/r1_next_wave.md` §4,
`r1_research_reconciliation_v0.md`, `r1_file_contract_review_v0.md` §§3–5,10,13,
and `r1a_matrix_errata_v0.md`. These qualifications supersede stale SHA,
close-opcode, EOF and SetSize prose in `r1a_acceptance_matrix_v0.md`.

## Proposed build (NOT executed or verified)

Use a separately authorized build slot and matching clean SDK/toolchain.
The existing clean-build scratch root is a **read-only reference for this card**;
do not run this command there without separate coordinator authorization.
After preparing an authorized isolated environment equivalent to
`port-x86-rfile64-v1`, the proposed Raptor command is:

```sh
sbs --toolcheck=off -d \
  -b "/home/yuzi/Documents/Sybian REAL/qemu-port/tests/r1a_fixture/bld.inf" \
  -c x86_urel.test -p r1a_fixture.mmp
```

`PRJ_TESTMMPFILES` follows the t_cactw layout. The variant must inject
`platform_paths.hrh` plus `SYMBIAN_ENABLE_64_BIT_FILE_SERVER_API` through the
approved PRODUCT_INCLUDE/forced-include route. Do not turn the feature off to
make the source compile. MMP imports `euser.lib efsrv.lib` and the standard
SDK EXE startup path; no local runtime is provided. Headers are `e32base.h`,
`e32debug.h`, `f32file.h` (which includes enabled `f32file64.h`/inlines).

Direct API/import families (the **actual emitted import table is pending**):

* EUSER: `RDebug::Printf(const char*,...)`, `User::CommandLineLength`,
  `CommandLine`, `Leave`, `CountAllocCells`, `WaitForRequest`, `CTrapCleanup::New`,
  cleanup-stack PushL/Pop/PopAndDestroy, trap support, CBase destructor and
  allocation/free helpers emitted by `new(ELeave)`/delete; descriptor operations
  (Copy/Append/Compare/CompareF/Trim/SetLength/Ptr/Zero and SDK inlines).
* EUSER scheduler: CActiveScheduler constructor/destructor, Install/Add/Start/Stop;
  CTimer constructor/destructor/ConstructL/After, CActive Cancel/RunError and
  inherited machinery, with RTimer CreateLocal/After/Cancel and handle Close
  used transitively by genuine CTimer.
* EFSRV: RFs Connect/Close/SetSessionPath/SessionPath; RFile Create/Open/Close,
  Read (current, positioned, length, async), Write (current/positioned), Seek,
  Size, SetSize, Flush; RFile64 Create/Open, explicit **TInt64** Read/Write,
  Size/Seek/SetSize, plus inherited Close/Flush and current-read inline path.
* Genuine startup/dependency imports are additional, not exhausted by this list.
  Inspect the built EXE and matching DLL identity/ordinal tables before binding.

Compile risks: MMP platform macro/include resolution, Raptor test configuration
and paths containing spaces; inline allocation/TRAP/compiler ABI support;
CBase virtual machinery and descriptor helper imports; EUSER late appended
exports and exact non-ABSENT ordinal binding; RFile64 overload/feature/inlining
selection. Explicit TInt64 casts avoid accidental TInt/TUint overload selection.
`RDebug::Printf` was source/export-checked, but its linked runtime trace route
has not been demonstrated here. Allocation-count balance may expose genuine
one-time SDK allocation behavior despite cleanup-stack warm-up; investigate
before changing the assertion. The placeholder UID3 `0xE1A0F001` needs coordinator
reservation/collision review and emitted SID/capability inspection; do not infer
actual identity merely from the MMP. Capability NONE is not SID zero.

## Markers and failure limits

Output begins `R1A:entry:port-authored:BEGIN`. Successful invocations end with
`R1A:<mode>:done:PASS`, after the mode-specific marker(s):

* `R1A:heap:heap-cbase-leave-trap:PASS`
* `R1A:sched:timer-runl:PASS`
* `R1A:file:verify:PASS size=65537` (and each other requested size),
  `R1A:file:seek-resize-async-positive:PASS`, `R1A:file:missing-errors:PASS`
* `R1A:rfile64:small-rfile64:PASS`
* `R1A:persist:verify:PASS size=65537` (and the other sizes).

`file1m`/`persist1m` use their literal names in markers. Individual assertion
failures print `R1A:<mode>:<step>:FAIL actual=<n> expected=<n>`; byte mismatches
also identify the offset. Trapped failure ends `done:FAIL actual=<n>` and returns
a negative process result. Allocation failure before trap setup has a dedicated
`R1A:entry:cleanup:FAIL` marker. Missing final markers are **not a pass**. Bounded
data/loop sizes do not bound a broken kernel wait: the external harness must
enforce a timeout and collect panic/death/exit information. No self-watchdog,
intentional panic, server kill, giant-file allocation or race test is included.

## Pending dependencies and proposed manual procedure

The coordinator must arrange genuine loader/startup/heap/TLS/trap binding,
matching EUSER/EFSRV imports and feature variant, request-status and timer/wait
transport, typed IPC, session-local file handles/path handling, and a writable
offset-capable FAT backend with explicit Flush. Privileged infrastructure must
start version-compatible `!FileServer` **before RFs::Connect**; Connect does not
launch it. No privileged mount/service operation belongs in this zero-cap app.

Draft procedure (blocked on build/runtime integration; **not performed**):

1. Review/compile in a separately authorized isolated build environment; record
   source/config/artifact hashes, emitted imports, UID/SID and zero capabilities.
2. Reserve the single QEMU slot. Allocate unique disposable image/log names
   under `/tmp/opencode`; never share a writable image between VMs. Preseed the
   two public directories and verify absence of target and negative-test names.
3. Boot the integrated image, start required services through the privileged
   harness, confirm debug-trace capture and exact argument forwarding. Run
   `heap`, then separately `sched`; record final markers and process results.
4. Run `file <letter>` (or select `file1m <letter>` **before** first creation if
   space permits). Require all size/semantic/error markers and final PASS. Run
   `rfile64 <letter>` independently; do not make scheduler success a file gate.
5. Preserve that volume, stop the VM through the agreed harness boundary,
   record VM-stop method and host-cache handling, then serialize a second boot
   on the same image. Start services and invoke the matching persistence mode.
   Do not rerun the writer, repair/recreate the image, or confuse same-boot
   reopen verification with persistence. VM termination is not a clean guest
   shutdown claim; Flush is not atomicity/power-loss protection.
6. Integration owner later runs existing stable regressions in the reserved
   slot; this directory does not wire itself into `test_stable.py`.
7. Only after coordinator approval and disposable-media confirmation, draft the
   equivalent reference-PC run with debug-log capture and shell recovery check.
   No physical PC was used here; a kernel trace is not visible genuine console
   evidence, and any visible-console/R1-B gate remains separate.

Excluded coverage: full M-1/M-2 sharing/session matrix, malformed raw IPC and
package canaries, N-series panic/size-limit tests, cancel races and exactly-once
proof, A-2 server death, Flush fault injection, full E-1 bad-name precedence,
crash-integrity guarantees and unchanged-app/console proof. Small RFile64 is
only tested-to-200-bytes **if eventually run**, not a >2-GiB claim.

Card verification is limited to host-side source/metadata inspection. No build,
toolchain mutation, guest execution, QEMU/hardware run, commit or push is part of
this delivery.

Host checks performed: a Python text-only check passed the four-file inventory,
MMP capability/library metadata, seven argument-mode names, boundary-size
constants, forbidden API-token scan, final newlines and trailing whitespace.
`git diff --check -- tests/r1a_fixture` returned no output, but the directory is
untracked, so that command alone does not validate these new files; the explicit
Python whitespace check covered them. These are not syntax/compile tests.
