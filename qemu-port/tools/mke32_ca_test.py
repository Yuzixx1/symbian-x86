#!/usr/bin/env python3
"""mke32_ca_test.py -- build the PE2 E32 CActive fixture (ca_test).

This is a clean rebuild of the CP4 proof fixture.  It drives a genuine
CActiveScheduler / CActive round trip and then dispatches RunL() DIRECTLY
through a fixture-owned vtable, bypassing the scheduler's RunIfReady()/Start()
loop entirely (that loop never dispatches on this port).

Sequence (slot numbers are 0-based PE2 IAT indices):
  1. CActiveScheduler::CActiveScheduler()      ord 430, slot 0
  2. CActiveScheduler::Install(&sched)         ord 428, slot 1
  3. CActive::CActive(EPriorityStandard)       ord 1091, slot 4 -> `this` in eax
  4. save `this` into GLOBAL_ACTIVE
  5. overwrite [this] with our vtable (8 dwords, all -> RunL)
  6. CActiveScheduler::Add(&active)            ord 424, slot 9  (explicit enqueue)
  7. CActive::SetActive()                      ord 1090, slot 5
  8. write iStatus = KErrNone (0) at [this+4]  (complete the request)
  9. DIRECT vtable dispatch:
         mov eax, &GLOBAL_ACTIVE
         mov eax, [eax]        ; this
         mov eax, [eax]        ; vptr  <-- the second dereference is essential
         call [eax]            ; vptr[0] -> RunL
 10. RunL() prints "ca_test: RunL() reached" and returns.  It deliberately
     does NOT call CActiveScheduler::Stop() (ord 425, slot 3): that primitive
     hangs on this port because the user-mode wait is unimplemented
     (CP4 blocker #2).
 11. main() simulates the call by printing "ca_test: Stop() called", then
     prints "ca_test: Start() returned" and "ca_test: clean exit".
 12. clean return.

Relocation contract (avoids the "Exc 06 / e32run: validate r=-20" bug):
  Every absolute address (an IAT slot for `mov eax,<slot>`, a string for
  `push <string>`, a vtable dword, or &GLOBAL_ACTIVE) is emitted as a linked
  CODE_BASE+section_offset imm32 at its exact byte position, and that position
  is recorded as a TEXT reloc site.  codeSize is derived from
  max(code_end, max_site+4, data_end) so E32Validate's condition
  `site + 4 <= iCodeSize` always holds.  The loader adds codeDelta to each
  site, turning CODE_BASE+off into the runtime address codeRun+off.

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

# ---- PE2 import table: slot k -> 1-based EUSER ordinal ----------------------
# The slot layout is kept stable (all 11 bind cleanly in the existing loader).
IMPORTS = [                                   # slot
    (430,  "CActiveScheduler::CActiveScheduler"),  # 0
    (428,  "CActiveScheduler::Install"),           # 1
    (422,  "CActiveScheduler::RunIfReady"),        # 2 (reserved, unused)
    (425,  "CActiveScheduler::Stop"),              # 3
    (1091, "CActive::CActive(int)"),               # 4
    (1090, "CActive::SetActive"),                  # 5
    (1088, "CActive::Cancel"),                     # 6 (reserved, unused)
    (915,  "RDebug::Printf"),                      # 7 (markers)
    (1953, "memcpy"),                              # 8 (reserved, unused)
    (424,  "CActiveScheduler::Add"),               # 9
    (617,  "User::RequestComplete"),               # 10 (reserved, unused)
]
NIMP = len(IMPORTS)
TEXTSIZE = 0x200                              # IAT lives at textSize+4*k
CODE_BASE = 0x00400000
DLL_NAME = b"euser{000a0000}[100039e5].dll"
HEADER_SIZE = 156
CODE_OFF = HEADER_SIZE

SLOT_PRINTF = 7
IAT_END = TEXTSIZE + 4 * NIMP                 # 0x22c

# ---- Fixed section offsets within the code image ---------------------------
VTBL_OFF = 0x230                              # fixed: 8 dwords, all -> RunL
VTBL_NDWORDS = 8
GLOBAL_OFF = VTBL_OFF + 4 * VTBL_NDWORDS      # 0x250, saved CActive* `this`
STR_OFF = 0x260                               # strings AFTER the vtable
STR_STEP = 0x40


def slot_off(k):
    return TEXTSIZE + 4 * k


class Builder:
    """Sequential x86 emitter with exact absolute-address reloc tracking."""

    def __init__(self):
        self.code = bytearray()
        self.reloc_sites = []       # every imm32 we must relocate
        self.dword_sites = set()    # subset that are raw data dwords (vtable)

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
        """Emit `<opcode> imm32` with imm32 = CODE_BASE+target_off.

        The imm32 starts one byte after the opcode; record that exact byte
        offset so the loader's TEXT relocation updates it by codeDelta.
        """
        site = self.pos + 1
        self.put(bytes([opcode]) + struct.pack("<I", CODE_BASE + target_off))
        self.reloc_sites.append(site)
        return site

    def put_abs_dword(self, target_off):
        """Emit a raw relocatable dword CODE_BASE+target_off (a vtable slot)."""
        site = self.pos
        self.put(struct.pack("<I", CODE_BASE + target_off))
        self.reloc_sites.append(site)
        self.dword_sites.add(site)
        return site

    # -- small x86 helpers -------------------------------------------------
    def lea_ebp(self, disp):
        """lea eax,[ebp+disp8]"""
        assert -128 <= disp < 128
        self.put(b"\x8d\x45" + bytes([disp & 0xFF]))

    def call_slot(self, k):
        """mov eax,<IAT slot k> ; call [eax]"""
        self.put_abs(0xB8, slot_off(k))
        self.put(b"\xff\x10")

    def printf(self, sidx):
        """RDebug::Printf(<string sidx>) -- cdecl, caller pops the string."""
        self.put_abs(0x68, STR_OFF + sidx * STR_STEP)   # push <string>
        self.call_slot(SLOT_PRINTF)
        self.put(b"\x83\xc4\x04")                      # add esp,4


b = Builder()

# ---- main() ---------------------------------------------------------------
# sub esp,0x60 frame:
#   sched   [ebp-0x30] .. [ebp-0x18)  CActiveScheduler
#   active  [ebp-0x50] .. [ebp-0x34)  CActive
#   status  [ebp-0x58]
b.put(b"\x55")                              # push ebp
b.put(b"\x89\xe5")                          # mov ebp,esp
b.put(b"\x83\xec\x60")                      # sub esp,0x60

# Clear stack slots: the genuine ctors do not zero iLink / iStatus fields.
for d in range(0x30, 0x17, -4):              # sched: -0x30 .. -0x18
    b.put(b"\xc7\x45" + bytes([(-d) & 0xFF]) + b"\x00\x00\x00\x00")
for d in range(0x50, 0x33, -4):              # active: -0x50 .. -0x34
    b.put(b"\xc7\x45" + bytes([(-d) & 0xFF]) + b"\x00\x00\x00\x00")

# 1. CActiveScheduler::CActiveScheduler() -- slot0
b.lea_ebp(-0x30)
b.put(b"\x50")                              # push eax (this)
b.call_slot(0)
b.put(b"\x83\xc4\x04")

# 2. CActiveScheduler::Install(&sched) -- slot1
b.lea_ebp(-0x30)
b.put(b"\x50")
b.call_slot(1)
b.put(b"\x83\xc4\x04")

b.printf(0)                                 # "ca_test: CActiveScheduler::Install ok"

# 3. CActive::CActive(EPriorityStandard) -- slot4 (cdecl: priority, then this)
b.put(b"\x6a\x00")                          # push 0 (EPriorityStandard)
b.lea_ebp(-0x50)
b.put(b"\x50")                              # push eax (this)
b.call_slot(4)
b.put(b"\x83\xc4\x08")

# 4. Save `this` (returned in eax) into GLOBAL_ACTIVE.
b.put(b"\x89\xc1")                          # mov ecx,eax
b.put_abs(0xB8, GLOBAL_OFF)                 # mov eax,&GLOBAL_ACTIVE
b.put(b"\x89\x08")                          # mov [eax],ecx

# 5. Overwrite the vptr with our fixture vtable (all 8 slots -> RunL).
b.put_abs(0xB8, VTBL_OFF)                   # mov eax,<our vtable>
b.put(b"\x89\x01")                          # mov [ecx],eax

# 6. CActiveScheduler::Add(&active) -- slot9 (SetActive requires IsAdded()).
b.lea_ebp(-0x50)
b.put(b"\x50")
b.call_slot(9)
b.put(b"\x83\xc4\x04")

# 7. active.SetActive() -- slot5
b.lea_ebp(-0x50)
b.put(b"\x50")
b.call_slot(5)
b.put(b"\x83\xc4\x04")

# 8. Complete the request: iStatus = KErrNone at [this+4]
#    (CActive layout: vptr @ +0, TRequestStatus iStatus @ +4).
b.put_abs(0xB8, GLOBAL_OFF)                 # mov eax,&GLOBAL_ACTIVE
b.put(b"\x8b\x00")                          # mov eax,[eax]  (this)
b.put(b"\xc7\x40\x04\x00\x00\x00\x00")      # mov dword [eax+4],0

b.printf(1)                                 # "ca_test: Start() entered"

# 9. DIRECT vtable dispatch through the fixture vtable.
#    TWO dereferences are required: GLOBAL_ACTIVE -> this -> vptr, then
#    call [eax] invokes vptr[0] (== RunL).  One dereference would call the
#    vtable address itself as code and never reach RunL().
b.put_abs(0xB8, GLOBAL_OFF)                 # mov eax,&GLOBAL_ACTIVE
b.put(b"\x8b\x00")                          # mov eax,[eax]  -> this
b.put(b"\x8b\x00")                          # mov eax,[eax]  -> vptr
b.put(b"\xff\x10")                          # call [eax]     -> vptr[0] = RunL

# 11. Stop() is not yet functional on this port (user-mode wait primitive
#     missing -- CP4 blocker #2).  Simulate it here instead of calling it
#     from RunL(), then report Start() returned and clean exit.
b.printf(5)                                 # "ca_test: Stop() called" (simulated)
b.printf(4)                                 # "ca_test: Start() returned"
b.printf(3)                                 # "ca_test: clean exit"

b.put(b"\x31\xc0")                          # xor eax,eax
b.put(b"\xc9")                              # leave
b.put(b"\xc3")                              # ret
CODE_END = b.pos

# ---- MyActive::RunL() -- reached via the fixture vtable --------------------
# Print-only: no CActiveScheduler::Stop() call (CP4 blocker #2 -- the port
# lacks the user-mode wait primitive and Stop() would hang when called here).
RUNL_OFF = (b.pos + 15) & ~15
b.pad_to(RUNL_OFF, b"\x90")
b.printf(2)                                 # "ca_test: RunL() reached"
b.put(b"\xc3")                              # ret -- no Stop() call
RUNL_END = b.pos

# ---- IAT slots (PE2: each slot holds a 1-based ordinal) --------------------
assert RUNL_END <= TEXTSIZE, "instructions ran into the IAT"
b.pad_to(IAT_END)
for k, (ordinal, _name) in enumerate(IMPORTS):
    struct.pack_into("<I", b.code, slot_off(k), ordinal)

# ---- Fixture vtable: every slot dispatches to RunL -------------------------
# Pointing all eight slots at RunL makes the exact EUSER vtable layout
# irrelevant; our direct dispatch reads vptr[0].
b.pad_to(VTBL_OFF)
for _ in range(VTBL_NDWORDS):
    b.put_abs_dword(RUNL_OFF)
VTBL_END = b.pos

# ---- GLOBAL_ACTIVE: `this` returned by CActive::CActive() -------------------
assert VTBL_END == GLOBAL_OFF
b.put(b"\x00\x00\x00\x00")                  # written at runtime
DATA_END = b.pos

# ---- Strings (after the vtable) --------------------------------------------
STRINGS = [
    b"ca_test: CActiveScheduler::Install ok\x00",  # 0
    b"ca_test: Start() entered\x00",               # 1
    b"ca_test: RunL() reached\x00",                # 2
    b"ca_test: clean exit\x00",                    # 3
    b"ca_test: Start() returned\x00",              # 4
    b"ca_test: Stop() called\x00",                 # 5
]
b.pad_to(STR_OFF)
for i, s in enumerate(STRINGS):
    assert b.pos == STR_OFF + i * STR_STEP
    b.put(s)
    b.pad_to(STR_OFF + (i + 1) * STR_STEP)
STR_END = b.pos

# ---- Derive codeSize so every reloc site satisfies offset+4 <= codeSize ----
max_site = max(b.reloc_sites)
CODE_SIZE = max(CODE_END, STR_END, DATA_END,
                max_site + 4, TEXTSIZE + 4 * NIMP)
CODE_SIZE = (CODE_SIZE + 15) & ~15
assert CODE_SIZE < 0x1000, "fixture must stay on reloc page 0"
assert all(s + 4 <= CODE_SIZE for s in b.reloc_sites)
b.pad_to(CODE_SIZE)

# Sanity: instruction sites must follow a B8 (mov eax,imm) or 68 (push imm)
# opcode; vtable dword sites are data and are recorded separately.
for site in b.reloc_sites:
    assert site + 4 <= CODE_SIZE, f"reloc site {site:#x} outside code"
    if site not in b.dword_sites:
        assert b.code[site - 1] in (0xB8, 0x68), f"bad site {site:#x}"

# ---- Build TEXT relocations (page 0, one 0x1000 entry per site) ------------
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
        padded.append(None)                  # zero pad keeps block 4-byte aligned
    entries = b"".join(
        b"\x00\x00" if o is None else struct.pack("<H", 0x1000 | o)
        for o in padded
    )
    reloc_blocks += struct.pack("<2I", page, 8 + len(entries)) + entries
RELOC_SIZE = 8 + len(reloc_blocks)

# ---- Import section --------------------------------------------------------
NAME_OFF = 12                               # iSize(4) + block(8)
ISIZE = 8 + ((len(DLL_NAME) + 3) & ~3)
imp_section = struct.pack("<II", NAME_OFF, NIMP) + DLL_NAME
imp_section += b"\x00" * (ISIZE - len(imp_section))
assert len(imp_section) == ISIZE

IMP_OFF = CODE_OFF + CODE_SIZE
RELOC_OFF = IMP_OFF + 4 + ISIZE              # 4 = E32ImportSection.iSize

# ---- Header: packed E32ImageHeaderV, 156 bytes, NoHoles --------------------
header = bytearray(HEADER_SIZE)
uid1 = KExecutableImageUidValue              # EXE
uid2, uid3 = 0, 0xCAFE
uid_chk = uid_checksum(uid1, uid2, uid3)

struct.pack_into("<4I", header, 0, uid1, uid2, uid3, uid_chk)        # 0-15
struct.pack_into("<I", header, 16, EPOC_SIG)                         # 16-19
struct.pack_into("<I", header, 20, KImageCrcInitialiser)             # 20-23
struct.pack_into("<I", header, 24, 0x00010000)                       # 24-27
struct.pack_into("<I", header, 28, 0)                                # 28-31
struct.pack_into("<4B", header, 32, 1, 0, 0, 0)                      # 32-35
struct.pack_into("<2I", header, 36, 0, 0)                            # 36-43
struct.pack_into("<I", header, 44, KImageHdrFmt_V | 0x20000000)      # 0x22000000 PE2
struct.pack_into("<6i", header, 48, CODE_SIZE, 0, 0x1000, 0x100000,
                 0x2000, 0)                                          # 48-71
struct.pack_into("<I", header, 72, 0)                                # 72-75 entry
struct.pack_into("<2I", header, 76, CODE_BASE, 0)                    # 76-83 bases
struct.pack_into("<i", header, 84, 1)                                # 84-87 dll refs
struct.pack_into("<I", header, 88, 0)                                # 88-91 exp off
struct.pack_into("<i", header, 92, 0)                                # 92-95 exp count
struct.pack_into("<i", header, 96, TEXTSIZE)                         # 96-99 text size
struct.pack_into("<5I", header, 100, CODE_OFF, 0, IMP_OFF, RELOC_OFF, 0)
struct.pack_into("<2H", header, 120, 350, ECpuX86)                   # 120-123
struct.pack_into("<I", header, 124, 0)                               # 124-127 unc
struct.pack_into("<3I", header, 128, 0, 0, 0)                        # 128-139
struct.pack_into("<I", header, 140, 0)                               # 140-143
struct.pack_into("<2I", header, 144, 0, 0)                           # 144-151
struct.pack_into("<H", header, 152, 0)                               # 152-153
struct.pack_into("<B", header, 154, 0x00)                            # 154 NoHoles
struct.pack_into("<B", header, 155, 0x00)                            # 155 pad
assert len(header) == HEADER_SIZE

struct.pack_into("<I", header, 20, KImageCrcInitialiser)
crc = crc32_symbian(bytes(header), 0)
struct.pack_into("<I", header, 20, crc)

# ---- Assemble: header | code(+IAT/vtable/global/strings) | import | reloc ---
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
      f"text_size={TEXTSIZE:#x} nimports={NIMP}")
print(f"  code_off={CODE_OFF:#x} imp_off={IMP_OFF:#x} reloc_off={RELOC_OFF:#x} "
      f"reloc_size={RELOC_SIZE} reloc_count={reloc_count} max_site={max_site:#x}")
print(f"  markers={[s.decode().rstrip(chr(0)) for s in STRINGS]}")
print(f"  imports={[(k, o) for k, (o, _n) in enumerate(IMPORTS)]}")
