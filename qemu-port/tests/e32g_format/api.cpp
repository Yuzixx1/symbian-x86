#include "e32g_format.h"
#include <cassert>
#include <cstring>

int main() {
    E32G::Image im = E32G::Image(); E32G::Error e = E32G::Error();
    E32G::Identity id = E32G::Identity();
    const E32G::Bytes invalid = {0, 1};
    assert(E32G::Parse(invalid,im,e) == E32G::InvalidArgument);
    assert(!im.file.data && e.issue == E32G::Buffer);
    const char* name = "EuSeR{000A0000}[100039E5].dll";
    E32G::Bytes b = {reinterpret_cast<const uint8_t*>(name),uint32_t(std::strlen(name))};
    assert(E32G::ParseIdentity(b,id,e) == E32G::Ok);
    assert(id.hasUid && id.uid3 == 0x100039e5 && id.hasVersion && id.version == 0xa0000);
    assert(id.base.offset == 0 && id.base.size == 5);
    assert(id.extension.offset == 25 && id.extension.size == 4);
    assert(E32G::ParseIdentity(invalid,id,e) == E32G::InvalidArgument);
    assert(!id.hasUid && !id.base.size);
    const uint8_t embedded[] = {'a',0,'b'};
    const E32G::Bytes embeddedBytes = {embedded, 3};
    assert(E32G::ParseIdentity(embeddedBytes,id,e) == E32G::Corrupt);
    const uint8_t plain[] = {'x','.','d','l','l'};
    const E32G::Bytes plainBytes = {plain, 5};
    assert(E32G::ParseIdentity(plainBytes,id,e) == E32G::Ok);
    assert(!id.hasUid && !id.hasVersion && id.base.size == 1);
    // Independently known raw/no-final-XOR CRC vector.
    const uint8_t check[] = {'1','2','3','4','5','6','7','8','9'};
    const E32G::Bytes checkBytes = {check, 9};
    assert(E32G::Crc32(checkBytes) == 0x2dfd2d88);
    return 0;
}
