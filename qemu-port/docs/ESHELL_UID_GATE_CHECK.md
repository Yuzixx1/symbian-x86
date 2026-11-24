# ESHELL UID3 gate check (read-only verification)

Verification only. No source/test/build edits. `kernelhwsrv/` untouched. No
commit/push. One report file created (this file). Snapshot: 2026-11-24
03:16–03:21 EET.

## Question

Does changing ESHELL UID3 from `0x101F7AE7` to `0x10000111` (the PS-fix build
change, made to avoid the EFILE single-owner guard) break the boot chain or the
ESTART/ESHELL retire-gate detection?

## Tree state / mtimes

| Artifact | At my test start | Current (03:21) |
|---|---|---|
| `build/kernfull.elf` | 03:15:15, 1025004 B | unchanged (stable before/after both runs) |
| `build/symbian-grub.iso` | 03:15:27, 75882496 B | unchanged |
| `genuine/build/etshell_.E32` | 03:11:16, 85792 B, **uid3=0x10000111**, chk=0x663961d6 | **03:21:00, uid3=0x101F7AE7** (patch reverted) |
| `genuine/build/estart_.E32` | 01:21:00, 25848 B, uid3=0x100000bf | **03:21:00, uid3=0x10272c04** |

- `qemu-slot.lock`: `.sybian/qemu-slot.lock` exists but is empty/stale
  (Nov 23 22:15). No `make`/`isogrub`/`genuine_build` process was running.
- Concurrent sibling activity was present: `r1_efile_acceptance.py` +
  `r1_genuine_retire_test.py` (03:16), and a further owner retire re-run
  (`estart_uid_retire_test.log`, 03:21). I did not take over or interfere.
- UID checksum of the patched build verified with `mke32.uid_checksum`:
  `(0x1000007a, 0x1000008c, 0x10000111) -> 0x663961d6` = stored value. Valid.

## Commands run

```
python3 tools/r1_genuine_retire_test.py --log-dir /home/yuzi/.cache/sybian-r1/retire
python3 tools/r1_genuine_fullchain_test.py --log-dir /home/yuzi/.cache/sybian-r1
```
Plus read-only greps/header reads of the gate, fullchain test, build script and
QEMU logs. No build/rebuild command was run by me.

## Does the gate key on UID?

No. In `tools/r1_genuine_retire_test.py` the only UID check (`uid_checksum`,
lines ~642–646) applies to **EUSER.DLL**. The `ESTART/ESHELL` gate (lines
~589–620) keys purely on boot strings: `ESTART_BOOT_MARKERS`
(`ESTART: genuine E32 image loaded`, `ESHELL: genuine E32 image loaded`),
`ESTART_MARKERS` / `ESHELL_MARKERS` and absence of faults. ESHELL's UID3 value
is never compared by the gate. The fullchain test likewise keys on banner
`ESHELL 0.01(4011)`, `GenuineBoot: ESHELL.E32 r=0` and `C:/>`.

## Results (patched-snapshot build, ESHELL uid3=0x10000111)

Retire test — per subsystem:

- `efsrv` **FAIL** — only `F32CLIENT: IPC round trip OK` missing; log shows
  `F32CLIENT: SessionPath r=-15` (the in-flight "-15 fix"; ESHELL is not even
  staged on this disk, so unrelated to the UID change). port-shim count 0,
  no faults.
- `ewsrv` **PASS**
- `estart` **PASS** — `shell boot markers: ['ESTART', 'ESHELL']`, no faults.
  This is the gate the question targets: it passes with the new UID.
- `euser` **PASS**
- overall `RESULT: FAIL` solely because of `efsrv`.

Fullchain test — 12/14 PASS. Key rows:

- `ESHELL genuine ESHELL banner` **PASS** (byte 58145)
- `ESHELL.E32 launched` **PASS** (`GenuineBoot: ESHELL.E32 r=0 pid=15`, byte 58245)
- `ESHELL C:/> prompt` **PASS** (byte 58478)
- `no fault`, `fallback not used`, `screendump` **PASS**
- `ESTART.E32 launched` **FAIL**, `genuine file server up` **FAIL**

Both fullchain FAILs are explained and not UID-caused:

1. `e32run: single-owner: ESTART.E32 uid3=100000bf already running or
   !FileServer published; launch refused` -> `GenuineBoot: ESTART.E32 r=-11`.
   ESTART deliberately keeps the EFILE UID `0x100000bf` (build-script comment),
   so with EFILE on the same disk the guard-only model refuses a second owner.
   This predates and is independent of the ESHELL UID patch — the patch's whole
   purpose was to *stop* ESHELL colliding with that UID, and it worked: ESHELL
   loaded with `r=0`. The retire ESTART gate stages ESTART without EFILE, hence
   it passes there.
2. The fullchain marker asks for `GenuineBoot: genuine file server up,
   svcmgr fallback skipped` while the kernel logs `GenuineBoot: file server up,
   svcmgr fallback skipped` — a stale expected-string, not a boot failure.

## Verdict

The `0x10000111` ESHELL UID3 change did **not** break boot or ESTART/ESHELL
gate detection on the build tested: retire ESTART/ESHELL = PASS with `ESTART`
and `ESHELL` markers, and fullchain reached the genuine ESHELL banner,
`r=0` launch and `C:/>` prompt. The observed FAILs are the pre-existing EFILE
single-owner refusal of ESTART, a stale fullchain marker string, and the
in-flight `SessionPath -15` issue — none caused by the ESHELL UID value.

**However, the patch is no longer in the tree.** A concurrent rebuild at
03:21:00 reverted `etshell_.E32` to `0x101F7AE7`, changed `estart_.E32` to
`0x10272c04`, and removed the `0x10000111` patch block from
`genuine/estart_eshell/build_estart_eshell.py` (now only genuine UIDs at
lines 100–101). All results above therefore apply to the 03:11 patched
snapshot; they are evidence only and must be re-run if the patch is restored.

## Caveats

- Kernel binary was byte-for-byte stable before and after both of my runs;
  the artifact churn was in `genuine/build/*.E32` after them.
- Another agent's retire re-run on the reverted build is in flight; I did not
  wait for or consume its result, and did not launch QEMU against the changed
  artifacts.
- No formal `r1_genuine_eshell_test.py` PS re-run was performed here.
