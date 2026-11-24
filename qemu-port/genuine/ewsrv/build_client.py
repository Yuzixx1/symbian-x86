#!/usr/bin/env python3
"""Build the GENUINE Symbian text window server (EWSRV) as a PE2 E32.

Compiles unmodified kernelhwsrv EWSRV/econs/ektran/edisp sources with -m32,
statically into one image that imports only the genuine EUSER.DLL (2503
exports).  Patched copies of three files (QEMU LFB display driver, direct
screen-driver factory, startup markers) live in genuine/ewsrv/patched/.
kernelhwsrv/ is never written.
"""
import os
import re
import shlex
import struct
import subprocess
import sys
from pathlib import Path

PORT = Path(__file__).resolve().parents[2]
REPO = PORT.parent
EKA = REPO / "kernelhwsrv" / "kernel" / "eka"
KH = REPO / "kernelhwsrv"
HERE = Path(__file__).resolve().parent
BUILD = HERE / "obj"
OUT = PORT / "genuine" / "build"
SYS = sys.argv[1:] or []

DEFS = ["__GCC32__", "__X86__", "__EPOC32__", "_UNICODE",
        "IMPORT_C=", "EXPORT_C=", "IMPORT_D=", "EXPORT_D=",
        "__NO_THROW=", "TEMPLATE_SPECIALIZATION=template<>",
        "__cdecl=__attribute__((cdecl))", "__fastcall=__attribute__((fastcall))",
        "__stdcall=__attribute__((stdcall))",
        "NONSHARABLE_CLASS(x)=class x", "NONSHARABLE_STRUCT(x)=struct x",
        "_FOFF(c,f)=(((TInt)&(((c*)0x1000)->f))-0x1000)"]
INCS = [EKA / "include", EKA / "include/kernel/x86", EKA / "include/nkern",
        EKA / "common", KH / "userlibandfileserver/domainmgr/inc",
        KH / "userlibandfileserver/fileserver/inc",
        KH / "halservices/hal/inc", PORT / "build/export", PORT / "inc"]
FLAGS = ["-m32", "-march=i486", "-Os", "-ffreestanding", "-nostdlib",
         "-fno-pic", "-fno-pie", "-fno-builtin", "-fno-stack-protector",
         "-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics",
         "-mpreferred-stack-boundary=2", "-fno-common",
         # Match genuine EUSER.DLL's MSVC/Symbian x86 register-return ABI for
         # small structs (TPoint etc.); avoid gcc's hidden-sret arg shift.
         "-freg-struct-return",
          # 16-bit wchar_t so _L("!E32WindowServer") is UTF-16 like prebuilt
          # EUSER.DLL's 2-byte TText (otherwise the name reads as "!" and
          # CreateSession returns KErrNotFound for correct UTF-16 clients).
          "-fshort-wchar", "-w"]

SOURCES = [
    HERE / "patched/co_cli.cpp",
    EKA / "ewsrv/co_twin.cpp",
    EKA / "ewsrv/co_utl.cpp",
    HERE / "ewsrv_shims.cpp",
    HERE / "ewsrv_client.cpp",
    HERE / "stubs.S",
]


def run(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def compile_one(src, out):
    cmd = ["g++"] + FLAGS
    for d in DEFS:
        cmd += ["-D" + d]
    for i in INCS:
        cmd += ["-I" + str(i)]
    cmd += ["-c", str(src), "-o", str(out)]
    r = run(*cmd)
    if r.returncode:
        raise SystemExit(f"compile failed: {src}")


def main():
    BUILD.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    objs = []
    for src in SOURCES:
        obj = BUILD / (src.stem + ".o")
        if src.suffix == ".S":
            r = run("gcc", "-m32", "-c", str(src), "-o", str(obj))
            if r.returncode:
                raise SystemExit(f"asm failed: {src}")
        else:
            compile_one(src, obj)
        objs.append(obj)
    start = PORT / "user/start.S"
    start_obj = BUILD / "start.o"
    if not start_obj.exists() or start.stat().st_mtime > start_obj.stat().st_mtime:
        r = run("gcc", "-m32", "-c", str(start), "-o", str(start_obj))
        if r.returncode:
            raise SystemExit("start.S compile failed")
    objs.insert(0, start_obj)

    link = ["ld", "-m", "elf_i386", "--emit-relocs",
            "-T", str(HERE / "link_ewsrv.ld"),
            "--unresolved-symbols=ignore-all", "-o", str(BUILD / "ewclient_pass1.elf")]
    link += [str(o) for o in objs]
    r = run(*link)
    if r.returncode:
        raise SystemExit("pass1 link failed")
    # .text size from pass1 ELF section header
    blob = (BUILD / "ewclient_pass1.elf").read_bytes()
    shoff = struct.unpack_from('<I', blob, 32)[0]
    shnum, shstr = struct.unpack_from('<HH', blob, 48)
    sections = [struct.unpack_from('<10I', blob, shoff + i * 40) for i in range(shnum)]
    names = blob[sections[shstr][4]:sections[shstr][4] + sections[shstr][5]]
    text = next(s for s in sections if names[s[0]:].split(b'\0')[0] == b'.text')
    text_size = text[5]
    # Fixed IAT offset: 216 x86 call thunks (~8B each) are added below, so keep
    # a generous gap.  Link asserts final .text < iat_start.
    iat_start = 0xD000
    print(f"pass1 text={text_size:#x} IAT at {iat_start:#x}", flush=True)

    # EUSER symbols needed, with authoritative ordinals from the EABI def.
    defs = {}
    for ln in (EKA / "bx86gcc/euseru.def").read_text(encoding="utf-8", errors="ignore").splitlines():
        m = re.match(r'\s*(\S+)\s+@\s*(\d+)', ln)
        if m:
            defs[m.group(1)] = int(m.group(2))
    nm = subprocess.run(["nm", "-u", str(BUILD / "ewclient_pass1.elf")],
                        capture_output=True, text=True).stdout
    undef = sorted({ln.split()[-1] for ln in nm.splitlines() if ln.strip()})
    local_stubs = {"_ZN4Exec10HeapSwitchEP10RAllocator"}  # defined in stubs.S
    undef = [s for s in undef if s not in local_stubs]
    needed = [s for s in undef if s in defs]
    unknown = [s for s in undef if s not in defs]
    print(f"EUSER imports={len(needed)} unknown={unknown}", flush=True)
    if unknown:
        raise SystemExit("unresolved non-EUSER symbols: " + ", ".join(unknown[:12]))

    # Generate one call thunk per EUSER import.  Genuine code calls the thunk
    # normally (R_386_PC32); the thunk jumps through the PE2 IAT slot (R_386_32
    # against an ABS symbol), which the loader overwrites with the real export.
    thunks = BUILD / "euser_thunks.S"
    with thunks.open("w") as f:
        f.write("/* Generated by build_ewsrv.py: EUSER PE2 import thunks. */\n")
        f.write(".section .text\n")
        for k, sym in enumerate(needed):
            f.write(f".globl {sym}\n.type {sym}, @function\n{sym}:\n")
            f.write(f"    jmp *{sym}_slot\n.size {sym}, .-{sym}\n")
        f.write('.section .note.GNU-stack,"",@progbits\n')
    thunk_obj = BUILD / "euser_thunks.o"
    r = run("gcc", "-m32", "-c", str(thunks), "-o", str(thunk_obj))
    if r.returncode:
        raise SystemExit("thunk asm failed")
    objs.append(thunk_obj)

    link2 = ["ld", "-m", "elf_i386", "--emit-relocs",
             "-T", str(HERE / "link_ewsrv.ld"), "-o", str(BUILD / "ewclient.elf")]
    for k, sym in enumerate(needed):
        link2.append(f"--defsym={sym}_slot=0x{0x400000 + iat_start + 4*k:x}")
    link2 += [str(o) for o in objs]
    r = run(*link2)
    if r.returncode:
        raise SystemExit("final link failed")

    ords = [defs[s] for s in needed]
    e32 = OUT / "EWCLIENT.E32"
    r = run("python3", PORT / "genuine/pack_multi.py",
            BUILD / "ewclient.elf", e32,
            "--iat-start", hex(iat_start),
            "--ordinals", ",".join(str(o) for o in ords))
    if r.returncode:
        raise SystemExit("pack failed")
    r = run("python3", PORT / "tools/e32read.py", e32, "--quiet")
    if r.returncode:
        raise SystemExit("E32 validation failed")
    print(f"RESULT: {e32} ({e32.stat().st_size} bytes, {len(ords)} EUSER imports)")


if __name__ == "__main__":
    main()
