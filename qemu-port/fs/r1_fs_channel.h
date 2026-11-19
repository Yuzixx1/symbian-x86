#ifndef R1_FS_CHANNEL_WIRE_H
#define R1_FS_CHANNEL_WIRE_H
#include <stdint.h>
#include "r1_fs_backend.h"
// Private fixed wire. User addresses occur only as arguments to DoControl;
// they are copied by the kernel and NEVER forwarded to FAT. All fields are
// fixed-width so the layout is identical for 32-bit user, 32-bit kernel and
// the host test build; the asserts below make a layout change a compile error.
namespace R1FsWire {
enum { Version=1, PathBytes=128, DataBytes=4096, Slots=64 };
enum Operation { Open=0, Read=1, Write=2, Resize=3, Flush=4,
                 Close=5, Size=6, Inspect=7 };
struct Packet {
    uint32_t version, op, slot, generation;
    uint32_t disposition, length, offset, requestedSize;
    uint32_t done, size, stable, quarantined;
    int32_t status;
    char path[PathBytes]; // canonical ASCII drive + absolute 8.3 path
    uint8_t data[DataBytes];
};
// Size check works on every compiler.
typedef char PacketSizeAssert[(sizeof(Packet)==4276)?1:-1];
#if defined(__GNUC__) && (__GNUC__ >= 4)
// 16+16+16+4 = 52, path at 52, data at 180, total 4276; no implicit padding.
typedef char PacketStatusOffsetAssert[(__builtin_offsetof(Packet,status)==48)?1:-1];
typedef char PacketPathOffsetAssert[(__builtin_offsetof(Packet,path)==52)?1:-1];
typedef char PacketDataOffsetAssert[(__builtin_offsetof(Packet,data)==180)?1:-1];
#endif
}

namespace R1Fs {
// Exchange() returns 0 only when the packet holds a complete, validated
// response. Any nonzero return is a transport/protocol fault: the packet must
// not be inspected, because it may be partially overwritten. Semantic results
// (including KErrBadHandle) travel in Packet::status with a 0 return.
class Transport {
public:
    virtual ~Transport() {}
    virtual int32_t Exchange(R1FsWire::Packet& packet) = 0;
};

// Portable user-side Backend adapter. One instance is owned by the FS owner
// thread. Attach() binds a transport and allocates the one fixed packet;
// Detach() unbinds it but keeps storage-quarantine/broken state. Quarantine is
// whole-device and is never cleared by Close/Detach/Attach in this object.
class BackendCore {
public:
    BackendCore();
    ~BackendCore();
    int32_t Attach(Transport& transport);
    void Detach();
    bool Connected() const { return transport_!=0; }
    bool Quarantined() const { return quarantined_; }
    bool Broken() const { return broken_; }
    int32_t Lookup(const Path&,Meta&);
    int32_t Open(const Path&,Disposition,Key&,Meta&);
    IoResult ReadAt(Key,uint64_t,uint8_t*,uint32_t);
    IoResult WriteAt(Key,uint64_t,const uint8_t*,uint32_t);
    IoResult Resize(Key,uint64_t);
    int32_t Flush(Key);
    int32_t Close(Key);
private:
    BackendCore(const BackendCore&);
    BackendCore& operator=(const BackendCore&);
    int32_t FailClosed() const;
    R1FsWire::Packet& PacketFor(unsigned operation);
    Transport* transport_;
    R1FsWire::Packet* packet_;
    bool quarantined_, broken_;
};
}
#endif
