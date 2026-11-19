# R1-G-FORMAT — host-tested format module

2026-10-23. Scoped format implementation, **not a loader, ABI freeze or
QEMU-ready feature**. Owned paths are `loader/e32g_format.{h,cpp}` and this
directory only. No existing loader/generator, Makefile, kernel, FS or shared
planning file is edited. No target build, E32 execution or QEMU was performed.

## API and ownership

Include `loader/e32g_format.h`. Namespace `E32G` is C++98, allocation-free,
STL-free, exception-free and independent of Symbian/host runtime headers except
`stdint.h`. The module performs byte reads, so unaligned x86 words are safe.
The test executables alone use host allocation and stdio.

- `Parse(Bytes, Image&, Error&)` validates an immutable borrowed buffer and
  returns `Ok`, `Corrupt`, `Unsupported`, or `InvalidArgument`. `Error` contains
  a named issue and diagnostic file offset (or offending out-of-range offset).
  Output is zeroed on any failure; input is never changed. Error enum values
  are module-local, **not** Symbian error codes or public wire ABI.
- `Image` contains explicit file-coordinate `Range` views for header, code,
  initialized data, import/export sections, export description and relocations;
  UIDs, SID, VID, both capability words, flags, exception descriptor, priority,
  heap/stack/BSS sizes and module version are preserved, not overridden.
- `codeLinkBase`/`dataLinkBase` are link-time addresses; `entryCodeOffset` is an
  offset within code. PE raw export values are section-relative coordinates,
  **not link addresses, file offsets or relocated run addresses**. This module
  has no run/load addresses and never applies relocations or stamps code.
- `ImportAt(image,index,...)` returns bounded filename and IAT file ranges,
  count and extracted `Identity`. Its base/extension ranges are relative to
  **the name view**, not the file. UID/version presence flags distinguish an
  explicit zero from a missing decoration. Block order partitions the IAT at
  `code.offset + textSize`. No per-import ordinal validation against providers
  is performed here; that belongs to graph/binding code.
- `ExportAt(image,ordinal,...)` is one-based; zero/out-of-range is invalid.
  `raw` is unchanged; `absent` is derived from the validated hole description.
  Zero alone is not a hole marker. Binder must independently reject required
  zero exports and absent exports. Genuine EXEs' unused zero export is accepted.
- `RelocBlockAt(image,view,cursor,block,page,error)` iterates the returned code
  or data relocation view. Initialize cursor to zero. `Ok` plus empty block
  means end. The returned block includes its eight-byte header and uint16
  entries; consumers read them as bytes, never cast serialized host structures.
- `ParseIdentity(Bytes,...)` also works independently. Accepts root dependency
  names, optional eight-hex version and UID suffixes before extension. No path,
  wildcard, control/non-ASCII byte or malformed brace/bracket is accepted. It
  does not case-fold, resolve paths, rank versions or impose security policy.
- `Crc32`, `HeaderCrc`, `UidChecksum` expose the format primitives. CRC is
  reflected polynomial `edb88320`, initial zero, no final XOR; HeaderCrc
  substitutes `c90fdaa2` into the checksum field without writing input.

Only pass unmodified successfully parsed `Image`/`RelocView` objects to the
accessors. Buffers must remain alive and unchanged; the types are views, not
capability-secure opaque handles. Accessors are not validators for fabricated
caller-built image structures. `ImportAt` scans preceding blocks; callers
should not assume random-access constant time. Validation itself is bounded
by file size, dependency count and export count; sparse bitmap checking is
linear in bitmap/export size, without allocation.

## Supported profile and boundaries

Uncompressed x86 V header, PE2 imports; variable header/export descriptions;
EXE initialized data and BSS; only zero-writable-data DLLs. Required paging,
named export data, PE/ELF imports, other CPU/header versions, compression and
writable DLLs return `Unsupported`. Unsupported profile is distinguished from
malformed supported data. Default paging flags are allowed; this parser does
not choose runtime paging policy.

Checks include checked arithmetic, header CRC/UID checksum, module-version
range, x86 ABI/entry flags, entry+16 bounds, section/file bounds, heap/stack
constraints, bounded PE2 names/counts/IAT, relocation types 1000/2000/3000,
block/page/site/count validation, and NoHoles/full/sparse export descriptions.
Relocation `iSize` excludes the eight-byte section header. Import `iSize`
includes its four-byte header. x86 relocation sites and file offsets need not
be naturally aligned. General relocation validation does not apply fixups,
infer 3000 targets, or reject all overlapping relocation writes: those are
materializer responsibilities with explicit policy.

Entry+16 is a structural bound, **not** verification of executable veneer
instructions or the zero stamp. Veneer shape, stamp conflicts/identity,
ordinal-zero binding policy, provider matching, dependency budgets/cycles,
object lifecycle, actual memory mapping, security enforcement/sysbin search
and publication are outside this module. No policy from the rejected
security-off loader candidate is implemented. Metadata is propagated intact.

NoHoles payload handling and sparse meta-byte counting follow upstream.
Additionally, the parser verifies description presence bits agree with raw
entry-offset hole markers for each declared export. It does not require unused
padding bits to be canonical or forbid all overlapping file sections when the
upstream range checks permit them. It is a bounded initial profile, not a
claim of complete E32 acceptance parity.

## Reproduce the host tests

From repository root, supply the immutable **v1** image directory and a new
scratch directory. The runner refuses an existing scratch path.

```sh
python3 qemu-port/tests/e32g_format/run.py \
  --fixtures /home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel \
  --scratch /tmp/opencode/e32g-format-my-unique-run
```

`--cxx` selects compiler. `--sanitize` requests ASan+UBSan (including host
probe); unavailable runtime libraries are a failed/blocked build, never a
silently skipped success. No system installation is needed or authorized.
Compile commands, compiler stdout/stderr/status, synthetic files and
`results.json` stay in scratch. No fixture bytes or host executable is copied
into this source directory. `v1_manifest.json` pins the four CLEAN-BUILD hashes
and independently checked expected field/count/metadata values. Original
fixture hashes are checked before and after. Probe verifies input immutability,
accessors, and empty failure output on every invocation.

The independent Python vector constructor uses format fields, polynomial-built
CRC table and bitwise CCITT UID checksum; it does **not** import or invoke the
pilot generator or replace the compiled C++ parser with a Python oracle.
Semantic header mutations recompute CRC, so failures reach their intended
branches. CRC-damaged vectors deliberately do not. Additional deterministic
mutation smoke checks assert no crash/partial result, not semantic correctness
for every random mutation.

## Actual results and provenance

Base port commit `fa62323eea7bc96cefd666e40f5189567731bd44`, originating branch
main, dirty baseline. Worktree detached at that commit:
`/tmp/opencode/r1-g-format-20261023/worktree`. All 195 available tracked and
untracked baseline files were copied over HEAD before implementation; hashes
are in sibling `baseline.json`. Existing upstream checkout is a task-policy
read-only symlink at commit `0c3208650587ac0230aed8a74e9bddb5288023eb`.
Latest reconciliation §“loader/FS candidates and fixture returned”, roadmap,
parallel-work rules and card 8 were read. No agents spawned or commits/pushes.

Original card-8 successful run (g++ host, C++11, `-Wall -Wextra -Werror -pedantic
-fno-exceptions -fno-rtti`):

```sh
python3 qemu-port/tests/e32g_format/run.py \
  --fixtures /home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel \
  --scratch /tmp/opencode/r1-g-format-20261023/final2
```

**78 named vectors passed**, plus direct C++ API assertions and **200**
deterministic mutation smoke cases. All four v1 images parsed with exact
counts/metadata, including 10 EUSER and 20 EFSRV holes. Standard CRC index-205
header accepted; otherwise identical legacy-bad-table CRC rejected. All three
legitimate relocation kinds accepted; wrong header-inclusive relocation size,
last-block truncation, overflow/count/type/site errors rejected. Full/sparse
bitmaps, names/decoration, entry bounds, data relocations and unsupported
writable DLL cases were exercised. Run output is sibling `final2.stdout`;
complete machine-readable evidence is `final2/results.json`.

Card-12 strict C++98 freestanding host compile (the original C++11 source did
not pass this check):

```sh
g++ -std=c++98 -ffreestanding -fno-exceptions -fno-rtti \
  -Wall -Wextra -Werror -pedantic -c qemu-port/loader/e32g_format.cpp \
  -o /home/yuzi/.cache/sybian-r1/r1-format-compat-v1/host.o
```

Sanitizer attempt `sanitize1` failed to link: host lacks
`/usr/lib64/libasan.so.8.0.0`; separate UBSan link also lacks
`/usr/lib64/libubsan.so.1.0.0`. **No sanitizer pass claimed.** Earlier runs
are preserved: run1 corrected an erroneous fhserver SID/caps test expectation
against bytes (actual SID 2323, caps zero); run2 fixed test issue-enum indexing;
first final run fixed a positive no-relocation vector's expected status.
These were harness corrections, not fixture edits or permissive parser repairs.

## Source evidence and future integration

### R1-FORMAT-COMPAT return and coordinator integration (2026-10-23)

Reviewed C++98 syntax-only conversion from detached
`/home/yuzi/.cache/sybian-r1/r1-format-compat-worktree`, port base
`fa62323eea7bc96cefd666e40f5189567731bd44`; upstream read-only
`0c3208650587ac0230aed8a74e9bddb5288023eb`. Public header unchanged.
Worker full handoff remains in that worktree's README. No semantic changes or
test removals; no demonstrated additional format defect found by review.
This does not establish full upstream validator parity.

Worker evidence under `/home/yuzi/.cache/sybian-r1/r1-format-compat-v1`:
strict host C++98 freestanding compile and 78 vectors/API/200 mutations passed.
Pinned MinGW GCC 3.4.5 produced compile-only objects for module/api/probe using
a private prefix, but Wine emitted a disk-quota diagnostic despite zero exits.
Do not describe that evidence as warning-free. No target link/guest execution,
SDK mutation, installs or sanitizer pass. Earlier failed quota/harness scratch
was preserved. Coordinator rerun paths/results are in reconciliation.

Pinned upstream references (read only):

- `kernel/eka/euser/us_func.cpp:2023-2111`: genuine CRC table/algorithm;
  index 205 is `e5d5be0d`, not pilot `e5d5c06d`.
- `userlibandfileserver/fileserver/inc/f32image.h:612-882`: V header rules;
  `:889-952`: export-description structure; `:967-1061`: relocation block
  extents/counts/types; `:1080-1208`: PE2 import ranges and IAT.
- `.../sfile/sf_ldr.cpp:463-553`: hexadecimal decorated identity suffixes;
  this API is deliberately root-name-only, not upstream's general path parser.
- `.../sfile/sf_lepoc.cpp:2699-2753`: full/sparse bitmap bit meaning;
  `:526-550`: required zero/absent exports rejected by binding stage.

Future integrator: map module errors to kernel/server errors; use views in
graph/materializer without changing coordinate meanings. Do not globally
replace pilot CRC or relocation semantics. Add separately owned kernel adapter
and targeted QEMU regression only after contract review and test-slot assignment.
Assert create-info entry/export copyback (no double base), actual IAT targets,
zero/absent rejection before Resume, validated stamp, data-template/BSS startup,
cleanup and unchanged explicit L behavior. Wire stable-suite tests and manual
steps in an integration card, not here. Future owner manual check: genuine
fixture launch/exit/reap twice and deliberate malformed/absent image rejection
on disposable media, with visible diagnostics. None performed now.

### Handoff

- Card **R1-G-FORMAT**, scoped module API v1; no blanket ABI freeze.
- Changes: two new format files and this new tests directory only.
- Tests: 78 vectors + API + 200 mutation cases; freestanding host compile pass;
  sanitizers blocked by missing host libraries. No guest/runtime equivalence.
- Limits: initial PE2/V profile and borrowed-view preconditions above; no
  materializer/search/security policy or loader integration.
- Blocked assumptions: runtime behavior, actual integration ABI and protected
  memory-model compatibility remain unverified. No required interface conflict
  encountered in this format-only slice.
- Integration order: coordinator review → graph/object/service boundary adoption
  → independent materializer/kernel wiring → targeted/stable/manual validation.
- No upstream/artifact edits, installs, deletion, target build or QEMU.
