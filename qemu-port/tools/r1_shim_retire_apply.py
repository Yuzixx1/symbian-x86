#!/usr/bin/env python3
"""r1_shim_retire_apply.py -- GUARDED execution of a shim retirement (DRY-RUN BY DEFAULT).

This is the execution companion to tools/r1_shim_retire_plan.py and
docs/SHIM_RETIREMENT_PLAN.md.  It reuses the plan's EXACT inventory / boot-line
strings and its gate parser (imported as `plan` below); those inputs are never
edited here.  It is deliberately NOT a planner: it only ever changes source
under qemu-port/ after its genuine gate reports PASS.

HARD CONTRACT
  * Default mode (no --apply) is a pure preview: it runs the retire gate with
    --dry-run (NO QEMU), prints each shim's BLOCKED/READY status plus the exact
    boot-list lines that would be bypassed, and REFUSES to change anything.
  * An actual change requires BOTH `--apply` AND `--confirm <TOKEN>` where TOKEN
    is the per-shim token printed by this tool (e.g. RETIRE-PORTFILESRV).
  * Before any write it re-runs the relevant gate WITHOUT --dry-run (a real
    QEMU boot) via `r1_genuine_retire_test.py --only <gate>`.  Anything other
    than PASS (FAIL, SKIP, UNKNOWN) is REFUSED; no file is touched.
  * It REFUSES to proceed if `git status --porcelain -- kernelhwsrv/` is
    non-empty (the kernel tree is read-only and never part of a switch), OR if
    any EDIT TARGET itself is already modified.
  * Changes are minimal exact-text replacements (comment-out / bypass) of
    HAND-WRITTEN shim lines only.  The genuine boot list (EFILE/EFSRV/ESTART/
    E32STRT/EWSRV/ESHELL) and the genuine launch path (GenuineBootSpawn /
    ShellSpawnFull) are NEVER edited: the retire gate requires them to boot.
    Every replacement asserts exactly ONE occurrence in the target file; zero
    or many aborts before writing.
  * Before the first write every touched file is snapshotted to
    /tmp/opencode/shim_retire_backup_<timestamp>/ with a sha256 manifest.  The
    copy's hash is verified against the source at backup time, and the manifest
    carries a rollback token that --rollback must present.
  * After apply the SAME gate is re-run.  ALL post-write operations (writes,
    post gate, rollback) are inside one try/except/finally: ANY failure after
    a write auto-rolls back, the rollback rc is checked, and the gate is re-run
    after rollback.  `--rollback <dir> --confirm <TOKEN>` restores manually.
  * NEVER deletes files, NEVER touches kernelhwsrv/, NEVER commits or pushes,
    NEVER runs `make clean`, and NEVER boots QEMU in preview/dry-run mode.

Exit codes: 0 success/preview; 2 refusal (bad args/token/dirty target/unsafe
snapshot); 3 pre-apply gate not PASS; 4 write error (rolled back); 5 post-write
failure (rolled back); 6 rollback FAILED (manual restore required).

Usage:
  python3 tools/r1_shim_retire_apply.py                      # preview all (no writes)
  python3 tools/r1_shim_retire_apply.py --shim winsrv        # preview one shim
  python3 tools/r1_shim_retire_apply.py --from-file LOG      # preview from captured log
  python3 tools/r1_shim_retire_apply.py --shim winsrv --apply --confirm RETIRE-WINSRV
  python3 tools/r1_shim_retire_apply.py --rollback DIR --confirm ROLLBACK-WINSRV-<ts>
"""
from __future__ import annotations

import argparse
import hashlib
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
KHWSRV = REPO / "kernelhwsrv"
BACKUP_ROOT = Path("/tmp/opencode")

# Reuse the plan's exact strings and parser instead of copying them.
sys.path.insert(0, str(TOOLS))
import r1_shim_retire_plan as plan  # noqa: E402

GATE = plan.GATE
GATE_NAME = plan.GATE_NAME
ROLLBACK_REF = "archive/handwritten-r25"
BASELINE_COMMIT = "56fbaff"
BACKUP_SCHEMA = "r1-shim-retire-apply-backup/v2"
FORBIDDEN = ("git commit", "git push", "make clean", "rm -rf", "os.remove",
              "os.unlink", "pathlib.Path.unlink")

# Aliases -> index into plan.SHIMS (the long plan names are accepted too).
SHIM_ALIASES = {
    "portfilesrv": 0,
    "r1fs": 1, "r1_fs": 1, "r1_fs_*": 1,
    "winsrv": 2,
    "svcmgr": 3,
}
for _i, _s in enumerate(plan.SHIMS):
    SHIM_ALIASES.setdefault(_s["name"].lower(), _i)

# FIX (review HIGH #2): only HAND-WRITTEN shim strings are ever bypassed.
# Tuples are indices into plan.BOOT_LIST_LINES[gate].  The genuine boot-list
# entries (kGenuineBootList EFILE/EFSRV/ESTART/E32STRT/EWSRV/ESHELL) and the
# genuine launch path (GenuineBootSpawn/ShellSpawnFull in qemu_variant_asic.cpp)
# are REQUIRED by the retire gate's boot markers and are NEVER edited:
#   efsrv  (1, 2): SvcMgrBootLaunch fallback + PORTFILESRV staging row
#   ewsrv  ()    : no hand-written boot-line shim; genuine EWSRV entry stays
#   estart (3,)  : SvcMgrExecInstall() call (svcmgr exec install)
# Indices 0 of efsrv/ewsrv/estart and 1 of estart are GENUINE entries and are
# deliberately excluded; ewsrv index 2 is a multi-line block containing the
# genuine GenuineBootSpawn/ShellSpawnFull boot path and is also excluded.
BOOT_EDIT_INDEX = {
    "efsrv": (1, 2),
    "ewsrv": (),
    "estart": (3,),
}

# Human-readable reason shown when a gate has no safe boot-line edit.
NO_EDIT_NOTE = {
    "ewsrv": ("no hand-written boot-line shim: genuine \\EWSRV.E32 entry and "
              "GenuineBootSpawn/ShellSpawnFull must stay; retire winsrv by "
              "removing its files per docs/SHIM_RETIREMENT_PLAN.md"),
}


# ---------------------------------------------------------------- helpers --
def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def shim_index(value: str | None) -> int | None:
    if not value:
        return None
    key = value.strip().lower()
    if key in SHIM_ALIASES:
        return SHIM_ALIASES[key]
    return None


def token_slug(name: str) -> str:
    return re.sub(r"[^A-Z0-9]", "", name.split()[0].upper())


def expected_token(idx: int) -> str:
    return "RETIRE-" + token_slug(plan.SHIMS[idx]["name"])


def comment_out(text: str) -> str:
    """Comment out an exact block line-for-line, keeping indentation/newlines."""
    out = []
    for line in text.split("\n"):
        if not line.strip():
            out.append(line)
        else:
            indent = len(line) - len(line.lstrip())
            out.append(" " * indent + "// RETIRED by r1_shim_retire_apply: "
                       + line.lstrip())
    return "\n".join(out)


def build_edits(idx: int) -> list[dict]:
    """Return minimal exact-text edits, reusing plan.BOOT_LIST_LINES verbatim."""
    shim = plan.SHIMS[idx]
    gate = shim["gate"]
    edits = []
    for n in BOOT_EDIT_INDEX[gate]:
        location, old = plan.BOOT_LIST_LINES[gate][n]
        rel = location.split(":")[0]
        edits.append({"path": rel, "location": location, "old": old,
                      "new": comment_out(old), "index": n})
    return edits


def planned_paths(idx: int) -> set[str]:
    """The only relative paths a rollback for this shim is allowed to touch."""
    return {e["path"] for e in build_edits(idx)}


def safe_target(rel: str) -> bool:
    """Never edit kernelhwsrv/ or anything outside qemu-port/."""
    p = (PORT / rel).resolve()
    return PORT.resolve() in p.parents and KHWSRV.resolve() not in p.parents and p != KHWSRV.resolve()


def check_edits(edits: list[dict]) -> list[dict]:
    """Check each exact string occurs exactly once; abort on ambiguity."""
    out = []
    for e in edits:
        p = PORT / e["path"]
        info = {"path": e["path"], "location": e["location"], "index": e["index"]}
        if not safe_target(e["path"]):
            info.update(ok=False, count=0, reason="REFUSED: target outside qemu-port/")
        elif not p.is_file():
            info.update(ok=False, count=0, reason="target file missing")
        else:
            count = p.read_bytes().count(e["old"].encode("utf-8"))
            info.update(ok=(count == 1), count=count,
                        reason="unique exact match" if count == 1 else
                        "expected exactly one occurrence; got %d" % count)
        out.append(info)
    return out


# ------------------------------------------------------------------ gate ---
def run_gate(gate_key: str, dry_run: bool, tag: str) -> dict:
    """Invoke the retire gate; parse verdicts with the plan's own parser."""
    logdir = BACKUP_ROOT / f"shim_retire_gate_{tag}"
    logdir.mkdir(parents=True, exist_ok=True)
    argv = [sys.executable, str(GATE), "--only", gate_key]
    if dry_run:
        argv.append("--dry-run")
    argv += ["--log-dir", str(logdir)]
    print("+ gate: " + " ".join(str(a) for a in argv), flush=True)
    proc = subprocess.run(argv, cwd=str(PORT), capture_output=True, text=True,
                          timeout=(180 if dry_run else 1200), check=False)
    status = plan.parse_status(proc.stdout + proc.stderr)
    verdict = status.get(gate_key, {}).get("verdict", "UNKNOWN")
    print(f"  gate {GATE_NAME} --only {gate_key}: {verdict} "
          f"(exit {proc.returncode}, dry-run={dry_run})", flush=True)
    return {"verdict": verdict, "status": status, "rc": proc.returncode,
            "output": proc.stdout + proc.stderr, "argv": argv}


def git_porcelain(paths: list[str]) -> tuple[bool, str]:
    """Return (clean, detail) for repo-relative `paths` via git status."""
    if not paths:
        return True, "clean (no paths)"
    argv = ["git", "-C", str(REPO), "status", "--porcelain", "--"] + paths
    try:
        proc = subprocess.run(argv, capture_output=True, text=True,
                              timeout=60, check=False)
    except (OSError, subprocess.SubprocessError) as exc:
        return False, f"git unavailable: {exc}"
    if proc.returncode != 0:
        return False, f"git status failed: {proc.stderr.strip()}"
    dirty = proc.stdout.strip()
    return (not dirty), (dirty if dirty else "clean")


def kernelhwsrv_clean() -> tuple[bool, str]:
    """REFUSE if the read-only kernel tree has any working-tree change."""
    return git_porcelain(["kernelhwsrv/"])


def edits_clean(edits: list[dict]) -> tuple[bool, str]:
    """FIX (review MED #9): REFUSE if any edit target is already modified."""
    rels = sorted({e["path"] for e in edits})
    return git_porcelain([str(Path("qemu-port") / r) for r in rels])


# --------------------------------------------------------------- preview ---
def preview(idx: int | None, status: dict, source: str, as_json: bool) -> None:
    ready = lambda g: status.get(g, {}).get("verdict", "UNKNOWN") == "PASS"
    rows = []
    indices = [idx] if idx is not None else list(range(len(plan.SHIMS)))
    if not as_json:
        print("r1_shim_retire_apply -- DRY-RUN PREVIEW: NO file is changed; "
              "no QEMU was booted (gate --dry-run).")
        print(f"gate status source: {source}")
    for i in indices:
        shim = plan.SHIMS[i]
        gate, label = shim["gate"], plan.GATE_LABELS[shim["gate"]]
        verdict = status.get(gate, {}).get("verdict", "UNKNOWN")
        state = "READY" if ready(gate) else "BLOCKED"
        edits = build_edits(i)
        checks = check_edits(edits)
        verdict_ok = verdict == "PASS"
        if as_json:
            rows.append({"shim": shim["name"], "gate": label,
                         "gate_verdict": verdict, "state": state,
                         "token": expected_token(i), "edits": checks,
                         "note": NO_EDIT_NOTE.get(gate) if not edits else None})
            continue
        print(f"\n=== {shim['name']}: {state}  (gate {label} = {verdict}) ===")
        print(f"  reason: gate {label} = {verdict}"
              + ("  (PASS required; dry-run never proves readiness)"
                 if not verdict_ok else ""))
        if not edits:
            print(f"  edit set: NONE (hand-written shims only; genuine entries kept)")
            print(f"  note: {NO_EDIT_NOTE.get(gate, 'no safe boot-line edit')}")
        for c in checks:
            tag = "unique" if c["ok"] else "AMBIGUOUS"
            print(f"  [{tag}] {c['location']} -> {c['path']}  ({c['reason']})")
            for line in c.get("lines", []):
                print(f"      {line}")
        print(f"  confirm token (if gate ever PASSes): {expected_token(i)}")
        print("  REFUSE: --apply not set (or gate not PASS) -- no file changed.")
    if as_json:
        print(json.dumps({"schema": "r1-shim-retire-apply-preview/v1",
                          "dry_run": True, "source": source, "shims": rows},
                         indent=2))
    else:
        print("\nApply needs: --shim <name> --apply --confirm <TOKEN>; "
              "the real (non-dry-run) gate must report PASS first.")


# --------------------------------------------------------------- snapshot --
def make_backup(idx: int, edits: list[dict], stamp: str) -> tuple[Path, dict]:
    d = BACKUP_ROOT / f"shim_retire_backup_{stamp}"
    suffix = 0
    while d.exists():
        suffix += 1
        d = BACKUP_ROOT / f"shim_retire_backup_{stamp}_{suffix}"
    d.mkdir(parents=True, exist_ok=False)
    token = f"ROLLBACK-{token_slug(plan.SHIMS[idx]['name'])}-{stamp}"
    manifest = {"schema": BACKUP_SCHEMA, "created": stamp, "dir": d.name,
                "shim": plan.SHIMS[idx]["name"], "gate": plan.SHIMS[idx]["gate"],
                "rollback_token": token, "files": []}
    seen: set[str] = set()
    for e in edits:
        rel = e["path"]
        if rel in seen:
            continue
        seen.add(rel)
        src, dst = PORT / rel, d / rel
        if not src.is_file():
            raise RuntimeError(f"cannot snapshot missing file: {src}")
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        # FIX (review MED #8): verify the copy, not just the source.
        src_sum, dst_sum = sha256(src), sha256(dst)
        if src_sum != dst_sum:
            raise RuntimeError(f"snapshot verification FAILED for {rel}: "
                               f"src={src_sum} dst={dst_sum}")
        manifest["files"].append({"path": rel, "sha256": src_sum})
    (d / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return d, manifest


def allowed_rollback_paths(manifest: dict) -> set[str]:
    """FIX (review MED #7): only planned paths for the manifest's shim."""
    name = manifest.get("shim", "")
    idx = next((i for i, s in enumerate(plan.SHIMS) if s["name"] == name), None)
    if idx is None:
        return set()
    return planned_paths(idx)


def do_rollback(backup_dir: Path, confirm: str | None = None) -> int:
    """Restore a verified snapshot; all-or-nothing via verified temp+rename."""
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
        print("REFUSE: unsupported backup manifest schema.", file=sys.stderr)
        return 2
    token = manifest.get("rollback_token")
    if not token or confirm != token:
        print(f"REFUSE: --confirm {token or '<token>'} is required for rollback. "
              "Nothing restored.", file=sys.stderr)
        return 2
    allowed = allowed_rollback_paths(manifest)
    if not allowed:
        print("REFUSE: manifest names an unknown shim; nothing restored.",
              file=sys.stderr)
        return 2
    entries = manifest.get("files", [])
    if not entries:
        print("REFUSE: empty backup manifest; nothing restored.", file=sys.stderr)
        return 2

    # Verify EVERY snapshot and stage EVERY temp file before writing anything.
    staged: list[tuple[Path, str]] = []  # (temp file, expected sha256)
    seen: set[str] = set()
    for entry in entries:
        rel, want = entry.get("path", ""), entry.get("sha256", "")
        if rel in seen:
            print(f"REFUSE: duplicate manifest path {rel!r}.", file=sys.stderr)
            return 2
        seen.add(rel)
        if rel not in allowed or not safe_target(rel) or Path(rel).is_absolute() \
                or ".." in Path(rel).parts:
            print(f"REFUSE: manifest path {rel!r} is not a planned edit target. "
                  "Nothing restored.", file=sys.stderr)
            return 2
        if not isinstance(want, str) or not re.fullmatch(r"[0-9a-f]{64}", want):
            print(f"REFUSE: bad sha256 for {rel!r}.", file=sys.stderr)
            return 2
        src, dst = backup_dir / rel, PORT / rel
        if not src.is_file():
            print(f"REFUSE: missing snapshot for {rel}; nothing restored.",
                  file=sys.stderr)
            return 2
        # FIX (review MED #8): detect a corrupt/tampered snapshot before writes.
        if sha256(src) != want:
            print(f"REFUSE: snapshot sha256 mismatch for {rel}; nothing restored.",
                  file=sys.stderr)
            return 2
        dst.parent.mkdir(parents=True, exist_ok=True)
        tmp = dst.parent / (dst.name + ".shim_retire_tmp")
        shutil.copy2(src, tmp)
        if sha256(tmp) != want:
            print(f"REFUSE: staged temp sha256 mismatch for {rel}; nothing restored.",
                  file=sys.stderr)
            return 2
        staged.append((tmp, want))

    # All verified: commit atomically one file at a time (temp -> final name).
    for tmp, want in staged:
        dst = tmp.parent / tmp.name[: -len(".shim_retire_tmp")]
        os.replace(tmp, dst)
        if sha256(dst) != want:
            print(f"REFUSE: post-restore sha256 mismatch for {dst}; "
                  "rollback incomplete.", file=sys.stderr)
            return 2
        print(f"  restored {dst.relative_to(PORT)}")
    print(f"rollback complete: {len(staged)} file(s) restored from {backup_dir}")
    return 0


def print_git_rollback() -> None:
    print("\n--- git rollback (this tool never commits or pushes) ---")
    print(f"# baseline: tag/branch {ROLLBACK_REF} @ {BASELINE_COMMIT}")
    print(f"git -C {REPO} status --short")
    print(f"git -C {REPO} diff -- qemu-port/")
    print(f"git -C {REPO} checkout {ROLLBACK_REF} -- qemu-port/<path>")
    print(f"# or: git -C {REPO} show {ROLLBACK_REF}:qemu-port/<path> > qemu-port/<path>")
    print("cd qemu-port && make isogrub && python3 tools/r1_os_selfcheck.py")
    print(f"git -C {REPO} status --short -- kernelhwsrv/   # must be empty")


# ------------------------------------------------------------------ apply --
def apply_shim(idx: int) -> int:
    shim = plan.SHIMS[idx]
    gate = shim["gate"]
    edits = build_edits(idx)
    print(f"== apply requested: {shim['name']} (gate {plan.GATE_LABELS[gate]}) ==")

    if not edits:
        print(f"REFUSE: no hand-written boot-line shim for {shim['name']}; "
              f"{NO_EDIT_NOTE.get(gate, 'no safe edit')}. Nothing changed.",
              file=sys.stderr)
        return 2

    clean, detail = kernelhwsrv_clean()
    if not clean:
        print(f"REFUSE: kernelhwsrv/ is modified: {detail!r}", file=sys.stderr)
        return 2
    print(f"  kernelhwsrv/ clean: {detail}")

    # FIX (review MED #9): refuse if any qemu-port edit target is already dirty.
    eclean, edetail = edits_clean(edits)
    if not eclean:
        print(f"REFUSE: edit target(s) already modified: {edetail!r}. "
              "Nothing changed.", file=sys.stderr)
        return 2
    print(f"  edit target(s) clean: {edetail}")

    checks = check_edits(edits)
    bad = [c for c in checks if not c["ok"]]
    if bad:
        for c in bad:
            print(f"REFUSE: ambiguous edit {c['location']}: {c['reason']}",
                  file=sys.stderr)
        return 2
    print(f"  edit targets: {len(edits)} exact-text replacements, all unique "
          "(hand-written shims only; genuine boot entries kept)")

    pre = run_gate(gate, dry_run=False, tag=f"{gate}_pre")
    if pre["verdict"] != "PASS":
        print(f"REFUSE: pre-apply gate {plan.GATE_LABELS[gate]} = "
              f"{pre['verdict']} (PASS required). No file changed.",
              file=sys.stderr)
        return 3

    stamp = time.strftime("%Y%m%d_%H%M%S")
    backup, manifest = make_backup(idx, edits, stamp)
    token = manifest["rollback_token"]
    print(f"  snapshot: {backup}  (rollback token: {token})")

    # FIX (review HIGH #1): EVERYTHING after the backup is inside one
    # try/except/finally; ANY failure after a write auto-rolls back and the
    # rollback rc is checked before reporting success.
    ok = False
    stage = "write"
    rollback_rc = 0
    try:
        for e in edits:
            p = PORT / e["path"]
            data = p.read_bytes()
            needle, repl = e["old"].encode("utf-8"), e["new"].encode("utf-8")
            if data.count(needle) != 1:
                raise RuntimeError(f"ambiguity changed before edit: {e['location']}")
            p.write_bytes(data.replace(needle, repl, 1))
            print(f"  bypassed {e['location']} -> {e['path']}")
        stage = "post"
        post = run_gate(gate, dry_run=False, tag=f"{gate}_post")
        if post["verdict"] != "PASS":
            raise RuntimeError(f"post gate {plan.GATE_LABELS[gate]} = "
                               f"{post['verdict']} (PASS required)")
        ok = True
    except Exception as exc:
        print(f"FAILURE during {stage}: {exc}; auto-rolling back snapshot.",
              file=sys.stderr)
    finally:
        if not ok:
            rollback_rc = do_rollback(backup, confirm=token)
            if rollback_rc != 0:
                print("CRITICAL: rollback FAILED; manual restore required "
                      f"from {backup}", file=sys.stderr)
            else:
                try:
                    post2 = run_gate(gate, dry_run=False,
                                     tag=f"{gate}_post_rollback")
                    print(f"  gate after rollback: {post2['verdict']}")
                except Exception as exc:
                    print(f"  gate after rollback unavailable: {exc}",
                          file=sys.stderr)

    if ok:
        print(f"\nSUCCESS: {shim['name']} retired; post gate "
              f"{plan.GATE_LABELS[gate]} = PASS.")
        print(f"  rollback snapshot: {backup}")
        print_git_rollback()
        return 0
    return 6 if rollback_rc != 0 else (4 if stage == "write" else 5)


# ------------------------------------------------------------------- main --
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--shim", metavar="NAME",
                    help="shim to preview/apply: portfilesrv, r1fs, winsrv, svcmgr")
    ap.add_argument("--apply", action="store_true",
                    help="actually change files (requires --shim AND --confirm TOKEN)")
    ap.add_argument("--confirm", metavar="TOKEN",
                    help="exact per-shim token, or the snapshot's rollback token")
    ap.add_argument("--dry-run", action="store_true",
                    help="force preview mode even with --apply (default)")
    ap.add_argument("--from-file", metavar="LOG",
                    help="preview from a captured retire-gate log (still no QEMU)")
    ap.add_argument("--rollback", metavar="DIR",
                    help="restore touched files from a backup directory "
                         "(requires --confirm ROLLBACK-TOKEN)")
    ap.add_argument("--json", action="store_true", help="JSON preview output")
    args = ap.parse_args()

    joined_argv = " ".join(sys.argv)
    if any(v in joined_argv for v in FORBIDDEN):
        print("REFUSE: forbidden operation requested.", file=sys.stderr)
        return 2

    if args.rollback:
        # FIX (review MED #7): do_rollback now enforces the manifest token.
        return do_rollback(Path(args.rollback), confirm=args.confirm)

    idx = shim_index(args.shim)
    if args.shim and idx is None:
        print(f"REFUSE: unknown --shim {args.shim!r}. Valid: "
              + ", ".join(sorted(SHIM_ALIASES)), file=sys.stderr)
        return 2
    if args.apply and idx is None:
        print("REFUSE: --apply requires --shim <name>. Nothing changed.",
              file=sys.stderr)
        preview(None, {}, "not queried (no --shim)", args.json)
        return 2

    if args.apply:
        if args.dry_run:
            print("REFUSE: --dry-run cannot be combined with --apply. "
                  "Nothing changed.", file=sys.stderr)
            return 2
        want = expected_token(idx)
        if not args.confirm:
            print(f"REFUSE: --apply requires --confirm {want}. Nothing changed.",
                  file=sys.stderr)
            return 2
        if args.confirm != want:
            print(f"REFUSE: wrong --confirm token: {args.confirm!r} "
                  f"(expected {want}). Nothing changed.", file=sys.stderr)
            return 2
        return apply_shim(idx)

    # Preview / dry-run (default): gate --dry-run, never QEMU, never writes.
    status, source = plan.gate_status(args.from_file)
    preview(idx, status, source, args.json)
    return 0  # a preview is never a gate


if __name__ == "__main__":
    sys.exit(main())
