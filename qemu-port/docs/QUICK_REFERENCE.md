# Quick Reference — EKA2 x86 QEMU port (one-page cheat sheet)

Current image: `build/symbian-grub.iso` (GRUB2 El Torito, full `kernfull`
kernel). Full recipe: [`r1_boot_and_selftest.md`](r1_boot_and_selftest.md).
Troubleshooting: [`FAQ_TROUBLESHOOTING.md`](FAQ_TROUBLESHOOTING.md).

## 1. Boot (primary GRUB ISO)

From `qemu-port/` (the workspace path contains a space, so always run `make`
here — paths in the Makefile are relative):

```
qemu-system-i386 -M q35 -m 128 -vga std -serial stdio -no-reboot \
  -cdrom build/symbian-grub.iso -boot d \
  -netdev user,id=n0 -device e1000,netdev=n0
```

- Wait through the ~10 s GRUB countdown; GRUB auto-selects the genuine full
  kernel and drops you at the `EKA2>` prompt. The shell is mirrored to COM1 and
  to the VBE LFB (VGA window) — **use `-vga std`, not `-display none`**.
- `-device e1000` is required for the netbridge/selftest and network apps.
  SLIRP: guest `10.0.2.15`, gateway `10.0.2.2`.
- GRUB loads `PROGRAMS.IMG` as a Multiboot module → RAM disk dev 8 → auto-mount
  at `X:` (boot prints `X: mounted (programs from GRUB module)`). FAT is
  **already mounted** — do NOT type `MOUNT`.

## 2. Shell essentials

| Command | Purpose |
|---|---|
| `DIR [/W] [X:][path [pattern]]` | List files; `*`, `?` wildcards; `/W` wide |
| `CD [X:][path|..|\]` | Change directory (`CD X:` stays switched) |
| `TYPE [X:]path` | Print a text file (`TYPE X:\README.TXT`) |
| `VOL` / `DRIVE [X]` | List volumes / show or switch active drive |
| `X:` | Switch to the programs drive (DOS-style) |
| `PS` | List processes and container PIDs (use with `KILL`) |
| `KILL <id>` | Kill a *user* process; `KILL 1` fails — pid 1 is `ekern.exe`, protected |
| `MEMINFO` / `UPTIME` | Heap/RAM/object census / ms since boot |
| `HELP [command]` | Built-in summary or per-command usage (`CMD /?` also works) |
| `CLS` / `VERSION` / `DATE` | Clear screen / kernel+CPU info / show-set RTC |

Other useful verbs: `WRITE`, `COPY`, `REN`, `DEL`, `MKDIR`, `RMDIR`, `FORMAT`,
`THREADS`, `SEGS`, `EXITS`, `WAIT`, `REAP`, `REBOOT`, `DISPLAY`, `PCI`, `IDE`,
`NIC`, `NETBRIDGE`, `AUDIO`, `USB`. See
[`SHELL_REFERENCE.md`](SHELL_REFERENCE.md) for every command and path rule.

## 3. Run a program

Unrecognized words launch an E32 from `X:` (with `.E32` auto-appended):

```
CALC                    REM integer expression calculator
X:CALC                  REM explicit drive prefix
SNAKE                   REM game
WBROWSER                REM text-mode web browser (needs e1000)
SELFTEST                REM all core services, single PASS/FAIL verdict
```

- `VOL` / `X:` / `DIR` to explore; `~70` bundled apps are catalogued in
  [`USER_APPS_CATALOG.md`](USER_APPS_CATALOG.md).
- A SELFTEST pass ends with `ST: failures=0` and
  `ST: RESULT PASS - all core services operational`. netbridge needs an e1000;
  bare `ipc_connect` was removed, use the supported IPC round trip.
- Install `EUSER.DLL` (ordinal 915 `RDebug::Printf` import) at `qemu-port/` before
  `make isogrub`, or many programs fail to load.

## 4. Build

```
make check-env          # verify gcc/as/ld/make/QEMU + -m32 probe
make                    # default: kernels + all ISOs + userland
make userland           # build all build/user/*.E32
make isogrub            # primary build/symbian-grub.iso (PROGRAMS.IMG module)
make kernfull           # full kernel + smoke boot
```

Prereqs: `python3 -m pip install pycdlib`; `dnf install mtools grub2-tools`;
QEMU, gcc/g++ with `-m32`, GNU as/ld, perl. `make check-env` does not check
pycdlib/mtools/GRUB. Avoid `make clean` in a shared workspace — see
[`BUILD_GUIDE.md`](BUILD_GUIDE.md).

## 5. Test

```
python3 tools/r1_os_selfcheck.py      # one command: build + ISO + smoke + cp4 + stress + selftest
python3 tools/r1_selftest.py          # SELFTEST only
python3 tools/r1_qemu_smoke.py        # boot smoke
```

Selfcheck writes per-step logs to `/tmp/opencode/r1_os_selfcheck_<step>.log`
and prints `OVERALL: PASS/FAIL`; exit 0 only on all steps passing. A flaky run
is often a stale QEMU holding `/tmp/opencode/r1_*.img` — `pgrep`/`pkill` the
orphan and re-run. See [`FAQ_TROUBLESHOOTING.md`](FAQ_TROUBLESHOOTING.md).

## 6. Key addresses, IDs & numbers

| Item | Value |
|---|---|
| Kernel entry point (bootsect hardcode) | `0x100030` |
| VBE LFB base (q35, mode 0x118, 1024x768x32) | `0xFD000000`, pitch 4096 |
| Legacy VGA text buffer | `0xB8000` (mirrored) |
| EUSER import ordinal (`RDebug::Printf`) | `915` |
| Console font scale / row advance | 2x, 18 px (16x16 glyph + 2 px gap) |
| Slow exec 240 | audio tone (`QE_EXEC_AUDIO_TONE`) |
| Slow exec 241 | process census (`QE_EXEC_PS`, shell `PS`) |
| Slow execs 247 / 248 | netbridge base / size (e1000 frame bridge) |
| Slow execs 249 / 250 / 251 | GUI fb base / fb size / draw |
| Slow execs 268 / 269 / 270 | SvcMgr launch / kill / ctl (KILLPROC uses 269) |
| Drives | `C:` (AHCI dev 4, part 0), `X:` (programs RAM disk, dev 8) |
| Guest IP / SLIRP gateway | `10.0.2.15` / `10.0.2.2` |

## 7. Other docs

- [`r1_boot_and_selftest.md`](r1_boot_and_selftest.md) — canonical boot/self-test recipe
- [`SHELL_REFERENCE.md`](SHELL_REFERENCE.md) — every shell command
- [`USER_APPS_CATALOG.md`](USER_APPS_CATALOG.md) — bundled programs
- [`BUILD_GUIDE.md`](BUILD_GUIDE.md) — toolchain, targets, build order
- [`FAQ_TROUBLESHOOTING.md`](FAQ_TROUBLESHOOTING.md) — common problems, real fixes
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — how the OS works
- [`GRUB_BOOT_INTERNALS.md`](GRUB_BOOT_INTERNALS.md) — El Torito → mounted `X:`
- [`NETWORKING_STACK.md`](NETWORKING_STACK.md), [`NETWORK_TESTING.md`](NETWORK_TESTING.md) — network
- [`GUI_AUDIO.md`](GUI_AUDIO.md), [`DRIVERS_HARDWARE.md`](DRIVERS_HARDWARE.md) — devices
- [`DOCS_INDEX.md`](DOCS_INDEX.md) — full document index

`kernelhwsrv/` is genuine EKA2 source, READ-ONLY; all changes live in
`qemu-port/`.
