# R1 file-service regression tests

Card 32 — wire a file-service regression into `tools/test_stable.py`, using
card 36's E32 `portfilesrv.exe` as the registered `!FileServer` host and the
card-30-approved `t_cactw.exe` (scheduler proof) and `fhserver.exe` (file
proof) as fixtures.

## What this tests

1. **Boot + !FileServer registration** — load `portfilesrv.exe` (card 36 E32
   image), which calls `UserSvr::FsRegisterThread()` exactly once at B2, making
   it the registered `!FileServer` host (process-critical, system-permanent).
2. **Fixture launch** — run `t_cactw.exe` (`CActiveScheduler` start/stop/wait
   directives) and `fhserver.exe` (file handle sharing via `!FileServer`).
3. **File round trip** — create/write/flush/close/reopen a file, verify bytes
   after reopen, test EOF (zero-length read) and no-hole writes, exercise both
   share orders (exclusive-then-readers and readers-then-exclusive).
4. **Deliberate negatives** — expect `FSCLIENT/19` (not found), `FSCLIENT/20`
   (bad handle), and `Panic/-8` where the genuine server panics on invalid
   handle usage.
5. **Second-boot persistence** — reboot on the same disposable FAT image,
   reopen the file, compare bytes and size.

## How to run

From `qemu-port/`, with the exclusive QEMU slot:

```sh
python3 tools/test_stable.py
```

The file-service regression runs as one of the sequential checks
(`test_r1_file_regression`). It builds `kernfull.elf`, provisions a fresh 64 MB
seeded FAT disk, copies `portfilesrv.exe`, `euser.dll`, `efsrv.dll`,
`t_cactw.exe`, `fhserver.exe` and `ECHO.E32` onto it, boots QEMU, and runs the
phases above. Logs go to `/tmp/opencode/stabilization/test_r1_file_regression.log`
and the guest serial transcript to
`/tmp/opencode/stabilization/r1_file_regression_scratch/serial.log`.

## Inputs (read-only, outside the repo)

| File | Source | Purpose |
| --- | --- | --- |
| `portfilesrv.exe` | `/home/yuzi/.cache/sybian-r1/r1-portfilesrv-e32-v1/output/portfilesrv` | `!FileServer` host (card 36) |
| `euser.dll` | `/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll` | import library for portfilesrv/t_cactw/fhserver |
| `efsrv.dll` | `/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/efsrv.dll` | import library for fhserver |
| `t_cactw.exe` | `/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/t_cactw.exe` | scheduler proof (card 30) |
| `fhserver.exe` | `/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/fhserver.exe` | file proof (card 30) |
| `ECHO.E32` | `build/user/ECHO.E32` | legacy survival check |

No commits, pushes, installs, or redistribution. No shared files touched
beyond `test_stable.py`, `manual_test_order.md`, and this directory.
