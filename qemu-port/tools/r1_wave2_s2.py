#!/usr/bin/env python3
"""r1_wave2_s2.py -- GUARDED Makefile edit for WAVE2 / S2 (docs/S2_PLAN.md).

S2 unwires the R1 private FS channel from the build: it deletes ONE 10-line
hunk, `Makefile:342-351` (the `R1_FS_CHANNEL ?= 0` opt-in block), and touches
nothing else.  The channel sources (fs/r1_fs_channel*, fs/r1_fs_channel_state.h,
fs/r1_fs_wire_logic.h, variant/r1_fs_channel.*, tests/r1_fs_channel) are KEPT --
S3 deletes those; S2 only unpicks the opt-in.

Every guard is reused BY IMPORT from tools/r1_winsrv_delete.py (never copied):
sha256, safe_target, git_porcelain, kernelhwsrv_clean and the /tmp/opencode
BACKUP_ROOT layout.  The gate ladder itself is reused BY IMPORT from
tools/r1_wave2_delete.py (run_cmd, gate_g12, gate_retire, gate_fullchain,
gate_strict, _gate); its R/F/S expectations are already parameterised and equal
S2's (4/4, 14/14, 11/11), and gate_g12 takes the expected residue count as an
argument -- which is the only number S2 needs to differ on.

HARD CONTRACT
  * The ONLY path this tool can ever write is `Makefile`.  Any other path --
    positional, --rollback dir inside qemu-port/, typo, glob -- is refused
    (exit 2) before a single write, so the 10 channel sources S3 owns and the
    READ-ONLY kernelhwsrv/ are unreachable.
  * The edit is content-bound twice: the pre-state must hash to d6f55b8d...ef98,
    and the exact 10-line hunk must occur EXACTLY ONCE (imported check_edits;
    0 occurrences = already applied, never a failure).  --apply needs BOTH
    --apply AND --confirm EDIT-WAVE2-S2-d6f55b8d, so a stale confirmation can
    never edit a Makefile that has since moved.
  * Pre-gate ladder (all must pass, else exit 3, nothing written): G12 residue
    count 18, retire 4/4, fullchain 14/14, strict 11/11, plus the two static
    make probes of docs/S2_MW_GATE_FIX.md: the DEFAULT `make -n kernfull` must
    name NO channel object (the channel is opt-in, `R1_FS_CHANNEL ?= 0`),
    while `R1_FS_CHANNEL=1 make -n kernfull` MUST name r1_fs_channel.o.
  * Post-gate ladder: G12 residue EXACTLY 10 if S1 (fs/r1_fs_core.mmp, sibling
    step) has NOT landed, or EXACTLY 9 if it HAS -- 18 minus this block's 8
    residue lines, minus S1's 1 when present.  Which of the two applies is
    derived from fs/r1_fs_core.mmp's presence and recorded in the manifest and
    the run log, never guessed.  Plus retire/fullchain/strict and
    `make -n kernfull` naming no channel object.  Any shortfall auto-rolls back.
  * Snapshot to /tmp/opencode/wave2_s2_backup_<stamp>/ with a sha256-verified
    manifest (schema r1-wave2-s2-edit-backup/v1) and rollback token
    ROLLBACK-WAVE2-S2-<stamp>; restore via --rollback DIR --confirm TOKEN
    (staged temp + verified os.replace, sha256 checked before AND after).
  * NEVER touches kernelhwsrv/ (READ-ONLY), NEVER commits or pushes, NEVER runs
    `make clean`, NEVER a bare `rm -rf`, NEVER boots QEMU in plan mode.
  * Out of scope by design: the stale comment prose at Makefile:337-341 (it
    names no symbol and is evidence, not wiring) and the build/link check --
    both stay for the human-reviewed follow-up.

Exit codes: 0 plan success; 2 refusal (bad args/token/dirty target/unsafe or
foreign path/forbidden argv); 3 pre-apply gate ladder not PASS; 4 write error
(rolled back); 5 post-edit gate ladder not PASS (rolled back); 6 rollback
FAILED (manual restore required).

Usage:
  python3 tools/r1_wave2_s2.py                    # plan, then REFUSE (rc 2)
  python3 tools/r1_wave2_s2.py --plan             # plan, rc 0, no writes
  python3 tools/r1_wave2_s2.py --apply --confirm EDIT-WAVE2-S2-d6f55b8d
  python3 tools/r1_wave2_s2.py --rollback /tmp/opencode/wave2_s2_backup_<stamp> \\
      --confirm ROLLBACK-WAVE2-S2-<stamp>
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

# Reuse the proven guards from the winsrv deletion tool BY IMPORT (never copied).
sys.path.insert(0, str(TOOLS))
import r1_winsrv_delete as wsd  # noqa: E402
import r1_wave2_delete as w2d  # noqa: E402  (reuses r1_winsrv_delete itself)

sha256 = wsd.sha256
safe_target = wsd.safe_target
git_porcelain = wsd.git_porcelain
kernelhwsrv_clean = wsd.kernelhwsrv_clean
check_edits = wsd.check_edits            # exactly-one-occurrence assertion
BACKUP_ROOT = wsd.BACKUP_ROOT            # /tmp/opencode

run_cmd = w2d.run_cmd                    # runs a gated command, forbids rm/clean
gate_g12 = w2d.gate_g12                  # expected residue count is a parameter
gate_retire = w2d.gate_retire            # 4/4
gate_fullchain = w2d.gate_fullchain      # 14/14 rows
gate_strict = w2d.gate_strict            # 11/11 pass, 0 skip, 0 fail
_gate = w2d._gate                        # shared gate record (print + dict)

# ---------------------------------------------------------------- op set ----
# The ONE path this tool may ever write (docs/S2_PLAN.md).
TARGET = "Makefile"
EDIT_SET = frozenset({TARGET})

PRE_SHA256 = "d6f55b8d047db5cf1f8501af374a3c1121ab0cd08f4ba76ff1f46b939306ef98"
CONFIRM_TOKEN = "EDIT-WAVE2-S2-" + PRE_SHA256[:8]
BACKUP_SCHEMA = "r1-wave2-s2-edit-backup/v1"
BACKUP_PREFIX = "wave2_s2_backup_"
ROLLBACK_PREFIX = "ROLLBACK-WAVE2-S2-"
TMP_SUFFIX = ".wave2_s2_tmp"

LOCATION = "Makefile:342-351"
# The hunk as exact bytes: 8 residue-counted lines plus the two continuations
# (file-prereq line and `endif`) that go with it -- 10 lines, nothing else.
OLD_LINES = [
    "R1_FS_CHANNEL ?= 0",
    "R1_FS_CHANNEL_OBJ :=",
    "ifeq ($(strip $(R1_FS_CHANNEL)),1)",
    "R1_FS_CHANNEL_OBJ := build/r1_fs_channel.o",
    "build/r1_fs_channel.o: variant/r1_fs_channel.cpp variant/r1_fs_channel.h \\",
    " fs/r1_fs_channel.h fs/r1_fs_channel_state.h fs/r1_fs_wire_logic.h \\",
    " variant/r1_fat_offset.h | build",
    "\t$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/r1_fs_channel.cpp -o $@",
    "endif",
    "KERNFULL_OBJS += $(R1_FS_CHANNEL_OBJ)",
]
OLD = "\n".join(OLD_LINES) + "\n"
OPS = [{"id": "E1", "type": "edit", "path": TARGET, "location": LOCATION,
        "old": OLD, "new": "", "index": 0}]

KEPT_NOTE = ("KEPT (S3 owns them, not unwired here): fs/r1_fs_channel.h, "
             "fs/r1_fs_channel_state.h, fs/r1_fs_wire_logic.h, "
             "variant/r1_fs_channel.cpp/.h, fs/r1_fs_channel*.c, "
             "tests/r1_fs_channel; also KEPT: Makefile:337-341 comment prose "
             "(no symbol, evidence) and every r1_g_* / KBINJ / netstack wiring")

# Gate ladder (docs/S2_PLAN.md "Pre-gate"/"Post-gate").
S1_TARGET = "fs/r1_fs_core.mmp"          # sibling S1 step's one-path delete
G12_PRE_PENDING = 18                     # S1 not landed: residue before
G12_PRE_LANDED = 17                      # S1 landed:    residue before
G12_POST_PENDING = 10                    # 18 - 8 (this block), no S1
G12_POST_LANDED = 9                      # 17 - 8 when S1 already took its own 1
RETIRE_EXPECT = w2d.RETIRE_EXPECT        # 4/4
FULLCHAIN_EXPECT = w2d.FULLCHAIN_EXPECT  # 14/14 rows
STRICT_EXPECT = w2d.STRICT_EXPECT        # 11/11
GATE_DIR = BACKUP_ROOT / "wave2_s2_gate"

# argv guard: this tool's own source legitimately calls os.replace, so the
# guard scans ARGV only, never the source.
FORBIDDEN_ARGV = ("git commit", "git push", "make clean", "rm -rf", "rm -r ")
FLAGS = {"--apply", "--plan", "--dry-run", "-h", "--help"}
VALUE_FLAGS = ("--rollback", "--confirm")


# ----------------------------------------------------------- static state ---
def s1_landed() -> bool:
    """S1's one-path delete removes fs/r1_fs_core.mmp; its absence = S1 landed.

    This is the ONLY discriminator used to pick the expected G12 counts, and it
    is recorded in the manifest and the log so a reading is never implicit.
    """
    return not (PORT / S1_TARGET).is_file()


def expectations() -> dict:
    landed = s1_landed()
    return {"s1_landed": landed,
            "g12_pre": G12_PRE_LANDED if landed else G12_PRE_PENDING,
            "g12_post": G12_POST_LANDED if landed else G12_POST_PENDING,
            "which": ("S1 landed (fs/r1_fs_core.mmp gone)" if landed else
                      "S1 pending (fs/r1_fs_core.mmp present)")}


def target_state() -> dict:
    """Read-only facts about Makefile; nothing is written."""
    p = PORT / TARGET
    info = {"path": TARGET, "in_set": TARGET in EDIT_SET,
            "safe": safe_target(TARGET), "exists": p.is_file()}
    if info["exists"]:
        info["size"] = p.stat().st_size
        info["sha256"] = sha256(p)
    info["sha256_ok"] = info.get("sha256") == PRE_SHA256
    return info


def target_git_clean() -> tuple[bool, str]:
    """REFUSE if Makefile is already modified (no stacking edits)."""
    return git_porcelain([str(Path("qemu-port") / TARGET)])


def hunk_count() -> int:
    p = PORT / TARGET
    return p.read_bytes().count(OLD.encode("utf-8")) if p.is_file() else 0


def expected_post_state() -> dict:
    """Compute the exact post-edit bytes in memory: the ONE hunk removed once."""
    p = PORT / TARGET
    if not p.is_file():
        return {"ok": False, "reason": "target file missing"}
    raw = p.read_bytes()
    old = OLD.encode("utf-8")
    n = raw.count(old)
    if n != 1:
        return {"ok": False, "reason": f"expected exactly one occurrence; got {n}"}
    post = raw.replace(old, b"", 1)
    if b"R1_FS_CHANNEL" in post:
        return {"ok": False, "reason": "post-state would still name R1_FS_CHANNEL"}
    return {"ok": True, "bytes": post, "size": len(post),
            "sha256": hashlib.sha256(post).hexdigest()}


def make_n_lines(opt_in: bool = False) -> tuple[bool, str]:
    """`make -n kernfull` (dry run, never clean) -- the build wiring probe.

    The channel is opt-in, so the flag is set EXPLICITLY rather than inherited:
    a launch with R1_FS_CHANNEL=1 in the environment must not turn the default
    probe into an opt-in probe.
    """
    argv = (["env", "R1_FS_CHANNEL=1"] if opt_in else
            ["env", "-u", "R1_FS_CHANNEL"])
    argv += ["make", "-n", "kernfull"]
    tag = "make -n kernfull" + (" (R1_FS_CHANNEL=1)" if opt_in else " (default)")
    proc = run_cmd(argv, tag, timeout=300)
    out = proc.stdout + proc.stderr
    return ("r1_fs_channel.o" in out), out


def gate_make_wiring(expect_wired: bool) -> dict:
    """Two static probes (docs/S2_MW_GATE_FIX.md); never a clean, never a build.

    MW-a  default `make -n kernfull` MUST NOT name r1_fs_channel.o -- asserting
          wiring here demanded a default link the tree never had: `R1_FS_CHANNEL
          ?= 0` guards the object and its rule behind the opt-in.
    MW-b  `R1_FS_CHANNEL=1 make -n kernfull` MUST name it before the edit
          (opt-in wiring intact) and MUST NOT after it (S2 removes the opt-in).
    """
    try:
        d_wired, d_out = make_n_lines(opt_in=False)
        o_wired, o_out = make_n_lines(opt_in=True)
    except (OSError, subprocess.SubprocessError) as exc:
        return _gate("MW", False, f"`make -n kernfull` could not run: {exc}")
    want = "wired" if expect_wired else "absent"
    ok = d_wired is False and o_wired is expect_wired
    return _gate("MW", ok,
                 f"MW-a default {('wired' if d_wired else 'absent')} (expected "
                 f"absent; {len(d_out.splitlines())} lines), MW-b "
                 f"R1_FS_CHANNEL=1 {('wired' if o_wired else 'absent')} "
                 f"(expected {want}; {len(o_out.splitlines())} lines)")


# --------------------------------------------------------------- argv guard --
def _inside_port(path: str) -> bool:
    p = Path(path)
    if not p.is_absolute():
        p = PORT / p
    try:
        rp = p.resolve()
    except OSError:
        return True
    return rp == PORT.resolve() or PORT.resolve() in rp.parents


def argv_refusals(argv: list[str]) -> list[str]:
    """Refuse any token that is not this tool's own flag or one of its values.

    Positional arguments are the real hazard: one typo or glob must never reach
    the 10 S3-owned channel sources or the READ-ONLY kernelhwsrv/ tree, so no
    positional path is accepted at all.
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
        bad.append(tok)                  # a positional path / glob / typo
    return bad


def refuse_if_foreign(argv: list[str]) -> int | None:
    """Shared pre-parse refusal; returns an exit code or None to continue."""
    joined = " ".join(argv)
    if any(v in joined for v in FORBIDDEN_ARGV):
        print("REFUSE: forbidden operation requested in argv (git commit / "
              "git push / make clean / rm -rf). Nothing changed.", file=sys.stderr)
        return 2
    bad = argv_refusals(argv)
    if bad:
        print(f"REFUSE: argument(s) outside the one-path set {sorted(EDIT_SET)}: "
              f"{bad!r}. Nothing changed.", file=sys.stderr)
        return 2
    return None


# ------------------------------------------------------------------ gates ---
def run_ladder(g12_expect: int, tag: str, wired: bool) -> dict:
    """G12 first (cheap, static) then R / F / S, plus the make-wiring probe."""
    GATE_DIR.mkdir(parents=True, exist_ok=True)
    gates = {"MW": gate_make_wiring(wired)}
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
def collect_pre_state() -> dict:
    st = target_state()
    clean, detail = kernelhwsrv_clean()
    gclean, gdetail = target_git_clean()
    return {"state": st, "hunk": hunk_count(), "post": expected_post_state(),
            "exp": expectations(),
            "kernelhwsrv": detail if clean else f"MODIFIED: {detail}",
            "kernelhwsrv_ok": clean,
            "git": "clean" if gclean else f"MODIFIED: {gdetail}", "git_ok": gclean}


def print_plan(pre: dict) -> None:
    exp = pre["exp"]
    print("r1_wave2_s2 -- PLAN: no file is changed; no QEMU is booted by this mode.")
    print(f"workspace: {PORT}")
    print(f"kernelhwsrv/ clean: {pre['kernelhwsrv']}")
    print(f"Makefile git state: {pre['git']}")
    st = pre["state"]
    print(f"Makefile sha256/size: {'MATCH' if st['sha256_ok'] else 'MISMATCH'}"
          f"  ({st.get('size')} B, {st.get('sha256')})")
    print(f"hunk occurrence count: {pre['hunk']} (need exactly 1; 0 = already applied)")
    print(f"post-state (computed in memory, sha256): "
          f"{pre['post'].get('sha256', 'n/a')}  ({pre['post'].get('reason', 'ok')})")

    print("\n=== EXACT OP (1 of 1) ===")
    print(f"  E1  edit  {LOCATION}  remove 10 lines ({len(OLD.encode())} B hunk)")
    for line in OLD_LINES:
        print(f"      - {line}")
    print("      action: staged temp + os.replace(qemu-port/Makefile)  # no rm, "
          "no glob, no recursion")
    print(f"  {KEPT_NOTE}")

    print("\n=== GATES ===")
    print("  pre  (all must pass before the write, else exit 3, nothing touched):")
    print(f"    G12  python3 tools/r1_shim_residue_sweep.py --json        "
          f"residue count == {exp['g12_pre']}  ({exp['which']})")
    print(f"    R    python3 tools/r1_genuine_retire_test.py              "
          f"{RETIRE_EXPECT}/{RETIRE_EXPECT}")
    print(f"    F    python3 tools/r1_genuine_fullchain_test.py           "
          f"{FULLCHAIN_EXPECT[0]}/{FULLCHAIN_EXPECT[1]} rows")
    print(f"    S    python3 tools/r1_final_verify.py --strict-skip       "
          f"{STRICT_EXPECT}/{STRICT_EXPECT}, 0 skip, 0 fail")
    print("    MW-a make -n kernfull          r1_fs_channel.o          must be "
          "ABSENT (R1_FS_CHANNEL ?= 0: the channel is opt-in)")
    print("    MW-b R1_FS_CHANNEL=1 make -n kernfull                   must name "
          "r1_fs_channel.o (opt-in wiring intact)")
    print(f"  post (after the edit, else auto-rollback + re-gate): G12 residue "
          f"EXACTLY {exp['g12_post']}  ({exp['which']}: {exp['g12_pre']} - 8"
          f"{' - 1 for S1' if exp['s1_landed'] else ''}; the other count is "
          f"NO-GO), plus R {RETIRE_EXPECT}/{RETIRE_EXPECT}, F "
          f"{FULLCHAIN_EXPECT[0]}/{FULLCHAIN_EXPECT[1]}, S {STRICT_EXPECT}/"
          f"{STRICT_EXPECT}, MW-a AND MW-b both empty")
    print("  (this mode runs no gate: no QEMU, no build, static only)")

    print("\n=== GUARDS ===")
    print(f"  scope     : only {TARGET} -- any positional arg or foreign "
          "--rollback dir -> exit 2")
    print(f"  confirm   : --apply --confirm {CONFIRM_TOKEN}  (bound to pre-state "
          f"sha256 {PRE_SHA256[:8]})")
    print("  refusals  : argv git commit / git push / make clean / rm -rf; dirty "
          "kernelhwsrv/; already-modified Makefile; sha256 mismatch; hunk count "
          "!= 1")
    print(f"  snapshot  : {BACKUP_ROOT}/{BACKUP_PREFIX}<YYYYmmdd_HHMMSS>/  "
          f"(manifest schema {BACKUP_SCHEMA}, sha256 verified at copy)")
    print(f"  rollback  : --rollback DIR --confirm {ROLLBACK_PREFIX}<stamp>  "
          "(sha256 checked before AND after restore)")
    print("  never     : kernelhwsrv/ writes, commit, push, `make clean`, QEMU in "
          "plan mode, bare rm")
    print(f"  out of scope: Makefile:337-341 comment reword + the link build "
          "(human-reviewed follow-up)")


# --------------------------------------------------------------- snapshot ---
def make_backup(stamp: str, pre_result: dict, post: dict,
                exp: dict) -> tuple[Path, dict]:
    """Snapshot Makefile; verify the copy's sha256 against the pre-state.

    `exp` is the same expectations() record whose g12_pre drove the pre-ladder.
    run_ladder returns only {ok, gates}, so the expected residue is passed in
    from the caller that computed it -- never read from a key it lacks.
    """
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
    shutil.copy2(src, dst)
    src_sum, dst_sum = sha256(src), sha256(dst)
    if src_sum != dst_sum or src_sum != PRE_SHA256:
        raise RuntimeError(f"snapshot verification FAILED for {TARGET}: "
                           f"src={src_sum} dst={dst_sum} expected={PRE_SHA256}")
    manifest = {"schema": BACKUP_SCHEMA, "created": stamp, "dir": d.name,
                "op": "edit", "path": TARGET, "location": LOCATION,
                "confirm_token": CONFIRM_TOKEN, "rollback_token": token,
                "pre_sha256": src_sum, "expected_post_sha256": post["sha256"],
                "removed_lines": OLD_LINES,
                "s1_landed": exp["s1_landed"], "g12_which": exp["which"],
                "expected_g12": {"pre": exp["g12_pre"], "post": exp["g12_post"]},
                "pre_gates": {k: v["detail"] for k, v in pre_result["gates"].items()},
                "files": [{"path": TARGET, "sha256": src_sum,
                           "size": src.stat().st_size}]}
    (d / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return d, manifest


# --------------------------------------------------------------- rollback ---
def do_rollback(backup_dir: Path, confirm: str | None = None) -> int:
    """Restore Makefile from the snapshot via staged temp + verified os.replace."""
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
        print(f"REFUSE: unsupported backup manifest schema (need "
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
        if rel not in EDIT_SET or not safe_target(rel) or Path(rel).is_absolute() \
                or ".." in Path(rel).parts:
            print(f"REFUSE: manifest path {rel!r} is not the allowlisted "
                  f"qemu-port target. Nothing restored.", file=sys.stderr)
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
            print(f"REFUSE: staged temp sha256 mismatch for {rel}; nothing "
                  "restored.", file=sys.stderr)
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
def apply_edit() -> int:
    pre = collect_pre_state()
    st, post, exp = pre["state"], pre["post"], pre["exp"]
    problems = []
    if not st["in_set"]:
        problems.append("target is not in the one-path EDIT_SET")
    if not st["safe"]:
        problems.append("target outside qemu-port/ or under kernelhwsrv/")
    if not st["exists"]:
        problems.append("target Makefile missing")
    elif not st["sha256_ok"]:
        problems.append(f"sha256 {st.get('sha256')} != expected {PRE_SHA256}")
    if pre["hunk"] != 1:
        problems.append(f"hunk occurs {pre['hunk']}x, expected exactly 1 "
                        "(0 = already applied)")
    if not post["ok"]:
        problems.append(f"post-state not derivable: {post.get('reason')}")
    if not pre["kernelhwsrv_ok"]:
        problems.append(f"kernelhwsrv/ modified: {pre['kernelhwsrv']}")
    if not pre["git_ok"]:
        problems.append(f"Makefile already modified: {pre['git']}")
    if problems:
        for p in problems:
            print(f"REFUSE: {p}", file=sys.stderr)
        print("Nothing changed.", file=sys.stderr)
        return 2
    print(f"  target verified: {TARGET} {st['size']} B sha256 {st['sha256']}")
    print(f"  hunk {LOCATION}: exactly 1 occurrence, {len(OLD.encode())} B")
    print(f"  post-state expected sha256: {post['sha256']}")
    print(f"  G12 expectations: pre {exp['g12_pre']}, post {exp['g12_post']} "
          f"({exp['which']})")
    print(f"  kernelhwsrv/ clean: {pre['kernelhwsrv']}")
    print(f"  Makefile git state: {pre['git']}")

    pre_result = run_ladder(exp["g12_pre"], "pre", wired=True)
    if not pre_result["ok"]:
        print(f"REFUSE: pre-apply gate ladder NOT PASS (need G12 residue exactly "
              f"{exp['g12_pre']}, R {RETIRE_EXPECT}/{RETIRE_EXPECT}, F "
              f"{FULLCHAIN_EXPECT[0]}/{FULLCHAIN_EXPECT[1]}, S {STRICT_EXPECT}, "
              "MW-a: a clean default `make -n kernfull`, and MW-b: the opt-in "
              "probe naming r1_fs_channel.o). No file changed.",
              file=sys.stderr)
        return 3

    stamp = time.strftime("%Y%m%d_%H%M%S")
    try:
        backup, manifest = make_backup(stamp, pre_result, post, exp)
    except (OSError, RuntimeError) as exc:
        print(f"REFUSE: snapshot failed ({exc}); nothing edited.", file=sys.stderr)
        return 2
    token = manifest["rollback_token"]
    print(f"  snapshot: {backup}  (rollback token: {token})")

    ok = False
    stage = "edit"
    rollback_rc = 0
    dst = PORT / TARGET
    try:
        if sha256(dst) != PRE_SHA256:
            raise RuntimeError("Makefile changed between snapshot and edit")
        tmp = dst.parent / (dst.name + TMP_SUFFIX)
        tmp.write_bytes(post["bytes"])
        shutil.copymode(dst, tmp)
        if sha256(tmp) != post["sha256"]:
            tmp.unlink()
            raise RuntimeError("staged temp sha256 does not match the computed "
                               "post-state; refusing to replace Makefile")
        os.replace(tmp, dst)
        print(f"  edited {TARGET}: removed {len(OLD_LINES)} lines at {LOCATION}")
        stage = "post"
        if sha256(dst) != post["sha256"]:
            raise RuntimeError("post-edit sha256 mismatch on Makefile")
        if hunk_count() != 0:
            raise RuntimeError("removed hunk is still present after the edit")
        post_result = run_ladder(exp["g12_post"], "post", wired=False)
        if not post_result["ok"]:
            g12 = post_result["gates"]["g12"]["detail"]
            raise RuntimeError(f"post-edit gate ladder NOT PASS ({g12}; need "
                               f"exactly {exp['g12_post']} residue "
                               f"[{exp['which']}] plus R/F/S PASS and no channel "
                               "object in either `make -n kernfull` probe)")
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
                    re_result = run_ladder(exp["g12_pre"], "rollback", wired=True)
                    print(f"  gate after rollback: "
                          f"{'PASS' if re_result['ok'] else 'NOT PASS'}")
                except Exception as exc:
                    print(f"  gate after rollback unavailable: {exc}",
                          file=sys.stderr)

    if ok:
        print("\nSUCCESS: S2 Makefile edit applied (1 file, 10 lines); pre and "
              "post gate ladders PASS.")
        print(f"  snapshot: {backup}  (token {token})")
        print(f"  expected G12 now: {exp['g12_post']} residue (was "
              f"{exp['g12_pre']}; {exp['which']})")
        print("  next (human-reviewed, NOT by this tool): reword Makefile:337-341, "
              "build/link kernfull, then commit when authorized.")
        print(f"  git fallback: git -C {REPO} checkout -- qemu-port/{TARGET}")
        return 0
    if rollback_rc != 0:
        return 6
    return 5 if stage == "post" else 4


# ------------------------------------------------------------------- main ---
def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv if argv is None else argv)
    rest = argv[1:].copy()
    # The argv guard runs BEFORE argparse so a foreign path, a glob or a
    # forbidden verb is refused by this tool's own contract, not as a usage error.
    rc = refuse_if_foreign(argv)
    if rc is not None:
        return rc
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", action="store_true",
                    help="actually remove the 10-line hunk (requires --confirm "
                         + CONFIRM_TOKEN + ")")
    ap.add_argument("--plan", "--dry-run", dest="plan", action="store_true",
                    help="pure preview: print the exact op and gates, then exit 0 "
                         "(default is the same preview, then REFUSE)")
    ap.add_argument("--confirm", metavar="TOKEN",
                    help="exact confirm token (" + CONFIRM_TOKEN + ") or a "
                         "snapshot's " + ROLLBACK_PREFIX + "<stamp> rollback token")
    ap.add_argument("--rollback", metavar="DIR",
                    help="restore Makefile from a backup dir (requires --confirm "
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
        return apply_edit()

    # Preview (no args, --plan or --dry-run): static facts only, no QEMU.
    print_plan(collect_pre_state())
    if args.plan:
        return 0
    print("REFUSE: no --apply given -- plan only, nothing changed.", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
