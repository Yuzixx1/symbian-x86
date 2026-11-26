# Genuine-component EUSER sret ABI audit

Status: **fixes landed; verification incomplete**. This report began as a
read-only static audit; the `-freg-struct-return` rebuilds it recommended have
since been applied to EFILE, EWSRV/EWCLIENT and estart/etshell. `kernelhwsrv/`
untouched; nothing committed/pushed; `make clean` not run. **EFILE is not fully
fixed**: its `EBadHandle` fault is cleared, but a new `CServer2::Start` blocker
is IN FLIGHT. No QEMU tests were run as part of this report update — all test
results below are as reported by the owning agents.

## 1. Scope and method

Audited every PE2 E32 under `qemu-port/genuine/build/`; `EFILE.E32` was
originally excluded (separate rebuild in flight) and is now covered in §3.

* Ordinal map (authoritative, read-only):
  `kernelhwsrv/kernel/eka/bx86gcc/euseru.def` — 2503 exports, 1-based ordinals
  (ordinal table built from the commented EXPORTS lines). Sanity check:
  `@ 1793` = `_ZNK7RThread2IdEv` (`RThread::Id() const`),
  matching the GDB diagnosis. The older ARM `bmarm/euseru.def` has different
  ordinals (`RThread::Id` = 702) and must **not** be used.
* Import extraction: PE2 IAT is at `code_off + text_size`, 1-based EUSER
  ordinals, `nimp` from the import section (`imp_off + 8`). Verified against
  `tools/e32read.py` / `tools/inspect_imports.py`.
* Return types: genuine declarations in `kernelhwsrv/kernel/eka/include/`
  (`e32std.h`, `e32cmn.h`, `e32des8.h`, `e32des16.h`, `e32event.h`) plus the
  def comments.
* Call sites: `objdump -d` on the staged ELFs in `genuine/build/` and
  `genuine/ewsrv/obj/ewsrv.elf`.

## 2. Which returns are actually hazardous

| Return category | MSVC/Symbian EUSER | gcc `-m32` default | Verdict |
|---|---|---|---|
| struct/class of **1/2/4/8 bytes** | EAX / EDX:EAX | hidden sret ptr pushed as **first** arg | **HAZARD** |
| struct/class **> 8 bytes** (`TRect` 16 B, `TExitCategoryName` ≈44 B, `TFileName` ≈516 B, `TDateTime`) | sret ptr | sret ptr | SAFE (same shape) |
| scalar typedef (`TUint`, `TUint32`, `TUint64`, `TInt`, enums) | EAX / EDX:EAX | EAX / EDX:EAX | SAFE |

The failure mechanism: EUSER reads `this` at `[ebp+8]`. When gcc inserts the
sret pointer first, EUSER reads the (uninitialised) return buffer as `this`,
and all real args shift one slot. In EFILE this produced `iHandle == 0` →
`Exec::ThreadId(0)` → `KERN 0x4 EBadHandle`. `-freg-struct-return` removes the
hidden pointer for ≤8 B returns, restoring EAX/EDX:EAX.

Important layout facts measured from the genuine headers:

* `TThreadId`/`TProcessId` derive from `TObjectId { TUint64 iId; }` → **8 B**.
* `TTimeIntervalMicroSeconds { TInt64 iInterval; }` → **8 B**.
* **`TDesC16 { TUint iLength; }` + `TPtrC16 { TUint16 *iPtr; }` → 8 B.**
  So `TDesC16::Mid/Left/Right` and `TLex16::NextToken/MarkedToken/Remainder`
  are **register-returned by EUSER** and are just as hazardous as `RThread::Id`.
  (A first pass that assumed TPtrC-derived types were >8 B was wrong; corrected
  here after reading `e32des16.h:156` and the `TPtrC16` declaration.)
* `TExitCategoryName` is `TBuf16<16>` (≈44 B) → SAFE; likewise `TFileName`.

## 3. Inventory and status

STATUS legend: **FIXED IN IMAGE** = rebuilt with `-freg-struct-return`, call-site
disassembly re-checked; **UNTESTED PATH** = fixed in image but that specific
call path has never been exercised; **PENDING** = verification/test task still
in flight. Import counts are post-rebuild where given.

| Component (`genuine/build/`) | EUSER imports | Risky imports (ordinal → return) | STATUS / evidence | Verification (reported) |
|---|---|---|---|---|
| **EFILE.E32** | 463 | 1793 `RThread::Id` → `TThreadId` (`FsThreadManager::SetMainThreadId`) | **FIXED IN IMAGE (partial)** — rebuilt 273040 B / 463 imports; new `SetMainThreadId` calls `RThread::Id` with only `this`, result from EAX. Flag in `tools/genuine_build.py`, `genuine/build_efile.py`, `genuine/f32_efsrv_build.py` | `EBadHandle` (`KERN 0x4`) fault **CLEARED**; new separate fault `CServer2::Start("!FileServer")` error → `User::Panic` reason=3 → `KERN 4` — **IN FLIGHT** (likely shim name collision). **NOT fully fixed** |
| **EWSRV.E32** | 212 | 1868 `TRawEvent::Pos() const` → `TPoint` (8 B). (672/676 `User::Lower/UpperCase` return scalar `TUint` — false positives.) | **FIXED IN IMAGE / pointer path UNTESTED** — rebuilt 56672 B / 212 imports; flag at `genuine/ewsrv/build_ewsrv.py:46`; `TRawEvent::Pos` sites 0x40083b/852/874/88e, 0x406104 pass only `this`, read EAX:EDX | `r1_genuine_ewsrv_test.py` PASS; pointer path UNTESTED (see §6) |
| **EWCLIENT.E32** | 20 | none (was SAFE) | **FIXED IN IMAGE** — rebuilt 53812 B / 20 imports; flag at `genuine/ewsrv/build_client.py:44` | `r1_genuine_ewsrv_test.py` PASS |
| **etshell_.E32** | 190 | 1793 `RThread::Id`; 1849 `RProcess::Id`; 1677 `TTime::MicroSecondsFrom`; 2104/2105/2106/2107 `TDesC16::Mid/Left/Right`; 1017/1738/1745 `TLex16::NextToken/MarkedToken/Remainder` → all 8 B sret | **FIXED IN IMAGE / command paths PENDING** — post-fix disasm: `RThread::Id` @0x4000a1, `RProcess::Id` @0x4000e0, `TTime::MicroSecondsFrom` @0x402ccd, `TDesC16::Mid` @0x400c65, `TLex16::NextToken` @0x40100b — sret push gone, EAX/EDX results | `r1_genuine_eshell_test.py` PASS; SYSINFO/TIME/argument paths **PENDING** (separate test task in flight, see §6) |
| **etshell_dbg.E32** | 190 | identical import set to `etshell_.E32` | **FIXED IN IMAGE** — rebuilt (190 imports); same flag/baseline as `etshell_.E32` | build verified; same PENDING command paths as `etshell_.E32` |
| **estart_.E32** | 95 | 2104/2105/2106 `TDesC16::Mid/Left`; 1017/1738/1745 `TLex16::*` → `TPtrC16` | **FIXED IN IMAGE / config paths PENDING** — rebuilt (95 imports); `TDesC16::Mid` @0x400c65, `TLex16::NextToken` @0x40100b — sret push gone, EAX:EDX results | estart gate PASS; config/argument paths **PENDING** (see §6) |
| dbg2_.E32 / dbg3_.E32 | 17 / 23 | none | SAFE (unchanged) | — |
| EXPORTPROBE, GENUINE, GENUINEDLL, GENUINESRV, F32CLIENT_RFS, dm_domaincli, dm_domainpolicy, dm_domainsrv, hal_halu, hal_smoke | 1 each | only ord 915 `RDebug::Printf` (variadic, returns `TInt`) | SAFE (unchanged) | — |

(Also checked outside `build/`: `F32PROBE.E32`, `F32CLIENT.E32`, each a single
ord-915 import → SAFE.)

## 4. Call-site evidence — pre-fix baseline (original audit)

Every call below showed the sret pointer pushed **before** `this`, i.e. the
broken shape. On cdecl the last push is `[ebp+8]`, the slot EUSER reads as `this`.
These offsets are retained as the baseline against which the post-fix images
(§4.1) were compared; they describe the **old** binaries, not the current ones.

| Call site (ELF offset) | Ordinal / symbol | Push sequence (pre-fix) |
|---|---|---|
| `genuine/build/etshell_.elf:0x4000a1-0x4000a9` | 1793 `_ZNK7RThread2IdEv` | `lea -0xf8(%ebp),%eax; push %esi (this); push %eax (sret); call`; result read back from `-0xf8(%ebp)` at `0x4000ae`. **This is the exact EFILE failure pattern.** |
| `genuine/build/etshell_.elf:0x4000e0-0x4000e8` | 1849 `_ZNK8RProcess2IdEv` | `lea -0x100(%ebp),%eax; push %edx (this); push %eax (sret); call` |
| `genuine/build/etshell_.elf:0x402ccd-0x402ce1` | 1677 `_ZNK5TTime16MicroSecondsFromES_` | `lea -0x290(%ebp),%edx; push <TTime hi>; push <TTime lo>; push %esi (this); push %edx (sret); call` |
| `genuine/build/etshell_.elf:0x4021c7-0x4021d6` | 2104 `_ZNK7TDesC163MidEi` | `lea -0x648(%ebp),%edx; push %esi (arg); push %edi (this); push %edx (sret); call` |
| `genuine/build/estart_.elf:0x400c65-0x400c6f` | 2104 `_ZNK7TDesC163MidEi` | `lea -0x28(%ebp),%esi (sret); push -0x30(%ebp) (arg); push -0x3c(%ebp) (this); push %esi; call`; result read from `-0x28/%ebp` and `-0x24(%ebp)` |
| `genuine/build/estart_.elf:0x40100b-0x40100d` (and 8 more) | 1017 `_ZN6TLex169NextTokenEv` | `push %esi (this); push %edi (sret); call`; result read from `-0x90(%ebp)` |
| `genuine/ewsrv/obj/ewsrv.elf:0x400844-0x40084b` (and 4 more) | 1868 `_ZNK9TRawEvent3PosEv` | `lea -0x38(%ebp),%eax (sret); push %edx (this); push %eax; call` — in `CEvent::RunL()`, mouse-move path |

Exception checked: `etshell_.elf:0x401318-0x40131c` (`TChar::GetUpperCase`)
pushes only one word (`lea -0x2c(%ebp),%eax; push %eax; call`), i.e. no hidden
sret slot; treat as single-argument `this`, not a flagged mismatch.

### 4.1 Post-fix disassembly proof (verified)

All rebuilt images now pass `this` as the first/only stack arg and read the
result from EAX/EDX:EAX; the `lea -0x..(%ebp),%reg` + two pushes pattern is gone:

* EFILE `SetMainThreadId`: `RThread::Id` — only `this` pushed, result from EAX.
* estart/etshell: `RThread::Id` @0x4000a1, `RProcess::Id` @0x4000e0,
  `TTime::MicroSecondsFrom` @0x402ccd, `TDesC16::Mid` @0x400c65,
  `TLex16::NextToken` @0x40100b.
* EWSRV `TRawEvent::Pos`: 0x40083b/852/874/88e and 0x406104 — only `this`
  pushed, result read as EAX:EDX.

## 5. Why the QEMU tests pass (and what that does not prove)

From `docs/BOOT_CHAIN_VERIFICATION.md` and the test sources:

* `r1_genuine_eshell_test.py` **PASS** — stages `estart_.E32`/`etshell_.E32`
  and drives only `estart`, `eshell`, `help`, `exit`. The risky command-parser
  paths (`TLex16::NextToken`, `TDesC16::Mid`, `GetThreadInfo`, `RunExec` timing)
  are never reached.
* `r1_genuine_ewsrv_test.py` — 8/8 functional markers PASS, no Exc/FAULT; the
  only FAIL is a known harness screendump/duplicate-launch issue. The
  `TRawEvent::Pos()` calls live in `CEvent::RunL()`'s pointer-event path; no
  mouse events are injected by the test.
* `r1_genuine_retire_test.py` **PASS** (ewsrv/estart/euser PASS, efsrv SKIP)
  checks boot markers and absence of faults, not the unexercised code paths.
* `EFILE` — the `EBadHandle` (`KERN 0x4`) fault is gone after the rebuild, but
  boot now fails later at `CServer2::Start("!FileServer")` → `User::Panic`
  reason=3 → `KERN 4`. That is a separate, IN FLIGHT blocker (likely a shim
  name collision), not an sret-ABI regression.

After the fixes, a PASS means "the exercised paths are now ABI-safe"; it still
does **not** cover the paths listed in §6.

## 6. Unverifiable paths

These paths are fixed in the images but cannot be validated in the current
environment; they remain the residual risk.

* **Pointer / mouse events (EWSRV `TRawEvent::Pos`).** QEMU is started without
  a mouse/USB tablet device, so no real pointer events reach `CEvent::RunL()`.
  The existing mouse test and press handling feed the **handwritten wsrv ring**
  directly, not the genuine `EWSRV` path, and there is no `Kern::AddEvent`
  caller in the image. Asserting sane coordinates / no fault therefore needs a
  guest mouse device plus a `Kern::AddEvent` source — kernel/variant changes
  that are out of scope for this ABI port.
* **eshell command paths (`SYSINFO`, `TIME`, argument parsing).** The current
  `r1_genuine_eshell_test.py` only drives `estart`/`eshell`/`help`/`exit`; it
  never tokenises a descriptor with `TLex16::NextToken`, slices one with
  `TDesC16::Mid`, or calls `RThread::Id`/`RProcess::Id`/`TTime::MicroSecondsFrom`
  via `GetThreadInfo`/`RunExec`. A separate test task is tasked with driving a
  command that parses a descriptor/token and a thread/process-info command and
  asserts no `Exc/PanicKernExec/FAULT`; that task is still in flight, so these
  paths are **PENDING**. The same caveat applies to estart config/argument
  parsing and to `etshell_dbg.E32`.
* **EFILE `CServer2::Start`.** Not a verification gap but an active blocker:
  the `!FileServer` server name likely collides with a shim name. IN FLIGHT in
  a different task; EFILE must not be reported as fully fixed until it clears.

## 7. Rebuild status (was: recommended rebuild order)

The flag is now present in all relevant build scripts:

1. `EFILE.E32` — `tools/genuine_build.py`, `genuine/build_efile.py`,
   `genuine/f32_efsrv_build.py`. Rebuilt (273040 B / 463 imports);
   `EBadHandle` cleared, `CServer2::Start` blocker IN FLIGHT.
2. `EWSRV.E32` / `EWCLIENT.E32` — `genuine/ewsrv/build_ewsrv.py:46`,
   `genuine/ewsrv/build_client.py:44`. Rebuilt (56672 B / 212 and 53812 B / 20).
3. `estart_.E32` / `etshell_.E32` / `etshell_dbg.E32` —
   `genuine/estart_eshell/build_estart_eshell.py` (`COMPILE_FLAGS` plus two
   direct gcc paths). Rebuilt (95 / 190 / 190 imports).
4. `dbg2_`, `dbg3_`, `dm_*`, `hal_*`, probe images — no risky imports; rebuild
   only for hygiene / one toolchain baseline.

Each rebuild passes `-freg-struct-return` to all C/C++ TUs, including
`glue.cpp`/`shims.cpp` and the genuine sources; `euser_thunks.S` is assembly and
needs no change. Import ordinals/counts were checked to be stable across each
rebuild.

## 8. Verification recipe (per component)

1. Rebuild with `-freg-struct-return`; keep the ordinal list stable (assert the
   PE2 IAT count/ordinals are unchanged — e.g. `tools/e32read.py <image>.E32`
   and re-extract the IAT).
2. `objdump -d --no-show-raw-insn <image>.elf | grep -B8 'call.*<symbol>'`
   for each ordinal in §3/§4: confirm the caller passes `this` as the **first**
   stack arg (no extra push) and reads the result from EAX/EDX:EAX rather than
   from a stack buffer. Specifically: no `lea -0x..(%ebp),%reg` immediately
   followed by two pushes before the call. (Done for the sites in §4.1.)
3. Re-run the component test and force the previously untested path:
   * estart/eshell: drive a shell command that parses a descriptor/token and a
     thread/process-info command; assert no `Exc/PanicKernExec/FAULT` —
     **PENDING**, see §6.
   * EWSRV: inject a pointer/mouse event so `CEvent::RunL()` calls
     `TRawEvent::Pos()`; assert coordinates are sane and no fault —
     **UNTESTED**, see §6.
4. Re-run `tools/r1_genuine_retire_test.py` and `tools/r1_os_selfcheck.py`.

## 9. Limitations

* No QEMU tests were executed for this report update; all PASS/FAIL results are
  as reported by the agents that landed and exercised each fix.
* `EFILE.E32` is only partially fixed — the `CServer2::Start("!FileServer")`
  panic (reason=3 → `KERN 4`) is IN FLIGHT and likely a shim name collision.
* Ordinal-to-name mapping comes from the x86gcc def, not from symbol names in
  the prebuilt EUSER.DLL (E32 export directories are ordinal-by-position).
* Return-type classification is from genuine header declarations; a handful of
  single-push call sites (e.g. `TChar::GetUpperCase`) were left UNKNOWN rather
  than asserted hazardous.
* This is largely a static audit: "FIXED IN IMAGE" means the mismatch is gone
  from the emitted code; it is not proof that every call path has executed. The
  gaps in §6 are where a latent fault could still surface.

**Supersession footer (2026-11-26, append-only — the body above is preserved as this audit's own state on 2026-11-24 and is NOT current):**

- **Header (:8) "EFILE is not fully fixed … `CServer2::Start` blocker is IN FLIGHT":** cleared. The `-fshort-wchar` rebuild this report called for is landed in all four drivers — `tools/genuine_build.py:92`, `genuine/build_efile.py:87`, `genuine/f32_efsrv_build.py:47`, `genuine/efat32/build_fsy_dll.py:55` — so `_LIT` names are UTF-16 and `CServer2::Start("!FileServer")` no longer returns KErrBadName (-28): `File server thread registered=1, port-shim=0, E32Main=0, 0 faults` (`docs/CHANGE_INVENTORY.md:54`).
- **§3 EFILE row (:65) "FIXED IN IMAGE (partial) … NOT fully fixed":** EFILE is fixed as an image. The current binary is `genuine/build/EFILE.E32`, 273 044 B / 463 imports; the row's 273 040 B is the pre-`-fshort-wchar` build. The row's "likely shim name collision" guess is dead on its own terms — `user/portfilesrv.cpp` and `user/svcmgr.cpp` no longer exist, so no shim publishes `!FileServer`; the only publishers left are the genuine `efsrv_min` host and the `fs/r1_fs_server.cpp` unit-test seam.
- **§6 third bullet and §9 second bullet (:154, :201):** the "active blocker" they describe is closed, not pending. The gate that carried it, `tools/r1_genuine_efsrv_test.py`, is `genuine_efsrv PASS 15.1s` in FINAL27 (`/tmp/opencode/final27_gate.log`), ending `F32CLIENT: IPC round trip OK` and `no Exc/PanicKernExec/FAULT`. §6's first two bullets — unexercised pointer/`TRawEvent::Pos` events and eshell command paths — remain a real coverage gap and are unaffected by this note.
- **Standing evidence for the whole note:** the authoritative gate is 15 rows, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`, HEAD `ee8490e`, `kernelhwsrv/` untouched, nothing committed or pushed.
