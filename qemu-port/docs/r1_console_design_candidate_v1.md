# R1-CONSOLE-DESIGN — genuine console candidate v1

Status: **candidate for coordinator review, not an ABI freeze or implementation**.
Verification: source inspection only; no builds, E32 execution, QEMU or hardware.
QEMU-ready: **none**. Hardware-verified: **none**.

## 0. Scope and provenance

Owned output: `qemu-port/docs/r1_console_design_candidate_v1.md` only. Preserve
the owner's genuine ECONS/EWSRV choice. The earlier custom serial R1-A fixture
may precede this work; it cannot satisfy unchanged `t_cactw` or console acceptance.
No private server, PortCon adapter, write-only ECONS, ESHELL stub, unconditional
test-boot success, upstream patch, build or implementation is authorized here.

Port HEAD: `fa62323eea7bc96cefd666e40f5189567731bd44`. Draft worktree:
`/tmp/opencode/r1-console-design`, detached at that HEAD. `main-baseline` and
`kernelhwsrv` links expose the current dirty main tree and upstream as read-only
references by working convention (not filesystem-enforced). Old HEAD alone was
not used as the current implementation baseline. Main tree had 68 dirty/untracked
paths at the recorded inspection; peer changes were not altered.
Upstream reference HEAD: `0c3208650587ac0230aed8a74e9bddb5288023eb`.

Selected SHA-256 identities of the actual main-tree inputs read:

| Input | SHA-256 |
|---|---|
| `qemu-port/Makefile` | `77a5365a2a3a333c3e575ef9198d665ad6b972979fc09df729626fded1db385b` |
| `qemu-port/variant/qemu_variant_rom.cpp` | `99ce3f7c8b68b036c87bc15afb9093c76ee6e729e3afe54c26738fc385ab575d` |
| `qemu-port/boot/ported/bootcpu_gas.S` | `67ddff7e23560cf4ca0ca0e3c1b3025c8dc5a2c86eac6a6bc384d1eae26a7e54` |
| `qemu-port/boot/ported/bootmain_gas.S` | `fab634aeff95d2c78b0502959cacea20211e9c49f2da2a76d9532320919f2978` |
| `qemu-port/variant/qemu_variant_int.cpp` | `21f7a5279e024e98815172f107b58fea9469bc2ad59425ad6e140f5384c79fa6` |
| `qemu-port/variant/kern_shell.cpp` | `2d6c2d67bc0dce6f5b7a39b3452ad6d0769a1aa16730013221e94cedc8261959` |
| `qemu-port/docs/r1_next_wave.md` | `29e657324c90e75b1aebb6cd4a1b74febff8153b2b3c10f7b33fce34403dbaef` |
| `qemu-port/docs/r1_research_reconciliation_v0.md` | `86758d2dde86fe1946a36c587b5426c2a5635c57767ebe88fe65df297490d222` |

Read the roadmap, current board, dispatch, reconciliation and console feasibility
report. Labels below: **S** = inspected source; **I** = inference; **P** = proposed
contract; **R** = future binary/runtime confirmation. `EK/` abbreviates
`kernelhwsrv/kernel/eka/`; `FS/` abbreviates
`kernelhwsrv/userlibandfileserver/fileserver/`. All paths are source-root relative.
No new binary audit is claimed; the earlier feasibility artifact observations
are attributed to that report, not independently rerun here.

## 1. Candidate decision

**P — recommend staged genuine ESHELL, ordinary false TestBootSequence, genuine
EWSRV/ECONS/EDISP/EKTRAN and port-owned EKDATA + HAL variant data.** Start services
explicitly after reviewed loader/FS bring-up; full ESTART/R3 supervision is not
required. Keep ewsrv's system-permanent policy and genuine bidirectional IPC.

This is several bounded implementation cards, not a console-only patch. The
loader and FS candidates must provide real process/DLL lifecycle and the FS
operations identified below. Runtime tests follow reviewed implementation; they
are not a circular condition for authorizing its source work.

## 2. Source-resolved corrections and dispositions

### 2.1 Image identity and matching

**S:** `EK/include/e32uid.h:31-34,75-76,93-106` distinguishes DLL UID1
`0x10000079`, EXE UID1 `0x1000007a`, shared-library UID2 `0x1000008d`, and Unicode
keyboard-data/translator UID3s. MMP `uid A B` supplies UID2/UID3, not UID1.

| Image | UID1 | UID2 | UID3 | Source |
|---|---|---|---|---|
| ECONS | `10000079` | `1000008d` | `100039e7` | `EK/ewsrv/econs.mmp:24-45`; `EK/euser/cbase/ub_cons.cpp:199-210` requested type |
| EWSRV | `1000007a` | `1000008c` | `100000bf` | `EK/ewsrv/ewsrv.mmp:24-39` |
| EKTRAN | `10000079` | `1000008d` | `100039e1` | `EK/ewsrv/ektran.mmp:24-40`; `EK/include/e32uid.h:105-106` |
| EDISP | `10000079` | `1000008d` | `100039eb` | `EK/drivers/edisp/edisp.mmp:24-35,55-58` |
| EKDATA | `10000079` | `1000008d` | `100039e0` | `kernelhwsrv/bsptemplate/asspandvariant/template_variant/cakdtemplate.mmp:27-44` |
| HAL | `10000079` | `1000008d` | `100039e8` | `kernelhwsrv/halservices/hal/src/hal_lib.mmp:18-20` (import-library identity; final image must be checked) |
| ESHELL | `1000007a` | `00000000` | `101f7ae7` | `FS/etshell/eshell.mmp:24-27,60` |

`0x1000008c` is the exact upstream EWSRV UID2; no named symbolic meaning was
located in the inspected UID header. A symbolic nickname is **not required** to
implement the exact identity; do not relabel it UID1 or manufacture a new UID.
DOMAINCLI's MMP does not specify a UID pair (`domainmgr/group/domaincli.mmp:24-41`);
record its actual generated image header rather than inventing one.

**S:** UID matching is no longer untraced: `FS/sfile/sf_image.h:342-345` accepts
requested zero or exact equality. `sf_ldr.cpp:1729-1736` applies that independently
to all three words; `sf_lepoc.cpp:989,1047` uses it for candidate images.
ECONS's `(null, shared-library, console)` request therefore wildcards only UID1.
**R:** final build identity, exports, imports and capability words remain to audit.

### 2.2 DOMAINCLI initialization versus domain service

**S:** `domainmgr/group/domaincli.mmp:24-35` builds only `domaincli.cpp`, links
EUSER, capability All. Inspection of that translation unit found no `E32Dll`
initializer or namespace-scope service-connecting object. The explicit
`RDmDomainManager::WaitForInitialization` routine at `domaincli.cpp:609-651`
attaches/subscribes to P&S; `Connect()` at `:670-673` explicitly connects the
power hierarchy. These functions do not execute merely because DOMAINCLI is
imported. Normal generated DLL startup still has to work in the loader.

EWSRV imports DOMAINCLI regardless of events. Its switch-on/off/case events
explicitly connect/transition and panic on failure (`EK/ewsrv/ws_main.cpp:671-719`);
special-key processing also has domain use (`ws_win.cpp:1583` and surrounding
switch). **P:** a first ordinary-key console profile may omit the running domain
manager, but must stage real DOMAINCLI and exclude power/case/special domain
events at the controlled event source. No forged successful domain response.
Power-management acceptance requires genuine domain service/hierarchy later.
This bounded profile is not general keyboard/power compatibility.

### 2.3 Security flags are not simply unknown or zero

**S:** `qemu_variant_rom.cpp:49-69` zeroes the synthetic header and sets geometry,
not `iKernelConfigFlags`; `EK/memmodel/epoc/putils.cpp:37-54` copies ROM flags to
the superpage. However `EK/include/u32std.h:518-530` forces Enforcement,
ProcessIsolation and EnforceSysBin unless `__PLATSEC_UNLOCKED__` is defined.
`EK/include/kernel/kern_priv.h:2897-2903` ORs those flags and Locked into the
effective value. `qemu-port/Makefile:59-60` does not define the unlock macro;
the port search found no such definition.

**I:** for these source/build definitions, zero raw flags imply effective
`0x9a` (bits 1,3,4,7), not security-off. This is a source-selected default, not a
fresh disassembly or measurement of the existing ELF. **P:** keep it; audit the
actual compile command/header environment and report effective flags before
launch. Do not weaken enforcement to make the console work.

EWSRV capability `all -tcb` includes ProtServ, WriteDeviceData and PowerMgmt
(`ewsrv.mmp:39`). Keep genuine image security metadata and loader capability
checks; DLL capability All is not a grant of All to its caller. Privileged launch
and HAL initialization stay outside the ordinary zero-capability proof app.
Forced software flags do **not** prove hardware memory isolation in direct model.

### 2.4 Linked VGA path, not the unrelated bootstrap map

**S:** `qemu-port/Makefile:267-272` links kernfull with `kern_shell.o`,
`usercon.o`, direct-model kernel objects and `bootmain_gas.o`/`bootcpu_gas.o`;
it does not link `mmu_gas.o` or `genuine_console.o`. The latter is linked by
`GEN_LINK_OBJS` at `:50`, so the claim that no target uses it is unnecessary.
The kernfull VGA writer is `variant/kern_shell.cpp:43-44` at `0xb8000`.
The `console/shell.cpp` message about a VGA hole is **not kernfull evidence**.

`boot/multiboot.S:139-144` enters ResetEntry; `boot/ported/bootmain_gas.S:57-78`
calls InitCpu then `_E32Startup_Body`, not an MMU-enable routine.
`bootcpu_gas.S:189-203` sets optional PSE, not CR0.PG;
`:233-245` installs flat base-zero ring-3 code/data segments whose range includes
VGA. **I:** with the expected paging-disabled boot handoff, linear=physical and
ring-3 access to `0xb8000` needs no newly mapped chunk on this direct path.
This closes the claim that a missing user page-table mapping is necessarily a
source blocker. **R:** inspect actual linked path and capture CR0.PG at launch;
prove a genuine EDISP write reaches VGA. If paging is enabled by a changed boot
path, stop and provide an explicit accessible display mapping—do not infer one.

**P:** keep this direct-model limitation explicit, not a security architecture.
Kernel-shell and EWSRV VGA ownership must be exclusive during console mode;
retain serial diagnostic access and restore legacy shell mode only by a reviewed
transition/reboot. Do not let both writers race over screen contents.

## 3. HAL, display and keymap implementation contract

### 3.1 Real HAL DLL with port-generated tables

**S:** `kernelhwsrv/halservices/hal/src/hal_main.cpp:186-203` has three distinct
Get paths: implementation function, immutable InitialValue, or P&S ReadWord for
settable/unimplemented attributes. `:26-48,51-68` defines/sets the latter under
system-category property keys and panics on unrecovered failure. The first
initializer needs WriteDeviceData. Thus **HAL does not unconditionally require
P&S for every Get**; neither is it safe to declare P&S generally optional.

**P:** generate genuine `Properties`, `Implementation`, `InitialValue` tables
from port-owned `config.hcf`/`values.hda`, using upstream HAL generation machinery.
Do not copy ARM/template CPU or tick values unchanged
(`bsptemplate/.../hal/values.hda:10-20`). Minimum selected profile:

| Attribute | Candidate implementation |
|---|---|
| EDisplayMode | Valid immutable 0, selecting upstream X86 VGA-text driver |
| EKeyboardIndex | Valid immutable 0 for fixed R1 layout; stage `EKDATA.00` as the same genuine port keymap image as `EKDATA.DLL` |
| EDisplayNumberOfScreens | Valid immutable 1 |
| CPU/ABI/page/tick identifiers | Derive from selected x86 kernel constants; audit generated values, no template ARM defaults |
| Display dimensions/memory attributes exposed | Consistent with selected text-mode definitions; use real kernel display provider where applicable, not guessed pixel-format tables |
| Unsupported contrast/backlight/power features | Explicit unsupported unless implemented; do not return fictitious success |

Immutable KeyboardIndex intentionally makes fixed-layout bring-up independent
of HAL property creation. It is not an upstream source modification or fake
Get success: it selects the genuine immutable-table branch. If coordinator
requires settable index (template `config.hcf:74`), retain genuine P&S storage,
initialize it from a privileged launcher, and test Define/Get/Set permissions
before EWSRV becomes system-permanent. Do not silently switch profiles.

### 3.2 Kernel display group

**S:** `EK/drivers/edisp/epoc/generic/wd_generic.cpp:489-497` selects VGA text
for mode zero. `epoc/vga/vgatext.cpp:81-92` receives ScreenInfo, requires valid
address, and adds `0x18000`; it hardcodes 80x25. `EK/euser/us_exec.cpp:5716-5723`
discards the HAL return code, so leaving output uninitialized is unsafe.

**P:** register `EHalGroupDisplay` through genuine `Kern::AddHalEntry` during
variant initialization before starting EWSRV (existing initial registration is
`EK/kernel/sinit.cpp:552-559`). Handler requirements:

- `EDisplayHalScreenInfo`: initialize **every** `TScreenInfoV01` field
  (`EK/include/e32event_private.h:29-36`): window-handle-valid false, null handle,
  screen-address-valid true only when accessible, base `0xa0000`, logical text
  extent 80x25 for this profile. Effective cell address is `0xb8000`; do not
  report `0xb8000` as base and accidentally target `0xd0000`.
- Copy the packaged structure through genuine kernel descriptor-copy APIs;
  validate capacity, propagate failure, no raw user-pointer memcpy.
- `EDisplayHalWsRegisterSwitchOnScreenHandling`: retain the requested boolean;
  `WsSwitchOnScreen`: implement the selected always-on VGA policy honestly.
  Callers are `us_exec.cpp:5787-5800`; void wrappers do not prove success.
- If mode/state queries or setters are exposed, return mode 0/real state;
  accept only supported transitions. Unknown functions return KErrNotSupported.
  `halservices/hal/src/userhal.cpp:340-360` shows pointer/scalar differences;
  don't reinterpret all a1 arguments identically.

This is a mode-specific provider, not bitmap framebuffer or GUI acceptance.
Mode changes, arbitrary palettes and power transitions are not implied.

### 3.3 EKDATA and raw input

**S:** `EK/ewsrv/ky_tran.cpp:655-681` requires real EKDATA.DLL and checks UID3;
`:684-710` changes optional layouts. `ws_main.cpp:2152-2157` attempts EKDATA.00
for index zero and ignores ChangeKeyData's return. Stage both names deliberately
so success does not depend on that ignored error. Preserve three fixed exports
from upstream `EK/eabi/ekdata.def`, matching functions in
`bsptemplate/.../specific/keymap.cpp:2206-2220`.

**P:** create port-owned keymap data from the upstream table format, not a
replacement translator. Pin ordinary US-layout letters/digits, Enter, Backspace,
Tab, Escape, modifiers, arrows and paired down/up scan events. Match Symbian
scan-code constants to the translation tables. Hardware Set-1 bytes are not
automatically Symbian scan codes or Unicode characters.

**S:** current `variant/qemu_variant_int.cpp:168-210` throws away release and
extended keys, translates to characters and calls QemuKbdPush. That is inadequate
for genuine EKTRAN. Keep the old path for legacy mode. Console mode needs a
bounded raw ring preserving make/break/E0 and a thread-context consumer.
`EK/kernel/eventq.cpp:73-106` requires Kern::AddEvent in thread context,
interrupts enabled, kernel unlocked, critical section, no held fast mutex.
**Do not call it from the PS/2 ISR or assume an arbitrary DFC satisfies these
preconditions.** Use a dedicated kernel thread; count ring/queue overflow and
define recovery of modifier/key state. USB character rings cannot claim full
key semantics; USB input is a separate raw-event adapter, not a prerequisite for
the controlled PS/2 candidate. Keep COM1 bytes out of the PS/2 scan decoder.

## 4. Event registration and readiness: idle is not failure

**S:** EWSRV registers its process first (`ws_main.cpp:2136`) and CEvent later
captures the hook, adds its active object and requests events (`:545-560,582-589`).
`EK/kernel/eventq.cpp:161-171,193-211` checks process/thread ownership and pending
request identity. Queue delivery requires an event **and** a ready request
(`:134-148`). A pending Read/RequestEvent with no input is ordinary idle.

**P:** record separate stages: registered WS process, hook owned by EWSRV event
thread, first genuine RequestEvent pending, source event enqueued, request
completed, request rearmed, translated client Read completed. Observe through
bounded kernel diagnostics or test hooks, not fabricated acknowledgments. Never
let a probe thread capture the single hook away from EWSRV.

| Observation | Interpretation / future oracle |
|---|---|
| Client Read remains pending, source silent | Expected idle; timer/scheduler and Write still work |
| Source enqueue succeeds but no request registration | Broken startup/registration; do not call idle |
| Request registered, controlled event accepted, no delivery | Queue/exec/completion failure |
| Raw completion occurs but client never sees key | Translation, focus/window or IPC failure |
| Queue/ring overflow | Explicit input-loss failure/recovery; not a hung active scheduler |
| Client cancel | Terminal completion once; retain status/buffer until completion, then re-read |

Readiness must not equal rendezvous: `CWsServer::New` signals at
`ws_main.cpp:113-124`, **before** display/keymap/event setup and ESHELL creation
at `:2142-2181`. Launcher arms rendezvous before Resume but then separately
requires a genuine console create/write/read-cancel probe with a deadline and
process-death monitoring. No scheduler runs the server requests until startup
reaches `CWsActiveScheduler::Start` at `:2194`. Diagnostic probe code must not
modify upstream EWSRV or replace its server protocol.

## 5. ESHELL/TestBootSequence: concrete scope choice

**S:** non-WINS EWSRV skips shell only when TestBootSequence is nonzero
(`ws_main.cpp:2165-2182`). Wrapper is `us_exec.cpp:5775-5781`.
Default PowerHal returns false (`EK/kernel/sinit.cpp:537-539`); the inspected
binary power implementation also returns EFalse (`drivers/power/binary/bpower.cpp:
146-148`). Historical description at `EK/release_old.txt:10021-10024` says test
department boot, not an unconditional console-disable flag. No supported
true-return configuration was found in the examined source. A made-up provider
returning KErrNotSupported is especially unsafe: nonzero is interpreted as true.

**P — recommended:** leave ordinary false path, stage genuine ESHELL plus real
dependencies. Do not keep feasibility §6's stub alternative. EWSRV faults on
Create failure after setting system-permanent flags; don't discover missing
images by trial launch. Preflight all images before authorizing EWSRV startup.

Genuine ESHELL's bounded initial requirements are concrete, not its entire
command catalogue:

- `FS/etshell/eshell.mmp:24-46,52,60-64`: seven shell sources plus genuine
  decompression sources, EUSER/EFSRV/HAL, 32-KiB stack, DiskAdmin+AllFiles,
  UID3 101f7ae7. Audit the built transitive imports; do not substitute PortFile.
- `ts_main.cpp:29-59`: trap cleanup, NewL, RunL and cleanup; preserve genuine
  constructor/leave/heap behavior.
- `ts_edshl.cpp:558-568`: real full-screen Console::NewL, line editor and command
  completer; `ts_clicomp.cpp:25-38` already requires another RFs connection.
- `ts_edshl.cpp:652-670`: system-drive selection, RFs Connect, SetSessionPath,
  SessionPath and CFileMan::NewL. `:672-706` probes AUTOEXEC.BAT across drives;
  `:716-734` gets session path and enters genuine line editing.
- Use a disposable known drive/root and an intentionally absent AUTOEXEC.BAT;
  return genuine not-found/not-ready for probes, not universal success. No
  uncontrolled boot batch or formatting command is part of acceptance.
- EWSRV resumes shell without waiting for its readiness. Shell blocking on a
  genuine console session until EWSRV starts scheduling is not alone deadlock.
  Nevertheless shell healthy idle/readiness and its focus interactions must
  be checked; merely creating then crashing ESHELL is not candidate success.

Alternative for coordinator: hold console implementation behind later full
genuine startup/domain/ESHELL integration and proceed only with custom serial
R1-A for now. This preserves owner choice but delays `t_cactw`. A true test-boot
variant is an alternative **only after** locating/documenting a genuine baseport
test configuration and its policy; none is proposed as implemented or approved
here. Stub ESHELL, patched-out launch and success-flag bypass are excluded.

## 6. Required cross-card loader and FS interfaces

**P:** these are requirements to the candidate owners, not competing shared ABI
definitions. Preserve `port-x86-rfile64-v1`, H0 and reviewed file-contract errors.

| Interface owner | Required console capability / boundary |
|---|---|
| Loader | Real !Loader sessions for RLibrary::Load and RProcess::Create, including UID wildcard checks, version matching, capabilities, TLS, DLL init/data/BSS/relocations/dependency order, fixed ordinal lookup and unload/refcounts |
| Loader | EWSRV child ESHELL handle/resume/close and command line `/p`; thread create/resume, semaphores, process/thread security flags, rendezvous and death observation |
| FS/staging | Searchable real system image paths/names for ECONS, EWSRV, EDISP, EKTRAN, EKDATA(.00), HAL, DOMAINCLI, ESHELL and audited transitive dependencies; account for aliases/versioned link names and enabled EnforceSysBin |
| FS session | Multiple simultaneous RFs sessions, system-drive query, rooted session path, SessionPath, Entry for absent boot files and valid drive errors; do not assume RFile-only fixture scope suffices for ESHELL |
| FS notifier | Inspect genuine notifier's RFs connection/plugin enumeration separately; startup uses TRAP_IGNORE in `ws_main.cpp:2119-2127`, not proof of notifier functionality |
| Kernel | Genuine session/message/descriptor/async completion, WS event hooks, P&S if selected HAL attributes need it, safe descriptor copy, timers and thread scheduling |
| Console | ECONS RConsole protocol remains genuine, including synchronous Write, asynchronous Read/cancel, window/focus/size behavior used by RTest/ESHELL |

The restricted kernel pilot DLL loader is not these interfaces. The feasibility
report §3.4 identified this separation; the loader contract card must settle
actual E32Loader marshaling rather than treating code-seg source presence as
runtime closure. Image storage/exec-size limits and the 96-MiB direct pool must
be budgeted with concurrent services, stacks and heaps; no concurrency guarantee
is inferred from pool arithmetic. Preserve old PortFile/PortCon/pilot regressions.

## 7. Proposed implementation dispatch (file ownership, review before launch)

All paths below are **proposed future ownership**, not edits authorized by this
report. Shared integration paths have exactly one integrator, not parallel writers.

| Card | Exclusive proposed output(s) | Inputs / bounded completion |
|---|---|---|
| CON-BUILD-DATA | New `qemu-port/r1-console/hal/{config.hcf,values.hda}` and keymap `{keymap.cpp,ekdata.mmp,bld.inf}`, build recipe/manifest within that new directory | Genuine sources unchanged; generated HAL tables, exact UID/ordinal/capability/import audits, EKDATA.00 staging policy. Own no SDK scratch of other workers |
| CON-DISPLAY | New `qemu-port/variant/r1_console_display.{cpp,h}` | Group handler + complete ScreenInfo, text-mode state, descriptor validation; direct-model assumptions explicit; unknown function errors |
| CON-INPUT | New `qemu-port/variant/r1_console_input.{cpp,h}` | Raw Set-1 queue, bounded dedicated thread, Symbian scan translation, explicit input-mode ownership, drops/state recovery; no shared ISR edit by worker |
| CON-STAGE-LAUNCH | New `qemu-port/r1-console/launch/` sources and staging manifest | Reviewed loader+FS candidates; genuine ESHELL preflight, explicit launch order/readiness/death monitoring, no restart/supervision subsystem |
| CON-INTEGRATE (serialized) | `qemu-port/Makefile`, `variant/qemu_variant_asic.cpp`, `variant/qemu_variant_int.cpp`, `variant/kern_shell.cpp`, and `variant/qemu_variant.h` only as reviewed hunks | Sole shared-file writer wires build/HAL registration/raw ISR forwarding and exclusive VGA/input mode; coordinate loader/FS integration owner first |
| CON-ACCEPT | New `qemu-port/tests/r1_console/`, `tools/test_r1_console.py` | Genuine console client source and future isolated serial/QEMU harness; SwEvent input injector is privileged separate helper, not capability added to zero-cap app |
| CON-REGRESSION-INTEGRATE (serialized) | `tools/test_stable.py`, `docs/manual_test_order.md` | Coordinator/integrator only after candidate tests pass; preserve default ISO build and legacy tests |

Recommended order: review loader/FS contracts and ESHELL delta → authorize
isolated console data/display/input/launcher source work → one shared integrator
→ build/artifact review → serialized runtime slot. New path names may be corrected
by coordinator before dispatch; they do not freeze public or private ABI.

## 8. Future tests and unresolved checks

No tests below ran on this card.

1. **Build/artifact:** compile genuine libraries/server/shell with pinned named
   configuration; audit image UIDs, ordinals (no absent-export callable binding),
   dependency closure, capabilities, generated HAL tables and actual kernel flags.
2. **Pre-system-permanent kernel probes:** validate ScreenInfo fields, descriptor
   bounds, linear VGA base/mode, CR0 state and effective security flags. Separate
   controlled malformed-descriptor child panic tests from unexpected scheduler
   failures. Do not probe unsafe pointers from EWSRV itself.
3. **Start:** real FS/!Loader ready; preflight dependency closure; EWSRV rendezvous
   then console protocol readiness. Observe genuine ESHELL healthy, not just
   process-created. Deadline expiry must distinguish process death and no server.
4. **Output:** ECONS create, size, positioning, write/newline/scroll, close/reopen;
   capture visible VGA before shell focus changes. Serial RDebug alone is not
   evidence of EDISP rendering.
5. **Idle input:** pending Read while timer/scheduler and output remain responsive;
   then one controlled down/up ordinary key completes once, correct code/modifiers,
   and later reads work. Test Shift release, arrows, cancel and bounded repeats.
6. **Route separation:** privileged SwEvent injector exercises genuine queue/client
   path separately from PS/2 event source. QEMU keyboard injection then tests the
   raw adapter. Neither proves physical-PC PS/2/USB behavior.
7. **Unchanged t_cactw:** use audited image, no RTest bypass; capture console
   creation, scheduler outcome and exit status. Release heap-mark no-ops are not
   leak-check evidence (feasibility §5).
8. **Regression:** serialized combined suite with distinct scratch images/logs;
   legacy kernel shell/PortCon/PortFile/pilot loader and snapshot/disk-full remain.
   Do not concurrently attach QEMUs to the same writable image. Coordinator adds
   manual PC steps only after runtime integration review.

| Earlier unknown | Disposition now | Still requires |
|---|---|---|
| UID matching and UID1/2 confusion | Source settled in §2.1 | Final image metadata/export audit |
| Symbolic meaning of 1000008c | Exact value settled; nickname immaterial | No runtime prerequisite |
| DOMAINCLI load initiates server contact | No such explicit source initialization; events separated | Generated DLL startup/import audit and execution |
| Security defaults | Forced bits source-settled under inspected definitions | Actual build environment/binary + effective flags measurement |
| Linked VGA mapping | Wrong bootstrap-map inference rejected; direct route identified | CR0/linked verification and real user EDISP write |
| HAL always needs P&S | False; path depends on generated tables | Adopt fixed or settable profile, audit tables; test P&S when used |
| Idle event pending | Legitimate only with registered request and no source input | Instrumented queue/source/read completion checks |
| ESHELL skip configuration | No legitimate true configuration identified | Adopt staged real shell or defer console; no bypass |
| Genuine service exec lifecycle | Required interface defined, not implemented | Loader/FS implementation and runtime checks |

## 9. Standard handoff

- **Card:** R1-CONSOLE-DESIGN (strong).
- **Base:** detached port `fa62323eea7bc96cefd666e40f5189567731bd44`;
  actual dirty main inputs and upstream revision recorded in §0.
- **Changed paths:** only new `qemu-port/docs/r1_console_design_candidate_v1.md`;
  initially drafted in `/tmp/opencode/r1-console-design`. Reference symlinks are
  local scratch, not deliverables. No shared matrix, ABI or implementation edits.
- **Evidence:** source-resolved dispositions §§2–5; proposed interfaces/cards
  §§3–7; independent future oracles and residual uncertainty §8.
- **Verification:** source/structural checks only; no builds, E32, QEMU, hardware,
  commits, pushes, system installs, agent spawning or scratch cleanup.
- **QEMU-ready / hardware-verified:** none / none.
- **Coordinator decisions:** adopt genuine ESHELL staging (recommended) versus
  console deferral; choose immutable versus settable KeyboardIndex profile;
  accept bounded ordinary-key/no-power-domain profile; allocate exclusive shared
  integration ownership and reconcile additional ESHELL FS requirements.
- **Remaining risks:** actual loader exec startup/marshaling; cumulative service
  memory; real image closure; direct-model VGA access and writer ownership;
  kernel event completion and PS/2 decoding; system-permanent failure blast
  radius. None is represented as runtime-closed by this design.
- **Next owner:** coordinator reviews/adopts/corrects candidate, then dispatches
  bounded source implementation. No owner policy question is required directly
  from this worker and no runtime slot is presumed allocated.
