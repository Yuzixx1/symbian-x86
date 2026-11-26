# CleanupStack::iStack link failure (EFSRVMIN)

**Symptom** (`/tmp/opencode/efsrvmin_build.log`): `genuine/efsrv_min/r1_fs_server.o` —
`R1Fs::FileServer::NewSessionL` undefined reference to `CleanupStack::iStack`
(.text+0x8f/0x98/0xcc/0xd6).

**Where it lives:** declared only, [`user/e32base.h:431`](../user/e32base.h:431)
`static CBase* iStack;`. The class is entirely inline (`PushL`/`Pop`/`PopAndDestroy`,
lines 421-428), so that storage is the only out-of-line symbol any image needs. `nm`
over `build/*.o` and `genuine/**/*.o`: this undefined ref is the only one in the tree —
nothing defines it. `kernelhwsrv/` has no CleanupStack (checked, read-only); genuine
Symbian keeps it in ESTLIB/EUSER, which our tree does not ship, so no existing object
can be linked instead.

**Decision:** define the storage in the image's own glue TU. Not `user/ca_glue.cpp`
(not on the EFSRVMIN link line: `start.o runtime.o ipc.o efsrv_min.o malloc.o
r1_fs_core.o r1_fs_server.o`), not `fs/` (shared freestanding engine code).

**Change:** [`genuine/efsrv_min/efsrv_min.cpp:45`](../genuine/efsrv_min/efsrv_min.cpp:45),
after the placement-new line: `CBase* CleanupStack::iStack = nullptr;`

**Ownership:** port glue. `e32base.h` is our EUSER shim; the definition is ours, one
per image, beside the existing per-image `operator new/delete`. One slot is correct:
EFSRVMIN dispatches on one owner thread.
