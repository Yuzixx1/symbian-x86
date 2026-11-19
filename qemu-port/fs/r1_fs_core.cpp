#include "r1_fs_core.h"

namespace R1Fs {
namespace {
bool Eq(const Path& a, const Path& b) {
    if (a.length != b.length) return false;
    for (uint32_t i=0; i<a.length; ++i) if (a.text[i]!=b.text[i]) return false;
    return true;
}
bool Letter(uint16_t c) { return c>='A' && c<='Z'; }
uint16_t Upper(uint16_t c) { return c>='a' && c<='z' ? c-32 : c; }
bool Allowed(const Path& p) {
    // Allow C:\R1TEST\... (with trailing backslash) and C:\R1TEST (directory name,
    // no trailing backslash). Both are valid within the R1 test fixture volume.
    static const char prefix[] = ":\\R1TEST\\";
    static const char prefixDir[] = ":\\R1TEST";
    if (!Letter(p.text[0])) return false;
    if (p.length == 9) {
        // Exactly "C:\R1TEST" - directory itself
        for (unsigned i=0; i<8; ++i) if (p.text[1+i]!=uint16_t(prefixDir[i])) return false;
        return true;
    }
    if (p.length < 10) return false;
    for (unsigned i=0; i<9; ++i) if (p.text[1+i]!=uint16_t(prefix[i])) return false;
    return true;
}
bool NameChar(uint16_t c) {
    if (c < 0x20 || c > 0x7e) return false;
    switch (c) { case '*': case '?': case '"': case '<': case '>':
                 case '|': case ':': case '/': return false; }
    return true;
}
bool Fit(uint64_t pos, uint32_t n) { return pos<=KFileMax && n<=KFileMax-pos; }
bool AllowedWithShares(const uint32_t requested,
                       const bool write, const uint32_t* modes,
                       const bool* writers, const unsigned count) {
    bool promotedReaders=false, promotedAny=false, existingWriter=false;
    for (unsigned i=0;i<count;++i) {
        if (modes[i]==ShareExclusive || requested==ShareExclusive) return false;
        if (modes[i]==ShareReaders) promotedReaders=true;
        if (modes[i]==ShareAny) promotedAny=true;
        if (writers[i]) existingWriter=true;
    }
    if (promotedReaders) return (requested==ShareReaders || requested==ShareFlexible) && !write;
    if (promotedAny) return requested==ShareAny || requested==ShareFlexible;
    if (requested==ShareReaders && existingWriter) return false;
    return true;
}
}
int32_t Canonical(const uint16_t* text, uint32_t length, const Path& cwd,
                  bool directory, Path& out) {
    out = Path();
    if (!text || !length || length>=KPathUnits) return BadName;
    uint32_t p=0, src=0;
    if (length>=2 && text[1]==':') {
        if (!Letter(Upper(text[0])) || length<3 || text[2]!='\\') return BadName;
        out.text[p++]=Upper(text[0]); out.text[p++]=':'; out.text[p++]='\\'; src=3;
    } else {
        if (cwd.length<3 || cwd.length>=KPathUnits) return BadName;
        for (uint32_t i=0;i<cwd.length;++i) out.text[p++]=cwd.text[i];
        if (text[0]=='\\') { p=3; src=1; }
    }
    if (src==length && !directory) return BadName;
    if (src<length && text[src]=='\\' && src) return BadName;
    if (p>3 && out.text[p-1]!='\\') { if (p==KPathUnits) return BadName; out.text[p++]='\\'; }
    uint32_t component=0;
    for (uint32_t i=src;i<length;++i) {
        const uint16_t c=Upper(text[i]);
        if (c=='\\') {
            if (!component) return BadName;
            if (component==1 && out.text[p-1]=='.') return BadName;
            if (component==2 && out.text[p-2]=='.' && out.text[p-1]=='.') return BadName;
            component=0;
        } else {
            if (!NameChar(c)) return BadName;
            ++component;
        }
        if (p>=KPathUnits-1) return BadName;
        out.text[p++]=c;
    }
    if (!component && !directory) return BadName;
    if (component==1 && out.text[p-1]=='.') return BadName;
    if (component==2 && out.text[p-2]=='.' && out.text[p-1]=='.') return BadName;
    if (directory && p>3 && out.text[p-1]!='\\') out.text[p++]='\\';
    if (!directory && p>3 && out.text[p-1]=='\\') return BadName;
    out.length=p;
    return Allowed(out) ? Success : AccessDenied;
}
int32_t Decode(const Frame& f, Decoded& d) {
    d=Decoded(); d.operation=uint16_t(f.function&65535); d.flags=f.function&~65535u;
    uint32_t want=0; int desc=-1, out=0;
    switch (d.operation) {
    case SessionPath: out=0; break;
    case SetSessionPath: case FileOpen: case FileCreate: case FileReplace: desc=0; break;
    case FileRead: case FileWrite: desc=0; want=d.flags&D2; break;
    case FileSeek: want=d.flags&D0; out=2; break;
    case FileSize: out=0; break;
    case FileSetSize: want=d.flags&D0; break;
    case FileClose: case FileFlush: case FileReadCancel: break;
    default: return NotSupported;
    }
    const uint32_t allowed=(d.operation==FileRead || d.operation==FileWrite)?D2:
        d.operation==FileSeek?(D0|D2):
        (d.operation==FileSize || d.operation==FileSetSize)?D0:0;
    if ((d.flags & ~allowed)!=0 || (d.operation==FileSeek && !(d.flags&D2)) ||
        (d.operation==FileSize && !(d.flags&D0))) return NotSupported;
    if (desc>=0 && !f.descriptor[desc]) return Argument;
    if (d.operation==FileOpen || d.operation==FileCreate || d.operation==FileReplace) {
        // Do not inspect reply capacity yet: the ordinary path/mode/backend
        // error precedes the failed handle-reply copy and its FSCLIENT panic.
        d.resultLength=4;
    }
    if (d.operation==FileRead || d.operation==FileWrite) {
        if (f.a[1]<0) return Argument;
        if (d.operation==FileRead && uint32_t(f.a[1])>f.capacity[0]) return Overflow;
        if (d.operation==FileWrite && uint32_t(f.a[1])>f.length[0]) return Argument;
    }
    if (want && (!f.descriptor[d.operation==FileSeek || d.operation==FileSetSize ? 0 : 2] ||
                 f.length[d.operation==FileSeek || d.operation==FileSetSize ? 0 : 2]!=8)) return Argument;
    if (out>=0 && (d.operation==SessionPath || d.operation==FileSeek || d.operation==FileSize)) {
        if (!f.descriptor[out] || (d.operation!=SessionPath && f.capacity[out]<8)) return Overflow;
    }
    if (d.operation==SetSessionPath || d.operation==FileOpen ||
        d.operation==FileCreate || d.operation==FileReplace) {
        if (f.length[0]>=KPathUnits) return BadName;
    }
    if ((d.operation==FileRead || d.operation==FileWrite) &&
        f.a[1]>1024*1024) return NotSupported;
    d.widePosition=want!=0; d.packageLength=want?8:0;
    if (d.operation==FileSeek || d.operation==FileSize) d.resultLength=8;
    return Success;
}
Core::Core(Backend& b) : backend_(b), nextSession_(1), nextHandle_(1), drive_('C') {
    for (unsigned i=0;i<KMaxSessions;++i) sessions_[i]=Session();
    for (unsigned i=0;i<KMaxShares;++i) shares_[i]=Share();
}
int32_t Core::SetDrive(uint16_t drive) {
    if (!Letter(Upper(drive))) return Argument;
    for (unsigned i=0;i<KMaxSessions;++i) if (sessions_[i].used) return InUse;
    drive_=Upper(drive); return Success;
}
int32_t Core::ValidateSession(uint32_t token) const {
    for (unsigned i=0;i<KMaxSessions;++i)
        if (sessions_[i].used && sessions_[i].generation==token) return Success;
    return BadHandle;
}
int32_t Core::Connect(uint32_t sid, uint32_t& token) {
    token=0; unsigned i=0;
    for (;i<KMaxSessions && sessions_[i].used;++i) {}
    if (i==KMaxSessions || nextSession_==0) return NoMemory;
    Session& s=sessions_[i]; s=Session(); s.used=true;
    s.generation=nextSession_++;
    s.path.text[0]=drive_; s.path.text[1]=':'; s.path.text[2]='\\';
    s.path.length=3;
    token=s.generation; return Success;
}
int32_t Core::Disconnect(uint32_t session) {
    if (ValidateSession(session)) return BadHandle;
    for (unsigned i=0;i<KMaxShares;++i) if (shares_[i].used && shares_[i].owner==session)
        CloseShare(shares_[i]);
    for (unsigned i=0;i<KMaxSessions;++i) if (sessions_[i].generation==session) {
        sessions_[i].used=false; return Success;
    }
    return BadHandle;
}
int32_t Core::GetPath(uint32_t session, Path& path) const {
    for (unsigned i=0;i<KMaxSessions;++i) if (sessions_[i].used && sessions_[i].generation==session) {
        path=sessions_[i].path; return Success;
    }
    return BadHandle;
}
int32_t Core::SetPath(uint32_t session, const uint16_t* text, uint32_t n) {
    Path old; int32_t r=GetPath(session,old); if (r) return r;
    Path p; r=Canonical(text,n,old,true,p); if (r) return r;
    if (p.text[0]!=drive_) return AccessDenied;
    for (unsigned i=0;i<KMaxSessions;++i) if (sessions_[i].used && sessions_[i].generation==session) {
        sessions_[i].path=p; return Success;
    }
    return BadHandle;
}
Core::Share* Core::Find(uint32_t session,uint32_t h,bool pending) {
    if (ValidateSession(session)) return 0;
    for (unsigned i=0;i<KMaxShares;++i) if (shares_[i].used && shares_[i].owner==session &&
        shares_[i].generation==h && (shares_[i].published || pending)) return &shares_[i];
    return 0;
}
const Core::Share* Core::Find(uint32_t s,uint32_t h) const {
    if (ValidateSession(s)) return 0;
    for (unsigned i=0;i<KMaxShares;++i) if (shares_[i].used && shares_[i].published &&
        shares_[i].owner==s && shares_[i].generation==h) return &shares_[i];
    return 0;
}
int32_t Core::Open(uint32_t session,const uint16_t* text,uint32_t n,uint32_t mode,
                   Disposition how,uint32_t& h) {
    h=0; Path cwd; int32_t r=GetPath(session,cwd); if (r) return r;
    if (how!=OpenExisting && how!=CreateNew && how!=ReplaceExisting) return Argument;
    const uint32_t share=mode&3;
    // AsyncAll requires a retained RMessage2 queue, not an immediate short
    // read. Do not advertise it until queue/cancel state is implemented.
    if ((mode&~(uint32_t(3)|ModeWrite|ModeAsyncAll|ModeBig)) ||
        (share==ShareReaders && (mode&ModeWrite)) ||
        ((mode&ModeAsyncAll) && (share==ShareReaders || share==ShareExclusive))) return Argument;
    if (mode&ModeAsyncAll) return NotSupported;
    Path p; r=Canonical(text,n,cwd,false,p); if (r) return r;
    if (p.text[0]!=drive_) return AccessDenied;
    const bool write=(mode&ModeWrite)!=0;
    unsigned free=KMaxShares, holders=0;
    uint32_t modes[KMaxShares]; bool writers[KMaxShares];
    for (unsigned i=0;i<KMaxShares;++i) {
        if (!shares_[i].used) { if (free==KMaxShares) free=i; continue; }
        if (Eq(shares_[i].path,p)) {
            modes[holders]=shares_[i].mode&3;
            writers[holders++]=shares_[i].write;
        }
    }
    if (!AllowedWithShares(share,write,modes,writers,holders)) return InUse;
    if (free==KMaxShares || !nextHandle_) return NoMemory;
    // Replacing a live file would leave existing backend keys referencing an
    // ambiguous old/new object. Refuse until all holders have closed.
    if (how==ReplaceExisting && holders) return InUse;
    Key key=Key(); Meta meta=Meta();
    r=backend_.Open(p,how,key,meta); if (r) return r;
    if (meta.size>KFileMax || (!(mode&ModeBig) && meta.size>0x7fffffffU)) {
        backend_.Close(key); return TooBig;
    }
    Share& s=shares_[free]; s=Share(); s.used=true; s.owner=session;
    s.generation=nextHandle_++; s.mode=mode; s.write=write; s.big=(mode&ModeBig)!=0;
    s.path=p; s.key=key; s.size=meta.size; h=s.generation;
    return Success;
}
int32_t Core::Commit(uint32_t session,uint32_t h) {
    Share* s=Find(session,h,true); if (!s || s->published) return BadHandle;
    s->published=true; return Success;
}
int32_t Core::CheckShare(uint32_t session,uint32_t h) const {
    return Find(session,h)?Success:BadHandle;
}
int32_t Core::CloseShare(Share& s) {
    int32_t r=backend_.Close(s.key); s.used=false; s.published=false; return r;
}
int32_t Core::Abort(uint32_t session,uint32_t h) {
    Share* s=Find(session,h,true); if (!s || s->published) return BadHandle;
    return CloseShare(*s);
}
int32_t Core::Close(uint32_t session,uint32_t h) {
    Share* s=Find(session,h); return s?CloseShare(*s):BadHandle;
}
int32_t Core::Size(uint32_t session,uint32_t h,uint64_t& size) {
    Share* s=Find(session,h); if (!s) return BadHandle;
    size=s->size; return s->poisoned?Corrupt:Success;
}
int32_t Core::Flush(uint32_t session,uint32_t h) {
    Share* s=Find(session,h); if (!s) return BadHandle;
    return s->poisoned?Corrupt:backend_.Flush(s->key);
}
int32_t Core::Writable(uint32_t session,uint32_t h) const {
    const Share* s=Find(session,h);
    return !s?BadHandle:(s->write?Success:AccessDenied);
}
int32_t Core::Readable(uint32_t session,uint32_t h) const {
    return Find(session,h)?Success:BadHandle;
}
int32_t Core::Resize(uint32_t session,uint32_t h,int64_t size) {
    Share* s=Find(session,h); if (!s) return BadHandle;
    if (!s->write) return AccessDenied;
    if (size<0) return Argument; // EUSER bridge issues FSCLIENT/20 after this check
    if ((!s->big && uint64_t(size)>0x7fffffffU) || uint64_t(size)>KFileMax) return TooBig;
    if (s->poisoned) return Corrupt;
    IoResult r=backend_.Resize(s->key,uint64_t(size));
    UpdateSize(s->path,r.stable?r.size:s->size,r.stable && r.size<=KFileMax &&
               (r.error!=Success || r.size==uint64_t(size)));
    return s->poisoned?Corrupt:r.error;
}
int32_t Core::Read(uint32_t session,uint32_t h,uint64_t position,bool current,
                   uint8_t* data,uint32_t count,uint32_t& actual) {
    uint64_t next=0;
    int32_t r=StageRead(session,h,position,current,data,count,actual,next);
    if (r) return r;
    return ConfirmRead(session,h,next);
}
int32_t Core::StageRead(uint32_t session,uint32_t h,uint64_t position,bool current,
                        uint8_t* data,uint32_t count,uint32_t& actual,uint64_t& next) {
    actual=0; Share* s=Find(session,h); if (!s) return BadHandle;
    if (s->poisoned) return Corrupt;
    if (count>KTransferBytes || (!data && count)) return Argument;
    if (!count) { next=s->position; return Success; }
    uint64_t pos=current?s->position:position;
    if (pos>s->size) pos=s->size;
    if (s->size-pos<count) count=uint32_t(s->size-pos);
    if (!count) { next=pos; return Success; }
    IoResult r=backend_.ReadAt(s->key,pos,data,count);
    if (r.committed>count || !r.stable ||
        (r.stable && (r.size>KFileMax || r.size<pos+r.committed))) s->poisoned=true;
    UpdateSize(s->path,s->poisoned?s->size:r.size,!s->poisoned);
    if (s->poisoned) return Corrupt;
    if (r.error) return r.error; // never copy partial read on failure
    if (!r.committed && pos<s->size) return Corrupt;
    actual=r.committed; next=pos+actual; return Success;
}
int32_t Core::ConfirmRead(uint32_t session,uint32_t h,uint64_t next) {
    Share* s=Find(session,h); if (!s) return BadHandle;
    if (s->poisoned || next>KFileMax) return Corrupt;
    s->position=next; return Success;
}
int32_t Core::Write(uint32_t session,uint32_t h,uint64_t position,bool current,
                    const uint8_t* data,uint32_t count,uint32_t& committed) {
    committed=0; Share* s=Find(session,h); if (!s) return BadHandle;
    if (!s->write) return AccessDenied;
    if (s->poisoned) return Corrupt;
    if (!s->big && s->size>0x7fffffffU) return TooBig;
    if (count>KTransferBytes || (!data && count)) return Argument;
    if (!count) return Success;
    uint64_t pos=current?s->position:position; if (pos>s->size) pos=s->size;
    if (!Fit(pos,count)) return TooBig;
    if (!s->big && pos+count>0x7fffffffU) return TooBig;
    IoResult r=backend_.WriteAt(s->key,pos,data,count);
    if (r.committed>count || !r.stable ||
        (r.stable && (r.size>KFileMax || r.size<pos+r.committed)) ||
        (r.error==Success && r.committed!=count)) s->poisoned=true;
    UpdateSize(s->path,s->poisoned?s->size:r.size,!s->poisoned);
    committed=r.committed;
    if (r.committed && !s->poisoned) s->position=pos+r.committed;
    return s->poisoned?Corrupt:r.error;
}
int32_t Core::Seek(uint32_t session,uint32_t h,int64_t offset,uint32_t mode,uint64_t& result) {
    Share* s=Find(session,h); if (!s) return BadHandle;
    if (s->poisoned) return Corrupt;
    if (mode==0 || mode>3) return NotSupported; // ESeekAddress unsupported
    if (mode==1) { if (offset<0 || uint64_t(offset)>KFileMax) return Argument;
        if (!s->big && uint64_t(offset)>0x7fffffffU) return TooBig;
        s->position=uint64_t(offset); result=s->position; return Success; }
    const uint64_t base=mode==2?s->position:s->size;
    uint64_t pos;
    if (offset<0) { uint64_t mag=uint64_t(-(offset+1))+1; pos=mag>base?0:base-mag; }
    else { pos=uint64_t(offset)>KFileMax-base?uint64_t(KFileMax):base+uint64_t(offset); }
    if (pos>s->size) pos=s->size;
    if (!s->big && pos>0x7fffffffU) return TooBig;
    s->position=result=pos; return Success;
}
void Core::UpdateSize(const Path& path, uint64_t size, bool stable) {
    for (unsigned i=0;i<KMaxShares;++i) if (shares_[i].used && Eq(shares_[i].path,path)) {
        shares_[i].size=size;
        if (!stable) shares_[i].poisoned=true;
    }
}
}
