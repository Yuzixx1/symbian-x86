# R1-SERVICE-MANAGER-DESIGN v1 — S-02 (ordered startup + supervision)

Status: design only. No code, no build, no QEMU, no kernelhwsrv changes. Recovered "done" criteria from
`full_os_checklist.md` §14 (deleted in dabf70b; text from git 1e67072): S-01 "kernel shell spawns everything
ad hoc; no service manager"; S-02 "ordered startup (storage -> F32 -> console -> loader -> IPC services),
restart-on-death supervision, shutdown protocol"; build order step 4 "S-02 + F-06 (service manager + F32/RFs)
-> retire PortFile". S-04 (EUSER/CBase) and I-08 (CServer2 portfilesrv) now land before S-02; I-07 (active
scheduler) is the immediate predecessor.

## Current state
- `variant/kern_shell.cpp:548` calls `E32Run(eBuf, got, inner, aTail)` ad hoc from the shell command loop;
  there is no boot-time service orchestrator. `portfilesrv` is just another E32 EXE the user types into the
  shell, not a managed service.
- `user/portfilesrv.cpp:209` `app_main` manually registers the FS owner thread (`exec0(81)`), constructs a
  `FatBackend` in-place, then `FileServer::NewL` + `Publish()`; `CServer2::Start` owns the accept loop.
- `fs/r1_fs_server.cpp:24` publishes `!FileServer` via `Start`; `NewSessionL`/`ServiceL` are the only IPC.
- `user/ipc.c` exports the exec-number bridge: server_create(126), receive_async(34)/cancel(35), complete(65),
  connect(128), call(77). `user/start.S` `_start` -> `app_main` -> `process_exit` (exec 115).
- Kernel side: `E32Run` (`variant/e32load.cpp:670`) validates -> `Kern::ProcessCreate` -> copy/reloc/bind ->
  `proc->Loaded` -> `NKern::LockSystem`/`proc->Resume`; `DProcess::Create` (`sprocess.cpp:127`) builds the main thread via
  `NewThread` with `iFunction = iFileEntryPoint`; `ExecHandler::ThreadCreate` (`sexec.cpp:1919`) mirrors it.
  Death primitives already exist: `E32Reap`, `E32Kill`, `E32ProcessDead`, `E32FindExit`, `E32PrintExits`
  (`e32load.cpp:963-1064`) plus `ExecHandler::ProcessLogon` (`sexec.cpp:1172`) for death notification.

## Needed (what is missing)
1. Ordered boot: storage (FAT32 mount) -> F32/`!FileServer` -> console -> loader -> other IPC services,
   each started only after its predecessor publishes its global name.
2. Supervision: logon on each child process, restart on unexpected death with a bounded retry policy.
3. Shutdown: ordered stop (clients before servers), publisher-side name withdrawal, final `E32Reap`.

## Concrete next steps
1. Add a service-manager E32 (`user/svcmgr.cpp`, `app_main`) driving a static table of
   `{image path, publish name, depends-on}` entries. It starts siblings by calling the existing `E32Run`
   path re-exposed as a trusted exec (kernel-side `SvcMgrLaunch`, not shell text parsing), mirroring how the
   shell already loads a file into a buffer and calls `E32Run`. Keep `kernelhwsrv/` untouched.
2. Supervision via `ExecHandler::ProcessLogon` / `E32ProcessDead` + `E32FindExit`: on child death check
   exit type/reason, then re-launch with backoff; treat deliberate shutdown separately from crash.
3. Enforce order by `ipc_connect(name)` readiness probes (server_create/connect already work) before
   advancing the table; storage/FAT mount stays kernel-side as today, F32 is `portfilesrv`, console/loader
   entries are stubbed until H-02/L-06 land.
4. Shutdown protocol: manager receives a stop IPC, walks the table in reverse, kills children via `E32Kill`,
   closes published names, calls `E32Reap`, then `process_exit`.
5. Retire `PortFile`: once `!FileServer` is manager-owned, remove the shell's direct PortFile path; verify
   with a QEMU boot log showing the ordered `publish` markers and a kill/restart marker (Q-01 extension).
