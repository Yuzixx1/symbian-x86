# R1-CONSOLE-PATHS — upstream econs/ewsrv inventory (v0)

Card: `r1_next_wave.md` Agent 3 (R1-CONSOLE-PATHS, "cheaper").
Owner decision already made: the **upstream econs route** is chosen; a private
console DLL is explicitly **not** adopted (brief §Agent 3; the (a)/(b) provider
question posed in `r1_startup_closure_v0.md` §9 is answered by that decision).
This report is **bounded inventory**: sources, MMP/library/capability/driver
dependencies, export/provider identities, the minimum requests actually
exercised by RTest/t_cactw, and compiled vs source-only vs absent status.
No server shim/design, no builds, no tests, no matrix edits.
Not a runtime claim: nothing below was built or executed in this pass.

Evidence base:
- Upstream checkout `kernelhwsrv/` (read-only, `K=` paths below cite
  `kernelhwsrv/…`; `EK=` abbreviates `kernelhwsrv/kernel/eka/…`).
- Scratch build evidence under `/tmp/opencode/r1-build-repro/`
  (from `r1_build_repro_v0.md` / `r1_artifact_audit_v0.md`; reconsumed here,
  not rebuilt).
- Port console as it exists today (provided, not upstream): K-port
  `qemu-port/console/{vga,shell,kbd}.cpp` — VGA 80x25 at 0xB8000 with COM1
  mirror and PS/2-IRQ1/serial input (`qemu-port/console/vga.cpp:1,7`,
  `shell.cpp:130`).

---

## 1. Provider identities (who provides what, by UID/name/ordinal)

| Role | Provider binary | Identity | Entry point actually reached | Evidence |
|---|---|---|---|---|
| Test harness (`RTest`) | **euser.dll** (RTest is *in* EUSER, not a test DLL) | euser exports RTest ordinals (e.g. o704 `RTest::End`, o705 `Next`, o710 `Start`, o711 `Title`, o715 ctor, o721 `operator()`, o2195 `CloseHandleAndWaitForDestruction`) | imported by `t_cactw` from euser — verified in scratch import list (`artifacts/t_cactw_euser_imports.txt`, 50 ordinals; def `EK=bx86gcc/euseru.def` lines 705–711, 715, 721, 2195) | RTest compiled into euser via `EK=euser/euser.mmh:40` (`source … us_test.cpp`) |
| `CConsoleBase` factory | **econs.dll** | UID1 0x1000008d, **UID3 = KConsoleDllUid = 0x100039e7** (Unicode build; non-Unicode value is 0x100000c5) | ordinal 1 `NewConsole` → `new CConsoleTextWin` | `EK=ewsrv/econs.mmp` (`uid 0x1000008d 0x100039e7`); `EK=ewsrv/co_twin.cpp:21-23` (`EXPORT_C TAny* NewConsole()`); UID values `EK=include/e32uid.h:117-120`; loader matches UID2 `KSharedLibraryUid = 0x1000008d` (`EK=include/e32uid.h:75`) |
| Console factory caller | **euser.dll** itself | `Console::NewL` = euser ordinal o1234 (non-imported by t_cactw: intra-euser call) | `EK=euser/cbase/ub_cons.cpp:319` (`Console::NewL`), `:299` (`_LIT(KConsImpl,"econs")`), `:199-205` (`CProxyConsole::Construct` → `RLibrary::Load` + `Lookup(1)`) | ord: `EK=bx86gcc/euseru.def` `?NewL@Console@@… @ 1234` |
| Window (text) server | **ewsrv.exe** | "!E32WindowServer" (`KE32WindowServer`), UID1 0x1000008c UID3 0x100000bf, capability `all -tcb` | answer-session side of `EConsole*` opcodes | `EK=ewsrv/ws_main.cpp:122` (`pS->Start(KE32WindowServer)`), `EK=ewsrv/ewsrv.mmp` (target/uid/capability/libs), `EK=include/e32twin.h:27` (`_LIT(KE32WindowServer,"!E32WindowServer")`) |
| Server dispatch | inside ewsrv.exe | opcode table `EConsoleCreate … EConsoleSetTextAttribute` | `CWsSession::ServiceL` | `EK=ewsrv/ws_main.cpp:149` (ServiceL) and case list `:162,183,209,218,227,238,250,261,270,281,292,303,314,325,390,428,442,451,461,466,473,485,495,504`; enum `EK=include/ws_std.h:286-309` |
| Screen driver | **edisp.dll** (hardware dependent, dynamically loaded by ewsrv) | x86 text build: `wd_generic.cpp` + `vgatext.cpp`; provides `CScreenDriver`, ordinal 1 = factory | loaded by `CWsWindow::New` | `EK=ewsrv/ws_win.cpp:113-124` (`Load(_L("EDISP.DLL"))` + `Lookup(1)` + `Init/SetMode(EMono)`); MMP selection `EK=drivers/edisp/edisp.mmp:25-41` (`#elif defined(X86)` picks `epoc/generic` + `epoc/vga`) |
| VGA text backend | inside edisp.dll | `CScreenDriverVgaText`; screen memory from `UserSvr::ScreenInfo` + 0x18000 | `Blit/ScrollUp/Clear` used by CWsWindow update path | `EK=drivers/edisp/epoc/vga/vgatext.cpp:83` (`UserSvr::ScreenInfo(infoPckg)`), `:89` (`s->iScreen = (TUint16*)(TUint32(info.iScreenAddress)+0x18000)`) |
| Key translator | **ektran.dll** | UID3 = KKeyboardTranUid = 0x100039e1, ordinal-based exports | `CKeyTranslator::New()` at ewsrv boot | `EK=ewsrv/ektran.mmp` (uid/capability), `EK=ewsrv/ws_main.cpp:2146` (`KeyTranslator=CKeyTranslator::New()`), analogue `EK=ewsrv/ky_tran.cpp:41-54` |
| Key data | **ekdata.dll** (variant keymap) | UID3 = KKeyboardDataUid = 0x100039e0 (non-Uni 0x100000dc); linked as `ekdata.dll` | loaded by `CKeyTranslatorX::ChangeKeyData` | `EK=include/e32uid.h:93` (0x100039e0), template MMP `bsptemplate/asspandvariant/template_variant/cakdtemplate.mmp` (uid `0x1000008d 0x100039e0`, source `specific/keymap.cpp`), load `EK=ewsrv/ky_tran.cpp:660-674` |
| Client session | inside econs.dll | `RConsole` (client API), messages over session "!E32WindowServer" | `Connect` / `SendReceive(opcode)` | `EK=ewsrv/co_cli.cpp:20-22` (`CreateSession(KE32WindowServer,Version(),KMessageSlots)`), opcode sends in same file (e.g. `EConsoleSet` at `:54`, `EConsoleWrite`/`EConsoleSize` etc., see §3/§4) |

Name-matching notes (loader-relevant):
- `RTest` loads econs **by name** `"econs"` restricted by `TUidType(KNullUid,
  KSharedLibraryUid, KConsoleDllUid)` (`EK=euser/cbase/ub_cons.cpp:199-215`
  — see the `iLib.Load(aImplDll, type)` call inside `Construct`);
  econs.mmp's UID triplet matches that type exactly.
- ewsrv loads `EDISP.DLL` **by bare name, no UID restriction**
  (`EK=ewsrv/ws_win.cpp:117`), tolerating `KErrAlreadyExists`.
- ektran loads `EKDATA.DLL` by bare name and then validates
  `lib.Type()[2] == KKeyboardDataUid` (`EK=ewsrv/ky_tran.cpp:666-672`,
  `:687-691`).

---

## 2. Who already exists vs source-only vs absent (build-level facts)

From the R1-BUILD-REPRO scratch (`/tmp/opencode/r1-build-repro/`, reconsumed;
see `r1_build_repro_v0.md` and `r1_artifact_audit_v0.md`):

| Component | Status | Evidence |
|---|---|---|
| `euser.dll` incl. RTest + `Console::NewL`/`CProxyConsole` | **compiled** (urel x86) | scratch `epoc32/release/x86/urel/euser.dll`; exports verified via `bx86gcc/euseru.def` (o1234, RTest ordinals) |
| `t_cactw.exe` | **compiled** | scratch `epoc32/release/x86/urel/t_cactw.exe`; imports 50 euser ordinals (`artifacts/t_cactw_euser_imports.txt`) |
| `efsrv.dll`, `eexe.lib`, def-derived libs, petran tools | **compiled/provided** | same scratch dir (see audit report) |
| `econs.dll` | **source-only** | not in scratch urel; source + `EK=ewsrv/econs.mmp` + `EK=bx86/econs.def` present (`r1_startup_closure_v0.md` §9) |
| `ewsrv.exe` | **source-only** | not built; needs `econs.lib`, `ektran.lib`, `hal.lib`, `efsrv.lib`, `domaincli.lib` (`EK=ewsrv/ewsrv.mmp`) |
| `ektran.dll` | **source-only** | `EK=ewsrv/ektran.mmp`, `EK=bx86/ektran.def` present |
| `edisp.dll` (x86 text variant) | **source-only** | `EK=drivers/edisp/edisp.mmp` builds it for `defined(X86)` from `epoc/generic` + `epoc/vga`; `EK=bx86/edisp.def` present |
| `ekdata.dll` (variant keymap) | **source-only, variant-dependent** | template `bsptemplate/asspandvariant/template_variant/cakdtemplate.mmp` + `specific/keymap.cpp`; needs a BSP variant image |
| A running console/keyboard for the EUSER test route | **absent today at runtime** | port's genuine chrome is the K-port console (`qemu-port/console/`), not econs/ewsrv |

Nothing in this table claims any of the source-only components work — they
have never been built or executed in this project.

---

## 3. Minimum exercised console path for t_cactw (this is the triage core)

Call sites in `EK=kerneltest/e32test/active/t_cactw.cpp`:
`test.Title()` at `:426`; `test.Start` at `:236,297`; `test.Next` at many
lines (e.g. `:247,257,267,277,299,304,311`); `test(r==…)/(thread…)`
boolean checks; `test.End()` at `:287,401,436`. **No `test.Getch()` /
console input of any kind. No cursor/size console calls.**

Client-side sequence (all inside euser.dll + econs.dll):

1. First use of the console → `RTest::CheckConsoleCreated()`
   (`EK=euser/us_test.cpp:27-35`): `Console::NewL(title, TSize(KConsFullScreen, KConsFullScreen))`
   under `TRAPD`; on failure **panics `ERTTestCreateConsole`** (`:33`).
   Consequence: a failing console kills t_cactw *before the title is even
   printed*; serial mirroring alone cannot substitute, because the panic
   happens before any output attempt (see step 5 for the RDebug mirror of
   successful prints).
2. `Console::NewL` (`EK=euser/cbase/ub_cons.cpp:319-347`) →
   `CProxyConsole::Construct("econs")` (`:199-212`) →
   `RLibrary::Load("econs", TUidType(KNullUid, KSharedLibraryUid,
   KConsoleDllUid))` → `Lookup(1)()` = `NewConsole` (`:205`) →
   `CConsoleTextWin` (factory `EK=ewsrv/co_twin.cpp:20-23`).
3. `Create(title, size)` (`co_twin.cpp:33-41`) → `RConsole::Init(aTitle,aSize)`
   (`EK=ewsrv/co_cli.cpp:44-54`): `Connect()` = `CreateSession
   ("!E32WindowServer", version, KMessageSlots)` then one
   `SendReceive(EConsoleSet, (&title, TPckgC<TSize>))`.
4. Immediately after: `Control("+Maximize +NewLine -Lock -Wrap")`
   (`co_twin.cpp:41`, `EK=ewsrv/ws_main.cpp:325` dispatches
   `EConsoleControl`).
5. **Output**: every `RTest::Printf` (`EK=euser/us_test.cpp:484-503`) does
   `iConsole->Write(buf)` → `CConsoleTextWin::Write` (`co_twin.cpp:104-112`)
   → `RConsole::Write` → `SendReceive(EConsoleWrite)`; the server side is
   `case EConsoleWrite` (`EK=ewsrv/ws_main.cpp:390`). Each newline of the
   same buffer is **also** mirrored to the kernel debug channel via
   `RDebug::RawPrint` (`us_test.cpp:492-503`) — i.e. a working up-console
   additionally duplicates output onto the debug/serial path.
   Boolean checks (`RTest::operator()`, `us_test.cpp:302-321`) print only on
   failure and then `RTest::Panic("Checkpoint-fail")`; `Getch()` there is
   reached only if `iLogging == EFalse` — but the default is `ETrue`
   (`us_test.cpp:76,92,109` ctor init `iLogging(ETrue)`), so for t_cactw the
   Getch branch is dead code.
6. **Input/size/cursor**: `RTest::Getch` is exported (o707 in
   `EK=bx86gcc/euseru.def`), but **t_cactw does not import it** (checked
   against `artifacts/t_cactw_euser_imports.txt`: only 704/705/710/711/715/
   721/2195 RTest-family ordinals; no 707). `EConsoleSize`,
   `EConsoleCursorPos`, `EConsoleRead`, `EConsoleReadCancel` are never
   reached from this fixture's call sites.
7. **Teardown**: t_cactw never sends `EConsoleDestroy`
   (`case` at `EK=ewsrv/ws_main.cpp:451` is unreached from this fixture):
   `CConsoleTextWin::~CConsoleTextWin` only does `iConsole.Close()`
   (`co_twin.cpp` dtor, session close), so server cleanup happens on session
   disconnect. Also imported o2195 `RTest::CloseHandleAndWaitForDestruction`
   is used on thread handles (scheduler-panic subtests), not the console
   (`EK=euser/us_test.cpp:524-561`; note `r1_startup_closure_v0.md` §9
   already flags the same requirement).

Summary — minimum exercised request set (per t_cactw run):
`Connect(version)` → `EConsoleSet(title,size)` → `EConsoleControl("…")` →
N x `EConsoleWrite(des)` → session `Close()`. That is the whole console
surface exercised by the R1 proof fixture. Create (EConsoleCreate),
size/cursor ops, read/read-cancel can be deferred behind the exercised set;
a genuine ewsrv must still *exist* and survive the exercised set.

Server-side failure semantics on the client path: every
`CConsoleTextWin` method funnels errors through `ConsServerCheck`
(`co_twin.cpp:50-64`): non-`KErrNone` from any console request
panics `EConsServerFailed` (client-side, "Console-TWIN" category,
`EK=ewsrv/co_utl.cpp:21-22`), except the debug-only print. So t_cactw dies
panicking, not hanging, if the server breaks mid-run.

---

## 4. Full upstream server dependency graph (vs the minimum above)

`ewsrv.exe` boot (`EK=ewsrv/ws_main.cpp:2134-2195`) requires, with *its*
minimum:

| # | Requirement | Where | Notes for feasibility |
|---|---|---|---|
| 1 | `UserSvr::WsRegisterThread()` | `ws_main.cpp:2136` | kernel executive call; upstream kernel has it; its effect interacts with screen/event server kernel-side state (strong review) |
| 2 | `UserSvr::WsRegisterSwitchOnScreenHandling(ETrue)` | `:2137` | ditto — screen-power event hook |
| 3 | `User::SetProcessCritical(ESystemPermanent)` (process + thread) | `:2138-2139` | genuine process-property support |
| 4 | `CWsServer::New()` → `Start(KE32WindowServer)` | `:122`, `:2142` | plain named server create |
| 5 | `CWsWindow::New()` → `RLibrary::Load("EDISP.DLL")` + ord 1 factory + `Init/SetMode(EMono)/GetAttributeColors` | `EK=ewsrv/ws_win.cpp:113-130` | **edisp baggage**: driver DLL is a hard boot dependency of ewsrv, unneeded by t_cactw's client-side minimum but unavoidable server-side |
| 6 | Visibility-map allocations sized by `ScreenDriver->Init` | `ws_win.cpp:137-145` | follows #5 |
| 7 | `CEvent::New()` → `UserSvr::CaptureEventHook` + `RequestEvent/RequestEventCancel` | `:549-598` (see `:559`, `:588`, `:598`) | **kernel input-event pipeline**: something must produce kernel events (keyboard driver upstream of `UserSvr::RequestEvent`); the port has no such producer today |
| 8 | `CKeyTranslator::New()` non-NULL — `Initialise()` = `ChangeKeyData(_L(""))` **synchronously loads `EKDATA.DLL`** and fails (→ NULL → `Fault(ENoKeyboardTranslator)` at `:2148`) if missing | `:2146-2148`, `EK=ewsrv/ky_tran.cpp:41-54,65-67,660-674` | **ekdata is a hard ewsrv boot prerequisite** (a variant keymap DLL must ship on the search path) |
| 9 | `HAL::Get(EKeyboardIndex)` → `ChangeKeyData("EKDATA.%02d")` | `:2152-2158` | result ignored; only meaningful with a keyboard, harmless error otherwise |
| 10 | `CKeyRepeat` (ETimer) | `:2160-2163` | plain active object |
| 11 | Optional `ESHELL.EXE` launch unless `UserSvr::TestBootSequence()` / self-bootstrap EXE | `:2166-2176`, `_LIT(KShellProcessName,"ESHELL")` `:33` | telling detail: **the upstream console starts the shell, not the other way round**; nothing in the base repo's found artifacts starts ewsrv itself — the base-ROM launch point for ewsrv is **not established** in the sources available locally (see §6 unknowns) |
| 12 | Notifier session thread and password render helpers | `:754`, `:998`, `RenderPassword` `:36` | in-process; no extra binaries |
| 13 | Power-transition hooks `RDmDomainManager` (domaincli) | `:675,680,694,708,713` | build-time lib dependency (`domaincli.lib` in ewsrv.mmp); not exercised during the console minimum |
| 14 | Screen rendering path: text blits through `CScreenDriver` (`Blit/SetForeground/…`) + optional cursor via `ECursorPeriodicPriority` timer | `ws_win.cpp:106-124` and dispatch cases | genuine VGA-text backend assumes a memory-mapped text screen whose address comes from `UserSvr::ScreenInfo` + 0x18000 (`vgatext.cpp:83,89`) — interacts with VBE/framebuffer choices in the port (strong review) |
| 15 | Text shell translator `ektran.dll` exports consumed by ewsrv | ewsrv.mmp `library … ektran.lib`, `KeyTranslator` usage §8 | ektran itself needs only euser (`EK=ewsrv/ektran.mmp`) |

Client-side extra beyond the minimum set (not exercised by the R1 fixture but
present in the client API): `EConsoleCreate`, `EConsoleClearScreen`
(`.209`), `ClearToEndOfLine`, `Set*Position/Height`, `Size/ScreenSize/
CursorPos` `.303,314,292`, `Read .428/ReadCancel .442`, `SetMode/SetPalette/
TextColors/SetUIColors/SetTextAttribute` `.461-504` — server handlers
must at least exist (upstream does) even if t_cactw never calls them.

Compile/deps recap per binary:

- `econs.mmp`: sources `co_twin.cpp co_cli.cpp co_utl.cpp`, libs
  `euser.lib` only, UID 0x1000008d 0x100039e7, capability **all**,
  `nostrictdef`, WINS baseaddress 0x54000000 (`EK=ewsrv/econs.mmp`).
- `ewsrv.mmp`: sources `ws_dat ws_main ws_utl ws_win`, libs
  `euser econs ektran hal efsrv domaincli`, capability `all -tcb`,
  `unpaged`, `SMPSAFE` (`EK=ewsrv/ewsrv.mmp`).
- `ektran.mmp`: sources `ky_capt.cpp ky_tran.cpp`, lib `euser.lib`,
  UID 0x1000008d 0x100039e1, capability **all**, def-file driven
  (`EK=ewsrv/ektran.mmp`).
- `edisp.mmp` (x86 branch): `wd_generic.cpp` + `vgatext.cpp`, def-file
  driven; before that a sibling `edisp_lib.mmp` (`EK=ewsrv/bld.inf`
  line ordering; both list under `PRJ_MMPFILES`).
- `ekdata` (catd template): `keymap.cpp`, libs `euser.lib`, def
  `kernel/eka/~/ekdata.def`, UID 0x1000008d 0x100039e0, capability **all**
  (`EK=bsptemplate/asspandvariant/template_variant/cakdtemplate.mmp`).

Def-file availability: `EK=bx86/econs.def`, `EK=bx86/edisp.def`,
`EK=bx86/ekdata.def`, `EK=bx86/ektran.def` all exist (seen in `ls bx86`),
so the Raptor x86 def route used by the build repro is plausible for all
five binaries — but **not demonstrated**.

Loader-services reached by the whole chain (both LLibrary-load and
session-connect):
- `RTest→econs`: `RLibrary::Load("econs", TUidType(KNullUid, 0x1000008d,
  0x100039e7))` + ordinal resolve 1.
- `ewsrv→edisp`: `RLibrary::Load("EDISP.DLL")` + ordinal resolve 1.
- `ektran→EKDATA(.NN/EKDATA.DLL)`: bare-name loads, UID3 validated after.
- `CreateSession("!E32WindowServer", version, KMessageSlots)`.
- t_cactw itself: static imports (50 euser ordinals — confirmed in scratch
  import list; this includes the `RTest` family listed in §1, **not**
  `Console::NewL` o1234).

This composes with the already-recorded loader constraint
(`r1_build_repro_v0.md` import-string facts; `r1_startup_closure_v0.md` §9
G7 "loader service for RLibrary::Load, console provider"): the exercise
requires **by-name DLL resolution beyond the initial static import set**,
a "search path" (not just an image-relative name), and the genuine loader
matching rules for UID-restricted loads.

---

## 5. Capability/security surface (only facts; no enforcement claim)

| Binary | Capability | Notes |
|---|---|---|
| econs.dll | `all` | `EK=ewsrv/econs.mmp` |
| ewsrv.exe | `all -tcb` | `EK=ewsrv/ewsrv.mmp` |
| ektran.dll | `all` | `EK=ewsrv/ektran.mmp` |
| ekdata.dll | `all` | template MMP |
| t_cactw (e32test exe) | — (not inspected here) | out of card scope |

Summary: the genuine route expects a platform where a normal test process can
create a session to a `all -tcb` server, and where that server can load DLLs
and reach `UserSvr` hooks. The port does not currently enforce platform
security (existing reports), so capability bits are acknowledged, not
simulated — recorded without any claim about knob-by-knob equivalence.

---

## 6. Unknowns, assumptions and strong-review flags

Assumptions made by this report (stated, not proven):
- ASS-1: `TConsolePanic`/`ERTTestCreateConsole` label strings haven't been
  traced (they occur only inside euser/ewsrv internals); a refusal to
  resolve exact panic-category text is deliberate — it doesn't affect the
  exercised-path mechanics.
- ASS-2: message-arg packing (`TPckgC<TSize>` etc.) sizes have not been
  cross-checked against the genuine `RMessage` copy contract; flagged.
- ASS-3: the port's existing `Ps::…`/kernel exec table identity for
  `WsRegisterThread` etc. was not re-verified in this pass (kernel side is
  genuine upstream, but variant hooks — see F-5) — flagged for the startup
  closure.

Strong feasibility review flags (these are the "console dependency
gaps" that `r1_startup_closure_v0.md` §9/§7 leaves open):

- F-1 **Runtime stack ordering.** The exercise needs: genuine euser.dll →
  econs.dll (RLibrary::Load by name+UidType) → ewsrv.exe reachable as a
  fresh process. Nothing on the port now spawns ewsrv or a second non-test
  process from the UI side; upstream's base repo does not show its own ROM
  launch point for ewsrv (not found in the iby/rom searches in this pass —
  search was shallow, no exhaustive conclusion).
- F-2 **DLL search-path/name resolution.** `RLibrary::Load("econs")`,
  `Load("EDISP.DLL")`, `Load("EKDATA.DLL")` require genuine loader
  resolution semantics (by bare name on some path, plus UID checks and
  `KErrAlreadyExists` subtleties). The port loader status is recorded in
  `r1_startup_closure_v0.md` §9; it is **not** resolved as ready in that
  report — treat as open.
- F-3 **Driver-backed text screen via `UserSvr::ScreenInfo`.**
  `vgatext.cpp:83,89` derives the mapping from the kernel ScreenInfo result
  (+0x18000 offset into it). Whether the port's screen setup (VBE vs VGA
  text) matches this expectation needs review before any claim of "ewsrv
  would display text". Note the K-port console writes 0xB8000 directly
  (`qemu-port/console/vga.cpp:7`); upstream's genuine kernel-side screen
  address plumbing is a separate question and not verified here.
- F-4 **Input pipeline exists nowhere on the port.** ewsrv's event capture
  (`UserSvr::CaptureEventHook` / `RequestEvent`) requires a kernel-side
  event producer (keyboard driver). The port has PS/2+serial input but
  handled port-side (kernel chrome), not through the genuine event
  pipeline. **Not needed for t_cactw** (§3 shows no reads), but essential
  for ewsrv *booting* (F-8 item #7) and any future input tests.
- F-5 **Kernel-side variant hooks for WsRegisterThread /
  SwitchOnScreenHandling / TestBootSequence.** Genuine exec calls exist in
  upstream kernel, but their behavior depends on variant glue that this
  port has not exercised. Not inspected beyond the grep that showed no
  port-side consumers (`qemu-port/{variant,inc,src…}` no hits for
  WsRegister/CaptureEventHook — checked once, no claim about other call
  routes).
- F-6 **EKDATA (keymap variant DLL) ship requirement.** Hard boot
  prerequisite for ewsrv (§4 #8): a `ekdata.dll` must be found at boot; the
  template source exists (`bsptemplate/.../keymap.cpp`) but an x86-build
  keymap with the right UID triplet for this port is unbuilt and unresolved.
- F-7 **`__UHEAP_MARK/__UHEAP_MARKEND` on t_cactw** map to
  `User::Invariant()` (o669) — imported, so it exercises the debug-only heap
  invariant path in release euser; behavior in scratch euser is recorded in
  the audit, not re-checked here.
- F-8 **Server dispatch breadth.** Even though the client's minimum is
  tiny (§3), the server's ServiceL still must *reach*, for every (rare)
  extra path, the expected handlers; unknown routes will be completed
  on demand. Low risk, listed for completeness.

Not decided by this report (and deliberately no design given):
- Whether the port bootstraps ewsrv from its shell or via some genuine
  boot-config mechanism (ties into `r1_startup_closure_v0.md` items and
  coordinator choices).
- Whether edisp's `vga` (text) backend or any seen backend maps to the
  port's display choice.
- Any ROM/iby composition (kickstart vs test ordering).

---

## 7. Cross-references and status pointers

- `r1_startup_closure_v0.md` §9: covers `t_cactw → Console::NewL → econs →
  "!E32WindowServer"` at mapping level, poses the (a)/(b) provider question
  — **answered** by the owner's choice (upstream route); this report
  supersedes that open question with detailed evidence but keeps the owner's
  r1-a matrix untouched.
- `r1_startup_closure_v0.md` V9 ("Console route (§9) selected provider loads
  and prints; one `Printf` through `RTest`") — this report makes V9's check
  precise: `Printf → Write(EConsoleWrite)` and the sequence of §3; the
  required minimum is exactly: F-1..F-5 resolved at least for the Write-only
  surface.
- `r1_build_repro_v0.md` / `r1_artifact_audit_v0.md`: artifact identity,
  import lists, absent binaries — reused verbatim here, not re-derived.
- `parallel_work.md` status table row "R1-BUILD-CONFIG / R1-HEAP-EXEC /
  R1-CONSOLE-PATHS" should be updated by the coordinator after review; this
  report returns that card as **research returned** with F-1..F-8 open.

---

## 8. Dependency inventory — completing the econs/ewsrv/loader build & source graph

Items the original brief asked for that §1/§2/§4 did not yet close:

**Build-graph survivors (all file:line-checked):**

- `edisp_lib.mmp` builds `edisp.lib` (`targettype implib`, **UID
  0x1000008d 0x100039eb**, same `deffile ../../~/` directory binding as
  the DLL) — the import-library companion for clients of `edisp.dll`
  (`EK=drivers/edisp/edisp_lib.mmp:22-28`).
- `edisp_vt100.mmp` builds a **separate** `edisp_vt100.dll` from
  `epoc/generic/wd_vt100.cpp` — a serial/VT100-terminal alternative screen
  driver, gated to `GENERIC_MARM | GENERIC_X86` builds
  (`EK=drivers/edisp/edisp_vt100.mmp:18-22`; `EK=ewsrv/bld.inf` tail).
- `base_e32_ewsrv.mrp` declares the component boundary: exactly *ewsrv +
  edisp* sources, exports directory, no third package included
  (`EK=ewsrv/base_e32_ewsrv.mrp`, component/base_e32_ewsrv block).
- Platform gating for the whole family: `ewsrv/bld.inf` builds ektran,
  econs, ewsrv, edisp, edisp_lib only under
  `GENERIC_MARM || WINS || GENERIC_X86` and
  `!MARM_THUMB && !MARM_ARMI` (`EK=ewsrv/bld.inf:32-43`).
- Def-file coverage re-checked for all five targets: `econs.def`,
  `edisp.def`, `ekdata.def`, `ektran.def`, `euseru.def` all exist in
  `EK=bx86/`; `econs.def` additionally in `EK=bmarm/`, `EK=bwins/`,
  `EK=bx86/`, `EK=eabi/` (seen by directory listing; the *x86gcc* EUSER
  def used by the repro is `EK=bx86gcc/euseru.def`). **ewsrv.exe needs no
  def** (exe target, `EK=ewsrv/ewsrv.mmp` has no deffile line).
- `ektran.lib` is consumed by ewsrv for `?New@CKeyTranslator`-family
  imports (consumer side of `EK=ewsrv/ky_tran.cpp:41-54`).

**Kernel-side bottom of the graph (previously unlocated):**

- Event pipeline: `UserSvr::AddEvent` (`EK=euser/us_exec.cpp:5706-5714`)
  → `ExecHandler::AddEvent` (`EK=kernel/eventq.cpp:50-70`),
  capability-gated `ECapabilitySwEvent` (+`PowerMgmt` for selected event
  classes). Consumers: `ExecHandler::CaptureEventHook`
  (`EK=kernel/eventq.cpp:153-170`), `RequestEvent` (`:193-212`),
  `RequestEventCancel` (`:214-220`) — the exact trio ewsrv's `CEvent`
  active object calls (`EK=ewsrv/ws_main.cpp:559,588,598`).
- Screen info: `UserSvr::ScreenInfo` (`EK=euser/us_exec.cpp:5716-5722`)
  = `Exec::HalFunction(EHalGroupDisplay, EDisplayHalScreenInfo, &anInfo,
  NULL)`; the variant-side contract (parameters, TPckgBuf<TScreenInfoV01>)
  is documented at `EK=include/u32hal.h:1205-1220`; template implementer
  `DLcdPowerHandler::ScreenInfo`
  (`EK=bsptemplate/asspandvariant/template_variant/specific/lcd.cpp:537,
  539`). This is *the* extension point whose port-side counterpart decides
  the F-3 feasibility question of §6.
- Boot gate: `UserSvr::TestBootSequence` is served by the **power HAL**
  entry `EPowerHalTestBootSequence` returning 0
  (`EK=kernel/sinit.cpp:523-541`, see `:537`), reached via
  `ws_main.cpp:2166`'s check that gates the ESHELL launch decision.
- `UserSvr::WsRegisterThread` kernel-side: `ExecHandler::WsRegisterThread`
  (`EK=kernel/sexec.cpp:2253`).
- HAL entry array shape: group-indexed `K::HalEntryArray` with kernel,
  variant and power handlers pre-set (`EK=kernel/sinit.cpp:548-559`),
  i.e. an upstream-port must provide a display-HAL entry to satisfy
  `EDisplayHalScreenInfo`.

**Still genuinely open in the dependency graph (not closed by search):**

- Any pre-built or generated **x86 `ekdata.dll`**: only the variant
  template (`VariantTarget(ekdata,dll)` producing a per-variant
  `ekdata.dll`, `EK=bsptemplate/.../cakdtemplate.mmp`) plus the frozen
  def-file exports (`EK=bx86/ekdata.def`) were found. No x86 variant
  build discovered in this checkout.
- An upstream **launcher** for ewsrv.exe (§6 F-1) — no iby/rom/start
  artifact found; search was limited, not exhaustive.

---

## 9. Existing coverage — console, keyboard, output, process lifecycle

Scope: the port's own test corpus (`qemu-port/tools/test_*.py`, harness
`tools/test_stable.py:8-14` — sequential runner; the stable suite passes on
the dirty tree per `qa_inventory_R0.md` §2 — *no fresh worktree baseline
claim*). For each behavior: **test path + assertion**, or **no test found**.

| # | Behavior | Test path | Assertion (exact) | Notes / limits |
|---|---|---|---|---|
| 1 | Shell text output (COM1 serial mirror of port console) | `tools/test_cli.py` | `wait_for` markers `b'EKA2> '`, `b'C:/> '` (`:51-54`); directory listing check `b"NEW      TXT" not in listing` (`:118`) via `wait_for` (`:13-29`) | Asserts the **serial channel**; QEMU runs `-display none` so the VGA cells themselves are never asserted anywhere in the suite |
| 2 | Program-side text output (genuine user apps) | `tools/test_userland.py` (ECHO/TYPE boot+run), `tools/test_samples.py` (calc, guess, wc, rev, fib, mem; `:60` asserts `'FAULT:' not in output and 'Exc ' not in output`) | marker waits + absence faults | exercises genuine user-mode text output through the port console, **not** through econs/ewsrv |
| 3 | Console **input** via the port's lease protocol | `tools/test_interact.py` (starts `INTERACT.E32`, waits `b'Name? '` `:33`, types `Ada`, waits `Hello Ada!`, `INTERACT: done`, `INTERACT: ready 20`, `random=`, no FAULT — `:40-52`) | lease → prompt → typed echo → release | exercises `PortCon` lease input (`user/runtime.c:129-142` `con_channel/con_acquire/con_release`, exec 132 + request 1/2) — a **port-private device select, not the upstream window-server Read path** |
| 4 | Keyboard device (USB) injection | `tools/test_xhci_keyboard.py` | runs QEMU `-device qemu-xhci -device usb-kbd`, monitor `sendkey` letters → expects `"xhci: keyboard ok"`, `"EKA2> version"`, `"key(s)"` counter (`:56-61`) | **diagnostic only: no asserts** and **not in test_stable.py CHECKS** (`:8-14`), so it is not a gated check; also port-side (chrome), not the upstream event pipeline |
| 5 | Console lease lifecycle (owner death reclaim) | kernel path exists: `variant/kern_shell.cpp:1314-1323` (`ConInputLeased()` → `ConForceRelease()` → `"console lease released (owner died)"`) | **no test found** asserting the reclaim path/message specifically | `test_crash.py` asserts crash attribution (`prog=CRASH.E32` in the reaper exit line `:41-43`) but not lease reclaim state; `variant/usercon.cpp:40-58` documents fail-closed acquire/release rules (owner-only release; strangers `KErrAccessDenied`) — untested by the runner |
| 6 | Process lifecycle (exit, kill, records) | `tools/test_crash.py` (`:27-43`: FAULT attribution, shell survives, ECHO runs after); `tools/test_wait.py` (blocking wait: live timeout vs dead-process records); `tools/test_conc.py` (concurrency); `tools/test_ipc.py` | per-file asserts | genuine user-side process lifecycle via the port's chrome (samples + port execs); upstream `CloseHandleAndWaitForDestruction` (t_cactw imports o2195) has **no port coverage** |
| 7 | Upstream console protocol requests (any of §3's `EConsole*` opcodes) | — | **no test found**: no `econs`, `ewsrv`, `RLibrary::Load`, `CreateSession`, or `"!E32WindowServer"` reference exists anywhere under `qemu-port/tools/*.py` | the whole econs/ewsrv request surface is untested and unexercised (also unbuilt, §2) |
| 8 | Cursor/size/text-attribute client ops, `Getch`/`Read`/`ReadCancel` | — | **no test found** at any layer (port chrome lease test #3 exercises a *leased line read*, not the upstream `RConsole::Read`/cursor family) | |

Explicit limit note (as briefed): rows 1–5 do **not** establish that the
upstream econs/ewsrv console protocol works — the port's shell-console
tests verify the PortCon/chrome path only. Row 3's "lease, typed input"
is a different wire contract from `RConsole::Read`/`EConsoleRead`;
row 4 only proves an input device exists at chrome level.

---

## 10. Acceptance scenarios (draft, source-backed, not executable, not frozen)

For each operation **actually used** in the source path; each scenario cites
exercising code. Separate "minimum RTest" (what the programmatic proof
fixture needs) from "later interactive coverage".

### 10.1 Minimum RTest requirements (t_cactw-shaped)

- **A-1 Console creation (lazy, on first RTest call).**
  Evidence chain: `us_test.cpp:27-35` (`CheckConsoleCreated`) →
  `ub_cons.cpp:319-347` (`Console::NewL`) → `:199-212`
  (`RLibrary::Load("econs", TUidType(KNullUid, 0x1000008d, 0x100039e7))`
  + `Lookup(1)`) → `co_twin.cpp:20-23` (`NewConsole` →
  `CConsoleTextWin`) → `co_cli.cpp:44-54` (`Init` → `EConsoleSet`) →
  `Connect` (`co_cli.cpp:20-22`).
  Draft check: after the loader resolves the by-name econs load,
  `RTest::Title()` must pass the point where `iConsole != NULL`
  and the run must proceed to the first `EConsoleWrite`; a run that instead
  panics `ERTTestCreateConsole` (us_test.cpp:33) means one of the chain
  links above broke. [Proposed, not executed.]
- **A-2 Text output path (per line).**
  `us_test.cpp:484-503` → `CConsoleBase::Write` →
  `CConsoleTextWin::Write` (`co_twin.cpp:104-112`) →
  `SendReceive(EConsoleWrite)`; handling on server side =
  `ws_main.cpp:390`. Draft check: at least one successful
  `EConsoleWrite`
  request on the live session, and every written newline mirrored through
  `RDebug::RawPrint` (`:492-503`), so a serial-capture harness sees the
  same text even without further VGA assumptions. [Proposed.]
- **A-3 Configuration control string.**
  The single `Control("+Maximize +NewLine -Lock -Wrap")` request
  (`co_twin.cpp:41`; server dispatch `ws_main.cpp:325`). Draft check: the
  `EConsoleControl` request succeeds once per console with the exact
  upstream flag string. [Proposed.]
- **A-4 Client exit / teardown.**
  `~CConsoleTextWin` closes the RConsole session handle **without**
  sending `EConsoleDestroy` (§3 step 7; server disconnect-cleanup only).
  Draft check: the server-side session object destruction is exercised
  through the session close only; no `EConsoleDestroy` request should be
  expected from this fixture shape. [Proposed.]
- **A-5 Not exercised — input/cancel.**
  `RTest::Getch` (euser o707) is not imported by t_cactw and `iLogging`
  defaults `ETrue` (so the `operator()` failure branch's Getch at
  `us_test.cpp:319-321` is unreachable in this fixture). Draft check:
  none for the upheld minimum; if a console-input scenario is *forced*
  later (e.g. a test config setting `iLogging=EFalse` or a
  `test.Getch()` call), the create-session path must additionally supply
  `EConsoleRead`/`EConsoleReadCancel` capability and, behind that, the
  kernel event pipeline (§8 eventq chain + F-4 of §6).
  [Explicitly out of the minimum.]
- **A-6 Not exercised — cursor/size/attribute.**
  No `EConsoleCursorPos/Size/ScreenSize/SetCursor*/SetMode/Set*Colors/
  SetTextAttribute` request is reachable from the t_cactw import set.
  The server handlers exist upstream (`ws_main.cpp:292,303,314; 461-504`)
  but the minimum fixture does not use them. [Out of the RTest minimum.]

### 10.2 Later interactive-console coverage (beyond the RTest minimum)

Deferred, but enumerated so the boundary is precise:

- **B-1 Keyboard input path (EConsoleRead/EConsoleReadCancel).** Requires
  ewsrv boot to survive `CEvent::New()` (`ws_main.cpp:549-598`) with the
  genuine event queue (`EK=kernel/eventq.cpp:153-220`), some
  SwEvent-capability producer for raw events (`EK=euser/us_exec.cpp:
  5706-5714`), plus ektran + a valid `EKDATA.DLL` (§4 rows 7-9, F-6).
- **B-2 Screen positioning/color ops.** Server-side only after
  `EDISP` load (`ws_win.cpp:113-145`); depends on F-3 (ScreenInfo mapping)
  and a choice of driver (vgatext vs vt100).
- **B-3 Window mode `Control` variations, palette, size queries.**
  Non-minimum; handlers `ws_main.cpp:303,314,461-504`.
- **B-4 A second concurrent console client** (two sessions) — exercises
  per-session window state; upstream shapes this via `CWsSession` per
  client (`ws_std.h:281-309`); currently no fixture uses it.

### 10.3 Scope boundary statement

§10 items are **proposed checks**, not executable tests, not a frozen
contract; they state where a source-anchored check *would* bind. The
minimum-RTest set (10.1) is exactly what the owner's chosen upstream route
must satisfy first; every 10.2 item can be deferred behind 10.1 without
changing the minimum.

---

## 11. Strong-review queue (unresolved, evidence-only, no inference)

| # | Question | Exact evidence paths | Why unresolved here |
|---|---|---|---|
| Q-1 | Who fills `Msg`/version semantics for `CreateSession("!E32WindowServer", Version(), KMessageSlots)` — is the version check mode the port must reproduce? | `EK=ewsrv/co_cli.cpp:20-22`; `EK=ewsrv/ws_main.cpp:149` (ServiceL); message enum `EK=include/ws_std.h:284-309` | Only the call sites and enum are inventoried; the *expected* version/policy behavior is not derivable from local sources without an interpretation choice the owner must make |
| Q-2 | Which screen backend maps to the port — genuine `vgatext` (ScreenInfo-driven, +0x18000 offset) vs `edisp_vt100` (serial-terminal shape) — and does the port's display path fit either? | `EK=drivers/edisp/epoc/vga/vgatext.cpp:81-89`; `EK=drivers/edisp/edisp_vt100.mmp:18-22`; `EK=include/u32hal.h:1205-1220`; `EK=bsptemplate/.../lcd.cpp:537`; port side `qemu-port/console/vga.cpp:1,7` | A display-HAL implementer decision, not inferable; both upstream variants have hard prerequisites the port may not meet (F-3) |
| Q-3 | Which `EKDATA`/keymap variant ships (and how it is generated per-variant for the port's x86 target)? | `EK=bsptemplate/asspandvariant/template_variant/cakdtemplate.mmp` (VariantTarget/ekdata, `uid 0x1000008d 0x100039e0`); `EK=ewsrv/ky_tran.cpp:660-674` (sync load at boot); `EK=ewsrv/ws_main.cpp:2146-2148` (NULL → Fault) | Variant keymap generation is a BSP-configuration act; no x86 keymap toolchain or artifact found (§8 open item) |
| Q-4 | What starts ewsrv.exe on the port (boot-config mechanism, shell exec, or genuine ROM `StartProcess`-style plumbing)? | startup evidence chain §7 (`ws_main.cpp:2166-2176`) plus F-1; no local iby/rom/start artifact found | An architectural/ownership choice; also gated by genuine `elf2e32`/toolchain route (unresolved per board) |
| Q-5 | Capability/security parity: does the exercised path need actual platform-security enforcement or is a capability-agnostic setup acceptable, given what euser/econs/ewsrv expect? | capability rows §5 (`econs.mmp`, `ewsrv.mmp`, `ektran.mmp`, cakdtemplate.mmp); port-side non-enforcement note | Owner's risk posture, unresolved |
| Q-6 | Loader by-name+UID resolution status (which of r1_startup_closure_v0.md's open items gates `RLibrary::Load("econs",…)`) | `EK=euser/cbase/ub_cons.cpp:199-212`; `r1_startup_closure_v0.md` §9 + G7 row | Outside this card: closure items are coordinator-owned; no inference here |
| Q-7 | Does any upstream answer exist for the `edisp.lib` **UID 0x100039eb** import-library's role in RLoader/Raptor (which def freeze does each mmp consume)? | `EK=drivers/edisp/edisp_lib.mmp:22-28` (same `deffile ../../~/` binding); def set listing §8 | Build-system semantics not derived from the local sources; flagged for the toolchain card (R1-BUILD-CONFIG) |
| Q-8 | Is `ewsrv` expected to also serve the not-exercised opcodes (`EConsoleRead` family etc.) from *day one* on the port — i.e. does a "write-only" server subset suffice as the owner's selection? | minimum set §3 vs full dispatch list `ws_main.cpp:162-504` | A deliberate boundary choice for the owner; both answers have upstream-fragment consequences (ewsrv's boot sequence requires more than the client minimum regardless, §4) |

No question in §11 is answered inside this report by inference; each needs
owner decision or a strong-review pass over the cited artifacts.

---

## 12. Handoff

```
- Card: R1-CONSOLE-PATHS  Status: done  Date: 2026-10-21
- Deliverable: qemu-port/docs/r1_console_paths_v0.md
- Summary: inventoried genuine upstream console route (econs/ewsrv/edisp/
  ektran/ekdata) with file:line evidence; min exercised request set for
  t_cactw = Connect/EConsoleSet/EConsoleControl/N x EConsoleWrite/Close,
  no read/cursor/size/destroy; separated full server graph (14 boot
  requirements) from minimum; compiled (euser/t_cactw) vs source-only
  (econs,ewsrv,ektran,edisp,ekdata) vs absent (runtime console/keyboard
  producer) mapped; loader by-name+UID load story cross-checked with
  closure §9 and repro import facts; capability surface summarized.
- Expansion (same report, owner-supplied extension): §8 completes the
  dependency graph downward (edisp_lib/edisp_vt100/mrp, platform gating,
  kernel eventq trio, HalFunction EDisplayHalScreenInfo contract, PowerHal
  TestBootSequence, WsRegisterThread exec); §9 inventories existing port
  tests (console/keyboard/output/lifecycle) with explicit "no test found"
  rows for anything upstream-econs; §10 drafts source-backed acceptance
  scenarios (proposed only) separating minimum-RTest from interactive
  coverage; §11 is an evidence-only strong-review queue Q-1..Q-8.
  Existing coverage explicitly does not verify the upstream protocol.
- Non-goals honored: no design, no private console, no builds, no
  matrix/report edits, no runtime claims.
- Key files/lines cited:
  kernelhwsrv/kernel/eka/{euser/us_test.cpp:27-35,484-503,302-321;
  euser/cbase/ub_cons.cpp:199-212,299,319-347; euser/euser.mmh:40;
  ewsrv/{econs.mmp,ewsrv.mmp,ektran.mmp,co_twin.cpp:20-23,33-41,50-64,
  104-112,co_cli.cpp:20-22,44-54,ws_main.cpp:122,149,162-504,549-598,
  2134-2195,ws_win.cpp:113-145};
  drivers/edisp/{edisp.mmp:25-41,epoc/vga/vgatext.cpp:83,89};
  include/{e32cons.h,e32twin.h:27,ws_std.h:286-309,e32uid.h:75,93,105,118};
  bsptemplate/asspandvariant/template_variant/{cakdtemplate.mmp,
  specific/keymap.cpp};
  kerneltest/e32test/active/t_cactw.cpp:84,236,247,287,297,401,426,436;
  qemu-port/console/{vga.cpp:1,7,shell.cpp:130}};
  /tmp/opencode/r1-build-repro/artifacts/{t_cactw_euser_imports.txt (50
  ordinals, RTest family 704/705/710/711/715/721/2195, no Console::NewL
  o1234, no RTest::Getch o707), scratch_raptor_patches.diff,
  euser_ordinal_offsets.txt}.
- Caveats/flags for reviewers: 10 assumptions & 8 strong-review flags in
  §6 (esp. F-1 launch of ewsrv, F-2 loader by-name path, F-3 ScreenInfo
  +0x18000 mapping, F-4 event pipeline, F-6 ekdata prerequisite); def-file
  availability noted but unbuilt; iby/rom launch-point search was shallow,
  not exhaustive; message-arg packing not re-validated (ASS-2); §10 is
  draft scenario wording, not a frozen contract; §9 rows 3-6 describe
  port-chrome coverage only, not the upstream protocol; §11 questions
  Q-1..Q-8 are evidence-only and unanswered.
- Next steps for the owner: mark R1-CONSOLE-PATHS returned on the board;
  review F-1/F-2/F-3/F-6 with the startup-closure items; the exercised-path
  contract (V9) can now be written into the acceptance work without a
  broader ewsrv bring-up commitment.
```
