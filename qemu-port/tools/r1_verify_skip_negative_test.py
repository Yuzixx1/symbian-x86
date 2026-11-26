#!/usr/bin/env python3
"""r1_verify_skip_negative_test.py -- falsifiability proof for B4.

Claim under test: in `tools/r1_final_verify.py` a `RESULT: SKIP` row is fatal
in the DEFAULT invocation, not only under `--strict-skip`, while PASS/FAIL
handling and `--strict-skip` behaviour are unchanged.

The test seeds a throwaway tree (a temp dir with fake gate scripts that just
print a verdict) and drives the REAL verifier module from this repo against it:

  * the verifier is imported by path, not re-implemented, so the code under
    test is the shipped code;
  * the fake gates are ~10-line python scripts, so no QEMU, no `make`, no ISO,
    no network, no artifacts and no writes outside a temporary directory;
  * every case asserts on the process exit code AND the printed
    `OVERALL:` line, so a change that only moves output around still fails.

Exit status: 0 when the fix is proven present, 1 when the behaviour is
falsified (e.g. the patch is not applied) or the harness itself breaks.

Usage (from qemu-port/, or anywhere -- the path is derived from __file__):
    python3 tools/r1_verify_skip_negative_test.py
    python3 tools/r1_verify_skip_negative_test.py -v   # per-case transcript
"""
import argparse
import contextlib
import hashlib
import importlib.util
import io
import os
import re
import shutil
import sys
import tempfile
import traceback

VERIFIER = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        "r1_final_verify.py")
OVERALL_RE = re.compile(r"^OVERALL: (PASS|FAIL)\b.*$", re.MULTILINE)

# Fake gate bodies. Kept deliberately tiny: each prints exactly one verdict
# line and exits with the requested status.
GATES = {
    "pass": 'print("RESULT: PASS  fake gate proved something")\n',
    "skip": 'print("RESULT: SKIP  fake gate proved nothing")\n',
    "fail": 'print("RESULT: FAIL  fake gate broke")\nraise SystemExit(1)\n',
}


class Case:
    """One falsifiable expectation about the verifier's verdict handling."""

    def __init__(self, name, roster, argv, want_rc, want_overall, why,
                 want_text=None, want_rc_any=None):
        self.name = name
        self.roster = roster          # gate kinds, in order
        self.argv = argv              # extra argv for the verifier
        self.want_rc = want_rc
        self.want_rc_any = want_rc_any
        self.want_overall = want_overall
        self.want_text = want_text or []
        self.why = why


def load_verifier():
    """Import tools/r1_final_verify.py by path as a private module."""
    if not os.path.isfile(VERIFIER):
        raise SystemExit(f"FAILED: verifier not found at {VERIFIER}")
    spec = importlib.util.spec_from_file_location("_r1_final_verify_undertest",
                                                 VERIFIER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def seed_tree(root):
    """Create <root>/tools with one fake gate script per verdict kind."""
    tools = os.path.join(root, "tools")
    os.makedirs(tools, exist_ok=True)
    for kind, body in GATES.items():
        path = os.path.join(tools, f"fake_{kind}_gate.py")
        with open(path, "w") as fh:
            fh.write("#!/usr/bin/env python3\n" + body)
        os.chmod(path, 0o755)
    logs = os.path.join(root, "logs")
    os.makedirs(logs, exist_ok=True)
    return tools, logs


def run_case(mod, case, root, logs, verbose):
    """Drive mod.main() for one case; return (rc, output)."""
    tools, _ = seed_tree(root)
    roster = [mod.Test(f"fake_{kind}", f"tools/fake_{kind}_gate.py", [], 60)
              for kind in case.roster]
    saved = (mod.PORT, mod.DEFAULT_LOGDIR, getattr(mod, "DEFAULT_TESTS", None))
    # Point the verifier's own constants at the throwaway tree so its
    # subprocess cwd, script paths and logs never escape the temp dir.
    mod.PORT = root
    mod.DEFAULT_LOGDIR = logs
    mod.DEFAULT_TESTS = roster
    sys.argv = ["r1_final_verify.py", "--log-dir", logs] + case.argv
    out = io.StringIO()
    rc = None
    try:
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
            rc = mod.main()
    except SystemExit as exc:                  # argparse rejected an option
        rc = f"SystemExit({exc.code})"
    finally:
        mod.PORT, mod.DEFAULT_LOGDIR = saved[0], saved[1]
        if saved[2] is not None:
            mod.DEFAULT_TESTS = saved[2]
    text = out.getvalue()
    if verbose:
        print(f"--- case {case.name}: argv={case.argv} roster={case.roster}")
        print(text.rstrip() or "(no output)")
    return rc, text


def check(mod, case, root, logs, verbose):
    """Return None when the case holds, else a failure string."""
    rc, text = run_case(mod, case, root, logs, verbose)
    m = OVERALL_RE.search(text)
    overall = m.group(1) if m else None
    problems = []
    if case.want_rc_any is not None:
        if rc not in case.want_rc_any:
            problems.append(f"exit {rc!r} not in {case.want_rc_any}")
    elif rc != case.want_rc:
        problems.append(f"exit {rc!r}, wanted {case.want_rc}")
    if overall != case.want_overall:
        problems.append(f"OVERALL {overall!r}, wanted {case.want_overall!r}")
    for needle in case.want_text:
        if needle not in text:
            problems.append(f"output missing {needle!r}")
    if problems:
        tail = "\n".join(text.strip().splitlines()[-6:])
        return (f"{case.name}: {'; '.join(problems)}\n"
                f"    expectation: {case.why}\n"
                f"    verifier tail:\n{tail}")
    return None


def builtin_cases():
    """Expectations, stated as behaviour, not as implementation details."""
    return [
        Case("default_skip_is_fatal", ["skip"], [], 1, "FAIL",
             "a SKIP row must fail the DEFAULT invocation",
             want_text=["failed: fake_skip"]),
        Case("pass_does_not_mask_skip", ["pass", "skip"], [], 1, "FAIL",
             "one passing gate must not hide a gate that proved nothing",
             want_text=["1 pass, 1 skip, 1 fail/not-run"]),
        Case("strict_skip_still_fatal", ["skip"], ["--strict-skip"], 1, "FAIL",
             "--strict-skip behaviour is preserved"),
        Case("allow_skip_advisory", ["skip"], ["--allow-skip"], 0, "PASS",
             "--allow-skip is the explicit opt-out back to advisory SKIPs",
             want_text=["1 skip"]),
        Case("strict_beats_allow_skip", ["skip"],
             ["--allow-skip", "--strict-skip"], 1, "FAIL",
             "--strict-skip wins over --allow-skip on the same command line"),
        Case("pass_unchanged", ["pass"], [], 0, "PASS",
             "PASS handling is untouched: a passing gate still exits 0"),
        Case("fail_unchanged", ["fail"], [], 1, "FAIL",
             "FAIL handling is untouched: a failing gate still exits 1"),
        Case("no_keep_going_after_skip", ["skip", "pass"], ["--no-keep-going"],
             1, "FAIL",
             "the stop-on-first-fatal path must treat SKIP as fatal and mark "
             "the remaining rows NOT RUN",
             want_text=["NOT RUN"]),
    ]


def predicate_cases(mod):
    """Direct checks on is_failure(); cheap and independent of subprocesses."""
    rows = [
        (("SKIP",), True, "is_failure('SKIP') must be True by default"),
        (("SKIP", False), False,
         "is_failure('SKIP', skip_fatal=False) must be False"),
        (("PASS",), False, "is_failure('PASS') must stay False"),
        (("FAIL",), True, "is_failure('FAIL') must stay True"),
        (("TIMEOUT",), True, "is_failure('TIMEOUT') must stay True"),
        (("ERROR",), True, "is_failure('ERROR') must stay True"),
        (("NOT RUN",), True, "is_failure('NOT RUN') must stay True"),
    ]
    problems = []
    for args, want, why in rows:
        try:
            got = mod.is_failure(*args)
        except TypeError as exc:                # signature not patched
            problems.append(f"is_failure{args}: TypeError ({exc}); {why}")
            continue
        if got is not want:
            problems.append(f"is_failure{args} -> {got}, wanted {want}; {why}")
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("-v", "--verbose", action="store_true",
                    help="print the verifier's own output for every case")
    args = ap.parse_args()

    mod = load_verifier()
    digest = hashlib.sha256(open(VERIFIER, "rb").read()).hexdigest()[:16]
    roster = len(getattr(mod, "DEFAULT_TESTS", []))
    print(f"== under test: {VERIFIER} (sha256 {digest}, roster {roster} rows)")
    print("== fake gates only: no QEMU, no make, no ISO, temp dir only")

    root = tempfile.mkdtemp(prefix="r1_skip_negative_")
    logs = os.path.join(root, "logs")
    failures = []
    try:
        problems = predicate_cases(mod)
        failures.extend(problems)
        print(f"== is_failure(): {'ok' if not problems else 'FALSIFIED'}")
        for case in builtin_cases():
            bad = check(mod, case, root, logs, args.verbose)
            if bad:
                failures.append(bad)
                print(f"-- {case.name}: FALSIFIED")
            else:
                print(f"-- {case.name}: ok")
    except Exception:                            # harness bug, not a proof
        traceback.print_exc()
        print("RESULT: FAIL  (harness error)", file=sys.stderr)
        return 1
    finally:
        shutil.rmtree(root, ignore_errors=True)

    if failures:
        print()
        for bad in failures:
            print(f"FALSIFIED: {bad}")
        print("\nRESULT: FAIL  (SKIP is not fatal in the DEFAULT invocation)")
        return 1
    print("\nRESULT: PASS  (every seeded SKIP row failed the DEFAULT run, "
          "PASS/FAIL and --strict-skip unchanged)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
