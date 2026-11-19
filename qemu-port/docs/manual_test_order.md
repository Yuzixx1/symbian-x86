# One-at-a-time testing: current stabilized baseline

## Build and automated baseline

From `qemu-port/`, run:

```
python3 tools/test_stable.py
```

This runs default `make` (including ISOs), then loader relocation validation,
arguments, DLL binding/VGA output, initialized-data relocations, compiled
userland, IPC, CLI, disk-full preservation, crash attribution, concurrency,
interactive input, the SDK end-to-end, six sample apps, blocking wait, and chunks,
plus the file-service regression. Each subprocess exit
status is checked; logs are in `/tmp/opencode/stabilization/`. All seventeen
stages passed in the current stabilization pass. This suite uses QEMU; it does
not certify physical hardware or VirtualBox.

Primary boot image: `build/symbian-grub.iso`. Compiled programs are under
`build/user/`; building the ISO does not install these programs onto a FAT disk.
Copy the E32 files onto your test FAT32 volume before manual userland tests.
A full test disk in one go (from `qemu-port/`):

```
python3 tools/mkdisk.py seeded /tmp/my.img 64
for f in build/user/*.E32; do mcopy -o -i /tmp/my.img@@2048S "$f" "::$(basename "$f")"; done
for f in ../sdk/samples/{calc,guess,wc,rev,fib,mem}/*.E32; do mcopy -o -i /tmp/my.img@@2048S "$f" "::$(basename "$f")"; done
```

Boot it on q35 with AHCI (serial is your log):

```
qemu-system-i386 -M q35 -m 128 -display none -serial stdio \
  -kernel build/kernfull.elf \
  -drive file=/tmp/my.img,format=raw,if=none,id=d0 \
  -device ich9-ahci,id=sata -device ide-hd,drive=d0,bus=sata.0 \
  -monitor none -no-reboot
```

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
4. **Compiled arguments.** Run `ECHO.E32 "two words"  three` (two
   spaces between `words"` and `three`). Expect the echoed tail to preserve
   both the quotes and the repeated space: `ECHO: "two words"  three`. If the
   trailing spaces collapse in your terminal, the echo is still correct —
   compare against the character count.
5. **User file reads/writes.** Run `TYPE.E32 HELLO.TXT`, then
   `CP.E32 "HELLO.TXT" "COPY.TXT"`, and `TYPE COPY.TXT`. Repeat from
   a subdirectory with relative paths. CP streams in 256-byte pieces up to
   the 64 KiB PortFile limit; sources over that limit fail at open and must
   leave an existing destination unchanged.
6. **Per-process directories.** Create `WORK/NOTE.TXT` and `WORK/SUB/NOTE.TXT`
   first (step 3 or `COPY`). Then `cd WORK`, run `C:\CWD.E32` (inherits
   `C:\WORK`, relative open works), `C:\CWD.E32 SUB` (moves to
   `C:\WORK\SUB`), and `C:\CWD.E32 \NOPE` (must be rejected with a
   `chdir failed` message, leaving the directory unchanged). The shell prompt
   must stay in `WORK` throughout: user chdir never leaks into shell or volume
   globals.
 7. **Handle cleanup.** With `C:\HELLO.TXT` present, run `FILECHK.E32`.
   Expect the independent-handles message. Run `PS`: PortFile channels should
   return to zero. Repeat to check cleanup across launches.
 8. **Heap, sleep, and chunks.** Run `MALLOC.E32`, expect the
   allocator-verified line, then `REAP` and `EXITS` (the MALLOC record with
   reason 0). Run `SLEEP.E32` and expect all stages through `done`
    (a real ~0.5 s pause, plus timer setup). Run `CHUNK.E32` (creates a private chunk, then parks
   holding a global one) and `CHKOPEN.E32`; expect `CHUNK: private ok`,
   `CHUNK: global ready`, and `CHUNKOPEN: shared ok` — two separate programs
   sharing kernel memory.
 9. **Local IPC.** Run `IPCSRV.E32`, then `IPCCLI.E32` repeatedly.
   Expect 8-bit and 16-bit descriptor verification, 50 rounds of overlapping
   async requests awaited out of order, plus 32 verified replies. Run
   `IPCCLI.E32 stop`, restart the server, then `IPCCLI.E32 die`.
   Expect the pending request to receive KErrServerTerminated. A subsequent
   client launch should fail to connect until the server is restarted.
 10. **Fault attribution and wait.** Run `CRASH.E32`: expect
   `CRASH: about to fault`, a genuine `Exc 06` trap line, and an exit line
   naming `prog=CRASH.E32`. The shell must stay responsive
   (`ECHO.E32 alive`, `PS` clean).
   Run `EXITS` after any deaths: names, ids, types, and reasons persist
   past reaping (unknown fields shown as `?` for already-gone objects).
   Run `WAIT <id>` to block until a process dies (default 30 s, max 300 s);
   a live one reports `still alive`, a dead one its record. Try both:
   `WAIT` on a running `IPCSRV.E32` with 2 s, and on a finished `ECHO.E32`.
 11. **Concurrency.** Run `IPCSRV.E32`, `IPCCLI.E32`, then `DATE`/`PS`
    while loaded; `KILL` the server id from `PS`, confirm clients fail to
    connect, `EXITS` records it, restart the server and confirm recovery.
 12. **Interactive input.** Run `INTERACT.E32` and wait for `Name? `;
    type a name and expect the greeting plus clock/random lines. The shell
    must go quiet while the program holds the console and return after
    `INTERACT: done`. Two expected cosmetics: the debug transport ends every
    `print` call with a newline (multi-part output arrives one fragment per
    line), and each echoed keystroke therefore lands on its own line.
 13. **SDK smoke.** On the host, scaffold and build a fresh app
    (`python3 sdk/tools/newapp.py Smoke /tmp && make -C /tmp/Smoke`),
    copy `SMOKE.E32` to the test disk, boot, and run `SMOKE.E32`. Expect the
    greeting and a zero exit in `EXITS`.
 14. **File-service regression (card 32).** Boot `kernfull.elf` with a fresh
    disposable FAT disk containing card 36's `portfilesrv.exe` (E32 image from
    `r1-portfilesrv-e32-v1/output/`), `euser.dll`, `efsrv.dll`,
    `t_cactw.exe` and `fhserver.exe` (from `r1-build-v1`), plus `ECHO.E32`.
    Run `python3 tests/r1_file_regression/test_r1_file_regression.py`
    (equivalently via `python3 tools/test_stable.py`). Expect:
    - `portfilesrv.exe` starts (user process) and calls `FsRegisterThread`
      once at B2 (process becomes critical/system-permanent);
    - `t_cactw.exe` starts and completes (`CActiveScheduler` start/stop/wait);
    - `fhserver.exe` starts and completes (file handle sharing via `!FileServer`);
    - create/write/flush/close/reopen round trip with byte-compare after reopen;
    - EOF (zero-length read) and no-hole writes verified;
    - deliberate negatives (`FSCLIENT/19`, `FSCLIENT/20`, `Panic/-8`) behave;
    - second-boot persistence: reboot on same media, reopen, byte/size match;
    - no `FAULT:` or `Exc ` markers in the serial transcript.
 15. **Six SDK samples.** Build with `make -C ../sdk/samples` from
     `qemu-port/`, and copy their `.E32` files to the FAT volume as above.
     Run `CALC 12 * 7` (84), `CALC 20 - 5` (15), `REV hello` (olleh),
     `FIB 10` (0 1 1 2 3 5 8 13 21 34), `MEM` (24 blocks), and
     `WC HELLO.TXT` (line/word count). Run `GUESS`, enter `50`, then
     `quit`; expect a higher/lower hint and `GUESS: bye`. No `EXEC`
     prefix is needed: the shell tries the typed name and then `.E32`.

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
  retain the 8.3 limit; existing long names (LFN) can be read and executed.
- TYPE output still uses DebugPrint, not a binary-safe console service.
- User programs have a 16 KiB first-fit `malloc`/`free` (single-threaded, no
  growth); chunk-backed heaps still out. Exit history (`EXITS`) persists past
  reaping, and blocking `WAIT` reports records with a bounded timeout.
- Genuine EUSER/CBase, active scheduling, and full filesystem/loader services
  remain unfinished integration work. User programs can also allocate genuine
  kernel chunks (`chunk_create/open/base/size/adjust`) and share global ones
  across processes.
- Loader failure-path cleanup after a partial ProcessCreate/Loaded still
  needs hardening; the reaper only tracks processes remembered at resume.
  `kill` now refuses the shell's own process by pointer (not numeric id)
  and both per-process tables are gated before creation; `pzomb` no longer
  dereferences processes after releasing the container lock.
- There is no networking or protected per-process address space yet.

Passing this baseline means the listed paths passed regression; it does not
mean the remaining operating-system services are complete.

Okay... 1. Pass 2.Pass 3.Pass 4.Fail 5.Pass 6.Idunno 7.Pass 8.Fail 9.Pass-ish 10. Fail 11.Pass (note: name entry each new letter comes on a new line.)
