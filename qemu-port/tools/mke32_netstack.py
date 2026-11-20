#!/usr/bin/env python3
"""mke32_netstack.py -- build the TCP/IP stack E32 fixture (netstack).

netstack.E32 is a PE2 E32 EXE that imports EUSER.DLL *by ordinal*.  The
IAT lives at the end of the text section (slot k at textSize + 4*k) and the
loader overwrites each slot with the resolved EUSER export address.  Ordinals
imported (all present in EUSER.DLL's 2503-export table):

    slot  ord   export                              purpose
      0   915   RDebug::Printf                      serial-console markers
      1   617   User::RequestComplete               IPC result export
      2   626   User::WaitForAnyRequest              IPC completion wait
      3   925   RTimer::CreateLocal                  timing (timer create)
      4   928   RTimer::After                        timing (timer after)
      5   929   RTimer::Cancel                       timing (timer cancel)

The guest code implements a small IPv4/IPv6 stack: it builds an IPv4 header
and verifies its one's-complement checksum, checks an IPv6 header version,
builds an ICMP echo request and flips it into an echo reply, publishes the
results into a shared result block ("IPC export") and prints ordered markers
through ord 915.  The C reference implementation lives in user/netstack.c.

Usage:
    python3 tools/mke32_netstack.py --output /tmp/opencode/netstack.E32
"""
import struct
import sys
from collections import defaultdict

sys.path.insert(0, "/home/yuzi/Documents/Sybian REAL/qemu-port/tools")
from mke32 import (
    crc32_symbian, KImageCrcInitialiser, EPOC_SIG, ECpuX86,
    KImageHdrFmt_V, KExecutableImageUidValue, uid_checksum,
)

# (ordinal, name) -- slot index == list position.
IMPORTS = [
    (915, "RDebug::Printf"),
    (617, "User::RequestComplete"),
    (626, "User::WaitForAnyRequest"),
    (925, "RTimer::CreateLocal"),
    (928, "RTimer::After"),
    (929, "RTimer::Cancel"),
]
NIMP = len(IMPORTS)
SLOT_PRINTF = 0
TEXTSIZE = 0x200
CODE_BASE = 0x00400000
DLL_NAME = b"euser{000a0000}[100039e5].dll"
HEADER_SIZE = 156
CODE_OFF = HEADER_SIZE

IAT_OFF = TEXTSIZE                      # 0x200
STR_OFF = 0x220
STR_STEP = 0x40
PKT4_OFF = 0x360                        # 20-byte IPv4 header
PKT6_OFF = 0x380                        # 40-byte IPv6 header
PKTICMP_OFF = 0x3A0                     # 16-byte ICMP echo
RESULT_OFF = 0x3C0                      # 16-byte IPC result block
CKSUM_OFF = 0x180                       # checksum routine


def slot_off(k):
    return IAT_OFF + 4 * k


class Builder:
    def __init__(self):
        self.code = bytearray()
        self.reloc_sites = []

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
        """mov/imm opcode (B8/68/BE/BF) with a relocated absolute address."""
        site = self.pos + 1
        self.put(bytes([opcode]) + struct.pack("<I", CODE_BASE + target_off))
        self.reloc_sites.append(site)
        return site

    def call_slot(self, k):
        self.put_abs(0xB8, slot_off(k))  # mov eax, [IAT slot address]
        self.put(b"\xff\x10")            # call [eax]

    def call_local(self, off):
        self.put_abs(0xB8, off)          # mov eax, routine address
        self.put(b"\xff\xd0")            # call eax

    def printf(self, sidx):
        self.put_abs(0x68, STR_OFF + sidx * STR_STEP)  # push format string
        self.call_slot(SLOT_PRINTF)
        self.put(b"\x83\xc4\x04")        # add esp,4


b = Builder()

# ---- main ------------------------------------------------------------------
b.put(b"\x55")                       # push ebp
b.put(b"\x89\xe5")                   # mov ebp,esp
b.put(b"\x83\xec\x40")               # sub esp,0x40  ([ebp-4]/[ebp-8] scratch)

b.printf(0)                           # EUSER ordinal imports bound

# Build an IPv4 header (version/IHL=4/5, total len 20, id=1,
# checksum field zero, src 10.0.0.1 -> dst 10.0.0.2).
b.put_abs(0xBF, PKT4_OFF)            # mov edi, pkt4
b.put(b"\xc7\x07" + struct.pack("<I", 0x45000014))
b.put(b"\xc7\x47\x04" + struct.pack("<I", 0x00010000))
b.put(b"\xc7\x47\x08" + struct.pack("<I", 0x00000000))  # checksum = 0
b.put(b"\xc7\x47\x0c" + struct.pack("<I", 0x0A000001))
b.put(b"\xc7\x47\x10" + struct.pack("<I", 0x0A000002))

# cksum(pkt4, 10 words) -> [ebp-4]
b.put(b"\x6a\x0a")                   # push 10 (word count)
b.put_abs(0xBF, PKT4_OFF)            # mov edi, pkt4
b.put(b"\x57")                       # push edi (pointer)
b.call_local(CKSUM_OFF)
b.put(b"\x83\xc4\x08")               # add esp,8
b.put(b"\x89\x45\xfc")               # mov [ebp-4],eax

# Store checksum into header, recompute -> must be 0xffff.
b.put(b"\x8b\x45\xfc")               # mov eax,[ebp-4]
b.put_abs(0xBF, PKT4_OFF + 8)        # mov edi, &checksum field
b.put(b"\x89\x07")                   # mov [edi],eax
b.put(b"\x6a\x0a")                   # push 10
b.put_abs(0xBF, PKT4_OFF)            # mov edi, pkt4
b.put(b"\x57")                       # push edi
b.call_local(CKSUM_OFF)
b.put(b"\x83\xc4\x08")               # add esp,8
b.put(b"\x66\x3d\xff\xff")           # cmp ax,0xffff
b.put(b"\x75\x05")                   # jne ipv4_fail (+5)
b.printf(1)                           # IPv4 checksum verified
b.put(b"\xeb\x06")                   # jmp ipv6_test (+6)
# ipv4_fail:
b.put(b"\x90")                       # nop (label)
b.put(b"\xeb\x00")                   # jmp ipv6_test (patched below)
ipv4_fail_jmp = b.pos - 1
b.printf(1)                           # (unreachable label filler for padding)

# ---- IPv6 header test ------------------------------------------------------
ipv6_test = b.pos
b.code[ipv4_fail_jmp] = ipv6_test - (ipv4_fail_jmp + 1)
b.put_abs(0xBF, PKT6_OFF)            # mov edi, pkt6
b.put(b"\xc7\x07" + struct.pack("<I", 0x60000000))  # version=6
b.put(b"\xc7\x47\x04" + struct.pack("<I", 0x00080000))  # icmp6 next hdr
b.put(b"\x8b\x07")                   # mov eax,[edi]
b.put(b"\xc1\xe8\x1c")               # shr eax,28
b.put(b"\x83\xf8\x06")               # cmp eax,6
b.put(b"\x75\x05")                   # jne v6_fail (+5)
b.printf(2)                           # IPv6 version=6 ok
b.put(b"\xeb\x06")                   # jmp icmp_test (+6)
# v6_fail:
b.put(b"\x90")
b.put(b"\xeb\x00")
v6_fail_jmp = b.pos - 1
b.printf(2)

# ---- ICMP echo request -> echo reply ---------------------------------------
icmp_test = b.pos
b.code[v6_fail_jmp] = icmp_test - (v6_fail_jmp + 1)
b.put_abs(0xBF, PKTICMP_OFF)         # mov edi, icmp
b.put(b"\xc7\x07" + struct.pack("<I", 0x00000800))  # type=8, code=0
b.put(b"\xc7\x47\x04" + struct.pack("<I", 0x00010001))  # id=1, seq=1
b.put(b"\xc7\x47\x08" + struct.pack("<I", 0x00000000))  # checksum=0
b.put(b"\xc7\x47\x0c" + struct.pack("<I", 0xDEADBEEF))  # payload
b.put(b"\x6a\x08")                   # push 8 (words)
b.put_abs(0xBF, PKTICMP_OFF)         # mov edi, icmp
b.put(b"\x57")                       # push edi
b.call_local(CKSUM_OFF)
b.put(b"\x83\xc4\x08")               # add esp,8
b.put(b"\x89\x45\xf8")               # mov [ebp-8],eax
b.put(b"\x66\x85\xc0")               # test ax,ax
b.put(b"\x74\x1e")                   # jz icmp_fail (+0x1e, patched)
icmp_fail_jz = b.pos - 1
# write checksum into ICMP field
b.put(b"\x8b\x45\xf8")               # mov eax,[ebp-8]
b.put_abs(0xBF, PKTICMP_OFF + 8)     # mov edi, &checksum
b.put(b"\x89\x07")                   # mov [edi],eax
# flip type 8 -> 0 (echo reply)
b.put_abs(0xBF, PKTICMP_OFF)         # mov edi, icmp
b.put(b"\xc6\x07\x00")               # mov byte [edi],0
b.put(b"\x6a\x08")                   # push 8
b.put_abs(0xBF, PKTICMP_OFF)         # mov edi, icmp
b.put(b"\x57")                       # push edi
b.call_local(CKSUM_OFF)
b.put(b"\x83\xc4\x08")               # add esp,8
b.put(b"\x66\x85\xc0")               # test ax,ax
b.put(b"\x74\x05")                   # jz icmp_fail (+5)
b.printf(3)                           # ICMP echo reply built
b.put(b"\xeb\x06")                   # jmp ipc_export (+6)
# icmp_fail:
b.put(b"\x90")
b.put(b"\xeb\x00")
icmp_fail_jmp = b.pos - 1
b.printf(3)

# ---- IPC result export -----------------------------------------------------
ipc_export = b.pos
b.code[icmp_fail_jmp] = ipc_export - (icmp_fail_jmp + 1)
b.put_abs(0xBF, RESULT_OFF)          # mov edi, result block
b.put(b"\xc7\x07" + struct.pack("<I", 1))  # ipv4_ok
b.put(b"\xc7\x47\x04" + struct.pack("<I", 1))  # ipv6_ok
b.put(b"\xc7\x47\x08" + struct.pack("<I", 1))  # icmp_ok
b.put(b"\x8b\x45\xfc")               # mov eax,[ebp-4]
b.put(b"\xc7\x47\x0c\x00\x00\x00\x00")
b.put(b"\x89\x47\x0c")               # result.ipv4_checksum = eax
b.printf(4)                           # results exported via IPC
b.printf(5)                           # clean exit

# Fix the first ICMP fail jump (its displacement changed after layout).
b.code[icmp_fail_jz] = ipc_export - (icmp_fail_jz + 1)

# exit: ThreadKill(KCurrentThreadHandle, EExitKill, 0, NULL) via SLOW_EXEC.
b.put(b"\x6a\x00")                   # push NULL category (arg4)
b.put(b"\x6a\x00")                   # push reason 0 (arg3)
b.put(b"\x6a\x00")                   # push EExitKill=0 (arg2)
b.put(b"\x68\x01\x80\xff\xff")       # push KCurrentThreadHandle (arg1)
b.put(b"\x6a\x00")                   # push filler ret slot
b.put(b"\xb8\x73\x00\x00\x00")       # mov eax,115 (EExecThreadKill)
b.put(b"\x53")                       # push ebx
b.put(b"\x56")                       # push esi
b.put(b"\x8b\x4c\x24\x0c")           # mov ecx,[esp+12] arg1
b.put(b"\x8b\x54\x24\x10")           # mov edx,[esp+16] arg2
b.put(b"\x8b\x5c\x24\x14")           # mov ebx,[esp+20] arg3
b.put(b"\x8b\x74\x24\x18")           # mov esi,[esp+24] arg4
b.put(b"\xcd\x21")                   # int 0x21
MAIN_END = b.pos

# ---- one's-complement checksum routine (cdecl) -----------------------------
# cksum(void* ptr, int words) -> eax = 16-bit Internet checksum.
b.pad_to(CKSUM_OFF, b"\x90")
assert b.pos == CKSUM_OFF
b.put(b"\x55")                       # push ebp
b.put(b"\x89\xe5")                   # mov ebp,esp
b.put(b"\x53")                       # push ebx
b.put(b"\x56")                       # push esi
b.put(b"\x8b\x75\x08")               # mov esi,[ebp+8]  (ptr)
b.put(b"\x8b\x4d\x0c")               # mov ecx,[ebp+12] (word count)
b.put(b"\x31\xd2")                   # xor edx,edx      (sum)
ck_loop = b.pos
b.put(b"\x0f\xb7\x1e")               # movzx ebx,word [esi]
b.put(b"\x01\xda")                   # add edx,ebx
b.put(b"\x83\xc6\x02")               # add esi,2
b.put(b"\xe2" + bytes([(ck_loop - (b.pos + 1)) & 0xFF]))  # loop ck_loop
b.put(b"\x89\xd0")                   # mov eax,edx
b.put(b"\xc1\xe8\x10")               # shr eax,16
b.put(b"\x81\xe2\xff\xff\x00\x00")   # and edx,0xffff
b.put(b"\x01\xc2")                   # add edx,eax
b.put(b"\x89\xd0")                   # mov eax,edx
b.put(b"\xc1\xe8\x10")               # shr eax,16
b.put(b"\x01\xc2")                   # add edx,eax
b.put(b"\x66\xf7\xd2")               # not dx
b.put(b"\x0f\xb7\xc2")               # movzx eax,dx
b.put(b"\x5e")                       # pop esi
b.put(b"\x5b")                       # pop ebx
b.put(b"\x5d")                       # pop ebp
b.put(b"\xc3")                       # ret
CKSUM_END = b.pos
assert CKSUM_END <= TEXTSIZE, f"checksum routine overran text ({CKSUM_END:#x})"
assert MAIN_END <= CKSUM_OFF

# ---- IAT + strings + data --------------------------------------------------
b.pad_to(IAT_OFF)
for k, (ordinal, _name) in enumerate(IMPORTS):
    struct.pack_into("<I", b.code, slot_off(k), ordinal)

STRINGS = [
    b"netstack: EUSER ordinal imports bound\r\n",
    b"netstack: IPv4 header checksum verified\r\n",
    b"netstack: IPv6 version=6 ok\r\n",
    b"netstack: ICMP echo reply built\r\n",
    b"netstack: results exported via IPC\r\n",
    b"netstack: clean exit\r\n",
]
b.pad_to(STR_OFF)
for i, s in enumerate(STRINGS):
    assert b.pos == STR_OFF + i * STR_STEP
    assert len(s) < STR_STEP
    b.put(s)
    b.pad_to(STR_OFF + (i + 1) * STR_STEP)
STR_END = b.pos
b.pad_to(PKT4_OFF)
b.put(b"\x00" * (RESULT_OFF + 16 - b.pos))
DATA_END = b.pos

max_site = max(b.reloc_sites)
CODE_SIZE = max(DATA_END, max_site + 4, IAT_OFF + 4 * NIMP)
CODE_SIZE = (CODE_SIZE + 15) & ~15
assert CODE_SIZE < 0x1000
b.pad_to(CODE_SIZE)

# ---- relocations (all pages 0) ---------------------------------------------
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

# ---- PE2 import section (DLL name only; ordinals live in the IAT) ---------
NAME_OFF = 12
ISIZE = 8 + ((len(DLL_NAME) + 3) & ~3)
imp_section = struct.pack("<II", NAME_OFF, NIMP) + DLL_NAME
imp_section += b"\x00" * (ISIZE - len(imp_section))
assert len(imp_section) == ISIZE
IMP_OFF = CODE_OFF + CODE_SIZE
RELOC_OFF = IMP_OFF + 4 + ISIZE

# ---- header ----------------------------------------------------------------
header = bytearray(HEADER_SIZE)
uid1 = KExecutableImageUidValue
uid2, uid3 = 0, 0x4E4554  # "NET"
uid_chk = uid_checksum(uid1, uid2, uid3)
struct.pack_into("<4I", header, 0, uid1, uid2, uid3, uid_chk)
struct.pack_into("<I", header, 16, EPOC_SIG)
struct.pack_into("<I", header, 20, KImageCrcInitialiser)
struct.pack_into("<I", header, 24, 0x00010000)
struct.pack_into("<I", header, 28, 0)
struct.pack_into("<4B", header, 32, 1, 0, 0, 0)
struct.pack_into("<2I", header, 36, 0, 0)
struct.pack_into("<I", header, 44, KImageHdrFmt_V | 0x20000000)  # PE2 imports
struct.pack_into("<6i", header, 48, CODE_SIZE, 0, 0x1000, 0x100000, 0x2000, 0)
struct.pack_into("<I", header, 72, 0)  # entry point offset
struct.pack_into("<2I", header, 76, CODE_BASE, 0)
struct.pack_into("<i", header, 84, 1)  # dll refs (EUSER)
struct.pack_into("<I", header, 88, 0)  # export dir offset
struct.pack_into("<i", header, 92, 0)  # export count
struct.pack_into("<i", header, 96, TEXTSIZE)
struct.pack_into("<5I", header, 100, CODE_OFF, 0, IMP_OFF, RELOC_OFF, 0)
struct.pack_into("<2H", header, 120, 350, ECpuX86)
struct.pack_into("<I", header, 124, 0)
struct.pack_into("<3I", header, 128, 0, 0, 0)
struct.pack_into("<I", header, 140, 0)
struct.pack_into("<2I", header, 144, 0, 0)
struct.pack_into("<H", header, 152, 0)
struct.pack_into("<B", header, 154, 0x00)
struct.pack_into("<B", header, 155, 0x00)
assert len(header) == HEADER_SIZE
struct.pack_into("<I", header, 20, crc32_symbian(bytes(header), 0))

file_data = bytes(header) + bytes(b.code)
file_data += struct.pack("<I", ISIZE) + bytes(imp_section)
file_data += struct.pack("<2I", RELOC_SIZE, reloc_count) + bytes(reloc_blocks)

out = "/tmp/opencode/netstack.E32"
for i, a in enumerate(sys.argv):
    if a == "--output" and i + 1 < len(sys.argv):
        out = sys.argv[i + 1]
with open(out, "wb") as f:
    f.write(file_data)

print(f"WROTE {out} ({len(file_data)} bytes)")
print(f"  codeSize={CODE_SIZE:#x} textSize={TEXTSIZE:#x} nimports={NIMP} "
      f"checksum_end={CKSUM_END:#x}")
print(f"  imports={[(k, o) for k, (o, _n) in enumerate(IMPORTS)]}")
