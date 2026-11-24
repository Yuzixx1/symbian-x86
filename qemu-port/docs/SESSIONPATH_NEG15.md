# `SessionPath r=-15` — F32CLIENT false-pass integrity check

Date: 2026-11-24. Owner scope: `genuine/f32_probe/fsclient.cpp` + its build
script + this doc only. `kernelhwsrv/` READ-ONLY and untouched. No commit/push,
no `make clean`.

## 1. Root cause

`genuine/f32_probe/fsclient.cpp` emitted the marker `F32CLIENT: IPC round trip
OK` for **any completed IPC reply** — it rejected only `KRequestPending`
(`0x80000001`). `user/ipc.c:88-95` returns the *completion status* written by
the server (or the send error), so a completed reply carrying
`KErrServerTerminated (-15)` was accepted as a successful file-service round
trip. Because `tools/r1_genuine_retire_test.py:85` (`EFSRV_CLIENT_MARKERS`) and
`tools/r1_genuine_efsrv_test.py:156,178` assert only on that marker, the genuine
EFSRV retire gate was a **FALSE PASS** on a dead server owner.

## 2. What SUCCESS actually is

* Genuine server handler `TFsSessionPath::DoRequestL`
  (`kernelhwsrv/.../fileserver/sfile/sf_svr.cpp:852-859`) does
  `WriteL(KMsgPtr0, Session()->Path()); return KErrNone;` → success is **0**.
* `-15` is `KErrServerTerminated`
  (`kernelhwsrv/kernel/eka/include/e32err.h:226-232`): "client/server
  send/receive operation cannot run, because the server has terminated".
  It is produced when the `DServer` is closing / has detached its sessions.
  It is never a valid path reply.

## 3. Marker meaning after the fix

`F32CLIENT: IPC round trip OK` now means: a genuine `SessionPath` (opcode 14)
request was delivered **and completed with status `KErrNone` (0)**. Any other
completed status prints the explicit status (`F32CLIENT: SessionPath r=<int>`)
plus `F32CLIENT: SessionPath FAILED (no genuine KErrNone reply)` and a non-zero
exit. The status line is always printed first, so the exact reply code remains
visible in every log.

## 4. Fix applied

`genuine/f32_probe/fsclient.cpp` (only file changed):

```c
int r = ipc_call_args(session, 14, &spa);
num("F32CLIENT: SessionPath r=", r);          /* explicit status, always */
if (r == (int)0x80000001u) { ... no IPC reply ... return 2; }
if (r != 0) {
    marker("F32CLIENT: SessionPath FAILED (no genuine KErrNone reply)");
    ipc_close(session);
    return 3;
}
ipc_close(session);
marker("F32CLIENT: IPC round trip OK");
return 0;
```

The `R1FileServer` fallback (`ipc_connect`, added after the genuine
`!FileServer` attempt) is preserved, so the minimal-server test is unaffected.
`F32CLIENT.E32` rebuilt with `build_probe.sh`'s client commands: 33020 bytes,
`python3 tools/e32read.py ... --quiet` → OK.

## 5. Evidence (exact serial sequences)

Before fix — `genuine/prove_efile_genuine.py`, kernel 03:06,
`/tmp/opencode/sessionpath_neg15_before.log:1354-1359`:

```
F32CLIENT: start / start
F32CLIENT: SessionPath r=-15 / r=-15
F32CLIENT: IPC round trip OK / OK          <-- false pass on KErrServerTerminated
```

After fix — authoritative auto-launch (GenuineBoot), stable guard-only kernel
`#PF=0`, `port-shim=0`, `/tmp/opencode/efile_acceptance.log` (copy saved as
`sessionpath_after_stable.log`), lines 1350-1355:

```
EFILE: genuine file server starting (×3), File server thread registered (×1),
EFILE: E32Main returned 0 (×2)
F32CLIENT: start / start
F32CLIENT: SessionPath r=-15 / r=-15
F32CLIENT: SessionPath FAILED (no genuine KErrNone reply) / FAILED
F32CLIENT: IPC round trip OK : 0            <-- false pass removed
```

Minimal-server regression — `/tmp/opencode/sessionpath_efsrv.log:520-525`:

```
F32CLIENT: SessionPath r=0 / r=0
F32CLIENT: IPC round trip OK / OK            <-- honest pass; fallback intact
```

## 6. Gate status after the honest fix

| Gate | Result | Notes |
|---|---|---|
| `r1_genuine_efsrv_test.py` (EFSRVMIN) | **PASS** | `r=0`, marker is an honest pass |
| `r1_genuine_retire_test.py` | **FAIL 3/4** | EFSRV/F32 FAIL (marker absent on `-15`); EWSRV, ESTART/ESHELL, EUSER PASS |
| `r1_efile_acceptance.py` (strict) | **FAIL 5/9** | previously 6/9 with a false `ipc` PASS; now `ipc` FAIL + `SessionPath r=-15` FAIL |
| `r1_qemu_smoke.py` | **PASS** | no Exc/PanicKernExec |

The honest fix makes the genuine EFSRV gate FAIL. This is reported, not
papered over: the gate is not weakened and no shim was retired.

## 7. Blockers / caveats

* The residual `-15` is a genuine **server-lifetime** defect, not a benign
  status: full EFILE shows 3 starts / 2 `E32Main returned 0` (intra-process
  `RThread::Create` re-entries) before the client request, so the owning
  `DServer` detaches the session. Fixing it belongs to the `variant/` boot /
  single-owner (and EUSER TLS) owner — outside this brief. Per the updated
  project rule, re-entry markers are tolerated **only if** IPC works,
  `port-shim == 0` and there are no faults; here IPC does not work.
  Do **not** re-add the rejected entry trampoline.
* Concurrent kernel rebuilds by another agent occurred during testing
  (kernfull.elf 03:11:29, size 1025488 → 03:15:15, size 1025004). Runs against
  the transient build showed the trampoline signature
  (`#PF=2`, `port-shim=2`, IPC=0) and are **not** attributed to this fix. All
  final numbers above are from the stable guard-only kernel (`port-shim=0`,
  `#PF=0`), verified unchanged before and after the run.
* `docs/EFILE_ACCEPTANCE.md` criterion 7 already required `SessionPath r=-15
  == 0`; the client fix makes that criterion's failure visible in the retire
  gate instead of being masked by a false success marker.
