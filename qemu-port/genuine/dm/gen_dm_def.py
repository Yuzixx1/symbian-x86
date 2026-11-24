#!/usr/bin/env python3
"""gen_dm_def.py -- generate a genuine_build.py-consumable .def + export maps
for a domainmgr client DLL (bx86/domaincliu.def, domainpolicyu.def, ...).

The genuine .def files use MSVC-mangled names with a `; public: ... Class::Method(args)`
comment. We compile the genuine source once, demangle its symbols with
c++filt, and match each .def entry to the GCC symbol by the
`Class::Method` (or `Class::~Class` / `Class::Class`) prefix, keeping the
ordinal order. Output: genuine/dm/<out>.def with `dm_ord<N> @ <ord>` aliases
and the `--export-map dm_ord<N>=<gcc symbol>` lines on stdout.

Usage:
  python3 genuine/dm/gen_dm_def.py <def-in> <compiled.o> <out.def>
"""
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def parse_def(path):
    entries = []
    in_exp = False
    for line in Path(path).read_text(errors="replace").splitlines():
        code = line.split(";", 1)
        body = code[0].strip()
        comment = code[1].strip() if len(code) > 1 else ""
        if not body:
            continue
        if body.upper().startswith("EXPORTS"):
            in_exp = True
            continue
        if not in_exp:
            continue
        m = re.match(r"^(.*)\s*@\s*(\d+)(?:\s|$)", body)
        if m:
            entries.append((int(m.group(2)), m.group(1).strip(), comment))
    entries.sort()
    return entries


def comment_key(comment):
    """Extract Class::Method / ctor / dtor key from a MSVC signature comment."""
    m = re.search(r"([A-Za-z_]\w*)::(~?[A-Za-z_]\w*)\s*\(", comment)
    if not m:
        return None
    return f"{m.group(1)}::{m.group(2)}("


def main():
    def_in, obj, out = sys.argv[1], sys.argv[2], sys.argv[3]
    entries = parse_def(def_in)
    nm = subprocess.run(["nm", "--defined-only", obj], capture_output=True, text=True).stdout
    syms = [ln.split()[-1] for ln in nm.splitlines() if " T " in ln or " W " in ln]
    dem = {}
    for s in syms:
        d = subprocess.run(["c++filt", s], capture_output=True, text=True).stdout.strip()
        dem[d] = s
    lines = ["EXPORTS"]
    maps = []
    missing = []
    for ordinal, msvc, comment in entries:
        key = comment_key(comment)
        match = None
        if key:
            for d, s in dem.items():
                if d.startswith(key) or d.startswith(key[:-1] + "::"):
                    match = s
                    break
            if match is None:  # try looser: Class::Method anywhere
                for d, s in dem.items():
                    if key[:-1] in d:
                        match = s
                        break
        alias = f"dm_ord{ordinal}"
        lines.append(f"\t{alias} @ {ordinal} NONAME\t; {msvc}")
        if match:
            maps.append((alias, match))
        else:
            missing.append((ordinal, msvc, comment))
    Path(out).write_text("\n".join(lines) + "\n")
    Path(out + ".maps").write_text("".join(f"--export-map {a}={s}\n" for a, s in maps))
    print(f"wrote {out}: {len(entries)} exports, {len(maps)} mapped, {len(missing)} missing")
    for alias, sym in maps:
        print(f"  --export-map {alias}={sym}")
    for ordinal, msvc, comment in missing:
        print(f"  MISSING ord {ordinal}: {msvc} ; {comment}", file=sys.stderr)


if __name__ == "__main__":
    main()
