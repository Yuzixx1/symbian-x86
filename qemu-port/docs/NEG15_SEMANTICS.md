# What `SessionPath r=-15` means in this port (read-only semantics report)

Date 2026-11-24. Author: read-only investigation agent. `kernelhwsrv/` READ-ONLY
and untouched; no QEMU run, no source/test/build edits, no commit/push, no
`make clean`. Sole write is this file. Evidence: source + the existing clean
prove `~/.cache/sybian-r1/efile_clean_prove.log` (464 binds, port-shim=0,
tightened-client IPC=0, `SessionPath r=-15`×2, `FAILED`×2, #PF/FAULT/Panic/Exc=0,
no SIGTERM, single owner claimed, ends at `C:/>`).

## 1. Verdict (for the -15 agent)

**`r=-15` is `KErrServerTerminated`. It is a REAL server-lifecycle failure, not
a benign status and not a client marshalling bug.** The kernel is telling the
client that the `DServer` owning its session is closing/terminating (or the
session was detached by `DServer::Close`) *at the moment of the send*. It is
**never** a valid `EFsSessionPath` reply.

Therefore:
- **(b) is WRONG if it means "accept -15 as success".** That is the exact false
  positive the tightened client was introduced to remove. Counting a completed
  reply regardless of its status made the EFSRV gate a false PASS on a dead
  owner; do not reintroduce it.
- **(c) is RULED OUT** as the source of `-15` (see §3). Even if the descriptor
  were malformed, marshalling failures surface as `KErrBadDescriptor` (-14),
  `KErrNoMemory`, or `KErrDisconnected` — never -15.
- **(a) is the correct classification.** The fix must make the file-server
  session stable (or make the probe wait for a genuinely successful
  `KErrNone` reply); see §5.

## 2. Evidence: -15 is produced only by the kernel's server-closing paths

- `kernelhwsrv/kernel/eka/include/e32err.h:232`:
  `const TInt KErrServerTerminated=(-15);`
- The genuine handler cannot return it. `kernelhwsrv/.../sfile/sf_svr.cpp:852-859`
  `TFsSessionPath::DoRequestL` does exactly
  `aRequest->WriteL(KMsgPtr0, aRequest->Session()->Path()); return(KErrNone);`
  and `Initialise` at :861 also returns `KErrNone`. `sf_func.h:207`, `sf_ops.h:43`
  confirm the binding. Only other outcome is a C++ leave (e.g. `KErrNoMemory`),
  which is not -15.
- Kernel producer sites (`kernelhwsrv/kernel/eka/kernel/sipc.cpp`):
  - :1551 and :1597 `DSession::Send`: `if (!iServer || iServer->IsClosing()) { r = KErrServerTerminated; goto error; }`
    (checked before *and* after descriptor pinning). The `error:` path at
    :1619-1627 returns `r` synchronously.
  - :1300 `DSession::Add`: `if (aSvr->IsClosing()) return KErrServerTerminated;`
  - :736 `DServer::Close`: `s->Detach(KErrServerTerminated)` for every session
    when the owning thread's DServer closes.
  - :221 `ExecHandler::SessionSecurityInfo` returns it when the session has no
    live server.
- The port's own comment already records this:
  `qemu-port/variant/e32load.cpp:29-35` — "a second EFILE ... steals the name
  when the first owner's thread exits (`DServer::Close` detaches every session
  with `KErrServerTerminated`, seen by F32CLIENT as -15)".
- Transport: `qemu-port/user/ipc.c:88-95` `ipc_call_args` uses exec 77
  (`EExecSessionSendSync`, `build/export/exec_enum.h:140`); it returns the
  synchronous error from exec directly, else the completion status. Either way
  -15 originates in the kernel `Send`/`Close` paths above, not in the client.

## 3. Why it is NOT a client marshalling bug

- Opcode 14 is correct: `F32plugin.h:42-59` — `EAddFileSystem=0 … ESetSubst=10,
  ERealName=11, EDefaultPath=12, ESetDefaultPath=13, ESessionPath=14`.
- `fsclient.cpp:108-109`: `IpcArgs.flags=5` = `TIpcArgs::EDes16`
  (`e32cmn.h:6335`, `EFlagDes|EFlag16Bit`), matching genuine
  `RFs::SessionPath(TDes& aPath)` → `SendReceive(EFsSessionPath,TIpcArgs(&aPath))`
  (`cl_cli.cpp:1464-1480`).
- `fsclient.cpp:107`: `sp = {0x20000000, 32, sessionPath}`. Kernel
  `K::ParseDesHeader` (`sutils.cpp:3755-3771`) maps type `0x2` to `EPtr`
  (`u32std.h:119` `{EBufC,EPtrC,EPtr,EBuf,EBufCPtr}`): max = 32, data = the
  `unsigned short[32]` buffer. `TDesHeader::IsWriteable()`
  (`kernel.h:742`) is true (max != `KConstMaxLength`), so the server's
  `WriteL(KMsgPtr0, path)` would succeed. Length 0x0 (empty) is harmless for a
  getter. A bad header would instead produce `KErrBadDescriptor` (-14) via
  `ReadDesHeaders` → `SetArgUndefined` (`sipc.cpp:501-507`) — not -15.
- `EConnect` also succeeded (`fsclient.cpp:89-90` would have returned/printed
  "connect failed" otherwise), so the failure is confined to the send against
  a closing session, not the connect handshake.

## 4. Why THIS port produces it (root context, owner: kernel/boot)

`docs/EFILE_DUPLICATE_LAUNCH_PLAN.md` established that the "3× `starting` / 2×
`E32Main returned 0`" signature is **one process, three `app_main()` entries**:
genuine `RThread::Create` re-enters the process veneer with EBX=1/2, but the
port's `user/start.S` ignores EBX and always calls `app_main()`
(`genuine/efile/efile_glue.cpp:43-53`), so reentries re-run `E32Main` and each
calls `process_exit` = exec 115 = `EExecThreadKill`
(`user/start.S:68-77`, `build/export/exec_enum.h:178`) — killing their own
threads. Those reentries churn `CServerFs::Start()`/DServer ownership while the
main thread blocks in `E32Main`; a client session can end up attached to a
DServer that is closing, so request 14 returns -15 before any path is written.
Per `docs/EFILE_SESSION_PATH_PLAN.md:§5` this is the only cause consistent with
the evidence (session-path shape was proved valid: `C:\Private\<SID>\`).

`docs/EFILE_DUPLICATE_LAUNCH_PLAN.md:§3` shows the guard-only build once yielded
`IPC round trip OK` with the old permissive client while -15 was also present —
i.e. `KErrNone` replies do occur in the race window; the current tightened
client correctly rejects the -15 path. The alternative reentry *trampoline*
in `variant/e32load.cpp` was tried and REJECTED (EUSER TLS absolute low-address
faults at `0x110010`, port-shim=2, IPC broken); project rule keeps the
guard-only EFILE model with no trampoline.

## 5. Recommended fix and owner

- **Do not** relax `fsclient.cpp:111-125` to count -15; keep the `r == 0`
  requirement and the explicit `SessionPath r=` print.
- **Preferred, real fix (owner: kernel/boot agent, not the -15 agent):** stop
  the reentrant `app_main()` calls from creating/closing DServers. Dispatch EBX
  to the genuine thread function in `qemu-port/user/start.S` and make
  `qemu-port/genuine/efile/efile_glue.cpp:43-53` ignore EBX!=0 reentries
  (no second `E32Main`, no DServer churn), while keeping the UID3-keyed
  single-owner claim in `variant/e32load.cpp` and **no trampoline**. Then a
  single live DServer owns `!FileServer` for the client's whole session.
- **Probe-side mitigation (owner: -15 agent, within fsclient.cpp/proves):**
  until the server fix lands, make the probe wait for a *genuine* success —
  bounded retry of connect+`SessionPath` until a `KErrNone` reply is observed,
  emitting `IPC round trip OK` only on `r == 0`. This is a change to a
  genuinely successful operation, not acceptance of -15, and is legitimate
  because §4 shows `KErrNone` replies do occur in the window.

## 6. GREEN criteria (all simultaneously, against FULL genuine EFILE)

- `GenuineBoot: EFILE.E32 r=0`, `EFILE: genuine file server starting`,
  `File server thread registered` (FULL EFILE, not `GENUINEFS`/`EFSRVMIN`/
  `R1FileServer`), single-owner claim, `EFILE: port-shim` count == 0.
- `F32CLIENT: start`; `F32CLIENT: SessionPath r=0`; **`F32CLIENT: IPC round trip
  OK` count >= 1** with the integrity-checked client
  (`tools/r1_genuine_retire_test.py:85,490`); **zero** `SessionPath r=-15` and
  zero `SessionPath FAILED` on the accepted path.
- `E32Main returned` 0 (or, if reentries remain, only with `IPC round trip OK>=1`
  per the project's tolerated-marker rule), `#PF`/`FAULT:`/`PanicKernExec`/`Exc `
  == 0, no SIGTERM, log ends at the shell prompt.
- Official gate then passes: `python3 tools/r1_final_verify.py --only genuine_efsrv`
  / `python3 tools/r1_genuine_retire_test.py --only efsrv` reports `[PASS]` for
  the EFSRV section. Any remaining `r=-15` means the server lifecycle fix (§5)
  has not landed — it is never a green.

Clean prove log evidence lines: 271 (single-owner claimed), 1236 (file server
thread registered), 1234/1241/1247 (3× starting) & 1243/1249 (2× `E32Main returned 0`),
1361-1362 (`SessionPath r=-15`×2), 1363-1364 (`FAILED`×2).
