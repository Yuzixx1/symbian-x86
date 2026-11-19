#include <x86.h>
#include <kernel/kernel.h>
#include <memmodel.h>
#include "r1_g_objects.h"

namespace R1Kernel {
namespace {
TUint Stamp=0; // serialized bootstrap-only allocator; never reset by Objects
TBool Client(const R1G::ClientContext& c) {
    return c.threadHandle==KCurrentThreadHandle && c.processHandle==KCurrentProcessHandle && c.ownerType==EOwnerThread;
}
R1G::Mapping Map(const TCodeSegCreateInfo& i) {
    R1G::Mapping m={i.iCodeLoadAddress,i.iCodeRunAddress,i.iDataLoadAddress,
        i.iDataRunAddress,i.iEntryPtVeneer,i.iExportDir};return m;
}
TBool Same(const R1G::Mapping& a,const R1G::Mapping& b) {
    return a.codeLoad==b.codeLoad && a.codeRun==b.codeRun && a.dataLoad==b.dataLoad &&
        a.dataRun==b.dataRun && a.entryRun==b.entryRun && a.exportsRun==b.exportsRun;
}
}
Objects::Objects():iCount(0),iEdgeCount(0),iCaller(0),iPublished(0),iActive(EFalse) {
    // TProcessCreateInfo contains descriptors: preserve constructor invariants.
    for(TUint n=0;n<MaxSegments;++n) {
        iSlots[n].code=0;iSlots[n].process=0;iSlots[n].generation=1;
        iSlots[n].edges=0;iSlots[n].temporary=EFalse;iSlots[n].loaded=EFalse;
    }
}
TInt Objects::Begin(const TDesC8& args) {
    if(iActive || iCount || (args.Length()&1))return KErrArgument;
    if(Kern::CurrentThread().iThreadType==EThreadUser)return KErrPermissionDenied;
    iCaller=&Kern::CurrentThread();iArguments.Set(args);iActive=ETrue;return KErrNone;
}
void Objects::DropEdges() {
    DCodeSeg::Wait();
    for(TUint n=iEdgeCount;n>0;--n)if(iEdges[n-1].held) {
        iSlots[iEdges[n-1].exporter].code->Close();iEdges[n-1].held=EFalse;
    }
    DCodeSeg::Signal();iEdgeCount=0;
}
void Objects::End() {
    for(TUint n=0;n<iCount;++n)__ASSERT_ALWAYS(!iSlots[n].temporary,Kern::Fault("R1GEnd",1));
    DropEdges();iArguments.Set(KNullDesC8);iActive=EFalse;
}
DProcess* Objects::PublishedProcess() const {return iPublished;}
const Slot* Objects::Inspect(TUint n) const {return n<iCount?&iSlots[n]:0;}
Slot* Objects::Find(R1G::CodeToken t) {
    if(!iActive || &Kern::CurrentThread()!=iCaller || t.slot>=iCount)return 0;
    Slot& s=iSlots[t.slot];return s.temporary && s.generation==t.generation?&s:0;
}
Slot* Objects::Find(R1G::ProcessToken t) {
    R1G::CodeToken c={t.slot,t.generation};Slot* s=Find(c);return s && s->process?s:0;
}
TInt Objects::Prepare(const R1G::CreateSpec& s,Slot*& out) {
    out=0;
    if(!iActive || &Kern::CurrentThread()!=iCaller || !Client(s.client))return KErrPermissionDenied;
    if(!s.image || !s.canonicalName.text || !s.canonicalName.units || s.canonicalName.units>KMaxFileName)return KErrArgument;
    if(iCount==MaxSegments || s.dependencies>MaxEdges)return KErrTooBig;
    const E32G::Image& im=*s.image;
    // Fail closed if an adapter is accidentally compiled/integrated with a
    // different CPU/ABI or a profile not admitted by the format reader.
    if(im.flags&0x18)return KErrNotSupported;
    if(im.dll && (im.data.size || im.bssSize))return KErrNotSupported;
    if(s.security.secureId!=im.secureId || s.security.vendorId!=im.vendorId ||
       s.security.caps[0]!=im.capabilities[0] || s.security.caps[1]!=im.capabilities[1])return KErrPermissionDenied;
    Slot& slot=iSlots[iCount];TProcessCreateInfo& i=slot.info;
    // Zero non-descriptor fields, then reconstruct entire class to clear paging,
    // handles, flags and the descriptor correctly (kernel allocation is zeroed).
    i.~TProcessCreateInfo();memclr(&i,sizeof(i));new (&i) TProcessCreateInfo;
    TUint leaf=0;
    for(TUint n=0;n<s.canonicalName.units;++n) {
        TUint ch=s.canonicalName.text[n];if(ch>127 || !ch)return KErrBadName;
        i.iFileName.Append(TUint8(ch));if(ch=='\\' || ch=='/')leaf=n+1;
    }
    E32G::Bytes name={i.iFileName.Ptr()+leaf,TUint(i.iFileName.Length())-leaf};
    E32G::Identity id;E32G::Error error;
    if(E32G::ParseIdentity(name,id,error)!=E32G::Ok)return KErrBadName;
    // Kernel filename excludes version decorations. Rebuild canonical root+ext;
    // UID is carried as metadata rather than redundant filename decoration.
    TBuf8<KMaxFileName> clean;clean.Copy(i.iFileName.Left(leaf));
    clean.Append(TPtrC8(name.data+id.base.offset,id.base.size));
    i.iExtOffset=clean.Length();clean.Append(TPtrC8(name.data+id.extension.offset,id.extension.size));
    i.iFileName=clean;i.iRootNameOffset=leaf;i.iRootNameLength=id.base.size;
    for(TUint n=0;n<3;++n)i.iUids.iUid[n].iUid=im.uids[n];
    i.iModuleVersion=im.moduleVersion;i.iAttr=ECodeSegAttGlobal;
    // x86 ABI bits are zero; parser has already rejected other ABI profiles.
    if(im.flags&0x4000)i.iAttr|=ECodeSegAttSMPSafe;
    i.iCodeSize=im.code.size;i.iTextSize=im.textSize;i.iDataSize=im.data.size;
    i.iBssSize=im.bssSize;i.iTotalDataSize=im.data.size+im.bssSize;
    i.iFileEntryPoint=im.entryCodeOffset;i.iEntryPtVeneer=im.entryCodeOffset;
    i.iExportDir=im.exportCount?im.exports.offset-im.code.offset:0;i.iExportDirCount=im.exportCount;
    i.iDepCount=s.dependencies;i.iExceptionDescriptor=im.exceptionDescriptor;
    i.iS.iSecureId=im.secureId;i.iS.iVendorId=im.vendorId;
    i.iS.iCaps[0]=im.capabilities[0];i.iS.iCaps[1]=im.capabilities[1];
    i.iHeapSizeMin=im.heapMin;i.iHeapSizeMax=im.heapMax;
    if(s.minStack>0x7fffffffu)return KErrArgument;
    i.iStackSize=im.stackSize>s.minStack?im.stackSize:s.minStack;
    i.iPriority=TProcessPriority(im.priority);i.iOwnerType=EOwnerThread;
    i.iClientHandle=KCurrentThreadHandle;i.iSecurityZone=KSecurityZoneUnique;
    i.iFlags=TProcessCreateInfo::EDataUnpaged;
    if(im.flags&0x800)i.iDebugAttributes|=TProcessCreateInfo::EDebugAllowed;
    out=&slot;return KErrNone;
}
void Objects::CopyOut(Slot& s,R1G::ObjectRecord& o) {
    const TUint index=&s-iSlots;R1G::ObjectRecord z={};o=z;
    o.code.slot=index;o.code.generation=s.generation;o.process.slot=index;o.process.generation=s.generation;
    o.mapping=Map(s.info);o.acquired=true;o.rootProcess=s.process!=0;
}
R1G::Result Objects::CreateProcess(const R1G::CreateSpec& spec,R1G::ObjectRecord& out) {
    Slot* s;TInt r=Prepare(spec,s);if(r)return r;
    if(iCount || spec.image->dll)return KErrNotSupported;
    HBuf* command=HBuf::New(iArguments);if(!command)return KErrNoMemory;
    DProcess* p=0;r=Kern::ProcessCreate(p,s->info,command,0);delete command;
    if(r) {
        // Kernel Release consumed the process's code reference. If successful
        // seg Create preceded a later thread failure, original creator ref is
        // still in info.iHandle and must be closed exactly once.
        if(s->info.iHandle) {DCodeSeg::Wait();((DCodeSeg*)s->info.iHandle)->Close();DCodeSeg::Signal();}
        return r;
    }
    s->process=p;s->code=(DCodeSeg*)s->info.iHandle;s->temporary=ETrue;++iCount;
    CopyOut(*s,out);return KErrNone;
}
R1G::Result Objects::CreateCode(const R1G::CreateSpec& spec,R1G::ObjectRecord& out) {
    Slot* s;TInt r=Prepare(spec,s);if(r)return r;
    if(!spec.image->dll)return KErrNotSupported;
    DCodeSeg* code=M::NewCodeSeg(s->info);if(!code)return KErrNoMemory;
    r=code->Create(s->info,0);
    if(r){DCodeSeg::Wait();code->Close();DCodeSeg::Signal();return r;}
    s->code=code;s->info.iHandle=code;s->temporary=ETrue;++iCount;CopyOut(*s,out);return KErrNone;
}
void Objects::AbortProcess(R1G::ProcessToken token) {
    Slot* s=Find(token);__ASSERT_ALWAYS(s && !iPublished,Kern::Fault("R1GAbort",1));
    DProcess* p=s->process;
    // Same synchronous unpublished cleanup as Kern::ProcessCreate failure.
    if(!p->iThreadQ.IsEmpty()) {
        DThread* t=_LOFF(p->iThreadQ.First()->Deque(),DThread,iProcessLink);
        t->iProcessLink.iNext=0;t->Stillborn();
    }
    s->process=0;p->Release();
}
void Objects::CloseCode(R1G::CodeToken token) {
    Slot* s=Find(token);__ASSERT_ALWAYS(s,Kern::Fault("R1GClose",1));
    // All graph-held pointers cease to be needed after root abort or successful
    // OpenDeps. Drop holds before first temporary is closed, under code mutex.
    DropEdges();DCodeSeg::Wait();s->code->Close();DCodeSeg::Signal();s->temporary=EFalse;
}
TInt Objects::RegionAddress(R1G::CodeToken t,R1G::Region reg,TUint off,TUint len,TLinAddr& addr) {
    Slot* s=Find(t);if(!s || s->loaded)return KErrArgument;
    TUint size=reg==R1G::CodeRegion?s->info.iCodeSize:s->info.iDataSize;
    if(reg!=R1G::CodeRegion && reg!=R1G::DataTemplateRegion)return KErrArgument;
    if(off>size || len>size-off)return KErrArgument;
    addr=(reg==R1G::CodeRegion?s->info.iCodeLoadAddress:s->info.iDataLoadAddress)+off;
    return KErrNone;
}
R1G::Result Objects::Write(R1G::CodeToken t,R1G::Region reg,uint32_t off,E32G::Bytes b) {
    TLinAddr addr;TInt r=RegionAddress(t,reg,off,b.size,addr);if(r)return r;
    if(b.size && !b.data)return KErrArgument;
    if(b.size)memcpy((TAny*)addr,b.data,b.size);
    return KErrNone;
}
R1G::Result Objects::ReadWord(R1G::CodeToken t,R1G::Region reg,uint32_t off,uint32_t& value) {
    TLinAddr addr;TInt r=RegionAddress(t,reg,off,4,addr);if(r)return r;
    const TUint8* b=(const TUint8*)addr;value=TUint(b[0])|TUint(b[1])<<8|TUint(b[2])<<16|TUint(b[3])<<24;return KErrNone;
}
R1G::Result Objects::WriteWord(R1G::CodeToken t,R1G::Region reg,uint32_t off,uint32_t value) {
    TLinAddr addr;TInt r=RegionAddress(t,reg,off,4,addr);if(r)return r;
    TUint8* b=(TUint8*)addr;for(TUint n=0;n<4;++n)b[n]=TUint8(value>>(n*8));return KErrNone;
}
R1G::Result Objects::AddDependency(R1G::CodeToken from,R1G::CodeToken to) {
    Slot* a=Find(from);Slot* b=Find(to);
    if(!a || !b || a==b || a->loaded || a->edges>=TUint(a->info.iDepCount))return KErrArgument;
    if(iEdgeCount==MaxEdges)return KErrTooBig;
    DCodeSeg::Wait();TInt r=a->code->AddDependency(b->code);
    if(!r) {
        b->code->CheckedOpen();EdgeRef& e=iEdges[iEdgeCount++];e.importer=from.slot;e.exporter=to.slot;e.held=ETrue;++a->edges;
    }
    DCodeSeg::Signal();return r;
}
R1G::Result Objects::FinaliseCode(R1G::CodeToken t,const R1G::Mapping& m) {
    Slot* s=Find(t);if(!s || s->process || s->loaded || s->edges!=TUint(s->info.iDepCount) || !Same(m,Map(s->info)))return KErrArgument;
    DCodeSeg::Wait();TInt r=s->code->Loaded(s->info);if(!r){s->code->iMark|=DCodeSeg::EMarkLoaded;s->loaded=ETrue;}DCodeSeg::Signal();return r;
}
R1G::Result Objects::FinaliseProcess(R1G::ProcessToken t,const R1G::Mapping& m) {
    Slot* s=Find(t);if(!s || s->loaded || s->edges!=TUint(s->info.iDepCount) || !Same(m,Map(s->info)))return KErrArgument;
    TInt r=s->process->Loaded(s->info);if(!r)s->loaded=ETrue;return r;
}
R1G::Result Objects::NextStamp(uint32_t& value) {
    if(!iActive || iCaller!=&Kern::CurrentThread())return KErrPermissionDenied;
    if(Stamp>=0xfffffffeu)return KErrOverflow;
    value=++Stamp;return KErrNone;
}
R1G::Result Objects::PublishProcess(R1G::ProcessToken t,const R1G::ClientContext& c,int32_t& handle) {
    Slot* s=Find(t);if(!s || !s->loaded || iPublished || !Client(c))return KErrPermissionDenied;
    TInt h=0;TInt r=iCaller->MakeHandleAndOpen(EOwnerThread,s->process,h);
    if(!r){handle=h;iPublished=s->process;s->process->iCreatorId=iCaller->iOwningProcess->iId;s->process->iCreatorInfo=iCaller->iOwningProcess->iS;}
    return r;
}
R1G::Result Objects::PublishLibrary(R1G::CodeToken,const R1G::ClientContext&,int32_t&) {return KErrNotSupported;}
} // namespace R1Kernel
