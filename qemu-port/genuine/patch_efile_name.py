#!/usr/bin/env python3
"""Patch the genuine EFILE.E32 server name for this port kernel (no ! support).

kernelhwsrv's sf_main.cpp publishes the protected "!FileServer" name, which
traps (K::Fault 4 / EMutexWaitNotDThread) in the port's ProtServ path.  Until
the !-name/caps fix lands in variant/e32load.cpp, patch the _LIT literal to an
unprotected 11-char name "RFileServer" (same encoded length) and patch the F32
probe client's ipc_connect string to match.  Both patched images are written
to genuine/build/ with _RFS suffix; originals are never modified.
"""
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BUILD = HERE / "build"

UTF32_NAME = b"".join(bytes([ord(c), 0, 0, 0]) for c in "!FileServer")
REPL_NAME = b"".join(bytes([ord(c), 0, 0, 0]) for c in "RFileServer")


def main():
    src = BUILD / "EFILE.E32"
    blob = bytearray(src.read_bytes())
    n = blob.count(UTF32_NAME)
    if not n:
        sys.exit("EFILE.E32: !FileServer literal not found")
    for i in range(len(blob) - len(UTF32_NAME) + 1):
        if blob[i:i + len(UTF32_NAME)] == UTF32_NAME:
            blob[i:i + len(UTF32_NAME)] = REPL_NAME
    out = BUILD / "EFILE_RFS.E32"
    out.write_bytes(blob)
    print(f"patched {n} server-name literal(s) -> RFileServer: {out}")
    subprocess.run([sys.executable, str(HERE.parent / "tools" / "e32read.py"),
                    str(out), "--quiet"], check=True)

    csrc = HERE / "f32_probe" / "F32CLIENT.E32"
    cblob = bytearray(csrc.read_bytes())
    m = cblob.count(b"R1FileServer\0")
    if not m:
        sys.exit("F32CLIENT.E32: R1FileServer string not found")
    cblob.replace(b"R1FileServer\0", b"RFileServer\0")
    cout = BUILD / "F32CLIENT_RFS.E32"
    cout.write_bytes(cblob)
    print(f"patched {m} client name string(s) -> RFileServer: {cout}")
    subprocess.run([sys.executable, str(HERE.parent / "tools" / "e32read.py"),
                    str(cout), "--quiet"], check=True)


if __name__ == "__main__":
    main()
