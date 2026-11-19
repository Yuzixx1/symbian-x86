#ifndef R1_FS_CHANNEL_STATE_H
#define R1_FS_CHANNEL_STATE_H
#include "r1_fs_channel.h"
#include "r1_fs_backend.h"
#include "r1_fs_wire_logic.h"
#ifdef __IN_KERNEL__
typedef TUint32 R1FsChannelSize;
#else
typedef uint32_t R1FsChannelSize;
#endif

// Portable channel-side FAT dispatch and reply-cleanup orchestration. The
// kernel LDD instantiates ChannelState with R1Fat::File and a small FatOps
// policy; host tests instantiate it with a fake file and fake operations. The
// same RunRequest() template is used by the LDD and the host tests, so the
// cleanup decision it encodes is the one actually executed in the kernel.
namespace R1FsWire {

template<class FileT,class OpsT>
class ChannelState {
public:
    ChannelState() : next_(1) {
        for (unsigned i=0;i<Slots;++i) entries_[i].used=false;
    }
    // All calls require UserFilesLock. Packet outputs are initialised by the
    // caller (PrepareResponse) before Dispatch. On Open success a newly acquired
    // key is published; AbandonNewKey() closes it when the reply cannot be
    // delivered.
    int32_t Dispatch(Packet& p) {
        if (p.op==Open) {
            if (!next_) return R1Fs::NoMemory;
            unsigned slot=0;
            while (slot<Slots && entries_[slot].used) ++slot;
            if (slot==Slots) return R1Fs::NoMemory;
            FileT f;
            int32_t r=OpsT::Open(p.path[0],p.path+2,p.disposition!=0,f);
            if (r) { p.stable=0; p.quarantined=(p.disposition!=0 && r!=R1Fs::AlreadyExists)?1:0; return r; }
            R1FsChannelSize size=0; r=OpsT::Size(f,size);
            if (r) { OpsT::Close(f); p.stable=0; p.quarantined=1; return r; }
            entries_[slot].file=f; entries_[slot].used=true;
            entries_[slot].generation=next_++;
            if (!entries_[slot].generation) { // no wrap reuse
                OpsT::Close(entries_[slot].file); entries_[slot].used=false;
                return R1Fs::NoMemory;
            }
            p.slot=slot; p.generation=entries_[slot].generation;
            p.size=size; p.stable=1; return R1Fs::Success;
        }
        if (p.slot>=Slots) return R1Fs::BadHandle;
        Entry& e=entries_[p.slot];
        if (!e.used || !MatchesKey(p,p.slot,e.generation)) return R1Fs::BadHandle;
        if (p.op==Close) { OpsT::Close(e.file); e.used=false; return R1Fs::Success; }
        R1FsChannelSize size=0; int32_t r=OpsT::Size(e.file,size);
        if (r) { p.stable=0; p.quarantined=1; return r; }
        p.size=size; p.stable=1;
        if (p.op==Size || p.op==Inspect) return R1Fs::Success;
        if (p.op==Read || p.op==Write) {
            uint32_t done=0;
            if (p.op==Read) r=OpsT::Read(e.file,p.offset,p.data,p.length,done);
            else r=OpsT::Write(e.file,p.offset,p.data,p.length,done);
            p.done=done;
        } else if (p.op==Resize) r=OpsT::Resize(e.file,p.requestedSize);
        else if (p.op==Flush) r=OpsT::Flush(e.file);
        else return R1Fs::NotSupported;
        R1FsChannelSize after=0; int32_t sr=OpsT::Size(e.file,after);
        if (sr) { p.stable=0; p.quarantined=1; p.size=0; }
        else { p.size=after; p.stable=1; }
        if (r && (p.op==Write || p.op==Resize || p.op==Flush)) {
            p.stable=0; p.quarantined=1; p.size=0;
        }
        return r?r:sr;
    }
    // Close a key published by a completed Open whose reply could not be
    // delivered. Only the exact slot+generation is touched; a reused slot has a
    // different generation, so a stale reply cannot close a live key. The
    // directory entry created by Open is deliberately NOT rolled back.
    bool AbandonNewKey(const Packet& p) {
        if (p.op!=Open || p.slot>=Slots) return false;
        Entry& e=entries_[p.slot];
        if (!e.used || !MatchesKey(p,p.slot,e.generation)) return false;
        OpsT::Close(e.file); e.used=false; return true;
    }
    // Release every still-open token. Used by the kernel channel destructor.
    void CloseAll() {
        for (unsigned i=0;i<Slots;++i) if (entries_[i].used) {
            OpsT::Close(entries_[i].file); entries_[i].used=false;
        }
    }
private:
    struct Entry { FileT file; uint32_t generation; bool used; };
    Entry entries_[Slots];
    uint32_t next_;
};

// Portable orchestration of one request: authorization checks, response
// initialisation, FAT dispatch under the owner lock, post-dispatch revoke
// recheck, user copy and cleanup of a newly acquired but undelivered key.
// LockT must provide static Enter()/Leave(); AuthT/CopyT are callables.
template<class StateT,class LockT,class AuthT,class CopyT>
inline int32_t RunRequest(StateT& state,Packet& packet,unsigned expectedOp,
                          AuthT auth,CopyT copyout) {
    if (!auth()) return R1Fs::AccessDenied;
    if (!ValidRequest(packet,expectedOp)) return R1Fs::Argument;
    PrepareResponse(packet);
    bool dispatched=false, newKey=false;
    LockT::Enter();
    if (auth()) {
        packet.status=state.Dispatch(packet);
        newKey=(packet.op==Open && packet.status==R1Fs::Success);
        dispatched=true;
    }
    LockT::Leave();
    if (!dispatched) return R1Fs::AccessDenied;
    if (!auth()) {
        if (newKey) { LockT::Enter(); state.AbandonNewKey(packet); LockT::Leave(); }
        return R1Fs::AccessDenied;
    }
    int32_t r=copyout(packet);
    if (r && newKey) { LockT::Enter(); state.AbandonNewKey(packet); LockT::Leave(); }
    return r;
}
}
#endif
