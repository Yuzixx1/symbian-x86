#!/usr/bin/env python3
"""build_estart_eshell.py -- build the GENUINE Symbian ESTART and ESHELL as PE2 E32.

kernelhwsrv/ is READ-ONLY: the genuine sources are compiled unmodified from
  fileserver/estart/estart.cpp + estartmain.cpp      (estart.mmp)
  fileserver/etshell/ts_*.cpp + sfile/sf_inflate.cpp + sf_decomp.cpp (eshell.mmp)
with the define/include set from tools/genuine_build.py (see genuine/README.md),
extended with the fileserver/hal/domainmgr include dirs.  Output goes to
qemu-port/genuine/build/estart_.E32 and etshell_.E32 (prefixed so the parallel
efsrv/ewsrv builders do not clash).  No make clean, no writes to kernelhwsrv.

Link strategy (same as genuine/ewsrv/build_ewsrv.py):
  pass 1 links with --unresolved-symbols=ignore-all; `nm -u` lists the imports.
  * symbols present in kernelhwsrv/kernel/eka/bx86gcc/euseru.def (the .def of
    the STAGED genuine EUSER.DLL -- RDebug::Printf is ord 915 there) become
    `jmp *<sym>_slot` thunks; the slots are absolute symbols at
    0x400000+iat_start+4k and pack_multi.py writes the ordinals into the PE2
    IAT, which the kernel loader overwrites with the real EUSER exports.
  * everything else (EFSRV.DLL / HAL.DLL / DOMAINCLI.DLL client symbols not
    hand-covered by shims.cpp) becomes a generated `xor %eax,%eax; ret` stub in
    obj/stubs_<name>.S.  Every stub symbol is printed at build time.
Hand-written shims (console over PortCon + EUSER ord 915, RFs/HAL/DM critical
path, operator new via User::Alloc) live in shims.cpp and are documented there.

Usage: python3 genuine/estart_eshell/build_estart_eshell.py [estart_|etshell_|all]
"""
import re
import shlex
import struct
import subprocess
import sys
from pathlib import Path

PORT = Path(__file__).resolve().parents[2]
REPO = PORT.parent
KH = REPO / "kernelhwsrv"
FS = KH / "userlibandfileserver" / "fileserver"
EKA = KH / "kernel" / "eka"
HERE = Path(__file__).resolve().parent
BUILD = PORT / "genuine" / "build"
OBJ = HERE / "obj"
TOOLS = PORT / "tools"

sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(PORT / "genuine"))
from pack_multi import pack as pack_multi      # noqa: E402

# Define/include set from tools/genuine_build.py + genuine/README.md (kept
# local: the shared tool file is being extended by a parallel builder).
GENUINE_DEFINES = [
    "__GCC32__", "__X86__", "__EPOC32__", "_UNICODE",
    "IMPORT_C=", "EXPORT_C=", "IMPORT_D=", "EXPORT_D=",
    "__NO_THROW=", "TEMPLATE_SPECIALIZATION=template<>",
    "__cdecl=__attribute__((cdecl))",
    "__fastcall=__attribute__((fastcall))",
    "__stdcall=__attribute__((stdcall))",
    "NONSHARABLE_CLASS(x)=class x",
    "NONSHARABLE_STRUCT(x)=struct x",
    "_FOFF(c,f)=((TInt)__builtin_offsetof(c,f))",
]
GENUINE_INCLUDES = [
    EKA / "include", EKA / "euser", EKA / "euser" / "epoc", EKA / "euser" / "maths",
    PORT / "build" / "export", PORT / "inc",
]
COMPILE_FLAGS = [
    "-m32", "-march=i486", "-mtune=generic", "-Os",
    "-ffreestanding", "-fno-pic", "-fno-pie", "-fno-builtin",
    "-fno-stack-protector", "-fno-exceptions", "-fno-rtti",
    "-fno-threadsafe-statics", "-fno-asynchronous-unwind-tables",
    "-fno-unwind-tables", "-mpreferred-stack-boundary=2", "-fno-common",
    "-fshort-wchar",   # genuine _L() casts L"..." (wchar_t) to TText*; Symbian
                       # wchar_t is 16-bit, Linux gcc's is 32-bit unless this
    # Genuine EUSER (MSVC) returns <=8-byte structs/classes (TThreadId,
    # TProcessId, TPtrC16, TTimeIntervalMicroSeconds, ...) in EAX/EDX:EAX;
    # gcc -m32 defaults to the Itanium hidden-sret pointer (pushed first),
    # which shifts every real arg and faults (see docs/ABI_AUDIT.md).
    "-freg-struct-return",
    "-w",
]

DEFS = GENUINE_DEFINES
INCS = list(GENUINE_INCLUDES) + [
    FS / "inc", FS / "group", FS / "estart", FS / "etshell", FS / "sfile",
    FS / "sfat32", FS / "sfat32" / "inc", FS / "sfsrv", FS / "sfsrv" / "traces",
    KH / "userlibandfileserver" / "domainmgr" / "inc",
    KH / "halservices" / "hal" / "inc",
]
FLAGS = COMPILE_FLAGS
LINK_SCRIPT = HERE / "link_exe.ld"
EUSER_DEF = EKA / "bx86gcc" / "euseru.def"
CODE_BASE = 0x400000

# Per-image genuine UID3s.  pack_multi.py defaults to 0x100000bf (the genuine
# EFILE UID), which the E32Run single-owner guard claims exclusively for the
# file server.  ESTART/ESHELL must carry their real UIDs so they are not
# refused as a second EFILE image (KErrAlreadyExists, -11).  dbg2_/dbg3_ are
# debug-only test helpers; give them distinct non-colliding UIDs.
UID3 = {
    "estart_":    0x10272C04,   # genuine ESTART UID3
    "etshell_":   0x101F7AE7,   # genuine ESHELL UID3
    "etshell_dbg": 0x101F7AE7,  # instrumented ESHELL: same genuine UID3
    "dbg2_":      0x10000112,   # debug-only EUSER-export bisect helper
    "dbg3_":      0x10000113,   # debug-only console-shim helper
}

COMPONENTS = {
    "estart_": dict(
        tag="ESTART",
        sources=[FS / "estart" / "estart.cpp", FS / "estart" / "estartmain.cpp",
                 HERE / "glue.cpp", HERE / "shims.cpp"],
    ),
    "etshell_": dict(
        tag="ESHELL",
        sources=[FS / "etshell" / f for f in
                 ("ts_com.cpp", "ts_data.cpp", "ts_edshl.cpp", "ts_main.cpp",
                  "ts_utl.cpp", "ts_deps.cpp", "ts_clicomp.cpp")]
                + [FS / "sfile" / "sf_inflate.cpp", FS / "sfile" / "sf_decomp.cpp",
                   HERE / "glue.cpp", HERE / "shims.cpp"],
    ),
    # debug-only variant: same sources as etshell_ but ts_main.cpp replaced by
    # dbg_main.cpp (instrumented E32Main) to locate a fault step by step.
    "etshell_dbg": dict(
        tag="ESHELLDBG",
        sources=[FS / "etshell" / f for f in
                 ("ts_com.cpp", "ts_data.cpp", "ts_edshl.cpp",
                  "ts_utl.cpp", "ts_deps.cpp", "ts_clicomp.cpp")]
                + [FS / "sfile" / "sf_inflate.cpp", FS / "sfile" / "sf_decomp.cpp",
                   HERE / "glue.cpp", HERE / "dbg_main.cpp", HERE / "shims.cpp"],
    ),
    # debug-only: fine-grained EUSER export bisect (no etshell sources at all)
    "dbg2_": dict(
        tag="DBG2",
        sources=[HERE / "glue.cpp", HERE / "dbg2_main.cpp", HERE / "shims.cpp"],
    ),
    # debug-only: console shim in isolation
    "dbg3_": dict(
        tag="DBG3",
        sources=[HERE / "glue.cpp", HERE / "dbg3_main.cpp", HERE / "shims.cpp"],
    ),
}

# libc-style helpers the compiler may emit calls to: real implementations,
# everything else gets a ret-0 stub.
REAL_STUBS_C = r"""
#include <stddef.h>
void* memcpy(void* d, const void* s, size_t n) { unsigned char* a=d; const unsigned char* b=s; while(n--) *a++=*b++; return d; }
void* memmove(void* d, const void* s, size_t n) { unsigned char* a=d; const unsigned char* b=s; if(a<b){while(n--)*a++=*b++;} else {a+=n;b+=n;while(n--)*--a=*--b;} return d; }
void* memset(void* d, int c, size_t n) { unsigned char* a=d; while(n--) *a++=(unsigned char)c; return d; }
int memcmp(const void* a, const void* b, size_t n) { const unsigned char* x=a; const unsigned char* y=b; while(n--){ if(*x!=*y) return *x-*y; ++x; ++y; } return 0; }
size_t strlen(const char* s) { size_t n=0; while(s[n]) ++n; return n; }
"""


def run(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    r = subprocess.run([str(a) for a in args], **kw)
    if kw.get("check") and r.returncode:
        raise SystemExit(f"FAILED: {' '.join(map(str, args))}")
    return r


def compile_one(src, dst, extra_defs=()):
    cc = "g++" if src.suffix in (".cpp", ".cc", ".cxx", ".C") else "gcc"
    cmd = [cc] + FLAGS + [f"-D{d}" for d in (*DEFS, *extra_defs)]
    cmd += [f"-I{i}" for i in INCS] + ["-c", str(src), "-o", str(dst)]
    run(*cmd, check=True)


def parse_euser_def():
    defs = {}
    for ln in EUSER_DEF.read_text(errors="replace").splitlines():
        m = re.match(r"\s*(\S+)\s*@\s*(\d+)", ln)
        if m:
            defs[m.group(1)] = int(m.group(2))
    return defs


def text_size(elf):
    blob = Path(elf).read_bytes()
    shoff, = struct.unpack_from("<I", blob, 32)
    _, shnum, shstr = struct.unpack_from("<HHH", blob, 46)
    sections = [struct.unpack_from("<10I", blob, shoff + i * 40) for i in range(shnum)]
    names = blob[sections[shstr][4]:sections[shstr][4] + sections[shstr][5]]
    for s in sections:
        if names[s[0]:].split(b"\0")[0] == b".text":
            return s[5]
    raise SystemExit("no .text in " + str(elf))


def undefined(elf):
    out = subprocess.run(["nm", "-u", str(elf)], capture_output=True, text=True).stdout
    return sorted({ln.split()[-1] for ln in out.splitlines() if ln.strip()})


def build(name, spec):
    OBJ.mkdir(parents=True, exist_ok=True)
    BUILD.mkdir(parents=True, exist_ok=True)
    objs = []
    for i, src in enumerate(spec["sources"]):
        tag = {"estart_": "ESTART", "etshell_": "ESHELL",
               "etshell_dbg": "ESHELLDBG", "dbg2_": "DBG2",
               "dbg3_": "DBG3"}.get(name, name)
        extra = [f'APP_TAG="{tag}"'] if src.name == "glue.cpp" else []
        dst = OBJ / f"{name}{src.stem}_{i}.o"
        compile_one(src, dst, extra)
        objs.append(dst)
    # port startup shim (same as genuine_build.py EXE path)
    for src in (PORT / "user" / "start.S", PORT / "user" / "runtime.c"):
        dst = OBJ / f"{name}port_{src.stem}.o"
        compile_one(src, dst)
        objs.append(dst)
    # real libc-helper implementations
    real_c = OBJ / f"{name}real_stubs.c"
    real_c.write_text(REAL_STUBS_C)
    compile_one(real_c, OBJ / f"{name}real_stubs.o")
    objs.append(OBJ / f"{name}real_stubs.o")

    libgcc = subprocess.check_output(["gcc", "-m32", "-print-libgcc-file-name"], text=True).strip()

    # -- pass 1: discover undefined symbols ------------------------------------
    pass1 = OBJ / f"{name}pass1.elf"
    run("ld", "-m", "elf_i386", "--emit-relocs", "-T", LINK_SCRIPT,
        "--unresolved-symbols=ignore-all", "-o", pass1, *objs, libgcc, check=True)
    undef = undefined(pass1)
    euser_def = parse_euser_def()
    euser_syms = [s for s in undef if s in euser_def]
    stub_syms = [s for s in undef if s not in euser_def]
    # e32def.h typedefs VA_LIST as TInt8*[1] (mangles "PPa"), while the staged
    # EUSER.DLL was built with VA_LIST = char* (mangles "Pc").  The ABI is the
    # same 4-byte pointer, so map PPa-variants onto the Pc export ordinals.
    remapped = []
    still_stub = []
    for s in stub_syms:
        alt = s.replace("PPa", "Pc")
        if alt != s and alt in euser_def:
            euser_syms.append(s)
            remapped.append((s, alt, euser_def[alt]))
        else:
            still_stub.append(s)
    stub_syms = still_stub
    for s, alt, o in remapped:
        print(f"    remap {s} -> ord {o} ({alt})", flush=True)
    print(f"{name}: {len(undef)} undefined -> {len(euser_syms)} EUSER imports, "
          f"{len(stub_syms)} generated stubs", flush=True)
    for s in stub_syms:
        print(f"    stub {s}", flush=True)

    # -- thunks for EUSER imports + stubs for the rest --------------------------
    thunks = OBJ / f"{name}thunks.S"
    with thunks.open("w") as f:
        f.write(f"/* generated: EUSER PE2 import thunks for {name} */\n.section .text\n")
        for s in euser_syms:
            f.write(f".globl {s}\n.type {s}, @function\n{s}:\n    jmp *{s}_slot\n.size {s}, .-{s}\n")
        f.write('.section .note.GNU-stack,"",@progbits\n')
    run("gcc", "-m32", "-freg-struct-return", "-c", thunks, "-o", OBJ / f"{name}thunks.o", check=True)

    stubs = OBJ / f"{name}stubs.S"
    with stubs.open("w") as f:
        f.write(f"/* generated: ret-0 stubs for non-EUSER imports of {name} "
                f"(EFSRV.DLL/HAL.DLL/DOMAINCLI.DLL client symbols not covered by shims.cpp) */\n.section .text\n")
        for s in stub_syms:
            f.write(f".globl {s}\n.type {s}, @function\n{s}:\n    xor %eax, %eax\n    ret\n.size {s}, .-{s}\n")
        f.write('.section .note.GNU-stack,"",@progbits\n')
    run("gcc", "-m32", "-freg-struct-return", "-c", stubs, "-o", OBJ / f"{name}stubs.o", check=True)

    objs += [OBJ / f"{name}thunks.o", OBJ / f"{name}stubs.o"]

    # -- final link: IAT slots after the code, then pack ------------------------
    t0 = text_size(pass1)
    iat_start = (t0 + 32 * len(euser_syms) + 0xFFF) & ~0xFFF
    elf = BUILD / f"{name}.elf"
    for attempt in range(4):
        cmd = ["ld", "-m", "elf_i386", "--emit-relocs", "-T", LINK_SCRIPT, "-o", elf]
        for k, s in enumerate(euser_syms):
            cmd.append(f"--defsym={s}_slot={CODE_BASE + iat_start + 4 * k:#x}")
        cmd += [str(o) for o in objs] + [libgcc]
        run(*cmd, check=True)
        if text_size(elf) <= iat_start:
            break
        iat_start = (text_size(elf) + 0xFFF) & ~0xFFF
    else:
        raise SystemExit(f"{name}: code does not fit below IAT")

    def ord_of(s):
        if s in euser_def:
            return euser_def[s]
        return euser_def[s.replace("PPa", "Pc")]   # VA_LIST PPa->Pc remap
    ords = [ord_of(s) for s in euser_syms]
    e32 = BUILD / f"{name}.E32"
    blob = pack_multi(elf.read_bytes(), ords, iat_start, uid3=UID3[name])
    e32.write_bytes(blob)
    print(f"{name}: {e32} (uid3={UID3[name]:#x}, {len(blob)} bytes, "
          f"{len(ords)} EUSER imports, IAT {iat_start:#x}, "
          f"ord915={'yes' if 915 in ords else 'NO'})", flush=True)
    run(sys.executable, TOOLS / "e32read.py", e32, "--quiet", check=True)
    print(f"OK: {e32} validated by e32read.py", flush=True)


def main():
    which = sys.argv[1:] or ["all"]
    for name, spec in COMPONENTS.items():
        if which != ["all"] and name not in which:
            continue
        build(name, spec)


if __name__ == "__main__":
    main()
