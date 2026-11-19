#ifndef R1_FS_FAT_H
#define R1_FS_FAT_H

#include "r1_fs_backend.h"
#include <e32err.h>

namespace R1Fs {

// FAT32 backend implementing the Backend interface using variant/fat32.cpp
// (FatRead/FatWrite/FatMount). Whole-file read/write with offset-based
// ReadAt/WriteAt synthesized by buffering. Suitable for R1 checkpoint 2:
// small-to-medium files on a disposable FAT32 volume.
class FatBackend : public Backend {
public:
    FatBackend();
    ~FatBackend();

    // Backend interface
    int32_t Lookup(const Path& path, Meta& meta);
    int32_t Open(const Path& path, Disposition how, Key& key, Meta& meta);
    IoResult ReadAt(Key key, uint64_t offset, uint8_t* dst, uint32_t capacity);
    IoResult WriteAt(Key key, uint64_t offset, const uint8_t* src, uint32_t length);
    IoResult Resize(Key key, uint64_t newSize);
    int32_t Flush(Key key);
    int32_t Close(Key key);

    // Mount the FAT32 volume on the given drive/partition. Must be called
    // before any Open. Drive letter 'C' is the default system drive.
    int32_t Mount(uint8_t dev, uint8_t part);

private:
    struct OpenFile {
        bool used;
        uint32_t generation;
        Key key;
        Path path;
        uint64_t size;
        bool write;
        bool big;
        uint8_t* buffer;   // whole-file image in RAM
        uint32_t bufferSize;
        bool dirty;        // buffer differs from on-disk image
    };

    static const uint32_t KMaxOpenFiles = 16;
    OpenFile files_[KMaxOpenFiles];
    uint32_t nextKeyGen_;
    bool mounted_;
    uint8_t mountDev_;
    uint8_t mountPart_;

    OpenFile* Find(const Key& key);
    const OpenFile* Find(const Key& key) const;
    int32_t PathToLeaf(const Path& path, char leaf[13]);
    int32_t LoadWholeFile(const Path& path, uint8_t*& buf, uint32_t& bufSize, uint64_t& size);
};

}

#endif // R1_FS_FAT_H
