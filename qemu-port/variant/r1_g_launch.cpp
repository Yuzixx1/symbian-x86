#include <x86.h>
#include <kernel/kern_priv.h>
#include <e32ldr_private.h>
#include "qemu_variant.h"
#include "r1_g_launch.h"
#ifdef R1_G_ASSETS
#include "r1_g_bootstrap.h"
#include "r1_g_assets.inc"

namespace {
struct Record {DProcess* process;TInt handle;TUint id;};
Record Records[4]; // handles belong to this serialized launching kernel thread
DThread* Owner=0;
TBool Eq(const char* a,const char* b){while(*a && *a==*b){++a;++b;}return *a==*b;}
TUint Word(TUint a){const TUint8* b=(const TUint8*)a;return TUint(b[0])|TUint(b[1])<<8|TUint(b[2])<<16|TUint(b[3])<<24;}
// Protect all borrowed process pointers with owned thread handles. Never put G
// segments in legacy ExecRecRemember: creator references were already closed.
TInt Reap() {
    TInt count=0;NKern::ThreadEnterCS();
    for(TUint n=0;n<4;++n)if(Records[n].process) {
        Record& rec=Records[n];NKern::LockSystem();
        TBool dead=rec.process->iExitType!=EExitPending && rec.process->iThreadQ.IsEmpty();
        TInt type=rec.process->iExitType,reason=rec.process->iExitReason;
        NKern::UnlockSystem();
        if(dead) {
            KPrintf("G:exit id=%u type=%d reason=%d\r\n",rec.id,type,reason);
            TInt h=rec.handle;rec.process=0;rec.handle=0;
            Kern::CloseHandle(NULL,h);++count; // no pointer use after close
        }
    }
    NKern::ThreadLeaveCS();return count;
}
TInt Observe(TUint slot) {
    TUint start=NKern::TickCount(),id=Records[slot].id;
    while(NKern::TickCount()-start<10000u) {
        Reap();if(!Records[slot].process)return KErrNone;
        NKern::Sleep(20);
    }
    KPrintf("G:timeout id=%u retained handle; use gboot reap or kill\r\n",id);return KErrTimedOut;
}
TInt Abandon(R1Kernel::BootResult& result,TUint slot) {
    Record& rec=Records[slot];rec.process=result.process;rec.handle=result.handle;rec.id=result.process->iId;
    NKern::ThreadEnterCS();
    // Published object: genuine death, NOT unpublished Stillborn/Release.
    result.process->Die(EExitKill,KErrCancel,KNullDesC);
    result.process=0;result.handle=0;
    NKern::ThreadLeaveCS();KPrintf("G:abandon requested\r\n");
    return Observe(slot); // keep handle until death observed, including timeout
}
TInt TemplateCheck(const E32G::Image& im,const TCodeSegCreateInfo& info) {
    for(TUint n=0;n<im.data.size;++n) {
        TUint8 expected=im.file.data[im.data.offset+n];
        TUint cursor=0,page=0;E32G::Range block;E32G::Error error;
        do {
            if(E32G::RelocBlockAt(im,im.dataRelocs,cursor,block,page,error)!=E32G::Ok)return KErrCorrupt;
            for(TUint q=8;q<block.size;q+=2) {
                const TUint8* e=im.file.data+block.offset+q;TUint entry=e[0]|TUint(e[1])<<8;
                if(!entry)continue;
                TUint off=page+(entry&4095);
                if(n<off || n-off>=4)continue;
                const TUint8* b=im.file.data+im.data.offset+off;
                TUint v=TUint(b[0])|TUint(b[1])<<8|TUint(b[2])<<16|TUint(b[3])<<24;
                TBool code=(entry&0xf000)==0x1000 || ((entry&0xf000)==0x3000 && TUint(v-im.codeLinkBase)<im.code.size);
                v+=code?info.iCodeRunAddress-im.codeLinkBase:info.iDataRunAddress-im.dataLinkBase;
                expected=TUint8(v>>((n-off)*8));
            }
        }while(block.size);
        if(((const TUint8*)info.iDataLoadAddress)[n]!=expected)return KErrCorrupt;
    }
    return KErrNone;
}
TInt Diagnose(R1Kernel::BootResult& result) {
    TUint flags=K::KernelConfigFlags();
    KPrintf("G:security effective=%08x forced=%08x\r\n",flags,TUint(__PLATSEC_FORCED_FLAGS__));
    if((flags&__PLATSEC_FORCED_FLAGS__)!=__PLATSEC_FORCED_FLAGS__)return KErrPermissionDenied;
    NKern::ThreadEnterCS();
    TAny* storage=Kern::AllocZ(sizeof(TCodeSegCreateInfo));
    if(!storage){NKern::ThreadLeaveCS();return KErrNoMemory;}
    TCodeSegCreateInfo* info=new(storage) TCodeSegCreateInfo;
    DCodeSeg::Wait();DCodeSeg* code=result.process->CodeSeg();
    if(!code){DCodeSeg::Signal();Kern::Free(storage);NKern::ThreadLeaveCS();return KErrCorrupt;}
    code->Info(*info);DCodeSeg::Signal();
    TInt r=KErrNone;E32G::Image image;E32G::Error error;
    if(E32G::Parse(gManifest[0].bytes,image,error)!=E32G::Ok)r=KErrCorrupt;
    else {
        KPrintf("G:suspended id=%u entry=%08x exports=%08x code=%08x/%08x data=%08x/%08x\r\n",
            result.process->iId,info->iFileEntryPoint,info->iExportDir,info->iCodeLoadAddress,info->iCodeRunAddress,info->iDataLoadAddress,info->iDataRunAddress);
        KPrintf("G:identity sid=%08x vid=%08x caps=%08x/%08x\r\n",info->iS.iSecureId,info->iS.iVendorId,info->iS.iCaps[0],info->iS.iCaps[1]);
        if(info->iFileEntryPoint!=info->iCodeRunAddress+image.entryCodeOffset ||
           info->iEntryPtVeneer!=info->iFileEntryPoint || info->iS.iSecureId!=image.secureId ||
           info->iS.iVendorId!=image.vendorId || info->iS.iCaps[0] || info->iS.iCaps[1])r=KErrCorrupt;
        TUint stamp=Word(info->iCodeLoadAddress+image.entryCodeOffset+12);
        if(!stamp || stamp==0xffffffffu)r=KErrCorrupt;
        // IAT must point into a genuine dependency, never an ordinal/pilot stub.
        for(TUint k=0;!r && k<image.dependencyCount;++k) {
            E32G::ImportView v;if(E32G::ImportAt(image,k,v,error)!=E32G::Ok){r=KErrCorrupt;break;}
            DCodeSeg* dependency=code->iDeps[k];if(!dependency){r=KErrCorrupt;break;}
            for(TUint j=0;!r && j<v.count;++j) {
                const TUint8* b=image.file.data+v.iat.offset+j*4;
                TUint ordinal=TUint(b[0])|TUint(b[1])<<8|TUint(b[2])<<16|TUint(b[3])<<24;
                TUint actual=Word(info->iCodeLoadAddress+v.iat.offset-image.code.offset+j*4);
                TUint expected=(TUint)dependency->Lookup(ordinal);
                if(!expected || actual!=expected)r=KErrCorrupt;
            }
        }
        // Template is allocated separately from final EXE data; don't demand
        // InitData/BSS values before genuine StaticCallList startup.
        if(image.data.size && (!info->iDataLoadAddress || !info->iDataRunAddress || info->iDataLoadAddress==info->iDataRunAddress))r=KErrCorrupt;
        if(!r)r=TemplateCheck(image,*info);
        KPrintf("G:checks iat/stamp/template result=%d stamp=%08x dataBytes=%u bss=%u\r\n",r,stamp,image.data.size,image.bssSize);
    }
    info->~TCodeSegCreateInfo();Kern::Free(storage);NKern::ThreadLeaveCS();return r;
}
}
TInt R1GCommand(const char* mode) {
    if(Owner && Owner!=&Kern::CurrentThread())return KErrPermissionDenied;
    Owner=&Kern::CurrentThread();
    if(Eq(mode,"reap")){KPrintf("G:reap count=%d\r\n",Reap());return KErrNone;}
    TBool check=Eq(mode,"check"),abandon=Eq(mode,"abandon"),heap=Eq(mode,"heap"),sched=Eq(mode,"sched");
    TBool badManifest=Eq(mode,"reject-manifest"),badImport=Eq(mode,"reject-import");
    if(!check && !heap && !sched && !abandon && !badManifest && !badImport) {
        KPrintf("gboot check|heap|sched|abandon|reap|reject-manifest|reject-import\r\n");return KErrArgument;
    }
    Reap();TInt slot=-1;for(TUint n=0;n<4;++n)if(!Records[n].process){slot=n;break;}
    if(slot<0)return KErrOverflow;
    R1Kernel::ManifestEntry manifest[3]={gManifest[0],gManifest[1],gManifest[2]};
    if(badManifest)manifest[0].absoluteName=manifest[1].absoluteName; // duplicate, no mutation
    R1Kernel::BootRequest req={};req.manifest=manifest;req.count=badImport?1:3;req.root=0;
    req.required.uid1=0x1000007au;req.required.version=0xffffffffu;
    static const uint16_t Heap[]={'h','e','a','p'},Sched[]={'s','c','h','e','d'};
    req.arguments.text=sched?Sched:Heap;req.arguments.units=sched?5:4;
    E32EnsureUserTss();R1Kernel::BootResult result;TInt r=R1Kernel::LoadSuspended(req,result);
    if(badManifest || badImport) {
        if(!r){Abandon(result,slot);return KErrCorrupt;}
        KPrintf("G:rejection %s result=%d\r\n",mode,r);return KErrNone;
    }
    if(r){KPrintf("G:load result=%d node=%u detail=%u\r\n",r,result.diagnostic.node,result.diagnostic.detail);return r;}
    r=Diagnose(result);
    if(r || check || abandon){TInt cleanup=Abandon(result,slot);return r?r:cleanup;}
    Record& record=Records[slot];record.process=result.process;record.handle=result.handle;record.id=result.process->iId;
    KPrintf("G:resume mode=%s id=%u\r\n",mode,record.id);
    NKern::ThreadEnterCS();NKern::LockSystem();result.process->Resume();NKern::UnlockSystem();NKern::ThreadLeaveCS();
    return Observe(slot);
}
#else
TInt R1GCommand(const char*) {KPrintf("G:unavailable provision hash-pinned local assets explicitly\r\n");return KErrNotSupported;}
#endif
