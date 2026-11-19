#ifndef R1_FS_BACKEND_H
#define R1_FS_BACKEND_H

// Private, injectable user-service/backend contract. C++98, no IPC pointers or
// raw kernel objects. The channel implementation MUST authenticate the pinned
// portfilesrv process on open, handle transfer and every request; this interface
// is not an authorization bypass and must never accept sys\bin mutations.
#include <stdint.h>

namespace R1Fs {
// Symbian error values used by the portable FS/channel code. Kept numeric so
// host tests do not need the SDK; the values are the genuine system codes.
enum Status { Success=0, NotFound=-1, General=-2, NoMemory=-4, NotSupported=-5,
              Argument=-6, BadHandle=-8, AlreadyExists=-11, PathNotFound=-12,
              InUse=-14, NotReady=-18, Corrupt=-20, AccessDenied=-21,
              Overflow=-22, DiskFull=-26, BadName=-28, TooBig=-40 };
enum { KPathUnits = 256, KTransferBytes = 4096 };
struct Path { uint16_t text[KPathUnits]; uint32_t length; }; // absolute, canonical
struct Key { uint32_t slot, generation; }; // backend-owned, never public index
struct Meta { uint64_t size; uint32_t identity; }; // stable while opened
enum Disposition { OpenExisting, CreateNew, ReplaceExisting };
struct IoResult {
    int32_t error;              // Symbian negative error or KErrNone
    uint32_t committed;         // confirmed data-sector prefix; not published size
    uint64_t size;              // authoritative ONLY when stable=true
    bool stable;                // false: size unknown; caller fails the share closed
};
class Backend {
public:
    ~Backend() {}
    // Path restricted to mounted public fixture namespace; no sys\bin write.
    // Lookup distinguishes missing parent (PathNotFound) and leaf (NotFound).
    int32_t Lookup(const Path& path, Meta& meta) { return NotFound; }
    // Failure may have side effects for Create/Replace: never claim rollback.
    // Caller serializes this with other Core calls on the FS owner thread;
    // reject unsupported names and cross-volume paths rather than truncating.
    int32_t Open(const Path& path, Disposition how,
                 Key& key, Meta& meta) { return NotFound; }
    // Each method validates key generation and offset/length before sector math.
    // ReadAt sets committed=actual bytes; WriteAt records committed prefix on
    // error. Caller owns bounded buffers until return; no client descriptors.
    IoResult ReadAt(Key key, uint64_t offset, uint8_t* dst,
                    uint32_t capacity) {
        IoResult r; r.error = NotFound; return r;
    }
    IoResult WriteAt(Key key, uint64_t offset, const uint8_t* src,
                     uint32_t length) {
        IoResult r; r.error = NotFound; return r;
    }
    // Resize returns actual size on error; no promise of atomic/power-safe FAT.
    IoResult Resize(Key key, uint64_t newSize) {
        IoResult r; r.error = NotFound; return r;
    }
    // Flush MUST propagate metadata and correct-volume device cache errors.
    int32_t Flush(Key key) { return NotFound; }
    // Release reference; close errors never masquerade as successful Flush.
    int32_t Close(Key key) { return BadHandle; }
};
}
#endif