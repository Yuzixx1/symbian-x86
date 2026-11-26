#!/usr/bin/env python3
"""r1_shim_residue_sweep.py -- G12: static residue sweep for the retired
qemu-port userspace shims (docs/PLAN_SURVEY_GATES.md:78, docs/G12_GAP_NOTE.md).

WHY THIS EXISTS
    The P1 shim deletions (winsrv/GUI desktop, r1_service_host, r1_fs +
    variant/r1_fs_channel, portfilesrv artifacts) were verified only by gates
    that boot QEMU and therefore never look at what is still wired into the
    build. A partial deletion therefore read green. This gate is the cheap,
    static assertion that retired things are gone and stayed gone.

WHAT COUNTS AS RESIDUE (FAIL)
    residue = a BUILD-ACTIVE reference to a retired shim, or the retired path
    itself still sitting in the tree:

      file      a retired source path still exists (fs/r1_fs_*, services/
                r1_service_host.*, variant/r1_fs_channel.*, user/winsrv.c,
                user/wsclient.*, user/guishost.h, variant/qemu_windowsrv.*,
                variant/qemu_gui_exec.cpp, tests/r1_fs_{core,channel}).
      artifact  a retired build output / staged binary still exists
                (PORTFILES.RV.E32, build/user/PORTFILESRV.EXE,
                build/r1_fs_*.o, build/*winsrv*, build/*qemu_windowsrv*,
                build/*qemu_gui_exec*).
      rule      a Makefile / *.mk / CMakeLists line whose CODE (comments
                stripped) names a retired file, object or build variable
                -- i.e. the build still knows about the shim.
      include   a live source file #includes a retired header.

WHAT IS REPORTED BUT NOT FATAL
    * Comment-only mentions. Documentation, historical notes and the
      "// RETIRED by ..." breadcrumbs are the record of the retirement, not
      residue. They are counted as `allowed comments` and never fail the gate.
    * ADVISORY rows: a live code line naming a retired artifact with no build
      effect -- e.g. the FCopy/FileMgr probe lists still naming
      "PORTFILESRV.E32". Worth cleaning, but it neither links nor deploys the
      shim, so it must not read as build-active residue.
    * Paths in WHITELIST, each with a written reason. Only the D4 default
      (Lua freestanding shim left untouched) is whitelisted today.
    * Paths in LIVE_KEEP: the genuine r1_fs files that genuine/efsrv_min/
      efsrv_min.cpp links directly, plus its two #includes of them. They match
      the retired `fs/r1_fs_*` / `tests/r1_fs_core` patterns by name only; the
      retired thing is the r1_fs_channel wire-opcode set, not the Core/Server/
      Fat/Backend implementation it rode on. Allowed with a reason, and still
      listed by --list so the carve-out stays visible.
    * Frozen-by-policy files (exec 81/269/270 hooks, genuine/ewsrv shims,
      ports/doom/e32_shim.c) are never in the retired-name list, so the sweep
      cannot accidentally flag them.

SCOPE
    Scans the qemu-port tree only. Excluded: .git, .cache, __pycache__,
    docs/ (historical narrative), build/ (text scan; artifact globs still
    apply), wads/, disk. Sibling kernelhwsrv/ is never read or written.

GUARANTEES
    Read-only and offline: no QEMU, no subprocess, no network, no file write,
    no git, no `make`. It opens source files for reading and prints a verdict.

USAGE
    python3 tools/r1_shim_residue_sweep.py            # verdict, exit 0/1
    python3 tools/r1_shim_residue_sweep.py --list     # dry inventory, no verdict
    python3 tools/r1_shim_residue_sweep.py --json     # machine-readable
    python3 tools/r1_shim_residue_sweep.py --max-print 200
    python3 tools/r1_shim_residue_sweep.py --root PATH  # scan another tree

    DEFAULT_TESTS note: <5 s, so this belongs FIRST in r1_final_verify.py so a
    stale build wiring fails fast before any QEMU spend.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
from pathlib import Path

# ------------------------------------------------------------------ safety --
READ_ONLY = True
assert READ_ONLY, "r1_shim_residue_sweep must remain read-only"

TOOLS = Path(__file__).resolve().parent
PORT = TOOLS.parent
SELF = Path(__file__).resolve()

EXCLUDED_DIRS = {".git", ".cache", "__pycache__", "node_modules", ".opencode",
                 ".sybian", "build", "docs", "wads"}
EXCLUDED_FILE_NAMES = {"disk"}

# File types that can carry a build-active reference.
BUILD_FILE_NAMES = ("Makefile", "makefile", "GNUmakefile", "CMakeLists.txt")
BUILD_FILE_SUFFIXES = (".mk", ".mak", ".mmp")
SOURCE_SUFFIXES = (".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
                   ".S", ".s", ".inc", ".inl")

# --------------------------------------------------------------- inventory --
# Retired paths, relative to qemu-port. kind: "file" (source) or "artifact"
# (build output / staged binary). Globs are expanded, so a name scan is used
# rather than a file count (docs/PLAN_OVERNIGHT_FEASIBILITY.md "count drift").
RETIRED_PATHS = (
    # --- winsrv / GUI desktop: deleted with the EWSRV migration -------------
    ("winsrv/gui", "file", "user/winsrv.c"),
    ("winsrv/gui", "file", "user/wsclient.c"),
    ("winsrv/gui", "file", "user/wsclient.h"),
    ("winsrv/gui", "file", "user/guishost.h"),
    ("winsrv/gui", "file", "variant/qemu_windowsrv.cpp"),
    ("winsrv/gui", "file", "variant/qemu_windowsrv.h"),
    ("winsrv/gui", "file", "variant/qemu_gui_exec.cpp"),
    ("winsrv/gui", "artifact", "build/*winsrv*"),
    ("winsrv/gui", "artifact", "build/*wsclient*"),
    ("winsrv/gui", "artifact", "build/*guishost*"),
    ("winsrv/gui", "artifact", "build/*qemu_windowsrv*"),
    ("winsrv/gui", "artifact", "build/*qemu_gui_exec*"),
    # --- r1_service_host: the hand-rolled Ring-3 service directory ----------
    ("svchost", "file", "services/r1_service_host.cpp"),
    ("svchost", "file", "services/r1_service_host.h"),
    ("svchost", "file", "services/r1_service_state.h"),
    ("svchost", "artifact", "build/*r1_service_host*"),
    ("svchost", "artifact", "build/*r1_service_state*"),
    # --- r1_fs: the private R1Fs wire opcodes superseded by genuine RFile ----
    ("r1_fs", "file", "fs/r1_fs_*"),
    ("r1_fs", "file", "variant/r1_fs_channel.cpp"),
    ("r1_fs", "file", "variant/r1_fs_channel.h"),
    ("r1_fs", "file", "tests/r1_fs_core"),
    ("r1_fs", "file", "tests/r1_fs_channel"),
    ("r1_fs", "artifact", "build/r1_fs_*.o"),
    # --- portfilesrv: the hand-rolled !FileServer shim ----------------------
    ("portfilesrv", "artifact", "PORTFILES.RV.E32"),
    ("portfilesrv", "artifact", "build/PORTFILESRV.EXE"),
    ("portfilesrv", "artifact", "build/user/PORTFILESRV.EXE"),
)

# Build-active name tokens. Deliberately specific: a bare "winsrv" would match
# frozen exec-hook prose and never converge, so each retired artifact is named.
TOKENS = (
    ("winsrv/gui", re.compile(r"\bwinsrv\.c\b"), "user/winsrv.c"),
    ("winsrv/gui", re.compile(r"\bwsclient\.(?:c|h)\b"), "user/wsclient.*"),
    ("winsrv/gui", re.compile(r"\bguishost\.h\b"), "user/guishost.h"),
    ("winsrv/gui", re.compile(r"\bqemu_windowsrv\b"), "variant/qemu_windowsrv.*"),
    ("winsrv/gui", re.compile(r"\bqemu_gui_exec\b"), "variant/qemu_gui_exec.cpp"),
    ("svchost", re.compile(r"\br1_service_host\b"), "services/r1_service_host.*"),
    ("svchost", re.compile(r"\br1_service_state\b"), "services/r1_service_state.h"),
    ("r1_fs", re.compile(r"\br1_fs_[A-Za-z0-9_]+\b"), "fs/r1_fs_*, variant/r1_fs_channel.*"),
    ("r1_fs", re.compile(r"\bR1_FS_[A-Z0-9_]+\b"), "R1_FS_CHANNEL build opt-in"),
    ("portfilesrv", re.compile(r"\bPORTFILES\.RV\.E32\b"), "PORTFILES.RV.E32"),
    ("portfilesrv", re.compile(r"\bPORTFILESRV\.EXE\b", re.IGNORECASE), "PORTFILESRV.EXE"),
    ("portfilesrv", re.compile(r"\bportfilesrv\b", re.IGNORECASE), "portfilesrv staging path"),
)

GROUP_ORDER = ("winsrv/gui", "svchost", "r1_fs", "portfilesrv")

# Explicit, reviewed exceptions. (path_regex, line_regex|None, reason).
# A None line_regex means "any line of that path".
WHITELIST = (
    (re.compile(r"^ports/lua/e32_shim\.(?:c|h)$"), None,
     "D4 default: Lua freestanding shim left untouched; LUA.E32 fate is an open decision"),
    (re.compile(r"^user/user\.mk$"), re.compile(r"ports/lua/", re.IGNORECASE),
     "D4 default: LUA.E32 wiring into ports/lua, not a retired shim"),
    (re.compile(r"^genuine/efsrv_min/efsrv_min\.cpp$"),
     re.compile(r'^\s*#\s*include\s*"\.\./fs/r1_fs_(?:core|server)\.h"'),
     "genuine efsrv_min host entry point includes the live R1Fs::Core/Server "
     "declarations, not the retired channel headers"),
)

# Genuine keepers: the r1_fs implementation itself, still built and linked by
# genuine/efsrv_min/efsrv_min.cpp (fs/r1_fs_core.o + fs/r1_fs_server.o sit in
# that directory). These only collide with the retired `fs/r1_fs_*` glob and
# the `tests/r1_fs_core` test directory by name. Matched against the
# tree-relative path of a retired-path finding; a hit is ALLOWED, never FAIL.
# Deliberately NOT listed: fs/r1_fs_core.mmp (the orphan .mmp project file),
# fs/r1_fs_channel*, fs/r1_fs_channel_state.h, fs/r1_fs_wire_logic.h,
# variant/r1_fs_channel.*, tests/r1_fs_channel -- that is the retired set.
LIVE_KEEP = (
    (re.compile(r"^fs/r1_fs_core\.(?:cpp|h)$"),
     "live R1Fs::Core implementation; efsrv_min.cpp includes fs/r1_fs_core.h"),
    (re.compile(r"^fs/r1_fs_server\.(?:cpp|h)$"),
     "live R1Fs::Server (genuine CServer2/CSession2); efsrv_min links "
     "fs/r1_fs_server.cpp directly and includes fs/r1_fs_server.h"),
    (re.compile(r"^fs/r1_fs_backend\.h$"),
     "live R1Fs::Backend contract header used by the efsrv_min port"),
    (re.compile(r"^fs/r1_fs_fat\.(?:cpp|h)$"),
     "live FAT backend behind the efsrv_min port; no channel opcodes"),
    (re.compile(r"^tests/r1_fs_core$"),
     "unit test of the live R1Fs::Core, not the retired channel test"),
)


# ------------------------------------------------------------------ helpers --
def rel(path: Path) -> str:
    return path.relative_to(PORT).as_posix()


def classify(path: Path) -> str:
    """'build', 'source' or 'other' for a regular file."""
    name = path.name
    if name in BUILD_FILE_NAMES or name.startswith("Makefile"):
        return "build"
    if name.endswith(BUILD_FILE_SUFFIXES):
        return "build"
    if name.endswith(SOURCE_SUFFIXES):
        return "source"
    return "other"


def strip_build_comment(line: str) -> str:
    """Drop a Makefile/CMake comment (unescaped '#'), keep the code part."""
    return re.sub(r"(?<!\\)#.*$", "", line)


def strip_c_like(line: str, in_block: bool) -> tuple[str, bool]:
    """Return (code part, still-in-block-comment) for one C/C++ source line."""
    out: list[str] = []
    i, n = 0, len(line)
    quote = ""
    while i < n:
        c = line[i]
        if in_block:
            if c == "*" and i + 1 < n and line[i + 1] == "/":
                in_block = False
                i += 2
                out.append("  ")
            else:
                i += 1
            continue
        if quote:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(line[i + 1])
                i += 2
                continue
            if c == quote:
                quote = ""
            i += 1
            continue
        if c in "\"'":
            quote = c
            out.append(c)
            i += 1
            continue
        if c == "/" and i + 1 < n:
            if line[i + 1] == "/":
                break
            if line[i + 1] == "*":
                in_block = True
                i += 2
                continue
        out.append(c)
        i += 1
    return "".join(out), in_block


def iter_scan_files():
    """Yield (path, kind) for every text file in scope, deterministically."""
    found: list[tuple[Path, str]] = []
    for root, dirs, files in os.walk(PORT):
        dirs[:] = sorted(d for d in dirs if d not in EXCLUDED_DIRS)
        for name in sorted(files):
            if name in EXCLUDED_FILE_NAMES:
                continue
            path = Path(root) / name
            if path.resolve() == SELF:
                continue
            kind = classify(path)
            if kind == "other":
                continue
            found.append((path, kind))
    return found


def expand_retired() -> list[tuple[str, str, str, list[Path]]]:
    """[(group, kind, pattern, matched paths)] for the retired-path inventory."""
    out = []
    for group, kind, pattern in RETIRED_PATHS:
        matches = sorted(PORT.glob(pattern)) if any(ch in pattern for ch in "*?[") \
            else ([PORT / pattern] if (PORT / pattern).exists() else [])
        out.append((group, kind, pattern, matches))
    return out


def retired_files() -> set[str]:
    """Every file considered retired: direct matches plus files under a
    retired directory (tests/r1_fs_core, tests/r1_fs_channel)."""
    out: set[str] = set()
    for _, _, _, matches in expand_retired():
        for path in matches:
            if path.is_dir():
                out.update(rel(p) for p in path.rglob("*") if p.is_file())
            elif path.is_file():
                out.add(rel(path))
    return out


def whitelisted(rel_path: str, line_text: str) -> str:
    for path_rx, line_rx, reason in WHITELIST:
        if not path_rx.search(rel_path):
            continue
        if line_rx is not None and not line_rx.search(line_text):
            continue
        return reason
    return ""


def live_keep(rel_path: str) -> str:
    """Reason if a retired-path finding is actually a genuine keeper."""
    for path_rx, reason in LIVE_KEEP:
        if path_rx.match(rel_path):
            return reason
    return ""


# --------------------------------------------------------------------- scan --
def sweep() -> dict:
    started = time.monotonic()
    retired = expand_retired()
    # Retired files themselves are reported once, as `file`/`artifact`; the
    # #includes inside them are not separate residue (noise, not signal).
    retired_set = retired_files()

    findings: list[dict] = []      # build-active residue -> FAIL
    advisory: list[dict] = []      # live code mention, no build effect
    allowed: list[dict] = []
    per_group_allowed = {g: 0 for g in GROUP_ORDER}

    # 1. retired paths still present
    for group, kind, pattern, matches in retired:
        for path in matches:
            rpath = rel(path)
            keep = live_keep(rpath)
            if keep:
                allowed.append({"group": group, "path": rpath, "line": 0,
                                "text": "", "reason": keep,
                                "detail": f"genuine keeper [live-keep: {keep}]"})
                per_group_allowed[group] = per_group_allowed.get(group, 0) + 1
                continue
            findings.append({
                "kind": kind,
                "group": group,
                "path": rpath,
                "line": 0,
                "text": "",
                "detail": f"retired {kind} still present ({pattern})",
            })

    # 2. build-active references, per line
    for path, fkind in iter_scan_files():
        rpath = rel(path)
        if rpath in retired_set:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError as exc:                       # unreadable: report, never crash
            findings.append({"kind": "ref", "group": "r1_fs", "path": rpath, "line": 0,
                             "text": "", "detail": f"unreadable scan target: {exc}"})
            continue
        in_block = False
        for lineno, raw in enumerate(text.splitlines(), 1):
            if fkind == "build":
                code = strip_build_comment(raw)
            else:
                code, in_block = strip_c_like(raw, in_block)
            hits = [detail for group, rx, detail in TOKENS if rx.search(code)]
            if not hits:
                if fkind == "build":
                    comment_only = strip_build_comment(raw) != raw and raw.strip()
                else:
                    comment_only = code.strip() != raw.strip() and raw.strip()
                if not comment_only:
                    continue
                hit_comments = list(dict.fromkeys(g for g, rx, _ in TOKENS
                                                  if rx.search(raw)))
                if not hit_comments:
                    continue
                for group in hit_comments:
                    per_group_allowed[group] = per_group_allowed.get(group, 0) + 1
                # One entry per comment LINE, never one per matched token.
                allowed.append({"group": "/".join(hit_comments), "path": rpath,
                                "line": lineno, "text": raw.strip()[:160]})
                continue
            group = next(g for g, rx, _ in TOKENS if rx.search(code))
            reason = whitelisted(rpath, code)
            is_include = bool(re.match(r"\s*#\s*include\b", code))
            entry = {"kind": "include" if is_include else
                     ("rule" if fkind == "build" else "ref"),
                     "group": group, "path": rpath, "line": lineno,
                     "text": code.strip()[:160],
                     "detail": "; ".join(dict.fromkeys(hits))}
            if reason:
                entry["detail"] += f" [whitelisted: {reason}]"
                allowed.append({"group": group, "path": rpath, "line": lineno,
                                "text": raw.strip()[:160], "reason": reason})
                per_group_allowed[group] = per_group_allowed.get(group, 0) + 1
            elif entry["kind"] == "ref":
                # Live code naming a retired artifact but with no build effect
                # (e.g. a probe list of FAT file names). Reported, never fatal:
                # residue is defined as build rules + #includes of the retired
                # headers + the retired paths themselves.
                advisory.append(entry)
            else:
                findings.append(entry)

    return {
        "findings": findings,
        "advisory": advisory,
        "allowed": allowed,
        "per_group_allowed": per_group_allowed,
        "retired": retired,
        "elapsed": time.monotonic() - started,
    }


# ------------------------------------------------------------------- output --
def print_inventory(result: dict) -> None:
    print(f"G12 residue sweep -- dry inventory (no verdict) -- {PORT}")
    print("scope: qemu-port sources + build files; excluded "
          + ", ".join(sorted(EXCLUDED_DIRS)) + ", docs narrative")
    print()
    print("RETIRED PATHS")
    for group, kind, pattern, matches in result["retired"]:
        state = f"PRESENT ({len(matches)})" if matches else "gone"
        print(f"  [{kind:8}] {group:12} {pattern:44} {state}")
        for path in matches[:12]:
            print(f"             -> {rel(path)}")
        if len(matches) > 12:
            print(f"             -> ... {len(matches) - 12} more")
    print()
    print("ALLOWED COMMENTS BY GROUP")
    for group in GROUP_ORDER:
        print(f"  {group:12} {result['per_group_allowed'].get(group, 0)}")
    print()
    print("WHITELIST")
    for path_rx, line_rx, reason in WHITELIST:
        print(f"  {path_rx.pattern}  line={line_rx.pattern if line_rx else '*'}")
        print(f"      {reason}")
    print()
    print("LIVE KEEP (genuine files carved out of the retired set, non-fatal)")
    present = sorted({rel(p) for _, _, _, ms in result["retired"] for p in ms})
    for path_rx, reason in LIVE_KEEP:
        hits = [p for p in present if path_rx.match(p)]
        state = f"PRESENT ({', '.join(hits)})" if hits else "absent"
        print(f"  {path_rx.pattern}  {state}")
        print(f"      {reason}")
    print()
    print(f"tallied in {result['elapsed']:.2f}s -- no verdict printed (--list)")


def print_verdict(result: dict, max_print: int) -> None:
    findings = result["findings"]
    n_allowed = len(result["allowed"])
    print(f"G12 residue sweep -- {PORT}")
    print(f"scanned sources + build files in {result['elapsed']:.2f}s "
          f"(static, no QEMU)")
    if findings:
        print()
        print(f"RESIDUE ({len(findings)})")
        for i, f in enumerate(findings):
            if i >= max_print:
                print(f"  ... {len(findings) - max_print} more (see --json)")
                break
            loc = f"{f['path']}:{f['line']}" if f["line"] else f["path"]
            print(f"  [{f['kind']:7}] {f['group']:12} {loc}")
            if f["text"]:
                print(f"            {f['text']}")
            print(f"            {f['detail']}")
    if result["advisory"]:
        print()
        print(f"ADVISORY ({len(result['advisory'])}) -- live code names a retired "
              f"artifact, no build effect (not residue)")
        for a in result["advisory"][:max_print]:
            print(f"  {a['path']}:{a['line']}  {a['text']}")
    if result["allowed"]:
        print()
        print(f"ALLOWED ({n_allowed}) -- comment-only mentions + whitelisted refs")
        for a in result["allowed"][:min(max_print, 5)]:
            if a["line"]:
                print(f"  {a['path']}:{a['line']}  {a['text']}")
            else:
                print(f"  {a['path']}  [allowed: {a.get('reason', '')}]")
        if n_allowed > min(max_print, 5):
            print(f"  ... {n_allowed - min(max_print, 5)} more")
    verdict = "PASS" if not findings else "FAIL"
    print()
    print(f"RESULT: {verdict} ({len(findings)} residue, {n_allowed} allowed comments)")


# --------------------------------------------------------------------- main --
def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="r1_shim_residue_sweep.py",
        description="G12: static residue sweep for retired qemu-port shims "
                    "(winsrv/GUI, r1_service_host, r1_fs/variant channel, "
                    "portfilesrv). Read-only: no QEMU, no build, no writes.",
        epilog="exit 0 = PASS, exit 1 = residue found. --list prints a dry "
               "inventory and no verdict (always exit 0).")
    ap.add_argument("--list", action="store_true",
                    help="dry inventory of retired paths and allowed comments; "
                         "no verdict, exit 0")
    ap.add_argument("--json", action="store_true",
                    help="machine-readable findings (implies no human table)")
    ap.add_argument("--max-print", type=int, default=40, metavar="N",
                    help="cap printed findings (default 40)")
    ap.add_argument("--root", metavar="PATH", default=None,
                    help="scan the tree at PATH instead of the one this script "
                         "sits in (default: parent of tools/)")
    args = ap.parse_args(argv)

    global PORT
    if args.root:
        PORT = Path(os.path.expanduser(args.root)).resolve()

    if not (PORT / "Makefile").is_file():
        print(f"RESULT: FAIL (0 residue, 0 allowed comments) -- not a qemu-port tree: {PORT}")
        return 1

    result = sweep()
    if args.json:
        print(json.dumps({
            "verdict": "PASS" if not result["findings"] else "FAIL",
            "residue": len(result["findings"]),
            "allowed_comments": len(result["allowed"]),
            "per_group_allowed": result["per_group_allowed"],
            "findings": result["findings"],
            "whitelist": [{"path": p.pattern,
                           "line": l.pattern if l else None,
                           "reason": r} for p, l, r in WHITELIST],
            "live_keep": [{"path": p.pattern, "reason": r}
                          for p, r in LIVE_KEEP],
            "elapsed_s": round(result["elapsed"], 3),
        }, indent=2))
        return 0 if not result["findings"] else 1
    if args.list:
        print_inventory(result)
        return 0
    print_verdict(result, args.max_print)
    return 0 if not result["findings"] else 1


if __name__ == "__main__":
    sys.exit(main())
