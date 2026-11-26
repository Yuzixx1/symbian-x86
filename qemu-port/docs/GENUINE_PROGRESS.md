# GENUINE_PROGRESS — live status of the genuine-userspace pivot

**Date:** 2026-11-24. **Owner:** tracking subagent. `kernelhwsrv/` is read-only
and untouched (`git status -- kernelhwsrv/` clean). No commits/pushes made.

Status companion to [`GENUINE_USERSPACE.md`](GENUINE_USERSPACE.md) (rule and
ordered plan), [`GENUINE_BUILD_REVIEW.md`](GENUINE_BUILD_REVIEW.md) (toolchain
audit) and [`SHIM_RETIREMENT_PLAN.md`](SHIM_RETIREMENT_PLAN.md) (dry-run
inventory). Sources of truth: `tools/r1_genuine_retire_test.py`,
`tools/r1_genuine_{efsrv,eshell,ewsrv}_test.py`, `tools/r1_qemu_smoke.py` and
the serial boot-chain reports [`BOOT_CHAIN_VERIFICATION.md`](BOOT_CHAIN_VERIFICATION.md),
[`FULLCHAIN_VERIFICATION.md`](FULLCHAIN_VERIFICATION.md), plus the read-only
diagnoses [`EFILE_IPC_CONNECT_ANALYSIS.md`](EFILE_IPC_CONNECT_ANALYSIS.md) and
[`EFSRV_UNBLOCK_CHECK.md`](EFSRV_UNBLOCK_CHECK.md) (all 2026-11-24).

## 1. Subsystem status table

"Retired shim?" = the hand-written `qemu-port` replacement is out of the
production path. **No shim is fully retired yet.** "Runs in QEMU?" is the
latest full serial run (2026-11-24).

| Subsystem | Genuine source (kernelhwsrv/) | Built? | Runs in QEMU? | Retired shim? | Blocker |
|---|---|---|---|---|---|
| EUSER (runtime DLL) | `kernel/eka/euser/` → prebuilt `euser.dll`, UIDs `10000079/1000008d/100039e5`, **2503 exports** | YES (header+UID checksum validated) | YES — resident in every boot (`e32run: EUSER.DLL resident ... exports=2503`); retire gate **PASS** | Partial — DLL genuine; static `user/euser_shim.cpp` + `e32base.h` still linked into import-free E32s | `tools/elf2e32.py` must emit real PE2 imports for all user E32s (plan step 1) |
| HAL | `halservices/hal/` → `genuine/build/hal_halu.E32` (DLL, 6 exports from `bx86/halu.def`) + `hal_smoke.E32` exe twin | YES | YES — `genuine/prove_hal_dm.py` PASS: `HAL::Get(ECPU)=2`, `EMemoryPageSize=4096`, `Set/Get(ELanguageIndex)` round-trip, `GetAll`=121 entries, no faults (not a retire gate) | NO | `UserSvr::HalFunction` still stubbed `KErrNotSupported`; HAL exec 79 remains port-variant code |
| Domain manager | `userlibandfileserver/domainmgr/` → `dm_domainsrv.E32` (server exe), `dm_domaincli.E32` (51 ords), `dm_domainpolicy.E32` (3 ords) | YES | YES (smoke) — `TTransitionConfig::CheckValues r=0`, bad spec `r=-260` (`prove_hal_dm.py` PASS) | NO | Full `E32Main()` bring-up needs real EUSER server exports (CServer2, RMessagePtr2); `dm_stubs.S` marks the boundary |
| File server / F32 | `fileserver/{sfat32,sfsrv,sfile,fs_utils}/` → archives `F32.A`/`EFSRV.A`/`EFILE.A` (93 sources compile clean); probes `f32_probe/F32PROBE.E32`, `F32CLIENT.E32`, `efsrv_min/EFSRVMIN.E32`; full **`EFILE.E32` now BUILDS (273,068 B)** | YES — full EFILE builds and binds **464 EUSER imports in QEMU**, reaches `E32Main` with `File server thread registered: 1`, `E32Main returned=0`, port-shim=0, **0 FAULT/Panic** and no exit 111; F32CLIENT uses genuine UTF-16 `!FileServer` and gets **IPC round trip ×2**; `r1_genuine_efsrv_test.py` PASS; **retire gate efsrv PASS** (was FAIL/SKIP); **on-disk FAT I/O VERIFIED** via `FATIOCLIENT.E32` (`r1_genuine_fat_io_test.py` PASS, see §2) | YES (probes) — F32PROBE runs unmodified genuine F32 byte code; EFSRVMIN + F32CLIENT IPC round trip PASS (`r1_genuine_efsrv_test.py`); static protocol 13/13 PASS | NO — `user/portfilesrv.cpp` + `fs/r1_fs_*` + `variant/fat32.cpp` still production | **B5 on-disk FAT I/O DONE** (2026-11-24, `FATIOCLIENT.E32` + `tools/r1_genuine_fat_io_test.py` PASS: disk-only `SEED.TXT` read, `FATIO.TXT` create/write/reopen/read `FATIO-MARKER-42A7` byte-for-byte, all r0, no faults); **EFSRV retire gate PASS (2026-11-24, was FAIL/SKIP)** — genuine UTF-16 `!FileServer`, 464 binds, thread registered=1, port-shim=0, IPC round trip ×2, 0 FAULT/Panic, no exit 111; r1_genuine_efsrv_test PASS, estart PASS, smoke PASS. Port-side exec-81 teardown fix LANDED (0 FAULT). **B3** follow-up IN FLIGHT. |
| Window server | `kernel/eka/ewsrv/` → `genuine/build/EWSRV.E32` (unmodified ewsrv/econs/ektran/edisp sources + 4 patched copies in `genuine/ewsrv/patched/`) | YES | YES — boot-time markers with **0 faults** (212 EUSER imports bound); retire gate **PASS**; `r1_genuine_ewsrv_test.py` **PASS** (boot-time marker detection, no duplicate launch); screendump renders | **RETIRED** — `user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp` **deleted** (commit `94ce9c9`); GUI apps run on the genuine EWSRV session through `user/ws_e32_client.c` (migrated in `97f224b`) | **VERIFIED (2026-11-26, FINAL27 15/15, `FINAL27_RC=0`)** — `EWCLIENT.E32` `Create`/`Init`/`SetTitle`/`Write` all `r=0`, window drawn, 232 PE2 EUSER imports bound, no fault/panic |
| Startup / shell | `fileserver/estart/` + `fileserver/etshell/` → `genuine/build/estart_.E32`, `etshell_.E32` | YES | YES — boot chain loads `EUSER → EFILE/EFSRV → ESTART → EWSRV → ESHELL` with svcmgr fallback; full markers, `C:\>` prompt, `E32Main returned 0`, 0 faults; retire gate **PASS** | NO — `user/svcmgr.cpp` + execs 268–270 (`variant/e32load.cpp`) still the fallback boot path | B4 FIXED; genuine `efsrv.dll` still absent so fallback stays live |

### Retire-test verdicts — latest full run (2026-11-24, exit 0)

```
efsrv    PASS    (genuine UTF-16 !FileServer; full EFILE 464 binds, thread registered=1, port-shim=0, IPC round trip x2, 0 FAULT/Panic, no exit 111; r1_genuine_efsrv_test.py PASS; static protocol 13/13 PASS; on-disk FAT I/O verified)
ewsrv    PASS    (boot-time markers + fresh screendump, 212 binds, no faults)
estart   PASS    (ESTART/ESHELL boot markers, no faults)
euser    PASS    (resident, 2503 exports, in every boot)
RESULT: PASS  (4 passed, 0 skipped, 0 failed)
```

`--dry-run` reports BUILT + static checks only: 13/13 opcode check PASS,
EWSRV/`estart_`/`etshell_`/EUSER BUILT YES, all runtime SKIP (exit 0). The
shim-inventory front end is `tools/r1_shim_retire_plan.py` (dry-run only,
**0 READY / 4 BLOCKED** — see `SHIM_RETIREMENT_PLAN.md`).

## 2. Verified today (2026-11-24)

> The EFILE `-fshort-wchar` KErrBadName (-28), `SessionPath r=-15` and
> duplicate-EFILE-owner bullets below are **historical** (2026-11-24 debugging).
> The **EFSRV gate PASS** verdict in §1 and the fullchain bullet are the current
> state: genuine UTF-16 `!FileServer`, 464 binds, port-shim=0, IPC round trip x2,
> 0 FAULT/Panic; r1_genuine_efsrv_test PASS, retire efsrv PASS (was FAIL).

- Genuine boot chain runs on a **supervisor DThread from `Asic::Init3`**;
  `E32Run` grants `ECapabilityProtServ` so `!`-prefixed names work. Order:
  EUSER → EFILE/EFSRV → ESTART → EWSRV → ESHELL, with `svcmgr`/`portfilesrv`
  fallback. Full markers verified with **0 faults**; `r1_qemu_smoke.py` **PASS 11/11**.
- `r1_genuine_retire_test.py`: ewsrv PASS, estart PASS, euser PASS, efsrv PASS
  (was FAIL/SKIP); smoke PASS;
  `r1_genuine_ewsrv_test.py` now **PASS** (boot-time marker detection, 212 binds,
  no duplicate launch).
- **On-disk FAT I/O VERIFIED (B5 milestone).** `genuine/efsrv_min/efsrv_min.cpp`
  uses `DiskFatBackend`; the port bridge `R1Fs::Backend` → `file_*()` →
  `PortFile`/`PortBigFile` → real FAT32 cluster-chain I/O (`AtaRead`/`AtaWrite`).
  New `FATIOCLIENT.E32` (`genuine/efsrv_min/fatio_client.cpp`) + `tools/r1_genuine_fat_io_test.py`
  **PASS**: disk-only `R1TEST\SEED.TXT` opened, `FATIO.TXT` created/written/closed,
  reopened and `FATIO-MARKER-42A7` read byte-for-byte; all r0, no
  `Exc`/`PanicKernExec`/`FAULT`, `FATIO.TXT` absent pre-boot and host `mtype`
  reads the marker from the FAT partition post-boot. `r1_genuine_efsrv_test.py`
  PASS; `r1_qemu_smoke.py` PASS. Genuine `TFsMessage` opcodes/descriptors, Core and
  `r1_fs_server.cpp` untouched; cluster I/O uses the port `R1Fat` adapter, not
  full `sfat32` (PE2 slot limit), and F32 BPB parsing remains in `F32PROBE`.
- **IN FLIGHT (not yet verified):** EFILE `CServer2::Start("!FileServer")` fix —
  GDB-proven **KErrBadName (-28)** from `User::ValidateName` in EUSER's
  `RServer2::CreateGlobal`: the `_LIT` name descriptor is UTF-32 (Linux gcc 32-bit
  `wchar_t`, no `-fshort-wchar`) while prebuilt EUSER reads 16-bit chars. A temp
  UTF-16-repatched EFILE made Start succeed; `-fshort-wchar` rebuild (alongside
  `-freg-struct-return`) is in flight. The shim-collision theory is disproved
  (SVCMGR.E32 absent) and `docs/EFILE_START_ANALYSIS.md`'s ranked cause #1 (EUSER
  return translation of a successful create) was superseded by later GDB evidence
  — exec 126/127 were never reached. Also IN FLIGHT: EWSRV external client window
  fix (B1); `estart`/`etshell` sret-ABI rebuild and EWSRV sret-ABI flag. Audit:
  `docs/ABI_AUDIT.md`.
- Boot-chain regression suite all PASS: selfcheck, smoke, cp4_ca, cp4_stress,
  selftest, genuine_efsrv, genuine_eshell, retire; ISO boot **0 faults**. Full
  report: `docs/BOOT_CHAIN_VERIFICATION.md` (written before the ewsrv harness
  fix; the harness is now PASS as above).
- Full genuine `EFILE.E32` **BUILDS** (273,068 B, 464 EUSER imports bound in
  QEMU, reaches `E32Main`, `File server thread registered`, `E32Main returned=0`,
  **0 FAULT** after the port-side teardown fix). The earlier `KERN 0x4`
  (`EBadHandle`) sret mismatch is superseded: `variant/e32load.cpp:1200-1236`
  now wraps exec 81 (`FsRegisterThread`) to clear process-critical/system-permanent
  flags, eliminating the residual KERN 4. **Remaining blocker (GDB-proven):**
  `CServer2::Start("!FileServer")` returns **KErrBadName (-28)** from
  `User::ValidateName` inside EUSER's `RServer2::CreateGlobal`. GDB at guest
  0x3e31d8 showed the name descriptor `[0x17, 0x21,0,0,0, 0x46,0,0,0, …]` —
  4-byte chars interleaved with NULs — and EAX=-28 at 0x3f03ff; exec 126/127 and
  `ServerCreateWithOptions` were never reached (no shim collision — SVCMGR.E32
  absent). Cause: genuine EFILE encodes `_LIT` literals as UTF-32 (Linux gcc
  32-bit `wchar_t`, no `-fshort-wchar`) while prebuilt EUSER reads 16-bit chars;
  two UTF-32 literals at EFILE.E32 offsets 0x357e0/0x39420, length 0x17. A temp
  UTF-16-repatched EFILE made Start succeed (handle 0x7AC00000, port-shim count
  0). **Fix IN FLIGHT (not done):** add `-fshort-wchar` alongside
  `-freg-struct-return` in tools/genuine_build.py, genuine/f32_efsrv_build.py,
  genuine/build_efile.py, genuine/efat32/build_fsy_dll.py.
- EFSRV const-descriptor blocker **root-caused and fixed** in
  `fs/r1_fs_server.cpp` (genuine kernel returns `KConstMaxLength ==
  KErrBadDescriptor (-38)`; the pre-scan treated it as fatal). On-disk FAT file
  I/O is now verified (FATIOCLIENT, see above); any remaining non-virtual
  `Backend` clean-up in `fs/r1_fs_backend.h` stays **IN FLIGHT**.
- **EFSRV / F32 — HISTORICAL diagnosis; gate now PASS (2026-11-24).** Earlier the
  F32CLIENT name fix had landed (UTF-16 `!FileServer` TLitC16, 464 binds,
  port-shim 0) but proof was pending: post-fix logs showed `SessionPath r=-15`
  (`KErrServerTerminated`) after EConnect, the freshest log was SIGTERM'd
  mid-load, and ranked residual causes were duplicate EFILE owners / session
  path form / descriptor width
  ([`EFSRV_UNBLOCK_CHECK.md`](EFSRV_UNBLOCK_CHECK.md),
  [`EFILE_IPC_CONNECT_ANALYSIS.md`](EFILE_IPC_CONNECT_ANALYSIS.md) §4).
  **RESOLVED:** genuine UTF-16 `!FileServer`, full EFILE 464 binds,
  `File server thread registered: 1`, port-shim=0, IPC round trip ×2, 0
  FAULT/Panic, no exit 111; `r1_genuine_efsrv_test.py` PASS and retire efsrv
  PASS (was FAIL); estart PASS, smoke PASS. Shim not yet retired.
- **IN FLIGHT (not yet verified; 2026-11-24):** EWSRV/EUSER boot-marker drift;
  guard-vs-trampoline investigation (port-shim=2 in some runs); proving the
  EFSRV PASS is non-flaky (repeated clean runs); eshell PS output.
- **VERIFIED (2026-11-26, FINAL27 15/15, `FINAL27_RC=0`):** full
  `r1_final_verify.py --strict` gate PASS — `OVERALL: PASS (15 pass, 0 skip,
  0 fail/not-run)`, log `/tmp/opencode/final27_gate.log`; `--strict-skip` is no
  longer pending.
- **Full genuine boot chain in ONE boot — PASS 14/14**
  ([`FULLCHAIN_VERIFICATION.md`](FULLCHAIN_VERIFICATION.md)): EFILE → ESTART →
  EWSRV → ESHELL all `r=0` in genuine order, `exports=2503`, 0
  `Exc`/`PanicKernExec`/`FAULT`, svcmgr fallback absent, QMP screendump shows the
  `C:/>` prompt. Caveat: result is against the **staged ISO** (concurrent
  `kernfull.elf`/`EWSRV.E32` rebuilds were not re-verified); re-run on a fresh
  build. Proves launch order, not per-server IPC functionality.

## 3. Port-layer blockers (B1–B7)

| # | Blocker | Owner | Status / refs |
|---|---|---|---|
| B1 | `!`-name registration + capabilities. | Port kernel (loader/DObjectCon) | **Capabilities FIXED** — `E32Run` grants `ECapabilityProtServ` on every loaded E32, boot-chain `!`-names publish. **External EWSRV client name lookup VERIFIED (2026-11-26, FINAL27 15/15, `FINAL27_RC=0`)** — `EWCLIENT.E32` `Create`/`Init`/`SetTitle`/`Write` all `r=0`, window drawn; name registration was never the blocker. |
| B2 | Import slots 16 → 512 (`variant/qemu_variant.h:394 iSlots[512]`, `variant/e32load.cpp:454`, `tools/e32read.py`); multi-DLL via `genuine/pack_multi.py`. | Loader/toolchain | **DONE** — enough for EWSRV's 212/219 ordinals and full EFILE's 463. |
| B3 | Const-descriptor rejection over the IPC bridge. | Port kernel IPC / `fs/r1_fs_*` | **Root cause FIXED** in `fs/r1_fs_server.cpp` (`-38` is normal for const descriptors). On-disk FAT file I/O now **verified** via FATIOCLIENT; any non-virtual `Backend` clean-up in `fs/r1_fs_backend.h` stays **IN FLIGHT**. |
| B4 | Boot chain: `svcmgr` faulted in `kheap.cpp`; execs 268–270 test-only; estart mount stubbed. | Startup/boot-chain workstream | **FIXED** — supervisor DThread from `Asic::Init3`, genuine order with `svcmgr` fallback; gates PASS. Execs 268–270 remain only as fallback. |
| B5 | Ring-3 FAT adapter + full file server. Production still `portfilesrv.cpp` / `fs/r1_fs_*` / `variant/fat32.cpp`. | F32/EFSRV + toolchain workstreams | **ON-DISK FAT I/O DONE (2026-11-24)** — `efsrv_min.cpp` uses `DiskFatBackend`; port bridge `R1Fs::Backend` → `file_*()` → `PortFile`/`PortBigFile` → real FAT32 cluster-chain I/O (`AtaRead`/`AtaWrite`). `FATIOCLIENT.E32` + `tools/r1_genuine_fat_io_test.py` **PASS** (disk-only `SEED.TXT` read, `FATIO.TXT` create/write/reopen/read `FATIO-MARKER-42A7` byte-for-byte, all r0, no faults; marker read back from FAT partition by host `mtype`). Cluster I/O via port `R1Fat` adapter, not full `sfat32` (PE2 slot limit); BPB parsing still via `F32PROBE`. Production shims not retired; exec-81 teardown fix LANDED (0 FAULT); EFILE `CServer2::Start` KErrBadName UTF-32 → `-fshort-wchar` rebuild **IN FLIGHT**. |
| B6 | Toolchain review: `.def` MSVC-decorated names + `--name` path sanitization. | Toolchain workstream | **DONE** — `GENUINE_BUILD_REVIEW.md` issues #1/#2 verified fixed. |
| B7 | G-01 boot auto-launch fault `KERN 0x62 (98)` during EUSER import binding. | Startup/boot-chain workstream | **FIXED** — replaced by supervisor-DThread `GenuineBootSpawn()`; retire gates back to PASS. No longer active. |

### What is unblocked / what remains

- **Unblocked:** genuine chain boot and retire gates (B1 caps, B4, B7), 512
  import slots (B2), toolchain safety (B6), EUSER residency + 2503 exports,
  **EFSRV gate PASS**, EFSRV wire protocol 13/13 PASS, EWSRV boot/run PASS.
- **Still blocked / IN FLIGHT:** B3 clean-up, EWSRV/EUSER marker drift,
  guard-vs-trampoline (port-shim=2), flakiness proof, eshell PS,
  `estart`/`etshell` sret-ABI rebuild and EWSRV sret-ABI flag
  (audit: `docs/ABI_AUDIT.md`). Port-side exec-81 teardown fix LANDED (0 FAULT).
  B5 on-disk FAT I/O DONE, but the ring-3 `sfat32` PSL and shim retirement
  remain. No shim retired; shim inventory 0 READY / 4 BLOCKED.
- **VERIFIED, no longer pending (2026-11-26, FINAL27 15/15, `FINAL27_RC=0`):**
  EWSRV external client window (B1 client side) and the full
  `r1_final_verify.py --strict` gate. `winsrv` and the GUI execs are deleted
  (`94ce9c9`), so that retirement is no longer on the remaining list.

## 4. Next steps (dependency order)

1. **EFILE fix (IN FLIGHT, `tools/genuine_build.py`, `genuine/f32_efsrv_build.py`,
   `genuine/build_efile.py`, `genuine/efat32/build_fsy_dll.py`)** — add
   `-fshort-wchar` alongside `-freg-struct-return` to the GCC flags and rebuild so
   `_LIT` literals are UTF-16 (not UTF-32) and `TThreadId` returns in EDX:EAX.
   The exec-81 teardown fix has already removed the residual `KERN 0x4` (0 FAULT,
   464 binds, `File server thread registered`, `E32Main returned=0`); the remaining
   failure is `CServer2::Start("!FileServer")` → **KErrBadName (-28)** from UTF-32
   literals. Confirm the rebuilt EFILE publishes `!FileServer`, drops the
   port-shim count to 0 and gets `F32CLIENT: IPC round trip OK`. Then link
   `EFAT32.FSY`/`EFSRV.DLL` and re-run `r1_genuine_retire_test.py` to flip `efsrv`
   SKIP → PASS. Do not claim fixed until the gate flips. Earlier theories (shim
   name collision; EUSER translation of a successful create — `docs/EFILE_START_ANALYSIS.md`
   ranked cause #1) were **superseded** by GDB: Start fails before exec 126/127
   are reached.
2. **B3 follow-up (IN FLIGHT)** — non-virtual `Backend` methods in
   `fs/r1_fs_backend.h`; then verify real file I/O (`r1_fcopy_test.py`,
   `r1_largefile_test.py`, `tests/r1_file_regression/`).
3. **EWSRV external clients — VERIFIED (2026-11-26, FINAL27 15/15,
   `FINAL27_RC=0`, log `/tmp/opencode/final27_gate.log`)** —
   `!E32WindowServer` client name lookup works (`EWCLIENT.E32`
   `Create`/`Init`/`SetTitle`/`Write` `r=0`, window drawn), and the retirement
   half is already done: `winsrv.c` / `guishost.h` / `qemu_windowsrv.*` /
   `qemu_gui_exec.cpp` were deleted in `94ce9c9` and GUI apps run on genuine
   EWSRV via `user/ws_e32_client.c` (`97f224b`). Only **`edisp` porting** to the
   QEMU framebuffer remains (gate: window tests, DOOM frame unchanged).
4. **B5 finish — ring-3 `sfat32` PSL** (on-disk FAT I/O DONE via `R1Fat` adapter)
   → replace the port `R1Fat` adapter with full `sfat32`, then retire
   `portfilesrv.cpp` / `fs/r1_fs_*` / `variant/fat32.cpp`; gate: DOOM WAD still
   loads from FAT32.
5. **Shrink/delete `svcmgr.cpp`** once the genuine file server is live; keep
   execs 268–270 only while the fallback is needed.
6. **EUSER closure** — `elf2e32.py` emits real PE2 imports for every user E32;
   delete `euser_shim.cpp` (plan step 1).
7. **Networking stays authored** — ESOCK is absent from this `kernelhwsrv/`
   snapshot, so `netstack.c` stays documented authored code behind a frozen
   Symbian-shaped ABI (never a "genuine" gate).

## 5. References

- [`GENUINE_USERSPACE.md`](GENUINE_USERSPACE.md) — rule, subsystem map, build
  strategy, ordered retirement plan §4, harness semantics §6.
- [`GENUINE_BUILD_REVIEW.md`](GENUINE_BUILD_REVIEW.md) — toolchain audit
  (CONDITIONAL verdict; issues #1–#3 fixed, see B6).
- [`SHIM_RETIREMENT_PLAN.md`](SHIM_RETIREMENT_PLAN.md) — dry-run shim inventory
  and ordered switch/rollback (0 READY / 4 BLOCKED).
- [`BOOT_CHAIN_VERIFICATION.md`](BOOT_CHAIN_VERIFICATION.md) — serial regression
  suite and ISO boot verification (2026-11-24).
- `genuine/F32_EFSRV_REPORT.md`, `genuine/HAL_DM_NOTES.md`,
  `genuine/COORD_EWSRV.md`, `genuine/estart_eshell/README.md`,
  `genuine/BUILD_MANIFEST.json` — per-workstream build/run reports.
- Tests: `tools/r1_genuine_retire_test.py`, `tools/r1_genuine_efsrv_test.py`,
  `tools/r1_genuine_ewsrv_test.py`, `tools/r1_genuine_eshell_test.py`,
  `tools/r1_qemu_smoke.py`, `genuine/prove_qemu.py`, `genuine/prove_hal_dm.py`.

## 6. Correction — 2026-11-25 (supersedes the §1 "Retired shim?" column)

**Authority:** [`P5_PROVENANCE_RESULT.md`](P5_PROVENANCE_RESULT.md) — P5 static
audit **PASS** (`tools/r1_source_provenance_audit.py`, static read-only, no build,
no QEMU, no commits). The §1 preamble "**No shim is fully retired yet.**", the
"NO" cells in the "Retired shim?" column and the closing "No shim retired; shim
inventory 0 READY / 4 BLOCKED" in §3 are **historical as of 2026-11-24** and are
corrected here rather than rewritten.

Actual tree state — shims retired **4 of 6**:

| Shim | State per P5 | Evidence in tree |
|---|---|---|
| `portfilesrv` | **RETIRED** (headers kept) | `user/portfilesrv.cpp` absent |
| `svcmgr` | **RETIRED** (headers kept) | `user/svcmgr.cpp` absent |
| `winsrv` | **RETIRED** (headers kept) | `user/winsrv.c`, `user/guishost.h`, `variant/qemu_windowsrv.{cpp,h}`, `variant/qemu_gui_exec.cpp` absent |
| `euser` static shim | **RETIRED** (headers kept) | `user/euser_shim.cpp` absent; `user/e32base.h` + `user/e32cmn.h` retained |
| `r1_fs_*` | **LIVE** — 15 files, 6 linked | `fs/r1_fs_*` (13) + `variant/r1_fs_channel.*` (2); the 6 linked are the `r1_fs_channel` family behind `R1_FS_CHANNEL` (off by default, not in `KERNFULL_OBJS`) |
| `netstack` | **KEEP_AUTHORED** | `user/netstack.c` + `netstack_iat.S` — ESOCK absent, explicit authored exception (plan §3 step 7) |

Consequences for the §1 blockers: B5 still stands for `r1_fs_*` / `variant/fat32.cpp`
(the ring-3 `sfat32` PSL is unfinished, `build/fat32.o` is in `KERNFULL_OBJS`),
but the `portfilesrv` half of B5 is closed by deletion. §4 steps 3, 5 and 6
(EWSRV/`winsrv`, `svcmgr`, EUSER closure) are likewise already done at file level;
what remains is `edisp` porting and the external-client window gate, not shim
removal. §1's "Partial" cell for EUSER should read **RETIRED** for the static shim.

**Limitation:** the genuine/glue byte ratio in P5 remains **N/A** — attribution
needs build artifacts (`build/obj/*.o`, `kernel.elf`, link map), which the audit
tool never produces by design. Track 4/6 retired + authored netstack as the
current shim baseline, not "no shim retired".

## 7. Correction — 2026-11-25 (file-server artifact numbers)

Two numbers in §1/§2 were stale. Corrected here, the original lines are kept.

- §1 line 29 — "B4 FIXED; genuine `efsrv.dll` still absent so fallback stays live"
  is **wrong**. `genuine/build/EFSRV.DLL` is present, **263,580 B (257.4 KiB)**,
  mtime 2026-11-24 01:43 (`ls -l genuine/build/EFSRV.DLL`); the P5 audit reports
  the same 263,580 B as the EFSRV artifact. Its sibling `genuine/build/EFAT32.FSY`
  is present too, 264,720 B. So an absent genuine FSY/DLL is no longer the reason
  for any fallback: the row's svcmgr-fallback clause is stale on its own account
  (`user/svcmgr.cpp` is deleted — see §6), and the only open item on this row is
  the §4 step to re-run `r1_genuine_retire_test.py` against the staged
  `EFAT32.FSY`/`EFSRV.DLL`.
- `EFILE.E32` is **273,044 B**, not 273,068 (§1 line 27 and §2 line 88). Measured
  `ls -l genuine/build/EFILE.E32` → 273044, the same value P5 records for the
  EFILE artifact. The 464-bind / `E32Main returned=0` results in those lines are
  unaffected; only the byte count was stale.

## 8. Correction — 2026-11-26 (EUSER shim + four deletions)

The §1 EUSER row (line 24) clause — "static `user/euser_shim.cpp` + `e32base.h`
still linked into import-free E32s" — is **superseded**. `user/euser_shim.cpp`
is **deleted** (commit `0f9ec6a`, "Close euser_shim: ca_test on genuine EUSER
PE2 imports"; `git log --diff-filter=D -1 -- qemu-port/user/euser_shim.cpp`
agrees, and the path is absent from the working tree). Three further shims named
in that column are deleted too, each in its own commit rather than `0f9ec6a`:
`user/winsrv.c` (`94ce9c9`), `user/svcmgr.cpp` (`dd22ae2`), and
`user/portfilesrv.cpp` (`e29a072`). With `portfilesrv.cpp` gone, genuine
`EFILE`/`EFSRV` are the **sole** file server on the boot path — no static port
file-server shim remains. The authoritative gate is the **15-row** roster —
**15/15 PASS** (`OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`,
`FINAL26_RC=0`, log `/tmp/opencode/final26_gate.log`); the earlier 13-row
`13/13 PASS` figure (`FINAL13_RC=0`, `final13_gate.log`) is superseded history.
Only `e32base.h` remains of the original
clause; the row's "Partial" verdict and its `elf2e32.py` PE2-imports blocker
still stand.

## 9. Supersession — 2026-11-26 (FINAL27, `FINAL27_RC=0`, 15/15)

Historical text above stays as written. This section supersedes three "IN FLIGHT"
anchors: line 73 (EFILE `CServer2::Start` / `-fshort-wchar`), line 123
(EWSRV/EUSER boot-marker drift) and line 155 ("Still blocked / IN FLIGHT").
Gate of record: `r1_final_verify.py --strict` → `OVERALL: PASS  (15 pass, 0 skip,
0 fail/not-run)`, `FINAL27_RC=0`, log `/tmp/opencode/final27_gate.log`, HEAD
`ee8490e`, no commits.

- **Line 73 — EFILE UTF-32 `_LIT` / KErrBadName (-28): CLOSED.**
  `-fshort-wchar` now sits beside `-freg-struct-return` in every build entry
  point — `tools/genuine_build.py:92`, `genuine/f32_efsrv_build.py:47`,
  `genuine/build_efile.py:87`, `genuine/efat32/build_fsy_dll.py:55`. EFILE
  publishes `!FileServer`, registers its server thread, port-shim 0; FINAL27
  `genuine_efsrv` **PASS 15.1s**. The temp UTF-16 repatch is no longer needed.
- **Line 123 — EWSRV/EUSER boot-marker drift: CLOSED for the gated set.**
  FINAL27 `genuine_ewsrv` **PASS 17.0s**, `genuine_eshell` **PASS 19.7s**,
  `genuine_retire` **PASS 44.4s**, `qemu_smoke` **PASS 50.1s**, `selfcheck`
  **PASS 234.1s**. The same 15/15 holds in FINAL25 and FINAL26, so the drift does
  not reproduce across three consecutive gates. `port-shim=2` stays an audit
  observation, not a gate failure.
- **Line 155 — what actually stays open.** B3 non-virtual `Backend` clean-up in
  `fs/r1_fs_backend.h`, the ring-3 `sfat32` PSL, and shim retirement
  (0 READY / 4 BLOCKED) are unchanged. Dropped from that row: EWSRV external
  client window (verified, B1 client side) and `winsrv`/GUI execs (deleted in
  `94ce9c9`). eshell PS output is still unproven — `genuine_eshell` passing
  proves launch, not PS rendering.
