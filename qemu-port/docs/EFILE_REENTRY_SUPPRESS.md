# EFILE user-thread re-entry suppression (x86 port shim)

## Problem

The genuine EFILE.EXE (`!FileServer`) was dying during boot. From the baseline
clean prove (`~/.cache/sybian-r1/efile_clean_prove.log`, before this change):

| marker | count |
|---|---|
| `e32run: bind slot` | 464 |
| `EFILE: genuine file server starting` | 3 |
| `File server thread registered` | 1 |
| `E32Main returned` | 2 |
| `EFILE: port-shim` | 0 |
| `F32CLIENT: SessionPath r=-15` | 2 |
| `F32CLIENT: IPC round trip OK` | 0 |
| `#PF` / `FAULT:` / `PanicKernExec` / `Exc ` | 0 |

Strict acceptance was **FAIL 5/9**.

### Root cause

The genuine EKA2 x86 user-thread start path
(`kernelhwsrv/kernel/eka/kernel/x86/ckernel.cia:109-123`) does **not** jump to
the function passed to `RThread::Create()`. It enters user mode at
`DCodeSeg::iEntryPtVeneer` for the first thread and at `DProcess::iReentryPoint`
for every later thread; genuine EUSER's entry veneer decodes the
`SThreadCreateInfo` on the stack to reach the real thread function. This port's
entry is our own `user/start.S -> app_main`, which has no such dispatcher.

EFILE creates two genuine threads — the loader thread in `InitLoader()`
(`sf_ldr.cpp:367`) and the startup thread in `commonInitialize()`
(`sf_main.cpp:517`). Each re-entered `app_main`, re-ran genuine `E32Main()`,
returned 0 through its `ServerIsRunning()` shortcut, and then `process_exit()`
tore down the file-server process and detached the client session. The client
therefore received `KErrServerTerminated` (-15) instead of a genuine session
path.

A user-side trampoline dispatching to the real thread function was tried in an
earlier iteration and is explicitly rejected by `AGENTS.md`: genuine EUSER TLS
uses absolute low addresses (e.g. `0x110010`) mapped supervisor-only, so the
trampoline faulted with `#PF`. This change implements the alternative: suppress
re-entry and keep the one genuine server thread alive.

## Shim (files owned by this task, `kernelhwsrv/` untouched)

1. **`genuine/efile/efile_glue.cpp`** — `app_main()` now claims ownership with
   an atomic test-and-set on a process-local flag. Only the first (original
   process) thread runs `EfileInstallHeap()`, constructors and genuine
   `E32Main()`. The two re-entrant threads return 0 immediately; their
   `Rendezvous()`/`Logon()` complete with KErrNone, so `InitLoader()` and
   `commonInitialize()` proceed to `CActiveSchedulerFs::Start()` and the single
   genuine `CServerFs` serves IPC. Re-entrant threads log
   `EFILE: re-entrant app_main suppressed` (count 2, not matching any gate
   needle).

2. **`genuine/efile/patched/sf_request.cpp`** — a verbatim copy of the genuine
   source plus one x86 port macro, `EFILE_PORT_NOOP_LOCK()`, replacing the 20
   `FsPluginManager::ReadLockChain()` / `UnlockChain()` calls. Once the server
   actually stays alive, the genuine `TFsSessionPath` request deadlocked inside
   the prebuilt EUSER.DLL `RReadWriteLock::ReadLock()` (its reader semaphore
   never signals). No plugin is ever mounted on this x86 port (no ESTART, no
   media drivers), so the plugin chain is always empty and the lock is
   redundant. `build_efile.py` compiles this patched copy unconditionally.
   No genuine logic is changed in `kernelhwsrv/`.

3. **`genuine/build_efile.py`** — compiles the patched `sf_request.cpp` on every
   build; the instrumented `sf_main.cpp` copy remains diagnostic-only under
   `EFILE_DEBUG_MAIN`.

`EFILE.E32` rebuilt: 463 EUSER imports, UID3 `0x100000bf`, UTF-16 literals,
`e32read` clean, IAT `0x40000`.

## After (fresh logs)

Clean prove (`.../efile-final-prove/efile_clean_prove_raw.log`):

| marker | expected | actual |
|---|---|---|
| `e32run: bind slot` | == 464 | 464 |
| `EFILE: genuine file server starting` | >= 1 | 1 |
| `File server thread registered` | >= 1 | 1 |
| `E32Main returned` | == 0 | 0 |
| `EFILE: port-shim` | == 0 | 0 |
| `F32CLIENT: IPC round trip OK` | >= 1 | 2 |
| `F32CLIENT: SessionPath r=` | info | 2 (both `r=0`) |
| `F32CLIENT: SessionPath r=-15` | == 0 | 0 |
| `#PF` / `FAULT:` / `PanicKernExec` / `Exc ` / SIGTERM | == 0 | 0 |
| `EFILE: re-entrant app_main suppressed` | info | 2 |

`CAPTURE: PASS`, `EVIDENCE: PASS (18/18 hard rows)`.

## Gate verdicts

| gate | verdict |
|---|---|
| `r1_efile_clean_prove.py` (fresh private log) | PASS, 18/18 hard rows |
| `r1_efile_acceptance.py` (fresh log) | PASS, **9/9** |
| `r1_genuine_retire_test.py` | PASS 4/4; **EFSRV/F32 flipped to PASS** (EWSRV, ESTART/ESHELL, EUSER also PASS) |
| `r1_qemu_smoke.py` | PASS |
| `r1_genuine_efsrv_test.py` | PASS |

## Risks / follow-ups

- The re-entrant EFILE threads no longer run the genuine loader/startup thread
  functions. On this port those functions only load ESTART/media drivers that
  are absent, and the existing `UserPanic`/`UserSetCritical` shims already
  handle that path; no functional file-server capability is lost.
- The plugin-chain lock is a no-op. Safe while the chain is always empty; any
  future work that mounts genuine plugins must replace the shim with a working
  per-process TLS mapping (`mmu/pd.cpp` / EUSER loader), which is a separate,
  explicitly approved task per `AGENTS.md`.
- The flag is process-local to EFILE.E32. F32CLIENT still re-enters `app_main`
  twice; both of its sessions now succeed (`r=0`), but the general x86
  user-thread re-entry issue remains for other images.
- No `#PF`, `FAULT`, panic, exception, or `port-shim` diagnostic appeared in
  any of the five gate runs above.
