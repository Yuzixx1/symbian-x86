# R1-FIXTURE-BUILD v1 — isolated incremental build and import audit

Date: 2026-10-23. **Build succeeded on the single attempt; never executed.**
Owned deliverable: this report only. No fixture/shared-source edits, commits,
pushes, guest execution, QEMU or hardware. QEMU-ready: **none**.

## 1. Result and limits

The submitted port-authored fixture compiled and linked using byte-verified
copies of the reviewed v1 SDK inputs. Raptor reported **0 errors, 1 warning**;
the warning is the previously known missing `epoc32/tools/checklib`. Compiler
and linker emitted no diagnostics. No first compile/link blocker occurred;
there was no fixture patch or target-build retry.

The image is 11,468 bytes, E32 V-header / PE2 imports, X86, uncompressed, with
UID3/SID `e1a0f001`, VID `70000001`, and **both capability words zero**. Its 72
imports (49 EUSER, 23 EFSRV) all resolve to present, non-ABSENT ordinals in the
hash-pinned v1 providers. Header CRC, UID checksum, import/IAT structure and
relocation extents/counts passed the bounded independent byte checks below.

This is **one incremental fixture build**, not a third cold SDK build, official
SDK equivalence, runtime proof, R1-A acceptance, R1-B or genuine console proof.
**Every mode requires EFSRV binding**, including heap/sched: these are modes of
one linked EXE, not independently linked applications.

## 2. Provenance, storage and isolation

- Port HEAD `fa62323eea7bc96cefd666e40f5189567731bd44`, substantial existing dirty
  main-tree work preserved. Read latest dispatch §10/Common, board, roadmap,
  reconciliation, fixture and clean/repeat build reports.
- Detached report worktree `/tmp/opencode/r1-fixture-build` at that HEAD; actual
  submitted fixture and reports were read from the dirty main workspace, not
  assumed present in old HEAD. Build uses a byte-identical private fixture copy.
- Upstream `kernelhwsrv` HEAD `0c3208650587ac0230aed8a74e9bddb5288023eb`, clean
  before/after; no upstream edits.
- New root **`/home/yuzi/.cache/sybian-r1/r1-fixture-build-v1`** was absent,
  including as a symlink, before provisioning. Available bytes: 270,451,900,416;
  conservative provisioning threshold 4 GiB. `quota -s` showed only tmpfs quotas
  (5443/6297 MiB on the busy tmpfs), no reported home quota. No build allocation
  was placed on that quota-limited tmpfs; Wine sockets remain host-managed.
- Private fresh `wineprefix`, `tmp`, `epoc32`, fixture sources and Raptor/tool
  copies. `wineboot -u` rc 0; `M:` mapped to **this root's** `tools/mingw` before
  build. No mapping policy changes mid-build. No old prefix used or copied.
- Reuse origin: `/home/yuzi/.cache/sybian-r1/r1-build-v1`. Copied tools, Raptor
  subtree, generated SDK headers, release images/import/startup libraries and
  installed tools. No old `epoc32/build` copied. Standard genuine `eexe.lib`
  supplies EXE startup; no local replacement runtime.
- Source build repository identity verified before copying:
  `f458a4ce83f74d603362fe6b71eaa647ccc62fee`, live reviewed D1/D2 diff SHA-256
  `fd504a6009d3a70039bece577b0f2a3082987d56b415f4209a93ba50f289e50d`.
  Only its Raptor subtree is needed/copied; no third SDK rebuild.
- 7,287 regular reused files were hashed at source and verified after copying.
  Full names/sizes/SHA-256: `$R/logs/reused_inputs.json`. Original files were
  rehashed after build and remained unchanged. Prior roots were not cleaned,
  moved or rebuilt; repeat/older scratch were read-only references or untouched.
- Reviewed environment/wrappers changed **only old-root text to new-root text**;
  12 x86 tool wrappers, petran wrapper and env.sh. Two absolute Raptor Python/make
  symlinks repointed to private copies. No tool-byte/source patches or banner
  bypasses. Lists: `logs/provision.json`. Copied wrappers/tools still inherit
  disclosed old compiled-in default strings from earlier work; not SDK provenance
  purification. New runtime paths select private tools.
- Host: gcc/cpp 16.2.1, sed 4.9, Perl 5.42.3, Wine 11.0 Staging; exact outputs,
  kernel/host details, disk/quota and initial git status in `logs/host_preflight.json`.
  Variant: **`port-x86-rfile64-v1`**, actual target `x86_urel.test`.

### Pinned input SHA-256 (live verified)

| Input in reuse origin | SHA-256 |
|---|---|
| `epoc32/release/x86/urel/euser.dll` | `4851fec4b6439bf5263e6f10f7f3010828f84df55df050c710f57077f2dd017b` |
| `epoc32/release/x86/urel/efsrv.dll` | `332d97274a151bb17a18112e728684b98c8462fe8723bfadb1de0d471e372b6d` |
| `epoc32/tools/petran.exe` | `b200bf0878679e6c7d6c20e5a78cb2bb93f16d5b21790a2b66c7dd04a53ec155` |
| `epoc32/include/platform_paths.hrh` | `8a8ebdc3f40804d630def1af4c0c3a68568a79d13eb08d0661d9ba252dd96f31` |
| `epoc32/include/variant/port_x86_rfile64_v1.hrh` | `500889884b3063c1b0712c000d2c62f55e70774cbfda41b51b82f343be277106` |
| `epoc32/tools/variant/variant.cfg` | `2c505b4bb43e4b74e362589ce78f866053e4e9d8283688e47cca321c58e08cd0` |
| `tools/py27/bin/python2.7` | `a19750e5cf60ddc0e0c56b6af395864906338da3e44a1e9a8c1c4c944e7589cd` |
| `tools/sbshost/bin/make` | `33c10ef85671e2624c3d81e1f598cf96f6165cc6957a169f30e4deca84abc7bc` |

Submitted fixture hashes (unchanged in main tree/private copy after build):

| File | SHA-256 |
|---|---|
| `r1a_fixture.cpp` | `33617bc2aef62d9d5fab5102fda0988e36d326261908269ecf8680ad9d8c71e7` |
| `r1a_fixture.mmp` | `364a8b3c3ea2f23226b7b38519d37fb53e835d4b27a35884154cf9a024fb76f0` |
| `bld.inf` | `45168b0e36b71ad77d6c1d74cb2a3da19d43ce5e1a52678c0dab45f81d9a4262` |
| `README.md` | `3c2701db3a0978de2684a9ee250b48fcb741b1fe77f67dcd1db939eb8d6d649b` |

## 3. UID collision preflight

Case-insensitive ASCII `e1a0f001` scan of 6,747 main-repo regular files, including
hidden/ignored files but excluding `.git` and symlink traversal, found exactly:
fixture MMP:22, fixture README:107, reconciliation:344, dispatch:222. No other
identity declaration found. Dedicated grep tool independently found those four.
This is a bounded repo textual scan, **not global UID registration or exhaustive
binary UID collision detection**. Retain coordinator's local-only reservation.
Evidence: `logs/uid_collision_scan.txt`.

Provisioning/logging note: shell `rg` was unavailable; a host logging command
failed before scanning, then the explicit Python byte scan completed before the
build. This was not a compiler/link failure or a target-build retry.

## 4. Exact attempt, argv and warning disposition

`R=/home/yuzi/.cache/sybian-r1/r1-fixture-build-v1`; source `$R/env.sh`:
EPOCROOT `$R/`, SBS_HOME `$R/src/build/sbsv2/raptor`, SBS_GCCX86BIN
`$R/tools/x86bin`, real `/usr/bin/cpp`/`sed`, private PERL5LIB/WINEPREFIX/tmp.

```sh
bash "$R/src/build/sbsv2/raptor/bin/sbs" --toolcheck=off -d -j 1 \
  -f "$R/logs/sbs_fixture.xml" -b "$R/fixture/bld.inf" \
  -c x86_urel.test -p r1a_fixture.mmp
```

2026-10-23 15:10:57–15:10:59 +03:00, rc **0**, one source compile and target
recipe, attempt=1 in XML. Full expanded top-level argv/status/time is
`logs/sbs_fixture.cmd`; exact compiler/link/petran argv and stage statuses are
`logs/sbs_fixture.xml:531-545`. stdout/err archived; stderr empty.

- Compiler: reviewed MinGW g++, `-O2 -Wall -fno-rtti -fno-exceptions -fcheck-new`,
  genuine gcc.h and PRODUCT_INCLUDE variant, `__EXE__`, NDEBUG, SDK include dirs.
  No allocation/TRAP/async/overload warnings. Absence of warnings is not semantic
  proof of leaves, scheduler cleanup, allocation accounting or descriptor safety.
- Link: `-nostdlib`, `--entry=__E32Startup`, forced genuine `eexe.lib`, emitted
  objects response file, copied `euser.lib` and `efsrv.lib`; no fixture runtime.
- Petran: `-sid 0xe1a0f001 -version 10.0 -capability +none -uid1 0x1000007a
  -uid2 0 -uid3 0xe1a0f001 -vid 0x70000001 -smpsafe`, default paging, no compression.
- One Raptor warning: missing `epoc32/tools/checklib`. Did not fabricate/install
  it or suppress warning. Explicit emitted import/provider checks below cover
  ordinal availability, not every check an official checklib might perform.
- No compile/link blocker: **no minimal fixture fix proposed or applied**.

## 5. Emitted bytes and bounded structure checks

Image SHA-256: **`87fde53b240e54ac9aecf4024546b77c91f06a57d83f60c0101c704e0ed6a6a0`**.
Location: `$R/epoc32/release/x86/urel/r1a_fixture.exe`.

| Field | Actual |
|---|---|
| UIDs / UID checksum | `1000007a / 00000000 / e1a0f001`, checksum `fa944545` |
| SID / VID / capabilities | `e1a0f001 / 70000001 / 00000000 00000000` |
| Flags / header | `22004000`, V format 2, PE2, header/code offset `0x9c` |
| Version / CPU | 10.0 / X86; SMP-safe, not debuggable |
| Code offset/size/link base | `0x9c / 0x29c0 / 0x00400000` |
| Initialized data offset/size/base | `0x2a5c / 0x10 / 0x004029c0` |
| BSS | 0 |
| Entry offset / unbound stamp | `0x260`; word at code+entry+12 is **0** |
| Export table | offset `0x2a58`, one in-code export, no holes |
| Heap min/max / stack | `0x1000 / 0x100000 / 0x2000` |
| Imports / IAT | section `0x2a6c`, size `0x50`; code-relative IAT `0x2898` |
| Code relocations | offset `0x2abc`, payload `0x1f4`, 237 nonpadding entries, all type `0x3000` |
| Data relocations | offset `0x2cb8`, payload `0x0c`, 1 entry type `0x3000`; section ends exactly at file end `0x2ccc` |

`petran -v`, `-dump i`, `-dump e` and host `objdump -drC` ran as inspection tools
only, never executing E32. Exact argv/rc and stdout/err: `logs/*.{cmd.json,out,err}`.
Independent `harness/audit_fixture.py` checks little-endian fields, UID CRC16s,
standard reflected CRC32 (initial zero, no final XOR, header CRC word initialized
to c90fdaa2), code/data/export bounds, PE2 name/count/IAT agreement and trailing
IAT terminator, relocation 8+payload extent, block bounds/alignment/kinds/counts,
unique targets and no relocation overlap with entry+12 stamp. Results:
`logs/structure_audit.json`, `structure_audit.stdout`.

Source rules: `kernelhwsrv/userlibandfileserver/fileserver/inc/f32image.h:161-218`
header layout; `:101,107` V/PE2 flags; `:437` relocation payload definition;
`:650-666` UID/header checks. This bounded audit is not a complete loader validator.
Host audit development corrected initial hand-entered field offsets and an
incorrect per-provider IAT-terminator assumption; final byte checks agree with
Petran. These host-script corrections did not change image/source or rerun build.
Zero entry stamp is the immutable prebinding value, **not initialized process
identity**. EXE writable data is permitted by the selected loader profile;
provider DLL data/BSS policy still belongs to loader bootstrap review.

## 6. Every required ordinal and API binding

Both reused providers' image hashes match §2. Parsed actual export dumps
(EUSER 2503 slots, EFSRV 421) and checked every fixture import against emitted
non-ABSENT/nonzero entries and matching upstream definition names. All **72/72
PASS**. Complete per-import symbol/ordinal/export-offset ledger:
`logs/ordinal_audit.{tsv,json}`. No absent-marker callable binding accepted.

| Decorated import identity | Required ordinals (actual IAT order) |
|---|---|
| `efsrv{000a0000}[100039e4].dll` | 44,390,68,93,300,96,97,101,105,109,363,369,373,375,216,257,261,262,263,264,384,386,387 |
| `euser{000a0000}[100039e5].dll` | 85,196,203,207,208,424,425,426,428,430,583,585,604,609,613,622,641,644,646,649,654,669,2123,2126,782,2190,2191,881,885,886,887,890,915,953,959,966,968,988,2128,1088,1089,1169,2177,1360,1472,1718,1806,1822,1826 |

Definition provenance: `kernelhwsrv/kernel/eka/bx86gcc/euseru.def` and
`kernelhwsrv/userlibandfileserver/fileserver/eabi/efsrvu.def`; no bx86 MSVC mixing.
Examples of actual selected API bindings:

- EUSER 583 `User::CommandLine(TDes16&)`, 622 `CommandLineLength()`, 915
  `RDebug::Printf(char const*,...)`; genuine command/debug overloads, no PortCon.
- EUSER 196 CTrapCleanup::New, 2190/2191 TTrap::Trap/UnTrap, 1360 genuine
  UserHeap::SetupThreadHeap and 585 User::InitProcess from SDK startup path.
- EFSRV 369 `RFile64::Write(TInt64,...)`, 384 `Read(TInt64,...)`, 375
  `SetSize(TInt64)`, 386 `Seek(...,TInt64&)`, 387 `Size(TInt64&)`; not TInt
  overloads or disabled-API stubs. 363/373 are real Open/Create in pinned v1.
- EFSRV 262 selects positioned async RFile Read; no cancel coverage inferred.

Source `r1a_fixture.cpp:323-350` consumes **argument string only**, length <=64,
trims and compares modes; launcher must not prepend program name.
`:362-375` emits entry/debug markers then trap cleanup and dispatch. Build/map
and disassembly confirm API references, **not correct command forwarding or
actual serial trace capture**. Those remain future loader/debug integration tests.

## 7. Archive, preservation and handoff

Scratch retained at `$R`; provision script retained in detached worktree as
`provision_fixture.py`. New private helpers are not repository implementation.
`logs/final_hashes.sha256` inventories fixture, harness, logs and release files;
`logs/preservation_check.txt` records original 7,287-file rehash, unchanged fixture
and clean upstream. Compiler response/dependency/make files remain in private
`epoc32/build/fixture`; no prior root overwritten or cleaned.

- **Card:** R1-FIXTURE-BUILD / base and variant in §2; no ABI frozen.
- **Changed repo paths:** only new `qemu-port/docs/r1_fixture_build_v1.md`.
  Fixture README remains source-owner's original uncompiled/unrun text; this
  report records later compile status without changing their file.
- **Commands/results:** one Raptor attempt rc0 (§4), Petran/objdump inspection
  rc0, final independent byte/import audit PASS, preservation checks PASS.
- **Blockers/minimal fix:** no compile/link blocker; no fixture fix requested.
  Known missing-checklib warning remains disclosed. Local UID reservation only.
- **Still blocked:** genuine G loader/startup/TLS/heap and EFSRV binding,
  file service/security/staging, timer/request completion, RDebug capture,
  writable disposable media and two-boot persistence. No behavior validated.
- **Manual step:** none run/edited; fixture README's proposed procedure still
  awaits reviewed loader/service integration and allocated serialized runtime slot.
- **Next owner:** coordinator reviews emitted artifact and ledger, routes exact
  image/provider identities to service/loader owners; source owner may later
  update README status. No additional build/retry or guest execution presumed.
