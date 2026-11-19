#ifndef E32G_FORMAT_H
#define E32G_FORMAT_H

// Port-owned immutable x86 E32 PE2/V format reader. No allocation or execution.
#include <stdint.h>

namespace E32G {
struct Bytes { const uint8_t* data; uint32_t size; };
struct Range { uint32_t offset, size; }; // file coordinates, never addresses
enum Status { Ok, Corrupt, Unsupported, InvalidArgument };
enum Issue {
    None, Buffer, Header, Profile, Uid, Signature, Crc, Version, Sizes,
    Entry, Exports, ExportDescription, Imports, Name, Relocations, Index
};
struct Error { Status status; Issue issue; uint32_t offset; };
struct Identity {
    Range base, extension; // views into the input NAME bytes, not the image
    uint32_t uid3, version;
    bool hasUid, hasVersion;
};
struct RelocView { Range section; uint32_t count; }; // section includes 8-byte header
struct ImportView { Range name, iat; uint32_t count; Identity identity; };
struct ExportView { uint32_t raw; bool absent; }; // raw PE section-relative value
struct Image {
    Bytes file; // borrowed immutable storage, must outlive every view
    uint32_t uids[3], flags, moduleVersion;
    uint32_t secureId, vendorId, capabilities[2], exceptionDescriptor;
    uint32_t heapMin, heapMax, stackSize, bssSize;
    uint32_t codeLinkBase, dataLinkBase, entryCodeOffset, textSize;
    uint16_t priority;
    bool dll;
    Range header, code, data, imports, exports, exportDescription;
    uint32_t dependencyCount, importCount, exportCount;
    uint8_t exportDescriptionType;
    RelocView codeRelocs, dataRelocs;
};

// CRC is reflected polynomial 0xedb88320, seed 0, no final xor.
uint32_t Crc32(Bytes bytes);
uint32_t HeaderCrc(Bytes header); // substitutes 0xc90fdaa2 at bytes 20..23
uint32_t UidChecksum(Bytes firstTwelveBytes);
Status ParseIdentity(Bytes name, Identity& out, Error& error);
Status Parse(Bytes file, Image& out, Error& error);
// Accessors accept only an unmodified successful Parse result and live file.
Status ImportAt(const Image& image, uint32_t index, ImportView& out, Error& error);
Status ExportAt(const Image& image, uint32_t ordinal, ExportView& out, Error& error);
// Relocation iteration: cursor starts at 0; returns block file views, with entries
// in the last block at block.offset+8. End: Ok with block.size==0.
Status RelocBlockAt(const Image& image, const RelocView& view,
                    uint32_t& cursor, Range& block, uint32_t& page, Error& error);
} // namespace E32G
#endif
