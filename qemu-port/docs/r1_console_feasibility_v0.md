# R1-CONSOLE-FEASIBILITY v0 (strong review of R1-CONSOLE-PATHS)

Status: research report. No ABI freeze, no implementation, no shared-doc edits.
QEMU-ready: **none**. Hardware-verified: **none**.
Verification: **static + artifact inspection only; no E32 execution; no QEMU; no hardware.**

Evidence labels: [S] source read, [A] artifact/tool inspection, [I] inference, [U] unknown.
`EK=` means `kernelhwsrv/kernel/eka/`.

## 0. Provenance

| Item | Value |
|---|---|
| Port base (worktree) | detached `fa62323eea7bc96cefd666e40f5189567731bd44`, worktree `/tmp/opencode/r1-console-feas`, `kernelhwsrv` symlink |
| Upstream `kernelhwsrv` | `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean |
| Main tree | 64 dirty/untracked paths at start. Untracked docs (inventory, reconciliation, next_wave) were read from the main tree because they are not in `fa62323`. |
| Linked kernel reviewed | main-tree `qemu-port/build/kernfull.elf`, mtime 2026-10-20 22:27, SHA-256 `53b54fb0f5549221f3979e135d731549d59645ceb59add555b228781e8d6a09e` (gitignored). Linked sources `variant/kern_shell.cpp`, `variant/usercon.cpp`, `variant/e32load.cpp`, `Makefile` are modified in the main tree relative to HEAD. |
| Existing upstream build | `/tmp/opencode/r1-build-repro/epoc32/release/x86/urel/` (read-only) |
| Scratch | `/tmp/opencode/r1-console-feas-scratch/` (`kernfull.dis` only) |

No E32 image was run, no `petran`/Wine invoked, no build performed.

## 1. Verdict (one paragraph)

The genuine upstream console route (`RTest` -> `Console::NewL` -> `RLibrary::Load("econs")` -> `RConsole` -> `!E32WindowServer` -> `ewsrv.exe` -> `edisp` -> `ekdata`) **is not feasible on the current port as a small step**. Nothing on that route is built or staged today, and four prerequisites are absent: (1) a loader service (`!Loader` in the file-server process), (2) a launcher for `ewsrv.exe`, (3) a display HAL group providing `ScreenInfo` and (4) a keyboard-data DLL for the port. In addition, ewsrv's own startup carries a **deterministic system-permanent panic**: it sets itself `ESystemPermanent`, then tries to start `ESHELL`, and on this kernel `TestBootSequence()` returns 0, so it creates `ESHELL`, which does not exist in the port, and calls `Fault(ECreateShell)`. That Fault takes the whole system down (§6). No feasibility blocker was found that is *inherent* to GCC or X86 def resolution (§3.3). The route is gated on the loader/F32 decision, not on toolchain. Console output for the provisional proof (`t_cactw.exe`) therefore must not be assumed to come from the genuine console route; serial via `RDebug::RawPrint` is the only already-linked output path (§4.3).

## 2. Corrections to `r1_console_paths_v0.md`

| # | Inventory claim | Corrected finding | Evidence |
|---|---|---|---|
| C-1 | econs `UID1 0x1000008d`, `UID3 0x100039e7` (§1 row 2) | The MMP `uid A B` pair is **UID2 UID3**, not UID1. `econs.mmp` `uid 0x1000008d 0x100039e7` gives UID2=`KSharedLibraryUid` (0x1000008d), UID3=`KConsoleDllUid` (0x100039e7). UID1 is supplied by the build as 0x10000079 (DLL) or 0x1000007a (EXE). The inventory's own "loader matches UID2 = 0x1000008d" line is right; its "UID1" label is the error. | [A] built headers: `euser.dll` 10000079/1000008d/100039e5, `efsrv.dll` 10000079/1000008d/100039e4, `t_cactw.exe` 1000007a/0/0. [S] Raptor `lib/flm/base.xml` (`UID1` default 1000007a), `analysis.xml` (exe 1000007a, dll 10000079), `e32postlink.mk` (`--uid1=`). |
| C-2 | Same for `ewsrv` (`0x1000008c 0x100000bf`) | Those are UID2=0x1000008c, UID3=0x100000bf; UID1=0x1000007a (EXE). Meaning of UID2 0x1000008c is [U] (no symbol in `e32uid.h`). | [S] `ewsrv.mmp`; [A] by analogy with built EXE headers. |
| C-3 | Client loads econs "by name+UidType" | Correct, and exact: `TUidType(KNullUid, KSharedLibraryUid, KConsoleDllUid)`, so UID1 is wildcard, UID2 and UID3 must match; the loader's exact matcher in `sf_ldr`/`e32ldr` was not traced [U]. | [S] `EK=euser/cbase/ub_cons.cpp:199-210`; `RLibrary::Load` doc says `KNullUid` is a wildcard. |
| C-4 | Items in §3 imply `RTest` calls several console methods directly | Directly from `RTest`: `Console::NewL` (via `CheckConsoleCreated`), `Write` (`us_test.cpp:485`), `Getch` (`:519`, also from `End` when `iLogging` false), and `delete iConsole` (`Close`, `:123`). Everything else (`Init`, `EConsoleSet`, `EConsoleCreate`, `SendReceive`) is **transitive** via `CProxyConsole` -> `CConsoleTextWin` -> `RConsole`. | [S] `us_test.cpp`, `ub_cons.cpp:319-355`, `ewsrv/co_twin.cpp`, `ewsrv/co_cli.cpp:24-58`. |
| C-5 | F-1: nothing spawns ewsrv | Upstream does have a launcher. `rombuild/user.iby` places `EWSRV.EXE`, `EKTRAN`, `ECONS`, `EDISP`; `estart.cpp:101` names `KWindowServerRootName1 "EWSRV.EXE"`, and `TFSStartup::CreateServer` (`estart.cpp:450`, called at `:2018`) does `RProcess::Create` + `Resume`. ESTART is itself launched by the file server (`sf_main.cpp:~392-412`, `eStart.Create(KStartupExeSysBinName,...)`). The statement stays true for the **port**: nothing in the port launches ewsrv, ESTART, or the file server. | [S] as cited; [S] port grep: `e32load.cpp`/`userfiles.cpp`/`kern_shell.cpp` have no ESTART/EWSRV/ESHELL reference. |
| C-6 | F-7: `__UHEAP_MARK/MARKEND` map to `User::Invariant()` | In release builds both macros expand to nothing (`e32def.h` `#define __UHEAP_MARK` empty outside `_DEBUG`). The urel `t_cactw.exe.sym` has one `User::Invariant` reference, which is inside `invalid_entry` (`euser/epoc/x86/uc_exe.cia:58,84`), the EXE entry guard, unrelated to heap marks. | [S] `e32def.h`, `t_cactw.cpp:427,437`; [A] urel `.sym`. |
| C-7 | The linked port console is `qemu-port/console/*` | Stale for `kernfull`. The `Makefile` `KERNFULL_OBJS` list links `variant/kern_shell.cpp` (kernel-mode VGA at 0xB8000, serial, keyboard ring) plus `build/userfiles.o build/usercon.o`. `variant/usercon.cpp` is a transitional kernel LDD named `PortCon` (`DPortCon::Request`), **not ECONS**. `qemu-port/console/{vga,kbd,shell}.cpp` belong to the non-kernfull bootstrap objects. `variant/genuine_console.cpp` has a Makefile rule (`GEN_LINK_OBJS` path) but is not in `KERNFULL_OBJS` [U whether any other target links it]. | [S] `Makefile` 267/613; [A] `nm -C kernfull.elf`: `DPortCon::Request`, `VgaTeletype`, `ConPollChar` present. |
| C-8 | Display backend question (Q-2) is open | Narrowed: on X86 `CScreenDriver::New` asks `HAL::Get(EDisplayMode)`; value 0 selects `NewScreenDriverVgaText()`, which needs `UserSvr::ScreenInfo` -> `Exec::HalFunction(EHalGroupDisplay, EDisplayHalScreenInfo)` and uses `iScreenAddress + 0x18000` (so the provider must report 0xA0000 for text at 0xB8000). Linked kernel has no display HAL group (§3.5). The `edisp_vt100` variant additionally needs the `ECOMM` LDD and a serial keyboard class (`wd_vt100.cpp`), which the port also lacks. | [S] `wd_generic.cpp:489-500`, `vgatext.cpp:70-93`, `us_exec.cpp:5716-5723`, `wd_vt100.cpp:34-37`. |

## 3. Per-dependency feasibility

Legend: **Present** (built/linked now), **Source-only** (upstream source exists, not built for the port), **Absent**.

### 3.1 Summary

| Dependency | State | Feasibility | Key gate |
|---|---|---|---|
| `econs.dll` | Source-only; not in `r1-build-repro` urel | Buildable in principle (GCC, eabi def fallback, §3.3) | Needs built `ewsrv` headers (`ws_std.h` is private to `EK=include`, USERINCLUDE `../include`) and an installed image path |
| `ewsrv.exe` | Source-only | Buildable in principle; **launch and panic gates** | Launcher, loader, ESHELL, display, keymap, domain (§3.4-§3.8) |
| `ektran.dll` | Source-only | Buildable; needs `EKDATA.DLL` at runtime | keymap DLL |
| `ekdata.dll` | Source-only in `template_variant` (`cakdtemplate.mmp`, `specific/keymap.cpp`); none for the port | Needs a port-owned keymap DLL | Port-specific source; new work, not a patch of upstream |
| `edisp.dll` (generic+vgatext) | Source-only; MMP X86 branch = `wd_generic.cpp` + `vgatext.cpp` | Buildable; **runtime depends on HAL display group** | `ScreenInfo` provider |
| `hal.dll` | Import lib `hal.lib` built in build-repro; `hal.dll` not in urel listing | Needs generated `values.cpp/config.cpp` (`halcfg.pl`, `config.hcf`/`values.hda`) | Port HAL config; uses `RProperty` at runtime |
| `domaincli.dll` | Source-only (`domainmgr/group/domaincli.mmp`, capability All, lib euser) | Must exist for `ewsrv.exe` to load (link-time import) | See §3.6 |
| Loader (`!Loader`) | Source in F32; **absent in port** | Hard gate for `RLibrary::Load` and `RProcess::Create` | §3.4 |
| ESTART/ESHELL/F32 | Source-only; absent in port | Not needed for a minimal proof if ewsrv is started by hand, but ewsrv itself requires ESHELL (§6) | §6 |

### 3.2 Build graph [S]

`ewsrv/bld.inf` builds ektran, econs, ewsrv, edisp, edisp_lib, and (GENERIC_MARM|GENERIC_X86) `edisp_vt100`, guarded `!MARM_THUMB && !MARM_ARMI`. MMP facts:

- `econs.mmp`: dll, sources `co_twin co_cli co_utl`, lib euser, `uid 0x1000008d 0x100039e7`, capability all.
- `ewsrv.mmp`: exe, `ws_dat ws_main ws_utl ws_win`, libs `euser econs ektran hal efsrv domaincli`, `uid 0x1000008c 0x100000bf`, capability `all -tcb`.
- `ektran.mmp`: dll, `ky_capt ky_tran`, `uid 0x1000008d 0x100039e1`, capability all.
- `edisp.mmp` (X86): `wd_generic.cpp` + `vgatext.cpp`, libs `euser hal`, `uid 0x1000008d 0x100039eb`.
- `ekdata`: `bsptemplate/.../template_variant/cakdtemplate.mmp`, `uid 0x1000008d 0x100039e0`.

Tooling in the existing build covers only edll/edllstub/eexe/efsrv/euser/fhserver/rpipe/t_cactw [A]. None of the console binaries have been built; no claim of buildability is runtime- or build-verified.

### 3.3 Def-file resolution under GCC (traced, not name-matched) [S]

`raptor_meta.py:43-50`: `PlatformDefaultDefFileDir['X86'] = ['bx86gcc','eabi']`. `bx86gcc/` contains only `euseru.def` and similar; there is no econs/ektran/edisp/ekdata def, so these fall back to `eabi/`, as EFSRV already does in the repro build. The eabi defs use ARM-ABI mangling, which matches GCC output for X86 (`unsigned int` args appear as `j`; `TUint` = `unsigned int`, `TUint32` = `unsigned long` in `e32def.h`). `eabi/econs.def` (35 lines): `NewConsole @1`, `Panic(TConsolePanic) @2`, `RConsole::*` from @3. `eabi/edisp.def`: only `_ZN13CScreenDriver3NewEv @1`. `eabi/ekdata.def`: `KeyDataConvTable @1`, `KeyDataFuncTable @2`, `KeyDataSettings @3`. `eabi/ektran.def` has typeinfo/vtable entries for `CCaptureKeys`, which the importlib rule rewrites as ABSENT (`e32importlib.mk:54`). Consumers use fixed ordinals: `iLib.Lookup(1)` in `ub_cons.cpp:205`, `edisp.Lookup(1)` in `ws_win.cpp`, and keymap exports in `ky_tran.cpp` enum (`EKeyDataConv=1`...). **No X86-specific blocker found.** Mismatch risk remains for `RConsole::SetUIColors(unsigned int,...)` style signatures if a header differs [I, low].

### 3.4 Loader service and process launch [S]

- `RLibrary::Load` -> `RLoader::LoadLibrary` -> `SendReceive(ELoadLibrary, ...)` after `E32Loader::WaitDllLock()`; `RProcess::Create` -> `RLoader::LoadProcess` (`us_ksvr.cpp:4927-4930`). `RLoader::Connect` creates a session to `"!Loader"`, version `(KLoaderMajorVersionNumber=1, KLoaderMinorVersionNumber=0)` (`e32ldr_private.h`).
- Messages (`TLoaderMsg`): `ELoadProcess=1`, `ELoadLibrary=2`, ... `EGetInfo=7`, ... `ELoaderRunReaper=17`. `TLdrInfo` carries `iRequestedUids`, `iOwnerType`, `iHandle`, `iSecureId`, `iRequestedVersion`, `iMinStackSize`.
- Server side: `LoaderThread` in the file-server process (`sf_ldr.cpp:310`): `SetCritical(ESystemCritical)`, scheduler + server, `Rendezvous`, `gTheLoaderFs.Connect()`. It therefore needs F32 running in-process, a file system with the images, and `E32Loader::*` exec calls.
- Port: `variant/e32load.cpp` implements a **kernel-side** image validator and runner (`E32Validate`, `E32Run`, `E32Kill`, `E32Reap`) and a resident `EUSER.DLL` pilot loader which explicitly rejects DLLs with data, bss, dependencies or relocations (`E32LoadDll`). It has no `!Loader` server, no `RLibrary` support, no `E32Loader::LibraryLoad/Attach`. `kernfull.elf` has no `!Loader` string and no `E32Loader::` symbols [A].
- Inference [I]: econs/edisp/ekdata/ektran loading therefore requires either a real F32 loader port or a new loader provider. **This is the primary gate.** Version rule is as already ruled: provider minor >= requested minor, same major (`User::QueryVersionSupported`, `us_func.cpp:1098-1121`; a client request of (1,0) matches any provider (1,x)).

### 3.5 Display path [S]/[A]

- `CWsWindow::New` (`ws_win.cpp:108-135`) does `RLibrary::Load("EDISP.DLL")`, `Lookup(1)`, and `Fault(EWindowsInitialisation)` if the driver is NULL.
- `CScreenDriver::New` (X86) -> `HAL::Get(EDisplayMode, mode)`; mode initialised to 0, so a failed `HAL::Get` also yields the VGA-text branch [I]. `NewScreenDriverVgaText` requires `info.iScreenAddressValid`; **`info` is a stack object that is never zeroed** (`vgatext.cpp:81`), so if `ScreenInfo` does nothing the validity flag is uninitialised stack [S]. A provider must write all of `TScreenInfoV01` (valid flags, address 0xA0000, size) [I].
- `UserSvr::ScreenInfo` -> `Exec::HalFunction(EHalGroupDisplay=4, EDisplayHalScreenInfo, ...)` [S]. In the linked kernel `K::InitHalEntryArray` registers only groups 0 (`kernelHal`), 1 (`variantHal`) and 3 (`PowerHal`) (`sinit.cpp:552-559`; `InitHalEntryArray` at 0x128fc0 in `kernfull.elf`, referenced from `K::Init3`) [S][A]. Port grep: no `AddHalEntry`, `ScreenInfo`, `EHalGroupDisplay`, `EDisplayHal`, `iScreenAddress`. So group 4 is unregistered and `HalFunction` returns `KErrNotSupported` [I from `ExecHandler::HalFunction`: no entry -> not supported]. **A display HAL group provider (or variant hook) is missing.** It must also handle `EDisplayHalWsRegisterSwitchOnScreenHandling`, which `ewsrv` calls at start (`us_exec.cpp:5790`).
- Port memory: the kernfull VGA hole 0xA0000-0x100000 is excluded from the identity direct map (`console/shell.cpp:91`, bootstrap path) [S]; whether kernfull maps VGA into a user process address space is [U]. User-mode access to 0xB8000 would need a mapped chunk or equivalent; this is not designed anywhere in the port [U].

### 3.6 HAL and domaincli [S]

- `HAL::Get` consults generated tables (`HalInternal::Properties/Implementation/InitialValue`, `hal_main.cpp:180-230`). Settable attributes without an implementation (`EKeyboardIndex : set = 0` in the template `config.hcf`) are stored with `RProperty` under `KUidSystemCategory`; `ReadWord` panics (`EGetPropFailed`) if the property read still fails after `InitialiseData()`. Hence `hal.dll` needs publish/subscribe in user mode, working capabilities for `RProperty::Define` [U], and the generated tables for the port. ewsrv's keymap lookup (`HAL::Get(EKeyboardIndex)`) goes through this path [S/I].
- `domaincli.dll` is linked by `ewsrv.mmp`; the DLL must be present at load, but it only matters at runtime when `RDmDomainManager` is used. Uses found: `ws_main.cpp:675-713` (switch-on / key-off events) and `ws_win.cpp:1583`. These are event-driven; they do **not** run at startup [S]. `mgr.Connect()` failure panics (`EWSRV SwitchOn0` etc.), so a power-key event would panic ewsrv, which is system-permanent. Whether domaincli imports require a running domain server at load [U] (not read: `domaincli.cpp`).
- `domainmanager.h` is exported to `epoc32/include/platform` only by the `domainmgr` bld.inf. The repro include dir has no `domainmanager.h` and no `ws_std.h` [A]. `e32svr.h`, `u32hal.h`, `k32keys.h`, `e32twin.h` exist under `platform/`.

### 3.7 Window-server kernel hooks (linked kernel) [A]/[S]

Present in `kernfull.elf`: `ExecHandler::WsRegisterThread` (sets `K::TheWindowServerProcess`, `sexec.cpp:2261-2263`), `ExecHandler::CaptureEventHook`, `ExecHandler::AddEvent`, `Kern::AddEvent`, `K::CreateEventQueue` (called from `K::Init3`, `sinit.cpp:431`), `K::HalEntryArray`, `ExecHandler::HalFunction`, `PowerHal`. `CaptureEventHook` raises process-isolation failure unless the caller's process is the registered window-server process (`eventq.cpp:161`), so call order `WsRegisterThread` before `CEvent::New` is satisfied by ewsrv itself [S]. Slow-exec decode shows `ServerReceive`, `SessionSend`, `SessionShare`, `SetSessionPtr`, `ProcessResume`, `LibraryLookup`, `ChangeNotifier*`, `TransferSession`; runtime behaviour of any of these in the port is untested [A, no execution]. Event sources: nothing in the port calls `Kern::AddEvent` or registers a keyboard HAL source that feeds the genuine event queue [I from port grep: no `AddEvent` callers listed in the review]; the port keyboard path is the PS/2 ISR ring (`QemuKbdPush`/`ConPollChar`) into `PortCon`/kern_shell, not the event queue.

### 3.8 Keyboard translator and keymap [S]

`CKeyTranslator::New()` returns NULL if `ChangeKeyData(_L(""))` fails, i.e. if `iDefaultKeyDataLib.Load("EKDATA.DLL")` fails or the library's UID3 is not `KKeyboardDataUid` (`ky_tran.cpp:655-677`). `E32Main` then `Fault(ENoKeyboardTranslator)` (`ws_main.cpp:2147`). A port keymap DLL (`ekdata.dll`, UID2 0x1000008d, UID3 0x100039e0, three exports) is mandatory. `template_variant/specific/keymap.cpp` is a source template only [S].

## 4. Startup order and reachability

### 4.1 Required order for the genuine route [S]/[I]

1. Kernel up with loader-capable user side (F32 process + `!Loader` thread) [absent].
2. `ewsrv.exe` created and resumed (ESTART in upstream; nothing in the port) [absent].
3. ewsrv `E32Main` (`ws_main.cpp:2134-2197`): `WsRegisterThread`, `WsRegisterSwitchOnScreenHandling(ETrue)` (needs display HAL group), `SetProcessCritical(ESystemPermanent)`, `SetCritical(ESystemPermanent)`, scheduler, `CWsServer::New` (starts `!E32WindowServer` and `Rendezvous`), `CWsWindow::New` (EDISP load + screen driver), `CEvent::New` (`CaptureEventHook`, `RequestEvent`), `CKeyTranslator::New` (EKDATA), keymap change via `HAL::Get(EKeyboardIndex)`, `KeyRepeat`, `ESHELL` create/resume, notifier thread.
4. Client: `RTest::Printf` -> `CheckConsoleCreated` -> `Console::NewL` -> `RLibrary::Load("econs", TUidType(Null, Shared, Console))` -> `NewConsole()` -> `CConsoleTextWin::Create` -> `RConsole::Init` -> `Connect()` -> `CreateSession("!E32WindowServer", Version(1,0), 3 slots)` -> `EConsoleSet`.
5. `CreateSession` to a `!`-prefixed server name requires `ProtServ` in the *server's* creator at `Start` (`sipc.cpp:837`); ewsrv has capability `all -tcb`, which includes ProtServ [S]. Whether the port superpage sets `EKernelConfigPlatSecEnforcement` is [U]; if clear, capability failures only emit diagnostics ("would have failed", `ssecure.cpp:~212`).

### 4.2 `RTest` console panic path [S]

`CheckConsoleCreated` -> `TRAPD(r, Console::NewL(...))`, `__ASSERT_ALWAYS(r==KErrNone, ::Panic(ERTestCreateConsole))` (`us_test.cpp:27-33`, `ERTestCreateConsole=85` in `e32panic.h`). **Without the console route, the first `test.Printf`/`Next`/`Start` panics `t_cactw`** (the client dies, not the system). `Console::NewL` has no fallback outside `__WINS__` (`ub_cons.cpp:325-348` only under `__WINS__`) [S].

### 4.3 Already-working alternative output path [S]/[A]

`RTest::Printf` mirrors to `RDebug::RawPrint` (serial) when `iLogging` is true (default) **after** `CheckConsoleCreated()`, so the mirror does not help avoid the panic: console creation precedes the write [S]. Whether `t_cactw` can use a non-console path without modifying test source is therefore **No** [I]. This is a hard consequence for proof-source selection: either the genuine console route works, or `t_cactw` is replaced/supplemented by a port fixture that does not use `RTest`.

## 5. Release heap-mark behaviour (card item)

`__UHEAP_MARK`, `__UHEAP_MARKEND` and friends are empty macros in release (`e32def.h`), so the `t_cactw` marks at `:427,437` generate **no code** in urel; they cannot cause a `User::Invariant`/panic on a leak in urel [S][A]. In `_DEBUG` builds they call `User::__DbgMarkStart/End`, which require kernel heap-debug support [U]. The kernel in `kernfull` is built `_DEBUG` (`KERN_DEFS` has `-D_DEBUG`) while user images are urel; this does not conflict.

## 6. System-permanent and critical panic risks

| Risk | Trigger | Consequence | Evidence |
|---|---|---|---|
| **R-1 ESHELL create failure** | `UserSvr::TestBootSequence()` returns 0 here: group 3 -> `PowerHal` in `sinit.cpp:537-538` returns 0 when there is no `K::PowerModel` (`K::PowerModel` is a BSS symbol set only by `DPowerController::Register`, `power.cpp:729`; port has no `PowerController` reference [A grep]). So ewsrv takes the `else` branch, `shell.Create("ESHELL", "/p")`, `__ASSERT_ALWAYS(r==KErrNone, Fault(ECreateShell))` (`ws_main.cpp:2175-2182`). ESHELL is not in the port and requires F32 (`etshell/eshell.mmp`, capability DiskAdmin AllFiles; `f32.iby` stages it with tcb+diskadmin+allfiles). | `Fault` in a process flagged `ESystemPermanent` -> `K::Fault(EPermanentProcessExit)` / `ESystemProcessPanic` -> whole-system fault | [S] `ws_main.cpp`, `sprocess.cpp:557-559`, `sthread.cpp:738-740`; [A] `K::PowerModel` symbol, `K::Fault` present. |
| R-2 any ewsrv Fault/panic after startup | `ENoKeyboardTranslator`, `EWindowsInitialisation`, `ECreateEvent`, `EStartServer`, domain connect panics, `HAL` property panics | same as R-1 | [S] |
| R-3 loader thread critical | `LoaderThread` `SetCritical(ESystemCritical)`; its panic reboots | [S] `sf_ldr.cpp:310` |
| R-4 capability gates | `ProcessSetFlags`/`ThreadSetFlags` for the critical flags need `ProtServ` (`sexec.cpp:550,727`); ewsrv has it (`all -tcb`). If enforcement is off, a missing capability only logs; with it on, `LockedPlatformSecurityPanic` | [S]; enforcement state [U] |
| R-5 notifier thread | `TRAP_IGNORE(CNotifierServer::NewL())` then signals semaphore, so absent F32 (`fs.Connect()` fails) does **not** kill ewsrv | [S] `ws_main.cpp:2119-2127, 955-965` |
| R-6 domain events | Power/key-off events call `mgr.Connect()`, panic on failure | [S] `ws_main.cpp:675-713` |

Mitigations that do not require editing upstream source (proposals, not decisions): (a) a `PowerController`-equivalent making `TestBootSequence` return non-zero, which makes ewsrv skip ESHELL (`TestBootSequence=TRUE` branch) [I; this changes variant behaviour and needs a ruling]; (b) stage a stub `ESHELL.EXE` image. Each is a port-owned artifact, not a patch to ewsrv. Both are **unverified**. Capability subset checks do not establish isolation (standing ruling).

## 7. Unknowns [U]

1. Loader match rule for UID1/2/3 vs `TLdrInfo.iRequestedUids` (not traced in `e32ldr`/`sf_ldr`).
2. Whether `domaincli.dll` import resolution at ewsrv load needs a running domain manager.
3. Whether the port superpage sets `EKernelConfigPlatSecEnforcement`.
4. Meaning of ewsrv UID2 0x1000008c.
5. Whether `HAL::Get` -> `RProperty` works in the port's user-mode P&S (kernel P&S is initialised, `PubSubPropertyInit` in `K::Init3`; user-side is untested).
6. Whether kernfull can map VGA memory into a user process.
7. Whether any target links `variant/genuine_console.cpp`.
8. Exec-table behaviour of server/session execs in the port (decoded, not exercised).
9. Whether a `bx86gcc` def-frozen set is expected by the integrator or the `eabi` fallback is accepted.
10. Release-mode `User::Invariant` mapping for non-`t_cactw` images was not surveyed.

## 8. Proposals for the integrator (research proposals only)

| ID | Proposal | Needed because | Cost class |
|---|---|---|---|
| P-1 | Decide loader provider: port `!Loader` + F32 vs kernel-side loader service | §3.4; gates `RLibrary::Load` and `RProcess::Create` | Large; blocks all console routes |
| P-2 | Treat the genuine console route as a **multi-card staged goal** gated on P-1; do not schedule it before R1-EUSER/R1-FS | Dependencies are transitive, not a console-only feature | Planning |
| P-3 | For an earlier proof, use a **port fixture** that does not use `RTest` and writes through `RDebug::RawPrint`/serial, labelled as a separate mode | §4.2-4.3: `t_cactw` panics without the console | Small |
| P-4 | Display group provider: register `EHalGroupDisplay` handler (variant HAL hook) answering `ScreenInfo` (fully initialised, address 0xA0000), `EDisplayHalMode`, `WsRegisterSwitchOnScreenHandling` | §3.5 | Medium |
| P-5 | Port keymap DLL (`ekdata`) from the template shape | §3.8 | Small-medium |
| P-6 | Resolve R-1 (TestBootSequence or stub ESHELL) before any ewsrv launch | §6 | Needs ruling |
| P-7 | Generate `hal.dll` tables for the port | §3.6 | Medium |

None of these is approved or implemented here.

## 9. QEMU/hardware status

QEMU-ready: **none.** Hardware-verified: **none.** Every claim above is source or artifact reading. The unknowns in §7 can only be closed by runtime work not authorised here.

## 10. Handoff

- **Card:** R1-CONSOLE-FEASIBILITY (strong).
- **Base:** port `fa62323eea7bc96cefd666e40f5189567731bd44` (worktree, detached; main tree dirty with 64 paths); upstream `kernelhwsrv` `0c3208650587ac0230aed8a74e9bddb5288023eb`; linked `kernfull.elf` SHA-256 `53b54fb0...a09e` (§0).
- **Changed paths:** `qemu-port/docs/r1_console_feasibility_v0.md` (new, in worktree `/tmp/opencode/r1-console-feas`; not copied to the main tree). Nothing else.
- **Evidence:** §0-§7, labelled [S]/[A]/[I]/[U].
- **Verification:** static + artifact inspection only; no E32 execution; no QEMU; no hardware. QEMU-ready: none. Hardware-verified: none.
- **Remaining gates:** P-1 loader/F32 provider; display HAL group; keymap DLL; R-1 ESHELL/TestBootSequence ruling; unknowns §7; R1-ABI/v1 and runtime-verified toolchain for any implementation.
- **Questions for the coordinator:**
  1. Is the genuine console route still required for the first `t_cactw` proof, given §4.2 shows `t_cactw` panics without it? Or may P-3 (port fixture) be the first proof, with the console route as a later staged goal?
  2. Is the loader provider decision (P-1) assigned to a card, and does it include F32 or only a loader service?
  3. Is R-1 mitigation (a) or (b) acceptable for research, or should ewsrv launch stay out of scope until F32/ESHELL exist?
- **Next owner:** coordinator (board update, report copy to main tree, ruling on the questions); then the integrator for R1-ABI/v1.
