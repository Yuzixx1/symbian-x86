#!/usr/bin/env python3
"""mke32.py -- minimal valid E32 EXE builder (host-side, no Symbian toolchain).

Emits an import-free, uncompressed E32ImageHeaderV image for ECpuX86 that
passes the genuine validator in
  kernelhwsrv/userlibandfileserver/fileserver/inc/f32image.h
  (E32ImageHeaderV::ValidateHeader / ValidateWholeImage).

Layout reference (f32image.h:122-134): header, code part
(.text + 12-byte code-seg-id slot), then rest-of-data (empty here).

Field order / constants: see f32image.h and e32ldr_private.h.
CRC32 table + UID checksum: kernelhwsrv/kernel/eka/euser/us_func.cpp
  (CrcTab32, crcTab, TCheckedUid::Check, KMaxCheckedUid=3).

Usage:
  python3 tools/mke32.py build/hello.e32 [--uid2 0x1000ABCD ...]
"""
import struct
import sys
import time

# --- genuine constants -------------------------------------------------------
KImageCrcInitialiser = 0xC90FDAA2
KExecutableImageUidValue = 0x1000007A
KDynamicLibraryUidValue = 0x10000079
KImageDll = 0x00000001
EPOC_SIG = 0x434F5045  # 'EPOC' little-endian
ECpuX86 = 0x1000
KImageHdrFmt_V = 0x02000000
KImageImpFmt_ELF = 0x10000000
KFormatNotCompressed = 0
KCodeSegIdOffset = 12

CRC_TAB_16 = (
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,0x8108,0x9129,0xa14a,
    0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,
    0x72f7,0x62d6,0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,0x2462,
    0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,0xa56a,0xb54b,0x8528,0x9509,
    0xe5ee,0xf5cf,0xc5ac,0xd58d,0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,
    0x46b4,0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,0x48c4,0x58e5,
    0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,
    0x9969,0xa90a,0xb92b,0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
    0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,0x6ca6,0x7c87,0x4ce4,
    0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,
    0x8d68,0x9d49,0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,0xff9f,
    0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,0x9188,0x81a9,0xb1ca,0xa1eb,
    0xd10c,0xc12d,0xf14e,0xe16f,0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,
    0x6067,0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,0x02b1,0x1290,
    0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,
    0xe54f,0xd52c,0xc50d,0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,0x26d3,0x36f2,0x0691,
    0x16b0,0x6657,0x7676,0x4615,0x5634,0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,
    0xb98a,0xa9ab,0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,0xcb7d,
    0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,0x4a75,0x5a54,0x6a37,0x7a16,
    0x0af1,0x1ad0,0x2ab3,0x3a92,0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,
    0x8dc9,0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,0xef1f,0xff3e,
    0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,
    0x3eb2,0x0ed1,0x1ef0,
)

CRC_TAB_32 = (
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
    0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
    0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
    0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
    0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
    0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
    0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
    0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
    0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
    0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
    0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
    0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
    0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
    0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
    0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
    0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
    0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
    0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
    0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
    0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
    0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
    0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
    0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5C06D,0x7CDCEFB7,0x0BDBDF21,
    0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
    0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
    0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
    0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
    0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
    0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
)


def crc16_ccitt(data: bytes, init: int = 0) -> int:
    crc = init
    for b in data:
        crc = ((crc << 8) ^ CRC_TAB_16[((crc >> 8) ^ b) & 0xFF]) & 0xFFFF
    return crc


def crc32_symbian(data: bytes, init: int = 0) -> int:
    crc = init & 0xFFFFFFFF
    for b in data:
        crc = ((crc >> 8) ^ CRC_TAB_32[(crc ^ b) & 0xFF]) & 0xFFFFFFFF
    return crc


def uid_checksum(uid1: int, uid2: int, uid3: int) -> int:
    raw = struct.pack("<3I", uid1 & 0xFFFFFFFF, uid2 & 0xFFFFFFFF, uid3 & 0xFFFFFFFF)
    even = bytes(raw[0::2])  # offsets 0,2,4,6,8,10
    odd = bytes(raw[1::2])   # offsets 1,3,5,7,9,11
    lo = crc16_ccitt(even)
    hi = crc16_ccitt(odd)
    return ((hi << 16) | lo) & 0xFFFFFFFF


def build_e32(uid2: int, uid3: int, secure_id: int, spin: bool = False,
              prio: int = 350, noheap: bool = False, nodata: bool = False) -> bytes:
    header_size = 156  # iCodeOffset for NoHoles V header (packed layout)
    code_size = 512
    data_base = 0x00500000
    # .data (16B): signature + tag the demo reads through a relocated
    # absolute address (proves data copy + fixups, see reloc section).
    data = bytearray(16)
    struct.pack_into("<I", data, 0, 0xDEADBEEF)
    data[4:8] = b"DATA"
    # x86 demo at offset 0: draws "HI!", loads [dataSig] via a reloc'd
    # absolute DWORD (linked dataBase+0), compares with 0xDEADBEEF, draws
    # 'D' (green) or 'X' (red), then exits via ThreadKill slow exec.
    code = bytearray(code_size)
    DATASIG = 0x00500000  # linked address of data[0]; loader adds dataDelta
    BSSSIG = 0x00500010   # linked address of bss[0] (dataBase+16)
    stub = bytes([
        0xBF, 0xC0, 0x0D, 0xB8, 0x00,  # mov edi,0xB80DC0 (VGA row 22)
        0xB8, 0x48, 0x0E, 0x49, 0x0E,  # mov eax,"HI"
        0x89, 0x07,                    # mov [edi],eax
        0xB8, 0x21, 0x0E, 0x20, 0x0E,  # mov eax,"! "
        0x89, 0x47, 0x04,              # mov [edi+4],eax
        0xA1,
    ]) + struct.pack("<I", DATASIG) + bytes([
        0x3D, 0xEF, 0xBE, 0xAD, 0xDE,  # cmp eax,0xDEADBEEF
        0xB8, 0x58, 0x0C, 0x20, 0x0C,  # mov eax,"X " red (default)
        0x75, 0x05,                    # jne +5 (mismatch: keep red)
        0xB8, 0x44, 0x0A, 0x20, 0x0A,  # mov eax,"D " green (match)
        0x89, 0x47, 0x08,              # mov [edi+8],eax
        0xA1,
    ]) + struct.pack("<I", BSSSIG) + bytes([
        # NOTE: A1 already loaded the BSS *content* into eax — no second
        # deref (an earlier `mov eax,[eax]` here read address 0 and its
        # BIOS-area garbage 0xF000FF53 drew a bogus '3' for weeks).
        0x89, 0xC3,                    # mov ebx,eax (save full value)
        0x83, 0xE0, 0x0F,              # and eax,15 (low nibble)
        0x83, 0xC0, 0x30,              # add eax,'0'
        0x83, 0xF8, 0x39,              # cmp eax,'9'
        0x76, 0x03,                    # jbe +3 (digit: skip +7)
        0x83, 0xC0, 0x07,              # add eax,7 (a-f)
        0x88, 0x47, 0x0C,              # mov [edi+12],al (hex char)
        0xC6, 0x47, 0x0D, 0x0E,        # mov byte [edi+13],0x0E (yellow)
    ])
    # heap test: private chunk via EExecChunkCreate=107, bump-alloc two
    # dwords via EExecChunkBase=1, verify by readback, draw H/X at [edi+16].
    # TChunkCreate (24B) built on the user stack; SLOW_EXEC frame replicated.
    hb = bytearray()
    # Mutex probe first (EExecMutexCreate=111, SLOW_EXEC2 frame): tells
    # whether kernel-object creation generally works for our processes
    # ('m' green) or fails like ChunkCreate ('M' red). Draw at +24.
    hb += bytes([0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00])      # push 0,0,0 (fake ret+args)
    hb += bytes([0xB8, 0x6F, 0x00, 0x00, 0x00])            # mov eax,111
    hb += bytes([0x8B, 0x4C, 0x24, 0x04, 0x8B, 0x54, 0x24, 0x08,
                 0xCD, 0x21, 0x83, 0xC4, 0x0C])            # ecx,edx; int; add esp,12
    hb += bytes([0x89, 0x44, 0x24, 0xFC])                 # mov [esp-4],eax (stash mutex h; =P+60 after sub)
    hb += bytes([0x85, 0xC0])                              # test eax,eax
    hb += bytes([0x78, 0x05])                              # js +5 (fail)
    hb += bytes([0xB8, 0x6D, 0x0A, 0x20, 0x0A])            # mov eax,"m " green
    hb += bytes([0xEB, 0x05])                              # jmp +5
    hb += bytes([0xB8, 0x4D, 0x0C, 0x20, 0x0C])            # mov eax,"M " red
    hb += bytes([0x89, 0x47, 0x18])                        # mov [edi+24],eax
    hb += bytes([0x83, 0xEC, 0x40])                       # sub esp,64
    # Zero the whole 64B scratch first (pad bytes after the 24B struct
    # must not be garbage), then build TChunkCreate (genuine us_ksvr.cpp
    # conversion: att=0, force=0, bottom=0, top=max=4096, clear=0x03).
    hb += bytes([0x33, 0xC0])                             # xor eax,eax
    hb += bytes([0x89, 0x04, 0x24])                       # mov [esp],eax
    hb += bytes([0x89, 0x44, 0x24, 0x04])                 # mov [esp+4],eax
    hb += bytes([0x89, 0x44, 0x24, 0x08])                 # mov [esp+8],eax
    hb += bytes([0xC7, 0x44, 0x24, 0x0C, 0x00, 0x10, 0x00, 0x00])  # +12=4096
    hb += bytes([0xC7, 0x44, 0x24, 0x10, 0x00, 0x10, 0x00, 0x00])  # +16=4096
    hb += bytes([0xC6, 0x44, 0x24, 0x14, 0x03])            # +20=3 (clear, genuine dflt)
    hb += bytes([0x89, 0x44, 0x24, 0x18])                 # mov [esp+24],eax (pad)
    hb += bytes([0x89, 0x44, 0x24, 0x1C])                 # mov [esp+28],eax (pad)
    hb += bytes([0x89, 0x44, 0x24, 0x20])                 # mov [esp+32],eax (pad)
    hb += bytes([0xC7, 0x44, 0x24, 0x38, 0xFF, 0xFF, 0xFF, 0xFF])  # [esp+56]=-1 (chunk stash init; close=>err)
    hb += bytes([0x8D, 0x04, 0x24])                       # lea eax,[esp]
    hb += bytes([0x50, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00])  # push struct,0,0,0
    hb += bytes([0xB8, 0x6B, 0x00, 0x00, 0x00, 0x53])      # mov eax,107; push ebx
    # NOTE: arg registers computed WITHOUT a stack-slot load: after the
    # 5 pushes above, ESP = P-20 so ECX arg1 = [ESP+8] (a 0 slot),
    # EDX arg2 = [ESP+12] (a 0 slot), and EBX arg3 = ESP+20 = P.
    # (A `mov ebx,[esp+16]` here loaded 0 in every live test despite the
    # slot holding P — see README; recompute instead of reloading.)
    hb += bytes([0x8B, 0x4C, 0x24, 0x08, 0x8B, 0x54, 0x24, 0x0C,
                 0x8B, 0xDC, 0x83, 0xC3, 0x14, 0xCD, 0x21, 0x5B, 0x83, 0xC4, 0x10])
    # ... = mov ecx,[esp+8]; mov edx,[esp+12]; mov ebx,esp; add ebx,20;
    #        int 0x21; pop ebx; add esp,16
    hb += bytes([0x85, 0xC0])                              # test eax,eax
    hb += bytes([0x0F, 0x88, 0x00, 0x00, 0x00, 0x00])      # js FAIL1 (near, patched)
    h_fail1 = len(hb) - 4
    hb += bytes([0x89, 0xC6])                              # mov esi,eax (handle)
    hb += bytes([0x89, 0x44, 0x24, 0x38])                 # mov [esp+56],eax (stash chunk h; esp=P here)
    hb += bytes([0x56, 0x6A, 0x00])                       # push esi, push 0
    hb += bytes([0xB8, 0x01, 0x00, 0x00, 0x00])            # mov eax,1 (Base)
    hb += bytes([0x8B, 0x4C, 0x24, 0x04, 0xCD, 0x21, 0x83, 0xC4, 0x08])
    hb += bytes([0x85, 0xC0])                              # test eax,eax
    hb += bytes([0x0F, 0x84, 0x00, 0x00, 0x00, 0x00])      # jz FAIL2 (near, patched)
    h_fail2 = len(hb) - 4
    hb += bytes([0x89, 0xC6])                              # mov esi,eax (base)
    hb += bytes([0xC7, 0x06, 0x78, 0x56, 0x34, 0x12])      # [esi]=0x12345678
    hb += bytes([0xC7, 0x46, 0x04, 0xF0, 0xDE, 0xBC, 0x9A])  # [esi+4]=...
    hb += bytes([0x8B, 0x06, 0x3D, 0x78, 0x56, 0x34, 0x12])  # eax=[esi];cmp
    hb += bytes([0x0F, 0x85, 0x00, 0x00, 0x00, 0x00])      # jne FAIL3 (near, patched)
    h_fail3 = len(hb) - 4
    hb += bytes([0x8B, 0x46, 0x04, 0x3D, 0xF0, 0xDE, 0xBC, 0x9A])  # eax=[esi+4];cmp
    hb += bytes([0x0F, 0x85, 0x00, 0x00, 0x00, 0x00])      # jne FAIL4 (near, patched)
    h_fail4 = len(hb) - 4
    hb += bytes([0xB8, 0x48, 0x0A, 0x20, 0x0A])            # mov eax,"H " green
    hb += bytes([0xEB, 0x00])                              # jmp CLOSE (patched)
    h_ok = len(hb) - 1
    # Per-stage FAIL blocks: draw '1'..'4' red so serial/VGA tells which
    # exec step failed (1=ChunkCreate, 2=ChunkBase, 3/4=readback).
    # FAIL1 additionally draws -errno (low byte, 2 hex chars) at +20/+22
    # so we get the actual KErr code, not just the stage.
    fail_labels = []
    for idx, digit in enumerate((0x31, 0x32, 0x33, 0x34)):
        fail_labels.append(len(hb))
        if idx == 0:
            # EAX = negative KErr. ebx = -eax; draw '1' then hex byte.
            hb += bytes([0x89, 0xC3])                     # mov ebx,eax
            hb += bytes([0xF7, 0xDB])                     # neg ebx
            hb += bytes([0xB8, digit, 0x0C, 0x20, 0x0C])  # mov eax,"1 " red
            hb += bytes([0x89, 0x47, 0x10])               # mov [edi+16],eax
            # hex hi nibble of bl -> [edi+20], lo nibble -> [edi+22]
            hb += bytes([0x88, 0xDF])                     # mov bh,bl
            hb += bytes([0xC0, 0xEB, 0x04])               # shr bl,4
            hb += bytes([0x80, 0xE3, 0x0F])               # and bl,15
            hb += bytes([0x80, 0xFB, 0x0A])               # cmp bl,10
            hb += bytes([0x72, 0x03])                     # jb +3 (skip add 7)
            hb += bytes([0x80, 0xC3, 0x07])               # add bl,7
            hb += bytes([0x80, 0xC3, 0x30])               # add bl,'0'
            hb += bytes([0x88, 0x5F, 0x14])               # mov [edi+20],bl
            hb += bytes([0x88, 0xFB])                     # mov bl,bh
            hb += bytes([0x80, 0xE3, 0x0F])               # and bl,15
            hb += bytes([0x80, 0xFB, 0x0A])               # cmp bl,10
            hb += bytes([0x72, 0x03])                     # jb +3 (skip add 7)
            hb += bytes([0x80, 0xC3, 0x07])               # add bl,7
            hb += bytes([0x80, 0xC3, 0x30])               # add bl,'0'
            hb += bytes([0x88, 0x5F, 0x16])               # mov [edi+22],bl
            hb += bytes([0xC6, 0x47, 0x15, 0x0C])         # attr red
            hb += bytes([0xC6, 0x47, 0x17, 0x0C])         # attr red
            hb += bytes([0xB8, digit, 0x0C, 0x20, 0x0C])  # restore eax="1 "
        else:
            hb += bytes([0xB8, digit, 0x0C, 0x20, 0x0C])  # mov eax,"N " red
        hb += bytes([0xEB, 0x00])                          # jmp CLOSE (patched)
        fail_labels.append(len(hb) - 1)  # jmp slot, patched below
    # NOTE: fail_labels = [f1_addr, f1_jmp, f2_addr, f2_jmp, ...]
    # CLOSE: shut our chunk+mutex handles (EExecHandleClose=106, SLOW_EXEC1;
    # errors ignored) so exited processes leave no handle residue, then
    # restore esp and fall into END. esp=P here; stashes at +56/+60.
    h_close = len(hb)
    hb += bytes([0x89, 0xC6])                              # mov esi,eax (save draw char)
    hb += bytes([0x8B, 0x44, 0x24, 0x38, 0x50, 0x6A, 0x00,  # eax=[esp+56]; push eax,0
                 0xB8, 0x6A, 0x00, 0x00, 0x00,              # mov eax,106
                 0x8B, 0x4C, 0x24, 0x04, 0xCD, 0x21, 0x83, 0xC4, 0x08])  # ecx; int; add esp,8
    hb += bytes([0x8B, 0x44, 0x24, 0x3C, 0x50, 0x6A, 0x00,  # eax=[esp+60]; push eax,0
                 0xB8, 0x6A, 0x00, 0x00, 0x00,              # mov eax,106
                 0x8B, 0x4C, 0x24, 0x04, 0xCD, 0x21, 0x83, 0xC4, 0x08])  # ecx; int; add esp,8
    hb += bytes([0x8B, 0xC6])                              # mov eax,esi (restore draw char)
    hb += bytes([0x83, 0xC4, 0x40])                        # add esp,64 (restore)
    h_end = len(hb)
    hb += bytes([0x89, 0x47, 0x10])                        # END: mov [edi+16],eax
    fail_addrs = [fail_labels[0], fail_labels[2], fail_labels[4], fail_labels[6]]
    fail_jmps = [fail_labels[1], fail_labels[3], fail_labels[5], fail_labels[7]]
    import struct as _st
    for pos, tgt in zip((h_fail1, h_fail2, h_fail3, h_fail4), fail_addrs):
        # near jcc: pos points at rel32 start, insn ends at pos+4
        _st.pack_into("<i", hb, pos, tgt - (pos + 4))
    hb[h_ok] = (h_close - (h_ok + 1)) & 0xFF
    for jmp in fail_jmps:
        hb[jmp] = (h_close - (jmp + 1)) & 0xFF
    assert 0 < hb[h_ok] < 120, f"ok jump out of range {hb[h_ok]}"
    for jmp in fail_jmps:
        assert 0 <= hb[jmp] < 120, f"fail-end jump out of range {hb[jmp]}"
    if not noheap:
        stub += bytes(hb)
    # exit: ThreadKill(KCurrentThreadHandle, EExitKill, 0, NULL).
    # Frame matches genuine SLOW_EXEC4 after a CALL ([esp]=ret): push
    # arg4..arg1 then a filler ret slot, then ebx/esi, so [esp+12]=arg1.
    # (Two previous layouts shifted registers: handle landed in ECX only
    # with this exact order.)
    stub += bytes([
        0x6A, 0x00,                    # push NULL category (arg4)
        0x6A, 0x00,                    # push reason 0 (arg3)
        0x6A, 0x00,                    # push EExitKill=0 (arg2)
        0x68, 0x01, 0x80, 0xFF, 0xFF,  # push KCurrentThreadHandle (arg1)
        0x6A, 0x00,                    # push filler (ret slot)
        0xB8, 0x73, 0x00, 0x00, 0x00,  # mov eax,115 (EExecThreadKill)
        0x53,                          # push ebx
        0x56,                          # push esi
        0x8B, 0x4C, 0x24, 0x0C,        # mov ecx,[esp+12] (arg1)
        0x8B, 0x54, 0x24, 0x10,        # mov edx,[esp+16] (arg2)
        0x8B, 0x5C, 0x24, 0x14,        # mov ebx,[esp+20] (arg3)
        0x8B, 0x74, 0x24, 0x18,        # mov esi,[esp+24] (arg4)
        0xCD, 0x21,                    # int 0x21 (doesn't return)
    ])
    if spin:
        # Spinner variant: draw once, then loop forever (for `kill` tests).
        # Run it at Low priority (--prio=150) so the shell stays usable.
        stub = bytearray(stub)
        stub[-2:] = bytes([0xEB, 0xFE])  # jmp $ replaces int 0x21
        stub = bytes(stub)
    assert len(stub) <= code_size, f"stub {len(stub)} > {code_size}"
    code[0:len(stub)] = stub
    # Reloc for the absolute data DWORD: 5+5+2+5+3=20 bytes before 0xA1,
    # so A1 at 20 and the imm32 (fixup site) at 21. The BSS imm32 sits
    # 25 bytes later at 46 (5+5+2+5+3 after the first site).
    SIG_OFF = 21
    BSS_OFF = 46
    assert code[SIG_OFF - 1] == 0xA1
    assert code[BSS_OFF - 1] == 0xA1
    # Code reloc section: header + one block (page 0) + two DATA entries.
    reloc = struct.pack("<2i", 8 + 8 + 4, 2)
    reloc += struct.pack("<2I", 0, 8 + 4)
    reloc += struct.pack("<H", 0x2000 | SIG_OFF)
    reloc += struct.pack("<H", 0x2000 | BSS_OFF)
    data_off = header_size + code_size
    reloc_off = data_off + len(data)
    flags = KImageHdrFmt_V | KImageImpFmt_ELF  # x86: no Ept bits, no ABI bits
    uid1 = KExecutableImageUidValue
    uid_check = uid_checksum(uid1, uid2, uid3)
    module_version = 0x00010000  # 1.0
    compression = KFormatNotCompressed
    tools_version = struct.pack("BBh", 1, 0, 0)
    time_lo = 0
    time_hi = 0
    code_base = 0x00400000  # user side (<2GB), Direct model
    header = bytearray()
    header += struct.pack("<4I", uid1, uid2 & 0xFFFFFFFF, uid3 & 0xFFFFFFFF, uid_check)
    header += struct.pack("<I", EPOC_SIG)
    header += struct.pack("<I", KImageCrcInitialiser)  # placeholder, fixed below
    header += struct.pack("<I", module_version)
    header += struct.pack("<I", compression)
    header += tools_version
    header += struct.pack("<2I", time_lo, time_hi)
    header += struct.pack("<I", flags)
    ds = 0 if nodata else len(data)
    bs = 0 if nodata else 4
    header += struct.pack("<6i", code_size, ds, 0x1000, 0x100000, 0x2000, bs)
    header += struct.pack("<I", 0)  # entry point offset 0
    header += struct.pack("<2I", code_base, data_base)
    header += struct.pack("<i", 0)  # dll refs
    header += struct.pack("<I", 0)  # export dir offset
    header += struct.pack("<i", 0)  # export dir count
    header += struct.pack("<i", code_size)  # text size == code size (no IAT)
    header += struct.pack("<5I", header_size, data_off, 0, reloc_off, 0)
    header += struct.pack("<2H", prio & 0xFFFF, ECpuX86)  # priority, cpu
    header += struct.pack("<I", 0)  # uncompressed size (0: not compressed)
    header += struct.pack("<3I", secure_id & 0xFFFFFFFF, 0, 0)  # secure/vendor/caps[0]
    header += struct.pack("<I", 0)  # caps[1]
    header += struct.pack("<2I", 0, 0)  # exception descriptor, spare2
    header += struct.pack("<H", 0)  # export desc size (NoHoles)
    header += struct.pack("<B", 0x00)  # NoHoles
    header += struct.pack("<B", 0x00)  # exportDesc[0] pad
    assert len(header) == header_size, f"header {len(header)} != {header_size}"
    crc = crc32_symbian(bytes(header), 0)
    struct.pack_into("<I", header, 20, crc)
    return bytes(header) + bytes(code) + bytes(data) + bytes(reloc)


def build_imp_e32(uid2: int, uid3: int, secure_id: int,
                  prio: int = 350) -> bytes:
    """Lean import-demo EXE (--imports): draws IM, PutC('P') and Exit() via
    EUSER IAT slots (ordinals 1=Exit slot@64, 2=PutC slot@68). No data/BSS/
    heap — the loaded code proves import resolution end to end."""
    header_size = 156
    code_size = 256
    code_base = 0x00400000
    data_base = 0x00500000
    IAT0, IAT1 = 64, 68  # ord 1 (Exit), ord 2 (PutC); iTextSize=64
    code = bytearray(code_size)
    stub = bytearray()
    stub += bytes([0xBF, 0xC0, 0x8D, 0x0B, 0x00])  # mov edi,0xB8DC0 (row 22)
    stub += bytes([0xB8, 0x49, 0x0E, 0x4D, 0x0E])  # mov eax,"IM"
    stub += bytes([0x89, 0x07])                    # mov [edi],eax
    stub += bytes([0x6A, 0x50])                    # push 'P'
    putc_at = len(stub) + 1
    stub += bytes([0xB8]) + struct.pack("<I", code_base + IAT1)  # mov eax,slot
    stub += bytes([0xFF, 0x10])                    # call [eax] (PutC)
    stub += bytes([0x83, 0xC4, 0x04])              # add esp,4
    exit_at = len(stub) + 1
    stub += bytes([0xB8]) + struct.pack("<I", code_base + IAT0)  # mov eax,slot
    stub += bytes([0xFF, 0x10])                    # call [eax] (Exit; no return)
    stub += bytes([0x0F, 0x0B])                    # ud2 (backstop)
    assert len(stub) < IAT0, f"stub {len(stub)} overlaps IAT"
    code[0:len(stub)] = stub
    # TEXT relocs for the two slot-address immediates (loader adds codeDelta).
    reloc = struct.pack("<2i", 8 + 8 + 4, 2)
    reloc += struct.pack("<2I", 0, 8 + 4)
    reloc += struct.pack("<H", 0x1000 | putc_at)
    reloc += struct.pack("<H", 0x1000 | exit_at)
    # Import section: header + 1 ELF block (code offsets, ordinal=index+1).
    imp = struct.pack("<i", 8 + 8 + 6)  # iSize excl header
    imp += struct.pack("<I", 20)        # name offset from section start
    imp += struct.pack("<i", 2)         # two imports
    imp += struct.pack("<2I", IAT0, IAT1)
    imp += b"EUSER\x00"
    reloc_off = header_size + code_size
    imp_off = reloc_off + len(reloc)
    flags = KImageHdrFmt_V | KImageImpFmt_ELF
    uid1 = KExecutableImageUidValue
    uid_check = uid_checksum(uid1, uid2, uid3)
    header = bytearray()
    header += struct.pack("<4I", uid1, uid2 & 0xFFFFFFFF, uid3 & 0xFFFFFFFF, uid_check)
    header += struct.pack("<I", EPOC_SIG)
    header += struct.pack("<I", KImageCrcInitialiser)
    header += struct.pack("<I", 0x00010000)
    header += struct.pack("<I", KFormatNotCompressed)
    header += struct.pack("BBh", 1, 0, 0)
    header += struct.pack("<2I", 0, 0)
    header += struct.pack("<I", flags)
    header += struct.pack("<6i", code_size, 0, 0x1000, 0x100000, 0x2000, 0)
    header += struct.pack("<I", 0)
    header += struct.pack("<2I", code_base, data_base)
    header += struct.pack("<i", 1)  # dll refs (EUSER)
    header += struct.pack("<I", 0)
    header += struct.pack("<i", 0)
    header += struct.pack("<i", IAT0)  # text size (IAT lives past .text)
    header += struct.pack("<5I", header_size, 0, imp_off, reloc_off, 0)
    header += struct.pack("<2H", prio & 0xFFFF, ECpuX86)
    header += struct.pack("<I", 0)
    header += struct.pack("<3I", secure_id & 0xFFFFFFFF, 0, 0)
    header += struct.pack("<I", 0)
    header += struct.pack("<2I", 0, 0)
    header += struct.pack("<H", 0)
    header += struct.pack("<B", 0x00)
    header += struct.pack("<B", 0x00)
    assert len(header) == header_size, f"header {len(header)} != {header_size}"
    crc = crc32_symbian(bytes(header), 0)
    struct.pack_into("<I", header, 20, crc)
    return bytes(header) + bytes(code) + bytes(reloc) + bytes(imp)


def build_args_e32(uid2: int, uid3: int, secure_id: int,
                   prio: int = 350) -> bytes:
    """Args-demo EXE (--args): imports Exit + PutC + CmdLen (ords 1-3).

    Calls CmdLen(KCurrentProcessHandle), stores the returned length into
    its 4-byte BSS slot, then exits. The QEMU test reads the slot back,
    proving user-side access to the genuine process command line.
    """
    header_size = 156
    code_size = 256
    code_base = 0x00400000
    data_base = 0x00500000
    IAT0, IAT1, IAT2 = 64, 68, 72  # ord 1 (Exit), 2 (PutC), 3 (CmdLen)
    code = bytearray(code_size)
    stub = bytearray()
    stub += b"\x68\x00\x80\xFF\xFF"              # push KCurrentProcessHandle
    cmdlen_at = len(stub) + 1
    stub += b"\xB8" + struct.pack("<I", code_base + IAT2)  # mov eax,slot
    stub += b"\xFF\x10"                          # call [eax] (CmdLen -> EAX)
    stub += b"\x83\xC4\x04"                      # add esp,4
    bss_site = len(stub) + 1
    stub += b"\xA3" + struct.pack("<I", data_base + 16)  # mov [bss],eax
    exit_at = len(stub) + 1
    stub += b"\xB8" + struct.pack("<I", code_base + IAT0)  # mov eax,slot
    stub += b"\xFF\x10"                          # call [eax] (Exit; no return)
    stub += b"\x0F\x0B"                          # ud2 (backstop)
    assert len(stub) < IAT0, f"stub {len(stub)} overlaps IAT"
    code[0:len(stub)] = stub
    # TEXT relocs for the two slot-address immediates, DATA for the BSS store.
    reloc = struct.pack("<2i", 8 + 16, 3)
    reloc += struct.pack("<2I", 0, 8 + 8)
    reloc += struct.pack("<H", 0x1000 | cmdlen_at)
    reloc += struct.pack("<H", 0x1000 | exit_at)
    reloc += struct.pack("<H", 0x2000 | bss_site)
    reloc += struct.pack("<H", 0)
    data = bytes(16)
    data_off = header_size + code_size
    reloc_off = data_off + len(data)
    # Import section: header + 1 ELF block (code offsets, ordinal=index+1).
    imp = struct.pack("<i", 8 + 12 + 6)  # iSize excl header
    imp += struct.pack("<I", 24)         # name offset from section start
    imp += struct.pack("<i", 3)          # three imports
    imp += struct.pack("<3I", IAT0, IAT1, IAT2)
    imp += b"EUSER\x00"
    imp_off = reloc_off + len(reloc)
    flags = KImageHdrFmt_V | KImageImpFmt_ELF
    uid_check = uid_checksum(KExecutableImageUidValue, uid2, uid3)
    header = bytearray()
    header += struct.pack("<4I", KExecutableImageUidValue,
                          uid2 & 0xFFFFFFFF, uid3 & 0xFFFFFFFF, uid_check)
    header += struct.pack("<I", EPOC_SIG)
    header += struct.pack("<I", KImageCrcInitialiser)
    header += struct.pack("<I", 0x00010000)
    header += struct.pack("<I", KFormatNotCompressed)
    header += struct.pack("BBh", 1, 0, 0)
    header += struct.pack("<2I", 0, 0)
    header += struct.pack("<I", flags)
    header += struct.pack("<6i", code_size, len(data), 0x1000, 0x100000,
                          0x2000, 4)
    header += struct.pack("<I", 0)
    header += struct.pack("<2I", code_base, data_base)
    header += struct.pack("<i", 1)  # dll refs (EUSER)
    header += struct.pack("<I", 0)
    header += struct.pack("<i", 0)
    header += struct.pack("<i", IAT0)  # text size (IAT lives past .text)
    header += struct.pack("<5I", header_size, data_off, imp_off, reloc_off, 0)
    header += struct.pack("<2H", prio & 0xFFFF, ECpuX86)
    header += struct.pack("<I", 0)
    header += struct.pack("<3I", secure_id & 0xFFFFFFFF, 0, 0)
    header += struct.pack("<I", 0)
    header += struct.pack("<2I", 0, 0)
    header += struct.pack("<H", 0)
    header += struct.pack("<B", 0x00)
    header += struct.pack("<B", 0x00)
    assert len(header) == header_size, f"header {len(header)} != {header_size}"
    crc = crc32_symbian(bytes(header), 0)
    struct.pack_into("<I", header, 20, crc)
    return bytes(header) + bytes(code) + bytes(data) + bytes(reloc) + bytes(imp)


def build_datarel_e32(uid2: int, uid3: int, secure_id: int,
                      prio: int = 350) -> bytes:
    """EXE with a data-to-data pointer needing a DATA relocation.

    The code dereferences the relocated pointer and records a success word
    in BSS if it points to the expected word. This exercises data-run copy.
    """
    blob = bytearray(build_e32(uid2, uid3, secure_id, prio=prio, noheap=True))
    code_off = 156
    code_size = struct.unpack_from("<I", blob, 48)[0]
    data_off = struct.unpack_from("<I", blob, 104)[0]
    # data[8] = linked pointer to data[0]; data[0] is DEADBEEF already.
    struct.pack_into("<I", blob, data_off + 8, 0x00500000)
    # Standalone entry: load relocated address of data[8], dereference it,
    # compare target, store result into BSS, then exit through the slow exec.
    code = bytearray()
    code += b"\xA1" + struct.pack("<I", 0x00500008)   # mov eax,[data+8]
    code += b"\x81\x38" + struct.pack("<I", 0xDEADBEEF)  # cmp dword [eax],sig
    code += b"\xB8\x00\x00\x00\x00"                 # mov eax,0 (failure)
    code += b"\x75\x05"                                # jne +5
    code += b"\xB8\x52\x0A\x20\x0A"                 # mov eax,'R ' green
    # Store the result into the existing 4-byte BSS slot. The test queries
    # process data through the monitor, avoiding VGA scroll races.
    code += b"\xA3" + struct.pack("<I", 0x00500010)
    # Exit cleanly through the genuine slow exec (the BSS slot remains in
    # this direct-model test until the next process reuses that RAM).
    code += bytes([
        0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00,
        0x68, 0x01, 0x80, 0xFF, 0xFF, 0x6A, 0x00,
        0xB8, 0x73, 0x00, 0x00, 0x00, 0x53, 0x56,
        0x8B, 0x4C, 0x24, 0x0C, 0x8B, 0x54, 0x24, 0x10,
        0x8B, 0x5C, 0x24, 0x14, 0x8B, 0x74, 0x24, 0x18,
        0xCD, 0x21,
    ])
    assert len(code) < code_size
    blob[code_off:code_off + code_size] = code.ljust(code_size, b"\x00")
    # One TEXT relocation at code+1, matching the new absolute load.
    code_reloc_off = struct.unpack_from("<I", blob, 112)[0]
    bss_site = code.index(b"\xA3") + 1
    code_reloc = struct.pack("<2i2I2H", 20, 2, 0, 12, 0x2001, 0x2000 | bss_site)
    blob[code_reloc_off:] = code_reloc
    # One DATA relocation at data+8, padding word keeps the block 4-aligned.
    data_reloc_off = len(blob)
    blob += struct.pack("<2i2I2H", 20, 1, 0, 12, 0x2008, 0)
    struct.pack_into("<I", blob, 116, data_reloc_off)
    struct.pack_into("<I", blob, 20, KImageCrcInitialiser)
    struct.pack_into("<I", blob, 20, crc32_symbian(bytes(blob[:code_off])))
    return bytes(blob)


def build_euser_dll(uid2: int = 0x10000079, uid3: int = 0x10000079,
                    secure_id: int = 0x10000079) -> bytes:
    """Minimal EUSER.DLL: 3 exports (ord 1=Exit, ord 2=PutC, ord 3=CmdLen).

    Export directory holds code-section offsets; the kernel resolves
    ordinal i to dllCodeRun + entry[i-1]. Code is position-independent,
    so no relocations needed. CmdLen is the genuine
    EExecProcessCommandLineLength slow exec (82) in cdecl form.
    """
    header_size = 156
    code_size = 256
    code_base = 0x00600000
    data_base = 0x00700000
    EXIT_OFF, PUTC_OFF, CMDLEN_OFF = 32, 64, 96
    EXP_OFF = 128  # file offset of export dir within the code part
    code = bytearray(code_size)
    code[0] = 0xC3  # DLL entry: ret (loader does not call it yet)
    exit_stub = bytes([
        0xB9, 0x01, 0x80, 0xFF, 0xFF,  # mov ecx,KCurrentThreadHandle
        0x31, 0xD2, 0x31, 0xDB, 0x31, 0xF6,
        0xB8, 0x73, 0x00, 0x00, 0x00,  # mov eax,115 (EExecThreadKill)
        0xCD, 0x21,
    ])
    putc_stub = bytes([
        0x0F, 0xB6, 0x4C, 0x24, 0x04,  # movzx ecx,[esp+4]
        0xB5, 0x0E,                    # mov ch,0x0E
        0x66, 0x89, 0x0D, 0x20, 0x8D, 0x0B, 0x00,  # mov [0xB8D20],cx
        0xC3,
    ])
    cmdlen_stub = bytes([
        0xB8, 0x52, 0x00, 0x00, 0x00,  # mov eax,82 (EExecProcessCommandLineLength)
        0x8B, 0x4C, 0x24, 0x04,        # mov ecx,[esp+4] (process handle)
        0xCD, 0x21,                    # int 0x21
        0xC3,                          # ret
    ])
    code[EXIT_OFF:EXIT_OFF + len(exit_stub)] = exit_stub
    code[PUTC_OFF:PUTC_OFF + len(putc_stub)] = putc_stub
    code[CMDLEN_OFF:CMDLEN_OFF + len(cmdlen_stub)] = cmdlen_stub
    struct.pack_into("<3I", code, EXP_OFF, EXIT_OFF, PUTC_OFF, CMDLEN_OFF)
    text_size = EXP_OFF + 12
    flags = KImageDll | KImageHdrFmt_V | KImageImpFmt_ELF
    uid_check = uid_checksum(KDynamicLibraryUidValue, uid2, uid3)
    header = bytearray()
    header += struct.pack("<4I", KDynamicLibraryUidValue,
                          uid2 & 0xFFFFFFFF, uid3 & 0xFFFFFFFF, uid_check)
    header += struct.pack("<I", EPOC_SIG)
    header += struct.pack("<I", KImageCrcInitialiser)
    header += struct.pack("<I", 0x00010000)
    header += struct.pack("<I", KFormatNotCompressed)
    header += struct.pack("BBh", 1, 0, 0)
    header += struct.pack("<2I", 0, 0)
    header += struct.pack("<I", flags)
    header += struct.pack("<6i", code_size, 0, 0, 0, 0x2000, 0)
    header += struct.pack("<I", 0)  # entry point offset 0 (ret)
    header += struct.pack("<2I", code_base, data_base)
    header += struct.pack("<i", 0)  # no DLL refs of its own
    header += struct.pack("<I", header_size + EXP_OFF)  # export dir file offset
    header += struct.pack("<i", 3)  # three exports
    header += struct.pack("<i", text_size)
    header += struct.pack("<5I", header_size, 0, 0, 0, 0)
    header += struct.pack("<2H", 0, ECpuX86)  # DLLs carry no process priority
    header += struct.pack("<I", 0)
    header += struct.pack("<3I", secure_id & 0xFFFFFFFF, 0, 0)
    header += struct.pack("<I", 0)
    header += struct.pack("<2I", 0, 0)
    header += struct.pack("<H", 0)
    header += struct.pack("<B", 0x00)
    header += struct.pack("<B", 0x00)
    assert len(header) == header_size, f"header {len(header)} != {header_size}"
    crc = crc32_symbian(bytes(header), 0)
    struct.pack_into("<I", header, 20, crc)
    return bytes(header) + bytes(code)


def main() -> int:
    out = "build/hello.e32"
    uid2 = 0x10000001
    uid3 = 0x10000002
    secure_id = 0x10000002
    spin = False
    prio = 350
    noheap = False
    nodata = False
    use_imports = False
    use_data_reloc = False
    use_dll = False
    use_args = False
    args = sys.argv[1:]
    if args and not args[0].startswith("--"):
        out = args.pop(0)
    for a in args:
        if a.startswith("--uid2="):
            uid2 = int(a[7:], 0)
        elif a.startswith("--uid3="):
            uid3 = int(a[7:], 0)
        elif a.startswith("--secure="):
            secure_id = int(a[9:], 0)
        elif a == "--spin":
            spin = True
        elif a == "--noheap":
            noheap = True
        elif a == "--nodata":
            nodata = True
        elif a == "--imports":
            use_imports = True
        elif a == "--data-reloc":
            use_data_reloc = True
        elif a == "--dll":
            use_dll = True
        elif a == "--args":
            use_args = True
        elif a.startswith("--prio="):
            prio = int(a[7:], 0)
        else:
            print(f"unknown arg {a}", file=sys.stderr)
            return 2
    if sum((use_imports, use_data_reloc, use_dll, use_args)) > 1:
        print("--imports, --data-reloc, --dll and --args are separate tests", file=sys.stderr)
        return 2
    if use_dll:
        blob = build_euser_dll(uid2, uid3, secure_id)
    elif use_args:
        blob = build_args_e32(uid2, uid3, secure_id, prio)
    elif use_data_reloc:
        blob = build_datarel_e32(uid2, uid3, secure_id, prio)
    elif use_imports:
        blob = build_imp_e32(uid2, uid3, secure_id, prio)
    else:
        blob = build_e32(uid2, uid3, secure_id, spin, prio, noheap, nodata)
    with open(out, "wb") as f:
        f.write(blob)
    print(f"wrote {out} ({len(blob)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
