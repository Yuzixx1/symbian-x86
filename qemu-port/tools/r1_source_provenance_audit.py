#!/usr/bin/env python3
"""r1_source_provenance_audit.py -- static provenance metric for the
genuine-userspace pivot: genuine-sourced components vs hand-written x86/PC glue.

READ-ONLY CONTRACT -- STATIC ONLY:
    This tool never writes, creates, renames, deletes or stages any file. It
    never spawns a build, never boots QEMU, never touches the network, and
    never writes inside kernelhwsrv/ (a separate, read-only git repo). Every
    fact below is derived from:
      (a) the three local status docs
            docs/GENUINE_PROGRESS.md          (§1 subsystem status table)
            docs/REMAINING_SHIMS_AUDIT.md     (§2 glue inventory, legend A/B/C)
            docs/GENUINE_BUILD_REVIEW.md      (toolchain verdict)
      (b) file lists and byte/LOC counts under qemu-port/ (genuine/build
          artifacts, glue sources) and under kernelhwsrv/ (genuine sources,
          read-only).

WHAT IT REPORTS
    1. Genuine components observed: for each genuine subsystem (EUSER, EFILE,
       EFSRV, EWSRV, ESTART, ESHELL, HAL, DOMAINMGR) whether the genuine
       kernelhwsrv source tree is present, its static source footprint, and
       whether a built artifact exists under qemu-port/genuine/build/.
    2. Largest glue files: the present files of the doc's inventory, ranked by
       bytes and tagged with legend class (A = obsolete hand-written Symbian
       replacement, B = legitimate x86/PC port bridge, C = test/research only).
    3. Retired-shim status: per class-A item, whether implementation files
       still exist and whether the Makefile still links them -- RETIRED,
       RETIRED_HEADERS (only declaration headers left), RESIDUAL_FILES (files
       on disk, nothing links them) or LIVE. Printed next to the documented
       claim so doc/tree drift is visible; the docs are dated 2026-11-24 and
       have drifted since.
    4. Metrics and a PASS/FAIL verdict. RESULT: PASS means provenance could be
       established (sources + artifacts observed, inventory parsed), not that
       anything was built or run.

DOCUMENTED LIMITATION (printed in the report as well):
    A genuine-vs-glue BYTE RATIO of the shipped image cannot be computed
    statically. It needs build artifacts -- genuine/build/obj/*.o,
    build/kernfull.elf and its link map -- because only those attribute bytes
    to compiled translation units. This tool deliberately never builds and
    never treats build outputs as provenance truth, so it reports that ratio as
    N/A and prints source-byte footprints instead.

Usage:
    python3 tools/r1_source_provenance_audit.py            # full audit + metrics
    python3 tools/r1_source_provenance_audit.py --list     # inventory only
    python3 tools/r1_source_provenance_audit.py --json     # machine-readable
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import textwrap
from pathlib import Path

# ------------------------------------------------------------------ safety --
READ_ONLY = True
assert READ_ONLY, "r1_source_provenance_audit must stay read-only"

TOOLS = Path(__file__).resolve().parent
PORT = TOOLS.parent
KHWSRV = PORT.parent / "kernelhwsrv"       # separate git repo: READ ONLY
GENUINE_BUILD = PORT / "genuine" / "build"

DOC_PROGRESS = PORT / "docs" / "GENUINE_PROGRESS.md"
DOC_SHIMS = PORT / "docs" / "REMAINING_SHIMS_AUDIT.md"
DOC_BUILD = PORT / "docs" / "GENUINE_BUILD_REVIEW.md"

SOURCE_EXTS = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".inc", ".s", ".asm",
    ".py", ".ld", ".def", ".mmp", ".mmh", ".inf", ".mrp",
}
HEADER_EXTS = (".h", ".hh", ".hpp")

LIMITATION = (
    "byte_ratio: N/A -- a genuine-vs-glue byte ratio of the shipped image needs "
    "build artifacts (genuine/build/obj/*.o, build/kernfull.elf and its link map) "
    "to attribute bytes to compiled translation units; this static tool never "
    "builds and never treats build outputs as provenance truth. Source-byte "
    "footprints are reported instead."
)

# --------------------------------------------------------------- helpers ----
def human(n: int) -> str:
    """Compact byte size, e.g. 412.1K."""
    if n >= 1024 * 1024:
        return f"{n / (1024 * 1024):.1f}M"
    if n >= 1024:
        return f"{n / 1024:.1f}K"
    return str(n)


def fit(text: str, width: int) -> str:
    """Left column that never overflows: keep the tail, elide the head."""
    return text if len(text) <= width else "\u2026" + text[len(text) - width + 1:]


def file_metrics(path: Path) -> tuple[int, int]:
    """(bytes, source_lines) for one file; line count only for source exts."""
    try:
        st = path.stat()
    except OSError:
        return 0, 0
    if path.suffix.lower() not in SOURCE_EXTS or st.st_size > 8 << 20:
        return st.st_size, 0
    try:
        return st.st_size, path.read_bytes().count(b"\n")
    except OSError:
        return st.st_size, 0


def tree_metrics(root: Path) -> tuple[int, int, int]:
    """(file_count, bytes, source_lines) over a directory tree, read-only."""
    files = size = lines = 0
    if not root.is_dir():
        return 0, 0, 0
    for path in sorted(root.rglob("*")):
        if not path.is_file() or ".git" in path.parts:
            continue
        b, ln = file_metrics(path)
        files += 1
        size += b
        lines += ln
    return files, size, lines


def doc_lines(path: Path) -> int:
    try:
        return path.read_text(errors="replace").count("\n")
    except OSError:
        return 0


def md_table_rows(text: str, want_cells: int) -> list[list[str]]:
    """Rows of a markdown table with exactly `want_cells` cells."""
    rows = []
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith("|") or set(line) <= set("|-: "):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) == want_cells:
            rows.append(cells)
    return rows


def plain(cell: str) -> str:
    """Drop markdown emphasis/backticks but keep escaped literal asterisks."""
    text = cell.replace("\\*", "\x00")
    text = text.replace("**", "").replace("`", "").replace("*", "")
    return text.replace("\x00", "*").strip()


def first_clause(cell: str, width: int) -> str:
    """Short single-line rendering of a long table cell."""
    text = re.sub(r"\s+", " ", plain(cell)).strip()
    for sep in (";", " \u2014 ", " - "):
        if sep in text:
            text = text.split(sep)[0].strip()
            break
    return text[:width] + ("\u2026" if len(text) > width else "")


def wrap_print(indent: str, text: str, width: int = 94) -> None:
    """Print a long sentence folded to terminal-friendly lines."""
    for line in textwrap.wrap(text, width=width) or [""]:
        print(f"{indent}{line}")


def expand_braces(pattern: str) -> list[str]:
    """`fs/r1_fs_core.{cpp,h}` -> `fs/r1_fs_core.cpp`, `fs/r1_fs_core.h`."""
    match = re.search(r"\{([^{}]*)\}", pattern)
    if not match:
        return [pattern]
    out: list[str] = []
    for alt in match.group(1).split(","):
        out += expand_braces(f"{pattern[:match.start()]}{alt.strip()}{pattern[match.end():]}")
    return out


def resolve_pattern(pattern: str) -> list[Path]:
    """Expand one doc path pattern to the paths that exist today.

    The audit doc's Path column mixes repo-relative paths (`user/x.cpp`) with
    bare filenames (`kern_shell.cpp`) that actually live in variant/ or user/;
    bare names are tried against those prefixes. Directories (`tests/`) count
    as present.
    """
    candidates: list[str] = []
    for pat in expand_braces(pattern):
        pat = pat.strip()
        if "/" in pat:
            candidates.append(pat)
        else:
            candidates += [f"{p}{pat}" for p in ("variant/", "user/", "tools/", "fs/", "")]
    found: set[Path] = set()
    for rel in candidates:
        try:
            for hit in PORT.glob(rel):
                if hit.is_file() or hit.is_dir():
                    found.add(hit)
        except (OSError, ValueError):
            continue
    return sorted(found)


# ------------------------------------------------- genuine component table ---
class Component:
    def __init__(self, name, role, kdirs, artifacts, doc_alias, required=False):
        self.name = name
        self.role = role
        self.kdirs = tuple(kdirs)
        self.artifacts = tuple(artifacts)
        self.doc_alias = doc_alias
        self.required = required

    def observe(self) -> dict:
        kfiles = kbytes = klines = 0
        present_dirs = []
        for rel in self.kdirs:
            path = KHWSRV / rel
            f, b, ln = tree_metrics(path)
            if path.is_dir():
                present_dirs.append(rel)
            kfiles += f
            kbytes += b
            klines += ln
        artifact = None
        for rel in self.artifacts:
            path = PORT / rel
            if path.is_file():
                size, _ = file_metrics(path)
                artifact = {"path": rel, "bytes": size,
                            "mtime": int(path.stat().st_mtime)}
                break
        return {
            "component": self.name,
            "role": self.role,
            "khw_dirs": present_dirs,
            "khw_dirs_missing": [d for d in self.kdirs if d not in present_dirs],
            "khw_files": kfiles,
            "khw_bytes": kbytes,
            "khw_lines": klines,
            "sourced": bool(present_dirs),
            "artifact": artifact,
            "artifact_candidates": list(self.artifacts),
            "required": self.required,
        }


COMPONENTS = (
    Component("EUSER", "runtime DLL", ("kernel/eka/euser",),
              ("EUSER.DLL", "genuine/build/EUSER.DLL"), "EUSER", required=True),
    Component("EFILE", "file server exe",
              ("userlibandfileserver/fileserver/sfsrv",
               "userlibandfileserver/fileserver/sfat32",
               "userlibandfileserver/fileserver/sfile",
               "userlibandfileserver/fileserver/fs_utils"),
              ("genuine/build/EFILE.E32",), "File server", required=True),
    Component("EFSRV", "file server DLL + FSY", ("userlibandfileserver/fileserver/sfsrv",),
              ("genuine/build/EFSRV.DLL", "genuine/build/EFAT32.FSY"), "File server"),
    Component("EWSRV", "window server", ("kernel/eka/ewsrv",),
              ("genuine/build/EWSRV.E32",), "Window server", required=True),
    Component("ESTART", "startup", ("userlibandfileserver/fileserver/estart",),
              ("genuine/build/estart_.E32",), "Startup", required=True),
    Component("ESHELL", "shell", ("userlibandfileserver/fileserver/etshell",),
              ("genuine/build/etshell_.E32",), "Startup", required=True),
    Component("HAL", "hal services", ("halservices/hal",),
              ("genuine/build/hal_halu.E32",), "HAL"),
    Component("DOMAINMGR", "domain manager", ("userlibandfileserver/domainmgr",),
              ("genuine/build/dm_domainsrv.E32",), "Domain manager"),
)

# Class-A items that have no genuine counterpart to retire to.
AUTHORED_EXCEPTIONS = {"netstack"}

# Which GENUINE_PROGRESS §1 row documents each class-A shim's retirement.
A_DOC_ALIAS = {
    "portfilesrv": "File server",
    "r1_fs": "File server",
    "svcmgr": "Startup",
    "winsrv": "Window server",
    "euser static shim": "EUSER",
    "netstack": None,
}


# ------------------------------------------------------------------ audit ---
def parse_progress(text: str) -> dict[str, dict]:
    """§1 subsystem rows: heading -> {built, runs, retired_shim, blocker}."""
    rows: dict[str, dict] = {}
    for cells in md_table_rows(text, 6):
        head = plain(cells[0])
        if not head or head.lower().startswith("subsystem"):
            continue
        rows[head] = {
            "genuine_source": plain(cells[1])[:70],
            "built": first_clause(cells[2], 24),
            "runs": first_clause(cells[3], 24),
            "retired_shim": first_clause(cells[4], 34),
            "blocker": first_clause(cells[5], 60),
        }
    return rows


def progress_row_for(rows: dict[str, dict], alias: str | None, extra: str = "") -> dict | None:
    if not alias and not extra:
        return None
    needles = [n.lower() for n in (alias or "", extra) if n]
    for head, row in rows.items():
        if any(n in head.lower() for n in needles):
            return row
    return None


def alias_for(item_name: str) -> str | None:
    if item_name in A_DOC_ALIAS:
        return A_DOC_ALIAS[item_name]
    for key, alias in A_DOC_ALIAS.items():
        if item_name.startswith(key):
            return alias
    return None


def parse_inventory(text: str) -> list[dict]:
    """§2 inventory rows: legend class, item name, path patterns."""
    items = []
    for cells in md_table_rows(text, 6):
        cell = plain(cells[0])
        match = re.match(r"^([ABC])\s+(.+)$", cell)
        if not match:
            continue
        cls, name = match.group(1), match.group(2).strip()
        # Parentheticals explain; tokens carrying `:1234-1300` are cited line
        # references (`e32load.cpp:1258-1452`), not inventory paths.
        path_cell = re.sub(r"\([^)]*\)", " ", cells[1])
        patterns = [p.strip() for p in re.findall(r"`([^`]+)`", path_cell)
                    if ":" not in p]
        if not patterns:
            continue
        items.append({
            "class": cls,
            "item": name,
            "patterns": patterns,
            "doc_role": first_clause(cells[2], 44),
            "doc_replacement": first_clause(cells[3], 40),
            "doc_gate": first_clause(cells[4], 30),
            "doc_need": first_clause(cells[5], 30),
        })
    return items


def observe_inventory(items: list[dict], makefile: str) -> None:
    """Annotate each inventory item with what exists on disk today."""
    for item in items:
        entries: dict[str, dict] = {}
        missing: list[str] = []
        for pattern in item["patterns"]:
            hits = resolve_pattern(pattern)
            if not hits:
                missing.append(pattern)
            for path in hits:
                rel = path.relative_to(PORT).as_posix()
                if path.is_dir():
                    count, size, lines = tree_metrics(path)
                    entries[rel] = {"path": rel, "kind": "dir", "files": count,
                                    "bytes": size, "lines": lines, "class": item["class"]}
                else:
                    size, lines = file_metrics(path)
                    entries[rel] = {
                        "path": rel, "kind": "file", "files": 1, "bytes": size,
                        "lines": lines, "class": item["class"],
                        "linked": rel in makefile or path.name in makefile,
                    }
        item["files"] = [entries[k] for k in sorted(entries)]
        item["missing"] = missing
        item["bytes"] = sum(f["bytes"] for f in item["files"])
        item["lines"] = sum(f["lines"] for f in item["files"])
        item["linked"] = [f["path"] for f in item["files"] if f.get("linked")]


def glue_files(items: list[dict]) -> list[dict]:
    """All present glue entries merged by path, with every class that claims it."""
    merged: dict[str, dict] = {}
    for item in items:
        for entry in item["files"]:
            slot = merged.setdefault(entry["path"], dict(entry, classes=set()))
            slot["classes"].add(item["class"])
    return sorted(merged.values(), key=lambda e: e["bytes"], reverse=True)


def retire_verdict(item: dict) -> str:
    """RETIRED / RETIRED_HEADERS / RESIDUAL_FILES / LIVE, or n/a for B and C."""
    if item["class"] != "A":
        return "n/a (bridge)" if item["class"] == "B" else "n/a (test)"
    if item["item"] in AUTHORED_EXCEPTIONS:
        return "KEEP_AUTHORED"
    impl = [f for f in item["files"]
            if f["kind"] == "file" and not f["path"].endswith(HEADER_EXTS)]
    headers = [f for f in item["files"]
               if f["kind"] == "file" and f["path"].endswith(HEADER_EXTS)]
    if not impl and not headers:
        return "RETIRED"
    if not impl:
        return "RETIRED_HEADERS"
    return "LIVE" if any(f.get("linked") for f in impl) else "RESIDUAL_FILES"


# ----------------------------------------------------------------- report ---
def print_header() -> None:
    print("r1_source_provenance_audit -- static provenance inventory "
          "(no build, no QEMU, read-only)")
    print(f"  port       : {PORT}")
    print(f"  kernelhwsrv: {KHWSRV} (read-only)")
    print("  docs       : "
          f"GENUINE_PROGRESS.md {doc_lines(DOC_PROGRESS)}L, "
          f"REMAINING_SHIMS_AUDIT.md {doc_lines(DOC_SHIMS)}L, "
          f"GENUINE_BUILD_REVIEW.md {doc_lines(DOC_BUILD)}L")
    print()


def print_components(observed: list[dict], progress: dict[str, dict]) -> None:
    print("1. genuine components observed (source of truth: kernelhwsrv/)")
    print(f"   {'component':<10} {'khw source':<40} {'files':>5} {'src':>6} "
          f"{'artifact':<16} {'bytes':>7}  doc(retired shim?)")
    for row in observed:
        spec = next(c for c in COMPONENTS if c.name == row["component"])
        doc = progress_row_for(progress, spec.doc_alias)
        src = fit(row["khw_dirs"][0], 40) if row["khw_dirs"] else "MISSING"
        art = row["artifact"]["path"].rsplit("/", 1)[-1] if row["artifact"] else "NOT BUILT"
        size = human(row["artifact"]["bytes"]) if row["artifact"] else "-"
        print(f"   {row['component']:<10} {src:<40} {row['khw_files']:>5} "
              f"{human(row['khw_bytes']):>6} {art:<16} {size:>7}  "
              f"{doc['retired_shim'] if doc else 'n/a'}")
    print("   (doc column is the 2026-11-24 claim; the artifact column is what "
          "is on disk now)")
    print()


def print_inventory(items: list[dict]) -> None:
    print("2. glue inventory (docs/REMAINING_SHIMS_AUDIT.md legend: "
          "A obsolete shim, B x86/PC bridge, C test only)")
    print(f"   {'cls':<4} {'item':<20} {'pats':>4} {'files':>5} {'miss':>4} "
          f"{'bytes':>7} {'lines':>6} {'linked':>6}  verdict")
    for item in items:
        sources = sum(1 for f in item["files"] if f["kind"] == "file")
        linked = f"{len(item['linked'])}/{sources}"
        print(f"   {item['class']:<4} {item['item'][:20]:<20} "
              f"{len(item['patterns']):>4} {len(item['files']):>5} "
              f"{len(item['missing']):>4} {human(item['bytes']):>7} "
              f"{item['lines']:>6} {linked:>6}  {retire_verdict(item)}")
    print()
    print("   largest glue files (classes A+B, files only, present today):")
    glue = [e for e in glue_files(items)
            if e["kind"] == "file" and e["classes"] & {"A", "B"}]
    for rank, entry in enumerate(glue[:8], 1):
        classes = "".join(sorted(entry["classes"]))
        note = "" if entry.get("linked") else "  (not in Makefile)"
        print(f"     {rank}. [{classes}] {entry['path']:<40} "
              f"{human(entry['bytes']):>7} {entry['lines']:>6} lines{note}")
    print()


def print_retirement(items: list[dict], progress: dict[str, dict]) -> None:
    print("3. retired-shim status (observed tree vs documented claim)")
    print(f"   {'item':<20} {'observed':<16} {'files':>5} {'linked':>6}  "
          f"doc claim (GENUINE_PROGRESS \u00a71)")
    retired, kept = [], []
    for item in items:
        if item["class"] != "A":
            continue
        verdict = retire_verdict(item)
        if verdict in ("RETIRED", "RETIRED_HEADERS"):
            retired.append(item["item"] + ("*" if verdict == "RETIRED_HEADERS" else ""))
        elif verdict == "KEEP_AUTHORED":
            kept.append(item["item"])
        doc = progress_row_for(progress, alias_for(item["item"]), item["item"])
        claim = doc["retired_shim"] if doc else "n/a (authored exception)"
        linked = sum(1 for f in item["files"] if f.get("linked"))
        print(f"   {item['item'][:20]:<20} {verdict:<16} {len(item['files']):>5} "
              f"{linked:>6}  {claim}")
        if 0 < linked < len(item["files"]):
            still = [f["path"] for f in item["files"] if f.get("linked")]
            shown, budget = [], 46
            for path in still:
                if len(path) + 2 > budget:
                    break
                budget -= len(path) + 2
                shown.append(path)
            hidden = len(still) - len(shown)
            listing = ", ".join(shown) + (f" (+{hidden} more)" if hidden else "")
            print(f"   {'':<44}still built: {listing}")
    if retired:
        wrap_print("   ", "doc/tree drift: docs state no shim is fully retired; "
                           f"observed RETIRED now: {', '.join(sorted(retired))} "
                           "(* = declaration headers kept)")
    else:
        print("   doc/tree drift: none -- no class-A shim is fully retired yet")
    if kept:
        print(f"   authored exceptions (never a genuine gate): {', '.join(kept)}")
    print()


def compute_metrics(observed, items, progress) -> dict:
    glue = glue_files(items)
    a_items = [i for i in items if i["class"] == "A"]
    verdicts = [(i["item"], retire_verdict(i)) for i in a_items]
    return {
        "genuine_components": len(observed),
        "genuine_sourced": sum(1 for r in observed if r["sourced"]),
        "genuine_artifacts": sum(1 for r in observed if r["artifact"]),
        "genuine_artifact_bytes": sum(r["artifact"]["bytes"] for r in observed if r["artifact"]),
        "genuine_khw_source_files": sum(r["khw_files"] for r in observed),
        "genuine_khw_source_bytes": sum(r["khw_bytes"] for r in observed),
        "glue_items": len(items),
        "glue_files_present": len(glue),
        "glue_patterns_missing": sum(len(i["missing"]) for i in items),
        "glue_bytes_present": sum(e["bytes"] for e in glue),
        "glue_lines_present": sum(e["lines"] for e in glue),
        "glue_A_bytes": sum(e["bytes"] for e in glue if "A" in e["classes"]),
        "glue_B_bytes": sum(e["bytes"] for e in glue if "B" in e["classes"]),
        "shims_retired": sum(1 for _, v in verdicts if v in ("RETIRED", "RETIRED_HEADERS")),
        "shims_live": sum(1 for _, v in verdicts if v == "LIVE"),
        "shims_residual": sum(1 for _, v in verdicts if v == "RESIDUAL_FILES"),
        "shims_authored": sum(1 for _, v in verdicts if v == "KEEP_AUTHORED"),
        "shims_total": len(a_items),
        "shim_verdicts": dict(verdicts),
        "byte_ratio": "N/A",
        "byte_ratio_reason": "needs build artifacts",
        "documented_limitation": LIMITATION,
        "doc_rows_parsed": len(progress),
    }


def run_checks(observed, items, progress) -> list[tuple[str, bool, str]]:
    required = [r for r in observed if r["required"]]
    return [
        ("docs parsed",
         bool(DOC_PROGRESS.is_file() and DOC_SHIMS.is_file() and DOC_BUILD.is_file()
              and progress and items),
         f"{len(progress)} status rows, {len(items)} inventory rows"),
        ("kernelhwsrv read-only present", KHWSRV.is_dir(), str(KHWSRV)),
        ("genuine sources located",
         bool(observed) and all(r["sourced"] for r in observed),
         f"{sum(1 for r in observed if r['sourced'])}/{len(observed)} components"),
        ("required artifacts observed",
         all(r["artifact"] for r in required),
         ", ".join(f"{r['component']}="
                   + (r["artifact"]["path"].rsplit("/", 1)[-1] if r["artifact"] else "MISSING")
                   for r in required)),
        ("glue inventory resolved",
         bool(items) and any(i["files"] for i in items),
         f"{sum(1 for i in items if i['files'])}/{len(items)} items have files on disk"),
        ("retirement state computed",
         all(retire_verdict(i) for i in items if i["class"] == "A"),
         f"{sum(1 for i in items if i['class'] == 'A')} class-A items classified"),
        ("byte-ratio limitation documented", bool(LIMITATION.strip()), "printed"),
    ]


def load() -> tuple[dict, list[dict]]:
    progress_text = DOC_PROGRESS.read_text(errors="replace") if DOC_PROGRESS.is_file() else ""
    shims_text = DOC_SHIMS.read_text(errors="replace") if DOC_SHIMS.is_file() else ""
    progress = parse_progress(progress_text)
    items = parse_inventory(shims_text)
    makefile = ""
    if (PORT / "Makefile").is_file():
        makefile = (PORT / "Makefile").read_text(errors="replace")
    observe_inventory(items, makefile)
    return progress, items


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Static provenance metric: genuine-sourced components vs "
                    "x86/PC glue. Read-only; no build, no QEMU.")
    ap.add_argument("--list", action="store_true", help="inventory only, no metrics")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    args = ap.parse_args()

    progress, items = load()
    observed = [c.observe() for c in COMPONENTS]

    if args.list:
        print_header()
        print_components(observed, progress)
        for item in items:
            print(f"   [{item['class']}] {item['item']} -- {item['doc_role']}")
            for pattern in item["patterns"]:
                hits = resolve_pattern(pattern)
                if hits:
                    size = sum(tree_metrics(h)[1] if h.is_dir() else file_metrics(h)[0]
                               for h in hits)
                    state = f"{len(hits)} path(s), {human(size)}"
                else:
                    state = "absent"
                print(f"       {pattern:<42} {state}")
        makefile = (PORT / "Makefile").read_text(errors="replace") \
            if (PORT / "Makefile").is_file() else ""
        stale = [t for t in ("winsrv", "guishost", "qemu_windowsrv", "qemu_gui_exec",
                             "svcmgr", "portfilesrv", "euser_shim") if t in makefile]
        print()
        print("   stale Makefile references to deleted shims: "
              f"{', '.join(stale) if stale else 'none'}")
        return 0

    metrics = compute_metrics(observed, items, progress)
    checks = run_checks(observed, items, progress)
    ok = all(passed for _, passed, _ in checks)

    if args.json:
        print(json.dumps({
            "port": str(PORT),
            "kernelhwsrv": str(KHWSRV),
            "docs": {"progress": str(DOC_PROGRESS), "shims": str(DOC_SHIMS),
                     "build_review": str(DOC_BUILD)},
            "components": observed,
            "inventory": items,
            "metrics": metrics,
            "checks": [{"name": n, "ok": p, "detail": d} for n, p, d in checks],
            "result": "PASS" if ok else "FAIL",
        }, indent=2))
        return 0 if ok else 1

    print_header()
    print_components(observed, progress)
    print_inventory(items)
    print_retirement(items, progress)

    print("4. metrics")
    print(f"   genuine components sourced   : {metrics['genuine_sourced']}"
          f"/{metrics['genuine_components']}"
          f" ({metrics['genuine_khw_source_files']} khw source files, "
          f"{human(metrics['genuine_khw_source_bytes'])})")
    print(f"   genuine artifacts observed   : {metrics['genuine_artifacts']}"
          f" ({human(metrics['genuine_artifact_bytes'])}: genuine/build/ + EUSER.DLL)")
    print(f"   glue files present           : {metrics['glue_files_present']}"
          f" ({human(metrics['glue_bytes_present'])}, "
          f"{metrics['glue_lines_present']} lines) -- "
          f"A {human(metrics['glue_A_bytes'])}, B {human(metrics['glue_B_bytes'])}")
    print(f"   obsolete shims retired       : {metrics['shims_retired']}"
          f"/{metrics['shims_total']} (live {metrics['shims_live']}, "
          f"residual files {metrics['shims_residual']}, "
          f"authored {metrics['shims_authored']})")
    print(f"   genuine/glue byte ratio      : {metrics['byte_ratio']} "
          f"({metrics['byte_ratio_reason']})")
    print()
    print("5. documented limitation")
    wrap_print("   ", LIMITATION)
    print()

    for name, passed, detail in checks:
        if not passed:
            print(f"   CHECK FAILED: {name} ({detail})")
    if not ok:
        failed = [n for n, p, _ in checks if not p]
        print(f"RESULT: FAIL  ({len(failed)} check(s) failed: {', '.join(failed)})")
        return 1
    print("RESULT: PASS  "
          f"(genuine {metrics['genuine_sourced']}/{metrics['genuine_components']} sourced, "
          f"{metrics['genuine_artifacts']} artifacts, "
          f"shims retired {metrics['shims_retired']}/{metrics['shims_total']}, "
          f"byte-ratio N/A -- needs build artifacts)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
