#include "r1_fs_channel.h"
#include "r1_fs_backend.h"
#include "r1_fs_wire_logic.h"
#include <string.h>
namespace R1Fs {
namespace {
int32_t Encode(const Path& path,char* out) {
    if(path.length<11 || path.length>=R1FsWire::PathBytes) return R1Fs::BadName;
    for(uint32_t i=0;i<path.length;++i) {
        if(path.text[i]>0x7f) return R1Fs::BadName;
        out[i]=char(path.text[i]);
    }
    out[path.length]=0;
    return R1FsWire::ValidPath(out)?R1Fs::Success:R1Fs::BadName;
}
IoResult MakeResult(int32_t e,uint32_t committed,uint64_t size,bool stable){
    IoResult r; r.error=e; r.committed=committed; r.size=size; r.stable=stable; return r;
}
}
BackendCore::BackendCore() : transport_(0), packet_(0), quarantined_(false), broken_(false) {}
BackendCore::~BackendCore() { Detach(); delete packet_; packet_=0; }
int32_t BackendCore::Attach(Transport& transport) {
    if (!packet_) {
        packet_=new R1FsWire::Packet;
        if (!packet_) return R1Fs::NoMemory;
    }
    transport_= &transport;
    return R1Fs::Success;
}
void BackendCore::Detach() { transport_=0; }
int32_t BackendCore::FailClosed() const {
    return quarantined_ ? R1Fs::Corrupt : R1Fs::NotReady;
}
R1FsWire::Packet& BackendCore::PacketFor(unsigned operation) {
    // Deterministic: the whole packet is zeroed, then the protocol fields set.
    // No user-supplied response/output field can survive a previous call.
    memset(packet_,0,sizeof(*packet_));
    packet_->version=R1FsWire::Version; packet_->op=operation;
    return *packet_;
}
int32_t BackendCore::Lookup(const Path& path,Meta& meta) {
    Key key={0,0}; int32_t r=Open(path,OpenExisting,key,meta);
    if (r) return r;
    return Close(key);
}
int32_t BackendCore::Open(const Path& path,Disposition how,Key& key,Meta& meta) {
    key.slot=key.generation=0; meta.size=0; meta.identity=0;
    if (!transport_) return R1Fs::NotReady;
    if (quarantined_) return R1Fs::Corrupt;
    if (broken_) return R1Fs::NotReady;
    if (how==ReplaceExisting) return R1Fs::NotSupported;
    if (how!=OpenExisting && how!=CreateNew) return R1Fs::Argument;
    R1FsWire::Packet& p=PacketFor(R1FsWire::Open);
    p.disposition=(how==CreateNew)?1:0;
    int32_t e=Encode(path,p.path); if (e) return e;
    int32_t t=transport_->Exchange(p);
    if (t) { broken_=true; return t; }
    int32_t r=p.status;
    if (R1FsWire::StorageUncertain(p)) { quarantined_=true; return r?r:R1Fs::Corrupt; }
    if (r) return r;
    if (!p.stable || p.slot>=R1FsWire::Slots || !p.generation) { broken_=true; return R1Fs::Corrupt; }
    key.slot=p.slot; key.generation=p.generation;
    meta.size=p.size; meta.identity=p.generation; return R1Fs::Success;
}
IoResult BackendCore::ReadAt(Key key,uint64_t offset,uint8_t* dst,uint32_t n) {
    if (!transport_) return MakeResult(R1Fs::NotReady,0,0,false);
    if (quarantined_) return MakeResult(R1Fs::Corrupt,0,0,false);
    if (broken_) return MakeResult(R1Fs::NotReady,0,0,false);
    if ((n && !dst) || n>R1FsWire::DataBytes || offset>(uint64_t)0xffffffffu)
        return MakeResult(R1Fs::Argument,0,0,false);
    R1FsWire::Packet& p=PacketFor(R1FsWire::Read);
    p.slot=key.slot; p.generation=key.generation; p.offset=(uint32_t)offset; p.length=n;
    int32_t t=transport_->Exchange(p);
    if (t) { broken_=true; return MakeResult(t,0,0,false); }
    int32_t r=p.status;
    if (R1FsWire::StorageUncertain(p)) { quarantined_=true; return MakeResult(r?r:R1Fs::Corrupt,0,0,false); }
    if (r) return MakeResult(r,0,0,false);
    if (!p.stable || R1FsWire::MalformedTransfer(p,n)) { broken_=true; return MakeResult(R1Fs::Corrupt,0,0,false); }
    for (uint32_t i=0;i<p.done;++i) dst[i]=p.data[i];
    return MakeResult(R1Fs::Success,p.done,p.size,true);
}
IoResult BackendCore::WriteAt(Key key,uint64_t offset,const uint8_t* src,uint32_t n) {
    if (!transport_) return MakeResult(R1Fs::NotReady,0,0,false);
    if (quarantined_) return MakeResult(R1Fs::Corrupt,0,0,false);
    if (broken_) return MakeResult(R1Fs::NotReady,0,0,false);
    if ((n && !src) || n>R1FsWire::DataBytes || offset>(uint64_t)0xffffffffu)
        return MakeResult(R1Fs::Argument,0,0,false);
    R1FsWire::Packet& p=PacketFor(R1FsWire::Write);
    p.slot=key.slot; p.generation=key.generation; p.offset=(uint32_t)offset; p.length=n;
    for (uint32_t i=0;i<n;++i) p.data[i]=src[i];
    int32_t t=transport_->Exchange(p);
    if (t) { broken_=true; return MakeResult(t,0,0,false); }
    int32_t r=p.status;
    if (R1FsWire::StorageUncertain(p)) { quarantined_=true; return MakeResult(r?r:R1Fs::Corrupt,p.done,0,false); }
    if (r) return MakeResult(r,0,0,false);
    if (!p.stable || R1FsWire::MalformedTransfer(p,n)) { broken_=true; return MakeResult(R1Fs::Corrupt,0,0,false); }
    return MakeResult(R1Fs::Success,p.done,p.size,true);
}
IoResult BackendCore::Resize(Key key,uint64_t size) {
    if (!transport_) return MakeResult(R1Fs::NotReady,0,0,false);
    if (quarantined_) return MakeResult(R1Fs::Corrupt,0,0,false);
    if (broken_) return MakeResult(R1Fs::NotReady,0,0,false);
    if (size>(uint64_t)0xffffffffu) return MakeResult(R1Fs::TooBig,0,0,false);
    R1FsWire::Packet& p=PacketFor(R1FsWire::Resize);
    p.slot=key.slot; p.generation=key.generation; p.requestedSize=(uint32_t)size;
    int32_t t=transport_->Exchange(p);
    if (t) { broken_=true; return MakeResult(t,0,0,false); }
    int32_t r=p.status;
    if (R1FsWire::StorageUncertain(p)) { quarantined_=true; return MakeResult(r?r:R1Fs::Corrupt,0,0,false); }
    if (r) return MakeResult(r,0,0,false);
    if (!p.stable) { broken_=true; return MakeResult(R1Fs::Corrupt,0,0,false); }
    return MakeResult(R1Fs::Success,0,p.size,true);
}
int32_t BackendCore::Flush(Key key) {
    if (!transport_) return R1Fs::NotReady;
    if (quarantined_) return R1Fs::Corrupt;
    if (broken_) return R1Fs::NotReady;
    R1FsWire::Packet& p=PacketFor(R1FsWire::Flush);
    p.slot=key.slot; p.generation=key.generation;
    int32_t t=transport_->Exchange(p);
    if (t) { broken_=true; return t; }
    int32_t r=p.status;
    if (R1FsWire::StorageUncertain(p)) { quarantined_=true; return r?r:R1Fs::Corrupt; }
    return r;
}
int32_t BackendCore::Close(Key key) {
    if (!transport_) return R1Fs::NotReady;
    if (broken_) return R1Fs::NotReady;
    R1FsWire::Packet& p=PacketFor(R1FsWire::Close);
    p.slot=key.slot; p.generation=key.generation;
    int32_t t=transport_->Exchange(p);
    if (t) { broken_=true; return t; }
    if (R1FsWire::StorageUncertain(p)) quarantined_=true;
    return p.status;
}
}
