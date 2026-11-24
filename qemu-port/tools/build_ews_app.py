#!/usr/bin/env python3
"""Build a migrated user/ app (EWCLIENT pattern) as a PE2 E32.

Usage: python3 tools/build_ews_app.py <app_name> <output.E32>
   e.g. python3 tools/build_ews_app.py shot_e32 build/user/SHOTE32.E32

Generalization of genuine/ewsrv/build_client.py (the proven EWCLIENT builder)
and of the one-off SHOT build: compiles the genuine EWSRV client stubs
(patched co_cli.cpp + co_twin/co_utl), the port shims (ewsrv_shims.cpp,
stubs.S), user/start.S, the plain-C port runtime (user/runtime.c, for
file_save), and user/<app_name>.c as C++ (-x c++), then links in two passes:
pass1 with --unresolved-symbols=ignore-all, nm -u to collect the EUSER
imports, one call thunk per import, final link with per-symbol <sym>_slot
defsyms just past .text, and pack with genuine/pack_multi.py using the
authoritative ordinals from kernelhwsrv euseru.def.  kernelhwsrv/ is only
read, never written.
"""
import re
import shlex
import struct
import subprocess
import sys
from pathlib import Path

PORT = Path(__file__).resolve().parents[1]
REPO = PORT.parent
EKA = REPO / "kernelhwsrv" / "kernel" / "eka"
KH = REPO / "kernelhwsrv"
EWS = PORT / "genuine" / "ewsrv"

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
        KH / "halservices/hal/inc", PORT / "build/export", PORT / "inc",
        PORT / "variant"]
FLAGS = ["-m32", "-march=i486", "-Os", "-ffreestanding", "-nostdlib",
         "-fno-pic", "-fno-pie", "-fno-builtin", "-fno-stack-protector",
         "-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics",
         "-mpreferred-stack-boundary=2", "-fno-common",
         # Match genuine EUSER.DLL's MSVC/Symbian x86 register-return ABI for
         # small structs (TPoint etc.); avoid gcc's hidden-sret arg shift.
         "-freg-struct-return", "-w"]

# Plain-C port runtime flags (same shape as user/user.mk USER_FLAGS).
C_FLAGS = ["-m32", "-march=i486", "-Os", "-ffreestanding", "-fno-pic",
           "-fno-pie", "-fno-builtin", "-fno-stack-protector",
           "-fno-exceptions", "-fno-asynchronous-unwind-tables",
           "-fno-unwind-tables", "-mpreferred-stack-boundary=2",
           "-fno-rtti", "-I" + str(PORT / "user"), "-w"]


def run(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def compile_cpp(src, out):
    cmd = ["g++"] + FLAGS
    for d in DEFS:
        cmd += ["-D" + d]
    for i in INCS:
        cmd += ["-I" + str(i)]
    cmd += ["-x", "c++", "-c", str(src), "-o", str(out)]
    r = run(*cmd)
    if r.returncode:
        raise SystemExit(f"compile failed: {src}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    app, out = sys.argv[1], Path(sys.argv[2])
    app_src = PORT / "user" / (app + ".c")
    if not app_src.exists():
        raise SystemExit(f"no such app source: {app_src}")
    BUILD = PORT / "build" / "ews_app" / app
    BUILD.mkdir(parents=True, exist_ok=True)
    out.parent.mkdir(parents=True, exist_ok=True)
    LIBGCC = subprocess.run(["gcc", "-m32", "-print-libgcc-file-name"],
                            capture_output=True, text=True).stdout.strip()

    cpp_sources = [
        EWS / "patched/co_cli.cpp",   # genuine RConsole client stubs
        EKA / "ewsrv/co_twin.cpp",
        EKA / "ewsrv/co_utl.cpp",
        EWS / "ewsrv_shims.cpp",      # port heap + operator new + RFs stubs
        app_src,                      # the app (C++ via -x c++)
    ]
    c_sources = [PORT / "user/runtime.c"]   # file_save and friends
    asm_sources = [EWS / "stubs.S", PORT / "user/start.S"]

    objs = []
    for src in cpp_sources:
        obj = BUILD / (src.stem + ".o")
        compile_cpp(src, obj)
        objs.append(obj)
    for src in c_sources:
        obj = BUILD / (src.stem + ".o")
        r = run("gcc", *C_FLAGS, "-c", str(src), "-o", str(obj))
        if r.returncode:
            raise SystemExit(f"compile failed: {src}")
        objs.append(obj)
    for src in asm_sources:
        obj = BUILD / (src.stem + ".o")
        r = run("gcc", "-m32", "-c", str(src), "-o", str(obj))
        if r.returncode:
            raise SystemExit(f"asm failed: {src}")
        objs.insert(0, obj)

    pass1 = BUILD / (app + "_pass1.elf")
    link = ["ld", "-m", "elf_i386", "--emit-relocs",
            "-T", str(EWS / "link_ewsrv.ld"),
            "--unresolved-symbols=ignore-all", "-o", str(pass1)]
    link += [str(o) for o in objs] + [LIBGCC]
    r = run(*link)
    if r.returncode:
        raise SystemExit("pass1 link failed")

    # .text size from pass1 ELF section header; IAT goes just past it.
    blob = pass1.read_bytes()
    shoff = struct.unpack_from('<I', blob, 32)[0]
    shnum, shstr = struct.unpack_from('<HH', blob, 48)
    sections = [struct.unpack_from('<10I', blob, shoff + i * 40) for i in range(shnum)]
    names = blob[sections[shstr][4]:sections[shstr][4] + sections[shstr][5]]
    text = next(s for s in sections if names[s[0]:].split(b'\0')[0] == b'.text')
    text_size = text[5]
    iat_start = max(0xD000, (text_size + 0xFFF) & ~0xFFF)
    print(f"pass1 text={text_size:#x} IAT at {iat_start:#x}", flush=True)

    # EUSER symbols needed, with authoritative ordinals from the EABI def.
    defs = {}
    for ln in (EKA / "bx86gcc/euseru.def").read_text(encoding="utf-8", errors="ignore").splitlines():
        m = re.match(r'\s*(\S+)\s+@\s*(\d+)', ln)
        if m:
            defs[m.group(1)] = int(m.group(2))
    nm = subprocess.run(["nm", "-u", str(pass1)],
                        capture_output=True, text=True).stdout
    undef = sorted({ln.split()[-1] for ln in nm.splitlines() if ln.strip()})
    local_stubs = {"_ZN4Exec10HeapSwitchEP10RAllocator"}  # defined in stubs.S
    undef = [s for s in undef if s not in local_stubs]
    needed = [s for s in undef if s in defs]
    unknown = [s for s in undef if s not in defs]
    print(f"EUSER imports={len(needed)} unknown={unknown}", flush=True)
    if unknown:
        raise SystemExit("unresolved non-EUSER symbols: " + ", ".join(unknown[:12]))

    # One call thunk per EUSER import: genuine code calls the thunk normally
    # (R_386_PC32); the thunk jumps through the PE2 IAT slot (R_386_32 against
    # an ABS symbol) which the loader overwrites with the real export.
    thunks = BUILD / "euser_thunks.S"
    with thunks.open("w") as f:
        f.write(f"/* Generated by tools/build_ews_app.py: EUSER PE2 import thunks for {app}. */\n")
        f.write(".section .text\n")
        for sym in needed:
            f.write(f".globl {sym}\n.type {sym}, @function\n{sym}:\n")
            f.write(f"    jmp *{sym}_slot\n.size {sym}, .-{sym}\n")
        f.write('.section .note.GNU-stack,"",@progbits\n')
    thunk_obj = BUILD / "euser_thunks.o"
    r = run("gcc", "-m32", "-c", str(thunks), "-o", str(thunk_obj))
    if r.returncode:
        raise SystemExit("thunk asm failed")
    objs.append(thunk_obj)

    final = BUILD / (app + ".elf")
    link2 = ["ld", "-m", "elf_i386", "--emit-relocs",
             "-T", str(EWS / "link_ewsrv.ld"), "-o", str(final)]
    for k, sym in enumerate(needed):
        link2.append(f"--defsym={sym}_slot=0x{0x400000 + iat_start + 4*k:x}")
    link2 += [str(o) for o in objs] + [LIBGCC]
    r = run(*link2)
    if r.returncode:
        raise SystemExit("final link failed")

    ords = [defs[s] for s in needed]
    r = run("python3", PORT / "genuine/pack_multi.py",
            final, out,
            "--iat-start", hex(iat_start),
            "--ordinals", ",".join(str(o) for o in ords))
    if r.returncode:
        raise SystemExit("pack failed")
    r = run("python3", PORT / "tools/e32read.py", out, "--quiet")
    if r.returncode:
        raise SystemExit("E32 validation failed")
    with open(BUILD / "imports.txt", "w") as f:
        for s, o in zip(needed, ords):
            f.write(f"{o} {s}\n")
    print(f"RESULT: {out} ({out.stat().st_size} bytes, {len(ords)} EUSER imports)")
    print("ordinals:", ",".join(str(o) for o in ords))


if __name__ == "__main__":
    main()
