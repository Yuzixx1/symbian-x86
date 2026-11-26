#!/usr/bin/env python3
"""r1_g12_negative_test.py -- falsifiability (negative) test for the G12
residue sweep's `--root PATH` option.

WHY THIS EXISTS
    A gate that cannot fail is not a gate. r1_shim_residue_sweep.py passes on
    the live qemu-port tree, but nothing proved it would notice a regression
    reappearing. This test seeds throwaway trees and asserts the sweep's
    verdict flips with their content.

WHAT IS ASSERTED
    negative tree   empty Makefile + fs/r1_fs_channel.cpp
                    -> RESULT: FAIL, rc != 0, the offending path named
    positive tree   empty Makefile only
                    -> RESULT: PASS, rc == 0

    Both trees are exercised with --help, --list and --json as well, and the
    sweep source is compile()-checked in memory (no round-trip through disk).

    Deliberately NOT seeded: fs/r1_fs_core.*, fs/r1_fs_server.*,
    tests/r1_fs_core. Those are LIVE_KEEP, i.e. allowed by design; seeding
    them would mask the very regression the negative tree is meant to catch.

GUARANTEES
    Read-only towards the repo. Writes only inside tempfile.mkdtemp() trees,
    which are removed on exit. No QEMU, no make, no git, no network.
"""
from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SWEEP = HERE / "r1_shim_residue_sweep.py"

failures: list[str] = []


def check(cond: bool, label: str, detail: str = "") -> None:
    if cond:
        print(f"  ok   {label}")
        return
    print(f"  FAIL {label}")
    if detail:
        print(f"       {detail}")
    failures.append(label)


def run_sweep(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(SWEEP), *args],
                          capture_output=True, text=True, timeout=300)


def seed_negative(root: Path) -> Path:
    """Makefile + the retired variant/r1_fs_channel.cpp. No LIVE_KEEP files."""
    (root / "Makefile").write_text("")
    seeded = root / "fs" / "r1_fs_channel.cpp"
    seeded.parent.mkdir(parents=True, exist_ok=True)
    seeded.write_text("// retired R1Fs channel wire-opcode set\n")
    return seeded


def seed_positive(root: Path) -> None:
    """Makefile only: a clean tree the sweep must bless."""
    (root / "Makefile").write_text("")


def exercise(root: Path, seeded: Path | None, want: str, want_rc: int,
             tree_label: str) -> None:
    print(f"[{tree_label}] {root}")

    # --- in-memory compile: the patched source is syntactically valid --------
    src = SWEEP.read_text(encoding="utf-8")
    try:
        compile(src, str(SWEEP), "exec")
        check(True, "compile() in memory")
    except SyntaxError as exc:                      # pragma: no cover
        check(False, "compile() in memory", repr(exc))

    # --- --help -------------------------------------------------------------
    p = run_sweep("--help")
    check(p.returncode == 0 and "--root" in p.stdout,
          "--help rc 0 and documents --root", p.stdout[-300:] + p.stderr[-300:])

    # --- default (verdict) run via --root ------------------------------------
    p = run_sweep("--root", str(root))
    out = p.stdout
    check(p.returncode == want_rc, f"verdict rc == {want_rc}", str(p.returncode))
    result_line = [ln for ln in out.splitlines() if ln.startswith("RESULT: ")]
    check(bool(result_line) and result_line[-1].startswith(f"RESULT: {want}"),
          f"verdict line starts with 'RESULT: {want}'",
          result_line[-1] if result_line else "<no RESULT line>")
    # --root took effect: the header must name the seeded tree, not tools/..
    check(str(root) in out, "header names the --root tree", out.splitlines()[0] if out else "")
    if seeded is not None:
        named = any(seeded.name in ln and "fs/" in ln for ln in out.splitlines())
        check(named, f"names {seeded.relative_to(root).as_posix()}",
              out[:600])
    check((root / "Makefile").is_file(), "seeded tree left intact")

    # --- --list: dry inventory, always exit 0, no verdict -------------------
    p = run_sweep("--root", str(root), "--list")
    check(p.returncode == 0, "--list rc 0", str(p.returncode))
    check("RESULT:" not in p.stdout, "--list prints no verdict", p.stdout[:300])
    check(str(root) in p.stdout, "--list header names the --root tree",
          p.stdout.splitlines()[0] if p.stdout else "")

    # --- --json -------------------------------------------------------------
    p = run_sweep("--root", str(root), "--json")
    check(p.returncode == want_rc, f"--json rc == {want_rc}", str(p.returncode))
    try:
        doc = json.loads(p.stdout)
    except json.JSONDecodeError as exc:             # pragma: no cover
        check(False, "--json parses", repr(exc))
        doc = {}
    check(doc.get("verdict") == want, f"--json verdict == {want}",
          json.dumps(doc.get("verdict")))
    want_residue = 0 if want == "PASS" else None
    if want_residue is not None:
        check(doc.get("residue") == 0, "--json residue == 0",
              str(doc.get("residue")))
    else:
        check((doc.get("residue") or 0) > 0, "--json residue > 0",
              str(doc.get("residue")))
        hit = [f for f in doc.get("findings", [])
               if f.get("path") == seeded.relative_to(root).as_posix()]
        check(bool(hit), "--json findings include the seeded r1_fs file",
              json.dumps(doc.get("findings", []))[:600])
    print()


def main() -> int:
    if not SWEEP.is_file():
        print(f"RESULT: FAIL -- sweep not found at {SWEEP}")
        return 1

    base = Path(tempfile.mkdtemp(prefix="g12_neg_"))
    try:
        neg = base / "negative"
        pos = base / "positive"
        neg.mkdir()
        pos.mkdir()
        seeded = seed_negative(neg)
        seed_positive(pos)

        exercise(neg, seeded, "FAIL", 1, "negative control")
        exercise(pos, None, "PASS", 0, "positive control")

        # Cross-check the two trees genuinely differ in content.
        check(sorted(p.name for p in neg.rglob("*") if p.is_file()) !=
              sorted(p.name for p in pos.rglob("*") if p.is_file()),
              "trees differ (negative carries the retired file)")
        check(not any(p.relative_to(neg).as_posix().startswith(("fs/r1_fs_core",
                                                                "fs/r1_fs_server",
                                                                "tests/r1_fs_core"))
                      for p in neg.rglob("*")),
              "no LIVE_KEEP file seeded in the negative tree")
    finally:
        shutil.rmtree(base, ignore_errors=True)

    if failures:
        print(f"RESULT: FAIL ({len(failures)} assertion(s))")
        for f in failures:
            print(f"  - {f}")
        return 1
    print("RESULT: PASS -- sweep fails on a seeded retired r1_fs file and "
          "passes on a clean tree")
    return 0


if __name__ == "__main__":
    sys.exit(main())
