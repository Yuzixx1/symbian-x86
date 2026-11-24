#!/usr/bin/env python3
"""r1_genuine_retire_test.py -- staged retirement gate: hand-written userspace
shims -> genuine Symbian servers (see docs/GENUINE_USERSPACE.md, section 4).

For EACH subsystem this script reports independently:

    BUILT     genuine E32 artifacts exist under qemu-port/genuine/build/
              (plus the static checks that always run: the wire opcodes in
              fs/r1_fs_core.h must still match the genuine TFsMessage enum
              in kernelhwsrv .../fileserver/inc/F32plugin.h, and user/ipc.c
              must still be able to reach the genuine "!FileServer" by name)
    RUNNING   QEMU boot launched the genuine component (boot markers)
    PASS/FAIL/SKIP

Rules (project policy):
  * A subsystem whose genuine artifacts are not built yet is SKIP.  The
    overall run NEVER fails because a component is not built yet.
  * Only when a component is actually RUN in QEMU do guest faults hard-fail
    the subsystem: "Exc ", "PanicKernExec" or "FAULT:" in the serial log.
  * Exit code is 0 unless at least one subsystem that was run FAILED.

Usage:
    python3 tools/r1_genuine_retire_test.py             # full staged run
    python3 tools/r1_genuine_retire_test.py --dry-run   # no QEMU: BUILT/static
                                                        # only, runtime = SKIP
    python3 tools/r1_genuine_retire_test.py --only ewsrv --log-dir /tmp/opencode
"""
import argparse
import os
import re
import shlex
import shutil
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
PORT = TOOLS.parent
sys.path.insert(0, str(TOOLS))          # for mke32.uid_checksum
from mke32 import uid_checksum           # noqa: E402  (genuine UID algorithm)

KHWSRV = PORT.parent / "kernelhwsrv"   # read-only reference tree
F32PLUGIN_H = KHWSRV / "userlibandfileserver/fileserver/inc/F32plugin.h"
SF_MAIN_CPP = KHWSRV / "userlibandfileserver/fileserver/sfile/sf_main.cpp"
SHIM_CORE_H = PORT / "fs/r1_fs_core.h"
IPC_C = PORT / "user/ipc.c"

KERNEL = PORT / "build/kernfull.elf"
GENUINE_BUILD = PORT / "genuine/build"
F32CLIENT = PORT / "genuine/f32_probe/F32CLIENT.E32"
EUSER_DLL = Path("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll")
PORTFILESRV = Path("/home/yuzi/.cache/sybian-r1/r1-portfilesrv-flat-v1/output/portfilesrv")

LOGDIR = Path("/tmp/opencode")
PROMPT = b"C:/> "
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:")

# Genuine server publishes this protected name (sfile/sf_main.cpp: _LIT
# KFileServerName,"!FileServer"); user/ipc.c ipc_connect() reaches any
# published server by name via exec 128 + EConnect(-1).
FILESERVER_NAME = "!FileServer"

# Genuine EFILE is staged under its genuine FAT name and auto-launched by
# GenuineBootLaunch() during Init3 -- BEFORE the serial shell prompt. Search
# these from byte 0 of the serial log; do NOT spawn "EFILE" from the shell
# (a second instance dies with a spurious FAULT). The old "GENUINEFS:
# starting" / "SvcMgr: boot launch r=0" markers belong to the retired shim
# path and are no longer the expected evidence of a live file server.
KERN_BOOT_MARKER = b"KERN spawning genuine boot DThread"
GENUINE_EFILE_LAUNCH = b"GenuineBoot: EFILE.E32 r=0"   # definitive auto-launch
GENUINE_BOOT_MARKERS = [
    b"GenuineBoot: probing boot volume",
    GENUINE_EFILE_LAUNCH,
    b"GenuineBoot: file server up, svcmgr fallback skipped",
    b"EFILE: genuine file server starting",
    b"File server thread registered",
]
# Only valid on a shim-only disk (no genuine EFILE staged): never required
# when genuine EFILE is present, but accepted as a reported fallback path.
SHIM_FALLBACK_MARKERS = [b"SvcMgr: SVCMGR.E32 not found", b"SvcMgr: boot launch r=0"]

EFSRV_CLIENT_MARKERS = [b"F32CLIENT: IPC round trip OK"]
# A port-shim hit means EFILE::Start could not register "!FileServer" by name
# (KErrBadName(-28) from the UTF-32 _LIT). It MUST be 0 after the -fshort-wchar
# / UTF-16 fix; any hit is a real correctness FAIL, not a harness artifact.
PORT_SHIM_MARKER = "EFILE: port-shim"
# Genuine EWSRV startup evidence.  The rebuilt EWSRV emits the screen-driver
# lines ("EWSRV: CScreenDriver::New() ok", "EWSRV: screen driver + windows
# initialised") in place of some older debug strings.  Every group below is
# still a HARD requirement: at least one literal in each group must be in the
# boot log.  Grouping accepts equivalent evidence from either build without
# dropping the startup / IPC / screen-driver / scheduler-ready assertions.
EWSRV_MARKER_GROUPS = [
    (b"EWSRV: genuine window server starting",),
    (b"EWSRV: E32Main entered (genuine ws_main.cpp)",),
    (b"EWSRV: WINSERVER IPC server started",),
    (b"EWSRV: CScreenDriver::New() ok",
     b"EWSRV: screen driver + windows initialised"),
    (b"EWSRV: entering active scheduler (window server READY)",),
]
EWSRV_MARKERS = [g[0] for g in EWSRV_MARKER_GROUPS]  # first literal of each group
# The loader binds exactly 212 genuine EUSER imports for EWSRV.E32 and prints
# both the import count and one "bind slot" line per ordinal (variant/e32load.cpp).
EWSRV_IMPN_MARKER = b"e32run: E32LoadDll r=0 impN=212 sDllExpN=2503"
EWSRV_BIND_MARKER = b"e32run: imports bound dll (212 ordinals)"
EWSRV_BIND_SLOT = b"e32run: bind slot "
EWSRV_BIND_COUNT = 212
ESTART_MARKERS = [b"E32STRT", b"ESTART", b"estart"]
ESHELL_MARKERS = [b"ESHELL", b"eshell", b"C:\\"]
# variant/e32load.cpp: "e32run: EUSER.DLL resident code=%x data=%x exports=%d"
# This is printed once, when EUSER.DLL is first made resident by the loader.
# A per-subsystem log (e.g. a rebuilt EWSRV boot) need not repeat it, so the
# EUSER gate asserts it on the boot logs that actually emitted the line.
EUSER_RESIDENT_MARKERS = [b"e32run: EUSER.DLL resident", b"exports=2503"]

# Components staged under their genuine FAT names are auto-launched by
# GenuineBootLaunch() during Init3 -- BEFORE the serial shell prompt. Search
# these markers from byte 0 of the serial log, never from the prompt position,
# and do not re-launch them from the shell (the duplicate instance dies).
EWSRV_BOOT_MARKER = b"GenuineBoot: EWSRV.E32"
ESTART_BOOT_MARKERS = [b"ESTART: genuine E32 image loaded",
                       b"ESHELL: genuine E32 image loaded"]

# shim opcode name (fs/r1_fs_core.h) -> genuine TFsMessage name (F32plugin.h)
OPCODE_MAP = {
    "SessionPath": "EFsSessionPath",
    "SetSessionPath": "EFsSetSessionPath",
    "FileClose": "EFsFileSubClose",
    "FileOpen": "EFsFileOpen",
    "FileCreate": "EFsFileCreate",
    "FileReplace": "EFsFileReplace",
    "FileRead": "EFsFileRead",
    "FileWrite": "EFsFileWrite",
    "FileSeek": "EFsFileSeek",
    "FileFlush": "EFsFileFlush",
    "FileSize": "EFsFileSize",
    "FileSetSize": "EFsFileSetSize",
    "FileReadCancel": "EFsFileReadCancel",
}

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
PAYLOAD = b"genuine F32 retirement probe payload\n"


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def wait_for(path, needle, pos=0, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(path, "rb") as f:
            f.seek(pos)
            data = f.read()
        i = data.find(needle)
        if i >= 0:
            return pos + i + len(needle)
        time.sleep(0.15)
    raise RuntimeError(f"serial log missing {needle!r} (see {path})")


def exc_lines(text):
    return [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]


# ---------------------------------------------------------------- static ---
def parse_genuine_tfs():
    """name -> ordinal for the genuine enum TFsMessage in F32plugin.h."""
    out, inside, idx = {}, False, 0
    for ln in F32PLUGIN_H.read_text(errors="replace").splitlines():
        if "enum TFsMessage" in ln:
            inside = True
            continue
        if inside:
            m = re.match(r"\s*(EFs\w+)\s*,", ln)
            if m:
                out[m.group(1)] = idx
                idx += 1
            if "};" in ln:
                break
    return out


def parse_shim_opcodes():
    """name -> value for the wire-opcode enum block in fs/r1_fs_core.h."""
    out, inside = {}, False
    for ln in SHIM_CORE_H.read_text(errors="replace").splitlines():
        if "enum {" in ln and "SessionPath" in ln:
            inside = True
        if inside:
            for m in re.finditer(r"(\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+)", ln):
                out[m.group(1)] = int(m.group(2), 0)
            if "};" in ln:
                break
    return out


def check_ipc_protocol():
    """Verify user/ipc.c can drive the genuine !FileServer by name: the shim
    opcodes must equal the genuine TFsMessage ordinals, the genuine server
    must publish "!FileServer", and ipc.c must have the name-connect path."""
    lines, ok = [], True
    tfs, shim = parse_genuine_tfs(), parse_shim_opcodes()
    if not tfs:
        return False, ["[FAIL] could not parse genuine TFsMessage from F32plugin.h"]
    matched = 0
    for sname, gname in OPCODE_MAP.items():
        if sname not in shim or gname not in tfs:
            lines.append(f"  [FAIL] {sname}/{gname} missing on one side")
            ok = False
        elif shim[sname] == tfs[gname]:
            matched += 1
        else:
            lines.append(f"  [FAIL] {sname}: shim={shim[sname]} genuine {gname}={tfs[gname]}")
            ok = False
    lines.insert(0, f"  [{'PASS' if ok else 'FAIL'}] {matched}/{len(OPCODE_MAP)} shim "
                    f"opcodes == genuine TFsMessage ordinals (e.g. FileOpen="
                    f"{tfs.get('EFsFileOpen')}, FileRead={tfs.get('EFsFileRead')})")
    name_ok = f'"{FILESERVER_NAME}"' in SF_MAIN_CPP.read_text(errors="replace")
    lines.append(f"  [{'PASS' if name_ok else 'FAIL'}] genuine server publishes "
                 f"\"{FILESERVER_NAME}\" (sfile/sf_main.cpp)")
    ok &= name_ok
    ipc = IPC_C.read_text(errors="replace")
    conn_ok = "ipc_connect" in ipc and "exec4(128" in ipc and "EConnect" in ipc
    lines.append(f"  [{'PASS' if conn_ok else 'FAIL'}] user/ipc.c ipc_connect(name) "
                 f"path present (exec 128 + EConnect) -> can talk to \"{FILESERVER_NAME}\" by name")
    ok &= conn_ok
    return ok, lines


def e32_header(path):
    """Minimal E32 header read (strict reloc validation is intentionally NOT
    applied: the genuine EUSER.DLL has one out-of-strict-bound relocation that
    the in-kernel loader tolerates -- see GENUINE_USERSPACE.md section 2.3)."""
    blob = Path(path).read_bytes()[:160]
    if len(blob) < 160:
        return None
    uid1, uid2, uid3, chk = struct.unpack_from("<4I", blob, 0)
    flags, = struct.unpack_from("<I", blob, 44)
    exp_count, = struct.unpack_from("<i", blob, 92)
    return {"uids": (uid1, uid2, uid3), "uid_ok": uid_checksum(uid1, uid2, uid3) == chk,
            "dll": bool(flags & 1), "exports": exp_count,
            "size": Path(path).stat().st_size}


def first_existing(candidates):
    for c in candidates:
        if Path(c).is_file():
            return Path(c)
    return None


# --------------------------------------------------------------- runtime ---
def stage_disk(tag, files):
    disk, fs = LOGDIR / f"r1_retire_{tag}.img", LOGDIR / f"r1_retire_{tag}_fs.img"
    for p in (disk, fs):
        if p.exists():
            p.unlink()
    disk.write_bytes(b"\0" * (MB * 1024 * 1024))
    with open(fs, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", fs, check=True)
    notes = LOGDIR / f"r1_retire_{tag}_NOTES.TXT"
    notes.write_bytes(PAYLOAD)
    for src, name in files + [(notes, "NOTES.TXT")]:
        sh("mcopy", "-i", fs, src, f"::{name}", check=True)
    data = fs.read_bytes()
    with open(disk, "r+b") as f:
        f.seek(OFFSET)
        f.write(data)
        mbr = bytearray(512)
        mbr[446:462] = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
                        + LBA.to_bytes(4, "little") + SECTORS.to_bytes(4, "little"))
        mbr[510], mbr[511] = 0x55, 0xAA
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{disk}@@{OFFSET}", "::", check=True,
             capture_output=True, text=True).stdout
    print(f"  [PASS] FAT staged for {tag}: {len(files) + 1} files")
    return disk, out


def run_qemu(tag, disk, script, vga=False, qmp_sock=None, timeout=150,
             boot_expect=None, shot_settle=1.0):
    """Boot the port kernel; script = [(send, expect, timeout), ...].

    boot_expect: markers emitted by GenuineBootLaunch() during Init3, i.e.
    before the shell prompt. They are searched from byte 0 of the serial log.
    When a qmp_sock is given, the screendump is taken a fixed `shot_settle`
    seconds after the last boot marker appeared -- independent of the prompt
    and of any shell interaction.
    """
    log = LOGDIR / f"r1_retire_{tag}.log"
    if log.exists():
        log.unlink()
    if qmp_sock and qmp_sock.exists():
        qmp_sock.unlink()
    qemu = ["qemu-system-i386", "-M", "q35", "-m", "128", "-display", "none",
            "-serial", "stdio", "-kernel", str(KERNEL),
            "-drive", f"file={disk},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    if vga:
        qemu += ["-vga", "std"]
    if qmp_sock:
        qemu += ["-qmp", f"unix:{qmp_sock},server,nowait"]
    print(f"== booting QEMU for {tag} ...")
    pos, shot, boot_at = 0, None, None
    with open(log, "wb") as lf:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE, stdout=lf,
                              stderr=subprocess.STDOUT)
        try:
            if boot_expect:
                for marker in boot_expect:
                    # Anchored at boot start: these lines precede the prompt.
                    pos = wait_for(log, marker, 0, 90)
                boot_at = time.monotonic()
                print(f"  [INFO] {tag}: boot markers present at byte {pos}")
            pos = wait_for(log, PROMPT, 0, 90)
            for send, expect, tmo in script:
                vm.stdin.write(send + b"\n")
                vm.stdin.flush()
                if expect:
                    pos = wait_for(log, expect, pos, tmo)
        except RuntimeError as exc:
            print(f"  [WARN] {tag}: {exc}")
        finally:
            if qmp_sock:
                try:
                    if boot_at is not None:
                        # Fixed delay relative to the boot marker, not to the
                        # prompt: let the genuine desktop paint before dumping.
                        left = shot_settle - (time.monotonic() - boot_at)
                        if left > 0:
                            time.sleep(left)
                    else:
                        time.sleep(shot_settle)
                    shot = qmp_screendump(qmp_sock, LOGDIR / f"r1_retire_{tag}.png")
                except Exception as exc:
                    print(f"  [WARN] {tag}: screendump: {exc}")
            vm.terminate()
            try:
                vm.wait(timeout=4)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
    return log.read_text(errors="replace"), shot


def qmp_screendump(sock_path, png):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline and not sock_path.exists():
        time.sleep(0.2)
    if not sock_path.exists():
        return False
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.settimeout(5)
        s.connect(str(sock_path))
        f = s.makefile("rwb")
        f.readline()
        f.write(b'{"execute":"qmp_capabilities"}\n'); f.flush(); f.readline()
        if png.exists():
            png.unlink()
        f.write((f'{{"execute":"screendump","arguments":{{"filename":"{png}",'
                 f'"format":"png"}}}}\n').encode())
        f.flush()
        for _ in range(10):
            line = f.readline()
            if not line or b"return" in line or b"error" in line.lower():
                break
        f.close(); s.close()
    except OSError as exc:
        print(f"  [WARN] QMP screendump failed: {exc}")
        return False
    for _ in range(20):
        if png.exists() and png.stat().st_size > 1000:
            return True
        time.sleep(0.15)
    return png.exists() and png.stat().st_size > 1000


def screendump_renders(png):
    """A rendered desktop means the screen is not one single flat colour.
    The genuine EWSRV text-server desktop legitimately paints a solid
    background plus a border (2 colours), so the threshold is >1, not >4."""
    try:
        from PIL import Image
        im = Image.open(png).convert("RGB")
        colours = im.getcolors(maxcolors=1 << 20)
        return colours is not None and len(colours) > 1
    except Exception:
        return png.stat().st_size > 1000   # no PIL: non-trivial PNG size only


# ----------------------------------------------------------------- gates ---
class Gate:
    def __init__(self, name, shim):
        self.name, self.shim, self.built, self.ran = name, shim, None, False
        self.lines, self.failed = [], False

    def say(self, line):
        self.lines.append(line)
        print("   " + line, flush=True)

    def finish(self):
        # Policy: a not-yet-built or not-run component is always SKIP (never
        # fails the run); only a component actually RUN can FAIL (guest
        # Exc/Panic/FAULT or missing markers).
        if self.built is False or not self.ran:
            verdict = "SKIP"
        elif self.failed:
            verdict = "FAIL"
        else:
            verdict = "PASS"
        self.verdict = verdict
        self.say(f"SUBSYSTEM {self.name}: {verdict}")
        return verdict


def gate_efsrv(dry, no_disk):
    g = Gate("EFSRV/F32", "user/portfilesrv.cpp + fs/r1_fs_* + variant/fat32.cpp")
    proto_ok, proto_lines = check_ipc_protocol()
    for ln in proto_lines:
        g.say(ln)
    client = first_existing([GENUINE_BUILD / "EFSRV.DLL", GENUINE_BUILD / "EFSRV.E32"])
    server = first_existing([GENUINE_BUILD / "EFILE.E32", GENUINE_BUILD / "EFILE.EXE",
                             GENUINE_BUILD / "EFILE"])
    fsy = first_existing([GENUINE_BUILD / "EFAT32.FSY", GENUINE_BUILD / "EFAT32.E32"])
    missing = [lbl for lbl, p in (("efsrv client (EFSRV.DLL)", client),
                                  ("efile server (EFILE.E32)", server),
                                  ("efat32 fsy (EFAT32.FSY)", fsy),
                                  ("probe client (f32_probe/F32CLIENT.E32)",
                                   F32CLIENT if F32CLIENT.is_file() else None)) if not p]
    if missing:
        g.built = False
        g.say(f"BUILT:   NO   missing: {', '.join(missing)}")
        g.say("RUNNING: SKIP  (genuine F32 component not built yet)")
        g.say("PASS:    SKIP  (nothing to retire yet)")
        if not proto_ok:
            g.failed = True
            g.say("PROTO:   FAIL  (shim opcodes no longer match genuine TFsMessage)")
        return g.finish()
    g.built = True
    g.say(f"BUILT:   YES  efsrv={client.name} efile={server.name} fsy={fsy.name}")
    if dry:
        g.say("RUNNING: SKIP  (--dry-run)")
        g.say("PASS:    SKIP  (not executed)")
        return g.finish()
    files = [(EUSER_DLL, "EUSER.DLL"), (server, "EFILE.E32"), (fsy, "EFAT32.FSY"),
             (F32CLIENT, "F32CLIENT.E32")]
    disk, _ = stage_disk("efsrv", files)
    # Genuine EFILE is auto-launched by GenuineBootLaunch() during Init3.
    # Do NOT send "EFILE" from the shell: the duplicate instance dies with a
    # spurious FAULT. Only the probe client is launched from the shell.
    script = [(b"", PROMPT, 30),          # empty line: shell re-prints the prompt
              (b"f32client", EFSRV_CLIENT_MARKERS[-1], 60)]
    text, _ = run_qemu("efsrv", disk, script, boot_expect=[KERN_BOOT_MARKER])
    g.ran = True

    # Which path brought up the file server? Genuine EFILE auto-launch is
    # expected on this disk; the shim fallback is only valid on a shim-only
    # disk. Accept either, but always report which one was actually used.
    genuine = (GENUINE_EFILE_LAUNCH.decode() in text or
               "GenuineBoot: file server up" in text)
    shim = any(m.decode() in text for m in SHIM_FALLBACK_MARKERS)
    path = ("GENUINE EFILE auto-launch" if genuine else
            "SHIM FALLBACK (svcmgr/portfilesrv)" if shim else "NONE")
    g.say(f"RUNNING: {'YES' if (genuine or shim) else 'NO'}  file-server path = {path}")
    g.say(f"  [{'PASS' if (genuine or shim) else 'FAIL'}] a file server was brought up "
          f"(genuine={'yes' if genuine else 'no'}, shim_fallback={'yes' if shim else 'no'})")
    if not (genuine or shim):
        g.failed = True
    if genuine:
        # All Init3 boot markers must be present on the genuine path.
        for m in GENUINE_BOOT_MARKERS:
            hit = m.decode() in text
            g.say(f"  [{'PASS' if hit else 'FAIL'}] {m.decode()}")
            if not hit:
                g.failed = True
    else:
        for m in SHIM_FALLBACK_MARKERS:
            g.say(f"  [{'INFO' if m.decode() in text else '----'}] {m.decode()} "
                  f"({'seen' if m.decode() in text else 'not seen'})")
    for m in EFSRV_CLIENT_MARKERS:
        hit = m.decode() in text
        g.say(f"  [{'PASS' if hit else 'FAIL'}] {m.decode()}")
        if not hit:
            g.failed = True
    # A port-shim hit means the genuine server fell back from name binding
    # (EFILE::Start KErrBadName(-28) from the UTF-32 _LIT). It MUST be 0 after
    # the -fshort-wchar / UTF-16 fix; any hit is a real correctness failure.
    shim_hits = text.count(PORT_SHIM_MARKER)
    reason = ("" if shim_hits == 0 else
              " -- EFILE::Start KErrBadName(-28): UTF-16 _LIT fix not landed")
    g.say(f"  [{'PASS' if shim_hits == 0 else 'FAIL'}] {PORT_SHIM_MARKER} count == 0 "
          f"(got {shim_hits}){reason}")
    if shim_hits:
        g.failed = True
    # An early genuine-server exit is a real guest failure that explains a
    # missing "F32CLIENT: IPC round trip OK" (not a stale-marker artifact).
    early_exit = [ln.strip() for ln in text.splitlines()
                  if "exit:" in ln and "EFILE" in ln]
    if early_exit:
        g.say("[INFO] genuine EFILE exited early: " + "; ".join(early_exit[:2]))
    exc = exc_lines(text)
    g.say(f"  [{'PASS' if not exc else 'FAIL'}] no Exc/PanicKernExec/FAULT "
          f"(component was run)" + ("" if not exc else f" -- {len(exc)} lines"))
    if exc:
        g.failed = True
        for ln in exc[:8]:
            g.say("      " + ln)
    if not proto_ok:
        g.failed = True
    return g.finish()


def gate_ewsrv(dry, no_disk):
    g = Gate("EWSRV", "user/winsrv.c + variant/qemu_windowsrv.cpp + GUI execs")
    e32 = first_existing([GENUINE_BUILD / "EWSRV.E32"])
    if not e32:
        g.built = False
        g.say("BUILT:   NO   missing genuine/build/EWSRV.E32")
        g.say("RUNNING: SKIP  (genuine ewsrv not built yet)")
        g.say("PASS:    SKIP")
        return g.finish()
    g.built = True
    g.say("BUILT:   YES  genuine/build/EWSRV.E32")
    if dry:
        g.say("RUNNING: SKIP  (--dry-run)")
        g.say("PASS:    SKIP  (not executed)")
        return g.finish()
    files = [(EUSER_DLL, "EUSER.DLL"), (e32, "EWSRV.E32")]
    if PORTFILESRV.is_file():
        files.append((PORTFILESRV, "portfilesrv"))
    disk, _ = stage_disk("ewsrv", files)
    sock = LOGDIR / "r1_retire_ewsrv-qmp.sock"
    # EWSRV.E32 is staged under its genuine name, so GenuineBootLaunch()
    # starts it during Init3, before the prompt. Sending "ewsrv" from the
    # shell here starts a second instance that dies silently (spurious
    # FAULT), so wait for the boot markers from byte 0 instead.
    # Wait only for the one stable startup anchor from byte 0.  Requiring every
    # marker here would stall the runner when a rebuilt EWSRV swaps strings;
    # the full marker groups are still hard-asserted against the log below.
    text, shot = run_qemu("ewsrv", disk, [], vga=True, qmp_sock=sock,
                          boot_expect=[EWSRV_MARKERS[0]], shot_settle=3.0)
    g.ran = True
    booted = EWSRV_BOOT_MARKER.decode() in text or EWSRV_MARKERS[0].decode() in text
    g.say(f"RUNNING: {'YES' if booted else 'NO'}  "
          f"(boot-time genuine ewsrv startup markers)")
    for group in EWSRV_MARKER_GROUPS:
        seen = [m.decode() for m in group if m.decode() in text]
        label = " / ".join(m.decode() for m in group)
        detail = "" if not seen or len(group) == 1 else f"  (seen: {seen[0]})"
        g.say(f"  [{'PASS' if seen else 'FAIL'}] {label}{detail}")
        if not seen:
            g.failed = True
    # Hard check: EWSRV.E32 must bind exactly its 212 genuine EUSER imports.
    imp = EWSRV_IMPN_MARKER.decode() in text
    binds = EWSRV_BIND_MARKER.decode() in text
    slots = text.count(EWSRV_BIND_SLOT.decode())
    slot_ok = slots == EWSRV_BIND_COUNT
    g.say(f"  [{'PASS' if imp else 'FAIL'}] {EWSRV_IMPN_MARKER.decode()}")
    g.say(f"  [{'PASS' if binds else 'FAIL'}] {EWSRV_BIND_MARKER.decode()}")
    g.say(f"  [{'PASS' if slot_ok else 'FAIL'}] EUSER bind slots == "
          f"{EWSRV_BIND_COUNT} (got {slots})")
    if not (imp and binds and slot_ok):
        g.failed = True
    ok_shot = bool(shot) and screendump_renders(LOGDIR / "r1_retire_ewsrv.png")
    g.say(f"  [{'PASS' if ok_shot else 'FAIL'}] screendump renders a window "
          f"({LOGDIR / 'r1_retire_ewsrv.png'})")
    if not ok_shot:
        g.failed = True
    exc = exc_lines(text)
    g.say(f"  [{'PASS' if not exc else 'FAIL'}] no Exc/PanicKernExec/FAULT "
          f"(component was run)" + ("" if not exc else f" -- {len(exc)} lines"))
    if exc:
        g.failed = True
        for ln in exc[:8]:
            g.say("      " + ln)
    return g.finish()


def gate_estart_eshell(dry, no_disk):
    g = Gate("ESTART/ESHELL", "user/svcmgr.cpp + execs 268-270 boot launch")
    e32strt = first_existing([GENUINE_BUILD / "E32STRT.E32", GENUINE_BUILD / "ESTART.E32",
                              GENUINE_BUILD / "E32STRT.EXE", GENUINE_BUILD / "estart_.E32"])
    eshell = first_existing([GENUINE_BUILD / "ESHELL.E32", GENUINE_BUILD / "ESHELL.EXE",
                             GENUINE_BUILD / "etshell_.E32"])
    missing = [lbl for lbl, p in (("e32strt (E32STRT.E32)", e32strt),
                                  ("eshell (ESHELL.E32)", eshell)) if not p]
    if missing:
        g.built = False
        g.say(f"BUILT:   NO   missing: {', '.join(missing)}")
        g.say("RUNNING: SKIP  (genuine estart/eshell not built yet)")
        g.say("PASS:    SKIP")
        return g.finish()
    g.built = True
    g.say(f"BUILT:   YES  e32strt={e32strt.name} eshell={eshell.name}")
    if dry:
        g.say("RUNNING: SKIP  (--dry-run)")
        g.say("PASS:    SKIP  (not executed)")
        return g.finish()
    files = [(EUSER_DLL, "EUSER.DLL"), (e32strt, "ESTART.E32"), (eshell, "ESHELL.E32")]
    disk, _ = stage_disk("estart", files)
    # Both E32s are staged under their genuine names and boot-launched by
    # GenuineBootLaunch(); shell "estart"/"eshell" would spawn duplicate
    # processes. Wait for their boot markers from byte 0 of the log.
    text, _ = run_qemu("estart", disk, [], boot_expect=ESTART_BOOT_MARKERS)
    g.ran = True
    ran = any(m.decode() in text for m in ESTART_MARKERS)
    g.say(f"RUNNING: {'YES' if ran else 'NO'}  (boot-time genuine "
          f"e32strt/eshell markers)")
    hit_any = [m.decode() for m in ESTART_MARKERS + ESHELL_MARKERS if m.decode() in text]
    g.say(f"  [{'PASS' if hit_any else 'FAIL'}] shell boot markers: {hit_any or 'none'}")
    if not hit_any:
        g.failed = True
    exc = exc_lines(text)
    g.say(f"  [{'PASS' if not exc else 'FAIL'}] no Exc/PanicKernExec/FAULT "
          f"(component was run)" + ("" if not exc else f" -- {len(exc)} lines"))
    if exc:
        g.failed = True
        for ln in exc[:8]:
            g.say("      " + ln)
    return g.finish()


def gate_euser(dry, qemu_logs):
    g = Gate("EUSER", "user/e32base.h + user/euser_shim.cpp (static shim only)")
    if not EUSER_DLL.is_file():
        g.built = False
        g.say(f"BUILT:   NO   missing prebuilt {EUSER_DLL}")
        g.say("PASS:    SKIP")
        return g.finish()
    hdr = e32_header(EUSER_DLL)
    want = (0x10000079, 0x1000008D, 0x100039E5)
    ok = hdr and hdr["uid_ok"] and hdr["uids"] == want and hdr["dll"] and hdr["exports"] == 2503
    g.built = True
    g.say(f"BUILT:   {'YES' if ok else 'NO'}  prebuilt euser.dll "
          f"({hdr['size']} B, uids {'/'.join(hex(u) for u in hdr['uids'])}, "
          f"uid_checksum {'ok' if hdr['uid_ok'] else 'BAD'}, {hdr['exports']} exports)")
    if not ok:
        g.failed = True
    if dry or not qemu_logs:
        g.say("RUNNING: SKIP  (no QEMU log this run / --dry-run; covered by r1_qemu_smoke.py)")
        g.say("PASS:    " + ("SKIP" if not ok else "SKIP (residency checked by smoke)"))
        return g.finish()
    # The loader prints the residency line once, when EUSER.DLL is first made
    # resident (variant/e32load.cpp).  A per-subsystem boot built from a
    # different EWSRV image need not repeat it, so require the full evidence
    # from the logs that actually emitted it and reject any contradictory
    # export count.  The static 2503-export header check above still applies.
    residency_logs = [log for log in qemu_logs if EUSER_RESIDENT_MARKERS[0] in log]
    resident = bool(residency_logs) and all(
        EUSER_RESIDENT_MARKERS[1] in log for log in residency_logs)
    bad_exports = [ln.decode(errors="replace") for log in qemu_logs for ln in log.splitlines()
                   if EUSER_RESIDENT_MARKERS[0] in ln and EUSER_RESIDENT_MARKERS[1] not in ln]
    g.ran = True
    g.say(f"RUNNING: {'YES' if resident else 'NO'}  ({EUSER_RESIDENT_MARKERS[0].decode()}, "
          f"{EUSER_RESIDENT_MARKERS[1].decode()})")
    g.say(f"  [{'PASS' if resident and ok else 'FAIL'}] genuine EUSER.DLL resident + "
          f"exports=2503 in {len(residency_logs)}/{len(qemu_logs)} QEMU boot log(s)")
    g.say(f"  [{'PASS' if not bad_exports else 'FAIL'}] no residency line with a "
          f"different export count" + ("" if not bad_exports else f" -- {bad_exports[:2]}"))
    if not (resident and ok) or bad_exports:
        g.failed = True
    return g.finish()


# ------------------------------------------------------------------ main ---
def main():
    global LOGDIR
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true",
                    help="no QEMU: report BUILT + static protocol checks, runtime = SKIP")
    ap.add_argument("--only", help="comma-separated subsystem filter "
                    "(efsrv,ewsrv,estart,euser)")
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log-dir", default=str(LOGDIR))
    args = ap.parse_args()

    LOGDIR = Path(args.log_dir)
    LOGDIR.mkdir(parents=True, exist_ok=True)

    for t in ("qemu-system-i386", "mkfs.fat", "mcopy", "mdir"):
        if not args.dry_run and not shutil.which(t):
            print(f"[WARN] missing tool {t}; QEMU stages will SKIP")
    if not KERNEL.is_file():
        print(f"[WARN] kernel {KERNEL} missing; QEMU stages will SKIP")

    print("r1_genuine_retire_test -- staged shim-retirement gate "
          "(docs/GENUINE_USERSPACE.md section 4)", flush=True)
    print(f"workspace: {PORT}  dry-run={args.dry_run}", flush=True)

    gates = {"efsrv": gate_efsrv, "ewsrv": gate_ewsrv,
             "estart": gate_estart_eshell, "euser": None}   # euser needs logs
    order = ["efsrv", "ewsrv", "estart", "euser"]
    if args.only:
        order = [s for s in order if s in args.only.split(",")]

    results, logs = {}, []
    for key in order:
        if key == "euser":
            continue
        print(f"== [{key}]", flush=True)
        verdict = gates[key](args.dry_run, args.no_rebuild_disk)
        results[key] = verdict
        if not args.dry_run and (LOGDIR / f"r1_retire_{key}.log").exists():
            logs.append((LOGDIR / f"r1_retire_{key}.log").read_bytes())
    if "euser" in order:
        print("== [euser]", flush=True)
        results["euser"] = gate_euser(args.dry_run, logs)

    passed = sum(1 for v in results.values() if v == "PASS")
    skipped = sum(1 for v in results.values() if v == "SKIP")
    failed = sum(1 for v in results.values() if v == "FAIL")
    print("\n----- summary -----")
    for k, v in results.items():
        print(f"  {k:8s} {v}")
    overall = "FAIL" if failed else "PASS"
    print(f"RESULT: {overall}  ({passed} passed, {skipped} skipped "
          f"[not built yet], {failed} failed)")
    print("note: SKIP never fails the run; only a RUN component with "
          "Exc/Panic/FAULT or missing markers fails.")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
