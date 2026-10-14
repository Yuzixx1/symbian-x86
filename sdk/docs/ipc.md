# IPC guide (client/server programs)

The kernel implements real Symbian client/server IPC (`DServer`/`DSession`/
messages). `ipc.h` bridges it to freestanding C — not the EUSER C++ API,
which does not exist here yet.

## Shape of a service

- Server: `ipc_server_create("Name")` → loop `ipc_receive()` → switch on
  `m.function` → `ipc_complete(&m, result)`. Accept connects (`function -1`)
  with `ipc_accept(&m, cookie)` after checking `m.args[0]` (the version word).
- Client: `ipc_connect("Name")` (connect + version handshake) →
  `ipc_call(session, function, argument)` or `ipc_call_args` for descriptors
  → `ipc_close(session)`.
- Integers ride in `args[4]`; binary data rides in descriptors
  (`IpcConst8`/`IpcBuffer8`, `IpcConst16`/`IpcBuffer16` with matching
  `TIpcArgs` flags: `EDesC8=6`, `EDes8=4`, `EDesC16=7`, `EDes16=5`).
- Server reads with `ipc_read8/16` (returns byte/char count) and answers into
  client buffers with `ipc_write8/16`. `ipc_length` reports argument size.
- Shutting the server down with a request outstanding wakes the client with
  `KErrServerTerminated` (-15) — design clients to expect it.

## Worked reference

The port's own fixtures are the canonical example (single-loop server,
multi-check client, restart and kill recovery):

- `../qemu-port/user/ipcsrv.c`, `../qemu-port/user/ipccli.c`
- Regression: `../qemu-port/tools/test_ipc.py`, `test_conc.py`

## Roadmap

Upstream `CServer2`/`CSession2`, multi-threaded servers, session sharing and
security policies arrive with the genuine EUSER/CBase integration. Until
then: one request at a time per thread, 8.3-style short service names, and
the fixtures above as your template.
