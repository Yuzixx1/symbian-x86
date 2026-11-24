# Full genuine boot-chain verification (ONE boot, all four components)

Date: 2026-11-24. Workspace: `qemu-port/`. Test:
`tools/r1_genuine_fullchain_test.py` (new file; no existing tests, build scripts
or source modified). `kernelhwsrv/` untouched; nothing committed/pushed;
`make clean` not run; **no `make isogrub` / rebuild** — the current staged ISO
was booted as-is.

## Why

`docs/BOOT_CHAIN_VERIFICATION.md` §4 finding 3: earlier gates prove EFILE,
ESTART, EWSRV and ESHELL individually, on separate staged disks, so the full
in-order chain in one boot was never asserted. This test stages all four E32s
plus genuine `EUSER.DLL` and `EFAT32.FSY` on a single FAT32 disk attached as
`C:`, boots the current GRUB ISO with that disk, and checks the launch markers
in strict sequential order from byte 0 of the serial log (components are
auto-launched by `GenuineBootLaunch()` during Init3, before the shell prompt —
they are never re-launched from the shell, which would spawn dying duplicates).

## Commands

```
cd qemu-port
python3 -m py_compile tools/r1_genuine_fullchain_test.py
python3 tools/r1_genuine_fullchain_test.py --dry-run       # inputs + expected marker list
python3 tools/r1_genuine_fullchain_test.py --log-dir /tmp/opencode
# exit 0 = PASS
```

## Real run — result: **PASS (14/14 rows)**

Staged disk `/tmp/opencode/r1_fullchain.img` (6 files); booted
`build/symbian-grub.iso` (75,880,448 B; sha256 prefix `5c397be4ff191626`) with
`-cdrom ... -boot d`, AHCI disk as `C:`, QMP screendump. At test time the ISO
was mtime 02:13 while `kernfull.elf`/`EWSRV.E32` were being rebuilt by other
agents (02:18); the ISO's embedded kernel was not re-verified here, so this is
a statement about the **staged ISO**, not about any concurrent rebuild.

Observed marker order (byte offsets in `/tmp/opencode/r1_fullchain.log`):

| # | Component | Marker (verbatim) | Byte | Verdict |
|---|---|---|---|---|
| 1 | kernel | `KERN spawning genuine boot DThread` | 6786 | PASS |
| 2 | kernel | `GenuineBoot: probing boot volume` | 7161 | PASS |
| 3 | EUSER | `e32run: EUSER.DLL resident` | 9168 | PASS |
| 4 | EUSER | `exports=2503` | 9214 | PASS |
| 5 | EFILE | `GenuineBoot: EFILE.E32 r=0` | 32918 | PASS |
| 6 | ESTART | `GenuineBoot: ESTART.E32 r=0` | 39390 | PASS |
| 7 | EWSRV | `GenuineBoot: EWSRV.E32 r=0` | 52326 | PASS |
| 8 | ESHELL | `ESHELL 0.01(4011)` banner | 63344 | PASS |
| 9 | ESHELL | `GenuineBoot: ESHELL.E32 r=0` | 63460 | PASS |
| 10 | kernel | `GenuineBoot: genuine file server up, svcmgr fallback skipped` | 63498 | PASS |
| 11 | ESHELL | console `C:/>` prompt | 63809 | PASS |

Additional assertions: `Exc `/`PanicKernExec`/`FAULT:` count = **0**; no
`SvcMgr: SVCMGR.E32 not found` / `SvcMgr: boot launch r=0` / `EFILE: port-shim`
fallback markers; QMP screendump `/tmp/opencode/r1_fullchain.png` shows the
real console banner and `C:/>` prompt (evidence retained).

`RESULT: PASS (14/14 rows PASS)`, exit 0.

## Caveats / interpretation

* The kernel prints `genuine file server up, svcmgr fallback skipped` **after**
  the whole boot list (it is decided once the loop finishes), and a component's
  own banner appears **before** its `GenuineBoot: <name> r=0` line. The table
  above is the real code order, not the conceptual EFILE → ESTART → EWSRV →
  ESHELL list order; all four `r=0` lines are in genuine order.
* The boot console prompt is `C:/>` (forward slash, kernel console), not the
  genuine shell's literal `C:\>` which appears only when ESHELL owns the
  console. The test accepts the console form; the ESHELL banner at byte 63344
  is the genuine-shell evidence. The genuine `C:\>` form can appear on other
  disks (e.g. `/tmp/opencode/eshell_dbg.log`) when ESHELL elides the hand-written
  shell.
* This proves launch order and fault-free boot, not that each server's IPC is
  functional; functional proofs remain the individual
  `r1_genuine_{efsrv,ewsrv,eshell}_test.py` gates. EFSRV retire gate is still
  SKIP because `EFSRV.DLL` is not built as an E32, but genuine `EFILE.E32` is
  staged here and the fallback is reported absent.
* An earlier run of this same script failed only the prompt row because it
  searched for `C:\>`; the marker was corrected to the console's `C:/>` and the
  rerun above passed. The first run's log (`/tmp/opencode/r1_fullchain.log`
  before the rerun) also contained all four `r=0` lines and zero faults.
* Concurrent agents were rebuilding `kernfull.elf` and `EWSRV.E32`; no rebuild
  or ISO re-staging was done by this test. Re-run it against a freshly built ISO
  to confirm the chain on a newer image.

## Re-verification 2026-11-24 (03:0x–03:14 EET) — **FAIL 11/14**, staged tree in flux

Commands: `python3 tools/r1_genuine_fullchain_test.py --log-dir /tmp/opencode`
(full run, exit 1) and `python3 tools/r1_efile_acceptance.py --dry-run` (exit 0,
tool present, no verdict issued). The first full run hit my 600 s outer cap
(`timeout 600`, exit 124) while boot stalled under 4–5 concurrent agents'
QEMU jobs; a manual ISO boot and a later clean tool rerun produced the data
below. Raw output: `/tmp/opencode/fullchain_tool_rerun.txt`, log
`/tmp/opencode/r1_fullchain.log`, screendump `/tmp/opencode/r1_fullchain.png`
(2440 B, present).

Artifact mtimes are a **moving target**, observed during this check:
`kernfull.elf` 02:55:51 → 03:06:11 → 03:11:29; `symbian-grub.iso`
02:56:04 → 03:06:24. No `make`/`genuine_build`/`isogrub` process was alive at
any sample, and ISO/ELF were out of sync at the final sample (ELF newer than
ISO), so the booted ISO does not contain the latest ELF. Results are a snapshot,
not a verdict on a settled build.

Marker order in the authoritative rerun (byte offsets, `r1_fullchain.log`):
`KERN spawning genuine boot DThread` 6786 → `GenuineBoot: probing boot volume`
7161 → `e32run: EUSER.DLL resident` 9168 → `exports=2503` 9214 → EFILE
`r=0` 32738 → **ESTART `r=-11` 34230** → EWSRV `r=0` 47167 → ESHELL banner
58145 → `Exc 00 EFLAGS=00051213` 58226 → ESHELL `r=0` 58299 →
`file server up` 58350 → `C:/>` 58808. EUSER, EFILE, EWSRV and ESHELL launch
in genuine order; ESTART does not.

Deviations vs the expected 14/14:
* **ESTART FAIL** — `single-owner: ESTART.E32 uid3=100000bf already running or
  !FileServer published; launch refused` ⇒ `GenuineBoot: ESTART.E32 r=-11`.
  The same guard also refused ESTART and ESHELL in the earlier manual boot
  (`r=-11`), so this outcome is timing-sensitive; ESHELL succeeded on rerun.
* **"genuine file server up" marker FAIL** — kernel now emits
  `GenuineBoot: file server up, svcmgr fallback skipped` (word "genuine"
  dropped), so the test's literal marker no longer matches. No
  svcmgr/portfilesrv/port-shim fallback markers present (`0` each).
* **faults row FAIL** — one `Exc 00 EFLAGS=00051213 FAR=00000000 ErrCode=00000000`
  line at byte 58226, inside ESHELL startup; `#PF`, `FAULT:`, `PanicKernExec`
  are all `0`.
* PASS rows: boot anchors (2), EUSER (2), EFILE, EWSRV, ESHELL banner + `r=0`,
  no-fallback, screendump, `C:/>` prompt = 11/14.

Caveats: the new AGENTS.md EFILE single-owner decision (guard-only, UID3
`0x100000bf`, no trampoline) explains the guard, but the guard is also firing
on ESTART/ESHELL, which share that UID3 in the log — whether that is intended
is for the kernel owner to decide. This test proves launch order/fault-freeness
only; IPC functionality is **not** established here, and the efile-acceptance
`--dry-run` issued no IPC verdict. Rerun once the rebuild settles and the ISO
is re-staged from the current ELF before treating this as a regression.

## Re-verification 2026-11-24 03:25 EET — **PASS 14/14** (post-UID-fix staged build)

Command: `cd qemu-port && python3 tools/r1_genuine_fullchain_test.py --log-dir /home/yuzi/.cache/sybian-r1` (exit 0).
Harness-only fix: the CHAIN marker `b"GenuineBoot: genuine file server up, svcmgr fallback skipped"`
was corrected to the kernel's actual `b"GenuineBoot: file server up, svcmgr fallback skipped"`
(matches `tools/r1_genuine_retire_test.py:77`). No assertion was weakened: 11 CHAIN
markers + faults + fallback + screendump = 14 rows, all hard requirements. Only
`tools/r1_genuine_fullchain_test.py` was edited; `kernelhwsrv/` untouched, no
commit/push, no `make clean`, no rebuild.

Preconditions checked (no race): no `make`/`genuine_build`/`isogrub`/`mkisofs`
process alive; `.sybian/qemu-slot.lock` empty and unheld (`state.json` qemu_lock
=None, mtime 2026-11-23 22:15); one concurrent smoke-test QEMU had exited before
the boot. Artifact mtimes: `symbian-grub.iso` 03:15:27 (75,882,496 B),
`kernfull.elf` 03:15:15 (ELF older than ISO — the booted ISO contains the current
kernel); post-UID-fix `genuine/build/estart_.E32` 03:21:00 (25,848 B) and
`etshell_.E32` 03:21:00 (85,792 B) — the test stages these fresh onto the disk,
so the caller UID fix (estart_ 0x10272C04, etshell_ 0x101F7AE7) is in effect.

Marker order (byte offsets in `/home/yuzi/.cache/sybian-r1/r1_fullchain.log`):
`KERN spawning genuine boot DThread` 6786 → `GenuineBoot: probing boot volume`
7161 → `e32run: EUSER.DLL resident` 9168 → `exports=2503` 9214 → EFILE `r=0`
32958 → **ESTART `r=0` pid=13 39430** → EWSRV `r=0` 52366 → ESHELL banner
`ESHELL 0.01(4011)` 63344 → ESHELL `r=0` pid=17 63444 → `GenuineBoot: file
server up, svcmgr fallback skipped` 63482 → `C:/>` prompt 63677. All four
`GenuineBoot: <name>.E32 r=0` lines present, in genuine order; ESTART refusal
`uid3=100000bf ... r=-11` from the 03:0x snapshot is gone.

`RESULT: PASS (14/14 rows PASS)`, exit 0. Fault counts in the log: `Exc `=0,
`PanicKernExec`=0, `FAULT:`=0, `#PF`=0, `EFILE: port-shim`=0; no svcmgr/
portfilesrv fallback markers. Screendump `/home/yuzi/.cache/sybian-r1/r1_fullchain.png`
PASS. Raw output retained at `/home/yuzi/.cache/sybian-r1/fullchain_rerun_0324.txt`.

Caveats: same code-order note as above (kernel prints `file server up` after the
boot list; a component's banner precedes its `r=0` line). This asserts launch
order and fault-freeness only, not per-server IPC functionality. The staged ISO
was booted as-is (no `make isogrub`); the E32 images under test came from the
current `genuine/build/` via the test's own staging. ESTART/ESHELL success here
is on this settled, post-fix snapshot; a later rebuild that reintroduces the
shared UID3 could restore the guard refusal.
