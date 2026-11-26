# PLAN SURVEY — importing MISSING genuine sources (UIKON/EIKON, GDI/FBSRV, ESOCK) from external upstream

Read-only survey. No downloads, no web, no edits outside this file, no QEMU,
no commit/push, no `make clean`. `kernelhwsrv/` untouched (read-only).
Standing rule (AGENTS.md): use as much REAL Symbian code as possible; own code
only as x86/PC glue, never as a replacement for real Symbian logic.

## Local sources already checked

- `kernelhwsrv/` (git repo, single commit `0c32086`): has genuine EUSER
  (`kernel/eka/euser/`), text EWSRV (`kernel/eka/ewsrv/`), edisp LDD,
  domainmgr, fileserver (F32/EFSRV/EFILE/estart/etshell). **No** UIKON, EIKON,
  GDI/FBSRV, font/bitmap server, graphical winsrv, ESOCK, commsdb, or socket
  client API — `git log --all` over `*uikon*/*eikon*/*fbsrv*/*esock*` is empty;
  EUSER.DLL's 2503 ordinals (`kernel/eka/bx86gcc/euseru.def`) contain zero
  socket/graphics-client exports.
- `sdk/` and `sdk.zip`: tools + hello/args/fileview examples only; 0 hits for
  uikon/eikon/fbsrv/esock.
- `symbian-x86-archive-56fbaff.tar.gz` (2.4 MB): git-source snapshot of this
  repo only (qemu-port, boot, skills); 0 hits for uikon/eikon/fbsrv/gdi/esock.
- `qemu-port/genuine/`, `ports/`, `user/`, `variant/`: authored port code +
  genuine builds of EWSRV/EFILE/EFSRV/HAL/DM; no vendored UIKON/FBSRV/ESOCK.
- Prior decisions: `docs/PLAN_SURVEY_UIKON_GDI.md` (verdict LOW — sources absent
  locally), `docs/PLAN_SURVEY_ESOCK.md` (genuine ESOCK impossible from this
  tree; authored `!SocketServer` stays), `docs/REMAINING_SHIMS_AUDIT.md` §2
  (netstack = explicit authored exception), `docs/GENUINE_PROGRESS.md` §4.7
  (networking stays authored).

## Candidate upstream sources (not fetched)

- Symbian Foundation open-source release (Symbian^3 / "Symbian" 2010 EPL
  drop) — the canonical upstream containing `uikon/`, `eikon/`, `gdi/`,
  `fbserv/`, `esock/`, `commsdb/`, `winserv` graphical sources; mirrored on
  the Symbian Foundation site and multiple public mirrors.
- Public GitHub mirrors of the Symbian open-source tree (community forks of
  the SF release, e.g. petran79/symbian and similar) — same content, easier
  sparse checkout.
- Nokia-internal-era source drops referenced in `kernel/eka/release_old.txt`
  lineage — provenance murkier; prefer the EPL-licensed SF release.

## Provenance/licensing risks

- EPL-1.0 covers the SF release, but UIKON/EIKON/GDI/FBSRV/ESOCK in that drop
  were under mixed Nokia/Symbian copyrights; verify per-directory license
  headers before import. Some components (fonts, `.mbm` glyph data, some
  winsrv bits) may be proprietary — treat as unverifiable, do not import.
- Importing upstream code into this repo changes provenance of the "genuine"
  claim: imported files must be marked (e.g. `genuine/imported/<area>/` +
  upstream commit hash in BUILD_MANIFEST.json) so audits can distinguish
  kernelhwsrv-sourced vs externally-imported genuine code.
- ABI mismatch risk is the real technical hazard, not license: upstream
  sources target ARM/EKA1-EKA2 toolchains (RVCT/Metrowerks), not the x86 gcc
  path already proven here; expect porting patches confined to patched copies,
  never edits to imported originals.
- ESOCK additionally needs commsdb + protocol modules + new EUSER client
  ordinals — but EUSER.DLL's 2503-ordinal table is frozen (kernelhwsrv
  read-only), so genuine ESOCK client classes cannot be imported as EUSER
  exports; they would have to live inside one statically-linked image.

## Build integration steps

1. Vendor imported sources under `qemu-port/genuine/imported/<area>/` (originals
   pristine) with provenance notes; patched copies under `.../patched/` per the
   existing EWSRV pattern (`genuine/ewsrv/patched/`).
2. Extend `tools/genuine_build.py` / `genuine/build_efile.py`-style per-area
   build scripts with the proven flag set: `-m32 -freg-struct-return
   -fshort-wchar -mpreferred-stack-boundary=2` + `__GCC32__/__X86__/__EPOC32__/
   _UNICODE`; emit PE2 via `tools/elf2e32.py` with real imports.
3. Loader constraints (measured): 512 import slots/image
   (`variant/e32load.cpp:414,533`), **one DLL dependency per image**
   (`ELoadLibrary` = KErrNotSupported), 512 KiB image/DLL buffers
   (`e32load.cpp:660,1265`) — a genuine UIKON+FBSRV stack must therefore be
   **statically compiled into a single PE2 E32**, not linked as separate DLLs;
   `genuine/pack_multi.py` exists for multi-image packing. A full desktop stack
   will exceed the 512 KiB buffer and need that cap raised first.
4. Server-name plumbing: fix cross-process `!`-name registration
   (DObjectCon/ProtServ) so imported CServer2 servers publish names — already
   the known B1 blocker for EWSRV clients, prerequisite for UIKON/FBSRV/ESOCK
   sessions.
5. Gate each import with a `tools/r1_genuine_<area>_test.py` mirroring
   `r1_genuine_ewsrv_test.py` before touching the boot chain.

## Verdict

Local sources contain **none** of UIKON/EIKON, GDI/FBSRV, or ESOCK — external
import is the only route to genuine code for these, and it is feasible only
as single-image static PE2 builds within the 512-slot/one-DLL/512 KiB loader
limits (raise the size cap, fix `!`-name registration first). ESOCK is the
weakest candidate: its EUSER client-ordinal dependency is impossible without
editing read-only kernelhwsrv, so even imported ESOCK sources can only serve
as one statically-linked server behind authored client ABI — the authored
`!SocketServer` decision in `PLAN_SURVEY_ESOCK.md` stands. UIKON/GDI on top
of the working genuine EWSRV+EDISP core is the highest-value import target.
