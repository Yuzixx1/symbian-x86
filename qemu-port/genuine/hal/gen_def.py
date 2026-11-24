#!/usr/bin/env python3
"""gen_def.py -- turn the genuine MSVC .def (bx86/halu.def) into a def file
that tools/genuine_build.py can consume.

genuine_build.py's parse_def() keys export maps by the export *name*, and
halu.def has duplicate names (?Get twice, ?Set twice), so this script emits
one unique alias per ordinal:

    EXPORTS
        hal_ord1 @ 1 NONAME     ; ?Get@HAL@@SAHW4TAttribute@HALData@@AAH@Z ...
        ...

The matching --export-map arguments (alias -> GCC Itanium symbol) are printed
on stdout for the build command; verify them with `nm genuine/build/*.elf`.

Usage: python3 genuine/hal/gen_def.py [halu.def] [out.def]
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEF_IN = HERE.parent.parent.parent / "kernelhwsrv/halservices/hal/bx86/halu.def"
DEF_OUT = HERE / "hal_halu.def"

# alias ordinal -> GCC (Itanium) mangled symbol for the genuine sources
# (class HAL : public HALData in inc/hal.h, HalInternal in inc/hal_int.h)
GCC_SYMBOL = {
    1: "_ZN3HAL3GetEN7HALData10TAttributeERi",   # HAL::Get(TAttribute, TInt&)
    2: "_ZN3HAL6GetAllERiRPNS_6SEntryE",           # HAL::GetAll(TInt&, SEntry*&)
    3: "_ZN3HAL3SetEN7HALData10TAttributeEi",    # HAL::Set(TAttribute, TInt)
    4: "_ZN3HAL3GetEiN7HALData10TAttributeERi",  # HAL::Get(TInt, TAttribute, TInt&)
    5: "_ZN3HAL3SetEiN7HALData10TAttributeEi",   # HAL::Set(TInt, TAttribute, TInt)
    6: "_ZN11HalInternal12InitialValueE",          # HalInternal::InitialValue
}


def main():
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else DEF_IN
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else DEF_OUT
    entries = []
    in_exports = False
    for line in src.read_text(errors="replace").splitlines():
        code = line.split(";", 1)[0].strip()
        if not code:
            continue
        if code.upper().startswith("EXPORTS"):
            in_exports = True
            continue
        if not in_exports:
            continue
        m = re.match(r"^(.*)\s*@\s*(\d+)(?:\s|$)", code)  # greedy: last @ ord
        if m:
            entries.append((int(m.group(2)), m.group(1), code))
    entries.sort()
    lines = ["EXPORTS"]
    maps = []
    for ordinal, name, code in entries:
        alias = f"hal_ord{ordinal}"
        lines.append(f"\t{alias} @ {ordinal} NONAME\t; {name}")
        sym = GCC_SYMBOL.get(ordinal, name)
        maps.append((alias, sym))
    out.write_text("\n".join(lines) + "\n")
    print(f"wrote {out} with {len(entries)} exports")
    for alias, sym in maps:
        print(f"  --export-map {alias}={sym}")


if __name__ == "__main__":
    main()
