#!/usr/bin/env python3
"""compile_objs.py -- compile the genuine domainmgr sources (and glue) into
genuine/dm/obj/ with the exact genuine_build.py flags, so gen_stubs.py /
gen_dm_def.py can inspect them. Run from qemu-port/."""
import importlib.util
import sys
from pathlib import Path

PORT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("gb", PORT / "tools/genuine_build.py")
gb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gb)

KH = PORT.parent / "kernelhwsrv"
DM = KH / "userlibandfileserver/domainmgr"
FS = KH / "userlibandfileserver/fileserver"
INC = list(gb.GENUINE_INCLUDES) + [DM / "inc", DM / "src", FS / "inc"]
DEF = list(gb.GENUINE_DEFINES)
OUT = Path(__file__).resolve().parent / "obj"
OUT.mkdir(exist_ok=True)

SOURCES = [
    DM / "src/domainsrv.cpp",
    DM / "src/domaincfg.cpp",
    DM / "src/domaincli.cpp",
    DM / "src/domainpolicy.cpp",
    Path(__file__).resolve().parent / "dm_glue.cpp",
    Path(__file__).resolve().parent / "dm_dll_glue.cpp",
]

for src in SOURCES:
    gb.compile_one(src, OUT / (src.stem + ".o"), INC, DEF)
    print("compiled", src.name)
