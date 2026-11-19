#include "e32g_format.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    FILE* f = std::fopen(argv[1], "rb");
    if (!f) return 2;
    if (std::fseek(f,0,SEEK_END)) return 2;
    long length = std::ftell(f);
    if (length < 0 || length > 0x0fffffff) return 2;
    std::rewind(f);
    unsigned char* bytes = static_cast<unsigned char*>(std::malloc(length ? length : 1));
    if (!bytes) return 2;
    if (std::fread(bytes,1,length,f) != static_cast<size_t>(length)) return 2;
    std::fclose(f);
    unsigned char* saved = static_cast<unsigned char*>(std::malloc(length ? length : 1));
    if (!saved) return 2;
    std::memcpy(saved,bytes,length);
    E32G::Image im = E32G::Image(); E32G::Error e = E32G::Error();
    const E32G::Bytes file = {bytes, static_cast<uint32_t>(length)};
    E32G::Status s = E32G::Parse(file,im,e);
    if (std::memcmp(saved,bytes,length)) return 3;
    std::printf("%u %u %u",unsigned(s),unsigned(e.issue),e.offset);
    if (s == E32G::Ok) {
        unsigned holes = 0, blocks = 0, imports = 0;
        for (uint32_t o = 1; o <= im.exportCount; ++o) {
            E32G::ExportView v = E32G::ExportView();
            if (E32G::ExportAt(im,o,v,e) != E32G::Ok) return 3;
            holes += v.absent;
        }
        for (uint32_t i = 0; i < im.dependencyCount; ++i) {
            E32G::ImportView v = E32G::ImportView();
            if (E32G::ImportAt(im,i,v,e) != E32G::Ok) return 3;
            imports += v.count;
        }
        const E32G::RelocView views[] = {im.codeRelocs,im.dataRelocs};
        for (unsigned i = 0; i < 2; ++i) {
            uint32_t cursor = 0, page; E32G::Range block = E32G::Range();
            do {
                if (E32G::RelocBlockAt(im,views[i],cursor,block,page,e) != E32G::Ok) return 3;
                if (block.size) ++blocks;
            } while (block.size);
        }
        if (imports != im.importCount) return 3;
        E32G::ExportView x = E32G::ExportView();
        E32G::ImportView y = E32G::ImportView();
        if (E32G::ExportAt(im,0,x,e) != E32G::InvalidArgument ||
            E32G::ImportAt(im,im.dependencyCount,y,e) != E32G::InvalidArgument) return 3;
        std::printf(" %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
            im.code.offset,im.code.size,im.data.size,im.bssSize,im.exportCount,
            holes,im.dependencyCount,im.importCount,im.codeRelocs.count,blocks,
            im.secureId,im.vendorId,im.capabilities[0],im.capabilities[1],im.entryCodeOffset);
    } else if (im.file.data || im.code.size) return 3; // no partial parse result
    std::puts("");
    std::free(saved); std::free(bytes); return 0;
}
