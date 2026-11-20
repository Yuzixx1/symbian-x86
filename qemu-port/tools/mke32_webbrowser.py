#!/usr/bin/env python3
"""mke32_webbrowser.py -- build the R1 text web-browser PE2 E32 fixture.

Emits a genuine PE2 E32 that imports EUSER.DLL ordinal 915
(RDebug::Printf(char const*, ...)) and uses it to print the browser markers on
the serial console.  The fixture:

  1. prints the banner / URL / DNS-query / DNS-answer / TCP / GET / HTTP lines;
  2. renders a canned HTML page by walking the bytes, skipping <...> tags and
     printing each text run as "WEB: render: <run>" through RDebug::Printf;
  3. prints the done/exit markers and returns to the shell.

The C source of the same browser is user/ca_test.c (static build).  This is the
PE2 world: markers are emitted through the real EUSER.DLL IAT, not the port's
DebugPrint executive.  Kept structurally identical to mke32_ca_test.py.

Usage:
    python3 tools/mke32_webbrowser.py --output build/user/ca_test.e32
"""
import struct
import sys
from collections import defaultdict

sys.path.insert(0, "/home/yuzi/Documents/Sybian REAL/qemu-port/tools")
from mke32 import (
    crc32_symbian, KImageCrcInitialiser, EPOC_SIG, ECpuX86,
    KImageHdrFmt_V, KExecutableImageUidValue, uid_checksum,
)

NIMP = 1
ORD_PRINTF = 915
DLL_NAME = b"euser{000a0000}[100039e5].dll"
CODE_BASE = 0x00400000
HEADER_SIZE = 156
CODE_OFF = HEADER_SIZE
TEXTSIZE = 0x200
SLOT_PRINTF = 0
IAT_END = TEXTSIZE + 4 * NIMP

STR_OFF = 0x220
STR_STEP = 0x40
FMT_OFF = STR_OFF + 10 * STR_STEP           # 0x4a0 "WEB: render: %s"
HTML_OFF = FMT_OFF + 0x40                   # 0x4e0 canned HTML
SCRATCH_OFF = HTML_OFF + 0x100              # 0x5e0 line scratch (256 B)
CODE_SIZE = (SCRATCH_OFF + 0x100 + 15) & ~15 # 0x700

STRINGS = [
    b"WEB: browser start",
    b"WEB: URL http://example.com/",
    b"WEB: DNS query A example.com",
    b"WEB: DNS answer 93.184.216.34",
    b"WEB: TCP connect 93.184.216.34:80",
    b"WEB: GET / HTTP/1.0",
    b"WEB: HTTP 200 OK",
    b"WEB: page rendered OK",
    b"WEB: browser exit",
    b"WEB: render: %s",
]
HTML = (b"<html><body><h1>Example Domain</h1>"
        b"<p>This domain is for use in illustrative examples.</p>"
        b"</body></html>\x00")


class Builder:
    def __init__(self):
        self.code = bytearray()
        self.reloc_sites = []
        self.labels = {}
        self.fix8 = []
        self.fix32 = []

    @property
    def pos(self):
        return len(self.code)

    def put(self, data):
        self.code += data
        return self.pos - len(data)

    def pad_to(self, target, fill=b"\x00"):
        assert self.pos <= target, f"overran {target:#x} (pos={self.pos:#x})"
        self.put(fill * (target - self.pos))

    def put_abs(self, opcode, target_off):
        site = self.pos + 1
        self.put(bytes([opcode]) + struct.pack("<I", CODE_BASE + target_off))
        self.reloc_sites.append(site)
        return site

    def label(self, name):
        self.labels[name] = self.pos

    def jmp(self, name):
        pos = self.pos + 1
        self.put(b"\xEB\x00")
        self.fix8.append((pos, name))

    def jcc(self, opcode, name):
        pos = self.pos + 1
        self.put(bytes([opcode, 0]))
        self.fix8.append((pos, name))

    def call(self, name):
        pos = self.pos + 1
        self.put(b"\xE8" + struct.pack("<i", 0))
        self.fix32.append((pos, name))

    def call_slot(self, k):
        self.put_abs(0xB8, TEXTSIZE + 4 * k)
        self.put(b"\xff\x10")

    def printf(self, sidx):
        self.put_abs(0x68, STR_OFF + sidx * STR_STEP)
        self.call_slot(SLOT_PRINTF)
        self.put(b"\x83\xc4\x04")

    def resolve(self):
        for pos, name in self.fix8:
            self.code[pos] = (self.labels[name] - (pos + 1)) & 0xFF
        for pos, name in self.fix32:
            struct.pack_into("<i", self.code, pos,
                             self.labels[name] - (pos + 4))


b = Builder()

# main(): esi=HTML cursor, edi=scratch cursor, ebx=current run start.
b.put(b"\x55")                              # push ebp
b.put(b"\x89\xe5")                          # mov ebp,esp
for i in range(7):                          # banner .. HTTP 200
    b.printf(i)

b.put_abs(0xBE, HTML_OFF)                   # mov esi, html
b.put_abs(0xBF, SCRATCH_OFF)                # mov edi, scratch
b.put(b"\x89\xfb")                          # mov ebx,edi (run start)

b.label("outer")
b.put(b"\x0f\xb6\x06")                      # movzx eax,byte [esi]
b.put(b"\x84\xc0")                          # test al,al
b.jcc(0x74, "flush_done")                   # jz flush_done
b.put(b"\x3c\x3c")                          # cmp al,'<'
b.jcc(0x74, "tag")                          # je tag
b.put(b"\x3c\x3e")                          # cmp al,'>'
b.jcc(0x74, "next")                         # je next
b.put(b"\x88\x07")                          # mov [edi],al
b.put(b"\x47")                              # inc edi
b.put(b"\x46")                              # inc esi
b.jmp("outer")

b.label("tag")
b.call("flush")                             # emit the run before the tag
b.label("skip")
b.put(b"\x46")                              # inc esi
b.put(b"\x0f\xb6\x06")                      # movzx eax,byte [esi]
b.put(b"\x84\xc0")                          # test al,al
b.jcc(0x74, "flush_done")                   # jz flush_done
b.put(b"\x3c\x3e")                          # cmp al,'>'
b.jcc(0x75, "skip")                         # jne skip
b.put(b"\x46")                              # inc esi (past '>')
b.put(b"\x89\xfb")                          # mov ebx,edi (new run start)
b.jmp("outer")

b.label("next")
b.put(b"\x46")                              # inc esi (ignore stray '>')
b.jmp("outer")

b.label("flush_done")
b.call("flush")                             # emit trailing run
b.printf(7)                                 # "WEB: page rendered OK"
b.printf(8)                                 # "WEB: browser exit"
# Clean exit: ThreadKill(KCurrentThreadHandle, EExitKill, 0, NULL) via the
# genuine SLOW_EXEC dispatcher (`int 0x21`).  A bare `ret` here would pop a
# garbage return address and fault with #UD after the last marker.
b.put(b"\x6a\x00")                          # push NULL category (arg4)
b.put(b"\x6a\x00")                          # push reason 0 (arg3)
b.put(b"\x6a\x00")                          # push EExitKill=0 (arg2)
b.put(b"\x68\x01\x80\xff\xff")              # push KCurrentThreadHandle (arg1)
b.put(b"\x6a\x00")                          # push filler ret slot
b.put(b"\xb8\x73\x00\x00\x00")              # mov eax,115 (EExecThreadKill)
b.put(b"\x53")                              # push ebx
b.put(b"\x56")                              # push esi
b.put(b"\x8b\x4c\x24\x0c")                  # mov ecx,[esp+12] arg1
b.put(b"\x8b\x54\x24\x10")                  # mov edx,[esp+16] arg2
b.put(b"\x8b\x5c\x24\x14")                  # mov ebx,[esp+20] arg3
b.put(b"\x8b\x74\x24\x18")                  # mov esi,[esp+24] arg4
b.put(b"\xcd\x21")                          # int 0x21 (does not return)

b.label("flush")
b.put(b"\xc6\x07\x00")                      # mov byte [edi],0
b.put(b"\x39\xdf")                          # cmp edi,ebx
b.jcc(0x74, "flush_ret")                    # je flush_ret (empty run)
b.put(b"\x53")                              # push ebx (line text)
b.put_abs(0x68, FMT_OFF)                    # push "WEB: render: %s"
b.call_slot(SLOT_PRINTF)                    # RDebug::Printf (ord 915)
b.put(b"\x83\xc4\x08")                      # add esp,8
b.put(b"\x89\xfb")                          # mov ebx,edi
b.label("flush_ret")
b.put(b"\xc3")                              # ret

b.resolve()
CODE_END = b.pos

# IAT: ordinal 915 in slot 0 (code-relative textSize + 4*k).
b.pad_to(IAT_END)
struct.pack_into("<I", b.code, TEXTSIZE + 4 * SLOT_PRINTF, ORD_PRINTF)

# Strings, render format and canned HTML.
b.pad_to(STR_OFF)
for i, s in enumerate(STRINGS):
    assert b.pos == STR_OFF + i * STR_STEP
    b.put(s + b"\x00")
    b.pad_to(STR_OFF + (i + 1) * STR_STEP)
b.pad_to(FMT_OFF)
b.put(b"WEB: render: %s\x00")
b.pad_to(HTML_OFF)
b.put(HTML)
b.pad_to(SCRATCH_OFF)
b.pad_to(CODE_SIZE)
assert all(s + 4 <= CODE_SIZE for s in b.reloc_sites)

# TEXT relocations for every absolute immediate (type 0x1000).
pages = defaultdict(list)
for site in b.reloc_sites:
    pages[site & ~0xFFF].append(site & 0xFFF)
reloc_blocks = bytearray()
reloc_count = 0
for page in sorted(pages):
    offs = sorted(set(pages[page]))
    reloc_count += len(offs)
    padded = list(offs)
    if len(padded) & 1:
        padded.append(None)
    entries = b"".join(
        b"\x00\x00" if o is None else struct.pack("<H", 0x1000 | o) for o in padded
    )
    reloc_blocks += struct.pack("<2I", page, 8 + len(entries)) + entries
RELOC_SIZE = 8 + len(reloc_blocks)

# PE2 import section: iSize at imp_off, then {nameOff=12, nImports},
# then the mangled provider DLL name.  IAT ordinals live in the text gap.
NAME_OFF = 12
ISIZE = 8 + ((len(DLL_NAME) + 3) & ~3)
imp_section = struct.pack("<II", NAME_OFF, NIMP) + DLL_NAME
imp_section += b"\x00" * (ISIZE - len(imp_section))
assert len(imp_section) == ISIZE
IMP_OFF = CODE_OFF + CODE_SIZE
RELOC_OFF = IMP_OFF + 4 + ISIZE

uid1 = KExecutableImageUidValue
uid2, uid3 = 0, 0x1000B005
uid_chk = uid_checksum(uid1, uid2, uid3)
header = bytearray(HEADER_SIZE)
struct.pack_into("<4I", header, 0, uid1, uid2, uid3, uid_chk)
struct.pack_into("<I", header, 16, EPOC_SIG)
struct.pack_into("<I", header, 20, KImageCrcInitialiser)
struct.pack_into("<I", header, 24, 0x00010000)         # version 1.0
struct.pack_into("<I", header, 28, 0)                  # uncompressed
struct.pack_into("<4B", header, 32, 1, 0, 0, 0)        # tools version
struct.pack_into("<2I", header, 36, 0, 0)              # timestamps
struct.pack_into("<I", header, 44,
                 KImageHdrFmt_V | 0x20000000)          # V header + PE2 imports
struct.pack_into("<6i", header, 48, CODE_SIZE, 0, 0x1000, 0x100000, 0x2000, 0)
struct.pack_into("<I", header, 72, 0)                  # entry offset
struct.pack_into("<2I", header, 76, CODE_BASE, 0)
struct.pack_into("<i", header, 84, 1)                  # one DLL (EUSER)
struct.pack_into("<I", header, 88, 0)                  # export dir
struct.pack_into("<i", header, 92, 0)                  # export count
struct.pack_into("<i", header, 96, TEXTSIZE)           # IAT in text gap
struct.pack_into("<5I", header, 100, CODE_OFF, 0,
                 IMP_OFF, RELOC_OFF, 0)
struct.pack_into("<2H", header, 120, 350, ECpuX86)
struct.pack_into("<I", header, 124, 0)
struct.pack_into("<3I", header, 128, 0, 0, 0)          # SID / VID / caps[0]
struct.pack_into("<I", header, 140, 0)                 # caps[1]
struct.pack_into("<2I", header, 144, 0, 0)
struct.pack_into("<H", header, 152, 0)                 # NoHoles export desc
struct.pack_into("<B", header, 154, 0x00)
struct.pack_into("<B", header, 155, 0x00)
assert len(header) == HEADER_SIZE
struct.pack_into("<I", header, 20, crc32_symbian(bytes(header), 0))

file_data = (bytes(header) + bytes(b.code) + b"\x00" * (CODE_SIZE - len(b.code))
             + struct.pack("<I", ISIZE) + bytes(imp_section)
             + struct.pack("<2I", RELOC_SIZE, reloc_count) + bytes(reloc_blocks))

out = "/tmp/opencode/ca_test.e32"
for i, a in enumerate(sys.argv):
    if a == "--output" and i + 1 < len(sys.argv):
        out = sys.argv[i + 1]
with open(out, "wb") as f:
    f.write(file_data)

print(f"WROTE {out} ({len(file_data)} bytes)")
print(f"  codeSize={CODE_SIZE:#x} code_end={CODE_END:#x} nimports={NIMP} "
      f"ordinals={[ORD_PRINTF]} relocs={reloc_count}")
