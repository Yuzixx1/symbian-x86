#!/usr/bin/env python3
"""r1_wave2_s3.py -- GUARDED multi-file delete for WAVE2 / S3
(docs/S3_PLAN.md: the 9 orphan R1Fs channel residue paths, 12 files).

SPENT -- S3 IS ALREADY APPLIED.  None of the 12 residue files, and not even
tests/r1_fs_channel/, exists any more, and the G12 residue count is 0 on this
tree, so every path, count and Makefile reference below is HISTORICAL
pre-state documentation kept for the review trail.  The tool now refuses with
exit 2 ("already applied/spent") before it parses arguments, so no mode --
--plan, --dry-run, --help, --apply or --rollback -- can act on this tree.
Nothing is deleted by that refusal.

Small add-only companion to tools/r1_wave2_delete.py (S1) and
tools/r1_winsrv_delete.py.  It removes EXACTLY the S3 residue set and nothing
else.  It REUSES the proven guards BY IMPORT (never copied): sha256,
safe_target, git_porcelain, kernelhwsrv_clean and the /tmp/opencode
BACKUP_ROOT layout from r1_winsrv_delete, and the whole gate ladder
(gate_g12 / gate_retire / gate_fullchain / gate_strict / run_gate_ladder) from
r1_wave2_delete.  Not one guard is re-implemented here.

HARD CONTRACT
  * The ONLY paths this tool can ever remove are the 12 files below plus the
    now-empty tests/r1_fs_channel/ directory (os.remove / os.rmdir only -- no
    rm, no glob, no recursion).  Any positional argument, any unknown flag and
    any --rollback dir inside qemu-port/ is refused (exit 2) before a single
    write, so one typo or glob can never reach the 7 LIVE fs/r1_fs_* keepers,
    fs/r1_fs_core.mmp (S1's), tests/r1_fs_core/, the docs/ history files or
    kernelhwsrv/.
  * No arguments (or --plan / --dry-run) is a pure preview: static facts plus
    the current G12 residue, no QEMU, no writes, no gate ladder.
  * A delete requires BOTH --apply AND --confirm DELETE-WAVE2-S3-d861b6e1.
    The slug is content-bound by the plan over the 12 sorted
    "<sha256>  <path>" lines; this tool additionally enforces that content
    binding itself via SCOPE_DIGEST (see the NOTE below), so a wave-2 confirm
    can never delete these paths and DELETE-WAVE2-S3-* can never delete S1's
    or a winsrv path.
  * Order is load-bearing (docs/S3_PLAN.md): S1 (fs/r1_fs_core.mmp) and S2
    (the 8 Makefile [rule] hits) must have landed, so the pre-gate G12 residue
    count must be EXACTLY 9 -- 18 was NO-GO -- before the unlink, together
    with retire 4/4, fullchain 14/14 and strict 11/11.  HISTORICAL pre-state:
    S1, S2 and S3 have all landed and the count is 0 today.  Otherwise exit 3
    and nothing is touched.
  * All 12 files are snapshotted to /tmp/opencode/wave2_s3_backup_<stamp>/ with
    an r1-wave2-delete-backup/v1 manifest and rollback token
    ROLLBACK-WAVE2-S3-<stamp>; every copy's sha256 is verified at copy time.
  * After the delete the post-gate ladder must show G12 residue EXACTLY 0
    (1 or more is NO-GO) plus strict 11/11, retire 4/4, fullchain 14/14.  That
    is the state this tree is in today (S3 spent).  The
    exit code alone is never evidence: the sweep still exits 1 while residue
    remains, so the count is what counts.  Any shortfall auto-rolls back from
    the manifest and re-gates.
  * NEVER touches kernelhwsrv/ (READ-ONLY), NEVER commits or pushes, NEVER runs
    `make clean`, NEVER a bare `rm -rf`/glob delete, NEVER boots QEMU.

NOTE on the confirm slug: docs/S3_PLAN.md fixes the token as
DELETE-WAVE2-S3-d861b6e1, described as sha256[:8] over the 12 sorted
"<sha256>  <path>" lines.  The straightforward serialization of that recipe
("\\n".join(...) + "\\n", paths sorted, two spaces) digests to affa56c7, not
d861b6e1, and no separator/prefix/sort/terminator variant reproduces the
documented slug.  This tool therefore keeps the documented literal token (so
it matches the plan and the review trail) AND independently enforces the
content binding through SCOPE_DIGEST == scope_digest() over the same 12 lines:
any byte change in any of the 12 files breaks the pre-state check and refuses
(exit 2) before the snapshot.  Re-run `--plan` to print the live digest.

Exit codes: 0 preview/plan success (pre-state trees only); 2 refusal (bad
args/token/dirty target/unsafe or foreign path/forbidden argv/content drift,
or the tree is already spent); 3 pre-apply gate ladder
not PASS; 4 write error (rolled back); 5 post-delete gate ladder not PASS
(rolled back); 6 rollback FAILED (manual restore required).

Usage (HISTORICAL -- every line below now refuses with rc 2 on this tree,
because S3 is spent; none of them touches a file any more):
  python3 tools/r1_wave2_s3.py --plan        # was: preview + residue, rc 0
  python3 tools/r1_wave2_s3.py               # same preview, then REFUSE (rc 2)
  python3 tools/r1_wave2_s3.py --apply --confirm DELETE-WAVE2-S3-d861b6e1
  python3 tools/r1_wave2_s3.py --rollback /tmp/opencode/wave2_s3_backup_<stamp> \\
      --confirm ROLLBACK-WAVE2-S3-<stamp>
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import sys
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
PORT = TOOLS.parent
REPO = PORT.parent

# Reuse the proven guards from the winsrv deletion tool BY IMPORT (never copied).
sys.path.insert(0, str(TOOLS))
import r1_winsrv_delete as wsd  # noqa: E402
import r1_wave2_delete as w2d  # noqa: E402

sha256 = wsd.sha256
safe_target = wsd.safe_target
git_porcelain = wsd.git_porcelain
kernelhwsrv_clean = wsd.kernelhwsrv_clean
BACKUP_ROOT = wsd.BACKUP_ROOT          # /tmp/opencode

# The whole gate ladder is reused from the S1 tool; only its log dir differs so
# S3 evidence never overwrites S1's.  run_gate_ladder takes the G12 expectation
# as a parameter, so nothing else needs touching.
GATE_DIR = BACKUP_ROOT / "wave2_s3_gate"
w2d.GATE_DIR = GATE_DIR
gate_g12 = w2d.gate_g12
gate_retire = w2d.gate_retire
gate_fullchain = w2d.gate_fullchain
gate_strict = w2d.gate_strict
run_gate_ladder = w2d.run_gate_ladder
run_cmd = w2d.run_cmd

# ---------------------------------------------------------------- op set ---
# The 12 files of the S3 residue set (docs/S3_PLAN.md table, sha256 verified
# live).  `path: (sha256, size)` -- both must match before the snapshot AND
# again immediately before each unlink.
FILES: dict[str, tuple[str, int]] = {
    "fs/r1_fs_channel.cpp": ("68c27fade26c", 6518),
    "fs/r1_fs_channel.h": ("f89aa078aad5", 2959),
    "fs/r1_fs_channel_eka.cpp": ("f5a5cb29f14e", 1860),
    "fs/r1_fs_channel_eka.h": ("e70d3cde7141", 960),
    "fs/r1_fs_channel_state.h": ("1d5588725737", 5445),
    "fs/r1_fs_wire_logic.h": ("9c3aaa1973dd", 2217),
    "variant/r1_fs_channel.cpp": ("b7618402534d", 4182),
    "variant/r1_fs_channel.h": ("65849335346f", 528),
    "tests/r1_fs_channel/README.md": ("61c6e461620f", 2735),
    "tests/r1_fs_channel/adapter_test.cpp": ("34ae6d68d3fc", 5444),
    "tests/r1_fs_channel/state_test.cpp": ("f4b75dd3e961", 7658),
    "tests/r1_fs_channel/wire_test.cpp": ("8f3e3ca8b73a", 3484),
}

# The one directory op: tests/r1_fs_channel/ is residue as a DIR (the sweep
# reports the path itself), so it is os.rmdir-ed once its 4 files are gone.
DIRS = ("tests/r1_fs_channel",)

# The exact-selection scope: the 9 residue paths G12 reports once S1 and S2
# have landed (6 fs/ files, 2 variant/ files, 1 tests/ directory).  No globs.
DELETE_SET = frozenset(set(FILES) | set(DIRS))
RESIDUE_PATHS = frozenset({
    "fs/r1_fs_channel.cpp", "fs/r1_fs_channel.h", "fs/r1_fs_channel_eka.cpp",
    "fs/r1_fs_channel_eka.h", "fs/r1_fs_channel_state.h",
    "fs/r1_fs_wire_logic.h", "variant/r1_fs_channel.cpp",
    "variant/r1_fs_channel.h", "tests/r1_fs_channel",
})

# NEVER in any op (docs/S3_PLAN.md "Never touched"): the 7 G12 LIVE_KEEP files,
# their real test dir, S1's orphan .mmp, the docs/ history files, kernelhwsrv/.
LIVE_KEEP = frozenset({
    "fs/r1_fs_core.cpp", "fs/r1_fs_core.h",
    "fs/r1_fs_server.cpp", "fs/r1_fs_server.h",
    "fs/r1_fs_backend.h", "fs/r1_fs_fat.cpp", "fs/r1_fs_fat.h",
})
KEEP_DIRS = ("tests/r1_fs_core",)
KEEP_PATHS = LIVE_KEEP | {"fs/r1_fs_core.mmp"} | set(KEEP_DIRS)
KEEP_PREFIXES = ("kernelhwsrv/", "docs/")

KEPT_NOTE = ("KEPT (LIVE genuine efsrv_min + history): fs/r1_fs_core.cpp/_core.h/"
             "_server.cpp/_server.h/_backend.h/_fat.cpp/_fat.h + tests/r1_fs_core "
             "(G12 LIVE_KEEP), fs/r1_fs_core.mmp (S1's orphan MMP), "
             "docs/r1_fs_channel_*.md + docs/r1_wire_research_v0.md (history; "
             "G12 skips docs/), kernelhwsrv/ (READ-ONLY)")

CONFIRM_TOKEN = "DELETE-WAVE2-S3-d861b6e1"
# sha256[:8] over the 12 sorted "<sha256>  <path>" lines (see the NOTE above).
SCOPE_DIGEST = "affa56c7"
BACKUP_SCHEMA = "r1-wave2-delete-backup/v1"
BACKUP_PREFIX = "wave2_s3_backup_"
ROLLBACK_PREFIX = "ROLLBACK-WAVE2-S3-"
TMP_SUFFIX = ".wave2_s3_tmp"

# Gate ladder (docs/S3_PLAN.md "Gates, pre and post").
G12_PRE = 9                             # HISTORICAL: residue once S1 + S2 landed
G12_POST = 0                            # EXACTLY 0, never 1 (reached today)
RETIRE_EXPECT = 4                       # retire 4/4
FULLCHAIN_EXPECT = (14, 14)             # fullchain 14/14 rows
STRICT_EXPECT = 11                      # final_verify --strict-skip 11/11

# argv guard: this tool's own source legitimately calls os.remove/os.rmdir for
# the allowlisted ops, so the guard scans ARGV only, never the source.
FORBIDDEN_ARGV = ("git commit", "git push", "make clean", "rm -rf", "rm -r ")
FLAGS = {"--apply", "--plan", "--dry-run", "-h", "--help"}
VALUE_FLAGS = ("--rollback", "--confirm")

# ---------------------------------------------------------------- spent ---
# S3 is SPENT.  The probe only stats the compile-time op set -- it never
# writes, deletes or spawns anything -- so it is safe to run before argparse:
# on a spent tree no mode is meaningful, and --plan / --dry-run / --help must
# refuse as well.
SPENT_RC = 2
SPENT_MSG = ("REFUSE: S3 is already applied/spent -- none of the 9 residue "
             "paths nor tests/r1_fs_channel/ is left and the G12 residue count "
             "is 0 on this tree.  The 9/0 ladder, the per-file sha256s, the "
             "scope digest and the DELETE-WAVE2-S3 token are historical "
             "pre-state only; there is nothing left to delete, and "
             "--plan/--help/--apply/--rollback are all refused here.  "
             "Nothing changed.")


def spent_evidence(root: Path | None = None) -> list[str]:
    """Reasons S3 counts as spent; empty list means a live pre-state tree."""
    base = PORT if root is None else root
    present = [rel for rel in sorted(DELETE_SET) if (base / rel).exists()]
    if present:
        return []
    return [f"all {len(DELETE_SET)} compile-time S3 op paths absent"]


def refuse_if_spent(root: Path | None = None) -> int | None:
    """Exit code 2 if S3 is already applied, else None to carry on."""
    why = spent_evidence(root)
    if not why:
        return None
    print(SPENT_MSG, file=sys.stderr)
    print("  evidence: " + "; ".join(why), file=sys.stderr)
    return SPENT_RC


# ------------------------------------------------------- scope invariants ---
def forbidden_path(rel: str) -> str:
    """Reason if `rel` must never be deleted, else ''."""
    if not isinstance(rel, str) or not rel:
        return "empty path"
    p = Path(rel)
    if p.is_absolute() or ".." in p.parts:
        return "absolute path or '..' component"
    if rel in KEEP_PATHS:
        return "LIVE_KEEP / S1 / history path"
    if any(rel.startswith(pre) for pre in KEEP_PREFIXES):
        return "outside the qemu-port source set (kernelhwsrv/ or docs/)"
    if any(rel == d or rel.startswith(d + "/") for d in KEEP_DIRS):
        return "under a KEPT test directory"
    return ""


def scope_digest() -> str:
    """sha256[:8] over the 12 sorted '<sha256>  <path>' lines (live files)."""
    lines = []
    for rel in sorted(FILES):
        p = PORT / rel
        lines.append(f"{sha256(p) if p.is_file() else 'MISSING':>64}  {rel}")
    blob = "\n".join(lines) + "\n"
    return hashlib.sha256(blob.encode()).hexdigest()[:8]


def _assert_scope() -> None:
    """Fail closed at import: the op set must equal the 9 residue paths and
    must not intersect anything we promise never to touch."""
    assert RESIDUE_PATHS <= DELETE_SET, "DELETE_SET is missing residue paths"
    for rel in DELETE_SET:
        why = forbidden_path(rel)
        assert not why, f"op set contains a protected path: {rel} ({why})"
        assert safe_target(rel), f"op set path outside qemu-port/: {rel}"
    assert not (DELETE_SET & KEEP_PATHS), "op set overlaps LIVE_KEEP/S1/history"


_assert_scope()


# ----------------------------------------------------------- static state ---
def file_state() -> dict:
    """Read-only facts about the 12 files + the dir; nothing is written."""
    out: dict[str, dict] = {}
    for rel, (want_sha, want_size) in FILES.items():
        p = PORT / rel
        info: dict = {"path": rel, "exists": p.is_file(),
                      "expected_sha256": want_sha, "expected_size": want_size}
        if info["exists"]:
            got = sha256(p)
            # want_sha is the plan's sha256[:12] prefix; compare the prefix.
            info.update(size=p.stat().st_size, sha256=got,
                        sha256_ok=got.startswith(want_sha),
                        size_ok=p.stat().st_size == want_size)
        else:
            info.update(sha256_ok=False, size_ok=False)
        out[rel] = info

    dirs: dict[str, dict] = {}
    for rel in DIRS:
        p = PORT / rel
        present = p.is_dir()
        members = sorted(x.name for x in p.iterdir()) if present else []
        dirs[rel] = {"path": rel, "exists": present, "members": members,
                     "members_ok": members == sorted(Path(f).name for f in FILES
                                                     if Path(f).parent.as_posix() == rel)}
    return {"files": out, "dirs": dirs}


def targets_git_clean() -> tuple[bool, str]:
    """REFUSE if any op target is already modified (no stacking deletes)."""
    paths = [str(Path("qemu-port") / rel) for rel in sorted(DELETE_SET)]
    return git_porcelain(paths)


def argv_refusals(argv: list[str]) -> list[str]:
    """Refuse any argv token that is not this tool's own flag.

    This tool takes NO positional argument: not a path, not a glob, not a
    residue path.  Selection is compile-time (DELETE_SET), so one typo cannot
    widen or redirect the delete.
    """
    bad: list[str] = []
    skip_flag = ""
    for tok in argv[1:]:
        if skip_flag:
            if skip_flag == "--rollback" and _inside_port(tok):
                bad.append(f"{skip_flag} {tok}")
            skip_flag = ""
            continue
        if tok in VALUE_FLAGS:
            skip_flag = tok
            continue
        matched = False
        for flag in VALUE_FLAGS:
            if tok.startswith(flag + "="):
                value = tok[len(flag) + 1:] or "."
                if flag == "--rollback" and _inside_port(value):
                    bad.append(tok)
                matched = True
                break
        if matched:
            continue
        if tok in FLAGS:
            continue
        bad.append(tok)                  # unknown flag, a path, a glob, a typo
    return bad


def _inside_port(path: str) -> bool:
    p = Path(path)
    if not p.is_absolute():
        p = PORT / p
    try:
        rp = p.resolve()
    except OSError:
        return True
    return rp == PORT.resolve() or PORT.resolve() in rp.parents


def refuse_if_foreign(argv: list[str]) -> int | None:
    """Shared pre-parse refusal; returns an exit code or None to continue."""
    joined = " ".join(argv)
    if any(v in joined for v in FORBIDDEN_ARGV):
        print("REFUSE: forbidden operation requested in argv "
              "(git commit / git push / make clean / rm -rf). Nothing changed.",
              file=sys.stderr)
        return 2
    bad = argv_refusals(argv)
    if bad:
        print(f"REFUSE: argument(s) outside this tool's flags -- the {len(DELETE_SET)} "
              f"op paths are compile-time and no positional arg is accepted: "
              f"{bad!r}. Nothing changed.", file=sys.stderr)
        return 2
    return None


# ------------------------------------------------------------------ gates ---
def current_residue() -> tuple[int | None, str]:
    """Informational read-only G12 residue count for the preview (no gate)."""
    try:
        proc = run_cmd([sys.executable, "tools/r1_shim_residue_sweep.py", "--json"],
                       "info G12", timeout=180)
        data = json.loads(proc.stdout)
        return int(data["residue"]), str(data.get("verdict", "?"))
    except (OSError, ValueError, KeyError, TypeError, RuntimeError) as exc:
        return None, f"unavailable: {exc}"


# ------------------------------------------------------------ plan/print ---
def print_plan(pre: dict, residue: tuple[int | None, str]) -> None:
    st, ds = pre["files"], pre["dirs"]
    print("r1_wave2_s3 -- PLAN: no file is changed; no QEMU is booted by this mode.")
    print(f"workspace: {PORT}")
    print(f"kernelhwsrv/ clean: {pre['kernelhwsrv']}")
    print(f"op targets git state: {pre['git']}")
    print(f"scope digest (live): {scope_digest()}   expected: {SCOPE_DIGEST}"
          f"  -> {'MATCH' if scope_digest() == SCOPE_DIGEST else 'DRIFT (apply refuses)'}")
    print(f"G12 residue now: {residue[0]} ({residue[1]}) -- apply needs exactly "
          f"{G12_PRE} (S1 + S2 landed)")

    print(f"\n=== EXACT OPS ({len(FILES)} deletes + {len(DIRS)} rmdir) ===")
    for i, rel in enumerate(sorted(FILES), 1):
        info = st[rel]
        ok = "MATCH" if info["sha256_ok"] and info["size_ok"] else "MISMATCH"
        print(f"  D{i:<2} delete  {rel}  {info['expected_size']} B  "
              f"sha256 {info['expected_sha256']}...  [{ok}]")
    for rel in DIRS:
        d = ds[rel]
        print(f"  M1  rmdir   {rel}/  (non-recursive; must hold exactly the 4 "
              f"test files above, got {d['members']})  "
              f"[{'MATCH' if d['members_ok'] else 'MISMATCH'}]")
    print("      action: os.remove() per file, then os.rmdir() -- no rm, no glob, "
          "no recursion")
    print(f"  {KEPT_NOTE}")
    print("\n=== GATES ===")
    print("  pre  (all must pass before the first unlink, else exit 3, nothing touched):")
    print(f"    G12  python3 tools/r1_shim_residue_sweep.py --json   residue count == {G12_PRE}"
          "  (18 = S1/S2 not landed -> NO-GO)")
    print(f"    R    python3 tools/r1_genuine_retire_test.py         {RETIRE_EXPECT}/{RETIRE_EXPECT}")
    print(f"    F    python3 tools/r1_genuine_fullchain_test.py      "
          f"{FULLCHAIN_EXPECT[0]}/{FULLCHAIN_EXPECT[1]} rows")
    print(f"    S    python3 tools/r1_final_verify.py --strict-skip  {STRICT_EXPECT}/{STRICT_EXPECT}, "
          "0 skip, 0 fail")
    print(f"  post (after the delete, else auto-rollback + re-gate): G12 residue "
          f"EXACTLY {G12_POST} (1+ = NO-GO), R {RETIRE_EXPECT}/{RETIRE_EXPECT}, "
          f"F {FULLCHAIN_EXPECT[0]}/{FULLCHAIN_EXPECT[1]}, S {STRICT_EXPECT}/{STRICT_EXPECT}")
    print("  (this mode runs no gate: no QEMU, no build, static only)")
    print("\n=== GUARDS ===")
    print(f"  scope     : {len(DELETE_SET)} compile-time paths (12 files + 1 dir); no "
          "positional arg accepted, any extra token -> exit 2")
    print(f"  confirm   : --apply --confirm {CONFIRM_TOKEN}  + live scope digest "
          f"{SCOPE_DIGEST} (content-bound; drift -> exit 2)")
    print("  refusals  : argv git commit / git push / make clean / rm -rf; dirty "
          "kernelhwsrv/; already-modified target; sha256 or size mismatch; "
          "unexpected dir members; absolute or '..' path")
    print(f"  snapshot  : {BACKUP_ROOT}/{BACKUP_PREFIX}<YYYYmmdd_HHMMSS>/  (manifest "
          f"schema {BACKUP_SCHEMA}, per-file sha256 verified at copy)")
    print(f"  rollback  : --rollback DIR --confirm {ROLLBACK_PREFIX}<stamp>  "
          "(sha256 checked before AND after restore)")
    print("  never     : kernelhwsrv/ writes, commit, push, `make clean`, QEMU, bare rm")


def collect_pre_state() -> dict:
    state = file_state()
    clean, detail = kernelhwsrv_clean()
    gclean, gdetail = targets_git_clean()
    return {"files": state["files"], "dirs": state["dirs"],
            "kernelhwsrv": detail if clean else f"MODIFIED: {detail}",
            "kernelhwsrv_ok": clean,
            "git": "clean" if gclean else f"MODIFIED: {gdetail}", "git_ok": gclean}


# --------------------------------------------------------------- snapshot ---
def make_backup(stamp: str, pre_result: dict) -> tuple[Path, dict]:
    """Snapshot all 12 files; verify every copy's sha256 at copy time."""
    d = BACKUP_ROOT / f"{BACKUP_PREFIX}{stamp}"
    suffix = 0
    while d.exists():
        suffix += 1
        d = BACKUP_ROOT / f"{BACKUP_PREFIX}{stamp}_{suffix}"
    d.mkdir(parents=True, exist_ok=False)
    token = f"{ROLLBACK_PREFIX}{stamp}"
    entries = []
    for rel in sorted(FILES):
        want_sha, want_size = FILES[rel]
        if not safe_target(rel) or forbidden_path(rel):
            raise RuntimeError(f"REFUSE: unsafe op target {rel!r}")
        src, dst = PORT / rel, d / rel
        if not src.is_file():
            raise RuntimeError(f"cannot snapshot missing file: {src}")
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        src_sum, dst_sum = sha256(src), sha256(dst)
        if src_sum != dst_sum or not src_sum.startswith(want_sha):
            raise RuntimeError(f"snapshot verification FAILED for {rel}: "
                               f"src={src_sum} dst={dst_sum} expected~{want_sha}")
        if src.stat().st_size != want_size:
            raise RuntimeError(f"snapshot size drift for {rel}: "
                               f"{src.stat().st_size} != {want_size}")
        entries.append({"path": rel, "sha256": src_sum, "size": src.stat().st_size})
    manifest = {"schema": BACKUP_SCHEMA, "created": stamp, "dir": d.name,
                "op": "multi-delete", "confirm_token": CONFIRM_TOKEN,
                "rollback_token": token, "scope_digest": SCOPE_DIGEST,
                "paths": sorted(DELETE_SET), "dirs": list(DIRS),
                "expected_g12": {"pre": G12_PRE, "post": G12_POST},
                "pre_gates": {k: v["detail"] for k, v in pre_result["gates"].items()},
                "files": entries}
    (d / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return d, manifest


# --------------------------------------------------------------- rollback ---
def _load_manifest(backup_dir: Path) -> dict:
    mf = backup_dir / "manifest.json"
    if not backup_dir.is_dir() or not mf.is_file():
        raise RuntimeError(f"no backup manifest at {mf}")
    manifest = json.loads(mf.read_text())
    if manifest.get("schema") != BACKUP_SCHEMA:
        raise RuntimeError(f"unsupported backup manifest schema (need {BACKUP_SCHEMA})")
    return manifest


def do_rollback(backup_dir: Path, confirm: str | None = None) -> int:
    """Restore the 12 files (staged temp + verified os.replace) then the dir."""
    try:
        manifest = _load_manifest(backup_dir)
    except (OSError, ValueError, RuntimeError) as exc:
        print(f"REFUSE: {exc}; nothing restored.", file=sys.stderr)
        return 2
    token = manifest.get("rollback_token")
    if not token or confirm != token:
        print(f"REFUSE: --confirm {token or '<token>'} is required for rollback. "
              "Nothing restored.", file=sys.stderr)
        return 2

    entries = manifest.get("files") or []
    if not entries:
        print("REFUSE: empty backup manifest; nothing restored.", file=sys.stderr)
        return 2
    dirs = manifest.get("dirs") or []

    for rel in dirs:
        if rel not in DIRS:
            print(f"REFUSE: manifest dir {rel!r} is not the allowlisted "
                  "tests/r1_fs_channel. Nothing restored.", file=sys.stderr)
            return 2

    staged: list[tuple[Path, str]] = []
    seen: set[str] = set()
    for entry in entries:
        rel, want = entry.get("path", ""), entry.get("sha256", "")
        if rel in seen:
            print(f"REFUSE: duplicate manifest path {rel!r}.", file=sys.stderr)
            return 2
        seen.add(rel)
        why = forbidden_path(rel)
        if rel not in FILES or why or not safe_target(rel):
            print(f"REFUSE: manifest path {rel!r} is not one of the 12 allowlisted "
                  f"files ({why or 'not in FILES'}). Nothing restored.", file=sys.stderr)
            return 2
        if not isinstance(want, str) or not re.fullmatch(r"[0-9a-f]{64}", want):
            print(f"REFUSE: bad sha256 for {rel!r}.", file=sys.stderr)
            return 2
        src, dst = backup_dir / rel, PORT / rel
        if not src.is_file():
            print(f"REFUSE: missing snapshot for {rel}; nothing restored.", file=sys.stderr)
            return 2
        if sha256(src) != want:
            print(f"REFUSE: snapshot sha256 mismatch for {rel}; nothing restored.",
                  file=sys.stderr)
            return 2
        # A live file may only be replaced if it is absent or byte-identical to
        # the snapshot -- never clobber a post-snapshot edit made by someone else.
        if dst.is_file() and sha256(dst) != want:
            print(f"REFUSE: live {rel} differs from the snapshot; nothing restored "
                  "(manual merge required).", file=sys.stderr)
            return 2
        dst.parent.mkdir(parents=True, exist_ok=True)
        tmp = dst.parent / (dst.name + TMP_SUFFIX)
        shutil.copy2(src, tmp)
        if sha256(tmp) != want:
            print(f"REFUSE: staged temp sha256 mismatch for {rel}; nothing restored.",
                  file=sys.stderr)
            return 2
        staged.append((tmp, want))

    for tmp, want in staged:
        dst = tmp.parent / tmp.name[: -len(TMP_SUFFIX)]
        os.replace(tmp, dst)
        if sha256(dst) != want:
            print(f"REFUSE: post-restore sha256 mismatch for {dst}; rollback "
                  "incomplete.", file=sys.stderr)
            return 2
        print(f"  restored {dst.relative_to(PORT)}")
    print(f"rollback complete: restored {len(staged)} file(s)"
          + (f" + {len(dirs)} dir(s)" if dirs else "") + f" from {backup_dir}")
    return 0


# ------------------------------------------------------------------ apply ---
def apply_delete() -> int:
    pre = collect_pre_state()
    problems: list[str] = []
    digest = scope_digest()
    if digest != SCOPE_DIGEST:
        problems.append(f"content drift: live scope digest {digest} != {SCOPE_DIGEST}")
    for rel, info in sorted(pre["files"].items()):
        if forbidden_path(rel):
            problems.append(f"{rel} is a protected path")
        elif not info["exists"]:
            problems.append(f"{rel} missing (already deleted?)")
        else:
            if not info["sha256_ok"]:
                problems.append(f"{rel} sha256 {info['sha256']} != "
                                f"{info['expected_sha256']}")
            if not info["size_ok"]:
                problems.append(f"{rel} size {info['size']} != {info['expected_size']}")
    for rel, d in pre["dirs"].items():
        if not d["exists"]:
            problems.append(f"{rel}/ missing (already deleted?)")
        elif not d["members_ok"]:
            problems.append(f"{rel}/ holds unexpected members {d['members']}")
    if not pre["kernelhwsrv_ok"]:
        problems.append(f"kernelhwsrv/ modified: {pre['kernelhwsrv']}")
    if not pre["git_ok"]:
        problems.append(f"op targets already modified: {pre['git']}")
    if problems:
        for p in problems:
            print(f"REFUSE: {p}", file=sys.stderr)
        print("Nothing changed.", file=sys.stderr)
        return 2
    print(f"  {len(FILES)} files verified; dir {DIRS[0]}/ holds exactly its 4 files")
    print(f"  scope digest: {digest} (MATCH)")
    print(f"  kernelhwsrv/ clean: {pre['kernelhwsrv']}")
    print(f"  op targets git state: {pre['git']}")

    pre_result = run_gate_ladder(G12_PRE, "pre")
    if not pre_result["ok"]:
        print(f"REFUSE: pre-apply gate ladder NOT PASS (need G12 residue exactly "
              f"{G12_PRE} -- has S2 landed? -- R {RETIRE_EXPECT}/{RETIRE_EXPECT}, F "
              f"{FULLCHAIN_EXPECT[0]}/{FULLCHAIN_EXPECT[1]}, S {STRICT_EXPECT}). "
              "No file changed.", file=sys.stderr)
        return 3

    stamp = time.strftime("%Y%m%d_%H%M%S")
    try:
        backup, manifest = make_backup(stamp, pre_result)
    except (OSError, RuntimeError) as exc:
        print(f"REFUSE: snapshot failed ({exc}); nothing deleted.", file=sys.stderr)
        return 2
    token = manifest["rollback_token"]
    print(f"  snapshot: {backup}  (rollback token: {token})")

    ok = False
    stage = "delete"
    rollback_rc = 0
    dirs_done = 0
    try:
        for rel in sorted(FILES):
            if not sha256(PORT / rel).startswith(FILES[rel][0]):
                raise RuntimeError(f"{rel} changed between snapshot and delete")
            os.remove(PORT / rel)   # allowlisted delete; argv guard forbids rm
            print(f"  deleted {rel}")
        for rel in DIRS:
            d = PORT / rel
            if d.is_dir():
                os.rmdir(d)         # non-recursive: refuses if anything is left
                dirs_done += 1
                print(f"  rmdir   {rel}/")
        stage = "post"
        post_result = run_gate_ladder(G12_POST, "post")
        if not post_result["ok"]:
            g12 = post_result["gates"]["g12"]["detail"]
            raise RuntimeError(f"post-delete gate ladder NOT PASS ({g12}; need "
                               f"exactly {G12_POST} residue plus R/F/S PASS)")
        ok = True
    except Exception as exc:
        print(f"FAILURE during {stage}: {exc}; auto-rolling back snapshot.",
              file=sys.stderr)
    finally:
        if not ok:
            rollback_rc = do_rollback(backup, confirm=token)
            if rollback_rc != 0:
                print(f"CRITICAL: rollback FAILED; manual restore required from "
                      f"{backup}", file=sys.stderr)
            else:
                try:
                    re_result = run_gate_ladder(G12_PRE, "rollback")
                    print(f"  gate after rollback: "
                          f"{'PASS' if re_result['ok'] else 'NOT PASS'}")
                except Exception as exc:
                    print(f"  gate after rollback unavailable: {exc}", file=sys.stderr)

    if ok:
        print(f"\nSUCCESS: S3 delete applied ({len(FILES)} files + {dirs_done} dir); "
              "pre and post gate ladders PASS.")
        print(f"  snapshot: {backup}  (token {token})")
        print(f"  expected G12 now: {G12_POST} residue (was {G12_PRE})")
        print("  next (human-reviewed, NOT by this tool): record S3 in "
              "docs/S3_PLAN.md / wave2 status docs and commit when authorized.")
        for rel in sorted(FILES)[:2]:
            print(f"  git fallback: git -C {REPO} show <pre-gate-sha>:qemu-port/{rel} "
                  f"> qemu-port/{rel}")
        return 0
    if rollback_rc != 0:
        return 6
    return 5 if stage == "post" else 4


# ------------------------------------------------------------------- main ---
def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv if argv is None else argv)
    rest = argv[1:].copy()
    # The spent guard also runs BEFORE argparse: on a spent tree --plan and
    # --help must refuse with the applied/spent message instead of a preview.
    rc = refuse_if_spent()
    if rc is not None:
        return rc
    # The argv guard runs BEFORE argparse so a foreign path, a glob or a
    # forbidden verb is refused by this tool's own contract, not as a usage error.
    rc = refuse_if_foreign(argv)
    if rc is not None:
        return rc
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", action="store_true",
                    help="actually delete the 12 files + the test dir (requires "
                         "--confirm " + CONFIRM_TOKEN + ")")
    ap.add_argument("--plan", "--dry-run", dest="plan", action="store_true",
                    help="pure preview: print the exact ops, guards and gates, then "
                         "exit 0 (default is the same preview, then REFUSE)")
    ap.add_argument("--confirm", metavar="TOKEN",
                    help="exact confirm token (" + CONFIRM_TOKEN + ") or a "
                         "snapshot's " + ROLLBACK_PREFIX + "<stamp> rollback token")
    ap.add_argument("--rollback", metavar="DIR",
                    help="restore all 12 files from a backup dir (requires --confirm "
                         + ROLLBACK_PREFIX + "<stamp>)")
    args = ap.parse_args(rest)

    if args.apply and args.plan:
        print("REFUSE: --apply and --plan are mutually exclusive. Nothing changed.",
              file=sys.stderr)
        return 2

    if args.rollback:
        if args.apply or args.plan:
            print("REFUSE: --rollback cannot be combined with --apply/--plan. "
                  "Nothing changed.", file=sys.stderr)
            return 2
        return do_rollback(Path(args.rollback), confirm=args.confirm)

    if args.apply:
        if not args.confirm or args.confirm != CONFIRM_TOKEN:
            print(f"REFUSE: --apply requires --confirm {CONFIRM_TOKEN} "
                  f"(got {args.confirm!r}). Nothing changed.", file=sys.stderr)
            return 2
        return apply_delete()

    # Preview (no args, --plan or --dry-run): static facts only, no QEMU.
    pre = collect_pre_state()
    print_plan(pre, current_residue())
    if args.plan:
        return 0
    print("REFUSE: no --apply -- plan only, nothing changed.", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
