# EFSRV unblock — read-only on-disk check

Date: 2026-11-24. Read-only audit taken while two agents are actively editing /
rebuilding. Nothing in `kernelhwsrv/` was touched; no builds, QEMU runs, or
writes other than this file were performed. **The retire test was NOT run.**

## Goal

Determine (a) whether the F32CLIENT name-mismatch fix landed — a genuine UTF-16
`!FileServer` literal compiled with `-fshort-wchar` and rebuilt — and (b) whether
a *fresh* log shows `F32CLIENT: IPC round trip OK` at least once against the
**FULL genuine EFILE**, not the EFSRVMIN/`R1FileServer` shim stand-in.

## State table

| File | mtime (local) | Evidence |
|---|---|---|
| `genuine/f32_probe/fsclient.cpp` | 2026-11-24 02:22:44 | UTF-16 fix present: `struct GenuineLitC16 { unsigned iTypeLength; wchar_t iBuf[12]; }; static const GenuineLitC16 KFileServerName = { 11, L"!FileServer" };` — converts to a `GenuineTBuf8` (EBuf `0x30000000\|11`) then `exec4(128, …)` + `ipc_call(EConnect)`. `ipc_connect("R1FileServer")` remains only as fallback. |
| `genuine/f32_probe/build_probe.sh` | 2026-11-24 02:18:51 | Builds `fsclient.o` with `gcc -m32 -fshort-wchar -c …`; ELF→E32 via `elf2e32.py … --euser-import 915`. |
| `genuine/f32_probe/F32CLIENT.E32` | 2026-11-24 02:22:50 | Rebuilt binary; contains UTF-16-LE `!FileServer` at file offset `0x11ec`, NO ASCII `!FileServer`, NO UTF-16 `R1FileServer`. Consistent with the fix. |
| `genuine/f32_probe/F32CLIENT.elf`, `.o` files | 2026-11-24 02:22:50 | Same rebuild round. |
| `genuine/build/EFILE.E32` | 2026-11-24 01:59:54 | FULL genuine EFILE (273036 bytes). Post-fix prove logs show `File server thread registered: 1` and `EFILE: port-shim: 0` — server name binding no longer falls back. |
| `tools/r1_genuine_retire_test.py` | 2026-11-24 01:49:22 | Requires `EFSRV_CLIENT_MARKERS = [b"F32CLIENT: IPC round trip OK"]` (line 85) as a hard PASS; also requires `EFILE: port-shim` count == 0 (line 477+), flagged `UTF-16 _LIT fix not landed` otherwise. F32CLIENT path expected at `genuine/f32_probe/F32CLIENT.E32` (line 53). |
| `/tmp/opencode/efile_prove_*.log` (after_fix … final) | 01:53:53 → 02:00:19 | All report `F32CLIENT: IPC round trip OK: 0`, `File server thread registered: 1`, `EFILE: port-shim: 0` (dbg5/dbg6: port-shim 4). These are against the FULL EFILE that was rebuilt at 01:59:54. |
| `/tmp/opencode/fallback_efile_genuine.log` | 2026-11-24 02:23:02 | Overwritten by a newer, **incomplete** run: ends mid-EFILE-load (`E32LoadDll: E32Validate r=0 … expCount=2503`) with `qemu-system-i386: terminating on signal 15`. No F32CLIENT client was spawned; no round-trip marker. |
| `/tmp/opencode/r1_retire_after_fix.log`, `verify_retire_full_final.log` | 02:02:54 / 01:51:13 | Genuine EFILE auto-launch chain PASS (`GenuineBoot: EFILE.E32 r=0`, `File server thread registered`, port-shim 0) but `[FAIL] F32CLIENT: IPC round trip OK`. |
| `/tmp/opencode/r1_genuine_efsrv.log` | 2026-11-24 01:46:18 | The ONLY log with actual `F32CLIENT: IPC round trip OK` lines (×2). It is **pre-fix** and shim-based: `GENUINEFS: starting`, no genuine-EFILE launch marker ⇒ this is the EFSRVMIN stand-in path, NOT FULL EFILE. Does not satisfy the proof requirement. |

`git status --short` (qemu-port): many modified/untracked port files; `genuine/`
as a whole is untracked, so the fix is not committed. No worktree changes were
made by this audit.

## Verdict

**INCONCLUSIVE — fix LANDED at source/binary level, proof still PENDING.**

- (a) Fix landed: **YES.** UTF-16 `!FileServer` TLitC16 literal + TBuf8/Exec-128
  path is in `fsclient.cpp`, compiled with `-fshort-wchar`, and `F32CLIENT.E32`
  was rebuilt at 02:22:50 containing the UTF-16 literal. Genuine EFILE binds the
  protected name (port-shim 0).
- (b) Fresh proof against FULL EFILE: **NOT DEMONSTRATED.** Every post-fix
  complete log reports round-trip `0`; the latest fresh log (02:23:02) was killed
  by signal 15 before the client ran. The only passing round-trip log is the
  pre-fix shim (`GENUINEFS`) path and must not count.

Two agents are mid-flight (source edited 02:22:44, binaries rebuilt 02:22:50,
fresh QEMU run started 02:23:02 then interrupted), so the on-disk state is a
moving target. Do not treat any of the above `0` counts as final until a fresh
run completes.

## Exact next verification step

1. Let the in-flight agents finish; confirm the newest `F32CLIENT.E32` mtime is
   >= that of `fsclient.cpp` and that `-fshort-wchar` + UTF-16 `!FileServer`
   are still present (avoid racing a half-written rebuild).
2. Run one **complete** fallback/efsrv run (do not terminate it early) and read
   the serial log. Require, simultaneously:
   - `GenuineBoot: EFILE.E32 r=0` / `EFILE: genuine file server starting` /
     `File server thread registered` (FULL genuine EFILE, not `GENUINEFS`,
     `EFSRVMIN`, or `R1FileServer`),
   - `EFILE: port-shim` count == 0,
   - `F32CLIENT: IPC round trip OK` count >= 1.
3. Only then run the official gate `python3 tools/r1_genuine_retire_test.py`
   and require its EFSRV section to report `[PASS] F32CLIENT: IPC round trip OK`
   (it was `[FAIL]` in `r1_retire_after_fix.log`).
