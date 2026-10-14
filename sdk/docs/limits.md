# Limits (read before shipping)

These are transitional-implementation boundaries, not mysteries. Each has a
regression behind it; each is slated for replacement per the port's
`docs/full_os_checklist.md`.

- **Programs must be < 64 KiB** (shell load buffer) and files ≤ 64 KiB
  (snapshot transport). Bigger inputs fail cleanly; destinations are preserved.
- **One thread per process.** No thread-spawn API yet; servers are
  single-loop. `malloc` is single-threaded.
- **One console lease at a time.** `con_acquire` returns InUse while held.
  Always release; crashes are reclaimed by the shell, not instantly.
- **One outstanding async request per thread.** Sleeps and IPC calls assume
  the next wakeup is theirs. Do not overlap them.
- **Text console.** Every `print` call starts a new line; no cursor control,
  no binary-safe output, no stdin without the lease.
- **Snapshot filesystem, not F32.** No file locking, no notifications, no
  long-name creation, no journaling. Cross-drive relative paths start at the
  other drive's root.
- **Command lines only** (no environment block), 8.3 names for new entries,
  no capability enforcement, no per-process memory protection yet.
- **No C library or C++ runtime.** Freestanding C only; `long long` division
  works (libgcc is linked) but `printf`/`fopen` do not exist.
- **QEMU-first.** The automated suite covers QEMU q35/AHCI; hardware and
  VirtualBox need your own runs (port `docs/hw_tests.md`).
