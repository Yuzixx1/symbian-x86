# R1-BUILD-REPEAT: second cold build vs v1

Card: R1-BUILD-REPEAT (`docs/r1_next_wave.md` §5). Owner of this file only.
Date: 2026-10-22. Status: **repeat built, no runtime test, no official-SDK claim.**

## 1. Result in one paragraph

A second cold build, with a fresh Wine prefix, fresh `epoc32` and fresh temp
dirs, completed on the first attempt (all stages rc 0, no dlltool failure, no
retries). It is **not raw byte-identical** to v1: all 20 release files differ.
The differences are explained by three things and nothing else that I found:

1. **Build-root string length.** The repeat root `/home/yuzi/.cache/sybian-r1/r1-build-v1-repeat`
   is 7 bytes longer than v1's `.../r1-build-v1`. Absolute source paths are
   embedded in 4 euser/e32test/f32test objects (and in 3 petran objects) via
   `__FILE__`/assert strings, which shifts later offsets.
2. **E32 header timestamp and CRC**, and PE timestamp/checksum for `petran.exe`.
3. **Root-derived names** in import libs, maps and `.sym` files.

I proved (1) by experiment: recompiling the 4 affected sources with the **v1**
root as the source path and relinking gives `euser.dll`, `t_cactw.exe` and
`fhserver.exe` that are identical to v1 after masking only the E32 header CRC
and timestamp (0 residual bytes). Exports, imports, headers and the four
RFile64 bodies are identical. **A future repeat should reuse the same root path
string (or an equal-length one) to expect raw-identical images.** This remains a
single repeat, so it is not a statistical reproducibility claim.

## 2. Provenance and constraints kept

- Repeat root: `/home/yuzi/.cache/sybian-r1/r1-build-v1-repeat` (did not exist
  before; v1 and everything under `/tmp/opencode` left untouched).
- Inputs: same reviewed v1 inputs. Build repo cloned from
  `/tmp/opencode/r1-build-repro/src/build` at `f458a4ce83f74d603362fe6b71eaa647ccc62fee`
  plus the D1/D2 patch (live diff sha `fd504a60…`). Variant `port-x86-rfile64-v1`
  (variant hrh sha `50088988…`, variant.cfg `2c505b4b…`, identical to v1).
  `kernelhwsrv` read-only; `git status` 0 lines before and after.
- Harness: copied from v1, only the root string changed in 3 files
  (`logs/harness_substitution.diff`). M: dlltool mapping kept as in v1
  (`m:` symlinked to the repeat `tools/mingw`, dlltool wrapper regenerated with the
  new root). Mapping policy not changed mid-build.
- Pinned tools reused (py27, make, bash, shim): all 4 hashes match v1. MinGW
  re-unzipped, `diff -rq` identical to v1. Target-generated headers and objects were
  **not** reused.
- Toolchain: host gcc/cpp 16.2.1, sed 4.9, perl 5.42.3, wine-11.0 (Staging),
  `sbs --toolcheck=off -d`.
- No patches added, no retries, no E32/QEMU/hardware, no commits, no system
  installs, no upstream edits.
- Footprint of the repeat root: 2.7 GB. Repeat `$R/tmp` stayed empty.

Caveat: `provision.sh` stdout was lost (its `tee` target `logs/` did not yet
exist). `logs/provision_note.txt` records the summary; the resulting state was
re-verified by `preflight pre` (31 ok) and by hash checks, not by that stdout.

All scratch artifacts: `/home/yuzi/.cache/sybian-r1/r1-build-v1-repeat/{logs,harness}`
(compare outputs in `logs/cmp/`, diagnostics in `logs/diag_*`).

## 3. Stages

All rc 0 on first try; per-stage logs `logs/sbs_<stage>.{cmd,out,err,xml}`.

| Stage | Result |
|---|---|
| preflight pre | 31 ok, rc 0 |
| raptor host utils | rc 0 (`logs/raptor_util.log`) |
| export_eka, export_f32 | rc 0 (`f32file64.h` 6406 B, `.inl` 25283 B present) |
| genexec | rc 0; hashes `169ca7a9…`/`1610cda7…`/`b645f308…` = v1 |
| petran (20 compiles, link) | 0 diagnostic bytes, rc 0 |
| 24 perl tools | installed, hashes identical to v1 |
| euser, hal, efsrv, t_cactw, fhserver | rc 0 (14:24:11 to 14:24:59 +03:00) |
| preflight post | 27 ok, rc 0 |
| `check_rfile64.sh` | rc 0 (four RFile64 bodies 80 B, 0 Panic, 1 IPC call) |

## 4. Raw hash comparison (first 12 hex)

| File | v1 | repeat | Raw equal |
|---|---|---|---|
| euser.dll | 4851fec4b643 | 3ae2581828d9 | no |
| efsrv.dll | 332d97274a15 | aca30d957773 | no |
| t_cactw.exe | f1d53fbaaac0 | 3009b364dbc5 | no |
| fhserver.exe | 717e040762d7 | 082025b191e9 | no |
| rpipe.dll | cb907021ad3a | cb2661a506d4 | no |
| the 5 urel `.lib`, 5 `.map`, 5 `.sym` | see `logs/release_hashes.sha256` vs `logs/v1_release_hashes_live.sha256` | | no (all 20 differ) |

v1 live hashes re-checked after the build and unchanged. Directory-level result
(`logs/cmp/tree_compare.txt`): `epoc32/include` 316/316 identical, `data` 8/8,
`rom` 42/42, `tools` 36/38 identical (differences: `petran`, `petran.exe`, see §7).
`epoc32/release`: v1 84 files, repeat 88; the 4 extra repeat files are `.def`
files (`eabi/econs.def`, `marm/econs.def`, `marm/eloclu.def`, `wins/eloclu.def`)
that exist in v1's `quarantine_partial_euser` rather than the v1 release tree.
The two I compared there were byte-identical to the repeat's; I did not compare
all four, so this is a v1-recovery artifact by inference.

## 5. Normalization rules (what was masked, and by format/offset)

Only these masks were applied; nothing in code, imports, exports or config was masked.

| Format | Mask | Why |
|---|---|---|
| E32 image (euser/efsrv/rpipe/t_cactw/fhserver) | header CRC, bytes 20–23; Time Stamp, bytes 36–43 | build time and CRC over content |
| PE (petran.exe) | `pe+8` timestamp, `pe+88` checksum (file offsets 0x88–0x89, 0xd8–0xda) | build time and checksum |
| ar archives (urel `.lib`) | per-member mtime field | archive mtime |
| import `.lib` / `.map` | root string `/home/yuzi/.cache/sybian-r1/r1-build-v1(-repeat)` → `ROOT`, `r1_build_v1_repeat` → `r1_build_v1`, sbs config dir `c_<16 hex>` → `c_HASH`, dlltool member-name prefix | path-derived text |

Normalization is not applied to code bytes. Where path length changes layout,
I report the difference as **explained, not masked**.

### 5.1 Objects (126 vs 126)

122 byte-identical. Differing: `heap_hybrid.o`, `debugfunction.o`, `t_cactw.o`,
`handshare.o`. Each embeds the absolute source path (assert/`__FILE__` strings in
`.rdata`). `heap_hybrid.o` has 0 disassembly difference. `debugfunction.o` has a
jump-table offset change (`jmp *0x74` vs `*0x7c`) plus relocation moves.
`t_cactw.o` and `handshare.o` have `.rdata` +16 and +32 B. Compile command lines
are identical modulo the root.

### 5.2 Diagnostic proof (equal-path experiment)

In `logs/diag_equal_path_objs` I recompiled the 4 sources from the repeat tree
with the v1-root source path: all 4 objects are **byte-identical to v1**. Relinking
and petran-ing with them (`logs/diag_relink`, `logs/cmp/diag_relink_vs_v1.txt`):
`euser.dll` (412104 B), `t_cactw.exe` (12892 B), `fhserver.exe` (17820 B) are
identical to v1 after the E32 header mask, 0 residual bytes. These diagnostics ran
outside the build outputs and did not alter them.

### 5.3 Images in the real repeat (`logs/cmp/e32_header_masked.txt`)

| Image | Size v1/repeat | After header mask |
|---|---|---|
| efsrv.dll | 71088/71088 | identical, 0 residual |
| rpipe.dll | 6196/6196 | identical, 0 residual |
| euser.dll | 412104/412104 | 287 residual bytes: 2 path strings (`sybian-r1`, at 0x4dc12 and 0x4dc92 in both) plus jump-table address ripple |
| t_cactw.exe | 12892/12908 | +16 B (`.rdata` growth) |
| fhserver.exe | 17820/17852 | +32 B (`.rdata` growth) |

`t_cactw.exe` and `fhserver.exe` each carry one UTF-16LE path string (offsets
0x22cc and 0x3bb4 in the repeat); the other images carry none.

## 6. Exports, imports, headers, RFile64

- `petran -dump e` and `-dump i` output for `euser.dll` and `efsrv.dll` identical to v1
  (euser 2503 ordinals, efsrv 421; 10 and 20 ABSENT respectively, same as v1).
  `rpipe.dll` import dump identical.
- `t_cactw.exe` and `fhserver.exe` import dumps differ only in the import-address-table
  offset (the +16/+32 B ripple).
- Headers: installed `epoc32/include` identical (316 files); variant files identical.
- RFile64: `rfile64_inventory.txt` identical to v1; `check_rfile64.sh` rc 0 (four
  bodies 80 B, 0 Panic, 1 IPC call); `efsrv.dll` identical to v1 after the header mask.

## 7. petran correction and result

`petran.exe` (677646 B both) **differs in about 600 bytes**, not only the PE
timestamp and checksum. Breakdown: 5 bytes in the PE header (0x88–0x89 timestamp,
0xd8–0xda checksum), 107 in `.text`, 484 in `.rdata` (embedded source-path strings,
e.g. `.../r1-build-v1/src/build/...farray.h` vs `.../r1-build-v1-repeat/...`), and 4 in
trailing data after the last section. The `.text` differences I spot-checked
(offsets 0x406d, 0x4d3c) are 4-byte operand addresses into `.rdata` that moved by
7 and 8 bytes; I did not inspect all 107. Of the 21 hashes in `petran_out/log/sha256.txt`, 17
are equal; `decode.o`, `e32image.o`, `encode.o` and `petran.exe` differ. This is
**consistent with the path-length cause but I did not run an equal-path experiment
for petran**, so it is not proven the way §5.2 is. (An earlier working note of mine
said timestamp/checksum only; that was wrong.)

## 8. Auxiliary files

- urel `.lib` (`edll`, `edllstub`, `eexe`, `eexe_instrumented_heap`, `eexe_monitor_heap`):
  identical after the ar mtime mask.
- Import libs `lib/{euser,efsrv,hal,rpipe}.lib` and `{000a0000}` copies: raw sizes
  differ (e.g. `hal.lib` 6776 vs 6856, `euser.lib` 2045028 vs 2065082). Cause:
  dlltool member names (`dykt.o` vs `deyct.o`, same member count 8/27/423/2505) and
  root-derived symbols (`__head_<root path>_hal_000a0000__lib`). After normalizing
  member names and the root string, `objdump -h`, `-t` and `-s` dumps of every member
  are **identical** (0 residual lines for all four libs; `logs/cmp/libs/`). Why the
  dlltool prefix differs was not investigated; it is internal to the archive.
- `.map`: after normalizing root, `c_<hash>` and member-name prefix, `efsrv.dll`,
  `rpipe.dll`, `euser.dll` maps have 0 diff lines. `t_cactw.exe` (14 lines) and
  `fhserver.exe` (30 lines) differ in address columns after `.rdata` grows
  (`0xb70` to `0xb80`); I read the first ones and did not classify every line.
- `.sym`: sizes equal for `euser.dll.sym` (298 raw bytes differ: header and ripple);
  other `.sym` sizes differ by 14 to 540 B. Not analyzed byte by byte; attributed to
  the same path-length ripple by inference only.
- The `c_<hash>` build directory name differs between roots (sbs derives it from the
  build path), so object paths differ even though contents match.

## 9. Warnings

sbs: 0 errors; the warning sets equal v1. One `checklib` missing-dependency
warning each for euser/efsrv/t_cactw/fhserver; two `NMAKEFILE… not supported on
Linux` for each of t_cactw/fhserver. euser compiler warnings: 419 = 415
"control reaches end of non-void function" plus 4 signed/unsigned comparison in
`um_spec.cpp`, same classes as v1. None looks like it invalidates the comparison,
but the 415 end-of-non-void warnings are a real code-quality caveat for an unreviewed
toolchain (host cpp 16.2.1) and I did not audit them.

## 10. Tool and `/tmp` hygiene

- `talon` and `sbs_descramble` have 0 `/tmp/opencode` and 0 v1-root strings; v1's old
  `make`/`bash` have compiled-in old-path strings (not a difference in outputs found here).
- `/tmp`: I created `/tmp/.nothing` (removed) and `/tmp/opencode/hx` as a scratch dir for
  the lib comparison (copied results into the repeat root, then removed). Wine server
  sockets in `/tmp/.wine-1000` are unavoidable. I otherwise did not modify
  `/tmp/opencode`.

## 11. Limits

- One repeat only; root path string differs from v1 (the main cause of raw differences).
- No equivalence claim with an official Symbian SDK. No E32 execution, QEMU or hardware.
- Host `cpp` 16.2.1 and Wine 11.0 Staging are not pinned in the repo.
- petran and `.sym`/`.map` ripple explanations are partially inferred (§7, §8).

## Handoff

```text
Card / base commit / owned branch: R1-BUILD-REPEAT / port repo HEAD fa62323eea7bc96cefd666e40f5189567731bd44 (dirty from other workers; no repo edits other than this file) / none
Contract revision and dependencies: no ABI contract touched. Depends on docs/r1_clean_build_v1.md (v1 hashes), docs/r1_build_config_v0.md (variant, petran recipe), build repo f458a4ce + D1/D2 patch fd504a60, kernelhwsrv 0c320865 (clean).
What changed (paths) or researched: added qemu-port/docs/r1_build_repeat_v1.md only. Scratch outputs under /home/yuzi/.cache/sybian-r1/r1-build-v1-repeat/ (logs/, logs/cmp/, logs/diag_*, harness/).
Tests run (exact commands, result, log paths): cold repeat build via harness stages (export_eka, export_f32, genexec, petran, euser, hal, efsrv, t_cactw, fhserver) all rc 0, logs/sbs_*; preflight pre 31 ok, post 27 ok; check_rfile64.sh rc 0; compare scripts harness/compare_{tree,e32,diag,aux}.py and compare_implib.sh -> logs/cmp/. Result: raw hashes differ (20/20), differences explained by root-path length, timestamps/CRCs and root-derived names; equal-path diagnostic gives identical euser/t_cactw/fhserver after header mask.
Manual step drafted/updated: none.
Compatibility and failure limits: not byte-reproducible across different root path lengths; single repeat; host cpp 16.2.1 and Wine 11.0 unpinned; petran/.sym/.map ripple partly inferred; no runtime verification; no official-SDK equivalence.
Blocked assumptions or cross-card interface requests: none blocking. Request to coordinator: decide whether the pinned build root string must be fixed (same path) for any future reproducibility gate; a repeat at the exact v1 root would need v1 moved or run after it is archived, which I did not do.
Recommended integration order and next owner: merge this doc after r1_clean_build_v1.md; coordinator to decide on a same-root or equal-length-root repeat and on pinning/recording the toolchain.
```
