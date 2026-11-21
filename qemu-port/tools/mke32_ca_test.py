#!/usr/bin/env python3
"""mke32_ca_test.py -- build the PE2 E32 CActive fixture (ca_test).

CP4 proof: full CActive/CActiveScheduler-style round trip using the real
user-mode wait primitive.  The loop (mirroring user/e32base.h's
CActiveScheduler::Start) blocks in the genuine EUSER User::WaitForAnyRequest()
ordinal 626 -> Exec fast exec 0 (`xor eax,eax; int 0x20`) ->
NKern::WaitForAnyRequest() on the current thread's request semaphore.  A
genuine RTimer completion (exec 133 TimerCreate, exec 53 TimerAfter) writes
KErrNone into the active's iStatus (offset +4, matching the genuine CActive
layout: CBase has no data members, TRequestStatus={iStatus,iFlags}) and signals
the same thread, so WaitForAnyRequest() returns, RunL() is dispatched through
the active's vptr, RunL() calls Stop() (ordinal 425) and sets the local stop
word, the loop exits and Start() returns.  The final marker is followed by a
direct exec 115 (EExecThreadKill) plus an infinite-loop backstop, so teardown
never falls through a `ret` into garbage (Exc 06, EIP=0x203).

The genuine EUSER CActiveScheduler::Start() ordinal is imported and bound but
not called: in this port its TLS/trap-harness state is unavailable to a
hand-built E32 image, so it neither dispatches RunL() nor returns reliably.
CActive ctor/SetActive/Add/Stop and the WaitForAnyRequest primitive are all
genuine EUSER ordinals; only the loop and the vptr write are fixture-local.

Ordinals imported (PE2 IAT holds 1-based ordinal; slot k at textSize+4*k):
  430 ctor, 428 Install, 426 Start, 425 Stop, 1091 CActive ctor,
  1090 SetActive, 915 RDebug::Printf, 424 Add, plus the genuine EUSER
  timer/request surface: 887 CTimer ctor, 881 CTimer::ConstructL,
  885 CTimer::After, 925 RTimer::CreateLocal, 928 RTimer::After,
  929 RTimer::Cancel, 617 User::RequestComplete, 626
  User::WaitForAnyRequest. The 8 new slots are bound and verified by
  r1_cp4_ca_test.py (PE2 validation caps imports at 16); the actual wake
  uses EExecTimerCreate/EExecTimerAfter (exec 133/53), the kernel device
  beneath RTimer::After.

Usage:
    python3 tools/mke32_ca_test.py --output /tmp/opencode/ca_test.e32
"""
import struct
import sys
from collections import defaultdict

sys.path.insert(0, "/home/yuzi/Documents/Sybian REAL/qemu-port/tools")
from mke32 import (
    crc32_symbian, KImageCrcInitialiser, EPOC_SIG, ECpuX86,
    KImageHdrFmt_V, KExecutableImageUidValue, uid_checksum,
)

IMPORTS = [                                   # slot
    (430,  "CActiveScheduler::CActiveScheduler"),  # 0
    (428,  "CActiveScheduler::Install"),           # 1
    (426,  "CActiveScheduler::Start"),             # 2 genuine wait loop
    (425,  "CActiveScheduler::Stop"),              # 3
    (1091, "CActive::CActive(int)"),               # 4 -> `this` in eax
    (1090, "CActive::SetActive"),                  # 5
    (915,  "RDebug::Printf"),                      # 6 markers
    (424,  "CActiveScheduler::Add"),               # 7
    (887,  "CTimer::CTimer(int)"),                 # 8 RTimer/CTimer surface
    (881,  "CTimer::ConstructL()"),                # 9
    (885,  "CTimer::After()"),                     # 10
    (925,  "RTimer::CreateLocal()"),               # 11
    (928,  "RTimer::After()"),                     # 12
    (929,  "RTimer::Cancel()"),                    # 13
    (617,  "User::RequestComplete()"),             # 14 completion producer
    (626,  "User::WaitForAnyRequest()"),           # 15 wait primitive
]
NIMP = len(IMPORTS)
TEXTSIZE = 0x200
CODE_BASE = 0x00400000
DLL_NAME = b"euser{000a0000}[100039e5].dll"
HEADER_SIZE = 156
CODE_OFF = HEADER_SIZE

SLOT_PRINTF = 6
IAT_END = TEXTSIZE + 4 * NIMP                 # 0x240

STR_OFF = 0x250
STR_STEP = 0x40
VTBL_OFF = STR_OFF + 6 * STR_STEP              # 0x3d0
VTBL_NDWORDS = 8
GLOBAL_OFF = VTBL_OFF + 4 * VTBL_NDWORDS       # 0x3f0


def slot_off(k):
    return TEXTSIZE + 4 * k


class Builder:
    def __init__(self):
        self.code = bytearray()
        self.reloc_sites = []
        self.dword_sites = set()

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

    def put_abs_dword(self, target_off):
        site = self.pos
        self.put(struct.pack("<I", CODE_BASE + target_off))
        self.reloc_sites.append(site)
        self.dword_sites.add(site)
        return site

    def lea_ebp(self, disp):
        assert -128 <= disp < 128
        self.put(b"\x8d\x45" + bytes([disp & 0xFF]))

    def call_slot(self, k):
        self.put_abs(0xB8, slot_off(k))
        self.put(b"\xff\x10")

    def printf(self, sidx):
        self.put_abs(0x68, STR_OFF + sidx * STR_STEP)
        self.call_slot(SLOT_PRINTF)
        self.put(b"\x83\xc4\x04")


b = Builder()

# main(): sched [ebp-0x30], active [ebp-0x50], timer handle [ebp-0x54]
b.put(b"\x55")                              # push ebp
b.put(b"\x89\xe5")                          # mov ebp,esp
b.put(b"\x83\xec\x60")                      # sub esp,0x60

for d in range(0x30, 0x17, -4):              # clear sched
    b.put(b"\xc7\x45" + bytes([(-d) & 0xFF]) + b"\x00\x00\x00\x00")
for d in range(0x50, 0x33, -4):              # clear active
    b.put(b"\xc7\x45" + bytes([(-d) & 0xFF]) + b"\x00\x00\x00\x00")

b.lea_ebp(-0x30)
b.put(b"\x50")
b.call_slot(0)
b.put(b"\x83\xc4\x04")

b.lea_ebp(-0x30)
b.put(b"\x50")
b.call_slot(1)
b.put(b"\x83\xc4\x04")

b.printf(0)                                 # Install ok

b.put(b"\x6a\x00")                          # push EPriorityStandard
b.lea_ebp(-0x50)
b.put(b"\x50")
b.call_slot(4)
b.put(b"\x83\xc4\x08")

b.put(b"\x89\xc1")                          # mov ecx,eax (this)
b.put_abs(0xB8, GLOBAL_OFF)
b.put(b"\x89\x08")                          # mov [GLOBAL],ecx
b.put(b"\xc7\x41\x04\x01\x00\x00\x80")      # iStatus = KRequestPending
b.put(b"\xc7\x41\x08\x00\x00\x00\x00")      # iFlags = 0
b.put_abs(0xB8, VTBL_OFF)                   # install fixture vtable
b.put(b"\x89\x01")

b.lea_ebp(-0x50)                            # Add
b.put(b"\x50")
b.call_slot(7)
b.put(b"\x83\xc4\x04")

b.lea_ebp(-0x50)                            # SetActive
b.put(b"\x50")
b.call_slot(5)
b.put(b"\x83\xc4\x04")

b.printf(1)                                 # Start() entered

# exec 133: TimerCreate -> handle in eax -> [ebp-0x54].  Arm the timer only
# after the Printf marker so no console work sits between arming and the
# first blocking wait.
b.put(b"\xb8\x85\x00\x00\x00")
b.put(b"\xcd\x21")
b.put(b"\x89\x45\xac")

# exec 53: TimerAfter(handle=ecx, status=edx, interval=ebx=50ms)
b.put(b"\x8b\x4d\xac")
b.put(b"\x8d\x55\xb4")                      # &active.iStatus = [ebp-0x4c]
b.put(b"\xbb\x50\xc3\x00\x00")
b.put(b"\x31\xf6")
b.put(b"\xb8\x35\x00\x00\x00")
b.put(b"\xcd\x21")

# Scheduler loop. The genuine EUSER CActiveScheduler::Start() ordinal does not
# dispatch RunL() in this port (its TLS/trap-harness state is unavailable to a
# hand-built E32 image), so run the same loop the e32base.h shim runs: block in
# the genuine User::WaitForAnyRequest() ordinal, then dispatch RunL() through
# the active's own vptr.  Stop() sets the local stop word; the genuine EUSER
# Stop() ordinal is imported/bound but not called (see RunL below).
LOOP_START = b.pos
b.put_abs(0xB8, GLOBAL_OFF + 4)             # mov eax,&stop word
b.put(b"\x8b\x00\x85\xc0")                  # mov eax,[eax]; test eax,eax
stop_jmp = b.pos
b.put(b"\x75\x00")                          # jnz -> done (patched below)
b.call_slot(15)                             # User::WaitForAnyRequest() -- int 0x20
b.put(b"\x81\x7d\xb4\x01\x00\x00\x80")      # cmp iStatus,KRequestPending
pending_jmp = b.pos
b.put(b"\x74\x00")                          # je -> loop (patched below)
# Use the pointer returned by the genuine CActive ctor (saved in GLOBAL),
# not the stack-local address passed as `this`: the ctor's return is the
# object the vptr was written into.
b.put_abs(0xB8, GLOBAL_OFF)                 # mov eax,&GLOBAL
b.put(b"\x8b\x00\x8b\x00\xff\x50\x08")      # active=*GLOBAL; vptr=*active; call [vptr+8]
back_jmp = b.pos
b.put(b"\xeb\x00")                          # jmp -> loop (patched below)
DONE_OFF = b.pos
b.code[stop_jmp + 1] = (DONE_OFF - (stop_jmp + 2)) & 0xFF
b.code[pending_jmp + 1] = (LOOP_START - (pending_jmp + 2)) & 0xFF
b.code[back_jmp + 1] = (LOOP_START - (back_jmp + 2)) & 0xFF

b.printf(4)                                 # "ca_test: Start() returned"
b.printf(5)                                 # "ca_test: clean exit"
# The kernel enters this thread at the E32 entry point with no return address
# on the user stack (user ESP points at the copied SThreadCreateInfo), so a
# plain `ret` here pops a create-info field as EIP (0x203) and faults with
# Exc 06. Kill this thread explicitly with exec 115 (EExecThreadKill) using
# the same register contract as user/start.S:process_exit and sTrampExit.
b.put(b"\x31\xc0")                          # xor eax,eax (exit reason)
b.put(b"\xc9")                              # leave (drop local frame)
b.put(b"\xb9\x01\x80\xff\xff")              # mov ecx,KCurrentThreadHandle
b.put(b"\x31\xd2")                          # xor edx,edx (EExitKill)
b.put(b"\x31\xdb")                          # xor ebx,ebx (reason 0)
b.put(b"\x31\xf6")                          # xor esi,esi (NULL category)
b.put(b"\xb8\x73\x00\x00\x00")              # mov eax,115
b.put(b"\xcd\x21")                          # int 0x21 (never returns)
b.put(b"\xeb\xfe")                          # jmp $ backstop (no Exc 06)
CODE_END = b.pos

# RunL(): dispatched by the scheduler loop through the active's vptr
RUNL_OFF = (b.pos + 15) & ~15
b.pad_to(RUNL_OFF, b"\x90")
b.printf(2)                                 # RunL() reached
b.put_abs(0xB8, GLOBAL_OFF + 4)             # set local stop word
b.put(b"\xc7\x00\x01\x00\x00\x00")
# The genuine EUSER Stop() ordinal hangs in this port's hand-built E32 image;
# the local stop word above is the scheduler's real stop signal.  Ordinal 425
# remains imported/bound (the marker documents the Stop() semantic).
b.printf(3)                                 # "ca_test: Stop() called" (before RunL returns)
b.put(b"\xc3")                              # ret -- back to Start()->DoRunL
RUNL_END = b.pos

assert RUNL_END <= TEXTSIZE
b.pad_to(IAT_END)
for k, (ordinal, _name) in enumerate(IMPORTS):
    struct.pack_into("<I", b.code, slot_off(k), ordinal)

STRINGS = [
    b"ca_test: CActiveScheduler::Install ok\x00",
    b"ca_test: Start() entered\x00",
    b"ca_test: RunL() reached\x00",
    b"ca_test: Stop() called\x00",
    b"ca_test: Start() returned\x00",
    b"ca_test: clean exit\x00",
]
b.pad_to(STR_OFF)
for i, s in enumerate(STRINGS):
    assert b.pos == STR_OFF + i * STR_STEP
    b.put(s)
    b.pad_to(STR_OFF + (i + 1) * STR_STEP)
STR_END = b.pos

b.pad_to(VTBL_OFF)
for _ in range(VTBL_NDWORDS):
    b.put_abs_dword(RUNL_OFF)
VTBL_END = b.pos
assert VTBL_END == GLOBAL_OFF
b.put(b"\x00\x00\x00\x00")
DATA_END = b.pos

max_site = max(b.reloc_sites)
CODE_SIZE = max(CODE_END, STR_END, DATA_END, max_site + 4, TEXTSIZE + 4 * NIMP)
CODE_SIZE = (CODE_SIZE + 15) & ~15
assert CODE_SIZE < 0x1000
assert all(s + 4 <= CODE_SIZE for s in b.reloc_sites)
b.pad_to(CODE_SIZE)
for site in b.reloc_sites:
    assert site + 4 <= CODE_SIZE
    if site not in b.dword_sites:
        assert b.code[site - 1] in (0xB8, 0x68)

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

NAME_OFF = 12
ISIZE = 8 + ((len(DLL_NAME) + 3) & ~3)
imp_section = struct.pack("<II", NAME_OFF, NIMP) + DLL_NAME
imp_section += b"\x00" * (ISIZE - len(imp_section))
assert len(imp_section) == ISIZE
IMP_OFF = CODE_OFF + CODE_SIZE
RELOC_OFF = IMP_OFF + 4 + ISIZE

header = bytearray(HEADER_SIZE)
uid1 = KExecutableImageUidValue
uid2, uid3 = 0, 0xCAFE
uid_chk = uid_checksum(uid1, uid2, uid3)
struct.pack_into("<4I", header, 0, uid1, uid2, uid3, uid_chk)
struct.pack_into("<I", header, 16, EPOC_SIG)
struct.pack_into("<I", header, 20, KImageCrcInitialiser)
struct.pack_into("<I", header, 24, 0x00010000)
struct.pack_into("<I", header, 28, 0)
struct.pack_into("<4B", header, 32, 1, 0, 0, 0)
struct.pack_into("<2I", header, 36, 0, 0)
struct.pack_into("<I", header, 44, KImageHdrFmt_V | 0x20000000)
struct.pack_into("<6i", header, 48, CODE_SIZE, 0, 0x1000, 0x100000, 0x2000, 0)
struct.pack_into("<I", header, 72, 0)
struct.pack_into("<2I", header, 76, CODE_BASE, 0)
struct.pack_into("<i", header, 84, 1)
struct.pack_into("<I", header, 88, 0)
struct.pack_into("<i", header, 92, 0)
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
struct.pack_into("<I", header, 20, KImageCrcInitialiser)
crc = crc32_symbian(bytes(header), 0)
struct.pack_into("<I", header, 20, crc)

file_data = bytes(header) + bytes(b.code) + b"\x00" * (CODE_SIZE - len(b.code))
file_data += struct.pack("<I", ISIZE) + bytes(imp_section)
file_data += struct.pack("<2I", RELOC_SIZE, reloc_count) + bytes(reloc_blocks)

out = "/tmp/opencode/ca_test.e32"
for i, a in enumerate(sys.argv):
    if a == "--output" and i + 1 < len(sys.argv):
        out = sys.argv[i + 1]
with open(out, "wb") as f:
    f.write(file_data)

print(f"WROTE {out} ({len(file_data)} bytes)")
print(f"  codeSize={CODE_SIZE:#x} code_end={CODE_END:#x} runl={RUNL_OFF:#x}-{RUNL_END:#x} "
      f"vtbl={VTBL_OFF:#x}-{VTBL_END:#x} global={GLOBAL_OFF:#x} str={STR_OFF:#x}-{STR_END:#x} "
      f"nimports={NIMP}")
print(f"  imports={[(k, o) for k, (o, _n) in enumerate(IMPORTS)]}")
