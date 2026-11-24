# HAL/domainmgr E32 regression check against the current toolchain/kernel

Verification-only, 2026-11-24. Read-only: no source/test/build-script changes, no
rebuild of kernfull/ISO, no commit/push, `make clean` not run, `kernelhwsrv/`
untouched. Only this report was created.

## Commands

```
cd qemu-port
stat -c '%y %s %n' build/kernfull.elf genuine/build/{hal_smoke,dm_domainsrv,hal_halu,dm_domaincli,dm_domainpolicy}.E32
python3 genuine/prove_hal_dm.py            # stdout: /tmp/opencode/hal_dm_regression_proof.log
python3 tools/inspect_imports.py genuine/build/<image>.E32
python3 tools/e32read.py --quiet genuine/build/{hal_halu,hal_smoke,dm_domainsrv,dm_domaincli,dm_domainpolicy}.E32
# direct IAT read: code_off(hdr+100)+text_size(hdr+96), nimp at imp_off+8
```

`prove_hal_dm.py` stages ONLY its own `HAL_SMOKE.E32` + `DM_DOMAINSRV.E32` plus
the cached genuine `EUSER.DLL` and `portfilesrv` onto a scratch FAT32 disk
(`/tmp/opencode/hal_dm_proof.img`); it boots the existing `build/kernfull.elf`.

## Evidence

- **Kernel used**: `build/kernfull.elf`, 1021784 B, mtime 2026-11-24 02:01:13
  +0200 — unchanged before/after the proof. No stale/concurrent-rebuild
  artifact detected; another agent's retire test was running on a separate
  QEMU instance/disk and did not interfere.
- **Proof result: PASS** (15/15 checks), serial log
  `/tmp/opencode/hal_dm_proof.log`:
  - `e32run: bind slot 32768 ord 915 -> 003e88b8` (x2), `e32run: Loaded r=0`
    (x2), `E32LoadDll r=0 impN=1 sDllExpN=2503`.
  - HAL: `HAL::Get(ECPU) -> r=0 value=2 (ECPU_X86=2)`,
    `EMemoryPageSize -> 4096`, `ELanguageIndex roundtrip=7`,
    `GetAll -> r=0 entries=121 ptr=02200620`, `SMOKE: done`.
  - DM: `TTransitionConfig(spec).CheckValues -> r=0`, `(bad) -> r=-260`,
    `TDmDomainState=2 iState=2`, `SMOKE: done`.
  - No `Exc ` / `PanicKernExec` / `FAULT:` lines.
- **Imports**: all five images (hal_halu, hal_smoke, dm_domainsrv,
  dm_domaincli, dm_domainpolicy) pass `e32read --quiet` and import exactly one
  EUSER ordinal, **915 = RDebug::Printf** (variadic, `TInt` scalar return).
  No ≤8-byte struct/class-returning import exists in any HAL/DM image, so the
  `-freg-struct-return` sret hazard from `docs/ABI_AUDIT.md` §2/§3 cannot occur
  across the EUSER boundary here. Every other EUSER/EFSRV reference is resolved
  by in-image stubs (`genuine/hal/hal_glue.cpp`, `genuine/dm/dm_stubs.S`,
  `dm_cli_stubs.S`, `dm_policy_stubs.S`), i.e. internally consistent ABI.
- **Staleness confirmed**: images date 2026-11-23 20:55–21:00, while the shared
  toolchain (`tools/genuine_build.py`, `genuine/BUILD_MANIFEST.json`, 01:43)
  now adds `-freg-struct-return` + `-fshort-wchar`. Binary proof:
  `genuine/build/hal_smoke.elf:0x3044` contains UTF-32 `48 00 00 00 41 00 00
  00 4C 00 00 00 00 00 00 00` for `_LIT(KLitHal,"HAL")`
  (`kernelhwsrv/halservices/hal/src/hal_main.cpp:20`); the UTF-16 pattern is
  absent. No `L""`/`_LIT` literals occur in the domainmgr sources or the
  generated HAL tables checked. There is no visible failure, so the images are
  stale but functional, not corrupted.

## Verdict

The older HAL/domainmgr E32s **still work unchanged** under the current staged
kernel and current EUSER.DLL: both load, all expected markers print, all
CheckValues codes match, and the run is fault-free. The `-fshort-wchar` staleness
is real but latent on the exercised paths (`_LIT(KLitHal,"HAL")` feeds only the
local `User::Panic` stub, which ignores `aCategory` and prints the reason).

## Recommendation

**No rebuild required for correctness today.** Per `docs/ABI_AUDIT.md` §7 item 4,
a rebuild is hygiene only (one toolchain baseline). Defer it until either
(a) HALU/DOMAINCLI/DOMAINPOLICY DLLs are loaded or linked against components
built with the new flags and a 16-bit descriptor or ≤8-byte struct-returning
export crosses that boundary, or (b) the genuine panic path is wired to real
EUSER rather than the local stub. If done later, rebuild via
`genuine/hal/build_hal.sh` + `genuine/dm/build_dm.sh` and re-run this proof;
assert the IAT stays at ord 915 only.

## Caveats

- DLL-typed images (hal_halu, dm_domaincli, dm_domainpolicy) cannot be started
  by `E32Run` (UID1 0x10000079 != 0x1000007a); they were validated structurally
  only, not executed.
- Smoke paths do not cover `UserSvr::HalFunction` implementation attributes,
  full `E32Main()` server bring-up, or real EUSER panic/descriptor handling.
- Results are from the staged 02:01 kernel; a subsequent kernfull rebuild
  invalidates the runtime portion of this check (the static import/literal
  findings still hold).
