#ifndef R1_G_OBJECTS_H
#define R1_G_OBJECTS_H
#include <kernel/kern_priv.h>
#include <e32ldr_private.h>
#include "../loader/e32g_load.h"

namespace R1Kernel {
enum { MaxSegments=16, MaxEdges=64 };
struct Slot {
    TProcessCreateInfo info;
    DCodeSeg* code; DProcess* process;
    TUint generation, edges;
    TBool temporary, loaded;
};
struct EdgeRef { TUint importer, exporter; TBool held; };
// Heap/static caller storage: never put this object on a small kernel stack.
// Single transaction, launching kernel thread only, CS held throughout; no code
// mutex/system lock held on entry. No user client impersonation or cache reuse.
class Objects : public R1G::LoaderObjects {
public:
    Objects();
    TInt Begin(const TDesC8& utf16ArgumentBytes);
    void End(); // after Materialise/Abort; releases adapter-only edge holds
    DProcess* PublishedProcess() const; // borrowed while returned handle lives
    const Slot* Inspect(TUint index) const;
    R1G::Result CreateProcess(const R1G::CreateSpec&, R1G::ObjectRecord&);
    R1G::Result CreateCode(const R1G::CreateSpec&, R1G::ObjectRecord&);
    void AbortProcess(R1G::ProcessToken);
    void CloseCode(R1G::CodeToken);
    R1G::Result Write(R1G::CodeToken,R1G::Region,uint32_t,E32G::Bytes);
    R1G::Result ReadWord(R1G::CodeToken,R1G::Region,uint32_t,uint32_t&);
    R1G::Result WriteWord(R1G::CodeToken,R1G::Region,uint32_t,uint32_t);
    R1G::Result AddDependency(R1G::CodeToken,R1G::CodeToken);
    R1G::Result FinaliseCode(R1G::CodeToken,const R1G::Mapping&);
    R1G::Result FinaliseProcess(R1G::ProcessToken,const R1G::Mapping&);
    R1G::Result NextStamp(uint32_t&);
    R1G::Result PublishProcess(R1G::ProcessToken,const R1G::ClientContext&,int32_t&);
    R1G::Result PublishLibrary(R1G::CodeToken,const R1G::ClientContext&,int32_t&);
private:
    Objects(const Objects&); Objects& operator=(const Objects&);
    TInt Prepare(const R1G::CreateSpec&,Slot*&);
    Slot* Find(R1G::CodeToken);
    Slot* Find(R1G::ProcessToken);
    TInt RegionAddress(R1G::CodeToken,R1G::Region,TUint,TUint,TLinAddr&);
    void CopyOut(Slot&,R1G::ObjectRecord&);
    void DropEdges();
    Slot iSlots[MaxSegments]; EdgeRef iEdges[MaxEdges];
    TUint iCount,iEdgeCount; DThread* iCaller;
    TPtrC8 iArguments; DProcess* iPublished; TBool iActive;
};
} // namespace R1Kernel
#endif
