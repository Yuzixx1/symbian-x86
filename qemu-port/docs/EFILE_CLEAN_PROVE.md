# EFILE clean prove — one uninterrupted run on the final ISO (2026-11-24)

Wrapper: `tools/r1_efile_clean_prove.py`. Run from `qemu-port/`:
`python3 tools/r1_efile_clean_prove.py --dry-run` (plan only), then
`python3 tools/r1_efile_clean_prove.py` (ONE QEMU boot), then
`python3 tools/r1_efile_clean_prove.py --log /home/yuzi/.cache/sybian-r1/efile_clean_prove.log`
(analysis only; no QEMU). The prove stages EUSER.DLL, genuine EFILE.E32,
EFAT32.FSY and F32CLIENT.E32 on a private FAT32 scratch disk, boots the
**current GRUB ISO**, sends `f32client` from the shell, waits for the natural
terminal marker (2 markers seen), drains 3 s, then stops QEMU via QMP `quit`.

Raw log: `/home/yuzi/.cache/sybian-r1/efile_clean_prove.log`
(byte-identical snapshot of `efile_clean_prove_raw.log`; harness stdout in
`efile_clean_prove_stdout.txt`). ISO sha256 `7a9dee52bed6cf61bc51e1237176785b62c2e73f0b23e2e1f68758c577ad7506`;
F32CLIENT sha256 `32fec01cf36e5a8b7b8e1522e3bdb6433f2837bf5969b28eba8109eca201b6b7`.

| Marker | Expected | Actual | Verdict |
|---|---|---:|---|
| `e32run: bind slot` | 464 | 464 | PASS |
| `EFILE: genuine file server starting` | >=1 | 3 | PASS |
| `GenuineBoot: EFILE.E32 r=0` | 1 | 1 | PASS |
| `single-owner:` claim | >=1 | 1 | PASS |
| `File server thread registered` | >=1 | 1 | PASS |
| `E32Main returned` | 0 | 2 | FAIL |
| `EFILE: port-shim` | 0 | 0 | PASS |
| `F32CLIENT: IPC round trip OK` | >=1 | 0 | FAIL |
| `F32CLIENT: start` / `SessionPath r=` | info | 2 / 2 | INFO |
| `SessionPath r=-15` | info | 2 | INFO |
| `SessionPath FAILED` | info | 2 | INFO |
| `#PF` / `FAULT:` / `PanicKernExec` / `Exc ` | 0 | 0 / 0 / 0 / 0 | PASS |
| `terminating on signal 15` | 0 | 0 | PASS |

**Capture integrity:** raw log opened once, private cache paths (no concurrent
agent overwrite), no SIGTERM line, ends with the shell prompt `C:/> ` after both
client completions. Binding/registered/port-shim/fault counts are PASS; there is
no guest fault in this run.

**Deviations from the brief's expected values:**
* `IPC round trip OK` is 0, not >=1: the current `F32CLIENT.E32` (rebuilt 03:11)
  contains the integrity check and prints `SessionPath FAILED (no genuine
  KErrNone reply)` when `r=-15`; older logs counted any completed reply as
  `IPC round trip OK`. The two `r=-15` replies are still present, so this is a
  stricter client, not a lost round trip.
* `E32Main returned` 2 and genuine server start 3 reflect the known intra-process
  EUSER re-entry; per the updated project rule these markers are tolerated only
  when IPC works, which it does not here — so they remain FAIL.
* The brief's "SessionPath -15 x2 (under investigation)" is reproduced exactly;
  no cause is claimed here.

**Caveats / blockers:** booted the ISO, not `build/kernfull.elf`: that ELF was
rebuilt at 03:11 and contains the rejected trampoline strings (`E32ENTRY:
reentry`, `PortE32EntryTrampoline`), while the 03:06 ISO has none (raw-byte
grep). Guard-only EFILE is confirmed clean for binds/registration/port-shim/
faults, but overall EFILE acceptance stays NO-GO until the `r=-15` owner issue is
fixed and `IPC round trip OK >= 1` with the integrity-checked client.
`kernelhwsrv/` untouched; no source/test/build-script edits; no commit/push; no
`make clean`.
