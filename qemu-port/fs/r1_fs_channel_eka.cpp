#include "r1_fs_channel_eka.h"
#include "r1_fs_channel.h"
#include <e32std.h>
namespace R1Fs {
class ChannelTransport : public RBusLogicalChannel, public Transport {
public:
    TInt OpenChannel() { _LIT(KName,"R1PrivateFat");
        return DoCreate(KName,TVersion(R1FsWire::Version,0,0),0,0,0,EOwnerThread); }
    int32_t Exchange(R1FsWire::Packet& p) {
        const uint32_t op=p.op;
        TInt r=DoControl(op,&p,&p);
        if (r) return r;
        if (p.version!=R1FsWire::Version || p.op!=op) return KErrCorrupt;
        return KErrNone;
    }
};
ChannelBackend::ChannelBackend() : channel_(0) {}
ChannelBackend::~ChannelBackend() { Disconnect(); }
int32_t ChannelBackend::Connect() {
    if (channel_) return KErrInUse;
    ChannelTransport* c=new ChannelTransport;
    if (!c) return KErrNoMemory;
    int32_t r=c->OpenChannel();
    if (r) { delete c; return r; }
    channel_=c;
    r=core_.Attach(*c);
    if (r) { c->Close(); delete c; channel_=0; return r; }
    return KErrNone;
}
void ChannelBackend::Disconnect() {
    core_.Detach();
    if (channel_) { channel_->Close(); delete channel_; channel_=0; }
}
int32_t ChannelBackend::Lookup(const Path& path,Meta& meta) {
    return core_.Lookup(path,meta);
}
int32_t ChannelBackend::Open(const Path& path,Disposition how,Key& key,Meta& meta) {
    return core_.Open(path,how,key,meta);
}
IoResult ChannelBackend::ReadAt(Key key,uint64_t offset,uint8_t* dst,uint32_t n) {
    return core_.ReadAt(key,offset,dst,n);
}
IoResult ChannelBackend::WriteAt(Key key,uint64_t offset,const uint8_t* src,uint32_t n) {
    return core_.WriteAt(key,offset,src,n);
}
IoResult ChannelBackend::Resize(Key key,uint64_t size) {
    return core_.Resize(key,size);
}
int32_t ChannelBackend::Flush(Key key) {
    return core_.Flush(key);
}
int32_t ChannelBackend::Close(Key key) {
    return core_.Close(key);
}
}
