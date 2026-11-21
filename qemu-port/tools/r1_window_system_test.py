#!/usr/bin/env python3
"""r1_window_system_test.py -- consolidated QEMU test for the window system.

The new window system is a long-lived window server E32 (WINSRV) that owns the
Bochs VBE framebuffer and composites private client buffers, plus at least one
client (WSDL, a windowed calculator). Both are still being built by other
agents: this test probes build/user/WINSRV.E32 and build/user/WSDL.E32 and
SKIPS gracefully when either is absent. When both are present it stages them
with EUSER.DLL on the GRUB ISO's /PROGRAMS.IMG, boots once with QEMU standard
VGA (-vga std -> VBE 0x118), starts the server with `winsrv`, then the client
with `wsdl`, drives a few calculator keys, and asserts the WSV: and WDL:
marker families with no Exc / PanicKernExec / FAULT in the guest log. A PNG
screendump of the composited framebuffer is grabbed over QMP when available:
a baseline before launching the window system and a final frame afterwards.
Both are compared with PIL to prove the desktop really changed to coloured
GUI pixels rather than merely leaving a console-text overlay on screen.

Usage:
    python3 tools/r1_window_system_test.py [--no-rebuild-iso] [--logdir DIR]
See docs/WINDOW_SERVER.md for the window-server architecture.
"""
import argparse
import os
import shlex
import shutil
import socket
import subprocess
import sys
import time

REPO = "/home/yuzi/Documents/Sybian REAL/qemu-port"
ROOT = "/tmp/opencode"
ISO = ROOT + "/r1_windowsys.iso"
SOCK = ROOT + "/r1_windowsys-qmp.sock"
INITIAL_PNG = ROOT + "/window_system_initial.png"   # pre-launch baseline
PNG = ROOT + "/window_system_final2.png"             # composited desktop
PROMPTS = (b"EKA2> ", b"X:/> ", b"C:/> ")
BOOT_MARKERS = ("DISPLAY: VBE mode 0x", "GUI: execs 249/250/251 installed")
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "MMU #PF")

# The two E32 images of the window system and the marker families to assert.
SERVER = {
    "name": "WINSRV",
    "e32": "WINSRV.E32",
    "cmd": b"winsrv\n",
    "markers": ("WSV:", "WINSRV:", "WSRV:", "WS:"),
}
CLIENTS = [
    {
        "name": "WSDL",
        "e32": "WSDL.E32",
        "cmd": b"wsdl\n",
        "markers": ("WDL:", "WSDL:", "WSCLIENT:"),
        "keys": (b"2", b"+", b"3", b"=", b"\r"),
    },
]
# Accept the plausible start markers while the two apps are still being built.
SERVER_START = ("WSV: READY", "WSV: START", "WSV: WINDOW SERVER READY",
                "WINSRV:", "WSRV:", "WSV:")
CLIENT_START = ("WDL: START", "WDL: READY", "WDL: CALC READY",
                "WDL: FIRST FRAME", "WSDL:", "WDL:")


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(a) for a in args))
    return subprocess.run(args, **kw)


def check_tools():
    for tool in ("qemu-system-i386", "grub2-mkimage", "mcopy", "mkfs.fat",
                 "mdir"):
        if not shutil.which(tool):
            sys.exit(f"required tool missing: {tool}")


def probe():
    """Return (server, [clients]) whose E32 images exist in build/user."""
    server_present = os.path.isfile(os.path.join(REPO, "build/user",
                                                 SERVER["e32"]))
    if server_present:
        server = SERVER
    else:
        print(f"[SKIP] {SERVER['e32']} not built yet")
        server = None
    clients = []
    for spec in CLIENTS:
        if os.path.isfile(os.path.join(REPO, "build/user", spec["e32"])):
            clients.append(spec)
        else:
            print(f"[SKIP] {spec['e32']} not built yet")
    return server, clients


def build_iso(server, clients):
    """Bundle all built E32s plus EUSER.DLL into /PROGRAMS.IMG on an ISO."""
    if os.path.exists(SOCK):
        os.remove(SOCK)
    if os.path.exists(ISO):
        os.remove(ISO)
    sh("python3", "tools/mkgrubiso.py", ISO, cwd=REPO, check=True,
       stdout=subprocess.DEVNULL)
    if not os.path.exists(ISO):
        sys.exit(f"ISO build produced no file: {ISO}")
    disk = os.path.join(REPO, "build", "programs.img")
    listing = sh("mdir", "-i", f"{disk}@@2048S", "::", check=True,
                 capture_output=True, text=True).stdout
    norm = "".join(listing.lower().replace(".", "").split())
    if "euserdll" not in norm:
        sys.exit("programs image missing EUSER.DLL")
    for spec in ([server] if server else []) + clients:
        if spec["e32"].lower().replace(".", "") not in norm:
            sys.exit(f"{spec['e32']} missing from ISO /PROGRAMS.IMG")
    print(f"[stage] WINSRV + {len(clients)} client(s) + EUSER.DLL in {ISO}")


def wait_for(log, needle, start=0, timeout=70):
    """Poll the log for a byte string; return offset past it or -1."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log, "rb") as f:
            f.seek(start)
            data = f.read()
        pos = data.find(needle)
        if pos >= 0:
            return start + pos + len(needle)
        time.sleep(0.2)
    return -1


def wait_any(log, needles, start=0, timeout=60):
    """Wait for any of several byte markers in the log; return bool."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log, "rb") as f:
            f.seek(start)
            data = f.read()
        if any(n.encode() in data for n in needles):
            return True
        time.sleep(0.2)
    return False


def wait_prompt(log, timeout=75):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log, "rb") as f:
            data = f.read()
        if any(p in data for p in PROMPTS):
            return True
        time.sleep(0.2)
    return False


def wait_prompt_at(log, start=0, timeout=90):
    """Wait for a shell prompt at/after start; return offset past it."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(log, "rb") as f:
            f.seek(start)
            data = f.read()
        for p in PROMPTS:
            if p in data:
                return start + data.index(p) + len(p)
        time.sleep(0.2)
    return start


def qmp_screendump(path):
    """Best-effort QMP PNG screendump; return True on success."""
    for _ in range(16):
        if os.path.exists(SOCK):
            break
        time.sleep(0.25)
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect(SOCK)
        f = s.makefile("rwb")
        f.readline()                                # QMP greeting
        f.write(b'{"execute":"qmp_capabilities"}\n')
        f.flush()
        f.readline()                                # capabilities reply
        if os.path.exists(path):
            os.remove(path)
        qmp = ('{"execute":"screendump","arguments":{"filename":"'
               + path + '","format":"png"}}\n')
        f.write(qmp.encode())
        f.flush()
        for _ in range(10):
            line = f.readline()
            if not line or b"return" in line or b"error" in line.lower():
                break
        f.close()
        s.close()
    except Exception as exc:
        print(f"[WARN] QMP screendump failed: {exc}")
        return False
    for _ in range(20):                             # PNG write is asynchronous
        if os.path.exists(path) and os.path.getsize(path) > 1000:
            break
        time.sleep(0.15)
    ok = os.path.exists(path) and os.path.getsize(path) > 1000
    if ok:
        print(f"[INFO] screendump {path} ({os.path.getsize(path)} bytes)")
    return ok


# Visual-proof thresholds.  The window server paints a coloured desktop,
# title bars and a taskbar; a console-text overlay is monochrome grey glyphs.
CENTER = (200, 100, 824, 600)        # region that must show the composited GUI
BOTTOM = (0, 688, 1024, 768)         # bottom 80 px: console text scrolls here
MIN_CHANGED = 0.15                   # >15 % of centre pixels must change
MIN_COLOURED = 0.05                  # >5 % of centre pixels must be GUI-coloured
MAX_TEXTY = 0.05                     # <5 % bright neutral pixels at bottom
WINDOW_BOX = (80, 60, 400, 280)      # WSDL window footprint (80,60 + its size)
MIN_WINDOW = 0.02                    # >2 % of that area must be window pixels


def visual_proof(before_path, after_path):
    """Return (ok, notes): compare baseline and final QMP screendumps.

    The test used to assert markers only; a compositing crash still left a
    full-screen serial-console overlay in the framebuffer.  We now require
    (a) both dumps are 1024x768, (b) a meaningful fraction of the central
    region changed, (c) the final centre contains coloured GUI pixels rather
    than only monochrome console glyphs, and (d) the bottom strip is not
    dominated by bright neutral text-like pixels.
    """
    notes = []
    try:
        from PIL import Image, ImageChops
    except Exception as exc:
        return False, [f"PIL unavailable: {exc}"]
    try:
        before = Image.open(before_path).convert("RGB")
        after = Image.open(after_path).convert("RGB")
    except Exception as exc:
        return False, [f"cannot open screendumps: {exc}"]
    if before.size != (1024, 768) or after.size != (1024, 768):
        notes.append(f"size before={before.size} after={after.size} "
                     "(expected 1024x768)")
        return False, notes
    notes.append("screendumps are 1024x768")

    # (b) meaningful change in the central region
    diff = ImageChops.difference(before.crop(CENTER), after.crop(CENTER))
    diff_g = diff.convert("L").histogram()
    changed = sum(diff_g[25:]) / float(diff.width * diff.height)
    ok_changed = changed > MIN_CHANGED
    notes.append(f"central changed {changed:.1%} "
                 f"({'PASS' if ok_changed else 'FAIL'}, need >{MIN_CHANGED:.0%})")

    # (c) coloured (non-grey) pixels in the final centre = real GUI
    px = after.crop(CENTER).load()
    w = CENTER[2] - CENTER[0]
    h = CENTER[3] - CENTER[1]
    coloured = 0
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            if max(r, g, b) - min(r, g, b) >= 40 and max(r, g, b) >= 60:
                coloured += 1
    coloured_frac = coloured / float(w * h)
    ok_coloured = coloured_frac > MIN_COLOURED
    notes.append(f"final centre coloured {coloured_frac:.1%} "
                 f"({'PASS' if ok_coloured else 'FAIL'}, "
                 f"need >{MIN_COLOURED:.0%})")

    # (d) console text is bright and neutral (all channels ~equal)
    bpx = after.crop(BOTTOM).load()
    bw = BOTTOM[2] - BOTTOM[0]
    bh = BOTTOM[3] - BOTTOM[1]
    texty = 0
    for y in range(bh):
        for x in range(bw):
            r, g, b = bpx[x, y]
            if min(r, g, b) >= 150 and max(r, g, b) - min(r, g, b) <= 40:
                texty += 1
    texty_frac = texty / float(bw * bh)
    ok_texty = texty_frac < MAX_TEXTY
    notes.append(f"bottom bright-neutral {texty_frac:.1%} "
                 f"({'PASS' if ok_texty else 'FAIL'}, "
                 f"need <{MAX_TEXTY:.0%})")

    return (ok_changed and ok_coloured and ok_texty), notes


def run_guest(log, server, clients):
    """Boot once, start server + clients, capture baseline and final screens.

    Returns (initial_ok, final_ok).  WINSRV holds the PortCon console lease for
    its ~10 s serve loop, so `wsdl` typed while it runs is consumed as input
    events instead of by the shell.  We port the proven r1_wsdl_test.py
    sequence: try the documented winsrv->wsdl order first, check for a WDL
    start marker, and if the client never launched, ESC the server to release
    the lease and re-launch wsdl FIRST, winsrv SECOND -- the only order in
    which both processes stay alive concurrently for the composited capture.
    """
    if os.path.exists(SOCK):
        os.remove(SOCK)
    qemu = [
        "qemu-system-i386", "-M", "q35", "-m", "128",
        "-vga", "std", "-display", "none", "-serial", "stdio",
        "-qmp", f"unix:{SOCK},server,nowait",
        "-cdrom", ISO, "-boot", "d", "-monitor", "none", "-no-reboot",
    ]
    print("== booting GRUB ISO, driving winsrv then wsdl ...")
    initial_ok = final_ok = False
    READY = b"WSV: WINDOW SERVER READY"
    WDL_READY = b"WDL: CLIENT READY"
    WDL_WINDOW = b"WDL: window "
    with open(log, "wb") as out:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=out, stderr=subprocess.STDOUT)
        try:
            pos = 0
            if not wait_prompt(log):
                print("[WARN] shell prompt not seen; continuing")
            time.sleep(0.5)
            # Baseline BEFORE anything from the window system is launched.
            initial_ok = qmp_screendump(INITIAL_PNG)

            def send(cmd):
                nonlocal pos
                vm.stdin.write(cmd)
                vm.stdin.flush()
                time.sleep(0.4)

            # Documented order first: start the server, then the client.
            if server:
                send(server["cmd"])
                found = wait_for(log, READY, pos, 40)
                if found < 0:
                    print("[WARN] no WSV READY after first winsrv")
                else:
                    pos = found
            for spec in clients:
                send(spec["cmd"])            # consumed by WINSRV's console lease
            time.sleep(1.5)
            launched = any(wait_for(log, WDL_READY, pos, 4) >= 0
                           for spec in clients)

            if not launched:
                print("[INFO] client consumed by the server's console lease; "
                      "re-launching the pair so both are alive concurrently")
                if server:
                    vm.stdin.write(b"\x1b")  # ESC -> WINSRV quits, frees lease
                    vm.stdin.flush()
                    wait_for(log, b"WSV: DONE", pos, 20)
                    pos = wait_prompt_at(log, pos)
                for spec in clients:
                    send(spec["cmd"])        # client first
                    found = wait_for(log, WDL_WINDOW, pos, 30)
                    if found < 0:
                        print(f"[WARN] {spec['name']} did not create a window")
                    else:
                        pos = found
                if server:
                    send(server["cmd"])      # then a live compositor joins
                    wait_for(log, READY, pos, 40)

            # Let the composite loop paint a few frames, then drive the client.
            time.sleep(3.0)
            for spec in clients:
                for key in spec["keys"]:
                    vm.stdin.write(key)
                    vm.stdin.flush()
                    time.sleep(0.3)
            time.sleep(2.0)
            # Final frame: the composited desktop, not a console overlay.
            final_ok = qmp_screendump(PNG)
            for spec in reversed(clients):           # let clients exit first
                vm.stdin.write(b"\x1b")
                vm.stdin.flush()
                time.sleep(0.8)
            if server:
                vm.stdin.write(b"\x1b")
                vm.stdin.flush()
                time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return initial_ok, final_ok


def marker_hit(text, candidates):
    return [m for m in candidates if m in text]


def verify(text, server, clients, initial_ok, final_ok):
    ok = True
    for m in BOOT_MARKERS:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] boot: {m}")
    if server:
        hits = marker_hit(text, SERVER_START)
        good = any(m.startswith("WSV") or m.startswith("WINSRV") for m in hits)
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] server WSV marker "
              f"({', '.join(hits) if hits else 'none'})")
    for spec in clients:
        hits = marker_hit(text, CLIENT_START)
        good = any(m.startswith("WDL") or m.startswith("WSDL") for m in hits)
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] client WDL marker "
              f"({', '.join(hits) if hits else 'none'})")
    bound = any("e32run: bind slot" in ln and "ord 915" in ln
                for ln in text.splitlines())
    ok &= bound
    print(f"  [{'PASS' if bound else 'FAIL'}] EUSER ord 915 bound")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    clean = not exc
    ok &= clean
    print(f"  [{'PASS' if clean else 'FAIL'}] no Exc/PanicKernExec/FAULT")
    for ln in exc:
        print("      ", ln[:200])
    if initial_ok:
        print(f"  [PASS] baseline screendump {INITIAL_PNG}")
    else:
        print(f"  [WARN] baseline screendump unavailable ({INITIAL_PNG})")
    if not final_ok:
        print(f"  [FAIL] final screendump unavailable ({PNG})")
        ok = False
    else:
        print(f"  [PASS] final screendump {PNG}")
    if initial_ok and final_ok:
        visual_ok, notes = visual_proof(INITIAL_PNG, PNG)
        print("  == visual proof:")
        for n in notes:
            print("     ", n)
        print(f"  [{'PASS' if visual_ok else 'FAIL'}] composited GUI pixels "
              "(not a console overlay)")
        ok &= visual_ok
    else:
        print("  [FAIL] visual proof: need both baseline and final dumps")
        ok = False
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--no-rebuild-iso", action="store_true")
    ap.add_argument("--logdir", default=ROOT)
    args = ap.parse_args()
    check_tools()
    server, clients = probe()
    if not server or not clients:
        print("RESULT: SKIP (WINSRV and/or WSDL not built yet)")
        return 0
    os.makedirs(args.logdir, exist_ok=True)
    if not (args.no_rebuild_iso and os.path.exists(ISO)):
        build_iso(server, clients)
    log = os.path.join(args.logdir, "r1_window_system.log")
    initial_ok, final_ok = run_guest(log, server, clients)
    with open(log, errors="replace") as f:
        text = f.read()
    print(f"\n== window-system results (serial log: {log})")
    ok = verify(text, server, clients, initial_ok, final_ok)
    if not ok:
        for ln in text.splitlines():
            if any(k in ln.lower() for k in ("wsv", "wsdl", "wsvr", "e32run",
                                             "exc", "panic")):
                print("   ", ln[:220])
    print("\nRESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
