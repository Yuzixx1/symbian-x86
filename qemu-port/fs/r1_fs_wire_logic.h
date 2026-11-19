#ifndef R1_FS_WIRE_LOGIC_H
#define R1_FS_WIRE_LOGIC_H
#include "r1_fs_channel.h"
namespace R1FsWire {
inline bool ValidPath(const char* p) {
    if (!p || p[0]<'A' || p[0]>'Z' || p[1]!=':' || p[2]!='\\') return false;
    const char prefix[]="\\R1TEST\\";
    for (unsigned i=0;i<8;++i) if (p[2+i]!=prefix[i]) return false;
    unsigned base=0,ext=0,n=10; bool dot=false;
    for (;n<PathBytes && p[n];++n) {
        char c=p[n];
        if (c=='\\') {
            if (!base || (dot && !ext)) return false;
            base=ext=0; dot=false; continue;
        }
        if (c=='.') {
            if (!base || dot) return false;
            dot=true; continue;
        }
        if (!(c>='A'&&c<='Z') && !(c>='0'&&c<='9') && c!='_') return false;
        if (dot) { if (++ext>3) return false; }
        else if (++base>8) return false;
    }
    return n<PathBytes && base && (!dot || ext);
}
inline bool ValidRequest(const Packet& p,unsigned op) {
    if(p.version!=Version || p.op!=op || op>Inspect || p.length>DataBytes) return false;
    if(op==Open) return p.disposition<=1 && ValidPath(p.path);
    return p.slot<Slots && p.generation!=0;
}
inline bool MatchesKey(const Packet& p,unsigned slot,unsigned generation) {
    return slot<Slots && generation && p.slot==slot && p.generation==generation;
}
inline bool AuthorizedProcess(const void* pinned,const void* requesting) {
    return pinned && requesting && pinned==requesting;
}
inline void PrepareResponse(Packet& p) {
    p.status=R1Fs::Success; p.done=0; p.size=0; p.stable=0; p.quarantined=0;
    if (p.op==Open) { p.slot=0; p.generation=0; }
}
inline bool StorageUncertain(const Packet& p) { return p.quarantined!=0; }
inline bool MalformedSuccess(const Packet& p) { return p.status==R1Fs::Success && !p.stable; }
inline bool MalformedTransfer(const Packet& p,unsigned requested) {
    return (p.op==Read || p.op==Write) && p.done>requested;
}
inline bool Uncertain(const Packet& p,int status,unsigned operation,unsigned requested) {
    if(p.quarantined || !p.stable) return true;
    if((operation==Write || operation==Resize || operation==Flush) && status) return true;
    return (operation==Read || operation==Write) && p.done>requested;
}
}
#endif
