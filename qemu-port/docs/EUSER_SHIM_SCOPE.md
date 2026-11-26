# EUSER_SHIM_SCOPE — scoping the static EUSER shim retirement (2026-11-24)

**Scope:** read-only scoping report. No edits besides this file, no QEMU, no
commit/push, no `make clean`. `kernelhwsrv/` untouched. Companion to
`docs/REMAINING_SHIMS_AUDIT.md` §2/§3 (cleanup step 6 / audit order step 4).

## 1. Symbol inventory — what the shim actually is

`user/euser_shim.cpp` is 34 lines and provides exactly **five** out-of-line
symbols. Everything else in `user/e32base.h` / `user/e32cmn.h` is header-inline:

| Symbol | Kind | Consumers |
|---|---|---|
| `CActiveScheduler::iScheduler` | static member | `ca_test.cpp` (Install/Add/Start/Stop), any CActiveScheduler user |
| `CleanupStack::iStack` | static member | `fs/r1_fs_server.cpp` (`CleanupStack::PushL/Pop`) |
| `User::iTlsIndex` | static member | declared only; no live use found |
| `User::WaitForExit(TUint,TRequestStatus&,TInt)` | function (P-06: `process_wait_exit` → genuine execs 123/124 ProcessLogon) | declared in e32base.h; no in-tree caller besides the definition |
| `__cxa_pure_virtual` | weak symbol for abstract `CActive::RunL/DoCancel` | any TU instantiating a CActive subclass (`ca_test.cpp`) |

Header-inline (NOT deleted with the shim): `CBase` (operator new/delete),
`CActive`, `CActiveScheduler` queue/Start loop, `CActiveSchedulerWait`,
`RMessage2` (over raw `ipc_*` execs), `CSession2`, `CServer2::Start` accept loop,
`CleanupStack` methods, `TDesC16/TDes16/TPtrC16/TBuf16`, 8-bit descriptors,
`TPckg*`, `TVersion`, `TUid/TSecureId`, `_LIT`, all `KErr*` constants.

### Linkers of `euser_shim.o` (complete list)

| Link line | Image | Shim symbols actually needed |
|---|---|---|
| `user/user.mk:205` `ca_test.elf` | `ca_test_static.e32` | `CActiveScheduler::iScheduler`, `__cxa_pure_virtual` |
| `user/user.mk:361` `svcmgr.elf` | `SVCMGR.E32` | `CleanupStack::iStack` + `__cxa_pure_virtual` (via `fs/r1_fs_server.o`: CServer2/CSession2/RMessage2/CleanupStack/User::Leave*) |
| `genuine/efsrv_min/build_server.sh:15` | research `efsrv_min` | same CServer2 set via `fs/r1_fs_server.cpp` |

Headers `e32base.h`/`e32cmn.h` are included by: `user/euser_shim.cpp`,
`user/svcmgr.cpp`, `user/ca_test.cpp`, `fs/r1_fs_server.h` (→ `r1_fs_server.cpp`,
also compiled at `user.mk:353`), `genuine/efsrv_min/efsrv_min.cpp`.
Generic rule `user.mk:263` (`%.o: user/%.cpp user/e32base.h`) also keys on the
header. **No C app links the shim** — all ~40 C user E32s already use
`elf2e32.py --euser-import 915` PE2 imports.

## 2. Conversion path to genuine PE2 imports (proven)

- `tools/elf2e32.py` **already emits PE2 imports**: `--euser-import ORD` writes
  a 1-based ordinal into an IAT slot at `codeBase+0x8000` (SHN_ABS defsym) and
  sets `iImpOff`/DLL-dep count 1 (`elf2e32.py:94-119,131-135`). Used by ~40 apps
  for ordinal 915 (`RDebug::Printf`).
- `genuine/pack_multi.py` generalizes to **N ordinals** at `--iat-start`
  (4-aligned, ≥ text length), per-image `--uid3`; the loader accepts up to
  **512 slots** (`variant/qemu_variant.h` `iSlots[512]`). Proven by EWSRV
  (212/219 ordinals, `--iat-start 0xD000`), full EFILE (463), EFSRV/F32,
  ESTART (`0x10272C04`)/ESHELL (`0x101F7AE7`).
- Genuine EUSER.DLL (2503 exports, resident every boot) covers the needed
  calls: 915 Printf, 423–428 CActiveScheduler, 426 Start, 604/626
  WaitForRequest/WaitForAnyRequest, 617 RequestComplete, 902–913/1701–1710
  RChunk, 210–215/1547–1549 RSessionBase/RMessage2, 1953 FsRegisterThread.

**Per-app plan:**
1. `ca_test` (C++): convert to `pack_multi` image — IAT slots for 426 (Start),
   626/604 (wait), 915 (Printf), plus exec-based timer kept as-is; replace
   `CActiveScheduler::Install/Add` statics with genuine ordinals 423–425.
   The PE2 fixture already exists as `tools/mke32_ca_test.py` /
   `tools/mke32_webbrowser.py` (`ca_test.e32`), so `ca_test_static.e32` is
   redundant once converted — retire the static build, keep the PE2 one.
2. `svcmgr` / `fs/r1_fs_server.cpp`: blocked on the svcmgr + r1_fs retirement
   (audit steps 1–2); do NOT convert in isolation. When svcmgr is deleted, this
   consumer disappears with it.
3. `genuine/efsrv_min/build_server.sh`: research-only; re-point at pack_multi or
   drop with the efsrv_min research build.

**Remains shim-only (header-level, no EUSER ordinal):** descriptor helpers
(`TDes16::SetLength/Zero`, `TBuf*`, `TPckg*`, `_LIT`, `TPtrC*`), `CBase::operator
new`, `CleanupStack` push/pop semantics. These stay as thin inline headers —
per `GENUINE_USERSPACE.md` §2, keep `e32base.h` as a declaration-only header
exposing genuine layouts; only `euser_shim.cpp` (the implementation shim) is
deleted. `User::WaitForExit` (P-06) has no direct EUSER export; map to genuine
`RProcess::Logon/LogonCancel` ordinals or keep as a small exec-based helper
outside the deleted file.

## 3. Deletion set and build-rule edits (when executed, not now)

1. `user/user.mk:205` — drop `build/user/euser_shim.o` from `ca_test.elf`.
2. `user/user.mk:350` — delete the `build/user/euser_shim.o` rule.
3. `user/user.mk:361` — drop `euser_shim.o` from `svcmgr.elf` (only after svcmgr
   itself is retired; until then svcmgr needs the statics).
4. `user/user.mk:263` — decouple the `%.o: %.cpp` pattern rule from `e32base.h`.
5. `genuine/efsrv_min/build_server.sh:15` — drop `euser_shim.o`.
6. Delete `user/euser_shim.cpp`. Keep `user/e32base.h`/`e32cmn.h` (declarations)
   until svcmgr/r1_fs/efsrv_min consumers are gone, then shrink to genuine
   layouts only.
7. `Makefile` — no direct reference (grep: no `euser_shim`); only via `user.mk`.

## 4. Gate criteria and test dependency

Of the 11 strict-gate tests (`r1_final_verify.py --strict-skip`), **none execute
an euser_shim-linked binary at runtime**: `SvcMgrBootLaunch()` is commented out
(`variant/e32load.cpp:1552`), `r1_genuine_retire_test.py` asserts the fallback
is *not* used, and the ca_test fixture under test is the PE2 `mke32_ca_test.py`
image. The dependency is **build-time only**: `make all` → `userland` builds
`SVCMGR.E32` and `ca_test_static.e32`, which link `euser_shim.o`.

Closing gate:
- `python3 tools/r1_final_verify.py --strict-skip` = **11/11 PASS** with
  `user/euser_shim.cpp` deleted and `make userland` green without it.
- `r1_genuine_retire_test.py` EUSER gate (`:634`) still PASS (header/UID/2503
  residency check — unaffected by the static shim).
- `r1_os_selfcheck.py` (incl. cp4_ca PE2 fixture) PASS.
- `e32read.py` on converted images shows the expected PE2 import count.

## 5. Risks

- `__cxa_pure_virtual` must be defined somewhere for any TU with abstract
  `CActive` subclasses once the shim object is gone (small runtime.c stub).
- `User::WaitForExit` (execs 123/124) is shim logic — do not silently drop it
  if P-06/WAITFOR depends on it; WAITFOR.E32 itself is import-free C and does
  not link the shim.
- Do not re-open the EFILE single-owner/trampoline questions; guard-only model
  stays. Never accept `SessionPath r=-15` as IPC success.
- `e32base.h` deletion is coupled to svcmgr/r1_fs retirement — do not delete
  the headers while `fs/r1_fs_server.cpp` still compiles against them.

## 6. Ordering relative to svcmgr / winsrv

Audit §3 order stands: file-server files → **svcmgr** (removes the
`svcmgr.elf` shim link line and the `r1_fs_server` consumer) → winsrv →
**euser_shim last** (its only remaining C++ consumers are svcmgr/r1_fs/
efsrv_min, all retired by then; `ca_test` converts to PE2 independently).
Netstack never retires (ESOCK absent).

---

## Footer (2026-11-26) — strict-skip gate supersession

The §4 closing gate above, `r1_final_verify.py --strict-skip` = **11/11 PASS**,
is the historical 11-row gate and is **superseded**. The live strict-skip gate is
the 15-row gate: `FINAL27_RC=0` — **15 pass, 0 skip, 0 fail**
(`/tmp/opencode/final27_gate.log`). The 2026-11-24 body is retained unchanged as
the historical record.
