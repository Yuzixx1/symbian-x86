# Genuine EFILE `exit: type=2 reason=111` — root cause and exact port-side fix

Scope: read-only diagnosis from logs + `kernelhwsrv/` sources. `kernelhwsrv/`
was not modified. No build/test/variant/script files were changed. Primary
evidence: `/tmp/opencode/fallback_efile_genuine.log` (also
`/tmp/opencode/prove_utf16_control.log`, identical) and
`/tmp/opencode/r1_retire_efsrv.log:1235`.

## 1. What the exit means

`variant/e32load.cpp:1052-1054` prints `exit: id=.. type=2 reason=111` from
`DProcess::iExitType` / `iExitReason`. In EKA2, `EExitPanic == 2`, so the
EFILE main thread was killed by a **USER panic with reason 111**. In
`kernelhwsrv/kernel/eka/include/e32panic.h:1132`:

```
EThrdHeapMaxLessThanMin=111,
```

and `kernelhwsrv/kernel/eka/euser/us_ksvr.cpp:3563-3570,3594-3643` raises it
in `RThread::Create(const TThreadCreateInfo&)`:

```
3569: __ASSERT_ALWAYS(aCreateInfo.iHeapMaxSize >= aCreateInfo.iHeapMinSize,
                       ::Panic(EThrdHeapMaxLessThanMin));
3642: createInfo.SetCreateHeap(aHeapMinSize, aHeapMaxSize);
```

`User::Panic` (`us_func.cpp:725-741`) calls
`ExitCurrentThread(EExitPanic, aReason, &cat)` → exec 115 →
`ExecHandler::ThreadKill` → `DThread::Die(EExitPanic, 111, …)`. That is the
exact `type=2 reason=111` seen in the log.

The panic is the **TInt-only global `Panic()`** overload, not
`User::Panic(const TDesC16&, TInt)`. `genuine/efile/efile_glue.cpp:63-65`
only shims the latter (`_ZN4User5PanicERK7TDesC16i`), which is why the log
has **no** `EFILE: port-shim User::Panic reason=` line and no
`EFILE: E32Main returned` line — the thread dies inside `E32Main()`.

## 2. Which call supplies max < min — exact chain

`kernelhwsrv/userlibandfileserver/fileserver/sfile/sf_ldr.cpp:360-367`:

```
365: RHeap* h = (RHeap*)&User::Allocator();
366: TInt maxsize = h->MaxLength();   // loader heap max = file server heap max
367: TInt r = loaderThread.Create(KLoaderThreadName, LoaderThread,
                                  KLoaderStackSize, KHeapMinSize, maxsize, NULL);
```

`InitLoader()` is reached from genuine `sf_main.cpp:491`
(`__ASSERT_ALWAYS(InitLoader()==KErrNone,Fault(ELdrRestartInit))`) inside
`commonInitialize()`, which `E32Main()` calls at `sf_main.cpp:572`
**immediately after** `UserSvr::FsRegisterThread()` (`:570`). This matches
the log ordering exactly:

```
1230: EFILE: genuine file server starting
1232: File server thread registered     <- sf_main.cpp:570 (FsRegisterThread)
1243: exit: id=9 type=2 reason=111 prog=efilex.E32
```

`RAllocator::MaxLength()` is **not virtual**;
`kernelhwsrv/kernel/eka/common/alloc.cpp:557-560`:

```
TInt RAllocator::MaxLength() const
    { return ((RAllocator*)this)->DebugFunction(EGetMaxLength); }
```

`EGetMaxLength == 49` (`e32cmn.h:2760`, `EGetSize=48, EGetMaxLength, …`);
the genuine `RHybridHeap::DebugFunction` returns `iMaxLength`
(`common/debugfunction.cpp:97-101`).

The port glue replaces the heap with a fake allocator:
`genuine/efile/efile_glue.cpp:38-49`:

```
static int QInt1(void*, int) { return 0; }
static void* sHeapVtable[10] = {
    QAlloc, QFree, QReAlloc, QInt0, QInt0, QInt0, QInt0,
    QInt1,       // slot 8 = MAllocator::DebugFunction -> 0 for EGetMaxLength
    QInt1, QInt1,
};
```

`MAllocator`'s 10 pure virtuals are in vtable order
(`e32cmn.h:2630-2643`): Alloc, Free, ReAlloc, AllocLen, Compress, Reset,
AllocSize, Available, **DebugFunction (slot 8)**, Extension_. Casting
`TQAllocator` to `RHeap*` is legal for the allocation calls, but
`DebugFunction(EGetMaxLength)` returns **0**. Hence `maxsize = 0`, which is
less than `KHeapMinSize`, so `sf_ldr.cpp:367` trips USER 111.

## 3. Why this is not a probe timing / connect-name problem

* The same `reason=111` appears with the control client, with
  `/tmp/opencode/prove_utf16_fsreg.log:1247`, and with the retire test
  (`r1_retire_efsrv.log:1235`, staging `EFILE.E32` under its real name).
* Without `FsRegisterThread` the same image exits `reason=0`
  (`fallback_efile_nofsreg.log:1242`) — i.e. the panic is deterministic in
  the EFILE image, only the registration path changes how it is reaped.
* `tools/r1_genuine_efsrv_test.py` passes `F32CLIENT: IPC round trip OK`
  against the minimal `EFSRVMIN` server, so the client protocol and the
  patched `!FileServer` connect name are fine. `F32CLIENT: connect failed`
  in this log (`fallback_efile_genuine.log:1344-1345`) is a downstream
  symptom: the real `!FileServer` thread died before any client could
  connect.
* The built `EFILE.E32` contains `EFILE: port-shim User::Panic reason=%d`
  and `EFILE: E32Main returned %d` but not the newer `EFILE-DBG:` prints;
  irrelevant to the cause, but explains why those debug lines are absent.

## 4. Exact needed change (outside this agent's ownership — NOT applied)

The fix belongs in the port glue, **not** in `kernelhwsrv/` (read-only) and
not in `genuine/prove_efile_genuine.py`. Change vtable slot 8 of
`sHeapVtable` in `genuine/efile/efile_glue.cpp:38-46` so `DebugFunction`
reports a heap max at least `KHeapMinSize`. Concretely:

```cpp
// genuine/efile/efile_glue.cpp (port glue only; kernelhwsrv untouched)
enum { EPortDebugGetMaxLength = 49 };   // RAllocator::EGetMaxLength (e32cmn.h:2760)
static int QDebugFunction(void*, int aFunc, void*, void*)
    {
    // genuine sf_ldr.cpp:366 does h->MaxLength() for the loader thread's
    // max heap; returning 0 makes RThread::Create panic USER 111.
    if (aFunc == EPortDebugGetMaxLength) return sizeof(sHeap);  // 8 MiB
    return 0;
    }
static void* sHeapVtable[10] = {
    (void*)QAlloc, (void*)QFree, (void*)QReAlloc,
    (void*)QInt0, (void*)QInt0, (void*)QInt0, (void*)QInt0,
    (void*)QDebugFunction,   // slot 8 = DebugFunction (was QInt1)
    (void*)QInt1, (void*)QInt1,
};
```

Keep `QAlloc`/`QFree` as the bump allocator; no genuine source changes.
After rebuilding `EFILE.E32` with this glue, `InitLoader()`'s
`loaderThread.Create(..., KHeapMinSize, 8MiB, ...)` passes the
`us_ksvr.cpp:3569` check, `E32Main()` proceeds past `commonInitialize()` to
`CActiveSchedulerFs::Start()` (`sf_main.cpp:573`), `CServerFs::New()`
publishes `!FileServer`, and `F32CLIENT: IPC round trip OK` should become
observable. If the loader thread's own heap then fails elsewhere on this
port, diagnose that separately — do not lower the assertion or fake the
`!FileServer` name in the prove script.
