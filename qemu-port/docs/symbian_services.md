# SymbianSource service integration

The upstream basis is https://github.com/SymbianSource, specifically the
read-only checkout of `oss.FCL.sf.os.kernelhwsrv`. Port adaptations belong
in `qemu-port/`; upstream kernel services should be used rather than duplicated.

## Executing now: genuine kernel client/server IPC

`user/ipc.c` bridges the freestanding user runtime to the existing executive
calls. Actual server registration, sessions, message queues, blocking,
completion, disconnect, and server-death handling execute upstream `sipc.cpp`.

ABI and lifecycle references in the upstream checkout:

- `kernel/eka/include/e32cmn.h`: RMessage2 and TIpcArgs layouts.
- `kernel/eka/euser/us_ksvr.cpp`: RSessionBase::DoConnect and SendSync.
- `kernel/eka/euser/us_exec.cpp`: RServer2::Receive.
- `kernel/eka/euser/cbase/ub_svr.cpp`: session-cookie setup.
- `kernel/eka/kernel/sipc.cpp`: actual IPC implementation.

`IPCSRV.E32` and `IPCCLI.E32` are integration fixtures, not replacements
for F32 or another shipping Symbian service. Run `python3 tools/test_ipc.py`
to exercise absent-server handling, connect, repeated integer requests,
unsupported requests, disconnect, shutdown, restart, and server death with
a pending client request. The server blocks in genuine request waiting while
the shell and client execute. The bridge currently assumes one outstanding
request per thread; it does not implement an active scheduler.

The bridge also implements 8-bit and 16-bit descriptor Read/Write through genuine
MessageIpcCopy and GetDesLength. Tests check a 257-byte binary payload (including
zero bytes), multi-chunk copies, nonzero read offsets, descriptor length updates,
an output guard byte, empty input, undersized output (KErrOverflow), incorrect
argument types and const output (KErrBadDescriptor), and negative service offsets.
16-bit coverage includes a 129-char payload with embedded zero, offset reads,
and rejection of 8-bit copies on 16-bit descriptors. ServerReceive/ServerCancel is tested with a pending receive before readiness;
the same server then handles the client workload. This is receive cancellation,
not a generic service-request cancellation protocol.

Server teardown uncovered an allocator mismatch: global new continued using
the bootstrap pool after kernel-heap initialization, but TServerMessage is
released via Kern::AsyncFree. `variant/kernfull_glue.cpp` now directs runtime
malloc/new allocations to the kernel heap, retaining the pool only for boot.

## Next integration layers

1. Concurrent asynchronous requests and dispatch using upstream EUSER
   active-object/scheduler semantics.
2. Build the genuine EUSER/CBase dependencies and loader import ABI needed
   for upstream CServer2/CSession2, rather than treating this C bridge as SDK
   compatibility.
3. Bring up the upstream file-server startup and RFs/RFile session protocol,
   adapting storage through a port backend. PortFile remains transitional:
   64 KiB snapshot/buffer, inherited + mutable validated CWD, 8.3 names.
   Streaming CP now covers the full PortFile range; larger files need a real
   server with offset reads/writes.
4. Integrate loader and console services against their upstream contracts.

The generated pilot EUSER.DLL is still not the upstream EUSER implementation.
These IPC fixtures establish the real transport but do not establish ordinary
Symbian application compatibility or a complete system-service stack.
