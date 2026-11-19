#include "e32g_load.h"
namespace R1G {
namespace {
uint32_t R32(const uint8_t* p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
uint16_t R16(const uint8_t* p) {return uint16_t(p[0])|uint16_t(p[1])<<8;}
bool Add(uint32_t a,uint32_t b,uint32_t& o){o=a+b;return o>=a;}
bool Overlap(uint32_t a,uint32_t as,uint32_t b,uint32_t bs){return as && bs && a<b+bs && b<a+as;}
Result Preflight(const Plan& p) {
    for(uint32_t n=0;n<p.nodeCount;++n) {
        const E32G::Image& im=p.nodes[n].image; const uint8_t* code=im.file.data+im.code.offset;
        const uint32_t ep=im.entryCodeOffset;
        if(code[ep]!=0xeb || code[ep+1]!=0x0e)return NotSupported;
        for(uint32_t i=ep+2;i<ep+16;++i)if(code[i])return CorruptImage;
        const E32G::RelocView views[2]={im.codeRelocs,im.dataRelocs};
        for(unsigned v=0;v<2;++v) {
            uint32_t c=0,page=0; E32G::Range block; E32G::Error e;
            do {
                if(E32G::RelocBlockAt(im,views[v],c,block,page,e)!=E32G::Ok)return CorruptImage;
                for(uint32_t q=8;q<block.size;q+=2) {
                    uint16_t entry=R16(im.file.data+block.offset+q);if(!entry)continue;
                    uint32_t off=page+(entry&4095);
                    if(v==0 && (Overlap(off,4,ep,16) || Overlap(off,4,im.textSize,im.importCount*4) ||
                        (im.exportCount && Overlap(off,4,im.exports.offset-im.code.offset,im.exports.size))))return CorruptImage;
                    uint32_t c2=0,p2=0;E32G::Range b2;
                    do {
                        if(E32G::RelocBlockAt(im,views[v],c2,b2,p2,e)!=E32G::Ok)return CorruptImage;
                        for(uint32_t z=8;z<b2.size;z+=2) {
                            uint32_t addr=b2.offset+z;if(addr>=block.offset+q)break;
                            uint16_t prev=R16(im.file.data+addr);
                            if(prev && Overlap(off,4,p2+(prev&4095),4))return CorruptImage;
                        }
                    }while(b2.size && b2.offset<block.offset);
                }
            }while(block.size);
        }
        if(Overlap(im.textSize,im.importCount*4,ep,16) ||
           (im.exportCount && (Overlap(im.exports.offset-im.code.offset,im.exports.size,ep,16) ||
            Overlap(im.exports.offset-im.code.offset,im.exports.size,im.textSize,im.importCount*4))))return CorruptImage;
    }
    return Success;
}
Result Relocate(const E32G::Image& im,const E32G::RelocView& view,Region region,const ObjectRecord& ob,LoaderObjects& a) {
    uint32_t c=0,page=0;E32G::Range block;E32G::Error e;
    do {
        if(E32G::RelocBlockAt(im,view,c,block,page,e)!=E32G::Ok)return CorruptImage;
        for(uint32_t q=8;q<block.size;q+=2) {
            uint16_t entry=R16(im.file.data+block.offset+q);if(!entry)continue;
            uint32_t off=page+(entry&4095),value=0;Result r=a.ReadWord(ob.code,region,off,value);if(r)return r;
            unsigned type=entry&0xf000;
            bool code=type==0x1000 || (type==0x3000 && uint32_t(value-im.codeLinkBase)<im.code.size);
            value+=code ? ob.mapping.codeRun-im.codeLinkBase : ob.mapping.dataRun-im.dataLinkBase;
            r=a.WriteWord(ob.code,region,off,value);if(r)return r;
        }
    }while(block.size);
    return Success;
}
Result ExportAddress(const E32G::Image& im,const Mapping& m,uint32_t raw,uint32_t& out) {
    if(raw<im.code.size)return Add(m.codeRun,raw,out)?Success:CorruptImage;
    uint32_t data=raw-im.code.size;
    if(data>=im.data.size && data-im.data.size>=im.bssSize)return NotSupported;
    return Add(m.dataRun,data,out)?Success:CorruptImage;
}
Result MappingValid(const E32G::Image& im,const Mapping& m) {
    uint32_t end=0,entry=0,exp=0;
    if(!m.codeLoad || !m.codeRun || !Add(m.codeLoad,im.code.size,end) || !Add(m.codeRun,im.code.size,end) ||
       !Add(m.codeRun,im.entryCodeOffset,entry) || entry!=m.entryRun)return CorruptImage;
    if(im.exportCount && (!Add(m.codeRun,im.exports.offset-im.code.offset,exp) || exp!=m.exportsRun))return CorruptImage;
    if(im.data.size && (!m.dataLoad || !Add(m.dataLoad,im.data.size,end)))return CorruptImage;
    if((im.data.size || im.bssSize) && (!m.dataRun || !Add(m.dataRun,im.data.size,end) || !Add(end,im.bssSize,end)))return CorruptImage;
    return Success;
}
} // anonymous
void InitLedger(Ledger& l,ObjectRecord* records,uint32_t capacity) {
    Ledger z={};l=z;l.records=records;l.capacity=capacity;l.state=IdleTransaction;
}
void Abort(Ledger& l,LoaderObjects& a) {
    if(l.state!=ActiveTransaction)return;
    if(l.count && l.records[0].acquired && l.records[0].rootProcess)a.AbortProcess(l.records[0].process);
    for(uint32_t i=l.count;i>0;--i)if(l.records[i-1].acquired){a.CloseCode(l.records[i-1].code);l.records[i-1].acquired=false;}
    l.count=0;l.state=AbortedTransaction;
}
Result Materialise(Plan& p,LoaderObjects& a,Ledger& l,LoadResult& out,Diagnostic& d) {
    LoadResult zero={};out=zero;Diagnostic dz={};d=dz;
    if(p.state!=ReadyPlan || l.state!=IdleTransaction || !l.records || l.capacity<p.nodeCount) {
        if(p.state==ReadyPlan)ReleasePlan(p);
        return d.result=Argument;
    }
    l.state=ActiveTransaction;Result r=Preflight(p);
    for(uint32_t i=0;!r && i<p.nodeCount;++i) {
        d.node=i;const Node& n=p.nodes[i];const E32G::Image& im=n.image;
        CreateSpec spec;spec.image=&im;spec.canonicalName.text=n.path.text;spec.canonicalName.units=n.path.units;
        spec.security=ImageSecurity(im);spec.dependencies=n.edgeCount;spec.minStack=p.request.minStack;spec.client=p.request.client;
        ObjectRecord rec={};
        r=(i==0 && p.request.kind==ProcessLoad)?a.CreateProcess(spec,rec):a.CreateCode(spec,rec);if(r)break;
        rec.acquired=true;rec.rootProcess=(i==0 && p.request.kind==ProcessLoad);l.records[i]=rec;++l.count;
        r=MappingValid(im,rec.mapping);if(r)break;
        E32G::Bytes bytes={im.file.data+im.code.offset,im.code.size};r=a.Write(rec.code,CodeRegion,0,bytes);if(r)break;
        if(im.data.size){bytes.data=im.file.data+im.data.offset;bytes.size=im.data.size;r=a.Write(rec.code,DataTemplateRegion,0,bytes);if(r)break;}
        r=Relocate(im,im.codeRelocs,CodeRegion,rec,a);if(r)break;
        r=Relocate(im,im.dataRelocs,DataTemplateRegion,rec,a);if(r)break;
        for(uint32_t o=1;!r && o<=im.exportCount;++o) {
            E32G::ExportView x;E32G::Error e;
            if(E32G::ExportAt(im,o,x,e)!=E32G::Ok){r=CorruptImage;break;}
            uint32_t value=0;
            if(x.raw || x.absent)r=ExportAddress(im,rec.mapping,x.raw,value);
            if(!r)r=a.WriteWord(rec.code,CodeRegion,im.exports.offset-im.code.offset+(o-1)*4,value);
        }
        if(r)break;
        uint32_t stamp=0;r=a.NextStamp(stamp);if(r)break;
        if(!stamp || stamp==0xffffffffu){r=CorruptImage;break;}
        uint32_t off=0;
        if(!Add(rec.mapping.entryRun-rec.mapping.codeRun,12,off) || off!=im.entryCodeOffset+12){r=CorruptImage;break;}
        r=a.WriteWord(rec.code,CodeRegion,off,stamp);
    }
    for(uint32_t i=0;!r && i<p.edgeCount;++i) {
        const Edge& edge=p.edges[i];const E32G::Image& im=p.nodes[edge.importer].image;
        const E32G::Image& ex=p.nodes[edge.exporter].image;
        const ObjectRecord& importer=l.records[edge.importer];const ObjectRecord& exporter=l.records[edge.exporter];
        E32G::ImportView iv;E32G::Error fe;
        if(E32G::ImportAt(im,edge.importIndex,iv,fe)!=E32G::Ok){r=CorruptImage;break;}
        for(uint32_t k=0;!r && k<iv.count;++k) {
            uint32_t ordinal=R32(im.file.data+iv.iat.offset+k*4),value=0;E32G::ExportView x;
            if(!ordinal || E32G::ExportAt(ex,ordinal,x,fe)!=E32G::Ok || x.absent || !x.raw || x.raw==ex.entryCodeOffset){r=NotSupported;break;}
            r=ExportAddress(ex,exporter.mapping,x.raw,value);
            if(!r)r=a.WriteWord(importer.code,CodeRegion,iv.iat.offset-im.code.offset+k*4,value);
        }
        if(!r)r=a.AddDependency(importer.code,exporter.code);
    }
    for(uint32_t i=0;!r && i<p.orderCount;++i) {
        uint32_t n=p.order[i];ObjectRecord& ob=l.records[n];
        r=ob.rootProcess?a.FinaliseProcess(ob.process,ob.mapping):a.FinaliseCode(ob.code,ob.mapping);
        if(!r)ob.finalised=true;
    }
    int32_t handle=0;
    if(!r)r=p.request.kind==ProcessLoad?a.PublishProcess(l.records[0].process,p.request.client,handle):a.PublishLibrary(l.records[0].code,p.request.client,handle);
    if(r)Abort(l,a);
    else {
        l.published=true;l.state=CommittedTransaction;out.clientHandle=handle;out.segments=p.nodeCount;
        for(uint32_t i=l.count;i>0;--i){a.CloseCode(l.records[i-1].code);l.records[i-1].acquired=false;}
        l.count=0;
    }
    ReleasePlan(p);d.result=r;return r;
}
} // namespace R1G
