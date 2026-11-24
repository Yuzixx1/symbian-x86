#!/usr/bin/env python3
"""Build the GENUINE edisp display driver as a standalone EDISP.DLL PE2.

Compiles the UNMODIFIED kernelhwsrv edisp source
(kernel/eka/drivers/edisp/epoc/generic/wd_generic.cpp -- the X86 source of
edisp.mmp, without the VGA-text backend) plus genuine/ewsrv/edisp_glue.cpp
(the port display-HAL bridge to the VBE LFB) through tools/genuine_build.py
--type dll, exporting CScreenDriver::New() at ordinal 1 exactly as declared
in kernelhwsrv/kernel/eka/bx86/edisp.def.  Validated with tools/e32read.py.

The port kernel loader binds only one DLL dependency per image (EUSER.DLL;
dynamic ELoadLibrary is KErrNotSupported), so the image carries a single
EUSER import: UserSvr::ScreenInfo (ord 1211) through the PE2 IAT slot.

kernelhwsrv/ is never written.  Output: genuine/build/EDISP.DLL.
"""
import shutil
import subprocess
import sys
from pathlib import Path

PORT = Path(__file__).resolve().parents[2]
REPO = PORT.parent
EKA = REPO / "kernelhwsrv" / "kernel" / "eka"
KH = REPO / "kernelhwsrv"
HERE = Path(__file__).resolve().parent


def main():
    cmd = [
        sys.executable, str(PORT / "tools" / "genuine_build.py"),
        # genuine edisp source, unmodified (read-only kernelhwsrv/)
        str(EKA / "drivers" / "edisp" / "epoc" / "generic" / "wd_generic.cpp"),
        # port glue: display HAL attributes <- VBE LFB via the ScreenInfo bridge
        str(HERE / "edisp_glue.cpp"),
        "--name", "EDISP",
        "--type", "dll",
        # same extra include dirs as build_ewsrv.py INCS (ws_std.h pulls in
        # twintnotifier.h -> f32file.h etc.)
        "-I", str(EKA / "include" / "kernel" / "x86"),
        "-I", str(EKA / "include" / "nkern"),
        "-I", str(EKA / "common"),
        "-I", str(KH / "userlibandfileserver" / "domainmgr" / "inc"),
        "-I", str(KH / "userlibandfileserver" / "fileserver" / "inc"),
        "-I", str(KH / "halservices" / "hal" / "inc"),   # <hal.h>
        "-I", str(EKA / "ewsrv"),                          # ws_std.h if needed
        "-D", "EDISP_STANDALONE",
        "-D", "EDISP_USE_IAT_SLOT",
        # edisp.mmp shape: one DLL, ordinal-1 export CScreenDriver::New()
        "--def", str(EKA / "bx86" / "edisp.def"),
        "--export-map", "?New@CScreenDriver@@SAPAV1@XZ=_ZN13CScreenDriver3NewEv",
        "--link-script", str(HERE / "link_edisp.ld"),
        # single EUSER import: UserSvr::ScreenInfo(TDes8&) = ord 1211
        "--euser-import", "1211",
        "--iat-sym", "edisp_screeninfo_slot",
        "--iat-off", "0x10000",   # wd_generic .text+.rodata (font tables) > 0x8000
    ]
    r = subprocess.run(cmd, cwd=PORT)
    if r.returncode:
        raise SystemExit("EDISP.DLL build failed")
    src = PORT / "genuine" / "build" / "EDISP.E32"
    dst = PORT / "genuine" / "build" / "EDISP.DLL"   # genuine FAT name (ws_win.cpp)
    shutil.copyfile(src, dst)
    r = subprocess.run([sys.executable, str(PORT / "tools" / "e32read.py"),
                        str(dst), "--quiet"])
    if r.returncode:
        raise SystemExit("EDISP.DLL e32read validation failed")
    print(f"RESULT: {dst} ({dst.stat().st_size} bytes, ordinal-1 CScreenDriver::New, "
          f"1 EUSER import ord 1211)")


if __name__ == "__main__":
    main()
