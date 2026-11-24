# Genuine ESTART + ESHELL (estart.mmp / eshell.mmp) as PE2 E32

Builds the **unmodified** genuine startup components from `kernelhwsrv/` on
our x86 kernel:

* `estart.cpp` + `estartmain.cpp` (fileserver/estart, estart.mmp) -> `genuine/build/estart_.E32`
* `ts_com/ts_data/ts_edshl/ts_main/ts_utl/ts_deps/ts_clicomp.cpp` +
  `sfile/sf_inflate.cpp` + `sf_decomp.cpp` (fileserver/etshell, eshell.mmp) -> `genuine/build/etshell_.E32`

`kernelhwsrv/` is read-only; all glue lives here.

## Build / prove

```bash
python3 genuine/estart_eshell/build_estart_eshell.py            # estart_ + etshell_ (+ dbg variants)
python3 genuine/estart_eshell/prove_eshell.py                   # QEMU proof (serial log)
python3 tools/r1_genuine_eshell_test.py                         # r1 assertion test
```

Pipeline: pass-1 link with `--unresolved-symbols=ignore-all`, `nm -u` splits
undefined symbols into (a) EUSER imports thunked (`jmp *sym_slot`) and bound by
`pack_multi.py` to the staged genuine EUSER.DLL ordinals from
`kernelhwsrv/kernel/eka/bx86gcc/euseru.def` (ord 915 = `RDebug::Printf` there),
and (b) generated `xor %eax,%eax; ret` stubs for everything else. Note:
`e32def.h` typedefs `VA_LIST` as `TInt8*[1]` (mangles `PPa`) while the staged
EUSER exports `Pc` variants, so `PPa` symbols are remapped to the `Pc`
ordinals (same 4-byte ABI) — see `remap` lines in the build log.

Flags add `-fshort-wchar` (Linux wchar_t is 32-bit; genuine `_L()` casts
`L"..."` to `TText*`, so without it every string literal has length 1).

## Files

* `build_estart_eshell.py` — builder (defines/includes per genuine/README.md +
  fileserver/hal/domainmgr include dirs). Outputs prefixed `estart_`/`etshell_`.
* `glue.cpp` — `app_main` -> runs `.init_array`, prints the banner via EUSER
  ord 915, calls genuine `E32Main()`. Also creates the **thread heap** via
  genuine EUSER `UserHeap::SetupThreadHeap` (ord 1360): our `E32Run` calls
  `Kern::ProcessCreate` directly, which skips the user thread heap that
  genuine `RProcess::Create` would create, so without this `User::Alloc`
  derefs NULL.
* `shims.cpp` — documented local definitions replacing non-EUSER deps:
  * console: `Console::NewL` (EUSER ord 1115 would load econs*.dll) and the
    used `CConsoleBase` methods; output via genuine EUSER `RDebug::Printf`
    (ord 915) for `Printf` and raw PortCon echo (exec 10 fn 3) for `Write`;
    input via the PortCon lease (exec 10 fn 0/1, poll with `sleep_ms`).
  * `TDes16::FormatList`/`AppendFormatList` (VA_LIST ABI mismatch, see above)
    with a local formatter.
  * EFSRV.DLL client critical path: `RFs::Connect/Close/DriveList/SessionPath/
    SetSessionPath/Entry/CharToDrive/GetSystemDriveChar/SetSystemDrive/
    StartupInitComplete`, `CFileMan::NewL` (+ ctors/dtors).
  * HAL.DLL: `HAL::Get/Set` (KErrNone, value 0).
  * DOMAINCLI.DLL: `RDmDomainManager::{Connect,Close,WaitForInitialization,
    RequestSystemTransition,RequestDomainTransition}`.
  * C++ runtime: `operator new/delete` -> EUSER `User::Alloc/Free`,
    `__cxa_atexit`, `__dso_handle`, `__cxa_pure_virtual`.
  * All other EFSRV/HAL/DOMAINCLI client symbols get generated ret-0 stubs
    (`obj/*stubs*.S`, printed at build time: 24 for estart_, 89 for etshell_).
* `dbg2_main.cpp` / `dbg3_main.cpp` / `dbg_main.cpp` — debug-only components
  (`dbg2_`, `dbg3_`, `etshell_dbg`) used to locate the heap/console/VA_LIST
  issues; not part of the shipped images.
* `prove_eshell.py` — QEMU proof runner.

## QEMU result (2026-11-23)

`r1_genuine_eshell_test.py` PASS: both images validate (e32read), load, bind
ord 915, print their banners via EUSER; genuine `CShell::DoBanner` output
("ESHELL 0.01(4011)   CFG=", "Copyright (c) 1998 Symbian Ltd"), the `C:\>`
editor prompt, typed `help` lists the genuine command table (ATTRIB, CHKDSK,
...), `exit` returns 0 from the genuine `E32Main`. ESTART boots and prints its
banner, then panics in the efsrv-stubbed mount path (expected until the real
efsrv.dll lands from the parallel F32 build). VGA screendump:
`/tmp/opencode/eshell_shell.ppm`.
