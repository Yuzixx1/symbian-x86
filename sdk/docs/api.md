# API reference

Headers: `runtime.h` (always) and `ipc.h` (client/server programs).
Sources of truth are `../qemu-port/user/runtime.h` and `../qemu-port/user/ipc.h`;
this page explains them. Error numbers are Symbian `KErr*` negatives
(`-1` NotFound, `-2` General, `-4` NoMemory, `-5` NotSupported, `-6` Argument,
`-9` Overflow, `-14` InUse, `-18` NotReady, `-33` TimedOut, `-38` BadDescriptor).

Entry point: `int app_main(void)` in your code. Startup calls it; returning
exits the process with the return value as the exit reason. `process_exit(reason)`
never returns.

## Console output

- `void print(const char* text)` / `void print_n(const char*, unsigned)` —
  debug-channel text transport, chunked internally. **Each call starts a new
  line on the wire**: assemble asserted lines into one buffer and emit once.

## Command line

- `int command_line(U16* buffer, unsigned capacity)` — UTF-16 tail from the
  shell (`exec PROG arg1 "two words"`). Returns character count (NUL-terminated),
  or negative (`-9` overflow when it does not fit). Cast elements `< 128` to char.

## Files (read)

- `int file_open(const char* path)` — snapshot open; absolute (`C:\A.TXT`),
  drive-relative, or relative to the inherited working directory. Returns a
  handle or negative. One snapshot per channel, up to 64 KiB files.
- `int file_read(int handle, void* buffer, unsigned capacity)` — capacity must
  be ≤ 256; returns bytes read, 0 at EOF, negative on error.
- `void file_close(int handle)` — handles auto-close at thread exit.

## Files (write)

- `int file_create_write(const char* path)` + `int file_append(int, const void*, unsigned)`
  + `int file_commit(int)` — staged write; the destination changes only on
  commit. Close without commit to abort. Total ≤ 64 KiB, 8.3 new names.
- `int file_save(const char* path, const void* data, unsigned length)` —
  one-shot wrapper. Returns `KErrNone` (0) on success.

## Directories

- `int change_dir(const char* path)` — validated `chdir`; leaves all volume
  state untouched on failure.
- `int get_cwd(char* buffer, unsigned capacity)` — returns length of
  `"X:\PATH"` or negative (`-9` when the buffer is small).

## Heap

- `void* malloc(unsigned size)` / `void free(void* ptr)` — first-fit over a
  16 KiB static pool. `free(0)` is safe. Oversize returns NULL. Single-threaded
  programs only; no growth (chunk-backed heaps are future work).

## Sleep, time, random (genuine executives, no server)

- `void sleep_ms(int ms)` / `void sleep_us(int us)` — fresh RTimer per call;
  capped at 60 s. With no other outstanding requests, the next wakeup is yours.
- `void thread_yield(void)` — sleep of zero (lets others run).
- `int time_now(void)` — seconds since 2000-01-01 (matches the RTC/`date`).
- `unsigned random32(void)` — kernel random, e.g. `random32() % 100`.

## Console input (leased)

- `int con_acquire(void)` — exclusive input lease; `-14` (InUse) when held.
- `void con_release(void)` — release; required on every exit path.
- `int con_tryread(void)` — one char 0..255, or negative (`-18` empty).
- `int read_line(char* buffer, unsigned capacity, int timeout_ms)` — echoed
  line input with backspace; ends at Enter; NUL-terminated; returns length,
  `-33` on timeout (negative timeout = wait forever). Polls every 20 ms with
  sleeps, so it burns no CPU. Type only after your program prompts: earlier
  keystrokes belong to the shell.
