#!/usr/bin/env python3
"""r1_shim_retire_plan.py -- AUDIT-ONLY inventory / dry-run planner for retiring
the hand-written qemu-port userspace shims (see docs/SHIM_RETIREMENT_PLAN.md).

HARD CONTRACT -- DRY RUN ONLY:
    This tool NEVER deletes, edits, creates, renames or stages any file. It
    never boots QEMU. It only:
      (a) reads the four verdicts of tools/r1_genuine_retire_test.py (either by
          invoking that gate with --dry-run, or by parsing a captured log with
          --from-file); and
      (b) reads (Path.is_file / Path.read_text) the shim files and boot-list
          lines it reports.
    There is no --apply, --delete, --edit or --commit mode, by design. The
    eventual switch is a reviewed, hand-applied step; this tool exists so that
    the change is a checklist, not a scramble.

READINESS SEMANTICS (deliberately conservative):
    READY   <=> the subsystem's retire gate reported PASS.
    BLOCKED <=  the gate reported SKIP or FAIL (including ANY dry-run, because
                a dry-run never executes the component in QEMU).
    Additional chain-level preconditions (B1/B3/B4/B5/B7) are printed as
    "extra conditions" but never downgrade or upgrade the gate-derived verdict;
    they are the human review checklist for the actual switch.

Usage:
    python3 tools/r1_shim_retire_plan.py                 # fresh gate --dry-run
    python3 tools/r1_shim_retire_plan.py --from-file LOG  # parse captured log
    python3 tools/r1_shim_retire_plan.py --json           # machine-readable
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

# ------------------------------------------------------------------ safety --
DRY_RUN_ONLY = True
assert DRY_RUN_ONLY, "r1_shim_retire_plan must remain read-only"
FORBIDDEN_VERBS = ("rm", "unlink", "delete", "mv", "rename", "git add",
                   "git commit", "git push", "qemu-system", "make clean")
GATE_NAME = "r1_genuine_retire_test.py"

TOOLS = Path(__file__).resolve().parent
PORT = TOOLS.parent
GATE = TOOLS / GATE_NAME


def assert_no_mutations(argv: tuple[str, ...] | None = None) -> None:
    """Fail loudly if this tool is ever changed into a mutating one."""
    assert DRY_RUN_ONLY
    if argv is not None:
        assert argv[1].endswith(GATE_NAME), "only the retire gate may be spawned"
        assert "--dry-run" in argv, "the gate must always be spawned with --dry-run"
        assert not any(v in a for a in argv for v in FORBIDDEN_VERBS), argv
    for path in (GATE, PORT / "variant/e32load.cpp", PORT / "user/svcmgr.cpp"):
        assert path.is_file(), f"missing expected read-only input: {path}"


# --------------------------------------------------------------- inventory --
# Gate key -> the name the gate prints in "SUBSYSTEM <name>: <VERDICT>".
GATE_LABELS = {
    "efsrv": "EFSRV/F32",
    "ewsrv": "EWSRV",
    "estart": "ESTART/ESHELL",
    "euser": "EUSER",
}
SUBSYSTEM_RE = re.compile(r"SUBSYSTEM\s+(\S+):\s+(PASS|FAIL|SKIP)")

# Four shims still in the production path. "gate" is the gate that proves
# readiness; "files" are the exact paths removed/bypassed at the switch.
SHIMS = [
    {
        "name": "portfilesrv",
        "gate": "efsrv",
        "provides": "Ring-3 owner of the hand-rolled \"!FileServer\" name; "
                    "injected in-memory FatBackend behind CServer2/CSession2.",
        "replacement": "genuine efile.exe + efat32.fsy + efsrv.dll "
                       "(kernelhwsrv/userlibandfileserver/fileserver/)",
        "files": ["user/portfilesrv.cpp", "tools/mke32_portfilesrv.py",
                  "services/r1_service_host.cpp", "services/r1_service_host.h",
                  ":portfilesrv"],
        "call_sites": ["variant/e32load.cpp: SvcMgrBootLaunch() reads \\SVCMGR.E32; "
                       "svcmgr kServices[] launches \\PORTFILESRV.E32",
                       "tools/r1_genuine_retire_test.py: stages the prebuilt "
                       "portfilesrv flat binary for the EWSRV gate"],
        "extra_conditions": ["B1: !-name registration + ECapabilityProtServ",
                             "B5: ring-3 efat32.fsy PSL adapter behind a device handle",
                             "doom still loads its WAD from FAT32; fcopy/largefile tests"],
    },
    {
        "name": "r1_fs_* (core/server/client)",
        "gate": "efsrv",
        "provides": "Private R1Fs wire opcodes, CServer2 server, client channel "
                    "and in-memory FAT backend; kernel-side FAT glue.",
        "replacement": "genuine RFs/RFile client ordinals (efsrv.dll) and the "
                       "real TFsMessage server (efile.exe + efat32.fsy)",
        "files": ["fs/r1_fs_core.cpp", "fs/r1_fs_core.h", "fs/r1_fs_core.mmp",
                  "fs/r1_fs_backend.h", "fs/r1_fs_server.cpp", "fs/r1_fs_server.h",
                  "fs/r1_fs_fat.cpp", "fs/r1_fs_fat.h", "fs/r1_fs_channel.cpp",
                  "fs/r1_fs_channel.h", "fs/r1_fs_channel_eka.cpp",
                  "fs/r1_fs_channel_eka.h", "fs/r1_fs_channel_state.h",
                  "fs/r1_fs_wire_logic.h", "variant/r1_fs_channel.cpp",
                  "variant/r1_fs_channel.h", "variant/fat32.cpp"],
        "call_sites": ["user/svcmgr.cpp: includes r1_fs_core.h + r1_fs_server.h; "
                       "FatBackend : R1Fs::Backend; R1Fs::FileServer::NewL",
                       "Makefile:326-331: build/r1_fs_core.o + build/r1_fs_fat.o "
                       "in KERNFULL_OBJS", "tests/r1_fs_core/core_test.cpp "
                       "(keep as a unit-test seam, not production)",
                       "genuine/efsrv_min/efsrv_min.cpp (probe host only)"],
        "extra_conditions": ["B3: const-descriptor rejection over the IPC bridge",
                             "B5: ring-3 FAT adapter replaces variant/fat32.cpp",
                             "13/13 opcode check must remain PASS (ABI seam)"],
    },
    {
        "name": "winsrv",
        "gate": "ewsrv",
        "provides": "Hand-written PE2 compositor and display-list transport "
                    "(execs 249/250/251 + kernel event ring 271/272/273).",
        "replacement": "genuine ewsrv.exe (ws_main.cpp) plus a ported edisp LDD/PSL",
        "files": ["user/winsrv.c", "user/guishost.h", "variant/qemu_windowsrv.cpp",
                  "variant/qemu_windowsrv.h", "variant/qemu_gui_exec.cpp"],
        "call_sites": ["user/{desktop,guishell,guihello,guimenu,guilib,breakout,"
                       "saver,shot,dtcalc,dtedit,gcblittest,fbblittest}.c and "
                       "user/wsclient.h include guishost.h / qemu_windowsrv.h",
                       "ports/doom/e32_platform.c: guishost.h GC_RECT fallback",
                       "variant/qemu_variant.h:112,250: gui_init/window-server hook",
                       "Makefile:258,261: qemu_gui_exec.o + qemu_windowsrv.o"],
        "extra_conditions": ["B1: register !E32WindowServer + client sessions",
                             "port edisp LDD to the QEMU framebuffer",
                             "e32test/window + window_system/winsrv/desktop tests",
                             "doom graphical frame byte-for-byte unchanged"],
    },
    {
        "name": "svcmgr",
        "gate": "estart",
        "provides": "Ordered service startup/supervision/shutdown; repurposed "
                    "slow execs 268/269/270 (launch/kill/reap) and boot launch.",
        "replacement": "genuine e32strt.exe (estart) + domainsrv.exe + eshell.exe "
                       "launched from the boot image",
        "files": ["user/svcmgr.cpp", "variant/e32load.cpp (SvcMgrExecInstall, "
                  "SvcMgrLaunchExec, SvcMgrKillExec, SvcMgrCtlExec, "
                  "SvcMgrBootLaunch; execs 268-270)",
                  "variant/qemu_variant_asic.cpp (SvcMgrExecInstall call)"],
        "call_sites": ["variant/e32load.cpp:1217-1229 SvcMgrBootLaunch() reads "
                       "\\SVCMGR.E32 -> E32Run",
                       "variant/e32load.cpp:1304 fallback from GenuineBootLaunch()",
                       "variant/qemu_variant_asic.cpp:194-196 boot hook order",
                       "variant/e32load.cpp:22 qemu_fbblit.h appended to exec 270"],
        "extra_conditions": ["B4: svcmgr boot-launch faults in kheap.cpp",
                             "B7: G-01 GenuineBootLaunch fault KERN 0x62 during "
                             "EUSER import binding", "domainpolicy.dll shutdown path"],
    },
]

# Exact boot-list lines (variant/e32load.cpp, kGenuineBootList, lines 1250-1258)
# that are bypassed / rewritten when each shim retires. These are *descriptions
# of the future edit*, never applied here.
BOOT_LIST_LINES = {
    "efsrv": [
        ('variant/e32load.cpp:1252-1253',
         '    { "\\\\EFILE.E32",   ETrue,  EFalse },  // genuine !FileServer host (sfile)\n'
         '    { "\\\\EFSRV.E32",   ETrue,  EFalse },  // alternate file-server name'),
        ('variant/e32load.cpp:1304',
         '    SvcMgrBootLaunch();  // fallback: our svcmgr starts \\PORTFILESRV.E32'),
        ('user/svcmgr.cpp:250',
         '    { "\\\\PORTFILESRV.E32", 0, "!FileServer", 0 },  /* F32 / R1 file server */'),
    ],
    "ewsrv": [
        ('variant/e32load.cpp:1256',
         '    { "\\\\EWSRV.E32",   EFalse, ETrue  },  // genuine window server (ws_main.cpp)'),
        ('variant/e32load.cpp:1241',
         '    // without EWSRV/ESHELL keep the console shell + GUI execs.'),
        ('variant/qemu_variant_asic.cpp:194-196',
         '    SvcMgrExecInstall();\n    GenuineBootSpawn();\n'
         '    ShellSpawnFull();         // interactive console as genuine NThread'),
    ],
    "estart": [
        ('variant/e32load.cpp:1254-1255',
         '    { "\\\\ESTART.E32",  EFalse, EFalse },  // genuine estart (estartmain.cpp)\n'
         '    { "\\\\E32STRT.E32", EFalse, EFalse },  // alternate estart name'),
        ('variant/e32load.cpp:1257',
         '    { "\\\\ESHELL.E32",  EFalse, EFalse },  // genuine shell (etshell ts_main.cpp)'),
        ('variant/e32load.cpp:1200,1213',
         'void SvcMgrExecInstall()\n'
         '    { ... KPrintf("SvcMgr: launch execs installed (268/269/270)"); }'),
        ('variant/qemu_variant_asic.cpp:194', '    SvcMgrExecInstall();'),
    ],
}

# The EUSER gate proves a DIFFERENT retirement (the static euser_shim, plan
# step 1). It is reported as context so no gate is silently ignored.
EUSER_CONTEXT = ("EUSER: the DLL is already genuine; its gate governs removal of "
                 "user/euser_shim.cpp / static e32base.h linkage, not the four "
                 "shims in this plan. No shim is retired by an EUSER PASS.")


# ------------------------------------------------------------------ status --
def parse_status(text: str) -> dict[str, dict]:
    """Parse the gate's stdout into {key: {verdict, label, detail}}."""
    labels = {v.lower(): k for k, v in GATE_LABELS.items()}
    out: dict[str, dict] = {}
    block: list[str] = []
    current = None
    for line in text.splitlines():
        if line.startswith("== ["):
            current = line.strip().strip("=[] ")
            block = []
        m = SUBSYSTEM_RE.search(line)
        if m:
            label, verdict = m.group(1), m.group(2)
            key = labels.get(label.lower(), label.lower())
            out[key] = {
                "verdict": verdict,
                "label": label,
                "detail": [b.strip() for b in block if b.strip()],
            }
        if current:
            block.append(line)
    return out


def gate_status(from_file: str | None) -> tuple[dict[str, dict], str]:
    """Read gate verdicts without ever booting QEMU."""
    assert_no_mutations()
    if from_file:
        path = Path(from_file)
        assert path.is_file(), f"captured gate log not found: {path}"
        return parse_status(path.read_text(errors="replace")), f"captured log: {path}"
    argv = (sys.executable, str(GATE), "--dry-run")
    assert_no_mutations(argv)
    proc = subprocess.run(argv, cwd=str(PORT), capture_output=True,
                          text=True, timeout=180, check=False)
    return parse_status(proc.stdout + proc.stderr), "live gate --dry-run (no QEMU)"


def file_state(rel: str) -> dict:
    # Some entries name a file plus the symbol region inside it, e.g.
    # "variant/e32load.cpp (SvcMgrExecInstall, ...)"; only the path is probed.
    path, _, annotation = rel.partition(" (")
    p = PORT / path.strip()
    return {"path": rel, "present": p.is_file(),
            "note": ("symbol region to delete from this file: " + annotation.rstrip(")")
                     if annotation else "whole file / path to remove or bypass")}


def boot_change(location: str, text: str) -> dict:
    """Verify the quoted boot-list line is still present; never edit it."""
    file_part = location.split(":")[0]
    needle = text.splitlines()[0].strip()
    still_there = None
    src = PORT / file_part
    if src.is_file() and needle:
        still_there = needle in src.read_text(errors="replace")
    return {"location": location, "line": text, "still_present": still_there,
            "action": "REVIEW AND EDIT BY HAND -- this tool does not apply it"}


def build_plan(status: dict[str, dict], source: str) -> dict:
    shims = []
    for shim in SHIMS:
        gate = status.get(shim["gate"], {})
        verdict = gate.get("verdict", "UNKNOWN")
        ready = verdict == "PASS"
        reasons = [f"gate {GATE_LABELS[shim['gate']]} = {verdict}"]
        if verdict == "SKIP":
            reasons.append("SKIP never proves readiness (component not built or "
                           "not executed; a --dry-run always SKIPs runtime)")
            reasons += [d for d in gate.get("detail", []) if d.startswith("BUILT:")]
        elif verdict == "FAIL":
            reasons.append("FAIL: guest Exc/Panic/FAULT or missing markers "
                           "(see GENUINE_PROGRESS.md blocker B7)")
        shims.append({
            "shim": shim["name"],
            "status": "READY" if ready else "BLOCKED",
            "gate": GATE_LABELS[shim["gate"]],
            "gate_verdict": verdict,
            "provides": shim["provides"],
            "replacement": shim["replacement"],
            "reasons": reasons,
            "files_that_would_change": [file_state(f) for f in shim["files"]],
            "boot_list_changes": [boot_change(loc, txt)
                                  for loc, txt in BOOT_LIST_LINES[shim["gate"]]],
            "extra_conditions": shim["extra_conditions"],
            "call_sites": shim["call_sites"],
        })
    return {
        "schema": "r1-shim-retire-plan/v1",
        "source_of_gate_status": source,
        "dry_run_only": True,
        "mutation_guarantee": "no delete / edit / stage / QEMU; read-only inspection",
        "euser_context": EUSER_CONTEXT,
        "euser_gate": status.get("euser", {}).get("verdict", "UNKNOWN"),
        "shims": shims,
        "summary": {
            "ready": sum(s["status"] == "READY" for s in shims),
            "blocked": sum(s["status"] == "BLOCKED" for s in shims),
        },
        "rollback": "everything is archived at git tag/branch archive/handwritten-r25 (56fbaff)",
    }


def print_text(plan: dict) -> None:
    print("r1_shim_retire_plan -- DRY-RUN ONLY: no file is deleted, edited or "
          "staged; no QEMU is booted.")
    print(f"gate status source: {plan['source_of_gate_status']}")
    for s in plan["shims"]:
        print(f"\n=== {s['shim']}: {s['status']}  (gate {s['gate']} = "
              f"{s['gate_verdict']}) ===")
        for r in s["reasons"]:
            print(f"  reason: {r}")
        print(f"  provides:    {s['provides']}")
        print(f"  replacement: {s['replacement']}")
        print("  files that would be removed/bypassed (NOT touched):")
        for f in s["files_that_would_change"]:
            print(f"    [{'present' if f['present'] else 'MISSING'}] {f['path']}")
        print("  boot-list lines that would change (NOT applied):")
        for b in s["boot_list_changes"]:
            tag = {True: "present", False: "CHANGED?", None: "n/a"}[b["still_present"]]
            print(f"    [{tag}] {b['location']}:")
            for ln in b["line"].splitlines():
                print(f"        {ln}")
        print("  extra chain-level conditions before the switch:")
        for e in s["extra_conditions"]:
            print(f"    - {e}")
        print("  dependents / call sites:")
        for c in s["call_sites"]:
            print(f"    - {c}")
    print(f"\n{plan['euser_context']}")
    print(f"EUSER gate this run: {plan['euser_gate']}")
    sm = plan["summary"]
    print(f"\nSUMMARY: {sm['ready']} READY, {sm['blocked']} BLOCKED. "
          f"Rollback: {plan['rollback']}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--from-file", metavar="LOG",
                    help="parse a captured r1_genuine_retire_test.py output file "
                         "instead of invoking the gate (still no QEMU)")
    ap.add_argument("--json", action="store_true", help="emit structured JSON")
    args = ap.parse_args()
    assert_no_mutations()
    status, source = gate_status(args.from_file)
    plan = build_plan(status, source)
    if args.json:
        print(json.dumps(plan, indent=2, sort_keys=False))
    else:
        print_text(plan)
    return 0  # a planner is never a gate


if __name__ == "__main__":
    sys.exit(main())
