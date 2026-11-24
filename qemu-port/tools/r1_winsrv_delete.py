#!/usr/bin/env python3
"""r1_winsrv_delete.py -- GUARDED executor for the winsrv deletion
(docs/WINSRV_DELETION_EXECUTOR.md + docs/WINSRV_DELETION_GATE.md).

This is the execution companion to the winsrv deletion gate docs.  Unlike
tools/r1_shim_retire_apply.py it DOES delete files: winsrv retires by
removing its files, so this tool has its own stricter allowlist (a static
DELETE_SET), while REUSING the proven helpers from r1_shim_retire_apply.py
BY IMPORT (never copied): sha256, git_porcelain, kernelhwsrv_clean,
safe_target, check_edits (exactly-one-occurrence assertion), run_gate
(r1_genuine_retire_test.py --only ewsrv, dry-run vs real), and the
PORT/REPO/BACKUP_ROOT layout constants.

HARD CONTRACT
  * No arguments (or --dry-run) is a pure preview: it prints the full op
    plan (phases P1 hooks -> P2 build wiring -> P3 source deletion) with
    per-op uniqueness assertions and REFUSES to change anything.  No QEMU
    is ever booted in preview mode.
  * An actual change requires BOTH `--apply` AND `--confirm DELETE-WINSRV-<token>`
    where the token is the one printed by this tool (CONFIRM_TOKEN below).
  * Before any write the tool re-runs the ewsrv retire gate WITHOUT
    --dry-run via the imported run_gate(); anything other than PASS is
    REFUSED (exit 3), no file touched.  P1-P6 preconditions are also checked.
  * It REFUSES if `git status --porcelain -- kernelhwsrv/` is non-empty, or
    if any op target is already modified.  Every op path must pass the
    imported safe_target() (inside qemu-port/, resolved, never under
    kernelhwsrv/).
  * Every `edit` op asserts its `old` string occurs EXACTLY ONCE (imported
    check_edits); MANY occurrences aborts before writing, but ZERO means the
    change is already applied (e.g. landed by an earlier commit such as
    0f03f7e) and the op auto-skips as "already applied" -- never a failure.
    `delete` ops are only accepted for paths in the static DELETE_SET.
  * Phases run in fixed order P1 -> P2 -> P3 with an incremental `make`
    (NEVER `make clean`) and a phase gate between them.  ANY failure after
    a write auto-rolls back every touched file (edits AND deletes) via the
    verified staged-temp + os.replace pattern, checks the rollback rc, and
    re-runs the gate after rollback.
  * Before the first write every op target (edited AND delete-listed) is
    snapshotted to /tmp/opencode/winsrv_retire_backup_<ts>/ with a sha256
    manifest (schema r1-winsrv-delete-backup/v1) and a rollback token
    ROLLBACK-WINSRV-<ts>.  Manual restore: --rollback DIR --confirm TOKEN.
  * NEVER touches kernelhwsrv/, NEVER commits or pushes, NEVER runs
    `make clean`, NEVER boots QEMU in preview mode, NEVER accepts
    `SessionPath r=-15` as IPC success, EFILE stays guard-only (no
    trampoline).

Exit codes: 0 preview success; 2 refusal (bad args/token/dirty target/
unsafe path/forbidden argv); 3 pre-apply gate not PASS; 4 write error
(rolled back); 5 phase/post gate failure (rolled back); 6 rollback FAILED
(manual restore required).

Usage:
  python3 tools/r1_winsrv_delete.py                     # plan + REFUSE (rc 2)
  python3 tools/r1_winsrv_delete.py --dry-run           # plan, rc 0, no writes
  python3 tools/r1_winsrv_delete.py --apply --confirm DELETE-WINSRV-<token>
  python3 tools/r1_winsrv_delete.py --rollback /tmp/opencode/winsrv_retire_backup_<ts> \
      --confirm ROLLBACK-WINSRV-<ts>
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

# Reuse the proven helpers from the shim-retire tool BY IMPORT (do not copy).
sys.path.insert(0, str(TOOLS))
import r1_shim_retire_apply as base  # noqa: E402

sha256 = base.sha256
git_porcelain = base.git_porcelain
kernelhwsrv_clean = base.kernelhwsrv_clean
safe_target = base.safe_target
check_edits = base.check_edits          # exactly-one-occurrence assertion
run_gate = base.run_gate                # r1_genuine_retire_test.py --only ewsrv
BACKUP_ROOT = base.BACKUP_ROOT          # /tmp/opencode

GATE_KEY = "ewsrv"                       # winsrv retire gate (plan entry gate=ewsrv)
CONFIRM_PREFIX = "DELETE-WINSRV-"
CONFIRM_TOKEN = CONFIRM_PREFIX + "EWSRV"  # printed by the plan; required with --apply
BACKUP_PREFIX = "winsrv_retire_backup_"
BACKUP_SCHEMA = "r1-winsrv-delete-backup/v1"
ROLLBACK_PREFIX = "ROLLBACK-WINSRV-"

# argv guard: this tool's own source legitimately calls os.remove for
# allowlisted deletes, so the guard scans ARGV only, never the source.
FORBIDDEN_ARGV = ("git commit", "git push", "make clean", "rm -rf", "rm -r ")

# ---------------------------------------------------------------- op set --
# Static DELETE_SET: the ONLY paths a `delete` op may ever remove
# (docs/WINSRV_DELETION_EXECUTOR.md §3 phase P3, D1-D9).  Refreshed at HEAD
# 0f03f7e after chunks A-D / P4 landed: user/wsdl.c and user/guilib.c are now
# MIGRATED live sources (ws_e32_client.c backend, zero winsrv refs -- P1 grep
# decides they stay, see user/user.mk:1227 guilib.o and :1384 wsdl.o), so they
# are NOT in the set; only the 7 genuine old-winsrv files are.
DELETE_SET = frozenset({
    "user/winsrv.c",
    "user/guishost.h",
    "user/wsclient.c",
    "user/wsclient.h",
    "variant/qemu_windowsrv.cpp",
    "variant/qemu_windowsrv.h",
    "variant/qemu_gui_exec.cpp",
})

# Explicitly KEPT (never in any op): variant/qemu_fbblit.* + exec 270 ops 2/3,
# all r1_* harnesses, netstack, PS/2 / AC'97 / storage / display bridges, the
# KBINJ genuine-EWSRV bridge (qemu_variant.h:253-280, qemu_variant_int.cpp
# :232-530,579-586, qemu_mouse.cpp:157-190 pointer side), user/ws_e32_client.c,
# user/shot_e32.c orphan, AND the migrated user/wsdl.c + user/guilib.c (chunks
# A-B landed at 0f03f7e: both now build on ws_e32_client.c with zero winsrv
# refs -- P1 grep decides they stay; WSDL.E32/GUIHELLO/DTEDIT/DTCALC live on).
KEPT_NOTE = ("KEPT: qemu_fbblit.* (exec 270), r1_* harnesses, netstack, "
             "PS/2+AC'97+storage+display bridges, KBINJ bridge "
             "(qemu_variant.h:253-280 / qemu_variant_int.cpp:232-530,579-586 / "
             "qemu_mouse.cpp:157-190), user/ws_e32_client.c, user/shot_e32.c, "
             "migrated user/wsdl.c + user/guilib.c")

# Edit ops: {id, phase, path, location, old, new}.  `old` must occur exactly
# once in the target file (asserted at plan AND at apply time); 0 occurrences
# = already applied -> auto-skip (not a failure).  Multi-line blocks are
# exact bytes, tabs written as \t.
EDIT_OPS = [
    # ---- Phase P1: kernel hooks + exec-table (file deletion comes in P3) ----
    {"id": "E1", "phase": "P1", "path": "variant/qemu_variant.h",
     "location": "variant/qemu_variant.h:103-111",
     "old": ("// GUI desktop shell exec host (variant/qemu_gui_exec.cpp). Exposes slow execs\n"
             "// 249 (fb base), 250 (fb size) and 251 (display-list runner / shared base)\n"
             "// to ring-3 E32 programs; layout is in user/guishost.h.\n"
             "TInt gui_init();                     // idempotent; installs execs 249/250/251\n"
             "// PS/2 button state -> userland bridge (variant/qemu_gui_exec.cpp).  The ISR\n"
             "// in qemu_mouse.cpp calls gui_publish_buttons() so gui_shared.buttons is live\n"
             "// without a display-list exec; gui_button_state() is the diagnostic readback.\n"
             "void gui_publish_buttons(TUint aButtons, TUint aSeq);   // ISR-safe, 2 stores\n"
             "void gui_button_state(TUint& aButtons, TUint& aSeq);    // kernel diagnostic\n"),
     "new": ""},
    {"id": "E1b", "phase": "P1", "path": "variant/qemu_variant.h",
     "location": "variant/qemu_variant.h:100",
     "old": "void GuiGeometryReset();             // refresh guishost shared fb geometry\n",
     "new": ""},
    {"id": "E2", "phase": "P1", "path": "variant/qemu_variant.h",
     "location": "variant/qemu_variant.h:243-251",
     "old": ("// Window-server shared memory + slow execs (variant/qemu_windowsrv.cpp).\n"
             "// Appended table slots 271 (base) / 272 (size) / 273 (multiplexed ctl);\n"
             "// layout + op codes in variant/qemu_windowsrv.h. Boot hook idempotent.\n"
             "TInt windowsrv_init();\n"
             "TUint32 windowsrv_base();\n"
             "TUint windowsrv_size();\n"
             "// PS/2 ISR bridges: no-ops until a window server registers via exec 273 op 1.\n"
             "void wsrv_publish_mouse(TInt aX, TInt aY, TUint aButtons, TUint aSeq);\n"
             "void wsrv_publish_key(TUint aKey);\n"),
     "new": ""},  # KEEP :253-280 (KBINJ genuine-EWSRV bridge)
    {"id": "E3", "phase": "P1", "path": "variant/qemu_mouse.cpp",
     "location": "variant/qemu_mouse.cpp:173",
     "old": "        wsrv_publish_mouse(gX, gY, gButtons, gButtonSeq);  // window-server ring\n",
     "new": ""},
    {"id": "E3b-1", "phase": "P1", "path": "variant/qemu_mouse.cpp",
     "location": "variant/qemu_mouse.cpp:42-44",
     "old": ("// Position bridge implemented in variant/qemu_gui_exec.cpp.  Declared locally\n"
             "// so the shared variant/qemu_variant.h (owned by another worker) is untouched.\n"
             "void gui_publish_position(TInt aX, TInt aY);\n"),
     "new": ""},
    {"id": "E3b-2", "phase": "P1", "path": "variant/qemu_mouse.cpp",
     "location": "variant/qemu_mouse.cpp:163",
     "old": "        gui_publish_buttons(gButtons, gButtonSeq);\n",
     "new": ""},
    {"id": "E3b-3", "phase": "P1", "path": "variant/qemu_mouse.cpp",
     "location": "variant/qemu_mouse.cpp:172",
     "old": "        gui_publish_position(gX, gY);\n",
     "new": ""},
    {"id": "E3b-4", "phase": "P1", "path": "variant/qemu_mouse.cpp",
     "location": "variant/qemu_mouse.cpp:302",
     "old": "    gui_publish_buttons(gButtons, gButtonSeq);\n    TInt x = (TInt)gX, y = (TInt)gY;\n",
     "new": "    TInt x = (TInt)gX, y = (TInt)gY;\n"},
    {"id": "E3b-5", "phase": "P1", "path": "variant/qemu_mouse.cpp",
     "location": "variant/qemu_mouse.cpp:346",
     "old": ("    gui_button_state(sharedBtns, sharedSeq);\n"
             "    KPrintf(\"mouse: shared buttons=%u seq=%u (driver buttons=%u seq=%u) %s\\r\\n\",\n"),
     "new": "    KPrintf(\"mouse: shared buttons=%u seq=%u (driver buttons=%u seq=%u) %s\\r\\n\",\n"},
    {"id": "E3b-6", "phase": "P1", "path": "variant/qemu_mouse.cpp",
     "location": "variant/qemu_mouse.cpp:420",
     "old": ("    gui_button_state(sharedBtns, sharedSeq);\n"
             "    KPrintf(\"MOUSE PRESS: mask=%u packets=%u driver buttons=%u seq=%u->%u \"\n"),
     "new": "    KPrintf(\"MOUSE PRESS: mask=%u packets=%u driver buttons=%u seq=%u->%u \"\n"},
    {"id": "E4", "phase": "P1", "path": "variant/qemu_display.cpp",
     "location": "variant/qemu_display.cpp:400",
     "old": "    GuiGeometryReset();                // guishost fb_w/fb_h + cursor\n",
     "new": ""},
    {"id": "E5", "phase": "P1", "path": "variant/kern_shell.cpp",
     "location": "variant/kern_shell.cpp:1751",
     "old": "            gui_button_state(b, s);\n",
     "new": ""},
    {"id": "E6", "phase": "P1", "path": "variant/qemu_variant_int.cpp",
     "location": "variant/qemu_variant_int.cpp:649",
     "old": "        wsrv_publish_key((unsigned int)c);  // window-server event ring\n",
     "new": ""},  # KEEP :648 QemuKbdPush(c) and the KBINJ path :232-530,579-586
    # ---- Phase P2: build wiring ----
    {"id": "E7", "phase": "P2", "path": "Makefile",
     "location": "Makefile:258-259",
     "old": ("build/qemu_gui_exec.o: variant/qemu_gui_exec.cpp variant/qemu_variant.h user/guishost.h | build\n"
             "\t$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_gui_exec.cpp -o $@\n"),
     "new": ""},
    {"id": "E8", "phase": "P2", "path": "Makefile",
     "location": "Makefile:261-262",
     "old": ("build/qemu_windowsrv.o: variant/qemu_windowsrv.cpp variant/qemu_windowsrv.h variant/qemu_variant.h | build\n"
             "\t$(CXX) $(KERN_FLAGS) $(KERN_DEFS) $(KERN_INCLUDES) -c variant/qemu_windowsrv.cpp -o $@\n"),
     "new": ""},
    {"id": "E9", "phase": "P2", "path": "Makefile",
     "location": "Makefile:318",
     "old": "build/qemu_gui_exec.o build/qemu_windowsrv.o ",
     "new": ""},  # keep build/qemu_fbblit.o
    # Refreshed at 0f03f7e: the guishost.h dep token is already gone from the
    # winsrv.o row (chunk landed), so the block now starts at the comment.
    {"id": "E10", "phase": "P2", "path": "user/user.mk",
     "location": "user/user.mk:1359-1374 (WINSRV.E32 comment+rules)",
     "old": ("\n# WINDOW SERVER E32 (WSV) -- a genuine PE2 E32 process that owns the VBE LFB,\n"
             "# composites client windows from the kernel's struct ws_shared (slow execs\n"
             "# WS_BASE / WS_SIZE / WS_OP), draws title bars / borders / focus highlight plus\n"
             "# a desktop and taskbar, and dispatches MOVE / DOWN / KEY events from the\n"
             "# shared input ring.  Falls back to a local shared instance when the kernel\n"
             "# window-server executives are unavailable.  Genuine PE2 import of EUSER.DLL\n"
             "# ordinal 915 (RDebug::Printf); IAT slot winsrv_printf_slot at 0x408000.\n"
             "userland: build/user/WINSRV.E32\n"
             "build/user/winsrv.o: user/winsrv.c user/runtime.h | build/user\n"
             "\t$(CC) $(USER_FLAGS) -c $< -o $@\n"
             "build/user/winsrv.elf: build/user/start.o build/user/runtime.o build/user/winsrv.o user/link.ld\n"
             "\tld -m elf_i386 --emit-relocs --defsym=winsrv_printf_slot=0x408000 -T user/link.ld -o $@ $(filter %.o,$^) $(LIBGCC)\n"
             "build/user/WINSRV.E32: build/user/winsrv.elf tools/elf2e32.py tools/mke32.py\n"
             "\tpython3 tools/elf2e32.py $< $@ --euser-import 915\n"
             "\tpython3 tools/e32read.py $@ --quiet\n"),
     "new": ""},
    # Refreshed at 0f03f7e: wsdl.c is MIGRATED (kept; WSDL.E32 stays, built from
    # wsdl.o + ws_e32_client.o).  Only the dead wsclient.o rule goes (its
    # source user/wsclient.c is in DELETE_SET and nothing links wsclient.o).
    {"id": "E11", "phase": "P2", "path": "user/user.mk",
     "location": "user/user.mk:1382-1383 (dead wsclient.o rule)",
     "old": ("build/user/wsclient.o: user/wsclient.c user/runtime.h | build/user\n"
             "\t$(CC) $(USER_FLAGS) -c $< -o $@\n"),
     "new": ""},
    # E12-* dep-token ops and D10-a/b/c (DOOM P4) DROPPED at 0f03f7e:
    # chunks A-D / P4 already landed -- user.mk dep rows no longer name
    # guishost.h / wsclient.h / qemu_windowsrv.h and ports/doom/e32_platform.c
    # is already fbblit-only (no guishost include, no blit_via_gcrect), so
    # those old strings occur 0 times and would REFUSE the apply.
]

# Delete ops are derived from DELETE_SET (phase P3, LAST).
DELETE_OPS = [
    {"id": "D%d" % (i + 1), "phase": "P3", "type": "delete", "path": p,
     "location": p, "note": note}
    for i, (p, note) in enumerate([
        ("user/winsrv.c", "includes guishost.h (:32)"),
        ("user/guishost.h", "also referenced by user/shot.c -- P1 must clear that first"),
        ("user/wsclient.c", "old exec backend"),
        ("user/wsclient.h", "ws_e32_client.c / ws_e32_draw_test.c includes re-pointed first"),
        ("variant/qemu_windowsrv.cpp", "WsExecInstall :215-258 -> exec count 274->271"),
        ("variant/qemu_windowsrv.h", "included by qemu_windowsrv.cpp + user.mk dep tokens"),
        ("variant/qemu_gui_exec.cpp", "execs 249/250/251 install + gui_publish_* + GuiGeometryReset"),
    ])
]

PHASES = ("P1", "P2", "P3")
PHASE_LABEL = {
    "P1": "kernel hooks + exec-table (edits; exec count 274->271 self-heals)",
    "P2": "build wiring (Makefile object rows + user.mk rows/dep tokens)",
    "P3": "source deletion LAST (DELETE_SET; DOOM P4 already landed at 0f03f7e), after P1+P2 gates",
}


def all_ops() -> list[dict]:
    ops = []
    for e in EDIT_OPS:
        o = dict(e)
        o["type"] = "edit"
        ops.append(o)
    ops.extend(DELETE_OPS)
    return ops


# ------------------------------------------------------- preconditions ----
P1_PATTERNS = ("QE_EXEC_GUI_", "QE_EXEC_WS_", r"guishost\.h", r"wsclient\.h",
               r"qemu_windowsrv\.h")
# P1: grep user/ + ports/ may match ONLY inside the deletion set (+ the DOOM
# consumer file edited in P3).  Everything else must be re-pointed first.
P1_ALLOWED = set(DELETE_SET) | {"ports/doom/e32_platform.c"}


def p1_consumers_clean() -> tuple[bool, list[str]]:
    """P1: consumer grep over user/ + ports/ (read-only)."""
    hits: list[str] = []
    for sub in ("user", "ports"):
        root = PORT / sub
        if not root.is_dir():
            continue
        for p in sorted(root.rglob("*")):
            if not p.is_file() or "build" in p.parts or p.suffix in (".o", ".elf", ".E32", ".png"):
                continue
            rel = p.relative_to(PORT).as_posix()
            if rel in P1_ALLOWED:
                continue
            try:
                text = p.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            for pat in P1_PATTERNS:
                if re.search(pat, text):
                    hits.append(f"{rel}: /{pat}/")
    return (not hits), hits


def p6_standing_invariants() -> tuple[bool, str]:
    """P6: EFILE stays guard-only (no active trampoline) + kernelhwsrv clean."""
    clean, detail = kernelhwsrv_clean()
    if not clean:
        return False, f"kernelhwsrv/ modified: {detail!r}"
    e32load = PORT / "variant" / "e32load.cpp"
    if e32load.is_file():
        for n, line in enumerate(e32load.read_text(encoding="utf-8",
                                                    errors="ignore").splitlines(), 1):
            s = line.lstrip()
            # HEAD=0f03f7e (11/11-blessed) carries the KPrintf string
            # "imports bound trampoline" at e32load.cpp:1014; strip string
            # literals so only real code references count as "active".
            code_only = re.sub(r'"(?:[^"\\]|\\.)*"', "", s)
            if "trampoline" in code_only.lower() and not s.startswith(("//", "*", "/*")):
                return False, f"EFILE trampoline active at variant/e32load.cpp:{n}"
    return True, "kernelhwsrv/ clean; EFILE guard-only (no active trampoline); " \
                 "never accept SessionPath r=-15; !E32WindowServer keeps ECapabilityProtServ"


def op_targets_clean(ops: list[dict], checks: list[dict] | None = None) -> tuple[bool, str]:
    """Refuse if any op target that will actually be WRITTEN is already
    modified (no stacking).  Ops that auto-skip (old-string already absent =
    applied by an earlier commit / a concurrent worker) are excluded: their
    files are never written by this run, so their working-tree state is not
    this run's concern."""
    skip_ids = {c["op"] for c in checks or [] if c.get("skip")}
    rels = sorted({o["path"] for o in ops if o["id"] not in skip_ids})
    return git_porcelain([str(Path("qemu-port") / r) for r in rels])


# ------------------------------------------------------------ plan/check --
def check_ops(ops: list[dict]) -> list[dict]:
    """Per-op verification.  Edits reuse the imported check_edits (exactly-one
    occurrence assertion); deletes are allowlist + safe_target + existence."""
    out = []
    edit_like = [{"path": o["path"], "location": o["location"],
                  "index": i, "old": o["old"]} for i, o in enumerate(ops)
                 if o["type"] == "edit"]
    edit_checks = {c["location"]: c for c in check_edits(edit_like)}
    for o in ops:
        info = {"op": o["id"], "phase": o["phase"], "type": o["type"],
                "path": o["path"], "location": o["location"]}
        if o["type"] == "edit":
            c = edit_checks[o["location"]]
            count = c.get("count", 0)
            if count == 0:
                # Already applied by an earlier commit (e.g. 0f03f7e): the
                # old-string is absent, so this op auto-skips -- NOT a failure.
                info.update(ok=True, count=0, skip=True,
                            reason="already applied (0 occurrences) -- auto-skip")
            else:
                info.update(ok=c["ok"], count=count, reason=c["reason"],
                            skip=False)
        else:
            p = PORT / o["path"]
            if o["path"] not in DELETE_SET:
                info.update(ok=False, count=0,
                            reason="REFUSED: not in the static DELETE_SET")
            elif not safe_target(o["path"]):
                info.update(ok=False, count=0,
                            reason="REFUSED: target outside qemu-port/ or under kernelhwsrv/")
            elif not p.is_file():
                info.update(ok=False, count=0, reason="delete target missing")
            else:
                info.update(ok=True, count=1, reason="in DELETE_SET, safe target, exists")
        out.append(info)
    return out


def print_plan(checks: list[dict], gate_status: str | None) -> None:
    print("r1_winsrv_delete -- DRY-RUN PLAN: NO file is changed; no QEMU booted.")
    if gate_status:
        print(f"gate status: {gate_status}")
    ops = all_ops()
    by_phase: dict[str, list[dict]] = {p: [] for p in PHASES}
    for c in checks:
        by_phase[c["phase"]].append(c)
    p1ok, p1hits = p1_consumers_clean()  # read-only consumer grep
    print(f"P1 consumer grep hits outside DELETE_SET: {len(p1hits)}"
          + ("" if p1ok else "  REFUSE: " + "; ".join(p1hits)))
    for ph in PHASES:
        n_apply = sum(1 for c in by_phase[ph] if c["ok"] and not c.get("skip"))
        n_skip = sum(1 for c in by_phase[ph] if c.get("skip"))
        print(f"\n=== Phase {ph} -- {PHASE_LABEL[ph]} ===  "
              f"{ph}={n_apply} op(s) to apply, {n_skip} already applied (auto-skip)")
        for c in by_phase[ph]:
            if c.get("skip"):
                tag = "applied"
            else:
                tag = "unique" if c["ok"] else "AMBIGUOUS"
            print(f"  [{tag}] {c['op']:>18}  {c['location']}  ({c['reason']})")
            if c["type"] == "edit":
                op = next(o for o in ops if o["id"] == c["op"])
                for line in op["old"].splitlines()[:3]:
                    print(f"      - {line}")
                if len(op["old"].splitlines()) > 3:
                    print(f"      - ... ({len(op['old'].splitlines())} lines total)")
                if op["new"]:
                    for line in op["new"].splitlines()[:3]:
                        print(f"      + {line}")
    print(f"\n{KEPT_NOTE}")
    print(f"\nconfirm token for --apply: {CONFIRM_TOKEN}")
    print("snapshot dir pattern: /tmp/opencode/" + BACKUP_PREFIX + "<ts>/  "
          "(manifest schema " + BACKUP_SCHEMA + ", rollback token " + ROLLBACK_PREFIX + "<ts>)")
    print("REFUSE: no --apply given -- nothing changed.")


# --------------------------------------------------------------- snapshot --
def make_backup(ops: list[dict], stamp: str) -> tuple[Path, dict]:
    """Snapshot EVERY op target (edited AND delete-listed) before any write;
    verify each copy's sha256 at backup time."""
    d = BACKUP_ROOT / f"{BACKUP_PREFIX}{stamp}"
    suffix = 0
    while d.exists():
        suffix += 1
        d = BACKUP_ROOT / f"{BACKUP_PREFIX}{stamp}_{suffix}"
    d.mkdir(parents=True, exist_ok=False)
    token = f"{ROLLBACK_PREFIX}{stamp}"
    manifest = {"schema": BACKUP_SCHEMA, "created": stamp, "dir": d.name,
                "gate": GATE_KEY, "confirm_token": CONFIRM_TOKEN,
                "rollback_token": token, "files": []}
    seen: set[str] = set()
    for o in ops:
        rel = o["path"]
        if rel in seen:
            continue
        seen.add(rel)
        if not safe_target(rel):
            raise RuntimeError(f"REFUSE: unsafe op target {rel!r}")
        src, dst = PORT / rel, d / rel
        if not src.is_file():
            raise RuntimeError(f"cannot snapshot missing file: {src}")
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        src_sum, dst_sum = sha256(src), sha256(dst)
        if src_sum != dst_sum:
            raise RuntimeError(f"snapshot verification FAILED for {rel}: "
                               f"src={src_sum} dst={dst_sum}")
        manifest["files"].append({"path": rel, "sha256": src_sum,
                                  "op": o["type"], "id": o["id"]})
    (d / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return d, manifest


# --------------------------------------------------------------- rollback --
def do_rollback(backup_dir: Path, confirm: str | None = None) -> int:
    """Restore a verified snapshot via staged temp + verified os.replace
    (recreates deleted files).  Reuses imported sha256 / safe_target."""
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
    entries = manifest.get("files", [])
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
        if not safe_target(rel) or Path(rel).is_absolute() or ".." in Path(rel).parts:
            print(f"REFUSE: manifest path {rel!r} is not a safe qemu-port target. "
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
        if sha256(src) != want:
            print(f"REFUSE: snapshot sha256 mismatch for {rel}; nothing restored.",
                  file=sys.stderr)
            return 2
        dst.parent.mkdir(parents=True, exist_ok=True)
        tmp = dst.parent / (dst.name + ".winsrv_delete_tmp")
        shutil.copy2(src, tmp)
        if sha256(tmp) != want:
            print(f"REFUSE: staged temp sha256 mismatch for {rel}; nothing restored.",
                  file=sys.stderr)
            return 2
        staged.append((tmp, want))

    for tmp, want in staged:
        dst = tmp.parent / tmp.name[: -len(".winsrv_delete_tmp")]
        os.replace(tmp, dst)
        if sha256(dst) != want:
            print(f"REFUSE: post-restore sha256 mismatch for {dst}; "
                  "rollback incomplete.", file=sys.stderr)
            return 2
        print(f"  restored {dst.relative_to(PORT)}")
    print(f"rollback complete: {len(staged)} file(s) restored from {backup_dir}")
    return 0


# ------------------------------------------------------------------ apply --
def run_cmd(argv: list[str], tag: str, timeout: int = 1800) -> subprocess.CompletedProcess:
    joined = " ".join(argv)
    if any(v in joined for v in FORBIDDEN_ARGV):
        raise RuntimeError(f"REFUSE: forbidden command {joined!r}")
    print(f"+ {tag}: {joined}", flush=True)
    return subprocess.run(argv, cwd=str(PORT), capture_output=True, text=True,
                          timeout=timeout, check=False)


def phase_gate(phase: str) -> str:
    """Incremental build (never `make clean`) + phase gate.  Returns 'PASS'
    only if every command exits 0 and the ewsrv retire gate reports PASS."""
    if phase == "P1":
        cmds = [["make"], [sys.executable, "tools/r1_os_selfcheck.py"]]
    elif phase == "P2":
        cmds = [["make", "isogrub"], [sys.executable, "tools/r1_os_selfcheck.py"]]
    else:
        cmds = [["make"], [sys.executable, "tools/r1_os_selfcheck.py"],
                [sys.executable, "tools/r1_qemu_smoke.py"],
                [sys.executable, "tools/r1_genuine_ewsrv_test.py"],
                [sys.executable, "tools/r1_genuine_input_test.py"],
                [sys.executable, "tools/r1_doom_test.py"],
                [sys.executable, "tools/r1_doom_audio_test.py"],
                [sys.executable, "tools/r1_doom_playtest.py"],
                [sys.executable, "tools/r1_doom_multikey_test.py"],
                [sys.executable, "tools/r1_genuine_fullchain_test.py"],
                [sys.executable, "tools/r1_genuine_retire_test.py"],
                [sys.executable, "tools/r1_final_verify.py", "--strict-skip"]]
    for argv in cmds:
        proc = run_cmd(argv, tag=f"gate[{phase}]")
        if proc.returncode != 0:
            tail = (proc.stdout + proc.stderr).strip().splitlines()[-5:]
            return "FAIL(" + " ".join(argv) + ") rc=%d: " % proc.returncode + " | ".join(tail)
    g = run_gate(GATE_KEY, dry_run=False, tag=f"{phase}_gate")
    return g["verdict"]


def apply_op(o: dict) -> None:
    """Apply one op; edits assert exactly-one occurrence right before writing."""
    p = PORT / o["path"]
    if not safe_target(o["path"]):
        raise RuntimeError(f"REFUSE: unsafe op target {o['path']!r}")
    if o["type"] == "edit":
        data = p.read_bytes()
        needle, repl = o["old"].encode("utf-8"), o["new"].encode("utf-8")
        cnt = data.count(needle)
        if cnt == 0:
            # Already applied (old-string absent): auto-skip, not a failure.
            print(f"  skip (already applied, 0 occurrences)  "
                  f"{o['location']} -> {o['path']}")
            return
        if cnt != 1:
            raise RuntimeError(f"ambiguity at apply time: {o['location']} "
                               f"(count={cnt})")
        p.write_bytes(data.replace(needle, repl, 1))
        print(f"  edited  {o['location']} -> {o['path']}")
    else:
        if o["path"] not in DELETE_SET:
            raise RuntimeError(f"REFUSE: delete not in DELETE_SET: {o['path']!r}")
        if not p.is_file():
            raise RuntimeError(f"delete target missing: {o['path']}")
        os.remove(p)  # allowlisted delete only; argv guard forbids rm -rf
        print(f"  deleted {o['path']}")


def apply_winsrv() -> int:
    ops = all_ops()
    checks = check_ops(ops)
    bad = [c for c in checks if not c["ok"]]
    if bad:
        for c in bad:
            print(f"REFUSE: op {c['op']} not verifiable: {c['location']}: {c['reason']}",
                  file=sys.stderr)
        return 2
    nskip = sum(1 for c in checks if c.get("skip"))
    print(f"  op set: {len(ops)} ops ({sum(1 for o in ops if o['type'] == 'edit')} edits, "
          f"{sum(1 for o in ops if o['type'] == 'delete')} deletes), "
          f"{nskip} already applied (auto-skip), rest unique/allowlisted")

    clean, detail = kernelhwsrv_clean()
    if not clean:
        print(f"REFUSE: kernelhwsrv/ is modified: {detail!r}", file=sys.stderr)
        return 2
    print(f"  kernelhwsrv/ clean: {detail}")

    oclean, odetail = op_targets_clean(ops, checks)
    if not oclean:
        print(f"REFUSE: op target(s) already modified: {odetail!r}. Nothing changed.",
              file=sys.stderr)
        return 2
    print(f"  op targets clean: {odetail}")

    p1ok, p1hits = p1_consumers_clean()
    if not p1ok:
        print("REFUSE: P1 consumers not grep-clean (chunks A-D must land first):",
              file=sys.stderr)
        for h in p1hits:
            print(f"    {h}", file=sys.stderr)
        return 2
    print("  P1 consumers grep-clean (matches only inside the deletion set)")
    p6ok, p6msg = p6_standing_invariants()
    if not p6ok:
        print(f"REFUSE: P6 standing invariant violated: {p6msg}", file=sys.stderr)
        return 2
    print(f"  P6: {p6msg}")

    pre = run_gate(GATE_KEY, dry_run=False, tag="winsrv_pre")
    if pre["verdict"] != "PASS":
        print(f"REFUSE: pre-apply gate {GATE_KEY} = {pre['verdict']} (PASS required). "
              "No file changed.", file=sys.stderr)
        return 3
    print(f"  pre-apply gate {GATE_KEY} = PASS")

    stamp = time.strftime("%Y%m%d_%H%M%S")
    backup, manifest = make_backup(ops, stamp)
    token = manifest["rollback_token"]
    print(f"  snapshot: {backup}  (rollback token: {token})")

    ok = False
    stage = "P1"
    rollback_rc = 0
    try:
        for phase in PHASES:
            stage = phase
            for o in ops:
                if o["phase"] == phase:
                    apply_op(o)
            verdict = phase_gate(phase)
            print(f"  phase {phase} gate: {verdict}")
            if verdict != "PASS":
                raise RuntimeError(f"phase {phase} gate = {verdict} (PASS required)")
        ok = True
    except Exception as exc:
        print(f"FAILURE during {stage}: {exc}; auto-rolling back snapshot.",
              file=sys.stderr)
    finally:
        if not ok:
            rollback_rc = do_rollback(backup, confirm=token)
            if rollback_rc != 0:
                print(f"CRITICAL: rollback FAILED; manual restore required from {backup}",
                      file=sys.stderr)
            else:
                try:
                    g = run_gate(GATE_KEY, dry_run=False, tag="post_rollback")
                    print(f"  gate after rollback: {g['verdict']}")
                except Exception as exc:
                    print(f"  gate after rollback unavailable: {exc}", file=sys.stderr)

    if ok:
        print(f"\nSUCCESS: winsrv deletion applied across P1-P3; all phase gates PASS.")
        print(f"  rollback snapshot: {backup}  (token {token})")
        print("  next (human-reviewed commit, NOT by this tool): record the retirement in")
        print("  docs/GENUINE_PROGRESS.md and docs/REMAINING_SHIMS_AUDIT.md (winsrv row -> retired).")
        print("  git fallback: git -C %s show <pre-gate-sha>:qemu-port/<path> > qemu-port/<path>" % REPO)
        return 0
    return 6 if rollback_rc != 0 else 4


# ------------------------------------------------------------------- main --
def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                  formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--apply", action="store_true",
                    help="actually change files (requires --confirm " + CONFIRM_PREFIX + "<token>)")
    ap.add_argument("--confirm", metavar="TOKEN",
                    help="exact confirm token (" + CONFIRM_TOKEN + ") or a snapshot's "
                         + ROLLBACK_PREFIX + "<ts> rollback token")
    ap.add_argument("--dry-run", action="store_true",
                    help="pure preview (default; also what no-args does, then refuses)")
    ap.add_argument("--rollback", metavar="DIR",
                    help="restore op targets from a backup dir (requires --confirm "
                         + ROLLBACK_PREFIX + "<ts>)")
    args = ap.parse_args()

    joined_argv = " ".join(sys.argv)
    if any(v in joined_argv for v in FORBIDDEN_ARGV):
        print("REFUSE: forbidden operation requested (commit/push/make clean/rm -rf).",
              file=sys.stderr)
        return 2

    if args.rollback:
        return do_rollback(Path(args.rollback), confirm=args.confirm)

    if args.apply:
        if not args.confirm:
            print(f"REFUSE: --apply requires --confirm {CONFIRM_TOKEN} "
                  "(DELETE-WINSRV-<token>). Nothing changed.", file=sys.stderr)
            return 2
        if args.confirm != CONFIRM_TOKEN or not args.confirm.startswith(CONFIRM_PREFIX):
            print(f"REFUSE: wrong --confirm token: {args.confirm!r} "
                  f"(expected {CONFIRM_TOKEN}). Nothing changed.", file=sys.stderr)
            return 2
        return apply_winsrv()

    # Preview (no-args or --dry-run): plan + read-only checks, then refuse.
    ops = all_ops()
    checks = check_ops(ops)
    gate_status = None
    if args.dry_run:
        g = run_gate(GATE_KEY, dry_run=True, tag="winsrv_preview")
        gate_status = f"{GATE_KEY} dry-run = {g['verdict']} (no QEMU booted)"
    print_plan(checks, gate_status)
    if not args.dry_run:
        print("REFUSE: no --apply -- plan only, nothing changed.", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
