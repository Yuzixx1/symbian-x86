# One-at-a-time testing: current stabilized baseline

## Build and automated baseline

From `qemu-port/`, run:

```
python3 tools/test_stable.py
```

This runs default `make` (including ISOs), then loader relocation validation,
arguments, DLL binding/VGA output, initialized-data relocations, compiled
userland, IPC, CLI, disk-full preservation, crash attribution, concurrency,
interactive input, and the SDK end-to-end. Each subprocess exit status
is checked; logs are in `/tmp/opencode/stabilization/`. All thirteen stages
passed in the current stabilization pass. This suite uses QEMU; it does not
certify physical hardware or VirtualBox.

Primary boot image: `build/symbian-grub.iso`. Compiled programs are under
`build/user/`; building the ISO does not install these programs onto a FAT disk.
Copy the E32 files onto your test FAT32 volume before manual userland tests.

## Manual order

1. **Boot and input.** Boot the GRUB ISO, confirm the shell appears, then use
   `HELP` and `HELP DIR`. Test each keyboard/controller separately.
   Run `DATE`, then set `DATE 2027-02-28 23:59:58` and read back; invalid
   dates (`2027-02-30`, `1999-01-01`) must be rejected without changing the clock.
2. **Storage discovery.** Run `MOUNT`, then `DIR`. Confirm the expected test
   volume is mounted before proceeding. Exercise other drives separately.
3. **File operations.** On the test volume, create a directory and a small
   text file; use `TYPE`, `COPY`, `REN`, and `DEL`. Use command-specific help
   for syntax. Confirm copied contents before deleting the test files.
4. **Compiled arguments.** Run `EXEC ECHO.E32 "two words"  three`. Expect
   quotes and the repeated space in the echoed command tail.
5. **User file reads/writes.** Run `EXEC TYPE.E32 HELLO.TXT`, then
   `EXEC CP.E32 "HELLO.TXT" "COPY.TXT"`, and `TYPE COPY.TXT`. Repeat from
   a subdirectory with relative paths. CP streams in 256-byte pieces up to
   the 64 KiB PortFile limit; sources over that limit fail at open and must
   leave an existing destination unchanged.
6. **Per-process directories.** With `WORK/NOTE.TXT` and `WORK/SUB/NOTE.TXT`
   present, `cd WORK`, then `EXEC C:\CWD.E32` (inherits `C:\WORK`, relative
   open works), `EXEC C:\CWD.E32 SUB` (moves to `C:\WORK\SUB`), and
   `EXEC C:\CWD.E32 \NOPE` (fails cleanly). The shell prompt must stay in
   `WORK` throughout: user chdir never leaks into shell or volume globals.
6. **Handle cleanup.** With `C:\HELLO.TXT` present, run `EXEC FILECHK.E32`.
   Expect the independent-handles message. Run `PS`: PortFile channels should
   return to zero. Repeat to check cleanup across launches.
7. **Local IPC.** Run `EXEC IPCSRV.E32`, then `EXEC IPCCLI.E32` repeatedly.
   Expect 8-bit and 16-bit descriptor verification plus 32 verified replies. Run
   `EXEC IPCCLI.E32 stop`, restart the server, then `EXEC IPCCLI.E32 die`.
   Expect the pending request to receive KErrServerTerminated. A subsequent
   client launch should fail to connect until the server is restarted.
8. **Fault attribution.** Run `EXEC CRASH.E32`: expect `CRASH: about to fault`,
   a genuine `Exc 06` trap line, and an exit line naming `prog=CRASH.E32`.
   The shell must stay responsive (`EXEC ECHO.E32 alive`, `PS` clean).
   Run `EXITS` after any deaths: names, ids, types, and reasons persist
   past reaping (unknown fields shown as `?` for already-gone objects).
9. **Concurrency.** Run `EXEC IPCSRV.E32`, `EXEC IPCCLI.E32`, then `DATE`/`PS`
   while loaded; `KILL` the server id from `PS`, confirm clients fail to
   connect, `EXITS` records it, restart the server and confirm recovery.
10. **Interactive input.** Run `EXEC INTERACT.E32` and wait for `Name? `;
    type a name and expect the greeting plus clock/random lines. The shell
    must go quiet while the program holds the console and return after
    `INTERACT: done`. Note: every `print` call ends its own line (DebugPrint
    transport), so multi-part output arrives one fragment per line.

For a failure, retain the full serial log and note the boot mode, emulator or
machine, controller, drive letter, command, and last successful step.

## Boundaries, not completed services

- Upstream SymbianSource kernel remains read-only. Kernel IPC is genuine;
  the C runtime bridge and IPC fixtures are port code, not shipping services.
- PortFile is a 64 KiB snapshot/buffer transport, not RFs/F32. Its handles
  are ID-tracked to the creator thread+process (no dangling pointers);
  cross-thread sharing is rejected. Overlong paths fail instead of opening
  a truncated file. Per-process CWD is mutable via validated chdir/getcwd;
  validation leaves all volume globals untouched.
- Other-drive relative paths resolve from that drive's root. New FAT names
  retain the 8.3 limit.
- TYPE output still uses DebugPrint, not a binary-safe console service.
- User programs have a 16 KiB first-fit `malloc`/`free` (single-threaded, no
  growth); chunk-backed heaps still out. Exit history (`EXITS`) persists past
  reaping, but blocking `wait` still out.
- Genuine EUSER/CBase, active scheduling, asynchronous multi-request dispatch,
  and full filesystem/loader services remain unfinished integration work.
- Loader failure-path cleanup after a partial ProcessCreate/Loaded still
  needs hardening; the reaper only tracks processes remembered at resume.
  `kill` now refuses the shell's own process by pointer (not numeric id)
  and both per-process tables are gated before creation; `pzomb` no longer
  dereferences processes after releasing the container lock.
- There is no networking or protected per-process address space yet.

Passing this baseline means the listed paths passed regression; it does not
mean the remaining operating-system services are complete.
