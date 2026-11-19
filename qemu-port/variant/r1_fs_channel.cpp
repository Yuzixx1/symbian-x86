#include <kernel/kern_priv.h>
#include <e32atomics.h>
#include "qemu_variant.h"
#include "r1_fat_offset.h"
#include "r1_fs_channel.h"
#include "../fs/r1_fs_channel.h"
#include "../fs/r1_fs_channel_state.h"
#include "../fs/r1_fs_wire_logic.h"

namespace W = R1FsWire;
namespace {
DProcess* sPinned=0; // held reference; never a PID/name/security-token alias
DProcess* Pinned() { return (DProcess*)__e32_atomic_load_acq_ptr((const volatile TAny*)&sPinned); }
TBool Authorized(DProcess* p) { return W::AuthorizedProcess(Pinned(),p); }

struct FatOps {
    static TInt Open(char d,const char* p,TBool c,R1Fat::File& f){ return R1Fat::Open(d,p,c,f); }
    static TInt Size(R1Fat::File& f,TUint32& s){ return R1Fat::Size(f,s); }
    static TInt Read(R1Fat::File& f,TUint32 o,TUint8* d,TUint n,TUint& done){ return R1Fat::Read(f,o,d,n,done); }
    static TInt Write(R1Fat::File& f,TUint32 o,const TUint8* d,TUint n,TUint& done){ return R1Fat::Write(f,o,d,n,done); }
    static TInt Resize(R1Fat::File& f,TUint32 s){ return R1Fat::Resize(f,s); }
    static TInt Flush(R1Fat::File& f){ return R1Fat::Flush(f); }
    static void Close(R1Fat::File& f){ R1Fat::Close(f); }
};
typedef W::ChannelState<R1Fat::File,FatOps> ChannelState;

struct FsLock { static void Enter(){UserFilesLock();} static void Leave(){UserFilesUnlock();} };
struct OwnerAuth { DThread* thread; bool operator()() const { return Authorized(thread->iOwningProcess); } };
struct UserReply { DThread* thread; TAny* dst; int32_t operator()(const W::Packet& p) const {
    return Kern::ThreadRawWrite(thread,dst,&p,sizeof(p)); } };

class Channel : public DLogicalChannelBase {
    ChannelState state_;
public:
    Channel() : state_() {}
    ~Channel() {
        UserFilesLock();
        state_.CloseAll();
        UserFilesUnlock();
    }
    TInt DoCreate(TInt,const TDesC8*,const TVersion& v) {
        if (v.iMajor!=W::Version) return KErrNotSupported;
        return Authorized(Kern::CurrentThread().iOwningProcess)?KErrNone:KErrAccessDenied;
    }
    TInt RequestUserHandle(DThread* t,TOwnerType type) {
        return (t && type==EOwnerThread && Authorized(t->iOwningProcess))?
            KErrNone:KErrAccessDenied;
    }
    TInt Request(TInt op,TAny* src,TAny* dst) {
        DThread& thread=Kern::CurrentThread();
        if (!Authorized(thread.iOwningProcess)) return KErrAccessDenied;
        if (!src || !dst || op<0 || op>W::Inspect) return KErrArgument;
        NKern::ThreadEnterCS();
        W::Packet* p=(W::Packet*)Kern::Alloc(sizeof(W::Packet));
        NKern::ThreadLeaveCS();
        if(!p) return KErrNoMemory;
        TInt r=Kern::ThreadRawRead(&thread,src,p,sizeof(*p));
        if (!r) {
            OwnerAuth auth; auth.thread=&thread;
            UserReply reply; reply.thread=&thread; reply.dst=dst;
            r=W::RunRequest<ChannelState,FsLock>(state_,*p,(unsigned)op,auth,reply);
        }
        NKern::ThreadEnterCS(); Kern::Free(p); NKern::ThreadLeaveCS();
        return r;
    }
};
class Device : public DLogicalDevice {
public:
    TInt Install() { _LIT(KName,"R1PrivateFat"); return SetName(&KName); }
    void GetCaps(TDes8& d) const { Kern::InfoCopy(d,0,0); }
    TInt Create(DLogicalChannelBase*& c) {
        if (!Authorized(Kern::CurrentThread().iOwningProcess)) return KErrAccessDenied;
        c=new Channel; return c?KErrNone:KErrNoMemory;
    }
};
}
TInt R1FsChannelPin(DProcess* p) {
    if(!p) return KErrArgument;
    NKern::ThreadEnterCS();
    TInt r=p->Open();
    if(!r) {
        NKern::LockSystem();
        if (sPinned) r=KErrInUse;
        else __e32_atomic_store_rel_ptr((volatile TAny*)&sPinned,p);
        NKern::UnlockSystem();
        if (r) p->Close(0);
    }
    NKern::ThreadLeaveCS(); return r;
}
void R1FsChannelRevoke() {
    NKern::ThreadEnterCS();
    NKern::LockSystem(); DProcess* p=sPinned;
    __e32_atomic_store_rel_ptr((volatile TAny*)&sPinned,(TAny*)0);
    NKern::UnlockSystem();
    if (p) p->Close(0);
    NKern::ThreadLeaveCS();
}
TInt R1FsChannelInstall() {
    NKern::ThreadEnterCS();
    Device* d=new Device;
    TInt r=d?Kern::InstallLogicalDevice(d):KErrNoMemory;
    if(r && d) d->Close(0);
    NKern::ThreadLeaveCS(); return r;
}
