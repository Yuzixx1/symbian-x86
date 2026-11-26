# P5 provenance result — static audit PASS

**Date:** 2026-11-25. **Tool:** `tools/r1_source_provenance_audit.py` (static, read-only; no build, no QEMU, no commits). `kernelhwsrv/` read-only.

**RESULT: PASS** — genuine 8/8 sourced, 8 artifacts, shims retired 4/6, byte-ratio N/A.

- Genuine components: 8/8 sourced (EUSER, EFILE, EFSRV, EWSRV, ESTART, ESHELL, HAL, DOMAINMGR) from 445 khw source files (6.6M).
- Genuine artifacts: 8 observed, 1.1M total (`genuine/build/` + EUSER.DLL). Glue: 201 files, 2.5M, 64298 lines (A 289.8K, B 554.5K).
- Largest glue: `variant/kern_shell.cpp` 120.1K/2830L, `variant/e32load.cpp` 68.6K/1568L, `variant/fat32.cpp` 66.1K/2022L, `variant/qemu_usb.cpp` 30.1K/818L.
- Shims: RETIRED = portfilesrv, svcmgr, winsrv, euser static shim (headers kept); LIVE = r1_fs_* (15 files, 6 linked); KEEP_AUTHORED = netstack.
- Limitation: genuine/glue byte ratio is N/A — needs build artifacts (`build/obj/*.o`, `kernel.elf`, link map) to attribute bytes to translation units; this tool never builds and does not treat build output as provenance truth. Source-byte footprints reported instead.

**Doc drift:** [`GENUINE_PROGRESS.md`](GENUINE_PROGRESS.md) §1 and [`REMAINING_SHIMS_AUDIT.md`](REMAINING_SHIMS_AUDIT.md) still claim no shim is fully retired, while the tree shows portfilesrv/svcmgr/winsrv retired (0 files, 0 linked) and r1_fs_* still live and built (`fs/r1_fs_channel.cpp/.h` +4). Docs need a §1 correction.

**Correction — 2026-11-25 (glue metrics re-measured).** The glue line above is stale.
Re-running `python3 tools/r1_source_provenance_audit.py --json` (same static read-only
audit, no build, no QEMU) now reports **204 files, 2,704,756 B (2.6M), 66,329 lines**
against the 201 files / 2.5M / 64,298 recorded, with the A/B split unchanged
(A 296,765 B = 289.8K, B 567,854 B = 554.5K). Everything else on that line still
holds: 445 khw source files (6.6M), 8/8 components sourced, 8 artifacts. The "largest glue" list is still correct as measured —
`variant/kern_shell.cpp` 122,979 B/2830L, `variant/e32load.cpp` 70,270/1568,
`variant/fat32.cpp` 67,726/2022, `variant/qemu_usb.cpp` 30,813/818. Artifact size worth
pinning because other docs disagree: `genuine/build/EFILE.E32` is **273,044 B**, not
273,068 (`GENUINE_PROGRESS.md` §1/§2 quote the stale figure); `genuine/build/EFSRV.DLL`
is 263,580 B. Shims, byte-ratio N/A and the documented limitation are unchanged.

**Correction — 2026-11-26 (r1_fs channel no longer live).** The `fs/r1_fs_channel.cpp/.h` (+4) claim on line 13 is superseded: S3 deleted all 12 channel paths (`fs/r1_fs_channel.cpp`, `fs/r1_fs_channel.h`, `fs/r1_fs_channel_eka.cpp`, `fs/r1_fs_channel_eka.h`, `fs/r1_fs_channel_state.h`, `fs/r1_fs_wire_logic.h`, `variant/r1_fs_channel.cpp`, `variant/r1_fs_channel.h`, and the 4 `tests/r1_fs_channel/` files) plus the now-empty `tests/r1_fs_channel/` directory, for 16 total `D` entries together with S1 (`fs/r1_fs_core.mmp`, `services/r1_service_host.cpp/.h`, `services/r1_service_state.h`); both files are absent from the tree, `tools/r1_shim_residue_sweep.py` re-run gives **RESULT: PASS (0 residue, 43 allowed comments)** and the 13-row gate is 13/13 PASS. Line 10's "LIVE = r1_fs_* (15 files, 6 linked)" is stale for the same reason: only the 7 genuine `fs/r1_fs_*` keepers (`_core.cpp/.h`, `_server.cpp/.h`, `_backend.h`, `_fat.cpp/.h`) plus `tests/r1_fs_core/` survive.

**Correction — 2026-11-26 (later — 15-row gate).** The 13-row figure in the correction above is superseded: the strict gate is **15/15 PASS** (`/tmp/opencode/final27_gate.log`, `OVERALL: PASS  (15 pass, 0 skip, 0 fail/not-run)`, `FINAL27_RC=0`), the two rows beyond the earlier 13 being `g12_negative` (`tools/r1_g12_negative_test.py`) and `hal_probe` (`tools/r1_genuine_hal_test.py`). The provenance verdict itself still holds — RESULT: PASS (genuine 8/8 sourced, 8 artifacts, shims retired 4/6), `r1_shim_residue_sweep.py` PASS (0 residue, 43 allowed comments) — so only the row count changes here; `kernelhwsrv/` untouched.
