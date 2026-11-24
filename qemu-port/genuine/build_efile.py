#!/usr/bin/env python3
"""Build the FULL genuine EFILE.EXE (!FileServer) PE2 E32.

Links the 48-object genuine sfile/EFILE.A archive together with the genuine
F32.A and EFSRV.A client archives (all compiled from read-only kernelhwsrv/
sources by f32_efsrv_build.py).  Port glue lives in genuine/efile/ only.

Every EUSER symbol the image needs becomes a generated call thunk jumping
through a PE2 IAT slot; tools/genuine_build.py compiles the glue/thunks and
tools/pack_multi.py writes the final E32.  Unlike tools/elf2e32.py (one import)
this emits all N EUSER imports in one DLL dependency, raising the reader/loader
caps coherently (already 512 in tools/e32read.py and variant/e32load.cpp).

Output: genuine/build/EFILE.elf + genuine/build/EFILE.E32 (e32read validated).
"""
import os
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PORT = HERE.parent
REPO = PORT.parent
KH = REPO / "kernelhwsrv"
FS = KH / "userlibandfileserver" / "fileserver"
SDK = Path("/home/yuzi/.cache/sybian-r1/r1-build-v1/epoc32/include")
BUILD = PORT / "genuine" / "build"
OBJ = HERE / "efile" / "obj_build"
DEF = KH / "kernel" / "eka" / "bx86gcc" / "euseru.def"
IAT_START = 0x40000
# Provided locally by efile_glue.cpp (see SetCritical shim rationale there).
SKIP_IMPORTS = {"_ZN4User11SetCriticalENS_9TCriticalE",
                "_ZN4User5PanicERK7TDesC16i"}

sys.path.insert(0, str(PORT / "tools"))
import genuine_build as gb  # noqa: E402  (compile_one shared with the toolchain)

CDEFS = ["__GCC32__", "__X86__", "__EPOC32__", "__GENERIC_X86__", "_UNICODE"]
INCS = [str(SDK / "variant"), str(SDK), str(SDK / "platform"),
        str(FS / "group"), str(FS / "inc"),
        str(KH / "kernel" / "eka" / "include")]


def run(*args, **kw):
    print("+", " ".join(str(a) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], check=True, **kw)


def euser_ordinals():
    ords = {}
    for line in DEF.read_text(errors="replace").splitlines():
        m = re.match(r"\s*(\S+)\s+@\s*(\d+)", line)
        if m:
            ords[m.group(1)] = int(m.group(2))
    return ords


def sh(*args, **kw):
    out = subprocess.run([str(a) for a in args], capture_output=True, text=True, **kw)
    return out


def main():
    if not (HERE / "EFILE.A").exists():
        sys.exit("run python3 genuine/f32_efsrv_build.py --rebuild first")
    if OBJ.exists():
        shutil.rmtree(OBJ)
    OBJ.mkdir(parents=True)
    BUILD.mkdir(parents=True, exist_ok=True)

    # 48 genuine sfile objects; sf_notifier.o is #ifdef'd out of efile.mmh
    # (SYMBIAN_F32_ENHANCED_CHANGE_NOTIFICATION), including it duplicates
    # CFsNotifyRequest / FsNotificationManager symbols from the handlers TU.
    efile_objs = [p for p in sorted((HERE / "obj" / "EFILE").glob("*.o"))
                  if p.name != "sf_notifier.o"]
    if len(efile_objs) != 47:
        sys.exit(f"expected 47 usable EFILE objs (48 minus sf_notifier), got {len(efile_objs)}")
    core = [str(p) for p in efile_objs] + [str(HERE / "F32.A"), str(HERE / "EFSRV.A")]

    # Compiler flags for the genuine-source copies under genuine/efile/patched/.
    sdk, fs = str(SDK), str(FS)
    PATCH_FLAGS = ["gcc", "-m32", "-Os", "-ffreestanding", "-fno-exceptions",
                   "-fno-rtti", "-fpermissive", "-w", "-fno-pic", "-fno-pie",
                   "-fno-stack-protector", "-freg-struct-return", "-fshort-wchar",
                   "-D__GCC32__", "-D__X86__",
                   "-D__EPOC32__", "-D__GENERIC_X86__", "-D_UNICODE",
                   f"-include{sdk}/gcc.h",
                   f"-include{sdk}/variant/port_x86_rfile64_v1.hrh",
                   f"-I{sdk}/variant", f"-I{sdk}", f"-I{sdk}/platform",
                   f"-I{fs}/group", f"-I{fs}/inc", f"-I{fs}/sfile",
                   f"-I{fs}/sfile/traces", f"-I{fs}/sfsrv", f"-I{fs}/sfsrv/traces",
                   f"-I{fs}/ftrace", f"-I{fs}/fs_utils", f"-I{fs}/srom",
                   f"-I{KH / 'kernel/eka/include'}", f"-I{KH / 'kernel/eka/kernel'}",
                   f"-DSYMBIAN_PRIVATE_EFSRV"]

    # x86 port shim (ALWAYS, not just diagnostics): the genuine
    # FsPluginManager plugin-chain lock uses the prebuilt EUSER.DLL
    # RReadWriteLock, which deadlocks in this port (its reader semaphore
    # never signals as no plugin is ever mounted).  Our patched copy of the
    # genuine sf_request.cpp replaces the ReadLockChain/UnlockChain calls
    # with a no-op; the plugin chain is always empty on this x86 port.
    # kernelhwsrv/ is never modified.  See docs/EFILE_REENTRY_SUPPRESS.md.
    shim_req = OBJ / "sf_request_shim.o"
    run(*(PATCH_FLAGS + ["-c", str(HERE / "efile/patched/sf_request.cpp"),
                        "-o", str(shim_req)]))
    core = [str(shim_req) if p.endswith("/sf_request.o") else p for p in core]

    if os.environ.get("EFILE_DEBUG_MAIN"):
        # Diagnostic only: instrumented copy of sf_main.cpp under genuine/efile/.
        patched = OBJ / "sf_main_debug.o"
        run(*(PATCH_FLAGS + ["-c", str(HERE / "efile/patched/sf_main.cpp"),
                            "-o", str(patched)]))
        core = [str(patched) if p.endswith("/sf_main.o") else p for p in core]

    # Port glue, compiled with the shared genuine_build.py compiler helper.
    glue = OBJ / "efile_glue.o"
    stubs = OBJ / "port_stubs.o"
    start = OBJ / "start.o"
    gb.compile_one(HERE / "efile" / "efile_glue.cpp", glue, [Path(p) for p in INCS], CDEFS)
    gb.compile_one(HERE / "efile" / "port_stubs.S", stubs, [Path(p) for p in INCS], CDEFS)
    gb.compile_one(PORT / "user" / "start.S", start, [Path(p) for p in INCS], CDEFS)
    libgcc = sh("gcc", "-m32", "-print-libgcc-file-name").stdout.strip()
    script = str(HERE / "efile" / "link_efile.ld")
    base = core + [str(start), str(glue), str(stubs), libgcc]

    # Pass 1: discover every undefined EUSER symbol (no thunks yet).
    p1 = OBJ / "efile_pass1.elf"
    run("ld", "-m", "elf_i386", "--emit-relocs", "-T", script,
        "--unresolved-symbols=ignore-all", "-o", p1, *base)
    undef_out = sh("nm", "-u", p1).stdout
    undef = {ln.split()[-1] for ln in undef_out.splitlines() if " U " in ln or " w " in ln}
    ords = euser_ordinals()
    needed = sorted(s for s in undef if s in ords and s not in SKIP_IMPORTS)
    missing = sorted(s for s in undef if s not in ords)
    if missing:
        sys.exit("unresolved non-EUSER symbols (need a port shim): " + ", ".join(missing[:20]))

    # One jmp-thunk per EUSER import, each bound to an ABS IAT slot symbol.
    thunk_src = OBJ / "euser_thunks.S"
    with thunk_src.open("w") as f:
        f.write("/* Generated by build_efile.py: EUSER PE2 import thunks. */\n")
        f.write(".section .text\n")
        for sym in needed:
            f.write(f".globl {sym}\n.type {sym}, @function\n{sym}:\n")
            f.write(f"    jmp *{sym}_slot\n.size {sym}, .-{sym}\n")
        f.write('.section .note.GNU-stack,"",@progbits\n')
    thunks = OBJ / "euser_thunks.o"
    gb.compile_one(thunk_src, thunks, [Path(p) for p in INCS], CDEFS)

    # Pass 2: final link + PE2 pack (all ordinals in one EUSER dependency).
    elf = BUILD / "EFILE.elf"
    ld = ["ld", "-m", "elf_i386", "--emit-relocs", "-T", script, "-o", elf]
    blob = p1.read_bytes()
    shoff, = struct.unpack_from("<I", blob, 32)
    shnum, shstr = struct.unpack_from("<HH", blob, 48)
    secs = [struct.unpack_from("<10I", blob, shoff + i * 40) for i in range(shnum)]
    names = blob[secs[shstr][4]:secs[shstr][4] + secs[shstr][5]]
    text_size = next(s[5] for s in secs
                     if names[s[0]:].split(b"\0", 1)[0] == b".text")
    if IAT_START < text_size:
        sys.exit(f"IAT {IAT_START:#x} below pass1 .text {text_size:#x}")
    for k, sym in enumerate(needed):
        ld.append(f"--defsym={sym}_slot={0x400000 + IAT_START + 4 * k:#x}")
    ld += core + [str(start), str(glue), str(stubs), str(thunks), libgcc]
    run(*ld)

    e32 = BUILD / "EFILE.E32"
    run(sys.executable, PORT / "genuine" / "pack_multi.py", elf, e32,
        "--iat-start", hex(IAT_START),
        "--ordinals", ",".join(str(ords[s]) for s in needed))
    run(sys.executable, PORT / "tools" / "e32read.py", e32, "--quiet")
    print(f"RESULT: {e32} ({e32.stat().st_size} bytes, {len(needed)} EUSER imports, "
          f"IAT {IAT_START:#x}), e32read OK")


if __name__ == "__main__":
    main()
