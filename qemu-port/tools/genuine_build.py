#!/usr/bin/env python3
"""genuine_build.py -- compile GENUINE Symbian userspace source into our PE2 E32.

This is build glue only: it feeds unmodified sources from kernelhwsrv/ through
the existing -m32 toolchain and the port's elf2e32.py / e32read.py pipeline.
It never modifies kernelhwsrv/ and never touches the hand-written user/ tree
(except reading user/start.S and user/runtime.c as the port startup shim).

Example (the proven hello proof):
  python3 tools/genuine_build.py \
      genuine/hello/hello.cpp \
      ../kernelhwsrv/kernel/eka/euser/epoc/x86/uc_utl.cpp \
      --name GENUINE --type exe --iat-sym genuine_printf_slot \
      --euser-import 915

Outputs: qemu-port/genuine/build/<NAME>.E32 (validated with e32read.py).
All outputs are confined to genuine/build/: --name must be a single safe
path component (no '/', '..', absolute paths).

Stack size note: every image gets an 8192-byte stack, hardcoded in
tools/elf2e32.py:109 -- it is NOT configurable via this tool or the link
scripts (review issue #6).

Self-check: `--selftest` runs parse_def over the genuine .def files
(kernelhwsrv/.../bx86/efsrvu.def, bx86/euseru.def, bx86gcc/euseru.def) and
synthetic cases, reporting parsed/mapped/failed counts.
"""
import argparse
import os
import re
import shlex
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PORT = HERE.parent
REPO = PORT.parent
EKA = REPO / "kernelhwsrv" / "kernel" / "eka"
BUILD = PORT / "genuine" / "build"
OBJ = BUILD / "obj"
TOOLS = PORT / "tools"

if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
from mke32 import (  # noqa: E402
    crc32_symbian, uid_checksum, KImageCrcInitialiser,
    KDynamicLibraryUidValue, KImageDll,
)

# -- the define set that actually compiles the genuine x86 EKA2 user headers
#    with modern gcc/g++ (verified on gcc 16.2.1, see genuine/README.md).
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
    EKA / "include",
    EKA / "euser",
    EKA / "euser" / "epoc",
    EKA / "euser" / "maths",
    PORT / "build" / "export",   # generated exec_enum.h used by u32exec.h
    PORT / "inc",                # port prefix shims (never shadow e32*.h)
    # NOTE: qemu-port/user is deliberately NOT here: its hand-written e32base.h
    # / e32cmn.h would shadow the genuine headers we are trying to compile.
]

COMPILE_FLAGS = [
    "-m32", "-march=i486", "-mtune=generic", "-Os",
    "-ffreestanding", "-fno-pic", "-fno-pie", "-fno-builtin",
    "-fno-stack-protector", "-fno-exceptions", "-fno-rtti",
    "-fno-threadsafe-statics", "-fno-asynchronous-unwind-tables",
    "-fno-unwind-tables", "-mpreferred-stack-boundary=2", "-fno-common",
    # Genuine EUSER.DLL (prebuilt, MSVC/Symbian x86 ABI) returns small structs
    # (e.g. TThreadId from RThread::Id()) in EDX:EAX, not the Itanium-ABI sret
    # hidden pointer that gcc -m32 defaults to. sret on the EUSER side of a call
    # shifts args by one slot and faults with KERN 4. Force register returns.
    "-freg-struct-return",
    # Genuine _L() literals are L"..." (wchar_t) cast to TText*: Symbian
    # wchar_t is 16-bit, Linux gcc's is 32-bit unless -fshort-wchar. Without
    # it _LIT strings are UTF-32 (interleaved NULs) and EUSER's 16-bit CServer2
    # name comparison fails with KErrBadName(-28) on "!FileServer".
    "-fshort-wchar",
    "-w",  # genuine headers are full of -Wvolatile/-Wreturn-local-addr noise
]

IAT_OFF_DEFAULT = 0x8000
CODE_BASE = 0x400000
LINK_SCRIPTS = {
    "exe": PORT / "genuine" / "link_exe.ld",
    "server": PORT / "genuine" / "link_server.ld",
    "dll": PORT / "genuine" / "link_dll.ld",
}


def sh(*args, **kw):
    print("+", " ".join(shlex.quote(str(a)) for a in args), flush=True)
    return subprocess.run([str(a) for a in args], **kw)


def compiler_for(src):
    if src.suffix in (".cpp", ".cc", ".cxx", ".C"):
        return "g++"
    if src.suffix == ".c":
        return "gcc"
    if src.suffix in (".S", ".s", ".asm"):
        return "gcc"
    raise ValueError(f"unknown source extension: {src}")


def compile_one(src, dst, includes, defines):
    cc = compiler_for(src)
    cmd = [cc] + COMPILE_FLAGS
    cmd += [f"-I{inc}" for inc in includes]
    cmd += [f"-D{d}" for d in defines]
    cmd += ["-c", str(src), "-o", str(dst)]
    sh(*cmd, check=True)


# .def EXPORTS line: [alias =] name @ ordinal [NONAME|DATA|...] [; comment]
# The name is everything before the FINAL " @ <digits>", so MSVC-decorated
# C++ names (?Printf@RDebug@@SAXPBDZZ @ 1345, the kernelhwsrv bx86/*.def
# format) parse too. The old regex stopped at the first '@' and silently
# dropped every decorated line, shifting all later export ordinals.
DEF_LINE_RE = re.compile(
    r"^(?:(?P<alias>[^\s=@]+)\s*=\s*)?(?P<name>.+?)\s*@\s*(?P<ord>\d+)"
    r"(?P<kw>(?:\s+(?:[A-Z][A-Z0-9_]*|\d+))*\s*)$"  # NONAME/DATA/ABSENT... [size]
)

# Read-only .def files used by --selftest (never written to).
SELFTEST_DEFS = [
    REPO / "kernelhwsrv" / "userlibandfileserver" / "fileserver" / "bx86" / "efsrvu.def",
    REPO / "kernelhwsrv" / "kernel" / "eka" / "bx86" / "euseru.def",
    REPO / "kernelhwsrv" / "kernel" / "eka" / "bx86gcc" / "euseru.def",
]

# --name must be one safe path component so BUILD / f"{name}.elf" can never
# escape genuine/build/ (review issue #2: path traversal via --name).
NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


def parse_def_text(text, origin):
    """Parse a Symbian .def EXPORTS section -> [(ordinal, name), ...] by ordinal.

    Accepts plain names (`EmptyFunction @ 1 NONAME`), MSVC-decorated C++
    names (`?Printf@RDebug@@SAXPBDZZ @ 1345 NONAME`) and aliases
    (`alias = real @ 2`, real = right-hand side). Ordinals are kept only for
    documentation; the E32 export directory is ordinal-by-position, so
    declaration/ordinal order is honoured. Any non-empty, non-comment line
    inside EXPORTS that does not parse raises ValueError with origin:line --
    an entry is NEVER silently dropped (a dropped entry shifts every later
    ordinal and mis-binds the export directory).
    """
    entries = []
    in_exports = False
    for lineno, line in enumerate(text.splitlines(), 1):
        code = line.split(";", 1)[0].strip()
        if not code:
            continue
        if code.upper().startswith("EXPORTS"):
            in_exports = True
            continue
        if not in_exports:
            continue
        m = DEF_LINE_RE.match(code)
        if not m:
            raise ValueError(f"{origin}:{lineno}: unparseable EXPORTS line: {code!r}")
        entries.append((int(m.group("ord")), m.group("name")))
    entries.sort(key=lambda e: e[0])  # stable: duplicates keep file order
    if not entries:
        raise ValueError(f"no exports parsed from {origin}")
    return entries


def parse_def(path):
    return parse_def_text(Path(path).read_text(errors="replace"), str(path))


def selftest():
    """Unit self-check for parse_def (review issue #1)."""
    print("== genuine_build.py parse_def self-check ==")
    failures = 0
    # synthetic cases: plain, MSVC-decorated, alias, keywords, comments
    good = (
        "EXPORTS\n"
        "\tEmptyFunction @ 1 NONAME ; plain\n"
        "\t?Printf@RDebug@@SAXPBDZZ @ 1345 NONAME ; MSVC-decorated\n"
        "\t??0CDir@@IAE@XZ @ 2\n"
        "\talias = ?Real@RC@@QAEXXZ @ 3 NONAME\n"
        "\t; just a comment\n"
        "\n"
        "\tmemclr @ 4\n"
    )
    entries = parse_def_text(good, "<selftest>")
    expect = [(1, "EmptyFunction"), (2, "??0CDir@@IAE@XZ"),
              (3, "?Real@RC@@QAEXXZ"), (4, "memclr"),
              (1345, "?Printf@RDebug@@SAXPBDZZ")]
    if entries != expect:
        print(f"  FAIL: synthetic parse {entries} != {expect}")
        failures += 1
    else:
        print(f"  synthetic: {len(entries)} cases parsed+mapped OK "
              "(plain, MSVC-decorated, alias, bare)")
    # a garbage line inside EXPORTS must raise loudly, never be dropped
    for bad in ("this is not an export @ all",
                "??broken@@QAE@XZ",          # no ordinal
                "?Name@Cls@@QAEXXZ @ notanum"):
        try:
            parse_def_text("EXPORTS\n\t" + bad + "\n", "<selftest-bad>")
        except ValueError as exc:
            print(f"  garbage rejected loudly: {bad!r} -> {exc}")
        else:
            print(f"  FAIL: garbage line silently accepted: {bad!r}")
            failures += 1
    # real genuine .def files (read-only)
    files = parsed = decorated = 0
    for deff in SELFTEST_DEFS:
        if not deff.exists():
            print(f"  SKIP (missing): {deff}")
            continue
        files += 1
        try:
            entries = parse_def(deff)
        except ValueError as exc:
            print(f"  FAIL {deff}: {exc}")
            failures += 1
            continue
        dec = sum(1 for _, n in entries if n.startswith("?"))
        parsed += len(entries)
        decorated += dec
        print(f"  {deff.name:12s} {len(entries):5d} entries: "
              f"plain={len(entries) - dec:5d} msvc-decorated={dec:5d} failed=0")
    print(f"  TOTAL: {files} def files, {parsed} entries parsed/mapped to "
          f"(ordinal,name), {decorated} MSVC-decorated, {failures} failure(s)")
    if failures:
        sys.exit(f"selftest: {failures} failure(s)")
    print("selftest: PASS (no EXPORTS line silently dropped)")


def export_map_dict(pairs):
    """--export-map NAME=SYM list -> dict; duplicate keys fail loudly."""
    mapping = {}
    for m in pairs:
        if "=" not in m:
            sys.exit(f"genuine_build: --export-map {m!r} must be NAME=SYM")
        k, v = m.split("=", 1)
        if k in mapping:
            sys.exit(f"genuine_build: duplicate --export-map key {k!r}")
        mapping[k] = v
    return mapping


def elf_symbols(blob):
    """Minimal ELF32-i386 symbol table reader: {name: (value, shndx)}."""
    e_shoff, = struct.unpack_from("<I", blob, 32)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from("<HHH", blob, 46)
    sections = [struct.unpack_from("<10I", blob, e_shoff + i * e_shentsize)
                for i in range(e_shnum)]
    result = {}
    for sec in sections:
        if sec[1] != 2:  # SHT_SYMTAB
            continue
        strings = blob[sections[sec[6]][4]:sections[sec[6]][4] + sections[sec[6]][5]]
        raw = blob[sec[4]:sec[4] + sec[5]]
        for pos in range(0, len(raw), 16):
            name_off, value, _, info, _, shndx = struct.unpack_from("<IIIBBH", raw, pos)
            name = strings[name_off:].split(b"\0", 1)[0].decode("utf-8", "replace")
            if name and shndx != 0:
                result[name] = (value, shndx)
    return result


def patch_exports(e32_path, export_dir_off, targets, make_dll):
    """Add an export directory to the E32 produced by elf2e32.py.

    elf2e32.py emits exp_off=exp_count=0 (import-free/one-import EXEs). For a
    DLL/server we need the export directory *inside the code region*; the link
    script places a zero-filled .genuine_exp pad at `export_dir_off` so there
    is room for it without moving any existing section or relocation.
    """
    blob = bytearray(e32_path.read_bytes())
    code_off, = struct.unpack_from("<I", blob, 100)
    exp_off = code_off + export_dir_off
    n = len(targets)
    blob[exp_off:exp_off + 4 * n] = struct.pack("<" + "I" * n, *targets)
    struct.pack_into("<II", blob, 88, exp_off, n)
    text_size, = struct.unpack_from("<i", blob, 96)
    struct.pack_into("<i", blob, 96, max(text_size, export_dir_off + 4 * n))

    if make_dll:
        uid2, uid3 = struct.unpack_from("<2I", blob, 4)
        flags, = struct.unpack_from("<I", blob, 44)
        struct.pack_into("<I", blob, 0, KDynamicLibraryUidValue)
        struct.pack_into("<I", blob, 12,
                         uid_checksum(KDynamicLibraryUidValue, uid2, uid3))
        struct.pack_into("<I", blob, 44, flags | KImageDll)

    struct.pack_into("<I", blob, 20, KImageCrcInitialiser)
    struct.pack_into("<I", blob, 20, crc32_symbian(bytes(blob[:code_off]), 0))
    e32_path.write_bytes(blob)
    print(f"patched {n} exports at code+{export_dir_off:#x}, "
          f"dll={make_dll}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sources", nargs="*", type=Path,
                    help="C/C++/asm sources (genuine ones come from kernelhwsrv)")
    ap.add_argument("--name", help="output base name; must be a single safe path "
                         "component (^[A-Za-z0-9][A-Za-z0-9._-]*$) -- outputs are "
                         "confined to genuine/build/ (required unless --selftest)")
    ap.add_argument("--selftest", action="store_true",
                    help="run the parse_def unit self-check over the genuine "
                         ".def files and exit")
    ap.add_argument("--type", choices=sorted(LINK_SCRIPTS), default="exe",
                    help="link type / link script (default: exe)")
    ap.add_argument("-I", "--include", action="append", default=[],
                    help="extra include dir (repeatable)")
    ap.add_argument("-D", "--define", action="append", default=[],
                    help="extra -D define (repeatable)")
    ap.add_argument("--def", dest="def_file", type=Path,
                    help="Symbian .def export list (DLL exports only: the kernel "
                         "loader rejects exports on exe/server images, "
                         "e32load.cpp:407)")
    ap.add_argument("--allow-non-dll-exports", action="store_true",
                    help="build an e32read-valid exe/server image with --def "
                         "anyway (loudly warned; the kernel loader will still "
                         "reject it)")
    ap.add_argument("--export-map", action="append", default=[], metavar="NAME=SYM",
                    help="map a .def export name to the ELF symbol that implements it")
    ap.add_argument("--euser-import", type=int, default=0, metavar="ORD",
                    help="emit a PE2 EUSER.DLL import for this 1-based ordinal "
                         "(915 = RDebug::Printf in the staged genuine EUSER.DLL)")
    ap.add_argument("--iat-off", type=lambda s: int(s, 0), default=IAT_OFF_DEFAULT,
                    metavar="OFF", help="PE2 IAT byte offset (default 0x8000)")
    ap.add_argument("--iat-sym", metavar="SYM",
                    help="define this symbol as the IAT slot at 0x400000+OFF")
    ap.add_argument("--defsym", action="append", default=[], metavar="SYM=EXPR",
                    help="raw ld --defsym argument (repeatable)")
    ap.add_argument("--no-port-startup", action="store_true",
                    help="do not add qemu-port/user/start.S + runtime.c (EXE/server)")
    ap.add_argument("--link-script", type=Path, help="override the default link script")
    args = ap.parse_args()

    if args.selftest:
        selftest()
        return
    if not args.name:
        ap.error("--name is required")
    if not args.sources:
        ap.error("at least one source file is required (or use --selftest)")

    # -- sanitize --name BEFORE anything is created: it must be a single safe
    # path component, otherwise BUILD / f"{name}.elf" escapes genuine/build/
    # (e.g. --name ../../../kernelhwsrv/evil would write into kernelhwsrv/).
    if not NAME_RE.match(args.name):
        sys.exit(f"genuine_build: --name {args.name!r} rejected: must be a single "
                 f"path component matching {NAME_RE.pattern} (no '/', '..', or "
                 f"absolute path); all outputs are confined to {BUILD}")

    def build_out(ext):
        p = (BUILD / f"{args.name}{ext}").resolve()
        if p.parent != BUILD.resolve():
            sys.exit(f"genuine_build: output path {p} escapes {BUILD}")
        return p

    # -- --def on exe/server is refused: the loader hard-rejects export-
    # carrying non-DLL images (variant/e32load.cpp:407 KErrNotSupported).
    if args.def_file and args.type != "dll":
        if not args.allow_non_dll_exports:
            sys.exit(f"genuine_build: --def with --type {args.type} refused: the "
                     f"kernel loader rejects non-DLL images with exports "
                     f"(variant/e32load.cpp:407: !isDll && expCount!=0 -> "
                     f"KErrNotSupported). Use --type dll, or pass "
                     f"--allow-non-dll-exports to build an e32read-only image.")
        print(f"WARNING: --def with --type {args.type}: e32read accepts this image "
              f"but the kernel loader will REJECT it (e32load.cpp:407)!",
              file=sys.stderr)

    PORT.joinpath("tools/elf2e32.py").exists() or sys.exit("elf2e32.py missing")
    BUILD.mkdir(parents=True, exist_ok=True)
    OBJ.mkdir(parents=True, exist_ok=True)

    includes = list(GENUINE_INCLUDES) + [Path(p).resolve() for p in args.include]
    defines = list(GENUINE_DEFINES) + list(args.define)
    script = args.link_script or LINK_SCRIPTS[args.type]

    # -- 1. compile every source -------------------------------------------------
    objs = []
    for i, src in enumerate(args.sources):
        src = src.resolve()
        if not src.exists():
            sys.exit(f"missing source: {src}")
        dst = OBJ / f"{src.stem}_{i}.o"
        compile_one(src, dst, includes, defines)
        objs.append(dst)

    # -- 2. add the port's freestanding startup for EXEs / servers --------------
    if args.type != "dll" and not args.no_port_startup:
        for src in (PORT / "user" / "start.S", PORT / "user" / "runtime.c"):
            dst = OBJ / f"port_{src.stem}.o"
            compile_one(src, dst, includes, defines)
            objs.append(dst)

    # -- 3. make room for an export directory, if requested ---------------------
    export_dir_off = None
    export_entries = []
    if args.def_file:
        export_entries = parse_def(args.def_file.resolve())
        export_names = [n for _, n in export_entries]
        dec = sum(1 for _, n in export_entries if n.startswith("?"))
        print(f"parsed {len(export_entries)} exports from {args.def_file} "
              f"(msvc-decorated={dec}, plain={len(export_entries) - dec})")
        mapping = export_map_dict(args.export_map)
        pad = OBJ / "genuine_export_pad.s"
        pad.write_text(
            '.section .genuine_exp,"a"\n'
            '.balign 4\n'
            '.global _genuine_export_base\n'
            '_genuine_export_base:\n'
            f'.space {4 * len(export_names)}\n'
        )
        compile_one(pad, OBJ / "genuine_export_pad.o", includes, defines)
        objs.append(OBJ / "genuine_export_pad.o")

    # -- 4. link ----------------------------------------------------------------
    elf = build_out(".elf")
    ld_cmd = ["ld", "-m", "elf_i386", "--emit-relocs", "-T", str(script),
              "-o", str(elf)] + [str(o) for o in objs]
    if args.iat_sym:
        ld_cmd.append(f"--defsym={args.iat_sym}={CODE_BASE + args.iat_off:#x}")
    ld_cmd += [f"--defsym={d}" for d in args.defsym]
    ld_cmd += [subprocess.check_output(["gcc", "-m32", "-print-libgcc-file-name"],
                                       text=True).strip()]
    sh(*ld_cmd, check=True)

    # -- 5. pack with the existing PE2 E32 converter ----------------------------
    e32 = build_out(".E32")
    pack_cmd = [sys.executable, str(TOOLS / "elf2e32.py"), str(elf), str(e32)]
    if args.euser_import:
        pack_cmd += ["--euser-import", str(args.euser_import),
                     "--iat-off", str(args.iat_off)]
    sh(*pack_cmd, check=True)

    # -- 6. exports (post-process; elf2e32.py itself emits exp_count=0) ---------
    if export_entries:
        symbols = elf_symbols(elf.read_bytes())
        mapping = export_map_dict(args.export_map)
        targets = []
        for ordinal, name in export_entries:
            sym = mapping.get(name, name)
            if sym not in symbols:
                # FAIL LOUDLY, never silently drop: a missing export here
                # would shift every later ordinal (review issue #1).
                if name.startswith("?"):
                    hint = (f" (MSVC-decorated .def name: map it explicitly with "
                            f"--export-map '{name}=<gcc-symbol>'; find the symbol "
                            f"with: nm {elf} | c++filt)")
                else:
                    hint = (f" -- use --export-map {name}=<mangled-symbol> "
                            f"(nm {elf} | c++filt)")
                sys.exit(f"export ordinal {ordinal}: {name!r} -> ELF symbol "
                         f"{sym!r} not found in {elf}{hint}")
            value = symbols[sym][0]
            target = value - CODE_BASE
            # conservative bound: code must stay below iat_off even when no
            # IAT exists (review issue #4, safe direction)
            if not 0 <= target < args.iat_off:
                sys.exit(f"export ordinal {ordinal} {name!r} at {value:#x} "
                         f"outside code region [0,{args.iat_off:#x})")
            targets.append(target)
            print(f"  export ord {ordinal} {name}: code+{target:#x}")
        if "_genuine_export_base" not in symbols:
            sys.exit("export pad symbol missing from linked ELF")
        export_dir_off = symbols["_genuine_export_base"][0] - CODE_BASE
        patch_exports(e32, export_dir_off, targets, make_dll=(args.type == "dll"))

    # -- 7. validate ------------------------------------------------------------
    sh(sys.executable, str(TOOLS / "e32read.py"), str(e32), "--quiet", check=True)
    print(f"OK: {e32} ({e32.stat().st_size} bytes, type={args.type}, "
          f"euser_ord={args.euser_import}, exports={len(export_entries)})")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, struct.error, IndexError, KeyError,
            subprocess.CalledProcessError) as exc:
        # clean message instead of a raw traceback (review issue #5:
        # truncated ELF -> struct.error/IndexError, huge arg list -> OSError)
        sys.exit(f"genuine_build: {exc}")
