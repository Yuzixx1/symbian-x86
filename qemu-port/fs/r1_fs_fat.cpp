// r1_fs_fat.cpp — FAT32 backend implementing R1Fs::Backend using variant/fat32.cpp
// Whole-file read/write with offset-based ReadAt/WriteAt synthesized by
// buffering. Suitable for R1 checkpoint 2: small-to-medium files on a
// disposable FAT32 volume.
#include "r1_fs_fat.h"
#include "r1_fs_backend.h"
#include <nk_trace.h>

// FatReadPath/FatWritePath/FatMount are defined in variant/fat32.cpp
// (compiled into the same kernel). Declare them here so the linker can
// resolve them. They are C++ functions with Symbian types, not extern "C".
extern int FatReadPath(const char* aPath, TUint8* aBuf, TUint aMax, TUint& aOutLen);
extern int FatWritePath(const char* aPath, const TUint8* aData, TUint aLen);
extern int FatMount(int aDev, int aPart);

namespace R1Fs {

FatBackend::FatBackend() : nextKeyGen_(1), mounted_(false), mountDev_(0), mountPart_(0)
{
    for (uint32_t i = 0; i < KMaxOpenFiles; ++i) {
        files_[i].used = false;
        files_[i].buffer = 0;
        files_[i].bufferSize = 0;
        files_[i].dirty = false;
    }
}

FatBackend::~FatBackend()
{
    for (uint32_t i = 0; i < KMaxOpenFiles; ++i) {
        if (files_[i].used && files_[i].buffer) {
            delete[] files_[i].buffer;
            files_[i].buffer = 0;
        }
    }
}

FatBackend::OpenFile* FatBackend::Find(const Key& key)
{
    for (uint32_t i = 0; i < KMaxOpenFiles; ++i) {
        if (files_[i].used && files_[i].key.slot == key.slot &&
            files_[i].key.generation == key.generation) {
            return &files_[i];
        }
    }
    return 0;
}

const FatBackend::OpenFile* FatBackend::Find(const Key& key) const
{
    for (uint32_t i = 0; i < KMaxOpenFiles; ++i) {
        if (files_[i].used && files_[i].key.slot == key.slot &&
            files_[i].key.generation == key.generation) {
            return &files_[i];
        }
    }
    return 0;
}

// Convert a Path (UTF-16, absolute canonical like "C:\\R1TEST\\FILE.EXT")
// to a null-terminated ASCII string. All characters in our paths are ASCII.
static int32_t PathToCStr(const Path& path, char* out, uint32_t outSize)
{
    if (path.length >= outSize) return BadName;
    for (uint32_t i = 0; i < path.length; ++i) {
        uint16_t c = path.text[i];
        if (c > 0x7f) return BadName;
        out[i] = (char)c;
    }
    out[path.length] = 0;
    return Success;
}

// Extract the leaf name from a path (everything after the last backslash)
// and validate 8.3 format.
int32_t FatBackend::PathToLeaf(const Path& path, char leaf[13])
{
    const uint16_t* text = path.text;
    uint32_t len = path.length;
    const uint16_t* lastSlash = 0;
    for (uint32_t i = 0; i < len; ++i) {
        if (text[i] == '\\') lastSlash = text + i;
    }
    const uint16_t* start = lastSlash ? lastSlash + 1 : text;
    uint32_t leafLen = len - (uint32_t)(start - text);
    if (leafLen < 1 || leafLen > 12) return BadName;

    char buf[13];
    uint32_t j = 0;
    for (uint32_t i = 0; i < leafLen && j < 12; ++i) {
        uint16_t c = start[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        if (c > 0x7f) return BadName;
        buf[j++] = (char)c;
    }
    buf[j] = 0;

    const char* dot = 0;
    for (uint32_t i = 0; i < j; ++i) {
        if (buf[i] == '.') { dot = buf + i; break; }
    }
    if (!dot) return BadName;
    uint32_t baseLen = (uint32_t)(dot - buf);
    uint32_t extLen = j - (uint32_t)(dot - buf) - 1;
    if (baseLen < 1 || baseLen > 8) return BadName;
    if (extLen < 1 || extLen > 3) return BadName;
    for (uint32_t i = 0; i < baseLen; ++i) {
        char c = buf[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) return BadName;
    }
    for (uint32_t i = 0; i < extLen; ++i) {
        char c = dot[i + 1];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return BadName;
    }

    for (uint32_t i = 0; i < j; ++i) leaf[i] = buf[i];
    leaf[j] = 0;
    return Success;
}

int32_t FatBackend::Mount(uint8_t dev, uint8_t part)
{
    TInt r = FatMount((int)dev, (int)part);
    if (r != KErrNone) return r;
    mounted_ = true;
    mountDev_ = dev;
    mountPart_ = part;
    return Success;
}

int32_t FatBackend::Lookup(const Path& path, Meta& meta)
{
    if (!mounted_) return NotReady;
    Key key = {0, 0};
    Meta m = {0, 0};
    int32_t r = Open(path, OpenExisting, key, m);
    if (r) return r;
    meta = m;
    return Close(key);
}

int32_t FatBackend::Open(const Path& path, Disposition how, Key& key, Meta& meta)
{
    if (!mounted_) return NotReady;

    // Find a free slot
    OpenFile* slot = 0;
    for (uint32_t i = 0; i < KMaxOpenFiles; ++i) {
        if (!files_[i].used) { slot = &files_[i]; break; }
    }
    if (!slot) return NoMemory;

    if (how == CreateNew) {
        // Create empty file
        slot->used = true;
        slot->generation = nextKeyGen_++;
        slot->key.slot = (uint32_t)(slot - files_);
        slot->key.generation = slot->generation;
        slot->path = path;
        slot->write = true;
        slot->big = false;
        slot->buffer = new uint8_t[1];
        if (!slot->buffer) { slot->used = false; return NoMemory; }
        slot->bufferSize = 0;
        slot->size = 0;
        slot->dirty = true;
        key = slot->key;
        meta.size = 0;
        meta.identity = slot->generation;
        return Success;
    }

    if (how == ReplaceExisting) return NotSupported; // not yet implemented

    // OpenExisting: read whole file into buffer
    char pathBuf[KPathUnits + 1];
    int32_t r = PathToCStr(path, pathBuf, sizeof(pathBuf));
    if (r) return r;

    static uint8_t tmpBuf[65536];
    TUint outLen = 0;
    TInt fr = FatReadPath(pathBuf, tmpBuf, sizeof(tmpBuf), outLen);
    if (fr != KErrNone) return fr;

    slot->used = true;
    slot->generation = nextKeyGen_++;
    slot->key.slot = (uint32_t)(slot - files_);
    slot->key.generation = slot->generation;
    slot->path = path;
    slot->write = false;
    slot->big = false;
    slot->buffer = new uint8_t[outLen];
    if (!slot->buffer) { slot->used = false; return NoMemory; }
    for (uint32_t i = 0; i < outLen; ++i) slot->buffer[i] = tmpBuf[i];
    slot->bufferSize = (uint32_t)outLen;
    slot->size = (uint64_t)outLen;
    slot->dirty = false;
    key = slot->key;
    meta.size = (uint64_t)outLen;
    meta.identity = slot->generation;
    return Success;
}

IoResult FatBackend::ReadAt(Key key, uint64_t offset, uint8_t* dst, uint32_t capacity)
{
    IoResult r; r.error = Success; r.committed = 0; r.size = 0; r.stable = false;
    OpenFile* s = Find(key);
    if (!s) { r.error = BadHandle; return r; }

    if (offset > s->size) offset = s->size;
    uint64_t remaining = s->size - offset;
    uint32_t toRead = (uint32_t)(remaining < capacity ? remaining : capacity);
    if (toRead > 0) {
        for (uint32_t i = 0; i < toRead; ++i) dst[i] = s->buffer[offset + i];
    }
    r.committed = toRead;
    r.size = s->size;
    r.stable = true;
    return r;
}

IoResult FatBackend::WriteAt(Key key, uint64_t offset, const uint8_t* src, uint32_t length)
{
    IoResult r; r.error = Success; r.committed = 0; r.size = 0; r.stable = false;
    OpenFile* s = Find(key);
    if (!s) { r.error = BadHandle; return r; }
    if (!s->write) { r.error = AccessDenied; return r; }

    if (offset > s->size) offset = s->size;
    uint64_t newSize = offset + length;

    if (newSize > s->bufferSize) {
        uint8_t* newBuf = new uint8_t[newSize];
        if (!newBuf) { r.error = NoMemory; return r; }
        for (uint32_t i = 0; i < s->bufferSize; ++i) newBuf[i] = s->buffer[i];
        for (uint32_t i = s->bufferSize; i < newSize; ++i) newBuf[i] = 0;
        delete[] s->buffer;
        s->buffer = newBuf;
        s->bufferSize = (uint32_t)newSize;
    }

    for (uint32_t i = 0; i < length; ++i) s->buffer[offset + i] = src[i];
    s->size = newSize;
    s->dirty = true;

    r.committed = length;
    r.size = newSize;
    r.stable = true;
    return r;
}

IoResult FatBackend::Resize(Key key, uint64_t newSize)
{
    IoResult r; r.error = Success; r.committed = 0; r.size = 0; r.stable = false;
    OpenFile* s = Find(key);
    if (!s) { r.error = BadHandle; return r; }
    if (!s->write) { r.error = AccessDenied; return r; }

    if (newSize > s->bufferSize) {
        uint8_t* newBuf = new uint8_t[newSize];
        if (!newBuf) { r.error = NoMemory; return r; }
        for (uint32_t i = 0; i < s->bufferSize; ++i) newBuf[i] = s->buffer[i];
        for (uint32_t i = s->bufferSize; i < newSize; ++i) newBuf[i] = 0;
        delete[] s->buffer;
        s->buffer = newBuf;
        s->bufferSize = (uint32_t)newSize;
    }
    s->size = newSize;
    s->dirty = true;

    r.committed = 0;
    r.size = newSize;
    r.stable = true;
    return r;
}

int32_t FatBackend::Flush(Key key)
{
    OpenFile* s = Find(key);
    if (!s) return BadHandle;
    if (!s->dirty) return Success;

    char pathBuf[KPathUnits + 1];
    int32_t r = PathToCStr(s->path, pathBuf, sizeof(pathBuf));
    if (r) return r;

    TInt fr = FatWritePath(pathBuf, s->buffer, s->size);
    if (fr != KErrNone) return fr;

    s->dirty = false;
    return Success;
}

int32_t FatBackend::Close(Key key)
{
    OpenFile* s = Find(key);
    if (!s) return BadHandle;
    if (s->buffer) {
        delete[] s->buffer;
        s->buffer = 0;
    }
    s->used = false;
    s->bufferSize = 0;
    return Success;
}

} // namespace R1Fs
