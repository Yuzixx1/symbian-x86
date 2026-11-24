#!/usr/bin/env python3
"""r1_genuine_eshell_test.py -- genuine ESTART/ESHELL boot proof under our kernel.

Stages on a FAT32 disk:
  * ESTART.E32   -- genuine kernelhwsrv fileserver/estart (estart.cpp +
                    estartmain.cpp), built by genuine/estart_eshell/
                    build_estart_eshell.py as genuine/build/estart_.E32
  * ESHELL.E32    -- genuine fileserver/etshell text shell (ts_*.cpp +
                    sfile/sf_inflate.cpp + sf_decomp.cpp), genuine/build/etshell_.E32
  * EUSER.DLL    -- genuine 2503-ordinal EUSER from the Raptor build root

Boots build/kernfull.elf in QEMU, E32Runs `estart` then `eshell` from the
serial shell, and asserts:
  * both images load, resume, and print their banners via EUSER ord 915
    (RDebug::Printf, PE2 import bound by the loader),
  * the genuine CShell::DoBanner output ("ESHELL ... CFG=", "Copyright (c)
    1998 Symbian Ltd") and the CLineEdit prompt "C:\\>",
  * the shell READS INPUT: typing HELP lists the genuine iCommand table
    (ATTRIB/CHKDSK/...), EXIT returns from the genuine E32Main (0),
  * EUSER ord 915 bound for both images, and no Exc/PanicKernExec/FAULT/#PF lines.

After HELP (before EXIT) the test also types real shell commands so the
rebuilt sret-ABI imports actually execute. Each command is delimited by the
genuine "C:\\>" prompt; its serial segment is printed as evidence and checked
for a plausible result plus the absence of fault lines:

  * TIME          -> genuine ShellFunction::Time (TTime/HomeTime/DateTime)
  * CHKDSK C:     -> genuine argument handling (StripQuotes/CharToDrive/CheckDisk)
  * DRVINFO Z:    -> DoExtractDriveLetter -> EUSER ord 1017 TLex16::NextToken
                     (Z: is rejected before PrintDrvInfo, so the token parse
                     runs but the unsupported drive-info print is avoided)
  * RUNEXEC 1 SRET.TXT alpha -> ShellFunction::RunExec; EUSER ord 2104
                     TDesC16::Mid twice (count/name split and parameter split)
  * ECHO / LABEL / SYSINFO -> further genuine commands with plausible output
  * PS -> genuine ShellFunction::Ps (interactive process status).  The test
    types PS, waits for the genuine "ps>" prompt, types T (list threads ->
    TShowProcInfo::GetThreads -> GetThreadInfo -> RThread::Id ord 1793 and
    RProcess::Id ord 1849), asserts "ID <n> (Proc <n>)" lines, types Q and
    waits for C:\\> again.  Fault-free only: no #PF/Exc/Panic.

Documented skips (unsupported on this port, not sret-ABI failures):
  * ATTRIB, DIR with args: genuine paths call RFs attribute/enumeration calls
    that are ret-0 stubs here; the shell owner dies (no FAULT line) before
    returning a prompt, so they cannot be part of a clean PASS run.
  * DRVINFO C: (instead of Z:): ord 1017 runs, then PrintDrvInfo dereferences
    an unsupported mounted-FS name and the shell dies.
  * TYPE: the shim RFile::Read returns 0 bytes, so ShellFunction::Type's
    TDesC16::Mid loop is not entered; RUNEXEC exercises ord 2104 instead.
  * ord 1677 TTime::MicroSecondsFrom: only reached in RunExec *after*
    RProcess::Create succeeds; EUSER RProcess::Create returns -1 here, so this
    import is still untested.
  * estart's ord 1017/2104 in TFSStartup::ParseMappingRecord: estart's E32Main
    exits before reading Z:\\SYS\\DATA\\ESTART.TXT.

Also validates both E32 images with tools/e32read.py.

Usage: python3 tools/r1_genuine_eshell_test.py [--no-rebuild-disk] [--log PATH]
"""
import argparse
import os
import re
import shlex
import subprocess
import sys
import time

ROOT = "/tmp/opencode"
DISK = ROOT + "/r1_genuine_eshell.img"
FSIMG = ROOT + "/r1_genuine_eshell_fs.img"
LOG_DEFAULT = ROOT + "/r1_genuine_eshell.log"
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KERNEL = REPO + "/build/kernfull.elf"
EUSER = "/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/release/x86/urel/euser.dll"
ESTART = REPO + "/genuine/build/estart_.E32"
ESHELL = REPO + "/genuine/build/etshell_.E32"
SRET_TXT = ROOT + "/r1_genuine_eshell_SRET.TXT"

MB, LBA, SECTORS = 32, 2048, 16384
FS_SIZE, OFFSET = SECTORS * 512, LBA * 512
EXC_KEYS = ("Exc ", "PanicKernExec", "FAULT:", "#PF")

MARKERS = [
    "ESTART: genuine E32 image loaded",          # glue banner via ord 915
    "ESHELL: genuine E32 image loaded",          # glue banner via ord 915
    "CFG=",                                      # genuine CShell::DoBanner
    "Copyright (c) 1998 Symbian Ltd",            # genuine DoBanner
    "C:\\>",                                     # genuine CLineEdit prompt
    "ATTRIB",                                    # HELP -> genuine iCommand table
    "CHKDSK",
    "ESHELL: E32Main returned 0",                # EXIT -> genuine E32Main returns
]

# (label, text typed at C:\>, expected bytes, optional regex over '?'-stripped text)
COMMANDS = [
    ("TIME", "time", None,
     r"\d{1,2}\s*/\s*\d{1,2}\s*/\s*\d{4}"),
    ("CHKDSK C:", "chkdsk C:", b"Completed - no errors found", None),
    ("DRVINFO Z:", "drvinfo Z:", b"Invalid drive specification", None),
    ("RUNEXEC Mid x2", "runexec 1 SRET.TXT alpha",
     b"Failed to spawn command SRET.TXT.EXE", None),
    ("ECHO", "echo sret-abi-ok", b"\r\nsret-abi-ok\r\n", None),
    ("LABEL", "label", b"Volume Label:", None),
    ("SYSINFO", "sysinfo", b"Crazy scheduler delays are", None),
    # PS is interactive: after the "ps>" sub-prompt we type T (threads), which
    # calls GetThreadInfo -> RThread::Id / RProcess::Id, then Q to return to C:\>.
    ("PS", "ps", b"--PROCESSES-->", None),
]


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args))
    return subprocess.run([str(a) for a in args], **kw)


def validate_inputs():
    ok = True
    for p in (KERNEL, EUSER, ESTART, ESHELL):
        if not os.path.isfile(p):
            print(f"[FAIL] missing {p} (run genuine/estart_eshell/build_estart_eshell.py)")
            ok = False
    if not ok:
        sys.exit(1)
    for p in (ESTART, ESHELL):
        r = sh("python3", REPO + "/tools/e32read.py", p, "--quiet")
        print(f"[{'PASS' if r.returncode == 0 else 'FAIL'}] E32 valid: {os.path.basename(p)}")
        ok &= r.returncode == 0
    return ok


def build_disk():
    for p in (DISK, FSIMG):
        if os.path.exists(p):
            os.remove(p)
    with open(DISK, "wb") as f:
        f.truncate(MB * 1024 * 1024)
    with open(FSIMG, "wb") as f:
        f.truncate(FS_SIZE)
    sh("mkfs.fat", "-F", "32", FSIMG, check=True)
    with open(SRET_TXT, "w") as f:
        f.write("sret-abi-ok\n")
    # Stage under names the GenuineBoot autostart probe does not recognise
    # (variant/e32load.cpp looks for \ESTART.E32 / \ESHELL.E32 at boot).  This
    # keeps the test in control of when each image is E32Run; UID3 of ESHELL is
    # patched by the builder so it is not mistaken for the single-owner EFILE.
    for src, name in ((ESTART, "ESTARTX.E32"), (ESHELL, "ESHELLX.E32"),
                      (EUSER, "EUSER.DLL"), (SRET_TXT, "SRET.TXT")):
        sh("mcopy", "-i", FSIMG, src, f"::{name}", check=True)
    with open(FSIMG, "rb") as f:
        data = f.read()
    if len(data) > FS_SIZE:
        sys.exit("FAT image larger than partition")
    entry = (bytes([0x80, 0, 2, 0, 0x0C, 0, 2, 0])
             + LBA.to_bytes(4, "little") + SECTORS.to_bytes(4, "little"))
    mbr = bytearray(512)
    mbr[446:462] = entry
    mbr[510], mbr[511] = 0x55, 0xAA
    with open(DISK, "r+b") as f:
        f.seek(OFFSET)
        f.write(data)
        f.seek(0)
        f.write(mbr)
    out = sh("mdir", "-i", f"{DISK}@@{OFFSET}", "::", check=True,
             capture_output=True, text=True)
    norm = "".join(out.stdout.lower().split()).replace(".", "")
    for want in ("estartxe32", "eshellxe32", "euserdll", "srettxt"):
        if want not in norm:
            sys.exit(f"disk missing {want}:\n{out.stdout}")
    print("mdir OK: ESTARTX.E32, ESHELLX.E32, EUSER.DLL, SRET.TXT staged")


def wait_for(path, needle, start=0, timeout=90):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        with open(path, "rb") as f:
            f.seek(start)
            data = f.read()
        i = data.find(needle)
        if i >= 0:
            return start + i + len(needle)
        time.sleep(0.2)
    raise RuntimeError(f"serial log missing {needle!r} (see {path})")


def run_qemu(log_path):
    qemu = ["qemu-system-i386", "-M", "q35", "-m", "128",
            "-display", "none", "-serial", "stdio", "-kernel", KERNEL,
            "-drive", f"file={DISK},format=raw,snapshot=on,if=none,id=d0",
            "-device", "ich9-ahci,id=sata", "-device", "ide-hd,drive=d0,bus=sata.0",
            "-monitor", "none", "-no-reboot"]
    print("== booting QEMU: E32Run genuine estart, then eshell, drive commands ...")
    with open(log_path, "wb") as log:
        vm = subprocess.Popen(qemu, stdin=subprocess.PIPE,
                              stdout=log, stderr=subprocess.STDOUT)
        evidence = []
        try:
            pos = wait_for(log_path, b"C:/> ", 0, 90)

            def send(text, expect, timeout=60):
                nonlocal pos
                vm.stdin.write(text.encode() + b"\n")
                vm.stdin.flush()
                pos = wait_for(log_path, expect, pos, timeout)

            send("estartx", b"ESTART: genuine E32 image loaded")
            time.sleep(2.0)                     # let genuine estart run to its panic/exit
            send("eshellx", b"ESHELL: genuine E32 image loaded")
            for m in (b"CFG=", b"Copyright (c) 1998 Symbian Ltd", b"C:\\>"):
                pos = wait_for(log_path, m, pos, 60)
            send("help", b"ATTRIB", 60)         # shell reads + executes HELP
            pos = wait_for(log_path, b"CHKDSK", pos, 30)
            pos = wait_for(log_path, b"C:\\>", pos, 60)  # prompt after HELP

            for label, typed, marker, regex in COMMANDS:
                before = pos
                vm.stdin.write(typed.encode() + b"\n")
                vm.stdin.flush()
                if label == "PS":
                    # PS is a genuine shell-in-shell: drive T (threads ->
                    # GetThreadInfo -> RThread::Id / RProcess::Id) then Q.
                    pos = wait_for(log_path, b"ps>", before, 60)
                    vm.stdin.write(b"t\n")
                    vm.stdin.flush()
                    pos = wait_for(log_path, b"--THREADS-->", pos, 60)
                    pos = wait_for(log_path, b"ID ", pos, 60)
                    pos = wait_for(log_path, b"ps>", pos, 60)
                    vm.stdin.write(b"q\n")
                    vm.stdin.flush()
                pos = wait_for(log_path, b"C:\\>", before, 60)
                with open(log_path, "rb") as f:
                    f.seek(before)
                    seg = f.read(pos - before)
                text = seg.decode("utf-8", "replace")
                clean = text.replace("?", "").replace("\r", "")
                hit = (bool(re.search(regex, clean)) if regex
                       else marker in seg)
                if label == "PS":
                    # Also require the thread-ID lines from GetThreadInfo and
                    # a plausible nonzero PID (the ids are genuine EUSER ords
                    # 1793 / 1849).  RThread::Id returns a TThreadId struct in
                    # EAX/EDX, RProcess::Id a TProcessId in EAX (sret ABI).
                    hit = (hit and b"--THREADS-->" in seg
                           and re.search(r"ID \d+ \(Proc [1-9]\d*\)", clean)
                           is not None)
                faults = [ln for ln in text.splitlines()
                          if any(k in ln for k in EXC_KEYS)]
                evidence.append((label, hit, not faults))
                one_line = " ".join(clean.split())
                snippet = (one_line if len(one_line) <= 150
                           else one_line[:50] + " ... " + one_line[-95:])
                print(f"  [CMD] {label}: {'PASS' if hit else 'FAIL'} "
                      f"fault-free={'PASS' if not faults else 'FAIL'} "
                      f"| {snippet}", flush=True)
                for ln in faults[:2]:
                    print("    " + ln)

            send("exit", b"ESHELL: E32Main returned 0", 60)
            time.sleep(1.0)
        finally:
            vm.terminate()
            try:
                vm.wait(timeout=3)
            except subprocess.TimeoutExpired:
                vm.kill()
                vm.wait()
        return evidence


def check(text, evidence):
    ok = True
    for m in MARKERS:
        good = m in text
        ok &= good
        print(f"  [{'PASS' if good else 'FAIL'}] {m}")
    binds = [ln for ln in text.splitlines() if "e32run: bind slot" in ln]
    b915 = [ln for ln in binds if "ord 915" in ln]
    good = len(b915) >= 2
    ok &= good
    print(f"  [{'PASS' if good else 'FAIL'}] EUSER ord 915 bound ({len(b915)} images)")
    for o, desc in ((1793, "RThread::Id"), (1849, "RProcess::Id")):
        got = [ln for ln in binds if f"ord {o} " in ln]
        ok &= bool(got)
        print(f"  [{'PASS' if got else 'FAIL'}] EUSER ord {o} {desc} bound "
              f"({len(got)} images)")
    for label, hit, clean in evidence:
        ok &= hit and clean
        print(f"  [{'PASS' if hit and clean else 'FAIL'}] command {label} "
              f"(marker={'PASS' if hit else 'FAIL'}, fault-free={'PASS' if clean else 'FAIL'})")
    good = len(evidence) == len(COMMANDS)
    ok &= good
    print(f"  [{'PASS' if good else 'FAIL'}] all {len(COMMANDS)} commands exercised "
          f"({len(evidence)} delivered)")
    exc = [ln for ln in text.splitlines() if any(k in ln for k in EXC_KEYS)]
    good = not exc
    ok &= good
    print(f"  [{'PASS' if good else 'FAIL'}] no Exc/PanicKernExec/FAULT/#PF")
    for ln in exc[:8]:
        print("    " + ln)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-rebuild-disk", action="store_true")
    ap.add_argument("--log", default=LOG_DEFAULT)
    args = ap.parse_args()
    ok = validate_inputs()
    if not args.no_rebuild_disk:
        build_disk()
    evidence = []
    try:
        evidence = run_qemu(args.log) or []
    except RuntimeError as exc:
        print("QEMU drive failed:", exc)
        ok = False
    text = open(args.log, "r", errors="replace").read()
    ok &= check(text, evidence)
    print(f"[{'PASS' if ok else 'FAIL'}] r1_genuine_eshell_test (log {args.log})")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
