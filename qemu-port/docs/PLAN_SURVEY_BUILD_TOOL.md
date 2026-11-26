# PLAN_SURVEY_BUILD_TOOL — genuine_build.py `--name` traversal + constraint headroom

**Date:** 2026-11-25. **Scope:** read-only survey (plus live rejection probes of
`--name`, which exit before any file is created). No edits outside this file, no
commit/push, no `make clean`, no QEMU runs. `kernelhwsrv/` untouched (its
`git status` stayed clean). Sources: [tools/genuine_build.py](/home/yuzi/Documents/Sybian REAL/qemu-port/tools/genuine_build.py),
[genuine/README.md](/home/yuzi/Documents/Sybian REAL/qemu-port/genuine/README.md),
[docs/GENUINE_BUILD_REVIEW.md](/home/yuzi/Documents/Sybian REAL/qemu-port/docs/GENUINE_BUILD_REVIEW.md)
issue #2, [docs/PLAN_SURVEY_PROVENANCE.md](/home/yuzi/Documents/Sybian REAL/qemu-port/docs/PLAN_SURVEY_PROVENANCE.md) §5,
[genuine/BUILD_MANIFEST.json](/home/yuzi/Documents/Sybian REAL/qemu-port/genuine/BUILD_MANIFEST.json),
[variant/e32load.cpp](/home/yuzi/Documents/Sybian REAL/qemu-port/variant/e32load.cpp),
[tools/e32read.py](/home/yuzi/Documents/Sybian REAL/qemu-port/tools/e32read.py),
[tools/elf2e32.py](/home/yuzi/Documents/Sybian REAL/qemu-port/tools/elf2e32.py),
[genuine/pack_multi.py](/home/yuzi/Documents/Sybian REAL/qemu-port/genuine/pack_multi.py),
and the `genuine/*/build_*.py` scripts.

## Defect description

GENUINE_BUILD_REVIEW issue #2 reported that `tools/genuine_build.py` used
`BUILD / f"{args.name}.elf"` (old lines 270–271/281) with no sanitization:
`--name /tmp/evil` made pathlib discard `BUILD` and write `/tmp/evil.elf|.E32`,
and `--name ../../../kernelhwsrv/evil` wrote into the read-only `kernelhwsrv/`
repo via an operator-supplied argument.

**In the current tree this defect is already fixed** (commit `7f76900`,
2026-11-24 08:28; working tree clean vs HEAD):

- `tools/genuine_build.py:148` — `NAME_RE = ^[A-Za-z0-9][A-Za-z0-9._-]*$`
  (single safe component: no `/`, no leading `.`, so `..`/`.`/absolute paths
  all rejected), checked at `:366-369` **before** `BUILD.mkdir()` or any compile.
- `tools/genuine_build.py:371-375` — `build_out()` re-resolves
  `BUILD / f"{name}{ext}"` and refuses unless `p.parent == BUILD.resolve()`;
  every `.elf`/`.E32` output goes through it (`:437`, `:448`). All other writes
  (`OBJ / f"{src.stem}_{i}.o"`, the fixed-name export pad) cannot contain `/`.

Residual nit (cosmetic, not exploitable): the regex uses `$`, which in Python
matches before a trailing newline, so `--name 'evil\n'` passes the regex; the
`build_out()` parent re-check still confines it inside `genuine/build/`, so no
escape is possible — only a strangely named output file. The per-component
scripts (`genuine/build_efile.py`, `genuine/ewsrv/build_*.py`,
`genuine/estart_eshell/build_estart_eshell.py`, `genuine/efat32/build_fsy_dll.py`)
take no user-controlled output path — all write fixed names under
`genuine/build/`.

## Risk

- Original (now-fixed) defect: arbitrary file write (`.elf` + `.E32`) anywhere
  the build user can write, including into the read-only `kernelhwsrv/` repo —
  violating the read-only guarantee and potentially poisoning future builds.
- Residual: none security-relevant. The `$`-vs-`\Z` nit cannot escape
  `genuine/build/` because of the independent `resolve()`/parent guard.
- Process risk: `docs/PLAN_SURVEY_PROVENANCE.md` §5 still states the `--name`
  traversal "must be fixed before any further genuine builds" — that line is
  **stale** (the fix landed in `7f76900` the same day the survey is dated) and
  could block or confuse future build planning.

## Minimal fix

No change is required to close the traversal — it is closed. If a hardening
commit is wanted, the minimal diff that changes no build output for any valid
name is one character: in `tools/genuine_build.py:148` anchor the regex with
`\Z` instead of `$` (`r"^[A-Za-z0-9][A-Za-z0-9._-]*\Z"`), rejecting trailing-newline
names at the first gate. Optionally delete the stale §5 sentence in
`docs/PLAN_SURVEY_PROVENANCE.md`. Both edits are output-neutral: every
previously accepted name that produced a build still matches `\Z`, and
rejected names already produced no output.

## Constraint headroom

Measured from the on-disk images (headers parsed directly) and the caps in
`tools/e32read.py:231` / `variant/e32load.cpp:533`:

| Constraint | Cap | Current worst case | Headroom |
|---|---|---|---|
| PE2 import slots (1-based ordinals) | 512 (`e32read.py:231`, `e32load.cpp:533`) | EFILE.E32 nimp=**463** | **49 slots** |
| — | | EWSRV.E32 nimp=212, etshell 190, EDISP 1 | 300 / 322 / 511 |
| DLL dependencies per image | 1 (single `euser{...}.dll` PE2 import; `elf2e32.py:18,131`, `pack_multi.py`, all images show `dllrefs=1`) | 1 everywhere | 0 — hard limit, needs pipeline work for more |
| Static DLL image buffer `sDllFile[512*1024]` (`e32load.cpp:660`) | 524,288 B | EUSER.DLL = 412,104 B | ~112 KB |
| Static server image buffer `sSvcImg[512*1024]` (`e32load.cpp:1265`) | 524,288 B | EFILE.E32 = 273,044 B (file), text 262,144 B | ~251 KB file / ~262 KB text |
| IAT placement | IAT at end of text, `text+4*n <= codeSize` (`e32load.cpp:536-538`); `pack_multi.py:64-65` enforces `iat_start >= text_len` | EFILE text 0x40000, EWSRV used iat_start 0xD000 | per-image, enforced loudly |
| Stack | fixed 8192 B (`elf2e32.py:109`, not configurable) | all images | 0 — fixed |

EFILE is the binding constraint on both the 512-slot cap (49 spare) and is the
largest server image (still ~48% of the 512 KB static buffer). EUSER.DLL fits
its loader buffer with ~112 KB spare.

## Verification

Performed read-only / no-side-effect this session:

- `python3 tools/genuine_build.py dummy.c --name '../../../kernelhwsrv/evil'` →
  `rc=1`, rejection message, no files created (check precedes `mkdir`/compile).
- Same for `--name /tmp/evil` and a newline name → `rc=1`; `ls /tmp/evil*`
  confirms nothing written; `git status -- kernelhwsrv/` clean.
- `git show 7f76900:qemu-port/tools/genuine_build.py` confirms `NAME_RE` +
  `build_out` were introduced in that commit; working tree matches HEAD.
- Import counts/sizes measured by parsing E32 headers directly (EFILE 463
  imports / 273,044 B; EWSRV 212 / 56,812 B; EDISP 1 / 65,952 B; etshell 190 /
  85,792 B; EUSER.DLL 412,104 B) and cross-checked with
  `tools/e32read.py` (`E32 OK`, `dllrefs=1`).
- No QEMU runs, no `make clean`, no commits/pushes, no edits outside this file.

## First steps

1. Treat the `--name` traversal as **closed** (fixed in `7f76900`); optionally
   apply the one-character `\Z` hardening and mark
   `PLAN_SURVEY_PROVENANCE.md` §5 stale — no build-output change either way.
2. Proceed with further genuine builds; if any new component is planned, check
   its EUSER import count against the 49-slot headroom on the 512 cap before
   compiling (EFILE is the near-boundary case).
3. Keep every new image under 512 KB file size (EFILE at 273 KB is the current
   worst case) and remember only one DLL (EUSER) can be imported per image.
4. Do not modify `kernelhwsrv/`; keep `genuine/BUILD_MANIFEST.json` updated per
   build so the byte-ratio metric stays accurate.
