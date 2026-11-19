#include "r1_fs_server.h"
#include <f32ver.h>
namespace R1Fs {
namespace { const TInt KAlreadyPanicked=1; }
class FileSession : public CSession2 {
public:
    explicit FileSession(Core& state) : state_(state), token_(0) {}
    ~FileSession() { if (token_) state_.Disconnect(token_); }
    void InitL(TUint32 sid) {
        uint32_t token=0; User::LeaveIfError(state_.Connect(sid,token));
        token_=TUint32(token);
    }
private:
    void ServiceL(const RMessage2& msg);
    void ServiceError(const RMessage2& msg,TInt error) {
        if (!msg.IsNull()) msg.Complete(error);
    }
    TInt Process(const RMessage2& msg,const Decoded& decoded);
    Core& state_; TUint32 token_;
};
FileServer::FileServer(Backend& backend)
    : CServer2(EPriorityNormal,EGlobalSharableSessions), core_(backend) {}
FileServer* FileServer::NewL(Backend& backend) { return new(ELeave) FileServer(backend); }
TInt FileServer::Publish() { _LIT(KName,"!FileServer"); return Start(KName); }
CSession2* FileServer::NewSessionL(const TVersion& version,const RMessage2& msg) const {
    const TVersion own(KF32MajorVersionNumber,KF32MinorVersionNumber,KF32BuildVersionNumber);
    if (!User::QueryVersionSupported(own,version)) User::Leave(KErrNotSupported);
    FileSession* session=new(ELeave) FileSession(const_cast<Core&>(core_));
    CleanupStack::PushL(session);
    session->InitL(msg.SecureId().iId);
    CleanupStack::Pop(session); return session;
}
void FileSession::ServiceL(const RMessage2& msg) {
    // RMessage2 stays on server owner thread. Descriptor flags are not TIpcArgs
    // slot-type metadata: interrogate kernel descriptor lengths and copy APIs.
    Frame frame=Frame(); frame.function=TUint32(msg.Function());
    frame.a[0]=msg.Int0(); frame.a[1]=msg.Int1();
    frame.a[2]=msg.Int2(); frame.a[3]=msg.Int3();
    const TUint32 op=frame.function&0xffff;
    const TInt slots[4]={
        (op==SetSessionPath || op==FileOpen || op==FileCreate || op==FileReplace ||
         op==FileRead || op==FileWrite || op==SessionPath || op==FileSize ||
         ((op==FileSeek || op==FileSetSize) && (frame.function&D0)))?1:0,
        0,
        ((op==FileRead || op==FileWrite) && (frame.function&D2)) || op==FileSeek,
        op==FileOpen || op==FileCreate || op==FileReplace
    };
    TInt r=KErrNone;
    for (TInt i=0;i<4;++i) if (slots[i]) {
        const TInt len=msg.GetDesLength(i);
        if (len<0) { r=len; break; }
        const TInt cap=msg.GetDesMaxLength(i);
        if (cap<0) { r=cap; break; }
        frame.descriptor[i]=1; frame.length[i]=TUint32(len);
        frame.capacity[i]=TUint32(cap);
    }
    if (r) { msg.Complete(r); return; }
    Decoded d; r=Decode(frame,d);
    if (!r) r=Process(msg,d);
    if (r==KAlreadyPanicked) return;
    if (r==BadHandle) { _LIT(KBad,"Panic"); msg.Panic(KBad,-8); return; }
    msg.Complete(r);
}
TInt FileSession::Process(const RMessage2& msg,const Decoded& d) {
    const TInt op=d.operation;
    if (op==SessionPath) {
        Path path; TInt r=state_.GetPath(token_,path); if (r) return r;
        if (msg.GetDesMaxLength(0)<TInt(path.length)) return KErrOverflow;
        TPtrC16 text(path.text,TInt(path.length)); return msg.Write(0,text);
    }
    if (op==SetSessionPath || op==FileOpen || op==FileCreate || op==FileReplace) {
        const TInt n=msg.GetDesLength(0);
        if (n<0 || n>=KPathUnits) return KErrBadName;
        TBuf<KPathUnits> name; TInt r=msg.Read(0,name); if (r) return r;
        if (name.Length()!=n) return KErrBadDescriptor;
        if (op==SetSessionPath) return state_.SetPath(token_,name.Ptr(),TUint32(n));
        uint32_t h=0;
        const Disposition how=op==FileOpen?OpenExisting:(op==FileCreate?CreateNew:ReplaceExisting);
        r=state_.Open(token_,name.Ptr(),TUint32(n),TUint32(msg.Int1()),how,h);
        if (r) return r;
        TInt reply=TInt(h); TPckgC<TInt> package(reply);
        r=msg.Write(3,package);
        if (r) {
            state_.Abort(token_,h);
            if (r==KErrOverflow) { _LIT(KClient,"FSCLIENT");
                msg.Panic(KClient,KErrOverflow); return KAlreadyPanicked; }
            return r;
        }
        return state_.Commit(token_,h);
    }
    const uint32_t h=TUint32(msg.Int3());
    if (op==FileClose) return state_.Close(token_,h); // EUSER void Close ignores error
    if (op==FileFlush) return state_.Flush(token_,h);
    if (op==FileReadCancel) {
        TInt r=state_.CheckShare(token_,h);
        return r?r:KErrNotSupported; // no queued messages
    }
    if (op==FileSize) {
        uint64_t n=0; TInt r=state_.Size(token_,h,n); if (r) return r;
        TInt64 sized=TInt64(n); TPckgC<TInt64> out(sized);
        return msg.Write(0,out);
    }
    if (op==FileSetSize) {
        TInt64 n=msg.Int0();
        if (d.widePosition) { TPckg<TInt64> p(n); TInt r=msg.Read(0,p); if (r) return r; }
        // Negative size on a valid writable share: kernel-side port panic
        // requires access validation before invoking it (Core::Resize).
        TInt r=state_.Writable(token_,h);
        if (r) return r;
        if (n<0) { _LIT(KClient,"FSCLIENT"); msg.Panic(KClient,20); return KAlreadyPanicked; }
        r=state_.Resize(token_,h,n);
        return r;
    }
    if (op==FileSeek) {
        TInt64 n=msg.Int0();
        if (d.widePosition) { TPckg<TInt64> p(n); TInt r=msg.Read(0,p); if (r) return r; }
        uint64_t result=0; TInt r=state_.Seek(token_,h,n,TUint32(msg.Int1()),result);
        if (r) return r;
        TInt64 pos=TInt64(result); TPckgC<TInt64> out(pos); return msg.Write(2,out);
    }
    if (op==FileRead || op==FileWrite) {
        TInt rights=op==FileWrite?state_.Writable(token_,h):state_.Readable(token_,h);
        if (rights) return rights;
        const TInt requested=msg.Int1();
        if (requested<0) return KErrArgument;
        TInt64 pos=d.widePosition?0:TInt64(TUint32(msg.Int2()));
        if (d.widePosition) { TPckg<TInt64> p(pos); TInt r=msg.Read(2,p); if (r) return r; }
        bool current=pos==TInt64(0xffffffffULL) || pos==-1;
        if (pos<0 && !current) return KErrArgument;
        // Bounded scratch. A request is staged in 4KiB chunks; do not turn a
        // short backend write into a successful full RFile::Write.
        if (requested>1024*1024) return KErrNotSupported;
        TBuf8<KTransferBytes> bytes;
        uint32_t total=0; TInt r=KErrNone;
        if (op==FileWrite) {
            do {
                const TInt chunk=requested-TInt(total)>KTransferBytes?
                    KTransferBytes:requested-TInt(total);
                bytes.Zero();
                if (chunk) { r=msg.Read(0,bytes,TInt(total)); if (r) return r;
                    if (bytes.Length()<chunk) return KErrArgument; }
                uint32_t done=0;
                // The first positioned chunk sets this share's position;
                // subsequent chunks are current-position writes.
                r=state_.Write(token_,h,uint64_t(pos),current,bytes.Ptr(),
                               uint32_t(chunk),done);
                if (r) return r;
                if (done!=uint32_t(chunk)) return KErrCorrupt;
                total+=done; current=true;
            } while (total<uint32_t(requested));
            return KErrNone;
        }
        do {
            const TInt chunk=requested-TInt(total)>KTransferBytes?
                KTransferBytes:requested-TInt(total);
            bytes.SetLength(chunk);
            uint32_t done=0; uint64_t next=0;
            r=state_.StageRead(token_,h,uint64_t(pos),current,
                               const_cast<TUint8*>(bytes.Ptr()),
                               uint32_t(chunk),done,next);
            if (r) return r;
            bytes.SetLength(TInt(done)); r=msg.Write(0,bytes,TInt(total));
            if (r) return r;
            r=state_.ConfirmRead(token_,h,next); if (r) return r;
            total+=done; current=true;
            if (done<uint32_t(chunk)) break; // EOF / short read
        } while (total<uint32_t(requested));
        return KErrNone;
    }
    return KErrNotSupported;
}
}
