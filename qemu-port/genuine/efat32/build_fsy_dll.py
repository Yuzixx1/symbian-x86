#!/usr/bin/env python3
"""Build the genuine F32 DLL / FSY images the EFSRV retire gate looks for.

Outputs (all under qemu-port/genuine/build/):
  EFSRV.DLL  -- genuine sfsrv/cl_*.cpp client library, 421 eabi/efsrvu.def
                ordinals, 134+ EUSER PE2 imports.
  EFAT32.FSY -- genuine sfat32/*.cpp + fs_utils FAT32 file system, export
                ordinal 1 = CreateFileSystem (sl_main.cpp).

kernelhwsrv/ is READ-ONLY: every genuine object comes from the archives built
by genuine/f32_efsrv_build.py; this script only reads them.

Strategy (extends build_efile.py with genuine_build.py's DLL export patch):
  1. pass-1 link (--unresolved-symbols=ignore-all) lists undefined symbols;
     EUSER symbols become jmp-thunks bound to PE2 IAT slots (pack_multi.py
     supports N imports), everything else gets a documented ret-0 stub.
  2. final link reserves a .genuine_exp pad inside .text; pack_multi.py emits
     the multi-import PE2 E32; genuine_build.patch_exports(make_dll=True)
     flips UID1/KImageDll and writes the ordinal export directory.
  3. tools/e32read.py validates the result (including exports and NoHoles
     export-desc tail).  No assertion is weakened anywhere.
"""
import re
import shlex
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
GEN = HERE.parent
PORT = GEN.parent
BUILD = GEN / "build"
TOOLS = PORT / "tools"
OBJ = GEN / "obj"
REPO = PORT.parent
KH = REPO / "kernelhwsrv"
FS = KH / "userlibandfileserver" / "fileserver"
EUSER_DEF = KH / "kernel/eka/bx86gcc/euseru.def"
EFSRV_DEF = FS / "eabi/efsrvu.def"
LINK_SCRIPT = HERE / "link_dll_multi.ld"
CODE_BASE = 0x400000
IAT_START = 0x40000           # code must fit below this; IAT follows.
                              # Keep 0x40000 so PE2 slots at 0x440000.. do
                              # NOT overlap the .data load base at 0x500000.
EXPORT_STUB = "efat32_export_stub"

sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(GEN))
import genuine_build as gb          # noqa: E402  (patch_exports, elf_symbols)
from pack_multi import pack as pack_multi  # noqa: E402

CFLAGS = ["-m32", "-Os", "-ffreestanding", "-fno-exceptions", "-fno-rtti",
          "-fpermissive", "-w", "-fno-pic", "-fno-pie", "-fno-stack-protector",
          "-freg-struct-return", "-fshort-wchar"]


def run(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    r = subprocess.run([str(a) for a in args], **kw)
    if kw.get("check") and r.returncode:
        raise SystemExit(f"FAILED ({r.returncode}): {' '.join(map(str, args))}")
    return r


def capture(*args):
    return subprocess.run([str(a) for a in args], capture_output=True, text=True).stdout


def parse_ord_def(path):
    """[(ordinal, name)] from a Symbian .def, sorted by ordinal."""
    out = []
    for ln in path.read_text(errors="replace").splitlines():
        m = re.match(r"\s*(\S+)\s+@\s+(\d+)", ln)
        if m:
            out.append((int(m.group(2)), m.group(1)))
    out.sort()
    if [o for o, _ in out] != list(range(1, len(out) + 1)):
        raise SystemExit(f"{path}: export ordinals are not contiguous 1..N")
    return out


def euser_ordinals():
    return dict((n, o) for n, o in
                ((m.group(1), int(m.group(2))) for m in
                 (re.match(r"\s*(\S+)\s+@\s+(\d+)", ln)
                  for ln in EUSER_DEF.read_text(errors="replace").splitlines()) if m))


def undefined(elf):
    out = capture("nm", "-u", elf)
    return sorted({ln.split()[-1] for ln in out.splitlines() if ln.strip()})


def compile_one(src, dst, cc="gcc"):
    dst.parent.mkdir(parents=True, exist_ok=True)
    run(cc, *CFLAGS, "-c", str(src), "-o", str(dst), check=True)


def text_size(elf):
    blob = Path(elf).read_bytes()
    shoff, = struct.unpack_from("<I", blob, 32)
    _, shnum, shstr = struct.unpack_from("<HHH", blob, 46)
    secs = [struct.unpack_from("<10I", blob, shoff + i * 40) for i in range(shnum)]
    names = blob[secs[shstr][4]:secs[shstr][4] + secs[shstr][5]]
    for s in secs:
        if names[s[0]:].split(b"\0", 1)[0] == b".text":
            return s[5]
    raise SystemExit(f"no .text in {elf}")


def build(name, objdir, exports, export_source):
    print(f"== {name}: {len(exports)} exports from {export_source}", flush=True)
    out = HERE / "obj" / name
    out.mkdir(parents=True, exist_ok=True)
    BUILD.mkdir(parents=True, exist_ok=True)

    glue, start = out / "dll_glue.o", out / "start.o"
    compile_one(HERE / "dll_glue.cpp", glue, "g++")
    compile_one(HERE / "start.S", start)
    objs = sorted(objdir.glob("*.o")) + [glue, start]
    libgcc = capture("gcc", "-m32", "-print-libgcc-file-name").strip()

    # -- pass 1: classify undefined symbols -------------------------------------
    p1 = out / "pass1.elf"
    run("ld", "-m", "elf_i386", "--emit-relocs", "-T", LINK_SCRIPT,
        "--unresolved-symbols=ignore-all", "-o", p1, *objs, libgcc, check=True)
    euser_def = euser_ordinals()
    imports, stubs = [], []
    for sym in undefined(p1):
        if sym in euser_def:
            imports.append((sym, euser_def[sym]))
        elif "PPa" in sym and sym.replace("PPa", "Pc") in euser_def:
            imports.append((sym, euser_def[sym.replace("PPa", "Pc")]))
        else:
            stubs.append(sym)
    print(f"   {len(undefined(p1))} undefined -> {len(imports)} EUSER imports, "
          f"{len(stubs)} ret-0 stubs", flush=True)
    for s in stubs:
        print(f"    stub {s}", flush=True)

    # -- thunks + stubs + export pad --------------------------------------------
    thunks_s = out / "thunks.S"
    with thunks_s.open("w") as f:
        f.write(f"/* generated: EUSER PE2 import thunks for {name} */\n.section .text\n")
        for sym, _ in imports:
            f.write(f".globl {sym}\n.type {sym}, @function\n{sym}:\n")
            f.write(f"    jmp *{sym}_slot\n.size {sym}, .-{sym}\n")
        f.write('.section .note.GNU-stack,"",@progbits\n')
    compile_one(thunks_s, out / "thunks.o")

    stubs_s = out / "stubs.S"
    with stubs_s.open("w") as f:
        f.write(f"/* generated: ret-0 stubs for non-EUSER imports of {name} "
                f"(EFILE/F32 cross-DLL exports, see README.md) */\n.section .text\n")
        for sym in stubs:
            f.write(f".globl {sym}\n.type {sym}, @function\n{sym}:\n")
            f.write(f"    xor %eax, %eax\n    ret\n.size {sym}, .-{sym}\n")
        f.write('.section .note.GNU-stack,"",@progbits\n')
    compile_one(stubs_s, out / "stubs.o")

    pad_s = out / "export_pad.s"
    pad_s.write_text('.section .genuine_exp,"a"\n.balign 4\n'
                     '.global _genuine_export_base\n_genuine_export_base:\n'
                     f'.space {4 * len(exports)}\n')
    compile_one(pad_s, out / "export_pad.o")

    # -- final link: reserve code below IAT_START, then PE2 pack ----------------
    elf = BUILD / f"{name}.elf"
    link_objs = objs + [out / "thunks.o", out / "stubs.o", out / "export_pad.o"]
    iat = IAT_START
    for _ in range(4):
        cmd = ["ld", "-m", "elf_i386", "--emit-relocs", "-T", LINK_SCRIPT,
               "-o", elf]
        for k, (sym, _) in enumerate(imports):
            cmd.append(f"--defsym={sym}_slot={CODE_BASE + iat + 4 * k:#x}")
        run(*cmd, *map(str, link_objs), libgcc, check=True)
        if text_size(elf) <= iat:
            break
        iat = (text_size(elf) + 0xFFF) & ~0xFFF
    else:
        raise SystemExit(f"{name}: code does not fit below IAT")
    print(f"   linked {name}.elf: .text={text_size(elf):#x}, IAT={iat:#x}", flush=True)

    # -- export directory: map .def names to real GCC symbols; stubs for RTTI ---
    symbols = gb.elf_symbols(elf.read_bytes())
    targets = []
    fallback = None
    if symbols.get(EXPORT_STUB, (0, 0))[0]:
        fallback = symbols[EXPORT_STUB][0] - CODE_BASE
    if fallback is None:
        raise SystemExit(f"{name}: {EXPORT_STUB} missing from linked ELF")
    unmapped = []
    for ordinal, sym in exports:
        value = symbols[sym][0] if sym in symbols else 0
        target = value - CODE_BASE
        if not value or not 0 <= target < iat:
            # An undefined symbol has value 0 (e.g. RTTI under -fno-rtti):
            # never emit target 0 -- that would alias the _start stub.
            unmapped.append((ordinal, sym))
            target = fallback
        targets.append(target)
    print(f"   exports mapped to real code: {len(exports) - len(unmapped)}/{len(exports)}; "
          f"{len(unmapped)} -> {EXPORT_STUB}", flush=True)
    for ordinal, sym in unmapped:
        print(f"    unmapped ord {ordinal} {sym} -> export stub", flush=True)

    pad_off = symbols["_genuine_export_base"][0] - CODE_BASE
    if not 0 <= pad_off < iat:
        raise SystemExit(f"{name}: export pad outside .text")

    # -- pack + DLL export patch + validate -------------------------------------
    e32 = BUILD / f"{name}.DLL" if name == "EFSRV" else BUILD / f"{name}.FSY"
    tmp = out / f"{name}_exe.E32"
    tmp.write_bytes(pack_multi(elf.read_bytes(), [o for _, o in imports], iat))
    tmp.replace(e32)
    gb.patch_exports(e32, pad_off, targets, make_dll=True)
    run(sys.executable, TOOLS / "e32read.py", e32, "--quiet", check=True)
    hdr = struct.unpack_from("<IIII", e32.read_bytes()[:160], 0)
    flags, = struct.unpack_from("<I", e32.read_bytes()[:160], 44)
    print(f"OK: {e32} ({e32.stat().st_size} B, uid1={hdr[0]:#x}, "
          f"flags={flags:#x}, {len(imports)} imports, {len(targets)} exports), "
          f"e32read PASS", flush=True)


def main():
    build("EFSRV", OBJ / "EFSRV", parse_ord_def(EFSRV_DEF), EFSRV_DEF)
    build("EFAT32", OBJ / "F32", [(1, "CreateFileSystem")],
          FS / "sfat32/sl_main.cpp:CreateFileSystem")


if __name__ == "__main__":
    main()
