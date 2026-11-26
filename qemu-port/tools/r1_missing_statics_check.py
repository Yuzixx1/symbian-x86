#!/usr/bin/env python3
"""r1_missing_statics_check.py -- W5: missing-statics guard for the two
declared-only statics `CActiveScheduler::iScheduler` and `User::iTlsIndex`
(docs/MISSING_STATICS_PLAN.md, docs/WAVE2_NEXT_STEPS.md:19).

WHY THIS EXISTS
    Both statics are DECLARED in user/e32base.h (:76 `static TInt iTlsIndex;`,
    :221 `static CActiveScheduler* iScheduler;`) and DEFINED nowhere. Today that
    is harmless: no linked image references them. The failure mode is silent and
    future-facing -- the first image that links a TU reaching an inline
    CActiveScheduler member (Install/Add/Start/Current/Stop) gets a hard linker
    error, or worse, a stale object keeps an old image green while the new TU
    cannot link. MISSING_STATICS_PLAN.md:11 predicted the exact shape of that
    surprise ("`iScheduler` and `iTlsIndex` appear in no object. LATENT for
    EFSRVMIN"). This gate keeps the prediction true instead of trusting prose.

WHAT COUNTS AS FAILURE
    undefined  a scanned object / image reports `U` (or weak-undefined `u`,
    `v`, `w`) for either target. The link needs a definition and no TU provides
    one. Fatal everywhere, and called out separately when the object is inside
    the EFSRVMIN link set (genuine/efsrv_min/build_server.sh:18-24) because that
    is the image the plan says must not grow these symbols.

WHAT IS REPORTED BUT NOT FATAL
    * defined    the target is defined somewhere (`B`/`D`/`V`/...). Reported OK:
      a definition is exactly the fix MISSING_STATICS_PLAN.md:20 prescribes.
    * lookalike  a symbol that CONTAINS the target word but is a different
      qualified name -- today the only one is `B FsPluginManager::iScheduler`
      (genuine/obj/EFILE/sf_plugin_man.o), an unrelated class's own static.
      Reported, never fatal. This is the whole reason matching is exact:
      a substring search for "iScheduler" would fire on it forever.
    * latched    LATENT (informational) when the target appears in no scanned
      object at all -- neither `U` nor defined. That is today's true state and
      it must keep reading as PASS, not SKIP.
    * unscanned  an artifact nm could not read (non-ELF, truncated). Counted and
      listed, never fatal -- a stale .E32 next to the image is not residue.

EXACT MATCHING AND THE ANTI-OVERMATCH GUARD
    A target fires only when the demangled name EQUALS it:
    `^CActiveScheduler::iScheduler$` / `^User::iTlsIndex$`. Never substring,
    never suffix. `anti_overmatch_guard()` re-proves that on fixed samples
    (`FsPluginManager::iScheduler`, `MyCActiveScheduler::iScheduler`,
    `CActiveScheduler::iSchedulerOld`, bare `iScheduler`, ...) every run, and a
    guard violation fails the gate: a matcher that drifted is worse than none.

SCOPE
    qemu-port only, and only what exists: *.o and *.elf under build/, genuine/
    and tests/. Sibling kernelhwsrv/ is never read or written.

GUARANTEES
    Read-only and offline: no `make`, no QEMU, no network, no file write, no git.
    The only subprocess is `nm -C` over existing artifacts. Zero objects is a
    PASS ("nothing to scan"), never a SKIP.

USAGE
    python3 tools/r1_missing_statics_check.py            # verdict, exit 0/1
    python3 tools/r1_missing_statics_check.py --list     # inventory, no verdict
    python3 tools/r1_missing_statics_check.py --json     # machine-readable
    python3 tools/r1_missing_statics_check.py --max-print 200

    DEFAULT_TESTS note: static and ~2 s over ~780 artifacts, so it belongs next
    to r1_shim_residue_sweep.py, before any QEMU spend.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

# ------------------------------------------------------------------ safety --
READ_ONLY = True
assert READ_ONLY, "r1_missing_statics_check must remain read-only"

TOOLS = Path(__file__).resolve().parent
PORT = TOOLS.parent
SELF = Path(__file__).resolve()

# Existing build output only. Nothing here is generated, cleaned or touched.
SCAN_ROOTS = ("build", "genuine", "tests")
SCAN_SUFFIXES = (".o", ".elf")
EXCLUDED_DIRS = {".git", ".cache", "__pycache__"}

NM = "nm"
NM_ARGS = ("-C",)
NM_TIMEOUT_S = 60


# ---------------------------------------------------------------- targets --
class Target:
    """One declared-only static, matched by EXACT demangled name."""

    __slots__ = ("key", "rx", "word", "decl", "defined_by", "reach")

    def __init__(self, key: str, word: str, decl: str, defined_by: str,
                 reach: str) -> None:
        self.key = key
        self.rx = re.compile(rf"^{re.escape(key)}$")
        # `word` is the member name used only to find LOOKALIKES (never to fire).
        self.word = word
        self.decl = decl
        self.defined_by = defined_by
        self.reach = reach

    def matches(self, name: str) -> bool:
        return self.rx.match(name) is not None


TARGETS = (
    Target(
        key="CActiveScheduler::iScheduler",
        word="iScheduler",
        decl="user/e32base.h:221 (static CActiveScheduler* iScheduler;)",
        defined_by="no TU defines it; user/ca_glue.cpp:21-23 states the omission "
                   "is deliberate",
        reach="REQUIRED the first time an image links a TU reaching an inline "
              "CActiveScheduler member (Install/Add/Start/Current/Stop)",
    ),
    Target(
        key="User::iTlsIndex",
        word="iTlsIndex",
        decl="user/e32base.h:76 (static TInt iTlsIndex;)",
        defined_by="no TU defines it and no in-tree reader exists",
        reach="LATENT/dead: declaration only, no reader",
    ),
)

# The image the plan says must never grow these symbols
# (genuine/efsrv_min/build_server.sh:18-24). Sorted for stable output.
EFSRVMIN_LINK_OBJECTS = (
    "build/user/ipc.o",
    "build/user/malloc.o",
    "build/user/runtime.o",
    "build/user/start.o",
    "genuine/efsrv_min/efsrv_min.o",
    "genuine/efsrv_min/r1_fs_core.o",
    "genuine/efsrv_min/r1_fs_server.o",
)
EFSRVMIN_LINK_IMAGES = (
    "genuine/efsrv_min/EFSRVMIN.elf",
    "genuine/efsrv_min/FATIOCLIENT.elf",
)
EFSRVMIN_LINK_SET = frozenset(EFSRVMIN_LINK_OBJECTS + EFSRVMIN_LINK_IMAGES)

# Anti-overmatch samples. Hard-coded, so a matcher change is caught here rather
# than by a green run over a tree that happens to lack the lookalike.
GUARD_MUST_MATCH = ("CActiveScheduler::iScheduler", "User::iTlsIndex")
GUARD_MUST_NOT_MATCH = (
    "FsPluginManager::iScheduler",          # real: B in sf_plugin_man.o today
    "FsPluginManager::iTlsIndex",
    "MyCActiveScheduler::iScheduler",
    "CActiveScheduler::iSchedulerOld",
    "CActiveScheduler::iScheduler2",
    "CScheduler::iScheduler",
    "iScheduler",
    "iTlsIndex",
    "User::iTlsIndexBackup",
    "CUser::iTlsIndex",
    "CActiveScheduler::iScheduler::iScheduler",
)

# nm(1) type letters. Undefined: U strong, u unique-global, v/w weak object/symbol.
UNDEFINED_TYPES = frozenset("Uuvw")

# `[address] type name` -- GNU nm pads the address field, absent for undefined.
NM_LINE = re.compile(r"^(?:(?P<addr>[0-9a-fA-F]+)\s+)?(?P<type>[A-Za-z?])\s(?P<name>.+)$")


# ------------------------------------------------------------------ helpers --
def rel(path: Path) -> str:
    return path.relative_to(PORT).as_posix()


def iter_targets() -> list[Path]:
    """Every existing *.o / *.elf under the scan roots, deterministically."""
    found: list[Path] = []
    for root in SCAN_ROOTS:
        base = PORT / root
        if not base.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = sorted(d for d in dirnames if d not in EXCLUDED_DIRS)
            for name in sorted(filenames):
                if not name.endswith(SCAN_SUFFIXES):
                    continue
                path = Path(dirpath) / name
                if path.resolve() == SELF:
                    continue
                found.append(path)
    return found


def anti_overmatch_guard() -> list[str]:
    """Problems with the matchers, if any. Empty list == guard holds."""
    problems: list[str] = []
    for target in TARGETS:
        for sample in GUARD_MUST_MATCH:
            if sample == target.key and not target.matches(sample):
                problems.append(f"{target.key!r} does not match its own name")
        for sample in GUARD_MUST_NOT_MATCH:
            if target.matches(sample):
                problems.append(f"{target.key!r} over-matches {sample!r}")
        # A target must not fire on the other target either.
        for other in TARGETS:
            if other is not target and target.matches(other.key):
                problems.append(f"{target.key!r} matches {other.key!r}")
    return problems


def nm_symbols(path: Path) -> tuple[list[tuple[str, str]], str]:
    """( [(type, name)], error ) for one artifact via `nm -C`."""
    try:
        proc = subprocess.run(
            [NM, *NM_ARGS, str(path)],
            cwd=PORT, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=NM_TIMEOUT_S, check=False,
        )
    except (OSError, subprocess.SubprocessError) as exc:
        return [], f"nm failed: {exc}"
    symbols: list[tuple[str, str]] = []
    for raw in proc.stdout.decode("utf-8", "replace").splitlines():
        match = NM_LINE.match(raw.lstrip())
        if not match:
            continue
        symbols.append((match.group("type"), match.group("name").strip()))
    if not symbols:
        detail = proc.stderr.decode("utf-8", "replace").strip().splitlines()
        return [], (detail[0] if detail else "no symbols (empty or non-ELF)")
    return symbols, ""


# --------------------------------------------------------------------- scan --
def scan() -> dict:
    started = time.monotonic()
    paths = iter_targets()
    guard = anti_overmatch_guard()

    undefined: list[dict] = []
    defined: list[dict] = []
    lookalike: list[dict] = []
    unscanned: list[dict] = []
    scanned = 0
    seen = {t.key: 0 for t in TARGETS}

    for path in paths:
        symbols, error = nm_symbols(path)
        if error:
            unscanned.append({"path": rel(path), "detail": error})
            continue
        scanned += 1
        rpath = rel(path)
        for sym_type, name in symbols:
            hit = next((t for t in TARGETS if t.matches(name)), None)
            if hit is not None:
                seen[hit.key] += 1
                row = {
                    "path": rpath,
                    "symbol": hit.key,
                    "type": sym_type,
                    "link_set": rpath in EFSRVMIN_LINK_SET,
                }
                if sym_type in UNDEFINED_TYPES:
                    row["detail"] = (
                        f"{hit.key} is undefined here and {hit.defined_by}"
                        + (" -- EFSRVMIN link set" if row["link_set"] else "")
                    )
                    undefined.append(row)
                else:
                    row["detail"] = f"{hit.key} is defined here ({sym_type})"
                    defined.append(row)
                continue
            # Not a target. Only the member word is interesting, for the record.
            for word in {t.word for t in TARGETS}:
                if word in name:
                    lookalike.append({"path": rpath, "symbol": name,
                                      "type": sym_type,
                                      "detail": "different qualified name; "
                                                "exact match correctly ignores it"})
                    break

    state = {}
    for target in TARGETS:
        if any(row["symbol"] == target.key for row in undefined):
            state[target.key] = "UNDEFINED"
        elif any(row["symbol"] == target.key for row in defined):
            state[target.key] = "DEFINED"
        else:
            state[target.key] = "LATENT"

    # Two distinct "nothing readable" worlds: no artifacts at all (PASS by
    # design) vs artifacts present but none parseable (broken toolchain -> FAIL).
    broken = ""
    if not paths:
        broken = ""
    elif scanned == 0:
        broken = "artifacts exist but nm read no symbols from any of them"
    elif guard:
        broken = "anti-overmatch guard failed: " + "; ".join(guard)

    return {
        "targets": paths,
        "scanned": scanned,
        "undefined": undefined,
        "defined": defined,
        "lookalike": lookalike,
        "unscanned": unscanned,
        "state": state,
        "seen": seen,
        "guard": guard,
        "broken": broken,
        "elapsed": time.monotonic() - started,
    }


# ------------------------------------------------------------------- output --
def print_inventory(result: dict) -> None:
    print(f"W5 missing-statics guard -- dry inventory (no verdict) -- {PORT}")
    print(f"scope: {', '.join(SCAN_ROOTS)} *.o + *.elf via `nm {' '.join(NM_ARGS)}`; "
          f"static, no make, no QEMU")
    print()
    print("TARGETS (exact demangled match)")
    for target in TARGETS:
        print(f"  {target.key}  state={result['state'][target.key]}  "
              f"hits={result['seen'][target.key]}")
        print(f"      regex  {target.rx.pattern}")
        print(f"      decl   {target.decl}")
        print(f"      define {target.defined_by}")
        print(f"      reach  {target.reach}")
    print()
    print("ANTI-OVERMATCH GUARD")
    for sample in GUARD_MUST_MATCH:
        ok = any(t.matches(sample) for t in TARGETS)
        print(f"  must match     {'ok  ' if ok else 'FAIL'} {sample}")
    for sample in GUARD_MUST_NOT_MATCH:
        ok = not any(t.matches(sample) for t in TARGETS)
        print(f"  must not match {'ok  ' if ok else 'FAIL'} {sample}")
    print(f"  => {'HOLDS' if not result['guard'] else 'VIOLATED'}")
    print()
    print("EFSRVMIN LINK SET (genuine/efsrv_min/build_server.sh)")
    present = {rel(p) for p in result["targets"]}
    for name in sorted(EFSRVMIN_LINK_SET):
        print(f"  {'present' if name in present else 'absent ':8} {name}")
    print()
    print(f"SCAN TARGETS ({len(result['targets'])} artifacts, "
          f"{result['scanned']} read)")
    for path in result["targets"][:20]:
        print(f"  -> {rel(path)}")
    if len(result["targets"]) > 20:
        print(f"  -> ... {len(result['targets']) - 20} more")
    for row in result["unscanned"]:
        print(f"  [unscanned] {row['path']}  {row['detail']}")


def print_verdict(result: dict, max_print: int) -> None:
    print("W5 missing-statics guard -- CActiveScheduler::iScheduler, User::iTlsIndex")
    print(f"scope: {len(result['targets'])} existing *.o/*.elf under "
          f"{', '.join(SCAN_ROOTS)} "
          f"({result['scanned']} read) in {result['elapsed']:.2f}s "
          f"(nm -C, static, no QEMU)")
    if result["broken"]:
        print(f"broken: {result['broken']}")
    if result["unscanned"]:
        print(f"unscanned: {len(result['unscanned'])} artifact(s) nm could not "
              f"read (not fatal; see --json)")
    print()

    if result["undefined"]:
        print(f"UNDEFINED ({len(result['undefined'])}) -- FAIL: needs a "
              f"definition, no TU provides one")
        for row in result["undefined"][:max_print]:
            tag = "link-set" if row["link_set"] else "other   "
            print(f"  [{tag}] {row['path']}")
            print(f"            {row['type']} {row['symbol']}")
            print(f"            {row['detail']}")
        if len(result["undefined"]) > max_print:
            print(f"  ... {len(result['undefined']) - max_print} more (see --json)")

    if result["defined"]:
        print(f"DEFINED ({len(result['defined'])}) -- OK: the static exists")
        for row in result["defined"][:max_print]:
            print(f"  {row['path']}  {row['type']} {row['symbol']}")
        if len(result["defined"]) > max_print:
            print(f"  ... {len(result['defined']) - max_print} more (see --json)")

    if result["lookalike"]:
        print(f"LOOKALIKE ({len(result['lookalike'])}) -- not a target; exact "
              f"match holds (not fatal)")
        for row in result["lookalike"][:max_print]:
            print(f"  {row['path']}  {row['type']} {row['symbol']}")
        if len(result["lookalike"]) > max_print:
            print(f"  ... {len(result['lookalike']) - max_print} more (see --json)")

    latent = [k for k, v in result["state"].items() if v == "LATENT"]
    if latent:
        print(f"LATENT -- absent from every scanned object, blocking nothing today: "
              f"{', '.join(latent)}")
        print(f"  If one ever shows as U, define it in the port-glue TU that also "
              f"defines CleanupStack::iStack (genuine/efsrv_min/efsrv_min.cpp), "
              f"per docs/MISSING_STATICS_PLAN.md")
    if not result["targets"]:
        print("no objects to scan -- nothing built under "
              f"{', '.join(SCAN_ROOTS)} (PASS by design, not SKIP)")

    fail = bool(result["undefined"]) or bool(result["broken"])
    verdict = "FAIL" if fail else "PASS"
    print()
    print(f"RESULT: {verdict} ({len(result['undefined'])} undefined, "
          f"{len(result['defined'])} defined, {len(result['lookalike'])} lookalike, "
          f"{len(result['targets'])} scanned)")


# --------------------------------------------------------------------- main --
def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="r1_missing_statics_check.py",
        description="W5: assert CActiveScheduler::iScheduler and User::iTlsIndex "
                    "are never an undefined static in a built qemu-port object. "
                    "Read-only: no make, no QEMU, no writes.",
        epilog="exit 0 = PASS, exit 1 = a target is undefined (or the matcher "
               "guard broke). --list prints a dry inventory and no verdict "
               "(always exit 0).")
    ap.add_argument("--list", action="store_true",
                    help="dry inventory of targets, guard samples and scan "
                         "set; no verdict, exit 0")
    ap.add_argument("--json", action="store_true",
                    help="machine-readable result (implies no human table)")
    ap.add_argument("--max-print", type=int, default=40, metavar="N",
                    help="cap printed rows per section (default 40)")
    args = ap.parse_args(argv)

    if not (PORT / "Makefile").is_file():
        print(f"RESULT: FAIL (0 undefined, 0 defined, 0 lookalike, 0 scanned) -- "
              f"not a qemu-port tree: {PORT}")
        return 1
    if shutil.which(NM) is None:
        # Without nm nothing can be asserted; say so loudly instead of passing.
        print(f"RESULT: FAIL (0 undefined, 0 defined, 0 lookalike, 0 scanned) -- "
              f"{NM} not found, cannot inspect objects")
        return 1

    result = scan()

    if args.json:
        print(json.dumps({
            "verdict": "FAIL" if (result["undefined"] or result["broken"]) else "PASS",
            "broken": result["broken"],
            "scanned": result["scanned"],
            "targets": [{"symbol": t.key, "regex": t.rx.pattern,
                         "state": result["state"][t.key],
                         "hits": result["seen"][t.key],
                         "declaration": t.decl, "defined_by": t.defined_by,
                         "reach": t.reach} for t in TARGETS],
            "undefined": result["undefined"],
            "defined": result["defined"],
            "lookalike": result["lookalike"],
            "unscanned": result["unscanned"],
            "guard": {"holds": not result["guard"],
                      "problems": result["guard"],
                      "must_match": list(GUARD_MUST_MATCH),
                      "must_not_match": list(GUARD_MUST_NOT_MATCH)},
            "efsrvmin_link_set": {"objects": list(EFSRVMIN_LINK_OBJECTS),
                                  "images": list(EFSRVMIN_LINK_IMAGES)},
            "scope": list(SCAN_ROOTS),
            "elapsed_s": round(result["elapsed"], 3),
        }, indent=2))
        return 1 if (result["undefined"] or result["broken"]) else 0

    if args.list:
        print_inventory(result)
        return 0

    print_verdict(result, args.max_print)
    return 1 if (result["undefined"] or result["broken"]) else 0


if __name__ == "__main__":
    sys.exit(main())
