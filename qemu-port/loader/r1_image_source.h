#ifndef R1_IMAGE_SOURCE_H
#define R1_IMAGE_SOURCE_H
#include "e32g_format.h"

namespace R1G {
typedef int32_t Result;
enum { Success=0, NotFound=-1, NoMemory=-4, NotSupported=-5,
       Argument=-6, CorruptImage=-20, TooBig=-40, PermissionDenied=-46 };
struct ImageName { const uint16_t* text; uint32_t units; };
struct ImageId { uint32_t uid1, uid2, uid3, version; };
struct Security { uint32_t secureId, vendorId, caps[2]; };
struct Snapshot { uint32_t slot, generation; };
enum { MaxNameUnits=256 };
struct Path { uint16_t text[MaxNameUnits]; uint32_t units; };
class ImageSource {
public:
    // Trusted manifest/sysbin policy is implemented here, never inferred from CRC.
    // Enumerate candidate absolute names for a root dependency name. NotFound ends
    // enumeration. No handles are acquired. Returned path is copied into caller.
    virtual Result Candidate(E32G::Bytes rootName, uint32_t index, Path& out)=0;
    // On failure OpenSnapshot acquires NOTHING; success owns one immutable view.
    virtual Result OpenSnapshot(ImageName absoluteName, Snapshot& out)=0;
    virtual Result Size(Snapshot, uint32_t& out)=0;
    virtual Result Bytes(Snapshot, E32G::Bytes& out)=0;
    virtual Result ReadAt(Snapshot, uint32_t offset, uint8_t* dst,
                          uint32_t capacity, uint32_t& actual)=0;
    // Non-failing, closes exactly the one acquired generation token.
    virtual void Close(Snapshot)=0;
protected: virtual ~ImageSource() {}
};
} // namespace R1G
#endif
