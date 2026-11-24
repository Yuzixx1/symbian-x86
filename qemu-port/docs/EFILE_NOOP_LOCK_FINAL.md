# EFILE no-op chain-lock patch: unconditional, EFSRV gate GREEN

Date 2026-11-24. Owner: EFILE port shim. `kernelhwsrv/` READ-ONLY and untouched;
no commit/push; no `make clean`.

## 1. Change

`docs/SESSIONPATH_REPLY_HANG.md` identified the final 8/9 hang: genuine
`CFsMessageRequest::Dispatch()` blocked in `FsPluginManager::ReadLockChain()`
(`RReadWriteLock::ReadLock`, `us_rwlock.cpp:84-106`) and never reached
`DoRequestL`/`Complete`. The port fix — a side-by-side patched copy of the
genuine `sfile/sf_request.cpp` that replaces every chain-lock call with
`EFILE_PORT_NOOP_LOCK()` (`genuine/efile/patched/sf_request.cpp:2`) — is now
compiled in **every** build.

In `genuine/build_efile.py:99-109` the patched TU is compiled and substituted
for `sf_request.o` unconditionally. The remaining `EFILE_DEBUG_MAIN` gate
(:111) covers **only** the instrumented `sf_main.cpp` diagnostic; it cannot
remove the lock patch. No trampoline was re-added; the guard-only re-entry
suppression in `efile_glue.cpp` is unchanged.

Safety: this port has one EFILE server thread and no plugin threads, so the
plugin chain is always empty and needs no synchronization.

## 2. Rebuild + static validation (DEFAULT, `EFILE_DEBUG_MAIN` unset)

```
env -u EFILE_DEBUG_MAIN python3 genuine/build_efile.py
RESULT: .../genuine/build/EFILE.E32 (273044 bytes, 463 EUSER imports,
        IAT 0x40000), e32read OK
```

- `e32read.py`: `EXE cpu=x86 ... code=263996 data=176 bss=8396912`,
  uids `0x1000007a 0x1000008c 0x100000bf` (UID3 correct), checksum ok,
  CRC/header/import validation ok.
- UTF-16 build flags `-D_UNICODE -fshort-wchar` are in `PATCH_FLAGS`; the
  smoke gate independently confirms `E32Validate: expDescType=2` (16-bit
  export descriptors).
- Patch present in the image: `nm -C genuine/efile/obj_build/sf_request_shim.o`
  has **zero** `ReadLockChain`/`UnlockChain` references, while the genuine
  `genuine/obj/EFILE/sf_request.o` has both `U` refs; the patched source
  itself contains no `ReadLockChain`/`UnlockChain` text. The linked ELF still
  defines the functions (from `sf_plugin_man.o`) but `sf_request` no longer
  calls them.

## 3. Runtime gates (all fresh, DEFAULT image)

Logs: `~/.cache/sybian-r1/efile_noop_lock_final/`.

| Gate | Result |
|---|---|
| `r1_efile_clean_prove.py` | `EVIDENCE: PASS (18/18 hard rows)`; `CAPTURE: PASS`; IPC round trip OK x2; SessionPath r=-15 = 0; `#PF`/FAULT/Panic/Exc = 0 |
| `r1_efile_acceptance.py` | `RESULT: PASS (9/9 criteria pass)`, exit 0 |
| `r1_genuine_retire_test.py` | efsrv/ewsrv/estart/euser all PASS; `RESULT: PASS (4 passed, 0 failed)`, exit 0 |
| `r1_qemu_smoke.py` | `RESULT: PASS`, exit 0 |
| `r1_genuine_efsrv_test.py` | PASS, `F32CLIENT: IPC round trip OK`, no Exc/Panic/FAULT, exit 0 |

Acceptance raw markers (`.../efile_acceptance.log`): start=1, registered=1,
E32Main returned=0, `#PF`=0, port-shim=0, `IPC round trip OK`=2,
`SessionPath r=-15`=0, Exc/Panic/FAULT=0, `GenuineBoot: EFILE.E32 r=0`=1;
`F32CLIENT: SessionPath r=0` x2 precedes each `IPC round trip OK`.

## 4. Conclusion

The EFSRV gate is GREEN with the default (non-debug) build; no environment
variable is required for EFILE IPC to work. This closes the last item from
`SESSIONPATH_REPLY_HANG.md`. Do not re-add the read/write lock calls to the
patched TU and do not re-add the EFILE thread-entry trampoline (the EUSER TLS
issue needs a separate approved per-process TLS mapping task).
