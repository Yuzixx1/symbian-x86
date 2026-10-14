#!/usr/bin/env python3
"""mkfat.py — minimal FAT16 image writer for the UEFI boot image.

Builds a small FAT16 volume containing:
  EFI/BOOT/BOOTX64.EFI   the UEFI loader
  EKA2/EKERN.ELF         the kernel ELF it loads

Usage: mkfat.py <loader.efi> <kernel.elf> <out.img> <size-mb> [--bare]
Default: partitioned disk image (MBR + FAT at LBA 2048), bootable by UEFI
as a hard disk. --bare: just the FAT volume at LBA 0 (for ISO embedding,
where mkiso.py adds the ESP partition entry).
Only depends on the two inputs; deterministic layout for stable ISOs.
"""
import os
import struct
import sys


def build_fat16(files, size_mb, out_path, partitioned=True):
    """files: list of (path_tuple, bytes), e.g. ('EFI','BOOT','BOOTX64.EFI')."""
    SECT = 512
    total = size_mb * 1024 * 1024 // SECT
    base = 2048 if partitioned else 0       # FAT volume start (sectors)
    if partitioned and total <= base + 4096:
        sys.exit("disk too small")
    spc = 1
    rsvd = 4
    fats = 2
    root_ents = 512
    root_secs = root_ents * 32 // SECT
    vsects = total - base                    # volume size in sectors
    clusters = (vsects - rsvd - root_secs) // spc
    fat = 0
    for _ in range(3):
        fat = (clusters * 2 + SECT - 1) // SECT
        clusters = (vsects - rsvd - root_secs - fats * fat) // spc
    fat = (clusters * 2 + SECT - 1) // SECT
    data_sec = rsvd + fats * fat + root_secs  # volume-relative
    img = bytearray(total * SECT)
    VO = base * SECT                          # volume byte offset in image

    def wr(off, data):
        img[off:off + len(data)] = data

    def vwr(sec, data):
        """Write at volume-relative sector."""
        wr(VO + sec * SECT, data)

    if partitioned:
        mbr = bytearray(SECT)
        mbr[446] = 0x00
        mbr[447], mbr[448], mbr[449] = 0x20, 0x21, 0x00
        mbr[450] = 0xEF  # EFI System Partition
        mbr[451], mbr[452], mbr[453] = 0xFE, 0xFF, 0xFF
        struct.pack_into("<I", mbr, 454, base)
        struct.pack_into("<I", mbr, 458, vsects)
        mbr[510:512] = b"\x55\xaa"
        wr(0, bytes(mbr))

    # Boot sector (valid BPB for firmware parsing; volume-relative fields).
    bs = bytearray(SECT)
    bs[0:3] = b"\xeb\x3c\x90"
    bs[3:11] = b"SYBUEFI "
    struct.pack_into("<H", bs, 11, SECT)
    bs[13] = spc
    struct.pack_into("<H", bs, 14, rsvd)
    bs[16] = fats
    struct.pack_into("<H", bs, 17, root_ents)
    struct.pack_into("<H", bs, 19, vsects if vsects < 65536 else 0)
    bs[21] = 0xF8
    struct.pack_into("<H", bs, 22, fat)
    struct.pack_into("<H", bs, 24, 32)
    struct.pack_into("<H", bs, 26, 64)
    struct.pack_into("<I", bs, 32, vsects)
    bs[36] = 0x80
    bs[38] = 0x29
    struct.pack_into("<I", bs, 39, 0x53454259)
    bs[43:54] = b"SYBUEFI    "
    bs[54:62] = b"FAT16   "
    bs[510:512] = b"\x55\xaa"
    vwr(0, bytes(bs))

    # FATs: media + EOC marks; allocator below chains clusters.
    for f in range(fats):
        fsec = rsvd + f * fat
        vwr(fsec, struct.pack("<HH", 0xFFF8, 0xFFFF))

    def fat_get(c):
        return struct.unpack_from("<H", img, VO + (rsvd + 0 * fat) * SECT + c * 2)[0]

    def fat_put(c, nxt):
        for f in range(fats):
            struct.pack_into("<H", img, VO + (rsvd + f * fat) * SECT + c * 2, nxt)

    next_free = 2

    def alloc(nbytes):
        nonlocal next_free
        need = (nbytes + spc * SECT - 1) // (spc * SECT)
        if need < 1:
            need = 1
        start = next_free
        for i in range(need):
            c = start + i
            fat_put(c, c + 1 if i + 1 < need else 0xFFFF)
        next_free = start + need
        return start

    def clba(c):
        return data_sec + (c - 2) * spc  # volume-relative sector

    def write_chain(start, data):
        c, off = start, 0
        while True:
            for s in range(spc):
                chunk = data[off:off + SECT]
                vwr(clba(c) + s, chunk + bytes(SECT - len(chunk)))
                off += SECT
                if off >= len(data):
                    return
            nxt = fat_get(c)
            if nxt >= 0xFFF8:
                return
            c = nxt

    def split83(name):
        if "." in name:
            b, e = name.rsplit(".", 1)
        else:
            b, e = name, ""
        return (b.upper() + " " * 8)[:8] + (e.upper() + " " * 3)[:3]

    def dir_entry(name83, attr, first, size):
        e = bytearray(32)
        raw = name83.encode("ascii")[:11]
        e[0:len(raw)] = raw
        for i in range(len(raw), 11):
            e[i] = 0x20
        e[11] = attr
        struct.pack_into("<H", e, 20, (first >> 16) & 0xFFFF)
        struct.pack_into("<H", e, 26, first & 0xFFFF)
        struct.pack_into("<I", e, 28, size)
        return bytes(e)

    # Directory tree (arbitrary depth): nested dicts, files as (name, data).
    tree = {}
    for path, data in files:
        node = tree
        for comp in path[:-1]:
            node = node.setdefault(comp, {})
        node[path[-1]] = data

    def read_clus(c):
        o = VO + clba(c) * SECT
        return bytearray(img[o:o + spc * SECT])

    def write_clus(c, data):
        o = VO + clba(c) * SECT
        wr(o, bytes(data[:spc * SECT]))

    def write_dir(node):
        """Allocate+write a directory, return its cluster. Files first."""
        dclus = alloc(spc * SECT)
        sub = bytearray(spc * SECT)
        sub[0:32] = dir_entry(".          ", 0x10, dclus, 0)
        sub[32:64] = dir_entry("..         ", 0x10, 0, 0)
        write_chain(dclus, bytes(sub))
        for name in sorted(node.keys()):
            val = node[name]
            if isinstance(val, dict):
                continue
            fclus = alloc(len(val))
            write_chain(fclus, val)
            cur = read_clus(dclus)
            for i in range(0, spc * SECT, 32):
                if cur[i] in (0x00, 0xE5):
                    cur[i:i + 32] = dir_entry(split83(name), 0x20,
                                              fclus, len(val))
                    break
            write_clus(dclus, bytes(cur))
        return dclus

    def fill_kids(node, dclus):
        cur = read_clus(dclus)
        for name in sorted(node.keys()):
            val = node[name]
            if not isinstance(val, dict):
                continue
            kclus = write_dir(val)
            kid = read_clus(kclus)
            kid[32:64] = dir_entry("..         ", 0x10, dclus, 0)
            write_clus(kclus, bytes(kid))
            for i in range(0, spc * SECT, 32):
                if cur[i] in (0x00, 0xE5):
                    cur[i:i + 32] = dir_entry(split83(name), 0x10,
                                              kclus, 0)
                    break
        write_clus(dclus, bytes(cur))

    root_off = VO + (rsvd + fats * fat) * SECT
    ent = 0

    def root_put(entry):
        nonlocal ent
        wr(root_off + ent * 32, entry)
        ent += 1

    for top in sorted(tree.keys()):
        val = tree[top]
        if isinstance(val, dict):
            dclus = write_dir(val)
            fill_kids(val, dclus)
            root_put(dir_entry(split83(top), 0x10, dclus, 0))
        else:
            fclus = alloc(len(val))
            write_chain(fclus, val)
            root_put(dir_entry(split83(top), 0x20, fclus, len(val)))

    with open(out_path, "wb") as f:
        f.write(img)
    print(f"FAT16: {out_path} ({size_mb}MB, {len(files)} files)")


if __name__ == "__main__":
    bare = "--bare" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--bare"]
    if len(args) != 4:
        sys.exit("usage: mkfat.py <loader.efi> <kernel.elf> <out.img> <size-mb> [--bare]")
    loader, kernel, out, mb = args[0], args[1], args[2], int(args[3])
    with open(loader, "rb") as f:
        ldr = f.read()
    with open(kernel, "rb") as f:
        kern = f.read()
    build_fat16([(("EFI", "BOOT", "BOOTX64.EFI"), ldr),
                 (("EKA2", "EKERN.ELF"), kern)], mb, out,
                partitioned=not bare)
