#ifndef R1_OBJECT_ADAPTER_H
#define R1_OBJECT_ADAPTER_H
#include "e32g_plan.h"

namespace R1G {
struct CodeToken { uint32_t slot, generation; };
struct ProcessToken { uint32_t slot, generation; };
// All are 32-bit TARGET addresses, not host pointers or link/file offsets.
struct Mapping {
    uint32_t codeLoad, codeRun, dataLoad, dataRun;
    uint32_t entryRun, exportsRun;
};
enum Region { CodeRegion, DataTemplateRegion };
struct CreateSpec {
    const E32G::Image* image; ImageName canonicalName; Security security;
    uint32_t dependencies, minStack; ClientContext client;
};
struct ObjectRecord {
    CodeToken code; ProcessToken process; Mapping mapping;
    bool acquired, rootProcess, finalised;
};
// Each successful Create owns one temporary reference, never an immortal cache.
// Root CreateProcess creates the root EXE segment too, suspended. Create failure
// MUST release partial/stillborn allocations itself and return no token; the
// core must not Release a pointer already consumed by failed kernel creation.
class LoaderObjects {
public:
    virtual Result CreateProcess(const CreateSpec&, ObjectRecord& out)=0;
    virtual Result CreateCode(const CreateSpec&, ObjectRecord& out)=0;
    // Non-failing destruction of unpublished root, including partial mappings.
    // Must happen before any dependent temporary code reference is released.
    virtual void AbortProcess(ProcessToken)=0;
    // Close exactly one temporary code reference, including root code reference.
    // A process record owns BOTH a process lifetime and this explicit temp ref.
    virtual void CloseCode(CodeToken)=0;
    // Bounded offsets relative to mapped code or initialized-data template;
    // adapter writes only transaction-owned allocations. No raw pointer IPC.
    virtual Result Write(CodeToken, Region, uint32_t offset, E32G::Bytes)=0;
    virtual Result ReadWord(CodeToken, Region, uint32_t offset, uint32_t& out)=0;
    virtual Result WriteWord(CodeToken, Region, uint32_t offset, uint32_t value)=0;
    virtual Result AddDependency(CodeToken importer, CodeToken exporter)=0;
    virtual Result FinaliseCode(CodeToken, const Mapping&)=0;
    // Loads root, OpenDeps/data init contract; still NO Resume or publication.
    virtual Result FinaliseProcess(ProcessToken, const Mapping&)=0;
    // Monotonic boot-wide ID allocator; exclude 0/ffffffff. May consume IDs on
    // failed load; never roll them back or reuse. Returns failure before wrap.
    virtual Result NextStamp(uint32_t& out)=0;
    // Atomic publication boundary: failure installs NO externally visible
    // handle/reference. On success graph/process refs are retained independently
    // of temporaries; process remains suspended. No IPC descriptor write here.
    // Dynamic provider must resolve genuine client handle rollback before use.
    virtual Result PublishProcess(ProcessToken, const ClientContext&, int32_t& handle)=0;
    virtual Result PublishLibrary(CodeToken, const ClientContext&, int32_t& handle)=0;
protected: virtual ~LoaderObjects() {}
};
} // namespace R1G
#endif
