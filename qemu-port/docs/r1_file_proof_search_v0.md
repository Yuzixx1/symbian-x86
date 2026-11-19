# R1-FILE-PROOF-SEARCH/v0 — search for an unchanged zero-capability file app

Status: **research only, NOT a selection, nothing built or run.**
Date: 2026-10-21. Port base `fa62323eea7bc96cefd666e40f5189567731bd44`.
Local upstream `kernelhwsrv/` `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean.
Owned artifact: this report only (detached worktree
`/tmp/opencode/r1-file-proof-search`; the `kernelhwsrv` symlink there is a
read-only reference, not a deliverable).

Follows `r1_proof_source_research_v0.md`, which found no simple unchanged file
app inside `kernelhwsrv/`. This report widens the search to other SymbianSource
packages and re-checks `kernelhwsrv/` mechanically.

## 1. Result in one paragraph

No unchanged candidate found so far is simpler than FHServer **and** covers the
R1 file assertions (create/write/close/reopen/read, EOF, missing file,
persistence). Every zero-capability app that really exercises RFile also drags
in one of: `RTest` console, a test framework (TEF), a large fixture set, a
second library stack (bafl, ezlib, FShell iocli), or a client/server pair. The
closest is `T_RscPanic.exe` (ossrv); it is a candidate for a **later** file
checkpoint, not a simpler R1 proof. Recommendation: do not widen R1 to one of
these. Keep the proof as `t_cactw.exe` (scheduler, provisional) plus a clearly
labelled supplemental file fixture, or stage FHServer later (section 6).

## 2. Method, revisions and limits

Python scans of `.mmp` files: `TARGETTYPE exe`, library list contains `efsrv`,
capability is explicit `NONE` or absent, with `#include`d `.mmh` capability
macros followed. Sources were then grepped for `RFile` plus
Write/Replace/Create plus Read. Greps are heuristics, not dependency analysis:
they miss macro-hidden calls and count calls in code never reached.

Local checkout: `kernelhwsrv` at the revision above. 151 EXE MMPs are
zero/default-cap after following capability includes. A first pass that did
**not** follow includes listed 24 such MMPs linking `efsrv`; it wrongly
included `t_nandbm` (see 3.2), so treat that count as an upper bound.

Shallow clones from `https://github.com/SymbianSource/` taken 2026-10-21 in
`/tmp/opencode/r1-fps/` (not part of the repository):

| Package | HEAD | Notes |
|---|---|---|
| `oss.FCL.sf.os.ossrv` | `1e9520caca186c601dd9768449b86bc72be39a22` | 229 zero/default-cap EXE MMPs |
| `oss.FCL.sf.os.fshell` | `30d5fcf516eed5ae7a5379a6a469c75f93a1345b` | 159 MMPs |
| `oss.FCL.sf.os.ostesttools` | `caed8dc5970cf0ddcb9937bc50cfbf710ec8e123` | 105 MMPs |
| `oss.FCL.interim.SFTest.SmokeTest` | `54d7f1bd9ee7b8951bbd1b769b93f3fd46e66077` | 89 MMPs |
| `oss.FCL.sf.app.testtools` | `0499facfa55a32dc32d671639fe239a8a7fac9ff` | 16 MMPs |
| `oss.FCL.sf.app.utils` | `4243375b2ad9b9e79dbfa82ec81872c0cf824d97` | 0 MMPs |

The org lists 216 repositories (GitHub web UI). The REST API returned 100 + 100
+ 16 on three pages; I inspected only the six above. **Not searched:** the other
`sf.os.*` packages (persistentdata, security, textandloc, mm*, ...), the SDK /
`API_REF` repos, and `oss.FCL.interim.QEMU` beyond its README. Absence of a
better candidate there is therefore not established.

Licensing/provenance: these are the public SymbianSource mirrors (EPL headers
in sampled files). I did not review each file's licence; do not redistribute
any binary built from them without that review.

## 3. Candidates, with evidence

"Cap" is the effective capability after following MMH includes. All paths are
relative to the package named.

### 3.1 `t_setkey.exe` (kernelhwsrv) — rejected

`kerneltest/e32test/group/t_setkey.mmp` (one source, `pccd/t_setkey.cpp`, 506
lines), EUSER+EFSRV, no capability line. It writes and reads files
(`:327-380`, `:454`) but: aborts unless a real SD-card key replaces the all-zero
`testDeviceKeyRawData` (`:200-205`); targets fixed drives F: and G:
(`:32-33`); uses `RemountDrive`, `NotifyChange`, a worker thread that writes
forever (`:137-193`); ends with `RTest::Getch` (`:109`). It never checks EOF or
a missing file. Not usable.

### 3.2 `t_nandbm.exe`, `t_logtofile.exe`, `t_kill.exe`, `t_sobject.exe`, `t_suser.exe`, `t_corruptlog.exe`

From the scan and the earlier report:

- `t_nandbm` calls `TBusLocalDrive` directly, bypassing the file server;
  its MMP includes `f32caps.mmh` (the file-server capability set), so it is
  **not** zero-capability despite the scan listing it by default.
- `t_logtofile` needs the external `d_logtofile.ldd`; `t_kill` leaves an async
  write outstanding; `t_sobject`/`t_suser` test kernel objects and processes,
  not file content; `t_corruptlog` only opens files (4 `Open` calls, no
  write/read).

None gives a create/write/close/reopen/read check.

### 3.3 `t_ioread.exe` (fshell) — rejected

`libraries/iosrv/tsrc/t_ioread.mmp`: one source, 162 lines, no capability line.
It does `RFile::Open/Size/Read` (`t_ioread.cpp:85-94`) but is a FShell command:
it derives `CCommandBase`, links `iocli.lib`, and needs the FShell IO server
and a harness to start it. It only **reads** and compares with the IO-server
view of the same file; no write, no missing-file case. Fails on scope.

### 3.4 `base64`, `fzip`, `hash`, others in `fshell/commands` — rejected

Each does read and write, but their MMPs use `FSHELL_CAP_MMP_NORMAL` or
`_MAX`, which `build/common/common.mmh:110-111,154-155` define as `all -tcb` /
`all` (or a long list that includes ReadUserData/WriteUserData). Not
zero-capability. All also link `iocli.lib`.

### 3.5 `ezlibtest.exe` (ossrv) — rejected

`compressionlibs/ziplib/test/rtest/group/ezlibtest.mmp`: one source, 1814 lines,
no capability line, links `euser ezlib efsrv hal`. File use is incidental and
its inputs are `z:\test\png\00000720.png` (`ezlibtest.cpp:1366-1372`), a ROM
fixture; the point is zlib behaviour. Requires `ezlib.dll`, `hal`, RTest. Not
a file-persistence check.

### 3.6 `StdioServer.exe`, `tlargefile.exe` (ossrv) — rejected

Zero capability, but `StdioServer` (10 sources, 2110 lines) is the POSIX
backend server and `tlargefile` links `testexecuteutils`/`testexecutelogclient`
(TEF) and `libc`. Each requires a larger runtime than R1 provides.

### 3.7 `gwstestexecute.exe` (ostesttools) — rejected

TEF script engine; links `ws32`, `testexecute*`, `domaincli`, `iniparser`,
three sources, ~4.9k lines. Far outside R1.

### 3.8 `T_RscPanic.exe` (ossrv) — **closest; provisional later checkpoint only**

`lowlevellibsandfws/apputils/tsrc/T_RscPanic.mmp`: one source
(`T_RscPanic.cpp`, 293 lines), `TARGETTYPE EXE`, `UID 0x1000007a`, libraries
`euser efsrv bafl`, no `CAPABILITY` line (default = none by the MMP; confirm in
the built image header). It is wired into `apputils/group/bld.inf:171` and
`BaflTests1.iby:57` (ROM test).

What it does with files (`T_RscPanic.cpp:40-80`, `:235-270`): `RFile::Create`,
`Open`, `Size`, `Read`, two `Write` calls, `BaflUtils::CopyFile/DeleteFile`, and
`SetAtt`, driving `RResourceFile`/`CResourceFile` into panics and leaves.

Why it is **not** simpler than FHServer for R1:

- Its subject is resource-file parsing. The file I/O is setup for those
  checks. The written copy is only consumed indirectly, by the resource
  parser, and the test expects an error (`test(err != KErrNone)`). It does
  not compare bytes read back, assert EOF, or assert a missing-file code from
  `RFile::Open`.
- Needs `bafl.lib` (BaflUtils, `RResourceReader`, `CResourceFile`) on top of
  EUSER/EFSRV: another DLL stack the port would have to supply.
- Inputs are ROM fixtures on `z:\system\data\` (`:190-227`); the port has no
  Z: ROM drive with those resources.
- Uses `RTest` (console) and `__UHEAP_MARK`; spawns a thread to provoke panics
  with `User::SetJustInTime`/`Logon` (`:150-170`, `:278-293`), so it needs the
  same thread/panic semantics as `t_cactw`.
- It would also use `BaflUtils::CopyFile`, which is not a bare RFs/RFile call.

So: an unchanged, zero-capability, one-source file consumer, but with a
heavier dependency set than the R1 slice, and weaker assertions. Treat as a
possible later compatibility checkpoint alongside FHServer.

## 4. Zero-capability scheduler + file together

No candidate found combines active-scheduler behaviour and a file round trip in
a single small unchanged EXE. FHServer does (see prior report) but needs its
client. `t_wsp.exe` (fshell) shows `CActiveScheduler` plus file calls but needs
`esock/insock` and winsock.

## 5. RTest is a shared dependency

Every candidate above (and `t_cactw`, FHServer) uses `RTest`. In
`kernel/eka/euser/us_test.cpp:30-32,109-124,515-519` the console is created via
`Console::NewL` (lazily) and `Getch` reaches it. A proof run therefore depends
on an RTest console route (here: VGA/console output, or a defined no-console
behaviour), not only on RFs/RFile. This was true of the earlier candidates and
should be settled once, by R1-STARTUP / R1-ABI, rather than per app.

## 6. Recommendation for the integrator

1. Do not select any of the apps in section 3 as the unchanged file proof for
   R1; none is simpler than FHServer **and** covers the assertions.
2. Keep `t_cactw.exe` as the provisional unchanged scheduling proof, still
   unbuilt.
3. For file behaviour, either (a) accept a port-authored supplemental fixture,
   labelled as supplemental in all reports, or (b) stage FHServer + t_handshare
   as a later checkpoint, with `T_RscPanic` as a second-tier option. The owner
   decides the scope; this report does not.
4. If a truly unchanged simple file app is still wanted, extend the search to
   the repos listed as not searched in section 2 before asking the owner to
   change R1's scope. Do not assume it exists.

## 7. Acceptance sketch for any later selection (not executed)

Build the original MMP unmodified with the matched toolchain; record package
HEAD, compiler, EUSER/EFSRV/bafl import metadata and hashes; run as an
ordinary process on a disposable FAT32 image; compare file bytes after a
reboot; check handle/segment baseline. Use the `r1_wire_research_v0.md`
vectors for the RFs/RFile slice. Physical-PC claims remain owner-only.

## 8. Verification and handoff

Source and metadata research only. No build, QEMU, hardware, or media write; no
upstream edit; no commit. Commands used: Python MMP/source scans over
`kernelhwsrv/` and the six clones above, `git rev-parse HEAD` per clone, and
reading the cited lines. The clones in `/tmp/opencode/r1-fps/` are scratch, not
deliverables. This report can be integrated by copying this single new file
after review.
