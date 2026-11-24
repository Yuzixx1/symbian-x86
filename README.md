# actually human text first:
Hello! Im yuzixx, a guy who has NO IDEA how to program nor how OS'es work.
I have been simply vibecoding this whole thing. I know its contreversial,
but im bored and why not use that sweet free ai? Anyway, heres what the ai
wrote for it and what it knows about it:
the text below this is all AI.


# Symbian on x86 — genuine EKA2 nanokernel + Kern, ported to QEMU PCs

This project boots the real Symbian EKA2 kernel (nanokernel + full Kern,
SymbianSource, read-only) on bare QEMU x86 PCs and grows a usable operating
system around it: DOS-style shell, FAT32 storage, compiled user programs,
genuine kernel IPC, and an app SDK. The GUI comes last.

## Layout

- `kernelhwsrv/` — upstream Symbian kernel/HW-services tree (**read-only**,
  Eclipse Public License v1.0; see headers such as
  `kernel/eka/kernel/sipc.cpp`). It is a shallow clone of
  `https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv.git`.
  When publishing, prefer a submodule pointing there over vendoring this
  directory (its `.git` also carries a local-only identity).
- `qemu-port/` — the port: BIOS/UEFI boot, x86 variant (PIC/PIT/UART/CMOS),
  ATA/AHCI/USB drivers, FAT32, E32 loader, shell, user runtime, tests.
  Start here: `qemu-port/README.md`.
- `sdk/` — application development: scaffolder, space-safe builder, six
  verified example apps, API/limits/IPC docs. Start here to write programs:
  `sdk/README.md`.

## Quick start

Prerequisites (Linux): `gcc` with 32-bit multilib, binutils, Python 3,
`mtools`, `qemu-system-i386`, GRUB tools (`grub2-mkimage`), OVMF firmware
for UEFI boots. `sh qemu-port/tools/check_env.sh` verifies the toolchain.

```
cd qemu-port
make                    # kernels + all boot ISOs (test media never stale)
python3 tools/test_stable.py   # 16 stages: build + 15 QEMU regressions
```

Primary boot image: `qemu-port/build/symbian-grub.iso` (BIOS + UEFI).
`sdk/` has the app walkthrough (`sdk/docs/getting-started.md`).

## Verification

From `qemu-port/`, run `python3 tools/r1_final_verify.py` for the full
one-command suite (11 tests, selfcheck through DOOM; stages a fresh DOOM ISO by
default). For a fast pre-flight only, use `python3 tools/r1_os_selfcheck.py`.
See `qemu-port/docs/DOCS_INDEX.md` for the documentation index and
`qemu-port/docs/FINAL_VERIFY_REVIEW.md` for the harness review.

## Status (2026-10-20, honest)

Done and regressed in QEMU: interactive shell with FAT32 read/write/format
and multi-drive support; ring-3 E32 programs with args, files, directories,
heap, sleep, clock, random, and leased console input; genuine kernel
client/server IPC (integers + 8/16-bit descriptors); exit history, crash
attribution, kill recovery; app SDK with six example programs. Type a program
name directly (for example `CALC 12 * 7`); `.E32` is tried automatically.

Not done: per-process memory protection (paging off by design), full DLL
loading, file/console system servers, active-scheduler dispatch, networking
of any kind, GUI. The complete list lives in
`qemu-port/docs/full_os_checklist.md`; the one-at-a-time test order in
`qemu-port/docs/manual_test_order.md`. Future milestones and compatibility
goals are in `qemu-port/docs/project_roadmap.md` (not implemented status).
For multi-agent development, see `qemu-port/docs/parallel_work.md` before
assigning overlapping work.

## Contributing

- Never edit `kernelhwsrv/` — port changes belong in `qemu-port/`.
- Every feature lands with a QEMU regression in `qemu-port/tools/test_*.py`
  wired into `test_stable.py`, plus a manual step in `manual_test_order.md`.
- Keep `make` (default target) rebuilding every test artifact.

## License

- `kernelhwsrv/` keeps its upstream EPL-1.0 terms.
- `qemu-port/` and `sdk/` have no license file yet — that choice belongs to
  the project owner before publishing (no license = all rights reserved).
