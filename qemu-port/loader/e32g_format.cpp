#include "e32g_format.h"

namespace E32G {
namespace {
bool Fits(uint32_t off, uint32_t size, uint32_t limit) {
    return off <= limit && size <= limit - off;
}
uint16_t R16(const uint8_t* p) { return uint16_t(p[0] | uint16_t(p[1]) << 8); }
uint32_t R32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
           uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
Status Fail(Error& e, Status s, Issue i, uint32_t o) {
    e.status = s; e.issue = i; e.offset = o; return s;
}
void ClearError(Error& e) { e.status = Ok; e.issue = None; e.offset = 0; }
uint32_t CrcByte(uint32_t crc, uint8_t byte) {
    crc ^= byte;
    for (unsigned i = 0; i != 8; ++i)
        crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
    return crc;
}
uint16_t Crc16Byte(uint16_t crc, uint8_t byte) {
    crc ^= uint16_t(byte) << 8;
    for (unsigned i = 0; i != 8; ++i)
        crc = uint16_t((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
    return crc;
}
bool Hex8(const uint8_t* p, uint32_t& value) {
    value = 0;
    for (unsigned i = 0; i != 8; ++i) {
        uint8_t c = p[i]; unsigned n;
        if (c >= '0' && c <= '9') n = c - '0';
        else if (c >= 'a' && c <= 'f') n = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') n = c - 'A' + 10;
        else return false;
        value = (value << 4) | n;
    }
    return true;
}
Status ValidateRelocs(Bytes f, uint32_t off, uint32_t sectionSize,
                      uint32_t restStart, RelocView& out, Error& e) {
    out = RelocView();
    if (!off) return Ok;
    if (off < restStart || !Fits(off, 8, f.size))
        return Fail(e, Corrupt, Relocations, off);
    const uint32_t size = R32(f.data + off), count = R32(f.data + off + 4);
    if ((size & 3) || count >= 0x80000000u || !Fits(off + 8, size, f.size))
        return Fail(e, Corrupt, Relocations, off);
    const uint32_t end = off + 8 + size;
    uint32_t p = off + 8, found = 0;
    while (p != end) {
        if (!Fits(p, 8, end)) return Fail(e, Corrupt, Relocations, p);
        const uint32_t page = R32(f.data + p), bs = R32(f.data + p + 4);
        if ((page & 4095) || bs < 8 || (bs & 3) || !Fits(p, bs, end))
            return Fail(e, Corrupt, Relocations, p);
        for (uint32_t q = p + 8; q != p + bs; q += 2) {
            const uint16_t v = R16(f.data + q);
            if (!v) continue;
            const unsigned type = v & 0xf000;
            // page+low12 cannot overflow: page is 4KiB aligned.
            const uint32_t dest = page + (v & 4095);
            if ((type != 0x1000 && type != 0x2000 && type != 0x3000) ||
                !Fits(dest, 4, sectionSize) || found == count)
                return Fail(e, Corrupt, Relocations, q);
            ++found;
        }
        p += bs;
    }
    if (found != count) return Fail(e, Corrupt, Relocations, off + 4);
    out.section.offset = off; out.section.size = 8 + size; out.count = count;
    return Ok;
}
// Validated sparse bitmap: absent meta bits imply all exports present.
bool Present(const Image& im, uint32_t index) {
    const uint8_t* d = im.file.data + im.exportDescription.offset;
    if (im.exportDescriptionType == 0) return true;
    if (im.exportDescriptionType == 1) return (d[index / 8] & (1u << (index & 7))) != 0;
    const uint32_t bitmapSize = (im.exportCount + 7) / 8;
    const uint32_t metaSize = (bitmapSize + 7) / 8;
    const uint32_t byte = index / 8;
    if (!(d[byte / 8] & (1u << (byte & 7)))) return true;
    uint32_t pos = metaSize;
    for (uint32_t i = 0; i < byte; ++i)
        if (d[i / 8] & (1u << (i & 7))) ++pos;
    return (d[pos] & (1u << (index & 7))) != 0;
}
Status Description(Image& im, Error& e) {
    const uint32_t n = im.exportDescription.size;
    if (152u + ((n + 3u + 3u) & ~3u) != im.header.size)
        return Fail(e, Corrupt, ExportDescription, 152);
    const uint32_t bitmap = (im.exportCount + 7) / 8;
    const uint8_t* d = im.file.data + im.exportDescription.offset;
    if (im.exportDescriptionType == 0) {
        // Upstream NoHoles ignores descriptor payload; keep that behavior.
    } else if (im.exportDescriptionType == 1) {
        if (n != bitmap) return Fail(e, Corrupt, ExportDescription, 152);
    } else if (im.exportDescriptionType == 2) {
        const uint32_t meta = (bitmap + 7) / 8;
        if (meta > n) return Fail(e, Corrupt, ExportDescription, 152);
        uint32_t expected = meta;
        for (uint32_t i = 0; i != meta; ++i)
            for (unsigned bit = 0; bit != 8; ++bit)
                if (d[i] & (1u << bit)) ++expected;
        if (expected != n) return Fail(e, Corrupt, ExportDescription, 152);
    } else return Fail(e, Unsupported, ExportDescription, 154);
    uint32_t payload = (bitmap + 7) / 8;
    for (uint32_t byte = 0; byte < bitmap; ++byte) {
        uint8_t bits = 255;
        if (im.exportDescriptionType == 1) bits = d[byte];
        else if (im.exportDescriptionType == 2 && (d[byte/8] & (1u << (byte&7))))
            bits = d[payload++];
        for (unsigned bit = 0; bit < 8 && byte*8+bit < im.exportCount; ++bit) {
            const uint32_t i = byte*8+bit;
            const uint32_t raw = R32(im.file.data + im.exports.offset + i * 4);
            // Zero alone is not a hole marker (genuine EXEs export zero).
            if (((bits & (1u << bit)) != 0) == (raw == im.entryCodeOffset))
                return Fail(e, Corrupt, ExportDescription, im.exports.offset + i * 4);
        }
    }
    return Ok;
}
Status Import(const Image& im, uint32_t index, uint32_t& p, uint32_t& total,
              ImportView& out, Error& e) {
    const uint32_t end = im.imports.offset + im.imports.size;
    if (!Fits(p, 8, end)) return Fail(e, Corrupt, Imports, p);
    const uint32_t name = R32(im.file.data + p), count = R32(im.file.data + p + 4);
    if (name >= im.imports.size) return Fail(e, Corrupt, Name, p);
    uint32_t n = 0, nameOff = im.imports.offset + name;
    while (n < end - nameOff && im.file.data[nameOff + n]) ++n;
    if (n == end - nameOff) return Fail(e, Corrupt, Name, nameOff);
    if (n > 80) return Fail(e, Unsupported, Name, nameOff);
    Identity id = Identity(); Error ne = Error();
    Bytes nameBytes = {im.file.data + nameOff, n};
    Status s = ParseIdentity(nameBytes, id, ne);
    if (s != Ok) return Fail(e, s, Name, nameOff + ne.offset);
    if (count >= 0x20000000u || total > 0x1fffffffu - count ||
        total + count > (im.code.size - im.textSize) / 4)
        return Fail(e, Corrupt, Imports, p + 4);
    out.name.offset = nameOff; out.name.size = n;
    out.count = count; out.identity = id;
    out.iat.offset = im.code.offset + im.textSize + total * 4;
    out.iat.size = count * 4;
    total += count; p += 8;
    (void)index;
    return Ok;
}
} // anonymous namespace

uint32_t Crc32(Bytes b) {
    uint32_t c = 0;
    if (!b.data) return 0;
    for (uint32_t i = 0; i < b.size; ++i) c = CrcByte(c, b.data[i]);
    return c;
}
uint32_t HeaderCrc(Bytes b) {
    uint32_t c = 0;
    if (!b.data) return 0;
    for (uint32_t i = 0; i < b.size; ++i)
        c = CrcByte(c, i >= 20 && i < 24 ? uint8_t(0xc90fdaa2u >> ((i-20)*8)) : b.data[i]);
    return c;
}
uint32_t UidChecksum(Bytes b) {
    if (!b.data || b.size < 12) return 0;
    uint16_t lo = 0, hi = 0;
    for (unsigned i = 0; i != 12; i += 2) {
        lo = Crc16Byte(lo, b.data[i]); hi = Crc16Byte(hi, b.data[i+1]);
    }
    return uint32_t(lo) | uint32_t(hi) << 16;
}
Status ParseIdentity(Bytes b, Identity& out, Error& e) {
    out = Identity(); ClearError(e);
    if (!b.data && b.size) return Fail(e, InvalidArgument, Buffer, 0);
    if (!b.size) return Fail(e, Corrupt, Name, 0);
    if (b.size > 80) return Fail(e, Unsupported, Name, 0);
    uint32_t ext = b.size;
    for (uint32_t i = 0; i < b.size; ++i) if (b.data[i] == '.') ext = i;
    uint32_t p = ext; Identity id = Identity();
    if (p >= 10 && b.data[p-1] == ']' && b.data[p-10] == '[') {
        if (!Hex8(b.data+p-9, id.uid3)) return Fail(e, Corrupt, Name, p-9);
        id.hasUid = true; p -= 10;
    }
    if (p >= 10 && b.data[p-1] == '}' && b.data[p-10] == '{') {
        if (!Hex8(b.data+p-9, id.version)) return Fail(e, Corrupt, Name, p-9);
        id.hasVersion = true; p -= 10;
    }
    if (!p || (ext < b.size && ext+1 == b.size)) return Fail(e, Corrupt, Name, p);
    // Dependency identity is a root name, not a search path. No policy/search here.
    for (uint32_t i = 0; i < b.size; ++i) {
        uint8_t c = b.data[i];
        const bool decoration = i >= p && i < ext;
        if (c < 0x20 || c >= 0x7f || c == '/' || c == '\\' || c == ':' ||
            c == '*' || c == '?' || (!decoration && (c == '[' || c == ']' || c == '{' || c == '}')))
            return Fail(e, Corrupt, Name, i);
    }
    id.base.offset = 0; id.base.size = p;
    id.extension.offset = ext; id.extension.size = b.size - ext; out = id;
    return Ok;
}
Status Parse(Bytes f, Image& out, Error& e) {
    out = Image(); ClearError(e);
    if (!f.data) return Fail(e, InvalidArgument, Buffer, 0);
    if (f.size < 156 || f.size > 0x0fffffffu) return Fail(e, Corrupt, Buffer, 0);
    const uint8_t* b = f.data; Image im = Image(); im.file = f;
    im.flags = R32(b+44);
    if ((im.flags & 0x0f000000u) != 0x02000000u || R16(b+122) != 0x1000)
        return Fail(e, Unsupported, Profile, 44);
    const uint32_t hs = R32(b+100);
    if (hs < 156 || hs > f.size) return Fail(e, Corrupt, Header, 100);
    im.header.offset = 0; im.header.size = hs;
    for (unsigned i = 0; i != 3; ++i) im.uids[i] = R32(b+i*4);
    const Bytes uidBytes = {b, 12};
    if (UidChecksum(uidBytes) != R32(b+12)) return Fail(e, Corrupt, Uid, 12);
    if (R32(b+16) != 0x434f5045) return Fail(e, Corrupt, Signature, 16);
    const Bytes headerBytes = {b, hs};
    if (HeaderCrc(headerBytes) != R32(b+20)) return Fail(e, Corrupt, Crc, 20);
    im.moduleVersion = R32(b+24);
    if (im.moduleVersion >= 0x80000000u || (im.moduleVersion & 65535) > 0x8000)
        return Fail(e, Unsupported, Version, 24);
    if (R32(b+28) || (im.flags & 0xf0000000u) != 0x20000000u ||
        (im.flags & (0xf8u | 0x400u | 0x200u | 0x2000u)))
        return Fail(e, Unsupported, Profile, 28);
    im.dll = (im.flags & 1) != 0;
    if (im.uids[0] != (im.dll ? 0x10000079u : 0x1000007au))
        return Fail(e, Unsupported, Uid, 0);
    im.code.offset = hs; im.code.size = R32(b+48);
    im.data.offset = R32(b+104); im.data.size = R32(b+52);
    im.heapMin = R32(b+56); im.heapMax = R32(b+60); im.stackSize = R32(b+64);
    im.bssSize = R32(b+68); im.entryCodeOffset = R32(b+72);
    im.codeLinkBase = R32(b+76); im.dataLinkBase = R32(b+80);
    im.dependencyCount = R32(b+84); im.exportCount = R32(b+92);
    im.textSize = R32(b+96); im.priority = R16(b+120);
    im.secureId = R32(b+128); im.vendorId = R32(b+132);
    im.capabilities[0] = R32(b+136); im.capabilities[1] = R32(b+140);
    im.exceptionDescriptor = R32(b+144);
    if (im.heapMin >= 0x80000000u || im.heapMax >= 0x80000000u || im.heapMax < im.heapMin ||
        im.stackSize >= 0x80000000u || im.bssSize >= 0x80000000u ||
        im.code.size >= 0x80000000u || im.data.size >= 0x80000000u ||
        im.textSize > im.code.size || !Fits(hs,im.code.size,f.size) ||
        ((im.codeLinkBase | im.dataLinkBase) & 3) ||
        im.data.size > 0x7fff0000u || im.bssSize > 0x7fff0000u-im.data.size)
        return Fail(e, Corrupt, Sizes, 48);
    const uint32_t rest = hs+im.code.size;
    if ((!im.data.offset && im.data.size) ||
        (im.data.offset && (im.data.offset < rest || !Fits(im.data.offset,im.data.size,f.size))))
        return Fail(e, Corrupt, Sizes, 104);
    if (!Fits(im.entryCodeOffset,16,im.code.size)) return Fail(e, Corrupt, Entry, 72);
    if ((im.exceptionDescriptor & 1) && im.exceptionDescriptor >= im.code.size)
        return Fail(e, Corrupt, Entry, 144);
    // Writable DLLs are an unsupported initial G profile, not corrupt images.
    if (im.dll && (im.data.size || im.bssSize)) return Fail(e, Unsupported, Profile, 52);
    if (im.exportCount > 65535) return Fail(e, Corrupt, Exports, 92);
    im.exports.offset = R32(b+88); im.exports.size = im.exportCount*4;
    if (im.exportCount && (im.exports.offset < hs || !Fits(im.exports.offset,im.exports.size,rest)))
        return Fail(e, Corrupt, Exports, 88);
    im.exportDescription.offset = 155;
    im.exportDescription.size = R16(b+152); im.exportDescriptionType = b[154];
    Status s = Description(im,e); if (s != Ok) return s;
    s = ValidateRelocs(f,R32(b+112),im.code.size,rest,im.codeRelocs,e); if (s != Ok) return s;
    s = ValidateRelocs(f,R32(b+116),im.data.size,rest,im.dataRelocs,e); if (s != Ok) return s;
    const uint32_t io = R32(b+108);
    if (im.dependencyCount >= 0x80000000u || (im.dependencyCount && !io))
        return Fail(e, Corrupt, Imports, 84);
    if (io) {
        if (io < rest || !Fits(io,4,f.size)) return Fail(e, Corrupt, Imports, io);
        im.imports.offset = io; im.imports.size = R32(b+io);
        if (im.imports.size < 4 || !Fits(io,im.imports.size,f.size) ||
            im.dependencyCount > (im.imports.size-4)/8)
            return Fail(e, Corrupt, Imports, io);
        uint32_t p = io+4, total = 0; ImportView v = ImportView();
        for (uint32_t i = 0; i < im.dependencyCount; ++i) {
            s = Import(im,i,p,total,v,e); if (s != Ok) return s;
        }
        im.importCount = total;
    }
    out = im; return Ok;
}
Status ImportAt(const Image& im, uint32_t index, ImportView& out, Error& e) {
    out = ImportView(); ClearError(e);
    if (index >= im.dependencyCount) return Fail(e, InvalidArgument, Index, index);
    uint32_t p = im.imports.offset+4, total = 0; ImportView v = ImportView();
    for (uint32_t i = 0; i <= index; ++i) {
        Status s = Import(im,i,p,total,v,e); if (s != Ok) return s;
    }
    out = v; return Ok;
}
Status ExportAt(const Image& im, uint32_t ordinal, ExportView& out, Error& e) {
    out = ExportView(); ClearError(e);
    if (!ordinal || ordinal > im.exportCount) return Fail(e, InvalidArgument, Index, ordinal);
    out.raw = R32(im.file.data+im.exports.offset+(ordinal-1)*4);
    out.absent = !Present(im,ordinal-1); return Ok;
}
Status RelocBlockAt(const Image& im, const RelocView& v, uint32_t& cursor,
                    Range& block, uint32_t& page, Error& e) {
    block = Range(); page = 0; ClearError(e);
    if (!v.section.size && !cursor) return Ok;
    if (!cursor) cursor = 8;
    if (cursor == v.section.size) return Ok;
    if (!Fits(cursor,8,v.section.size) || !Fits(v.section.offset,v.section.size,im.file.size))
        return Fail(e, InvalidArgument, Index, cursor);
    uint32_t off = v.section.offset+cursor, size = R32(im.file.data+off+4);
    if (size < 8 || !Fits(cursor,size,v.section.size)) return Fail(e, Corrupt, Relocations, off);
    block.offset = off; block.size = size;
    page = R32(im.file.data+off); cursor += size;
    return Ok;
}
} // namespace E32G
