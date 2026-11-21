#!/usr/bin/env python3
"""r1_ported_software_test.py -- consolidated QEMU test for the real software ports.

One test for the freestanding ports documented in docs/PORTED_SOFTWARE.md:
LUA (Lua 5.4.6), ZMACHINE (Mini-Zork / Z-machine interpreter), ZIP32
(miniz inflate / zip reader) and BC32 (bc-style desktop calculator).

Rather than re-inventing a fragile GRUB-ISO drive (the 75 MB ISO spends ~10 s
sitting in the GRUB menu and the generic launch names/markers never matched the
real ports, so the whole run was killed by the harness timeout with an empty
log), this harness reuses the *proven* sequences of the four component tests.
For every present port it calls the component module's build_disk(), run_qemu()
and check() directly: each port gets its own QEMU session, its own FAT32 disk,
the exact launch command, prompt-synced keystrokes, real markers, the genuine
EUSER ordinal 915 bind check and the guest-exception check.  Missing images are
SKIPPED.  One serial log per port is written under /tmp/opencode/.

Exit 0 means every available port passed (or all four were skipped).
Usage: python3 tools/r1_ported_software_test.py [--no-rebuild-iso] [--log-dir DIR]
"""
import argparse
import importlib
import os
import shutil
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.dirname(os.path.abspath(__file__))
if TOOLS not in sys.path:
    sys.path.insert(0, TOOLS)
ROOT = "/tmp/opencode"
LOG_DIR = ROOT

# display name -> (component module, primary E32 probed in build/user)
PORTS = (
    ("lua", "r1_lua_test", "LUA.E32"),
    ("zmachine", "r1_zmachine_test", "ZMACHINE.E32"),
    ("zip32", "r1_zip_test", "ZIP32.E32"),
    ("bc", "r1_bc32_test", "BC32.E32"),
)

# r1_lua_test inlines its assertions in main() instead of exposing check(), so
# mirror them here (same markers, order, exec/bind and exception checks).
LUA_MARKERS = (
    "LUA: LUA 5.4.6 READY", "LUA: fib(10)=55", "LUA: table t[2]=20",
    "LUA: T1=5", "LUA: LOOP=1", "LUA: LOOP=2", "LUA: LOOP=3",
    "LUA: T3=2", "LUA: T4=14", "LUA: DONE",
)


def check_lua(text):
    ok = True
    offsets = [text.find(m) for m in LUA_MARKERS]
    for marker, pos in zip(LUA_MARKERS, offsets):
        good = pos >= 0
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {marker}")
    ordered = (all(p >= 0 for p in offsets)
               and all(offsets[i] < offsets[i + 1]
                       for i in range(len(offsets) - 1)))
    ok &= ordered
    print(f"  [{'PASS' if ordered else 'FAIL'}] marker order")
    for label, good in (
        ("LUABOOT service exec 268", "LUABOOT: launched LUA.E32" in text),
        ("EUSER ord 915 bound", any("ord 915" in ln and "bind" in ln
                                    for ln in text.splitlines())),
    ):
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {label}")
    exc = [ln for ln in text.splitlines()
           if any(k in ln for k in ("Exc ", "PanicKernExec", "FAULT:"))]
    ok &= not exc
    print(f"  [{'PASS' if not exc else 'FAIL'}] no Exc/PanicKernExec/FAULT")
    for ln in exc[:5]:
        print("      ", ln[:200])
    return ok


def check_tools():
    for tool in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir", "python3"):
        if not shutil.which(tool):
            sys.exit(f"required tool missing: {tool}")


def disk_image(mod):
    """The component module's prepared FAT disk image, whatever it is named."""
    return getattr(mod, "DISK", None) or getattr(mod, "DISK_IMG", None)


def run_port(name, modname, e32, log_dir, rebuild):
    """Drive one port using its component test's proven disk/QEMU/check logic.

    Returns True (pass), False (fail) or None (skipped because images absent).
    """
    e32_path = os.path.join(REPO, "build/user", e32)
    if not os.path.isfile(e32_path):
        print(f"[SKIP] {e32} not built yet ({e32_path})")
        return None
    try:
        mod = importlib.import_module(modname)
    except Exception as exc:
        print(f"[FAIL] {name}: cannot import {modname}: {exc}")
        return False

    img = disk_image(mod)
    if not rebuild and img and os.path.exists(img):
        print(f"[INFO] {name}: reusing existing disk image {img}")
    else:
        try:
            mod.build_disk()
        except SystemExit as exc:              # component test calls sys.exit()
            print(f"[SKIP] {name}: missing staging image ({exc})")
            return None
        except Exception as exc:
            print(f"[FAIL] {name}: build_disk() failed: {exc}")
            return False

    log_path = f"{log_dir}/r1_ported_{name}.log"
    error = None
    print(f"\n== booting QEMU, driving {name} via its proven feed sequence "
          f"(serial log: {log_path})")
    try:
        mod.run_qemu(log_path)
    except Exception as exc:                   # RuntimeError on a missing marker
        error = exc
        print(f"[WARN] {name}: QEMU drive raised {exc!r}; validating log anyway")

    if not os.path.exists(log_path):
        print(f"[FAIL] {name}: no serial log produced at {log_path}")
        return False
    with open(log_path, errors="replace") as f:
        text = f.read()
    print(f"== {name} results")
    checker = getattr(mod, "check", None) or check_lua  # lua inlines its checks
    try:
        ok = bool(checker(text))
    except Exception as exc:
        print(f"[FAIL] {name}: check() failed: {exc}")
        return False
    if name == "zip32":                        # component test's host gzip check
        try:
            ok = bool(mod.verify_gzip_on_host()) and ok
        except Exception as exc:
            print(f"[FAIL] {name}: gzip interoperability check failed: {exc}")
            ok = False
    if error is not None:
        ok = False
        print(f"[FAIL] {name}: drive error: {error}")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-iso", action="store_true",
                    help="reuse already-staged per-port disk images")
    ap.add_argument("--log-dir", default=LOG_DIR)
    args = ap.parse_args()
    check_tools()
    os.makedirs(args.log_dir, exist_ok=True)

    results = {}
    for name, modname, e32 in PORTS:
        results[name] = run_port(name, modname, e32, args.log_dir,
                                 rebuild=not args.no_rebuild_iso)

    passed = [n for n, r in results.items() if r is True]
    failed = [n for n, r in results.items() if r is False]
    skipped = [n for n, r in results.items() if r is None]
    print("\n" + "=" * 60)
    print(f"ports passed : {', '.join(passed) or 'none'}")
    print(f"ports failed : {', '.join(failed) or 'none'}")
    print(f"ports skipped: {', '.join(skipped) or 'none'}")
    if failed:
        print("RESULT: FAIL")
        return 1
    if not passed:
        print("RESULT: SKIP (no ported software E32s built yet)")
        return 0
    print(f"RESULT: PASS ({len(passed)} port(s): {', '.join(passed)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
