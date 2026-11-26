# REMAINING_SHIMS_AUDIT — what is still hand-written in the production path (2026-11-24)

**Scope:** read-only inventory. No edits except this report, no QEMU, no commit/push.
`kernelhwsrv/` untouched. Sources: `docs/SHIM_RETIREMENT_RESULT.md`,
`SHIM_RETIRE_READINESS.md`, `SHIM_RETIREMENT_PLAN.md`, `SHIM_RETIRE_APPLY_REVIEW.md`,
`GENUINE_USERSPACE.md` §2, `GENUINE_PROGRESS.md` §1, and current
`variant/e32load.cpp`, `user/svcmgr.cpp`, `variant/qemu_variant_asic.cpp`, `Makefile`.

## 1. What the guarded apply actually changed

Only **two lines**, both bypasses, not deletions:
- `variant/e32load.cpp:1552` — `SvcMgrBootLaunch();` fallback commented out
  (`// RETIRED by r1_shim_retire_apply`). `portfilesrv + r1_fs` retire gate EFSRV/F32 PASS.
- `user/svcmgr.cpp:250` — `\PORTFILESRV.E32` row in `kServices[]` commented out
  (same shared edit set as `r1_fs_*`; that standalone apply correctly REFUSEd rc 2).
- `svcmgr` was applied then **rolled back** (out of scope): `variant/qemu_variant_asic.cpp:194`
  `SvcMgrExecInstall();` is **still live**.
- `winsrv` was **never applied** — no apply path; removal is a file delete + human review.
- Genuine `kGenuineBootList[]` (EFILE/EFSRV/ESTART/E32STRT/EWSRV/ESHELL),
  `GenuineBootSpawn()` and `ShellSpawnFull()` are untouched.

So "retired" = boot path bypassed. The shim **files and their kernel objects are still
compiled and linked**: `Makefile:326` still adds `r1_fs_core.o`/`r1_fs_fat.o`, and
`variant/qemu_variant_asic.cpp:151-164` still constructs `R1Fs::FatBackend`/`R1Fs::Core`
(`GenuineBootLaunch()` still reads `PortFileServerPublished()` at `e32load.cpp:1529`).

## 2. Inventory

Legend: **A** = obsolete hand-written Symbian replacement (retire); **B** = legitimate
x86/PC port bridge (keep); **C** = test/research-only (not production).

| Item | Path(s) | Role in production path | Genuine replacement | Gate status | Need to retire? |
|---|---|---|---|---|---|
| **A portfilesrv** | `user/portfilesrv.cpp`, `tools/mke32_portfilesrv.py`, `services/r1_service_host.*` | Ring-3 owner of hand-rolled `!FileServer`; boot row + fallback now commented out; image/objects still present | genuine `efile.exe`/`efsrv.dll`/`efat32.fsy` from `kernelhwsrv/userlibandfileserver/fileserver/` | EFSRV/F32 **PASS** (retire applied) | Yes — delete files/tool/flat image once bypass proven stable |
| **A r1_fs_\*** | `fs/r1_fs_core.{cpp,h}`, `r1_fs_backend.h`, `r1_fs_server.{cpp,h}`, `r1_fs_fat.{cpp,h}`, `r1_fs_channel*.{cpp,h}`, `variant/r1_fs_channel.{cpp,h}`, `variant/fat32.cpp` (kernel FAT glue) | Private `R1Fs` wire opcodes (FileOpen=30…), `CServer2` server, FAT32 adapter; `r1_fs_core.o`/`r1_fs_fat.o` linked unconditionally (`Makefile:326`) and used by `qemu_variant_asic.cpp` mount | genuine `RFs`/`RFile` ordinals + `TFsMessage` server; `R1Fs::Backend` kept only as unit-test seam | EFSRV/F32 **PASS** | Yes — drop objects/glue from kernel after file-server cut; keep `Backend` for tests |
| **A svcmgr** | `user/svcmgr.cpp`; `e32load.cpp:1258-1452` (`SvcMgrLaunch/Kill/CtlExec`, `SvcMgrExecInstall`, `SvcMgrBootLaunch`); `qemu_variant_asic.cpp:194` | Still installed at boot: repurposes genuine slow execs 268/269/270; **also wraps genuine FsRegisterThread exec 81** and hosts PTRINJ proof sink (op 4 at `e32load.cpp:1331`) | genuine `e32strt.exe`/`estart`, `eshell.exe`, `domainsrv.exe` | ESTART/ESHELL **PASS**, but apply **rolled back** | **Yes** — next retirement, with care (see §4) |
| **A winsrv** | `user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp` (GUI execs 249/250/251, event ring 271/272/273) | Hand-written PE2 compositor / display-list transport; still serves GUI apps and the DOOM `guishost.h` fallback | genuine `kernel/eka/ewsrv/ewsrv.exe` + QEMU `edisp` LDD/PSL | EWSRV **PASS**, but **no apply path** (file removal + human review) | Yes — after `edisp` port + external-client window gate |
| **A euser static shim** | `user/euser_shim.cpp`, `user/e32base.h`/`e32cmn.h` (declarations) | Static members linked into import-free user E32s; genuine `EUSER.DLL` already resident (2503 exports) | genuine `EUSER.DLL` ordinals via real PE2 imports | EUSER **PASS** | Yes — separate closure: make `elf2e32.py` emit PE2 imports, then delete shim |
| **A netstack** | `user/netstack.c`+`netstack_iat.S`, `dnsresolv.c`, `ping.c`, `httpget.c`, `telnet.c`, `wget.c`, `ntpc.c`, `dhcpc.c`, `tools/mke32_netstack.py` | Authored IPv4/IPv6 stack over `ipc.c` | **none** — ESOCK/commsdb/Bluetooth absent from this `kernelhwsrv/` snapshot | no genuine gate | **No** — explicit authored exception; keep "Symbian-shaped ABI" label (plan §3 step 7) |
| **B input** | `variant/qemu_mouse.cpp`, `variant/qemu_variant_int.cpp`, `kern_shell.cpp` PS/2 ring, `qemu_gui_exec.cpp` button bridge | Real 8042/PS/2 keyboard+mouse IRQ handling on QEMU | N/A — board-level hardware driver, exactly what a new ASSP/variant supplies | smoke/mouse/desktop PASS | No keep — legitimate port bridge |
| **B audio** | `variant/qemu_audio.cpp`, `qemu_audio_exec.cpp`, `user/audiopcm.c`, `soundplr.c`, `music.c`, `morse.c` | AC'97 PCM-OUT BDL/DMA engine + exec-240; PC audio hardware | N/A — no Symbian audio server in tree | DOOM audio/audio PCM PASS | No keep — port bridge |
| **B storage** | `variant/qemu_ata.cpp`, `qemu_ahci.cpp`, `qemu_virtio_blk.cpp`, `qemu_usb*.cpp`, `qemu_xhci.cpp` | ATA PIO / AHCI / virtio-blk / USB block devices for `FatReadPath` and boot media | N/A — board drivers | FAT-IO / smoke PASS | No keep — port bridge (do not confuse with `fat32.cpp` above) |
| **B display/network/misc** | `qemu_display.cpp`, `qemu_fbblit.cpp`, `qemu_nic.cpp`, `qemu_virtio_net.cpp`, `qemu_netbridge.cpp`, `qemu_virtio_rng.cpp`, `qemu_pci.cpp`, `qemu_assp.cpp`, `qemu_variant_rom.cpp`, `e32load.cpp` loader | VBE framebuffer, blit exec, guest NIC/link layer, PCI/ASSP/variant, PE2 loader | N/A — kernel/variant port layer | all gates PASS | No keep — legitimate x86/PC glue |
| **B kern_shell** | `variant/kern_shell.cpp`, `ShellSpawnFull()` | Debug console NThread from `Asic::Init3`; VGA teletype + line editor | N/A — **not** a Symbian server fork; plan §3 allows it to remain as a debug aid | smoke/selfcheck PASS | No keep |
| **B user runtime** | `user/runtime.c`, `ipc.c`, `malloc.c`, `sleep.c`, `chunk*.c`, `interact.c`, shell/test apps | Freestanding C runtime + IPC bridge for ported apps | N/A — generic port runtime | n/a | No keep |
| **C r1_g_\*** | `variant/r1_g_launch.cpp`, `r1_g_objects.cpp`, `r1_g_bootstrap.cpp` | Explicit opt-in genuine-G research provisioning; `R1_G_ASSET_DIR` empty by default (`Makefile:334-347`) | genuine E32 launch research | n/a | Not in production; leave off |
| **C r1_fs channel** | `variant/r1_fs_channel.cpp`, `fs/r1_fs_channel*.h` | Opt-in `R1_FS_CHANNEL=0`, excluded from default kernfull (`Makefile:349-363`) | n/a | n/a | Not in production |
| **C gate/test harness** | `tools/r1_*.py`, `tests/` | Verification tools (retire, fullchain, DOOM, per-subsystem) | n/a | n/a | Keep — do not delete with shims |

## 3. Recommended retirement order (dependency-first)

1. **Consolidate the applied file-server cut** (done at boot; still to do: remove dead
   files/objects and the `qemu_variant_asic.cpp` `R1Fs::*`) — only after a clean
   `r1_final_verify.py --strict-skip` on the current tree.
2. **svcmgr** — re-run the guarded apply/review; edit `qemu_variant_asic.cpp:194`,
   then remove `SvcMgr*Exec` handlers and execs 268–270 with the file. Preserve the
   exec-81 wrapper until EFILE teardown is proven without it (see risks).
3. **winsrv** — port `edisp` LDD/PSL, pass EWSRV external-client window gate, then
   hand-delete `winsrv.c`/`guishost.h`/`qemu_windowsrv.*`/GUI execs and fix the
   ~12 GUI users + DOOM `guishost.h` fallback. **Human review; no apply path.**
4. **EUSER static shim** — separate closure via real PE2 imports.
5. **netstack** — never retire; ESOCK absent.

## 4. Risks and caveats

- The two applied edits are **bypasses, not deletions**: shim files and kernel objects
  remain linked; a disk without staged genuine EFILE/EFSRV no longer boots. Keep a
  rollback snapshot (`/tmp/opencode/shim_retire_backup_20261124_083011`, token
  `ROLLBACK-PORTFILESRV-20261124_083011`) until the full gate is green.
- `SvcMgrExecInstall()` also patches the genuine **exec 81 `FsRegisterThread`**
  wrapper that clears system-permanent/critical flags so EFILE teardown gives
  0 FAULT (`K::ESystemThreadPanic=4`), and exec 270 op 4 is the PTRINJ proof sink.
  Blindly removing it reintroduces KERN 4 / breaks `r1_genuine_pointer_test.py`.
- **Gate authority (2026-11-26):** the two conflicting snapshots this bullet used to
  cite are **superseded** — `FINAL_GATE_STATUS.md`'s 03:26 table showing EFSRV FAIL
  (`SessionPath r=-15`) is historical, and `SHIM_RETIREMENT_RESULT.md`'s
  `--strict-skip` 10 PASS / 1 FAIL (with `cp4_stress` proven pre-existing by A/B stash)
  is likewise stale. The authority is **FINAL27: 15/15 PASS, `FINAL27_RC=0`**, all 15
  rows green ([`FINAL_GATE_STATUS.md`](FINAL_GATE_STATUS.md):248 plus that file's EOF
  correction footer). The one rule that survives: never accept `SessionPath r=-15`
  as IPC success.
- Winsrv removal has the widest blast radius (GUI apps + DOOM); do it last among
  servers. After step 5 the console may stay as a debug aid; it is not a fork.
- Never re-add the EFILE trampoline (EFILE single-owner decision). `kernelhwsrv/`
  is read-only throughout; no commit/push/make clean was performed here.

## 5. Correction — 2026-11-25 (supersedes the §1–§3 "still linked" claims)

**Authority:** [`P5_PROVENANCE_RESULT.md`](P5_PROVENANCE_RESULT.md) — P5 static
audit **PASS** (`tools/r1_source_provenance_audit.py`, static read-only, no build,
no QEMU, no commits). This section corrects §1–§3 in place of rewriting them.

§1's framing — "retired = boot path bypassed", shim files "still compiled and
linked" — no longer describes the tree. The A-class shims below were **deleted**,
not bypassed. P5 reports shims retired **4 of 6**.

| A-class item | §1–§3 said | Actual state (2026-11-25) |
|---|---|---|
| `portfilesrv` | files/objects still present | **RETIRED** — `user/portfilesrv.cpp` absent (headers kept) |
| `svcmgr` | installed at boot, apply rolled back | **RETIRED** — `user/svcmgr.cpp` absent (headers kept) |
| `winsrv` | never applied, file delete pending | **RETIRED** — `winsrv.c`, `guishost.h`, `qemu_windowsrv.{cpp,h}`, `qemu_gui_exec.cpp` all absent |
| `euser` static shim | linked into import-free E32s | **RETIRED** — `user/euser_shim.cpp` absent; `e32base.h`/`e32cmn.h` kept |
| `r1_fs_*` | linked unconditionally via `Makefile:326` | **LIVE** — 15 files, 6 linked; the 6 are the `r1_fs_channel` family, gated by `R1_FS_CHANNEL` (off by default, **not** in `KERNFULL_OBJS`) |
| `netstack` | never retire, ESOCK absent | **KEEP_AUTHORED** — unchanged, still the explicit authored exception |

Corrections to specific §1 claims: `Makefile:326` no longer adds
`r1_fs_core.o`/`r1_fs_fat.o` to `KERNFULL_OBJS`; the in-kernel r1_fs glue was
already removed (see the `Makefile:318-320` comment, `docs/R1FS_GLUE_FEASIBILITY.md`)
and the sole surviving kernel entry is `build/fat32.o`. The `R1Fs::*` lifecycle in
`variant/qemu_variant_asic.cpp` and `SvcMgrExecInstall()` at
`qemu_variant_asic.cpp:194` are gone with their sources, so §3 steps 1–3 and §4's
risk bullets on `SvcMgrExecInstall()`/exec 81 describe removed code.

What actually remains: `r1_fs_*` + `variant/fat32.cpp` (ring-3 `sfat32` PSL
unfinished — §3 step 1 and item **A r1_fs_\*** are still live), and `netstack`.
Nothing else in the A class is left to retire.

**Caveat (not fixed here, out of this doc's ownership):**
`genuine/efsrv_min/build_server.sh:15` still lists `build/user/euser_shim.o` on its
link line although `user/euser_shim.cpp` is deleted — a stale reference, not
evidence the shim is live. A build from a clean tree would fail there until that
line is dropped or the reference is confirmed covered by `build/user/`.

**Limitation:** P5's genuine/glue byte ratio stays **N/A** — it needs build
artifacts the tool never produces by design. The 4/6 retired baseline plus authored
netstack replaces §2's "still hand-written" count.

## 6. Correction — 2026-11-25 (line-level evidence for §5)

§5 is confirmed by re-running the checks read-only. The two claims it overturns
are recorded here with their command output, so the audit trail does not depend
on §5's prose alone.

- §1 lines 22–26 — "the shim **files and their kernel objects are still compiled
  and linked**: `Makefile:326` still adds `r1_fs_core.o`/`r1_fs_fat.o`" — is
  **false**. `grep -n "r1_fs_core\|r1_fs_fat\|R1_FS_CHANNEL" Makefile` returns
  only `342:R1_FS_CHANNEL ?= 0`, `343:R1_FS_CHANNEL_OBJ :=`, `345:R1_FS_CHANNEL_OBJ
  := build/r1_fs_channel.o`, `351:KERNFULL_OBJS += $(R1_FS_CHANNEL_OBJ)`. No
  unconditional r1_fs objects exist. The `Makefile:318-320` comment states the
  kernel-side r1_fs glue was removed because its sole in-kernel consumer (the
  `Asic::Init3` mount block) is now a direct `FatMount(4,0)`, and the only
  surviving kernel entry is the default-off `R1_FS_CHANNEL` object. The same
  paragraph's `R1Fs::FatBackend`/`R1Fs::Core` construction at
  `qemu_variant_asic.cpp:151-164` and the `PortFileServerPublished()` read at
  `e32load.cpp:1529` are dead code paths, gone with their sources.
- §3 steps 2 and 3 (retire `svcmgr`, then `winsrv`) are **already done at file
  level**: `ls` returns ENOENT for `user/svcmgr.cpp`, `user/winsrv.c`,
  `user/guishost.h`, `variant/qemu_windowsrv.cpp` and `variant/qemu_gui_exec.cpp`.
  §4's caveats — `SvcMgrExecInstall()` wrapping exec 81, winsrv's blast radius —
  describe code that no longer exists, so step 3's `edisp` port plus the
  external-client window gate is the remaining work, not shim deletion.
- Still live and unretired: `r1_fs_*` + `variant/fat32.cpp` (the ring-3 `sfat32`
  PSL is unfinished) and the authored `netstack`. Nothing else remains in class A.

**2026-11-26 — correction:** the §5 caveat above is resolved — `genuine/efsrv_min/build_server.sh` no longer references `build/user/euser_shim.o` (`grep -n euser_shim` on it returns nothing); its link line carries `build/user/{start,runtime,ipc,malloc}.o`, `genuine/efsrv_min/efsrv_min.o` and the objects built from `fs/r1_fs_core.cpp` and `fs/r1_fs_server.cpp` only.

**2026-11-26 — correction (§5 row at :100):** the `r1_fs_*` **LIVE** row above ("15 files, 6 linked", `r1_fs_channel` family gated by `R1_FS_CHANNEL`) is **superseded by §5's own correction at :103** and no longer describes the tree — `grep -n 'r1_fs_channel\|R1_FS_CHANNEL' Makefile` returns **zero** refs (the old `342:R1_FS_CHANNEL ?= 0` / `345:R1_FS_CHANNEL_OBJ` / `351:KERNFULL_OBJS += $(R1_FS_CHANNEL_OBJ)` block is gone), all **12** `r1_fs_channel` paths were deleted in S3 (§5 residue sweep **PASS, 0 residue**; only `docs/r1_fs_channel_{integration,review}_v1.md` and retirement tooling still name them), and the surviving `r1_fs_*` keepers are **`r1_fs_core` / `r1_fs_server` / `r1_fs_fat` / `r1_fs_backend`** (`fs/r1_fs_backend.h`, `fs/r1_fs_core.{cpp,h}`, `fs/r1_fs_fat.{cpp,h}`, `fs/r1_fs_server.{cpp,h}`) — treat this row's "6 linked / gated by `R1_FS_CHANNEL`" claim as retired.

## 7. Footer — 2026-11-26 (FINAL26 authority, ewsrv cache decouple, EUSER retarget)

Read-only audit additions; no files deleted, no build, no QEMU, no commits, `kernelhwsrv/` untouched.

- **Gate authority is FINAL27.** `r1_final_verify.py` = **15/15 PASS, `FINAL27_RC=0`**
  (`OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`), superseding both the 03:26
  `FINAL_GATE_STATUS.md` EFSRV-FAIL snapshot and `SHIM_RETIREMENT_RESULT.md`'s 10 PASS /
  1 FAIL. Evidence: [`FINAL_GATE_STATUS.md`](FINAL_GATE_STATUS.md):248 and its EOF footer.
  §4's gate-authority bullet above is updated in place to match.
- **ewsrv portfilesrv cache decouple.** `tools/r1_genuine_ewsrv_test.py` no longer stages
  the out-of-repo cached `portfilesrv` flat image: `grep -c -i cache` on it returns **0**,
  and it stages exactly `EWSRV.E32` + `EWCLIENT.E32` + genuine `EUSER.DLL`
  (`:93`, `FAT OK: EWSRV.E32 + EWCLIENT.E32 + EUSER.DLL staged` at `:116`). The gate row
  `genuine_ewsrv` is PASS 16.8s in FINAL26.
- **selfcheck EUSER retarget.** The two roster-reachable scripts that hardcoded the
  `~/.cache/sybian-r1/.../urel/euser.dll` fixture now read the tracked root `EUSER.DLL`
  (byte-identical payload, `sha256 4851fec4…dd017b`): `tools/r1_selftest.py:37`
  (`DLL = os.path.join(ROOT, "EUSER.DLL")`) and `tools/r1_cp4_ca_test.py:39`
  (path built from `__file__`). The `selfcheck` row runs both (`cp4_ca` + `selftest`) and
  is PASS 233.9s in FINAL26, so the cache-less-checkout FAILs are closed.
- **Remaining best-effort cache dependency, recorded not fixed.**
  `tools/r1_genuine_retire_test.py:55` still defines the cached
  `PORTFILESRV = Path("/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv")`
  and stages it at `:538-539` only under `if PORTFILESRV.is_file():` — best-effort, absent
  path is a no-op, not a FAIL. Consequence: `genuine_retire` is **4/4 PASS** either way,
  but that one row is not yet fully cache-free the way `genuine_ewsrv` now is. This is a
  tolerated dependency, not a dormant shim: `user/portfilesrv.cpp` stays absent, so no
  A-class item is reintroduced by it. Non-roster residue noted earlier and unchanged:
  `tools/r1_genuine_input_test.py:60`, `tools/r1_genuine_pointer_test.py:48`.
