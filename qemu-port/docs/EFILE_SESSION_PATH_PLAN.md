# EFILE session-path / system-drive investigation (ranked #2 EFSRV cause)

Date: 2026-11-24. Scope: `qemu-port/genuine/efile/` glue, `EFILE.E32`, this doc.
`kernelhwsrv/` READ-ONLY, untouched. No commit/push, no `make clean`.

## 1. Root cause: the reported #2 premise is false

`docs/EFILE_IPC_CONNECT_ANALYSIS.md:82` ranked "session-path shape" as #2 and
claimed the genuine `NewSessionL` path is `?:\Private\<SID>\` because EFILE has
no `'C'` shim. Source + QEMU evidence disprove this:

* `RFs::GetSystemDriveChar()` is **not** an EUSER import and **not** the EFAT32
  stub (`genuine/efat32/obj/EFAT32/stubs.S:219-224`, which returns 0 for the
  EFAT32 plugin). It is genuine **EFSRV** code (`sfsrv/cl_cli.cpp:161-193`)
  compiled from `EFSRV.A` straight into `EFILE.E32`. Disassembly of the final
  `genuine/build/EFILE.elf` confirms:
  `GetSystemDriveChar()` = `GetSystemDrive() + 0x41`.
* `GetSystemDrive()` calls
  `RProperty::Get(0x100039e3 /*KFileServerUidValue*/, 0x10283049 /*KSystemDriveKey*/, drive)`
  and, when that read fails, returns `EDriveC` (2). Hence `'A'+2 == 'C'`.
* `sf_main.cpp:121-126` starts from the literal `?:\\Private\\` but overwrites
  `[0]` with that result, so the literal `?` is a template, never the runtime
  value in this port.

## 2. Setting the system drive explicitly: rejected approach

A port-side `RProperty::Define(0x100039e3, 0x10283049, EInt, ...)` + `Set(..., 2)`
in glue was implemented and built first. QEMU (`/tmp/opencode/efile_session_path_prove.log:1230-1234`)
showed `Define r=-46` = `KErrPermissionDenied`. Kernel rule
(`sproperty.cpp:384-390`): category `0x100039e3` differs from EFILE's secure ID
(UID3 `0x100000bf`) and is below `KUidSecurityThresholdCategoryValue`, so
`ECapabilityWriteDeviceData` is required. That grant lives in
`variant/e32load.cpp` (not owned here), and no PortSvr facility exists.
Crucially it is **unnecessary**: the genuine fallback already returns `'C'`.
The failing calls were removed rather than left as a misleading no-op.

## 3. Implemented change (port glue only)

`genuine/efile/efile_glue.cpp`: `EfileLogSystemDrive()` logs
`RFs::GetSystemDriveChar()` once at startup via the genuine exported symbol
`_ZN3RFs18GetSystemDriveCharEv`, with a comment recording the P&S category/key,
the `-46` reason, and why no write is needed. No genuine logic is patched.
`genuine/efile/patched/sf_main.cpp` (diagnostic-only copy, built only with
`EFILE_DEBUG_MAIN=1`) prints the first three UTF-16 units of `thePath` in
`NewSessionL` for repeatable path evidence.

## 4. Evidence

* Build: `python3 genuine/build_efile.py` →
  `genuine/build/EFILE.E32` 273036 bytes, **463 EUSER imports**, `e32read OK`;
  UTF-16 literals `!FileServer`, `?:\Private\` present (`strings -el`).
* Diagnostic QEMU (`/tmp/opencode/efile_session_path_debug.log:1386`):
  `EFILE-DBG: session path chars 67,58,92 len=20` = `'C', ':', '\'` →
  `C:\Private\<hex SID>\` (20 chars). Session-path shape is **valid**.
* Final QEMU (`/tmp/opencode/efile_session_path_final.log`):
  `EFILE: system drive char=67 ('C')` ×3; 464 PE2 binds (463 EFILE + 1 client);
  `File server thread registered: 1`; `EFILE: port-shim: 0`; no guest fault.
* `tools/r1_genuine_efsrv_test.py --log /tmp/opencode/efile_session_path_efsrv.log` → PASS.
  Note: it stages EFSRVMIN (`PORTFILESRV.E32`), so it is a regression check,
  not a direct FULL-EFILE test.
* `tools/r1_qemu_smoke.py --log /tmp/opencode/efile_session_path_smoke.log` → PASS.

## 5. Blockers / honest status

IPC is **still `F32CLIENT: SessionPath r=-15`** (`KErrServerTerminated`).
The final log shows three `genuine file server starting` and two
`E32Main returned 0` — duplicate launches from ranked #1. Two owners exit and
detach every session (`sipc.cpp:723-745`), so request 14 fails before any path
is returned. **Only #1 can explain `-15`; #2 is cleared by the evidence above.**
No fake success is claimed. Next owner action: fix the duplicate launch in
`variant/`/boot prove (parallel agent); then re-run `prove_efile_genuine.py`
and require `F32CLIENT: IPC round trip OK` with a single owner.
