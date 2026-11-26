# GENUINE_BUILD_REVIEW — independent review of the genuine-Symbian build toolchain

Reviewer: subagent (review-only). Date: 2026-11-23.
Scope: `qemu-port/tools/genuine_build.py`, `qemu-port/genuine/link_{exe,server,dll}.ld`,
`qemu-port/genuine/README.md`, `qemu-port/tools/elf2e32.py` (git diff vs HEAD),
`qemu-port/tools/e32read.py` + `qemu-port/variant/{e32load.cpp,qemu_variant.h}` diffs,
`genuine/prove_qemu.py`, `genuine/pack_multi.py`. `kernelhwsrv/` was only read;
git status confirms it is untouched (clean before and after all rebuilds below).

## Verdict

**CONDITIONAL — NOT yet safe to build the big servers on.** The core
exe path (compile → link → elf2e32 → e32read → QEMU boot) is proven correct and
reproduces exactly (see Verification). But the export path that servers will rely
on (`--def` / `--export-map`) has one **high-severity silent-corruption bug**
(MSVC-decorated .def names are silently dropped, shifting ordinals), and the
builder has one **path-traversal write** (`--name`). Fix the 2 required items
below before building servers; the loader-rejection footgun for
`--type server/exe --def` should be guarded or documented loudly.

## What was checked and found correct

- **Shell-out safety**: `genuine_build.py:85-87` and `prove_qemu.py:43-45` use
  `subprocess.run([list])` — no shell, no injection via filenames. Sources are
  `resolve()`d to absolute paths before use (`genuine_build.py:238`), so a source
  named `-foo.c` cannot be misparsed as an option. Object names are
  `OBJ / f"{src.stem}_{i}.o"` (`:241`) — POSIX stems cannot contain `/`, and the
  index prevents collisions. Print path uses `shlex.quote`. **No injection found.**
- **Writes**: all writes go to `genuine/build/` (`BUILD`/`OBJ`, `:228-229`,
  `:259`, `:270`, `:281`, `:192`). Nothing in `genuine_build.py` (or any
  `genuine/*.py`) writes into `kernelhwsrv/` — verified by grep and by
  `git status -- kernelhwsrv/` staying clean across full rebuilds. **EXCEPT** the
  `--name` traversal below.
- **CRC/UID after export patching**: `patch_exports` (`genuine_build.py:165-194`)
  writes exp_off/exp_count (offset 88), bumps text_size (96), flips UID1/uid-checksum/
  KImageDll for DLLs (182-188), then recomputes the header CRC over `blob[:code_off]`
  with the CRC field set to `KImageCrcInitialiser` (190-191) — the same convention
  `elf2e32.py:122-123` and `e32read.py:127-131` use. Independently re-verified with a
  separate script (not e32read) on GENUINE/GENUINESRV/GENUINEDLL.E32: `crc_ok=True`,
  `uid_ok=True` for all, DLL uid1=0x10000079 chk=0x55081da8. Export dir entries are
  code-relative offsets (`value - CODE_BASE`, `:299`), which matches the loader's
  resolution `sDllExp[i] = run + Rd32(file + expOff + 4i)` (`e32load.cpp:625`).
  Export-desc tail (152/154, NoHoles) is correctly left untouched — both validators
  accept NoHoles for contiguous exports (`e32read.py:278-279`, `e32load.cpp:531-532`).
- **elf2e32.py was NOT modified** by the toolchain agent: `git diff` vs HEAD is
  empty (last change commit 79c4625). Its `--euser-import/--iat-off` support
  predates this work. It enforces `len(code) <= iat_off` loudly when an import is
  present (`elf2e32.py:95-96`) — so the "sfsrv 17k LOC exceeds 0x8000" case fails
  with a clear error, not silently; `--iat-off` is the documented workaround
  (ewsrv used `--iat-start 0xD000` via `pack_multi.py:58`, which also enforces
  `iat_start >= text_len`).
- **e32read.py / loader diffs are coherent**: the only e32read change is the PE2
  import-count cap 16→512 (`e32read.py:231`), matched by `e32load.cpp:454` and
  `iSlots[16]→[512]` in `qemu_variant.h` — needed for EWSRV's 216/219 EUSER
  ordinals. EWSRV.E32 validates. Header-CRC, UID-checksum, export-bounds, PE2-IAT
  (`text_size + 4*nimp <= code_size`, ordinals at `code_off+text_size+4k`) checks
  agree between `e32read.py` and `e32load.cpp`.
- **Link scripts**: all three are identical in layout (`.text` @0x400000 with
  `.genuine_exp` last, `.data` @0x500000, ENTRY(`_start`) from `user/start.S`
  `.text.start`), satisfying `elf2e32.py:42`'s base/entry contract. Stack size is
  NOT a linker setting — it is hardcoded 8192 in the E32 header by
  `elf2e32.py:109` (port-wide default), so link scripts are fine but stack size is
  not per-image configurable.
- **README matches reality**: the documented command was re-run verbatim and
  reproduces `genuine/build/GENUINE.E32` at exactly 33,008 bytes; define/include
  lists match `GENUINE_DEFINES`/`GENUINE_INCLUDES`; "no writes outside genuine/"
  holds for the default flow.

## Issues

| # | Severity | Location | Issue |
|---|----------|----------|-------|
| 1 | **HIGH** | `tools/genuine_build.py:128-130` (`parse_def`) | The export regex `([^@\s]+)\s*@\s*(\d+)` stops at the first `@`, so **MSVC-decorated .def names (`?Printf@RDebug@@SAXPBDZZ @ 1345`, the format of `kernelhwsrv/kernel/eka/bx86/euseru.def`) never match and are silently `continue`d**. Verified by running the regex. If a .def mixes decorated and plain lines, entries silently drop and **all later ordinals shift by one** — the export directory binds the wrong function to each ordinal; e32read/loader only check bounds, so this passes validation and corrupts at runtime. It also makes `--export-map '?Printf@...=_ZN...'` impossible (the key is never extracted), despite README.md:108-114 claiming decorated names are handled via `--export-map`. Fix: parse the name as everything before the final ` @ <digits>` (e.g. `^(?:[^=\s]+\s*=\s*)?(.+?)\s*@\s*(\d+)\b`), and **fail loudly on any non-empty, non-comment line inside EXPORTS that doesn't parse** instead of `continue`. |
| 2 | **MEDIUM-HIGH** | `tools/genuine_build.py:270-271, 281` (`--name`) | `BUILD / f"{args.name}.elf"` does no sanitization: `--name /tmp/evil` writes `/tmp/evil.elf`+`E32` (pathlib join with an absolute path discards `BUILD`), and `--name ../../../kernelhwsrv/evil` writes **into kernelhwsrv/** — violating the read-only rule via an operator-supplied arg. Verified with pathlib semantics. Fix: reject anything that is not a single safe component (`^[A-Za-z0-9][A-Za-z0-9._-]*$`, not `.`/`..`), e.g. `if Path(args.name).name != args.name or args.name in ('.','..'): sys.exit(...)`. |
| 3 | **MEDIUM** | `genuine_build.py:307` + `variant/e32load.cpp:407-408` | `--type exe`/`--type server` **with `--def`** produces an image with exports but EXE UID1; `e32read.py` accepts it (GENUINESRV.E32 validates) but the kernel loader hard-rejects it (`if (!isDll && expCount != 0) return KErrNotSupported`). README says only "not yet load-tested" — it is actually **unloadable** with the current loader. Fix: either refuse `--def` unless `--type dll` (or add a server-UID path in the loader), and print a loud warning; update README. |
| 4 | LOW | `genuine_build.py:300-301` | Export targets are bounded by `args.iat_off` even when `--euser-import 0` (no IAT exists); a big import-free image can only export code below 0x8000. Conservative (safe direction), but surprising. Document or bound by actual text size when no import. |
| 5 | LOW | `genuine_build.py:145-162` (`elf_symbols`), `:318` | Malformed/truncated ELF raises `struct.error`/`IndexError`, and very large source lists raise `OSError` (E2BIG) — both uncaught by `except (ValueError, CalledProcessError)`, so the tool dies with a traceback instead of a clean message. Loud, but not clean. Fix: catch `(ValueError, OSError, struct.error, IndexError, KeyError)` or use an ld `@responsefile` for huge lists. |
| 6 | LOW | `elf2e32.py:109` | Stack size hardcoded 8192 in every image; not configurable via `genuine_build.py`/link scripts. Deep-recursion servers may need more. |
| 7 | LOW | `genuine/pack_multi.py:127-129` | `struct.pack_into('<i', imp, 8, n)` overwrites the first 4 bytes of the DLL name in the import section, so the stored name is `r{000a0000}[100039e5].dll` instead of `euser{...}`. Passes validation only because just the `.dll\0` suffix is checked; benign here (loader binds PE2 by ordinal), but wrong. Fix: `imp = struct.pack('<III', isize, 12, n) + EUSER_PE2_DLL`. |
| 8 | INFO | `genuine_build.py:128` vs README | `bx86gcc/euseru.def` uses GCC-mangled names (`_ZN6RDebug6PrintfEPKcz @ 915`) which DO parse; only the MSVC `bx86/` defs hit issue #1. The GCC-def path is fine today. |
| 9 | INFO | hygiene | Stray 0-byte file `qemu-port/-D__cdecl=__attribute__((cdecl))` (from an unquoted manual shell define at 20:44). Not created by `genuine_build.py` (list-form args are safe); delete it. |
| 10 | INFO | duplicates/robustness | Duplicate symbol definitions fail loudly at `ld` (CalledProcessError → clean exit). Duplicate `--export-map` keys silently last-win; duplicate ELF symtab names silently last-win in `elf_symbols`. All fail-safe directions; noted only. |

## Required fixes before building servers

1. **Fix `parse_def`** (issue #1): accept MSVC-decorated names; error out on any
   unparseable EXPORTS line. This is the MSVC-__thiscall→GCC-symbol mapping the
   server `.def` files need; today it silently mis-binds ordinals.
2. **Sanitize `--name`** (issue #2): single path component only, so no write can
   ever land outside `qemu-port/genuine/build/` (including `kernelhwsrv/`).
3. **Guard `--def` on exe/server** (issue #3): refuse or warn, because the loader
   rejects export-carrying non-DLL images (`e32load.cpp:407`).

Recommended (not blocking): #5 clean error handling / responsefile, #6 stack-size
flag, #7 pack_multi name fix, delete the stray file (#9).

## Verification performed (all read-only on sources; build artifacts regenerated)

- `git diff` — `elf2e32.py` **unmodified** vs HEAD; `e32read.py` 1-line (16→512);
  `e32load.cpp`/`qemu_variant.h` matching 512-slot change; `kernelhwsrv/` clean
  before and after.
- `e32read.py` on GENUINE/GENUINESRV/GENUINEDLL/EWSRV.E32: all `E32 OK`.
- Independent re-check of patched images (own script, `mke32.crc32_symbian`/
  `uid_checksum`): header CRC and UID checksum valid after `patch_exports`;
  export dir inside code, targets < code_size, `text_size=0x8000`,
  `code_size=0x8004` (IAT slot at code+0x8000).
- **Rebuild from scratch** with the documented command (deleted
  `genuine/build/GENUINE.{elf,E32}` + `obj/*` first, no `make clean`):
  `python3 tools/genuine_build.py genuine/hello/hello.cpp ../kernelhwsrv/kernel/eka/euser/epoc/x86/uc_utl.cpp --name GENUINE --type exe --iat-sym genuine_printf_slot --euser-import 915`
  → `OK: .../GENUINE.E32 (33008 bytes, type=exe, euser_ord=915, exports=0)`;
  size matches README (33,008). `e32read.py genuine/build/GENUINE.E32` → `E32 OK`.
- **QEMU boot proof**: `python3 genuine/prove_qemu.py` → **RESULT: PASS** — all 3
  markers, `e32run: bind slot 32768 ord 915 -> 003e7898`, `imports bound dll (1 ordinals)`,
  `Loaded r=0`, `genuine started (user process, pid=8)`, no Exc/Panic/FAULT
  (log: `/tmp/opencode/genuine_proof.log`).

## Bottom line

Compile/link/pack/validate pipeline: correct and reproducible; CRC/UID handling
after export patching: correct (independently verified); proof: PASSES end-to-end
in QEMU. Do **not** build the big servers on it yet until issue #1 (silent
.def ordinal misalignment) and #2 (`--name` traversal) are fixed, and #3 is
guarded — those are exactly the paths servers will use.

---

## 2026-11-26 — SUPERSESSION of the "CONDITIONAL — NOT yet safe to build the big servers" verdict

The verdict in **this document's header (dated 2026-11-23) is stale**. It is kept
above unchanged for history; this footer is the authoritative verdict.

- **All three required fixes landed in-tree.** `parse_def` now takes the name as
  everything before the final ` @ <digits>` (issue #1), so MSVC-decorated
  `.def` names parse instead of being silently dropped, and any unparseable
  EXPORTS line raises loudly. `--name` is confined to a single safe component
  and rejected before anything is created, with a resolved-path check that
  keeps every write under `genuine/build/` (issue #2). `--def` with
  `--type exe|server` is refused outright, because the loader rejects
  export-carrying non-DLL images (issue #3).
- **Proof of #1:** `python3 tools/genuine_build.py --selftest` → **PASS**,
  `3 def files, 5105 entries parsed/mapped to (ordinal,name), 2432
  MSVC-decorated, 0 failure(s)`.
- **Also fixed:** #5 (`elf_symbols` catches `ValueError, OSError, struct.error,
  IndexError, KeyError` and exits cleanly), #7 (`pack_multi.py` builds the import
  record as `struct.pack('<III', isize, 12, n) + EUSER_PE2_DLL`, so the DLL name
  is no longer clobbered), #9 (the stray 0-byte
  `qemu-port/-D__cdecl=__attribute__((cdecl))` is gone).
- **Still live, both LOW:** **#4** export targets remain bounded by
  `--iat-off` even with `--euser-import 0` (conservative, documented at
  `genuine_build.py:476-480`), and **#6** the 8192 stack size is still hardcoded
  in `elf2e32.py:109` and not configurable. Neither blocks server builds.
- **The servers were built and pass.** The authoritative 15-row gate
  (`/tmp/opencode/final27_gate.log`) ends `OVERALL: PASS (15 pass, 0 skip,
  0 fail/not-run)` with `FINAL27_RC=0`, and its rows include **`genuine_ewsrv`
  PASS** and **`genuine_efsrv` PASS** — genuine EWSRV and EFSRV images are
  produced by this toolchain and validated end-to-end.

**Verdict: SUPERSEDED — safe to build the big servers on this toolchain.** The
only open items are #4 and #6, both LOW and non-blocking. `kernelhwsrv/` remains
read-only and untouched.
