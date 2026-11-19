#include "e32g_plan.h"

namespace R1G {
namespace {
uint32_t Word(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
Result FormatResult(E32G::Status s) {
    return s==E32G::Ok ? Success : s==E32G::Unsupported ? NotSupported : CorruptImage;
}
void ResetDiagnostic(Diagnostic& d) {
    Diagnostic z = {}; d=z;
}
bool EqualName(const Path& a, const Path& b) {
    if (a.units!=b.units) return false;
    for (uint32_t i=0;i<a.units;++i) {
        uint16_t x=a.text[i], y=b.text[i];
        if (x>='A' && x<='Z') x+=32;
        if (y>='A' && y<='Z') y+=32;
        if (x!=y) return false;
    }
    return true;
}
bool VersionMatches(uint32_t actual, uint32_t requested) {
    return requested>=0xfffffffeu ||
        ((actual>>16)==(requested>>16) && (actual&65535)>=(requested&65535));
}
Result NameIdentity(const Path& path, const E32G::Image& im,
                    const uint8_t* wanted, uint32_t wantedSize) {
    uint32_t start=0;
    for(uint32_t i=0;i<path.units;++i)
        if(path.text[i]=='\\' || path.text[i]=='/') start=i+1;
    if(path.units-start>80 || path.units==start)return NotSupported;
    uint8_t leaf[80];
    for(uint32_t i=start;i<path.units;++i) {
        if(path.text[i]>127)return NotSupported;
        leaf[i-start]=uint8_t(path.text[i]);
    }
    E32G::Bytes name={leaf,path.units-start};E32G::Identity id;E32G::Error e;
    if(E32G::ParseIdentity(name,id,e)!=E32G::Ok)return CorruptImage;
    if((id.hasUid && id.uid3!=im.uids[2]) ||
       (id.hasVersion && !VersionMatches(im.moduleVersion,id.version)))return NotFound;
    if(wanted) {
        if(id.base.size+id.extension.size!=wantedSize)return NotFound;
        for(uint32_t i=0;i<wantedSize;++i) {
            uint8_t x=leaf[i<id.base.size ? id.base.offset+i : id.extension.offset+i-id.base.size];
            uint8_t y=wanted[i];
            if(x>='A' && x<='Z')x+=32;
            if(y>='A' && y<='Z')y+=32;
            if(x!=y)return NotFound;
        }
    }
    return Success;
}
Result Match(const E32G::Image& im, const ImageId& id, const Security& sec,
             uint32_t sid) {
    if ((id.uid1 && id.uid1!=im.uids[0]) || (id.uid2 && id.uid2!=im.uids[1]) ||
        (id.uid3 && id.uid3!=im.uids[2])) return NotFound;
    if (!VersionMatches(im.moduleVersion,id.version)) return NotSupported;
    if ((sid && sid!=im.secureId) || (sec.caps[0]&~im.capabilities[0]) ||
        (sec.caps[1]&~im.capabilities[1])) return PermissionDenied;
    return Success;
}
Result Required(const E32G::Image& exporter, const E32G::Image& importer,
                const E32G::ImportView& v) {
    E32G::Error e;
    for (uint32_t i=0;i<v.count;++i) {
        uint32_t ordinal=Word(importer.file.data+v.iat.offset+i*4);
        E32G::ExportView x;
        if (!ordinal || E32G::ExportAt(exporter,ordinal,x,e)!=E32G::Ok ||
            x.absent || !x.raw || x.raw==exporter.entryCodeOffset) return NotSupported;
        const uint32_t data=x.raw-exporter.code.size;
        if (x.raw>=exporter.code.size &&
            (data>=exporter.data.size && data-exporter.data.size>=exporter.bssSize))
            return NotSupported;
    }
    return Success;
}
Result Open(Plan& p, ImageName name, Node& n, Diagnostic& d) {
    Node z={}; n=z;
    if (!name.text || !name.units || name.units>MaxNameUnits) return Argument;
    // Source owns authorization/sysbin policy. Core only retains exact name.
    n.path.units=name.units;
    for (uint32_t i=0;i<name.units;++i) n.path.text[i]=name.text[i];
    Result r=p.source->OpenSnapshot(name,n.snapshot);
    if (r) return r;
    uint32_t size=0; E32G::Bytes bytes; bytes.data=0; bytes.size=0;
    r=p.source->Size(n.snapshot,size);
    if (!r && (size>p.limits.imageBytes || size>p.limits.aggregateBytes-p.byteCount)) r=TooBig;
    if (!r) r=p.source->Bytes(n.snapshot,bytes);
    if (!r && (!bytes.data || bytes.size!=size)) r=CorruptImage;
    if (!r) r=FormatResult(E32G::Parse(bytes,n.image,d.format));
    if (r) { p.source->Close(n.snapshot); n=z; }
    return r;
}
// Bounded nonrecursive DFS. One active node is expanded per iteration; state
// 1 is discovered, 2 active, 3 complete. No giant automatic recursion stack.
Result Expand(Plan& p, Diagnostic& d) {
    while (p.orderCount<p.nodeCount) {
        uint32_t current=0;
        bool active=false;
        // Last active discovered node is the deepest unfinished child.
        for (uint32_t i=0;i<p.nodeCount;++i)
            if (p.nodes[i].state==2) { current=i; active=true; }
        if (!active) return CorruptImage;
        Node& n=p.nodes[current]; d.node=current;
        bool descend=false;
        for (uint32_t k=0;k<n.edgeCount;++k) {
            Node& child=p.nodes[p.edges[n.firstEdge+k].exporter];
            if (child.state==2) return NotSupported;
            if (child.state==1) { child.state=2; descend=true; break; }
        }
        if (descend) continue;
        if (n.edgeCount<n.image.dependencyCount) {
            const uint32_t index=n.edgeCount;
            E32G::ImportView iv; E32G::Error fe;
            if (E32G::ImportAt(n.image,index,iv,fe)!=E32G::Ok) return CorruptImage;
            // Edges for a node are reserved contiguously when discovered.
            const uint32_t slot=n.firstEdge+index;
            ImageId id={0x10000079u,0,iv.identity.uid3,
                iv.identity.hasVersion ? iv.identity.version : 0xffffffffu};
            Security sec=ImageSecurity(n.image);
            uint8_t root[80]; uint32_t len=0;
            const uint8_t* name=n.image.file.data+iv.name.offset;
            for (uint32_t i=0;i<iv.identity.base.size;++i) root[len++]=name[iv.identity.base.offset+i];
            for (uint32_t i=0;i<iv.identity.extension.size;++i) root[len++]=name[iv.identity.extension.offset+i];
            Node best={}; bool have=false; Result why=NotFound;
            for (uint32_t c=0;;++c) {
                Path path={}; E32G::Bytes rootBytes={root,len};
                Result r=p.source->Candidate(rootBytes,c,path);
                if (r==NotFound) break;
                if (r) { if(have) p.source->Close(best.snapshot); return r; }
                if(c==p.limits.candidates) {
                    if(have)p.source->Close(best.snapshot);
                    return TooBig;
                }
                if (path.units>MaxNameUnits || !path.units) {
                    if(have) p.source->Close(best.snapshot);
                    return CorruptImage;
                }
                Node candidate={}; ImageName absolute={path.text,path.units};
                r=Open(p,absolute,candidate,d);
                if (r) { if(have) p.source->Close(best.snapshot); return r; }
                r=NameIdentity(candidate.path,candidate.image,root,len);
                if(!r)r=Match(candidate.image,id,sec,0);
                if (!r) r=Required(candidate.image,n.image,iv);
                if (r) { why=r; p.source->Close(candidate.snapshot); continue; }
                // Highest compatible module version. Source order resolves ties.
                if (!have || candidate.image.moduleVersion>best.image.moduleVersion) {
                    if(have) p.source->Close(best.snapshot);
                    best=candidate; have=true;
                } else p.source->Close(candidate.snapshot);
            }
            if (!have) return why;
            uint32_t target=p.nodeCount;
            for (uint32_t i=0;i<p.nodeCount;++i) if (EqualName(p.nodes[i].path,best.path)) {
                // Immutable namespace contract; inconsistent bytes fail closed.
                const E32G::Bytes a=p.nodes[i].image.file, b=best.image.file;
                bool same=a.size==b.size;
                for (uint32_t j=0;same && j<a.size;++j) if(a.data[j]!=b.data[j]) same=false;
                if (!same) {p.source->Close(best.snapshot);return CorruptImage;}
                target=i; break;
            }
            if (target<p.nodeCount) p.source->Close(best.snapshot);
            else {
                if (p.nodeCount==p.nodeCapacity || p.nodeCount==p.orderCapacity ||
                    best.image.dependencyCount>p.edgeCapacity-p.edgeCount) {
                    p.source->Close(best.snapshot); return TooBig;
                }
                best.firstEdge=p.edgeCount; best.edgeCount=0; best.state=1;
                p.edgeCount+=best.image.dependencyCount;
                p.byteCount+=best.image.file.size;
                p.nodes[p.nodeCount++]=best;
            }
            Edge edge={current,target,index}; p.edges[slot]=edge; ++n.edgeCount;
            if (p.nodes[target].state==2) return NotSupported;
            if (p.nodes[target].state==1) p.nodes[target].state=2;
            continue;
        }
        n.state=3; p.order[p.orderCount++]=current;
    }
    return Success;
}
} // anonymous

Security ImageSecurity(const E32G::Image& im) {
    Security s={im.secureId,im.vendorId,{im.capabilities[0],im.capabilities[1]}}; return s;
}
void InitPlan(Plan& p, Node* n,uint32_t nc,Edge* e,uint32_t ec,uint32_t* o,uint32_t oc) {
    Plan z={}; p=z; p.nodes=n;p.nodeCapacity=nc;p.edges=e;p.edgeCapacity=ec;
    p.order=o;p.orderCapacity=oc;p.state=EmptyPlan;
}
void ReleasePlan(Plan& p) {
    if (p.source) for(uint32_t i=p.nodeCount;i>0;--i) p.source->Close(p.nodes[i-1].snapshot);
    p.source=0;p.nodeCount=0;p.edgeCount=0;p.orderCount=0;p.byteCount=0;p.state=EmptyPlan;
}
Result BuildPlan(Plan& p,ImageSource& source,const LoadRequest& req,const Limits& lim,Diagnostic& d) {
    ResetDiagnostic(d);
    if (p.state!=EmptyPlan || !p.nodes || !p.edges || !p.order || !p.nodeCapacity ||
        !p.orderCapacity || !lim.imageBytes || !lim.aggregateBytes || !lim.candidates)
        return d.result=Argument;
    p.source=&source;p.request=req;p.limits=lim;p.state=BuildingPlan;
    Node root={}; Result r=Open(p,req.name,root,d);
    if (!r) {
        Security caps={0,0,{0,0}};
        if(req.kind==LibraryLoad) caps=req.clientSecurity;
        r=NameIdentity(root.path,root.image,0,0);
        if(!r)r=Match(root.image,req.requested,caps,req.requestedSecureId);
        if (!r && (root.image.dll != (req.kind==LibraryLoad))) r=NotSupported;
        if (!r && root.image.dependencyCount>p.edgeCapacity) r=TooBig;
        if (r) source.Close(root.snapshot);
        else {
            root.state=2;root.firstEdge=0;root.edgeCount=0;
            p.nodes[0]=root;p.nodeCount=1;p.byteCount=root.image.file.size;
            p.edgeCount=root.image.dependencyCount;
            r=Expand(p,d);
        }
    }
    if(r) ReleasePlan(p); else p.state=ReadyPlan;
    d.result=r;return r;
}
} // namespace R1G
