# Testing & QA — the R1 EKA2 PC port

This document is the complete testing/QA story for the `qemu-port` tree: the
one-command verifier, the on-OS self-test, the full inventory of `r1_*` harness
scripts, the four core tests, how the QEMU harnesses stage disks and judge
results, and how to interpret (and de-false) failures. It is written against the
files as they exist in the tree; when in doubt, the code is the authority.

All commands below are run from the `qemu-port` workspace root, i.e.
`/home/yuzi/Documents/Sybian REAL/qemu-port`.

---

## 1. The one-command wrapper: `tools/r1_os_selfcheck.py`

`python3 tools/r1_os_selfcheck.py` is the top-level gate. It runs six steps, in
order, with no shell wrappers of its own:

| # | Step | Command | Wrapper timeout | Fail-fast? |
|---|------|---------|-----------------|------------|
| 1 | `build` | `make build/kernfull.elf userland` | 120 s | **yes** |
| 2 | `isogrub` | `make isogrub` (builds `build/symbian-grub.iso`) | 120 s | no |
| 3 | `qemu_smoke` | `python3 tools/r1_qemu_smoke.py` | 70 s | no |
| 4 | `cp4_ca` | `python3 tools/r1_cp4_ca_test.py` | 70 s | no |
| 5 | `cp4_stress` | `python3 tools/r1_cp4_ca_stress_test.py` | 90 s | no |
| 6 | `selftest` | `python3 tools/r1_selftest.py` | 110 s | no |

**PASS/FAIL semantics.** Each step is launched with `subprocess.Popen(...,
start_new_session=True)` so it gets its own process group. On timeout the whole
group is killed with `os.killpg(..., SIGKILL)` — this is deliberate, so no
QEMU/`timeout` grandchildren survive to contend with later steps. Each step is
printed as `[PASS] <name> (<elapsed>s): <reason>` or `[FAIL] ...`; the final
block prints `OVERALL: PASS` or `OVERALL: FAIL — N step(s) failed` plus a
`total elapsed` line. Exit status is **0 only if every step passed**.

Verdict inference differs by step type:

* `build` / `isogrub` pass on **exit code 0** only.
* The QEMU steps must both exit 0 **and** contain a PASS verdict in their
  output. `verdict()` scans for `^\s*(?:RESULT:\s*)?PASS\b` and the matching
  `FAIL` regex (multiline). A `FAIL` line wins over a `PASS` line; no PASS line
  at all is a failure ("no PASS verdict in output"). This covers both the
  `RESULT: PASS` form used by smoke/cp4/selftest and the bare `PASS: ...` line
  printed by the stress test.
* If the `build` step fails, the remaining steps are reported `[SKIP] ...
  (build failed, fail-fast)` — that is the only fail-fast step.

**Runtime.** A clean full run takes roughly **four minutes** end to end: the
ISO/build steps are tens of seconds, and each QEMU step runs a ~50–90 s inner
`timeout` plus disk staging and log polling. The per-step wrapper budgets above
(120/120/70/70/90/110 s) are upper bounds; a step that hits its wrapper timeout
is reported as a FAIL with the log tail appended to the reason.

**Per-step logs.** Full captured stdout/stderr for every step is written to
`/tmp/opencode/r1_os_selfcheck_<name>.log` (e.g.
`r1_os_selfcheck_qemu_smoke.log`). The console only shows a 15-line tail on
failure; use these files for post-mortem debugging. Note the stress test uses
PID-unique artifacts (`cp4_stress_<pid>.img` / `.log`) rather than this fixed
name — find its log via the `serial log:`/`==` lines in the wrapper log.

Quick start:

```sh
python3 tools/r1_os_selfcheck.py          # expect OVERALL: PASS, exit 0
tail -n 40 /tmp/opencode/r1_os_selfcheck_qemu_smoke.log
```

---

## 2. The on-OS self-test: `user/selftest.c` + `tools/r1_selftest.py`

This is a single E32 program that exercises core OS services *from userland*
and reports one PASS/FAIL verdict. It is distinct from the Python harness: the
C program produces the markers; the Python script stages, boots, launches and
judges them.

`tools/r1_selftest.py` stages `build/user/SELFTEST.E32` and the genuine
`EUSER.DLL` on a 32 MB FAT32 disk (`/tmp/opencode/r1_selftest.img`), boots the
kernel, and feeds the serial shell `portfilesrv` then `selftest` (the disk
auto-mounts at boot; a shell `mount` hangs on the M-03 kernel). The QEMU command
includes `-netdev user,id=n0 -device e1000,netdev=n0`; **e1000 must be present**,
because execs 247/248 (the netbridge base/size execs) are installed only when
the NIC is initialised.

**Six services** (`app_main` runs them in this exact order):

1. **euser** — EUSER.DLL ordinal 915 (`RDebug::Printf`) is actually bound. All
   markers flow through this slot, so seeing them proves the import works. The
   test checks the IAT slot holds neither 0 nor the ordinal 915 after binding.
2. **malloc** — chunk-backed user heap: two live allocations are distinct and
   writable, then freed.
3. **chunk** — `RChunk` round trip: `chunk_create("STEST", 4096, 4096)`,
   `chunk_base`/`chunk_size`, then `chunk_open("STEST")` returns the same size.
4. **file** — PortFile `file_save("STEST.TXT", ...)` + `file_open`/`file_read`/
   `file_close` byte-for-byte round trip. Requires `portfilesrv` running.
5. **ipc** — `ipc_server_create("STEST")` then `ipc_close`, non-blocking (it
   deliberately does **not** call `ipc_connect`, which would block without a
   server loop). No service manager is auto-started, so this is a create/close
   ABI check, not a client/server exchange.
6. **netbridge** — `exec2(247/248)` reports the shared base/size. Magic/version
   are stamped only after the `netbridge` shell command runs, so a mismatch is
   an informational `ST: netbridge note: ...` line. This one uses
   `result_soft()` — it prints the same `ST: netbridge OK` marker but never
   increments the failure counter. **The Python harness still looks for
   `ST: netbridge OK`, so e1000/execs 247/248 must really be present or the
   harness FAILs**, even though the C program's own verdict would stay PASS.

**Markers** checked by the harness (all must appear, and in this order):

```
ST: SELFTEST START - EKA2 PC OS core services
ST: euser OK
ST: malloc OK
ST: chunk OK
ST: file OK
ST: ipc OK
ST: netbridge OK
ST: failures=0
ST: RESULT PASS - all core services operational
```

Additional harness assertions beyond the markers: an `e32run: bind slot ... ord
915` line must exist (real PE2 binding, not just the DebugPrint fallback), and
no log line may contain `Exc ` or `PanicKernExec`. The script prints `[PASS]`/
`[FAIL]` per check, a `RESULT: PASS/FAIL`, and exits 0 only if all checks pass.
Its inner cap is `timeout 90`; the wrapper gives it 110 s.

---

## 3. Full inventory of `tools/r1_*.py`

These are the harnesses and utilities that glob out of `tools/r1_*.py`. The
`*_test.py` files are QEMU harnesses unless noted; they share the staging /
feed / marker / exception-scan pattern described in §5.

### 3.1 Infrastructure (not `*_test.py`)

| Script | One-line purpose |
|--------|------------------|
| `r1_os_selfcheck.py` | One-command wrapper: build, ISO, smoke, CP4, stress, selftest (§1). |
| `r1_selftest.py` | QEMU harness for the on-OS self-test E32 (§2). |
| `r1_decode_fault.py` | Host utility to decode a guest fault/exception dump. |

### 3.2 Core / kernel-level tests

| Script | One-line purpose |
|--------|------------------|
| `r1_qemu_smoke.py` | 11-marker boot/EUSER-load/import-bind smoke test (§4). |
| `r1_cp4_ca_test.py` | 6-marker genuine CActive Start→RunL→Stop round trip (§4). |
| `r1_cp4_ca_stress_test.py` | P-08 concurrency stress: 3 cycles × 4 clients + peer kill (§4). |
| `r1_qemu_nic_test.py` | e1000 bring-up, link up and 3/3 ARP replies (§4). |
| `r1_e32_validate_test.py` | E32 header/import validation acceptance cases. |
| `r1_qemu_audio_test.py` | AC97/audio device enumeration and mixer path. |
| `r1_qemu_display_test.py` | Bochs VBE framebuffer/display bring-up. |
| `r1_qemu_mouse_test.py` | PS/2 / USB mouse input path. |
| `r1_qemu_dns_test.py` | DNS resolver E32 against the SLIRP network. |
| `r1_usb_ehci_test.py` | USB EHCI controller enumeration. |
| `r1_usb_ohci_test.py` | USB OHCI controller enumeration. |
| `r1_virtio_blk_test.py` | VirtIO block device detection. |
| `r1_virtio_net_test.py` | VirtIO network device detection. |
| `r1_virtio_rng_test.py` | VirtIO RNG device detection. |

### 3.3 Network / service E32 tests

| Script | One-line purpose |
|--------|------------------|
| `r1_netbridge_test.py` | Kernel↔user e1000 frame bridge (execs 247/248) proof. |
| `r1_dhcpc_test.py` | DHCP client obtains a lease via the bridge. |
| `r1_dnsnet_test.py` | DNS query through the e1000 netbridge. |
| `r1_dnsres_test.py` | Standalone DNS resolver E32. |
| `r1_ping_test.py` | ICMP echo via the bridge. |
| `r1_httpget_test.py` | HTTP GET over the bridge. |
| `r1_telnet_test.py` | Telnet client session. |
| `r1_wget_test.py` | File download client. |
| `r1_wbrowser_test.py` | Web browser E32. |
| `r1_web_browser_test.py` | Alternate web browser harness. |
| `r1_ntpc_test.py` | NTP client time sync. |
| `r1_ifconfig_test.py` | Interface configuration command output. |

### 3.4 End-user application tests (one harness per E32)

| Script | App / one-line purpose |
|--------|------------------------|
| `r1_calc_test.py` | `calc` — integer expression calculator self-suite (build pattern, §7 of dev guide). |
| `r1_addrbook_test.py` | `addrbook` — address book CRUD. |
| `r1_applaunch_test.py` | `applaunch` — launching a program from a program. |
| `r1_asciiart_test.py` | `asciiart` — ASCII art rendering. |
| `r1_astro_test.py` | `astro` — astronomy calculations. |
| `r1_battle_test.py` | `battle` — battleship game. |
| `r1_bench_test.py` | `bench` — CPU/memory benchmark. |
| `r1_blackjack_test.py` | `blackjack` — card game. |
| `r1_cal_test.py` | `cal` — calendar display. |
| `r1_chess_test.py` | `chess` — chess move/engine logic. |
| `r1_clock_test.py` | `clock` — clock display. |
| `r1_codebreak_test.py` | `codebreak` — mastermind-style game. |
| `r1_connect4_test.py` | `connect4` — connect-four game. |
| `r1_currency_test.py` | `currency` — currency conversion. |
| `r1_dice_test.py` | `dice` — dice roller. |
| `r1_fcopy_test.py` | `fcopy` — file copy. |
| `r1_fgrep_test.py` | `fgrep` — text search. |
| `r1_filemgr_test.py` | `filemgr` — file manager. |
| `r1_flight_test.py` | `flight` — flight/booking sim. |
| `r1_fortune_test.py` | `fortune` — fortune cookie. |
| `r1_guishell_test.py` | `guishell` — GUI shell. |
| `r1_hangman_test.py` | `hangman` — word game. |
| `r1_hanoi_test.py` | `hanoi` — Tower of Hanoi. |
| `r1_hexdump_test.py` | `hexdump` — hex dump utility. |
| `r1_maze_test.py` | `maze` — maze generation/solve. |
| `r1_memtest_test.py` | `memtest` — user memory test. |
| `r1_mines_test.py` | `mines` — minesweeper. |
| `r1_monitor_test.py` | `monitor` — system monitor. |
| `r1_morse_test.py` | `morse` — Morse conversion. |
| `r1_movies_test.py` | `movies` — movie database. |
| `r1_music_test.py` | `music` — music/notes player. |
| `r1_notepad_test.py` | `notepad` — text editor. |
| `r1_ocean_test.py` | `ocean` — ocean sim. |
| `r1_pacman_test.py` | `pacman` — arcade game. |
| `r1_periodic_test.py` | `periodic` — periodic table. |
| `r1_primes_test.py` | `primes` — prime generation. |
| `r1_ps_test.py` | `ps` — process listing. |
| `r1_pwvault_test.py` | `pwvault` — password vault. |
| `r1_quiz_test.py` | `quiz` — trivia quiz. |
| `r1_recipes_test.py` | `recipes` — recipe browser. |
| `r1_saver_test.py` | `saver` — screen saver. |
| `r1_shot_test.py` | `shot` — shooting game. |
| `r1_snake_test.py` | `snake` — snake game. |
| `r1_snake2_test.py` | `snake2` — snake game variant. |
| `r1_solar_test.py` | `solar` — solar system display. |
| `r1_soundplr_test.py` | `soundplr` — sound player. |
| `r1_stopwatch_test.py` | `stopwatch` — timer. |
| `r1_sudoku_test.py` | `sudoku` — sudoku solver. |
| `r1_sysinfo_test.py` | `sysinfo` — system information. |
| `r1_taskmgr_test.py` | `taskmgr` — task manager. |
| `r1_tetris_test.py` | `tetris` — tetris game. |
| `r1_tictactoe_test.py` | `tictactoe` — tic-tac-toe. |
| `r1_units_test.py` | `units` — unit conversion. |
| `r1_wbrowser_test.py` | `wbrowser` — browser app. |
| `r1_wc_test.py` | `wc` — word/line count. |
| `r1_wordsearch_test.py` | `wordsearch` — word search game. |
| `r1_worldtime_test.py` | `worldtime` — world clock. |
| `r1_zodiac_test.py` | `zodiac` — zodiac signs. |

---

## 4. The four core tests in detail

**`r1_qemu_smoke.py` — 11 markers.** Stages `portfilesrv`, `EUSER.DLL`
(optionally `hello.e32`), boots the kernel with e1000 + AC97 + VBE, feeds a
newline then `portfilesrv`, and checks: export-descriptor tail
(`expDescSize=45 expDescType=2`), `E32Validate r=0`, EUSER resident,
`exports=2503`, bind slots `12348 ord 1221` and `12352 ord 1953`, 3 ordinals
bound, `ProcessCreate r=0`, `Loaded r=0`, `Resume()...`, and a clean log. The
final marker is condition-based: it passes only when **no** line contains
`Exc ` or `PanicKernExec`. An explicit `mount` is deliberately *not* sent —
`Asic::Init3` already mounts the partition, and `mount` hangs the M-03 kernel.

**`r1_cp4_ca_test.py` — 6 markers.** Stages `ca_test.e32` + `EUSER.DLL` and
checks, in order: `CActiveScheduler::Install ok`, `Start() entered`, `RunL()
reached`, `Stop() called`, `Start() returned`, `clean exit`. It also asserts
16 EUSER ordinals are bound (scheduler/active 430, 428, 426, 425, 1091, 1090,
915, 424; timer/request 887, 881, 885, 925, 928, 929, 617, 626) and checks
marker *order*, not just presence — this proves the genuine
`Start() → WaitForAnyRequest() → RunL(vptr) → Stop() → Start()` round trip. It
polls the log size until it stops growing before reading, to avoid losing the
last markers to `timeout`'s SIGTERM.

**`r1_cp4_ca_stress_test.py` — 3 cycles × 4 clients.** Uses `tools/test_cli.py`
and PID-unique disk/log names. After a baseline `segs` / `channels` count, each
cycle starts one `IPCSRV`, launches four `CPSTRESS <i>` clients (each draining
100 async requests), verifies the victim (id 2) is *in flight* (ticks but no
`done`), kills it mid-test via a peer `KILLPROC.E32` (executive 269 →
`E32Kill`, not the shell `KILL`), and requires the three survivors to emit
`CPSTRESS: done` with all 100 completions. It then kills the server, checks a
fresh client fails to connect, confirms both deaths in the `exits` ring, and
finally recovers with a fresh server + client. End checks: no `FAULT:`/`Exc `/`
PanicKernExec`, no failure markers, and channel/segment counts back at baseline.
Prints a bare `PASS: 3 cycles x 4 clients, ...` line.

**`r1_qemu_nic_test.py` — 3/3 ARP.** No disk files staged; with
`-device e1000,netdev=n0` it checks boot markers (`e1000: BAR0=`, `MAC=`,
`link=1`), then drives `nic arp 3` over serial and requires
`nic: e1000 MAC=`, `link=1`, three `nic arp: try N tx=0 (OK)` lines, `RX reply`,
and `nic arp: RESULT PASS (3/3 ARP replies)`. This proves sustained TX/RX, not
a single packet.

---

## 5. How the QEMU harnesses work

Every `r1_*` harness follows the same recipe (the app tests are near-copies of
`r1_calc_test.py`):

1. **Check tools** — `qemu-system-i386`, `mkfs.fat`, `mcopy`, `mdir`, `timeout`
   must be on `PATH`.
2. **FAT disk staging** — create a 32 MB raw disk, create a standalone FAT32
   image sized to the partition (`mkfs.fat -F 32`), `mcopy` the E32(s),
   `EUSER.DLL` and support binaries into it, then embed that image at byte
   offset `2048 * 512 = 1048576` (LBA 2048).
3. **MBR** — write a 16-byte partition entry at offset 446: bootable flag
   `0x80`, type `0x0C` (FAT32 LBA), start LBA 2048, 16384 sectors; end with the
   `0x55 0xAA` signature at bytes 510–511. Verify with
   `mdir -i <disk>@@1048576 ::` (normalise whitespace/dots so 8.3 or LFN names
   both match).
4. **Boot + timed serial feed** — QEMU is launched under `timeout` with
   `-M q35 -m 128 -display none -serial stdio -kernel build/kernfull.elf`, an
   AHCI disk (`ich9-ahci` + `ide-hd`), `-monitor none -no-reboot`. Serial input
   is *not* piped blindly: a bash `{ sleep; printf 'cmd\n'; sleep; } | qemu`
   feeder gives the guest shell time to initialise (e.g. 1.5 s, newline, 3 s,
   `portfilesrv`, 5–10 s). Exit code 124 from `timeout` is expected.
5. **Marker checking** — `MARKERS` is an ordered list of literal strings; each
   is `text.find()`-ed and presence plus strict ordering is asserted. The
   stress test is more elaborate, polling byte ranges after a command offset
   (cycle-aware) instead of using fixed sleeps.
6. **Exception scanning** — `EXC_KEYS = ("Exc ", "PanicKernExec")` (plus
   `FAULT:` in the stress test). Any matching line fails the test even if all
   shell-side markers printed, avoiding false passes when a fixture thread dies
   right after startup.
7. **Verdict** — print `[PASS]`/`[FAIL]` per check, a `RESULT: PASS/FAIL`
   (bare `PASS:` in the stress test), and exit 0/1. `r1_os_selfcheck.py` keys off
   this final line.

Notes specific to the disk workflow: the partition auto-mounts at boot via
`Asic::Init3`; sending `mount` hangs the shell on the M-03
per-process-page-table kernel. Some harnesses poll the log file size until it
stops growing (or sleep ~1 s after the inner timeout) because `timeout`'s
SIGTERM can reap QEMU before redirected serial stdio is fully flushed.

---

## 6. Timeouts, orphaned QEMUs, and cleanup

* **Inner vs outer timeout.** The QEMU processes use their own `timeout` values
  (smoke 50 s, cp4 60 s, calc/app 60 s, selftest 90 s, nic 70 s, network tests
  80–110 s). A *full* harness run includes FAT image construction, boot, and
  sometimes cold-boot retries — give network/HTTP-class harnesses a **~150 s
  outer wall-clock budget** even though their inner cap is 90 s, and
  `r1_netbridge_test.py` ~160 s because it retries a cold boot twice. The
  selfcheck wrapper encodes its own per-step budgets (§1).
* **Orphan contention.** If a harness is killed externally (or hits a CI
  wrapper timeout before its own `timeout` fires), a `qemu-system-i386` can be
  left alive holding disk images under `/tmp/opencode/r1_*.img`. Because most
  harnesses unlink and recreate their images at startup, a second run normally
  rebuilds them — but a live QEMU with `snapshot=on` maps the same backing
  file and can still corrupt reads, and two QEMUs fighting over the same file
  produce missing markers and apparent random failures. The selfcheck avoids
  this by killing the whole process group on timeout; the stress test uses
  PID-unique names.
* **Cleanup recipe** (safe when no test you care about is running):

  ```sh
  pgrep -af qemu-system-i386
  pkill -9 -f qemu-system-i386
  rm -f /tmp/opencode/r1_*.img /tmp/opencode/r1_*.log \
        /tmp/opencode/cp4_stress_*.img /tmp/opencode/cp4_stress_*.log
  ```

  Do this before re-running after an aborted or externally-killed test.

---

## 7. Interpreting results and common false failures

* **Exit 124 everywhere** is normal: the harness expects `timeout` to kill
  QEMU. Only the markers matter. A missing log or empty log at all is the real
  problem.
* **No markers at all / missing late markers** — usually flush loss, not a
  guest bug. Check that the harness polls log size or sleeps after the inner
  timeout; inspect `/tmp/opencode/<name>.log` for the raw serial output.
* **`ST: netbridge FAIL` (or no `ST: netbridge OK`)** — QEMU was launched
  without `-device e1000,netdev=n0`, or execs 247/248 are absent. Re-run with
  e1000 present (the selftest harness already does). The `netbridge` shell
  command only stamps magic/version; a "note" line is informational.
* **`ST: file FAIL`** — `portfilesrv` was not started or was staged under a
  name the guest FAT driver cannot match (use 8.3 short names). The feeder must
  run `portfilesrv` before `selftest`.
* **`ST: euser FAIL` / no `bind slot ... ord 915`** — the E32 was packed without
  `--euser-import 915`, so `EUSER.DLL` was never loaded and the marker fell back
  to `print()` only. Rebuild the E32 (see the developer guide, §2).
* **A PASS line plus a FAIL line** — the selfcheck wrapper treats this as FAIL
  (FAIL wins in `verdict()`). Look for the stray exception or a fixture thread
  that died after printing its markers.
* **Flaky CP4 stress runs** — check `ps` baseline counts and whether the victim
  actually reached `tick` before the kill; a too-fast kill ("finished before
  kill") or a too-slow one (victim already done) are timing artifacts. Also look
  for orphaned QEMUs (§6).
* **Stale binary** — the harness boots `build/kernfull.elf` and stages
  `build/user/*.E32`. If you changed sources without `make userland`, you are
  testing the old image. `r1_cp4_ca_stress_test.py` rebuilds kernel+userland
  itself; most others do not.
* **Never run `make clean` while other agents/builds are active** — it removes
  the shared `build/` artifacts every harness depends on and will manufacture
  failures across the whole suite.

For deeper background on the boot path, see `docs/r1_boot_and_selftest.md` and
`docs/ARCHITECTURE.md`; for network-specific harnesses see
`docs/NETWORK_TESTING.md`.
