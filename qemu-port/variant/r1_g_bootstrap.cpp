#include <x86.h>
#include <kernel/kernel.h>
#include "r1_g_bootstrap.h"

namespace R1Kernel {
namespace {
TBool Busy=EFalse; // serialized launching thread contract, not an IPC lock
TUint Fold(TUint c){return c>='A' && c<='Z'?c+32:c;}
TBool Equal(R1G::ImageName a,R1G::ImageName b) {
    if(a.units!=b.units)return EFalse;
    for(TUint n=0;n<a.units;++n)if(Fold(a.text[n])!=Fold(b.text[n]))return EFalse;
    return ETrue;
}
class Source : public R1G::ImageSource {
public:
    Source(const BootRequest& b):boot(b),serial(0) {memclr(tokens,sizeof(tokens));}
    TInt Validate() {
        if(!boot.manifest || !boot.count || boot.count>MaxSegments || boot.root>=boot.count)return KErrArgument;
        for(TUint n=0;n<boot.count;++n) {
            const ManifestEntry& e=boot.manifest[n];
            if(!e.bytes.data || !e.bytes.size || e.bytes.size>16*1024*1024 ||
               !e.absoluteName.text || e.absoluteName.units<12 || e.absoluteName.units>R1G::MaxNameUnits)return KErrArgument;
            const uint16_t* p=e.absoluteName.text;
            if(Fold(p[0])<'a' || Fold(p[0])>'z' || p[1]!=':')return KErrBadName;
            const char* prefix="\\sys\\bin\\";
            for(TUint k=0;k<9;++k)if(Fold(p[k+2])!=TUint(prefix[k]))return KErrPermissionDenied;
            for(TUint k=11;k<e.absoluteName.units;++k)
                if(p[k]>127 || !p[k] || p[k]=='\\' || p[k]=='/' || p[k]==':')return KErrBadName;
            for(TUint k=0;k<n;++k)if(Equal(e.absoluteName,boot.manifest[k].absoluteName))return KErrAlreadyExists;
        }
        return KErrNone;
    }
    R1G::Result Candidate(E32G::Bytes root,uint32_t index,R1G::Path& out) {
        TUint found=0;
        for(TUint n=0;n<boot.count;++n) {
            R1G::ImageName path=boot.manifest[n].absoluteName;uint8_t leaf[80];
            if(path.units-11>80)continue;
            for(TUint j=11;j<path.units;++j)leaf[j-11]=uint8_t(path.text[j]);
            E32G::Bytes name={leaf,path.units-11};E32G::Identity id;E32G::Error error;
            if(E32G::ParseIdentity(name,id,error)!=E32G::Ok)continue;
            if(root.size!=id.base.size+id.extension.size)continue;
            TBool match=ETrue;
            for(TUint j=0;j<root.size;++j) {
                TUint k=j<id.base.size?id.base.offset+j:id.extension.offset+j-id.base.size;
                if(Fold(leaf[k])!=Fold(root.data[j]))match=EFalse;
            }
            if(!match || found++!=index)continue;
            out.units=path.units;for(TUint j=0;j<path.units;++j)out.text[j]=path.text[j];return KErrNone;
        }
        return KErrNotFound;
    }
    R1G::Result OpenSnapshot(R1G::ImageName name,R1G::Snapshot& out) {
        for(TUint n=0;n<boot.count;++n)if(Equal(name,boot.manifest[n].absoluteName)) {
            // One retained plus best/trial duplicate views per graph candidate.
            for(TUint k=0;k<MaxSegments*3;++k)if(!tokens[k].generation) {
                if(serial==0xffffffffu)return KErrOverflow;
                tokens[k].slot=n;tokens[k].generation=++serial;out.slot=k;out.generation=serial;return KErrNone;
            }
            return KErrNoMemory;
        }
        return KErrPermissionDenied;
    }
    const ManifestEntry* Get(R1G::Snapshot t) {
        return t.slot<MaxSegments*3 && t.generation && tokens[t.slot].generation==t.generation?
            &boot.manifest[tokens[t.slot].slot]:0;
    }
    R1G::Result Size(R1G::Snapshot t,uint32_t& out) {const ManifestEntry* e=Get(t);if(!e)return KErrBadHandle;out=e->bytes.size;return KErrNone;}
    R1G::Result Bytes(R1G::Snapshot t,E32G::Bytes& out) {const ManifestEntry* e=Get(t);if(!e)return KErrBadHandle;out=e->bytes;return KErrNone;}
    R1G::Result ReadAt(R1G::Snapshot t,uint32_t off,uint8_t* dst,uint32_t cap,uint32_t& actual) {
        actual=0;const ManifestEntry* e=Get(t);if(!e)return KErrBadHandle;
        if(off>e->bytes.size || (cap && !dst))return KErrArgument;
        actual=e->bytes.size-off;if(actual>cap)actual=cap;
        if(actual)memcpy(dst,e->bytes.data+off,actual);
        return KErrNone;
    }
    void Close(R1G::Snapshot t) {__ASSERT_ALWAYS(Get(t),Kern::Fault("R1GSnapshot",1));tokens[t.slot].generation=0;}
private:
    const BootRequest& boot;TUint serial;R1G::Snapshot tokens[MaxSegments*3];
};
struct Work {
    Objects objects;Source source;
    R1G::Node nodes[MaxSegments];R1G::Edge edges[MaxEdges];TUint order[MaxSegments];
    R1G::ObjectRecord records[MaxSegments];R1G::Plan plan;R1G::Ledger ledger;
    Work(const BootRequest& b):source(b) {
        R1G::InitPlan(plan,nodes,MaxSegments,edges,MaxEdges,order,MaxSegments);
        R1G::InitLedger(ledger,records,MaxSegments);
    }
};
}
TInt LoadSuspended(const BootRequest& boot,BootResult& out) {
    BootResult zero={};out=zero;
    if(Busy)return KErrInUse;
    if(boot.arguments.units>256 || (boot.arguments.units && !boot.arguments.text))return KErrArgument;
    // Initial fixture is zero-capability; no arbitrary privileged EXE launch.
    Busy=ETrue;NKern::ThreadEnterCS();
    TAny* storage=Kern::AllocZ(sizeof(Work));
    if(!storage){NKern::ThreadLeaveCS();Busy=EFalse;return KErrNoMemory;}
    Work* w=new(storage) Work(boot);TInt r=w->source.Validate();
    TPtrC8 args((const TUint8*)boot.arguments.text,boot.arguments.units*2);
    if(!r)r=w->objects.Begin(args);
    if(!r) {
        R1G::LoadRequest req={};req.name=boot.manifest[boot.root].absoluteName;req.kind=R1G::ProcessLoad;
        req.requested=boot.required;req.requestedSecureId=boot.requiredSid;req.minStack=boot.minStack;
        req.client.threadHandle=KCurrentThreadHandle;req.client.processHandle=KCurrentProcessHandle;req.client.ownerType=EOwnerThread;
        R1G::Limits limits={16*1024*1024,32*1024*1024,MaxSegments};
        r=R1G::BuildPlan(w->plan,w->source,req,limits,out.diagnostic);
        if(!r && (w->plan.nodes[0].image.capabilities[0] || w->plan.nodes[0].image.capabilities[1]))r=KErrPermissionDenied;
        if(!r) {
            R1G::LoadResult result;r=R1G::Materialise(w->plan,w->objects,w->ledger,result,out.diagnostic);
            if(!r){out.handle=result.clientHandle;out.process=w->objects.PublishedProcess();}
        }
    }
    R1G::ReleasePlan(w->plan);w->objects.End();w->~Work();Kern::Free(storage);
    out.diagnostic.result=r;NKern::ThreadLeaveCS();Busy=EFalse;return r;
}
} // namespace R1Kernel
