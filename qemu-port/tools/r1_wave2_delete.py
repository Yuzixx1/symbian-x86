#!/usr/bin/env python3
"""r1_wave2_delete.py -- GUARDED one-path delete for WAVE2 / S1
(docs/S1_DELETE_TOOL_PLAN.md, docs/WAVE2_S1_TARGETS.md, docs/S1_PRE_BASELINE.md).

SPENT -- S1 IS ALREADY APPLIED.  fs/r1_fs_core.mmp no longer exists and the G12
residue count is 0 on this tree, so every count, sha256 and path below is
HISTORICAL pre-state documentation, kept only for the review trail.  The tool
now refuses with exit 2 ("already applied/spent") before it parses arguments,
so no mode -- --plan, --dry-run, --help, --apply or --rollback -- can act on
this tree.  Nothing is deleted by that refusal.

This is the small, add-only companion to tools/r1_winsrv_delete.py: it deletes
EXACTLY ONE orphan build descriptor, fs/r1_fs_core.mmp, and nothing else.  It
REUSES that tool's proven guards BY IMPORT (never copied): sha256, safe_target,
git_porcelain, kernelhwsrv_clean and the /tmp/opencode BACKUP_ROOT layout.

HARD CONTRACT
  * The ONLY path this tool can ever write or remove is fs/r1_fs_core.mmp
    (962 B, sha256 deb2d548...f53ffc).  Every other argument -- any positional
    path, any unknown flag, any --rollback dir inside qemu-port/ -- is refused
    (exit 2) before a single write, so one typo or glob can never reach the 7
    LIVE fs/r1_fs_* keepers or kernelhwsrv/.
  * No arguments (or --plan / --dry-run) is a pure preview: it prints the exact
    op plus the whole gate ladder, runs no QEMU and changes nothing.
  * A delete requires BOTH --apply AND --confirm DELETE-WAVE2-S1-deb2d548.
    The token is content-bound (sha256[:8]), so a wave-2 confirmation can never
    delete a winsrv path and DELETE-WINSRV-* can never delete this one.
  * The pre-state must be PROVEN before the unlink: target sha256 AND size
    match, kernelhwsrv/ is clean, the target is not already modified in git,
    and the pre-gate ladder passes -- G12 residue count 18 (HISTORICAL
    pre-state; S1 spent, today 0), retire 4/4, fullchain 14/14, strict 11/11.
    Otherwise exit 3, no file touched.
  * The file is snapshotted to /tmp/opencode/wave2_s1_backup_<stamp>/ with an
    r1-wave2-delete-backup/v1 manifest and rollback token
    ROLLBACK-WAVE2-S1-<stamp>; the copy's sha256 is verified at backup time.
  * After the delete the post-gate ladder must show G12 residue EXACTLY 17
    (HISTORICAL: 18 minus the one [file] r1_fs finding; today 0) -- 16 or 18 is
    NO-GO -- plus strict 11/11, retire 4/4, fullchain 14/14.  The exit code
    alone is never evidence:
    the sweep still exits 1 while residue remains, so the count is what counts.
    Any shortfall auto-rolls back from the manifest and re-gates.
  * NEVER touches kernelhwsrv/ (READ-ONLY), NEVER commits or pushes, NEVER runs
    `make clean`, NEVER a bare `rm -rf`/glob delete, NEVER boots QEMU in
    preview mode.

NOTE ON run_gate: r1_winsrv_delete.run_gate is deliberately NOT reused.  It can
only invoke the retire test as `--only <one gate>`, which cannot yield the 4/4
count this plan requires and which starves the euser gate of the same-run logs
it needs (r1_genuine_retire_test.py:698-715).  Wave 2 therefore runs each gate
once, whole, and parses its own summary line.

Exit codes: 0 preview/plan success (pre-state trees only); 2 refusal (bad
args/token/dirty target/unsafe or foreign path/forbidden argv, or the tree is
already spent); 3 pre-apply gate ladder not PASS; 4 write error (rolled back);
5 post-delete gate ladder not PASS (rolled back); 6 rollback FAILED (manual
restore required).

Usage (HISTORICAL -- every line below now refuses with rc 2 on this tree,
because S1 is spent; none of them touches a file any more):
  python3 tools/r1_wave2_delete.py --plan        # was: preview + gates, rc 0
  python3 tools/r1_wave2_delete.py               # same preview, then REFUSE (rc 2)
  python3 tools/r1_wave2_delete.py --apply --confirm DELETE-WAVE2-S1-deb2d548
  python3 tools/r1_wave2_delete.py --rollback /tmp/opencode/wave2_s1_backup_<stamp> \
      --confirm ROLLBACK-WAVE2-S1-<stamp>
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

TOOLS = Path(__file__).resolve().parent
PORT = TOOLS.parent
REPO = PORT.parent

# Reuse the proven guards from the winsrv deletion tool BY IMPORT (never copied).
sys.path.insert(0, str(TOOLS))
import r1_winsrv_delete as wsd  # noqa: E402

sha256 = wsd.sha256
safe_target = wsd.safe_target
git_porcelain = wsd.git_porcelain
kernelhwsrv_clean = wsd.kernelhwsrv_clean
BACKUP_ROOT = wsd.BACKUP_ROOT          # /tmp/opencode

# ---------------------------------------------------------------- op set ---
# The ONE path this tool may ever have removed (docs/WAVE2_S1_TARGETS.md: a
# proposed MMP, not built -- zero Makefile/toolchain.mk/*.mk build input).
# HISTORICAL: S1 spent, the file is already gone from this tree.
TARGET = "fs/r1_fs_core.mmp"
DELETE_SET = frozenset({TARGET})
EXPECTED_SHA256 = "deb2d548f85a80a39156b18b0cf79bedf84d4d2465d94bd81db5d29757f53ffc"
EXPECTED_SIZE = 962
TARGET_WHY = "\"proposed MMP, not built\" (tests/r1_fs_core/README.md:50); no build input"

# Explicitly KEPT (never in any op): the 7 LIVE fs/r1_fs_* keepers that
# genuine/efsrv_min/efsrv_min.cpp:30,32 includes and build_server.sh:16 links.
KEPT_NOTE = ("KEPT (LIVE, genuine efsrv_min): fs/r1_fs_core.cpp/_core.h/_server.cpp/"
             "_server.h/_backend.h/_fat.cpp/_fat.h + tests/r1_fs_core; G12 lists them "
             "under LIVE_KEEP, never FAIL")

CONFIRM_TOKEN = "DELETE-WAVE2-S1-" + EXPECTED_SHA256[:8]

# ---------------------------------------------------------------- spent ---
# S1 is SPENT.  The probe only stats one path -- it never writes, deletes or
# spawns anything -- so it is safe to run before argparse: on a spent tree no
# mode is meaningful, and --plan / --dry-run / --help must refuse as well.
SPENT_RC = 2
SPENT_MSG = ("REFUSE: S1 is already applied/spent -- fs/r1_fs_core.mmp is gone "
             "and the G12 residue count is 0 on this tree.  The 18/17 ladder, "
             "the sha256/size match and the DELETE-WAVE2-S1 token are "
             "historical pre-state only; there is nothing left to delete, and "
             "--plan/--help/--apply/--rollback are all refused here.  "
             "Nothing changed.")


def spent_evidence(root: Path | None = None) -> list[str]:
    """Reasons S1 counts as spent; empty list means a live pre-state tree."""
    base = PORT if root is None else root
    if (base / TARGET).exists():
        return []
    return [f"{TARGET} absent"]


def refuse_if_spent(root: Path | None = None) -> int | None:
    """Exit code 2 if S1 is already applied, else None to carry on."""
    why = spent_evidence(root)
    if not why:
        return None
    print(SPENT_MSG, file=sys.stderr)
    print("  evidence: " + "; ".join(why), file=sys.stderr)
    return SPENT_RC


BACKUP_SCHEMA = "r1-wave2-delete-backup/v1"
BACKUP_PREFIX = "wave2_s1_backup_"
ROLLBACK_PREFIX = "ROLLBACK-WAVE2-S1-"
TMP_SUFFIX = ".wave2_s1_tmp"

# Gate ladder (docs/S1_DELETE_TOOL_PLAN.md "Gates" + docs/WAVE2_S1_TARGETS.md).
G12_PRE = 18                            # HISTORICAL pre-state (S1 spent: today 0)
G12_POST = 17                           # HISTORICAL post-state (today 0)
RETIRE_EXPECT = 4                       # retire 4/4
FULLCHAIN_EXPECT = (14, 14)             # fullchain 14/14 rows
STRICT_EXPECT = 11                      # final_verify --strict-skip 11/11
GATE_DIR = BACKUP_ROOT / "wave2_s1_gate"

# argv guard: this tool's own source legitimately calls os.remove for the one
# allowlisted delete, so the guard scans ARGV only, never the source.
FORBIDDEN_ARGV = ("git commit", "git push", "make clean", "rm -rf", "rm -r ")
FLAGS = {"--apply", "--plan", "--dry-run", "-h", "--help"}
VALUE_FLAGS = ("--rollback", "--confirm")
PATH_SUFFIXES = (".mmp", ".c", ".h", ".cpp", ".o", ".e32", ".exe", ".elf", ".mk")

RETIRE_RE = re.compile(r"RESULT:\s*(PASS|FAIL)\s*\((\d+) passed,\s*(\d+) skipped\s*"
                       r"(?:\[[^\]]*\])?\s*,\s*(\d+) failed\)")
FULLCHAIN_RE = re.compile(r"RESULT:\s*(PASS|FAIL)\s*\((\d+)/(\d+) rows PASS\)")
STRICT_RE = re.compile(r"OVERALL:\s*(PASS|FAIL)\s*\((\d+) pass, (\d+) skip, "
                       r"(\d+) fail/not-run\)")


# ----------------------------------------------------------- static state ---
def target_state() -> dict:
    """Read-only facts about the one target; nothing is written."""
    p = PORT / TARGET
    info = {"path": TARGET, "in_set": TARGET in DELETE_SET,
            "safe": safe_target(TARGET), "exists": p.is_file()}
    if info["exists"]:
        info["size"] = p.stat().st_size
        info["sha256"] = sha256(p)
    info["size_ok"] = info.get("size") == EXPECTED_SIZE
    info["sha256_ok"] = info.get("sha256") == EXPECTED_SHA256
    return info


def target_git_clean() -> tuple[bool, str]:
    """REFUSE if the target is already modified (no stacking deletes)."""
    return git_porcelain([str(Path("qemu-port") / TARGET)])


def argv_refusals(argv: list[str]) -> list[str]:
    """Refuse any argv token that is not this tool's own flag or its one target.

    Positional paths are the real hazard: one typo or glob must not reach the
    LIVE fs/r1_fs_* keepers, so only `fs/r1_fs_core.mmp` may appear.
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
        if tok.startswith("-"):
            bad.append(tok)              # unknown flag: argparse refuses too
            continue
        if tok != TARGET:
            bad.append(tok)              # a second path / a glob / a typo
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
        print(f"REFUSE: argument(s) outside the one-path set {sorted(DELETE_SET)}: "
              f"{bad!r}. Nothing changed.", file=sys.stderr)
        return 2
    return None


# ------------------------------------------------------------------ gates ---
def run_cmd(argv: list[str], tag: str, timeout: int = 2400) -> subprocess.CompletedProcess:
    joined = " ".join(str(a) for a in argv)
    if any(v in joined for v in FORBIDDEN_ARGV):
        raise RuntimeError(f"REFUSE: forbidden command {joined!r}")
    print(f"+ {tag}: {joined}", flush=True)
    return subprocess.run([str(a) for a in argv], cwd=str(PORT), capture_output=True,
                          text=True, timeout=timeout, check=False)


def _gate(name: str, ok: bool, detail: str) -> dict:
    print(f"  gate {name}: {'PASS' if ok else 'FAIL'}  ({detail})", flush=True)
    return {"name": name, "ok": ok, "detail": detail}


def gate_g12(expect: int, tag: str) -> dict:
    """G12 residue COUNT (the sweep exits 1 while residue remains, so the exit
    code is not evidence -- the count is)."""
    argv = [sys.executable, "tools/r1_shim_residue_sweep.py", "--json"]
    proc = run_cmd(argv, f"gate G12[{tag}]", timeout=180)
    try:
        data = json.loads(proc.stdout)
        count = int(data["residue"])
    except (ValueError, KeyError, TypeError):
        tail = (proc.stdout + proc.stderr).strip().splitlines()[-3:]
        return _gate("G12", False, f"unparseable sweep output rc={proc.returncode}: "
                                   + " | ".join(tail))
    return _gate("G12", count == expect,
                 f"residue {count} (expected exactly {expect})")


def gate_retire(tag: str) -> dict:
    argv = [sys.executable, "tools/r1_genuine_retire_test.py",
            "--log-dir", str(GATE_DIR / f"retire_{tag}")]
    proc = run_cmd(argv, f"gate R[{tag}]")
    m = RETIRE_RE.search(proc.stdout + proc.stderr)
    if not m:
        return _gate("R", False, f"no RESULT summary line (rc={proc.returncode})")
    verdict, passed, skipped, failed = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
    ok = verdict == "PASS" and passed == RETIRE_EXPECT and failed == 0
    return _gate("R", ok, f"retire {passed}/{RETIRE_EXPECT} "
                          f"(skipped {skipped}, failed {failed}, verdict {verdict})")


def gate_fullchain(tag: str) -> dict:
    argv = [sys.executable, "tools/r1_genuine_fullchain_test.py",
            "--log-dir", str(GATE_DIR / f"fullchain_{tag}")]
    proc = run_cmd(argv, f"gate F[{tag}]")
    m = FULLCHAIN_RE.search(proc.stdout + proc.stderr)
    if not m:
        return _gate("F", False, f"no RESULT summary line (rc={proc.returncode})")
    verdict, got, total = m.group(1), int(m.group(2)), int(m.group(3))
    ok = verdict == "PASS" and (got, total) == FULLCHAIN_EXPECT
    return _gate("F", ok, f"fullchain {got}/{total} rows (verdict {verdict})")


def gate_strict(tag: str) -> dict:
    argv = [sys.executable, "tools/r1_final_verify.py", "--strict-skip",
            "--log-dir", str(GATE_DIR / f"strict_{tag}")]
    proc = run_cmd(argv, f"gate S[{tag}]", timeout=3600)
    m = STRICT_RE.search(proc.stdout + proc.stderr)
    if not m:
        return _gate("S", False, f"no OVERALL summary line (rc={proc.returncode})")
    verdict, passed, skipped, failed = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))
    ok = verdict == "PASS" and passed == STRICT_EXPECT and skipped == 0 and failed == 0
    return _gate("S", ok, f"strict {passed}/{STRICT_EXPECT} pass, {skipped} skip, "
                          f"{failed} fail (verdict {verdict})")


def run_gate_ladder(g12_expect: int, tag: str) -> dict:
    """G12 first (cheap, static) then R / F / S; every gate must pass."""
    GATE_DIR.mkdir(parents=True, exist_ok=True)
    gates = {}
    for key, fn in (("g12", lambda: gate_g12(g12_expect, tag)),
                    ("R", lambda: gate_retire(tag)),
                    ("F", lambda: gate_fullchain(tag)),
                    ("S", lambda: gate_strict(tag))):
        try:
            gates[key] = fn()
        except (OSError, subprocess.SubprocessError) as exc:
            gates[key] = _gate(key.upper(), False, f"gate could not run: {exc}")
    ok = all(g["ok"] for g in gates.values())
    return {"ok": ok, "gates": gates}


# ------------------------------------------------------------ plan/print ---
def print_plan(pre: dict, ladder_ran: bool, pre_result: dict | None) -> None:
    print("r1_wave2_delete -- PLAN: no file is changed; no QEMU is booted by "
          "this mode.")
    print(f"workspace: {PORT}")
    print(f"kernelhwsrv/ clean: {pre['kernelhwsrv']}")
    print(f"target git state:   {pre['git']}")
    print(f"target sha256/size: {'MATCH' if pre['state']['sha256_ok'] and pre['state']['size_ok'] else 'MISMATCH'}"
          f"  ({pre['state'].get('size')} B, {pre['state'].get('sha256')})")
    print("\n=== EXACT OP (1 of 1) ===")
    print(f"  D1  delete  {TARGET}  {EXPECTED_SIZE} B  sha256 {EXPECTED_SHA256}")
    print(f"      action: os.remove(qemu-port/{TARGET})   # allowlisted; no rm, "
          "no glob, no recursion")
    print(f"      why:    {TARGET_WHY}")
    print(f"  {KEPT_NOTE}")
    print("\n=== GATES ===")
    print("  pre  (all must pass before the unlink, else exit 3, nothing touched):")
    print("    G12  python3 tools/r1_shim_residue_sweep.py --json        "
          f"residue count == {G12_PRE}")
    print("    R    python3 tools/r1_genuine_retire_test.py              "
          f"{RETIRE_EXPECT}/{RETIRE_EXPECT}")
    print("    F    python3 tools/r1_genuine_fullchain_test.py           "
          f"{FULLCHAIN_EXPECT[0]}/{FULLCHAIN_EXPECT[1]} rows")
    print("    S    python3 tools/r1_final_verify.py --strict-skip       "
          f"{STRICT_EXPECT}/{STRICT_EXPECT}, 0 skip, 0 fail")
    print(f"  post (after the delete, else auto-rollback + re-gate): "
          f"G12 residue EXACTLY {G12_POST} (16 or 18 = NO-GO), plus R "
          f"{RETIRE_EXPECT}/{RETIRE_EXPECT}, F "
          f"{FULLCHAIN_EXPECT[0]}/{FULLCHAIN_EXPECT[1]}, S "
          f"{STRICT_EXPECT}/{STRICT_EXPECT}")
    if ladder_ran:
        print("  pre-gate ladder result now: "
              + ("PASS" if pre_result and pre_result["ok"] else "NOT PASS")
              + "  " + "; ".join(f"{g['name']}={g['detail']}"
                                 for g in (pre_result or {}).get("gates", {}).values()))
    else:
        print("  (this mode runs no gate: no QEMU, no build, static only)")
    print("\n=== GUARDS ===")
    print(f"  scope     : only {TARGET} -- any other path/positional arg -> exit 2")
    print(f"  confirm   : --apply --confirm {CONFIRM_TOKEN}  (content-bound sha256[:8])")
    print("  refusals  : argv git commit / git push / make clean / rm -rf; dirty "
          "kernelhwsrv/; already-modified target; sha256 or size mismatch")
    print(f"  snapshot  : {BACKUP_ROOT}/{BACKUP_PREFIX}<YYYYmmdd_HHMMSS>/  "
          f"(manifest schema {BACKUP_SCHEMA}, per-file sha256 verified at copy)")
    print(f"  rollback  : --rollback DIR --confirm {ROLLBACK_PREFIX}<stamp>  "
          "(sha256 checked before AND after restore)")
    print("  never     : kernelhwsrv/ writes, commit, push, `make clean`, QEMU in "
          "preview, bare rm")


def collect_pre_state() -> dict:
    st = target_state()
    clean, detail = kernelhwsrv_clean()
    gclean, gdetail = target_git_clean()
    return {"state": st, "kernelhwsrv": detail if clean else f"MODIFIED: {detail}",
            "kernelhwsrv_ok": clean,
            "git": "clean" if gclean else f"MODIFIED: {gdetail}", "git_ok": gclean}


# --------------------------------------------------------------- snapshot ---
def make_backup(stamp: str, pre_result: dict) -> tuple[Path, dict]:
    """Snapshot the ONE allowlisted target; verify the copy's sha256 at copy."""
    d = BACKUP_ROOT / f"{BACKUP_PREFIX}{stamp}"
    suffix = 0
    while d.exists():
        suffix += 1
        d = BACKUP_ROOT / f"{BACKUP_PREFIX}{stamp}_{suffix}"
    d.mkdir(parents=True, exist_ok=False)
    token = f"{ROLLBACK_PREFIX}{stamp}"
    if not safe_target(TARGET):
        raise RuntimeError(f"REFUSE: unsafe op target {TARGET!r}")
    src, dst = PORT / TARGET, d / TARGET
    if not src.is_file():
        raise RuntimeError(f"cannot snapshot missing file: {src}")
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    src_sum, dst_sum = sha256(src), sha256(dst)
    if src_sum != dst_sum or src_sum != EXPECTED_SHA256:
        raise RuntimeError(f"snapshot verification FAILED for {TARGET}: "
                           f"src={src_sum} dst={dst_sum} expected={EXPECTED_SHA256}")
    manifest = {"schema": BACKUP_SCHEMA, "created": stamp, "dir": d.name,
                "op": "delete", "path": TARGET, "confirm_token": CONFIRM_TOKEN,
                "rollback_token": token,
                "expected_g12": {"pre": G12_PRE, "post": G12_POST},
                "pre_gates": {k: v["detail"] for k, v in pre_result["gates"].items()},
                "files": [{"path": TARGET, "sha256": src_sum,
                           "size": src.stat().st_size}]}
    (d / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return d, manifest


# --------------------------------------------------------------- rollback ---
def do_rollback(backup_dir: Path, confirm: str | None = None) -> int:
    """Restore the one file via staged temp + verified os.replace."""
    mf = backup_dir / "manifest.json"
    if not backup_dir.is_dir() or not mf.is_file():
        print(f"REFUSE: no backup manifest at {mf}; nothing restored.", file=sys.stderr)
        return 2
    try:
        manifest = json.loads(mf.read_text())
    except (OSError, ValueError) as exc:
        print(f"REFUSE: unreadable backup manifest: {exc}", file=sys.stderr)
        return 2
    if manifest.get("schema") != BACKUP_SCHEMA:
        print("REFUSE: unsupported backup manifest schema (need "
              f"{BACKUP_SCHEMA}).", file=sys.stderr)
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

    staged: list[tuple[Path, str]] = []
    seen: set[str] = set()
    for entry in entries:
        rel, want = entry.get("path", ""), entry.get("sha256", "")
        if rel in seen:
            print(f"REFUSE: duplicate manifest path {rel!r}.", file=sys.stderr)
            return 2
        seen.add(rel)
        if rel not in DELETE_SET or not safe_target(rel) or Path(rel).is_absolute() \
                or ".." in Path(rel).parts:
            print(f"REFUSE: manifest path {rel!r} is not the allowlisted "
                  "qemu-port target. Nothing restored.", file=sys.stderr)
            return 2
        if not isinstance(want, str) or not re.fullmatch(r"[0-9a-f]{64}", want):
            print(f"REFUSE: bad sha256 for {rel!r}.", file=sys.stderr)
            return 2
        src, dst = backup_dir / rel, PORT / rel
        if not src.is_file():
            print(f"REFUSE: missing snapshot for {rel}; nothing restored.",
                  file=sys.stderr)
            return 2
        if sha256(src) != want:
            print(f"REFUSE: snapshot sha256 mismatch for {rel}; nothing restored.",
                  file=sys.stderr)
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
    print(f"rollback complete: restored {len(staged)} file(s) from {backup_dir}")
    return 0


# ------------------------------------------------------------------ apply ---
def apply_delete() -> int:
    pre = collect_pre_state()
    st = pre["state"]
    problems = []
    if not st["in_set"]:
        problems.append("target is not in the one-path DELETE_SET")
    if not st["safe"]:
        problems.append("target outside qemu-port/ or under kernelhwsrv/")
    if not st["exists"]:
        problems.append("target missing (already deleted?)")
    else:
        if not st["sha256_ok"]:
            problems.append(f"sha256 {st['sha256']} != expected {EXPECTED_SHA256}")
        if not st["size_ok"]:
            problems.append(f"size {st.get('size')} != {EXPECTED_SIZE}")
    if not pre["kernelhwsrv_ok"]:
        problems.append(f"kernelhwsrv/ modified: {pre['kernelhwsrv']}")
    if not pre["git_ok"]:
        problems.append(f"target already modified: {pre['git']}")
    if problems:
        for p in problems:
            print(f"REFUSE: {p}", file=sys.stderr)
        print("Nothing changed.", file=sys.stderr)
        return 2
    print(f"  target verified: {TARGET} {st['size']} B sha256 {st['sha256']}")
    print(f"  kernelhwsrv/ clean: {pre['kernelhwsrv']}")
    print(f"  target git state: {pre['git']}")

    pre_result = run_gate_ladder(G12_PRE, "pre")
    if not pre_result["ok"]:
        print(f"REFUSE: pre-apply gate ladder NOT PASS (need G12 residue exactly "
              f"{G12_PRE}, R {RETIRE_EXPECT}/{RETIRE_EXPECT}, F "
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
    try:
        if sha256(PORT / TARGET) != EXPECTED_SHA256:
            raise RuntimeError("target changed between snapshot and delete")
        os.remove(PORT / TARGET)  # the ONE allowlisted delete; argv guard forbids rm
        print(f"  deleted {TARGET}")
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
        print("\nSUCCESS: S1 delete applied (1 path); pre and post gate ladders PASS.")
        print(f"  snapshot: {backup}  (token {token})")
        print(f"  expected G12 now: {G12_POST} residue (was {G12_PRE})")
        print("  next (human-reviewed, NOT by this tool): record S1 in "
              "docs/S1_DELETE_TOOL_PLAN.md / WAVE2 status docs and commit when "
              "authorized.")
        print(f"  git fallback: git -C {REPO} show <pre-gate-sha>:qemu-port/{TARGET} "
              f"> qemu-port/{TARGET}")
        return 0
    if rollback_rc != 0:
        return 6
    return 5 if stage == "post" else 4


# ------------------------------------------------------------------- main --
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
                    help="actually delete the one path (requires --confirm "
                         + CONFIRM_TOKEN + ")")
    ap.add_argument("--plan", "--dry-run", dest="plan", action="store_true",
                    help="pure preview: print the exact op and gates, then exit 0 "
                         "(default is the same preview, then REFUSE)")
    ap.add_argument("--confirm", metavar="TOKEN",
                    help="exact confirm token (" + CONFIRM_TOKEN + ") or a "
                         "snapshot's " + ROLLBACK_PREFIX + "<stamp> rollback token")
    ap.add_argument("--rollback", metavar="DIR",
                    help="restore the target from a backup dir (requires --confirm "
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
    print_plan(pre, ladder_ran=False, pre_result=None)
    if args.plan:
        return 0
    print("REFUSE: no --apply -- plan only, nothing changed.", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
