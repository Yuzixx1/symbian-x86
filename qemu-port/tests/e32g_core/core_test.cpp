#include "e32g_load.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include <iostream>
using namespace R1G;
typedef std::vector<uint8_t> Data;
static uint32_t word(const Data& b,uint32_t o){assert(o+4<=b.size());return uint32_t(b[o])|uint32_t(b[o+1])<<8|uint32_t(b[o+2])<<16|uint32_t(b[o+3])<<24;}
static void put(Data& b,uint32_t o,uint32_t x){assert(o+4<=b.size());for(unsigned k=0;k<4;++k)b[o+k]=uint8_t(x>>(k*8));}
static Data read(const std::string& p){std::ifstream f(p.c_str(),std::ios::binary);assert(f);return Data(std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>());}
static std::string narrow(ImageName p){std::string s;for(uint32_t i=0;i<p.units;++i)s+=char(p.text[i]);return s;}
static std::vector<uint16_t> wide(const std::string& s){return std::vector<uint16_t>(s.begin(),s.end());}
static void seal(Data& b){E32G::Bytes all={&b[0],uint32_t(b.size())};put(b,12,E32G::UidChecksum(all));E32G::Bytes hdr={&b[0],word(b,100)};put(b,20,E32G::HeaderCrc(hdr));}
struct Source:ImageSource {
    std::map<std::string,Data> files;
    std::map<std::string,std::vector<std::string> > candidates;
    std::map<unsigned,std::string> live;
    unsigned serial,calls,failAt,opened,closed; bool shortSize;
    Source():serial(0),calls(0),failAt(0),opened(0),closed(0),shortSize(false){}
    bool fail(){return ++calls==failAt;}
    Result Candidate(E32G::Bytes root,uint32_t i,Path& out){
        if(fail())return NoMemory;
        std::string key(reinterpret_cast<const char*>(root.data),root.size);
        std::vector<std::string>& list=candidates[key];if(i>=list.size())return NotFound;
        assert(list[i].size()<=MaxNameUnits);out.units=uint32_t(list[i].size());
        for(uint32_t k=0;k<out.units;++k)out.text[k]=list[i][k];
        return Success;
    }
    Result OpenSnapshot(ImageName name,Snapshot& out){
        if(fail())return NoMemory;
        std::string path=narrow(name);
        if(files.find(path)==files.end())return NotFound;
        out.slot=++serial;out.generation=serial+1000;live[out.slot]=path;++opened;return Success;
    }
    Result Size(Snapshot h,uint32_t& out){if(fail())return NoMemory;assert(live.count(h.slot));out=uint32_t(files[live[h.slot]].size())+(shortSize?1:0);return Success;}
    Result Bytes(Snapshot h,E32G::Bytes& out){if(fail())return NoMemory;assert(live.count(h.slot));Data& b=files[live[h.slot]];out.data=&b[0];out.size=uint32_t(b.size());return Success;}
    Result ReadAt(Snapshot h,uint32_t off,uint8_t* dst,uint32_t cap,uint32_t& actual){
        actual=0;if(fail())return NoMemory;Data& b=files[live[h.slot]];
        if(off>b.size())return Argument;
        actual=uint32_t(b.size()-off);if(actual>cap)actual=cap;
        std::memcpy(dst,&b[off],actual);return Success;
    }
    void Close(Snapshot h){assert(h.generation==h.slot+1000);assert(live.erase(h.slot)==1);++closed;}
    void baseline(){assert(live.empty());assert(opened==closed);}
};
struct Fake:LoaderObjects {
    struct Object {Data code,data,runData; E32G::Image im; Mapping m; unsigned deps,edges,refs;bool temp,done,process;std::vector<unsigned> children;};
    std::vector<Object> objects;unsigned calls,failAt,temps,stamps,publishes,aborts;
    bool processAlive,invalidMapping,badStamp;std::vector<unsigned> finalOrder;
    Fake():calls(0),failAt(0),temps(0),stamps(100),publishes(0),aborts(0),processAlive(false),invalidMapping(false),badStamp(false){}
    bool fail(){return ++calls==failAt;}
    Object& get(CodeToken t){assert(t.slot<objects.size() && t.generation==99);return objects[t.slot];}
    Result create(const CreateSpec& spec,ObjectRecord& out,bool proc){
        if(fail())return NoMemory;
        Object ob;ob.im=*spec.image;ob.deps=spec.dependencies;ob.edges=0;ob.refs=proc?2:1;ob.temp=true;ob.done=false;ob.process=proc;
        assert(spec.security.secureId==ob.im.secureId && spec.security.vendorId==ob.im.vendorId && spec.security.caps[0]==ob.im.capabilities[0] && spec.security.caps[1]==ob.im.capabilities[1]);
        unsigned n=unsigned(objects.size());
        ob.m.codeLoad=0x20000000+n*0x100000;ob.m.codeRun=0x50000000+n*0x100000;
        ob.m.dataLoad=0x30000000+n*0x100000;ob.m.dataRun=0x60000000+n*0x100000;
        ob.m.entryRun=ob.m.codeRun+ob.im.entryCodeOffset;
        ob.m.exportsRun=ob.im.exportCount?ob.m.codeRun+ob.im.exports.offset-ob.im.code.offset:0;
        if(invalidMapping)ob.m.entryRun+=ob.m.codeRun; // catch double-base
        ob.code.resize(ob.im.code.size);ob.data.resize(ob.im.data.size);
        objects.push_back(ob);out.code.slot=n;out.code.generation=99;
        out.process.slot=n;out.process.generation=77;out.mapping=ob.m;
        ++temps;if(proc){assert(!processAlive);processAlive=true;}return Success;
    }
    Result CreateProcess(const CreateSpec& s,ObjectRecord& o){return create(s,o,true);}
    Result CreateCode(const CreateSpec& s,ObjectRecord& o){return create(s,o,false);}
    void drop(unsigned index){Object& o=objects[index];assert(o.refs);if(!--o.refs)for(unsigned k=0;k<o.children.size();++k)drop(o.children[k]);}
    void AbortProcess(ProcessToken t){assert(t.generation==77 && processAlive && !publishes);processAlive=false;++aborts;drop(t.slot);}
    void CloseCode(CodeToken t){Object& o=get(t);assert(o.temp);if(!publishes)assert(!processAlive);o.temp=false;--temps;drop(t.slot);}
    Data& region(CodeToken t,Region r){Object& o=get(t);return r==CodeRegion?o.code:o.data;}
    Result Write(CodeToken t,Region r,uint32_t off,E32G::Bytes b){if(fail())return NoMemory;Data& d=region(t,r);assert(off<=d.size() && b.size<=d.size()-off);std::memcpy(&d[off],b.data,b.size);return Success;}
    Result ReadWord(CodeToken t,Region r,uint32_t off,uint32_t& out){if(fail())return NoMemory;out=word(region(t,r),off);return Success;}
    Result WriteWord(CodeToken t,Region r,uint32_t off,uint32_t value){if(fail())return NoMemory;put(region(t,r),off,value);return Success;}
    Result AddDependency(CodeToken from,CodeToken to){if(fail())return NoMemory;Object& a=get(from);Object& b=get(to);assert(a.edges<a.deps);++a.edges;++b.refs;a.children.push_back(to.slot);return Success;}
    Result FinaliseCode(CodeToken t,const Mapping& m){if(fail())return NoMemory;Object& o=get(t);assert(o.edges==o.deps && o.m.entryRun==m.entryRun);o.done=true;finalOrder.push_back(t.slot);return Success;}
    Result FinaliseProcess(ProcessToken p,const Mapping& m){
        if(fail())return NoMemory;
        assert(p.generation==77);Object& o=objects[p.slot];assert(o.edges==o.deps && o.m.entryRun==m.entryRun);
        o.runData=o.data;o.runData.resize(o.im.data.size+o.im.bssSize,0);o.done=true;finalOrder.push_back(p.slot);return Success;
    }
    Result NextStamp(uint32_t& out){if(fail())return NoMemory;out=badStamp?0xffffffffu:++stamps;return Success;}
    Result PublishProcess(ProcessToken p,const ClientContext&,int32_t& h){
        if(fail())return NoMemory;
        assert(processAlive && objects[p.slot].done);++publishes;h=123;return Success;
    }
    Result PublishLibrary(CodeToken t,const ClientContext&,int32_t& h){if(fail())return NoMemory;assert(get(t).done);++get(t).refs;++publishes;h=456;return Success;}
    void baseline(){assert(!temps && !publishes && !processAlive);for(unsigned i=0;i<objects.size();++i)assert(!objects[i].refs);}
};
struct Storage {
    Node nodes[16];Edge edges[64];uint32_t order[16];ObjectRecord records[16];Plan plan;Ledger ledger;
    Storage(){InitPlan(plan,nodes,16,edges,64,order,16);InitLedger(ledger,records,16);}
};
static LoadRequest request(const std::vector<uint16_t>& path,bool library=false){
    LoadRequest q={};q.name.text=&path[0];q.name.units=uint32_t(path.size());q.kind=library?LibraryLoad:ProcessLoad;q.requested.version=0xffffffffu;return q;
}
static Limits limits(){Limits l={16*1024*1024,32*1024*1024,8};return l;}
static void configure(Source& s,const std::string& fixtures,const std::string& vectors){
    s.files["C:\\sys\\bin\\root.exe"]=read(vectors+"/root.e32");
    s.files["C:\\sys\\bin\\euser.dll"]=read(vectors+"/dll.e32");
    s.candidates["euser.dll"].push_back("C:\\sys\\bin\\euser.dll");
    (void)fixtures;
}
static void verify(const std::vector<Node>& nodes,const std::vector<Edge>& edges,const Fake& a){
    for(unsigned i=0;i<nodes.size();++i){
        const E32G::Image& im=nodes[i].image;const Fake::Object& o=a.objects[i];
        assert(word(o.code,im.entryCodeOffset+12)>100);
        assert(o.im.secureId==im.secureId);
        for(uint32_t j=0;j<im.exportCount;++j){
            uint32_t raw=uint32_t(im.file.data[im.exports.offset+j*4])|uint32_t(im.file.data[im.exports.offset+j*4+1])<<8|uint32_t(im.file.data[im.exports.offset+j*4+2])<<16|uint32_t(im.file.data[im.exports.offset+j*4+3])<<24;
            uint32_t expected=raw?o.m.codeRun+raw:0;
            assert(word(o.code,im.exports.offset-im.code.offset+j*4)==expected);
        }
    }
    for(unsigned i=0;i<edges.size();++i){
        const Edge& edge=edges[i];const E32G::Image& im=nodes[edge.importer].image;
        const E32G::Image& ex=nodes[edge.exporter].image;E32G::ImportView v;E32G::Error e;
        assert(E32G::ImportAt(im,edge.importIndex,v,e)==E32G::Ok);
        Data raw(im.file.data,im.file.data+im.file.size);
        Data exp(ex.file.data,ex.file.data+ex.file.size);
        for(uint32_t k=0;k<v.count;++k){
            uint32_t ordinal=word(raw,v.iat.offset+k*4);
            assert(word(a.objects[edge.importer].code,v.iat.offset-im.code.offset+k*4)==
                a.objects[edge.exporter].m.codeRun+word(exp,ex.exports.offset+(ordinal-1)*4));
        }
    }
}
int main(int argc,char**argv){
    assert(argc==4);std::string fixtures=argv[1],vectors=argv[2],fixture=argv[3];
    std::vector<uint16_t> path=wide("C:\\sys\\bin\\root.exe");LoadRequest q=request(path);Diagnostic d;
    unsigned boundaries=0;
    {
        Source s;configure(s,fixtures,vectors);Storage st;Fake a;
        assert(BuildPlan(st.plan,s,q,limits(),d)==Success && st.plan.nodeCount==2);
        assert(st.plan.order[0]==1 && st.plan.order[1]==0);
        std::vector<Node> nodes(st.plan.nodes,st.plan.nodes+st.plan.nodeCount);
        std::vector<Edge> edges(st.plan.edges,st.plan.edges+st.plan.edgeCount);
        LoadResult out;assert(Materialise(st.plan,a,st.ledger,out,d)==Success && out.clientHandle==123);
        assert(a.publishes==1 && a.temps==0);s.baseline();verify(nodes,edges,a);
        assert(word(a.objects[0].code,32)==a.objects[0].m.codeRun+32);
        boundaries=a.calls;Abort(st.ledger,a);assert(a.publishes==1);
    }
    for(unsigned fail=1;fail<=boundaries;++fail){
        Source s;configure(s,fixtures,vectors);Storage st;Fake a;a.failAt=fail;
        assert(BuildPlan(st.plan,s,q,limits(),d)==Success);LoadResult out;
        assert(Materialise(st.plan,a,st.ledger,out,d)!=Success);assert(!out.clientHandle);
        Abort(st.ledger,a);a.baseline();s.baseline();
    }
    unsigned sourceCalls=0;
    {Source s;configure(s,fixtures,vectors);Storage st;assert(BuildPlan(st.plan,s,q,limits(),d)==Success);sourceCalls=s.calls;ReleasePlan(st.plan);ReleasePlan(st.plan);s.baseline();}
    for(unsigned fail=1;fail<=sourceCalls;++fail){Source s;configure(s,fixtures,vectors);s.failAt=fail;Storage st;assert(BuildPlan(st.plan,s,q,limits(),d)!=Success);s.baseline();}
    // Real immutable v1 closures and separately built R1-A fixture; no execution.
    const char* roots[]={"t_cactw.exe","fhserver.exe","euser.dll","efsrv.dll","r1a_fixture.exe"};
    for(unsigned i=0;i<5;++i){
        Source s;Storage st;Fake a;
        s.files["C:\\sys\\bin\\euser.dll"]=read(fixtures+"/euser.dll");
        s.files["C:\\sys\\bin\\efsrv.dll"]=read(fixtures+"/efsrv.dll");
        s.candidates["euser.dll"].push_back("C:\\sys\\bin\\euser.dll");
        s.candidates["efsrv.dll"].push_back("C:\\sys\\bin\\efsrv.dll");
        s.files[narrow(q.name)]=read(i==4?fixture:fixtures+"/"+roots[i]);
        LoadRequest rq=q;rq.kind=(i==2 || i==3)?LibraryLoad:ProcessLoad;
        Result result=BuildPlan(st.plan,s,rq,limits(),d);assert(result==Success);
        std::vector<Node> nodes(st.plan.nodes,st.plan.nodes+st.plan.nodeCount);std::vector<Edge> edges(st.plan.edges,st.plan.edges+st.plan.edgeCount);
        LoadResult out;result=Materialise(st.plan,a,st.ledger,out,d);
        if(result){std::cerr<<roots[i]<<" materialise "<<result<<" node "<<d.node<<"\n";return 1;}
        verify(nodes,edges,a);assert(a.temps==0 && a.publishes==1);s.baseline();
        std::cout<<"PASS v1/fixture binding "<<roots[i]<<" segments "<<nodes.size()<<"\n";
    }
    // Constraint, budget, cycle and stamp negatives, each with a fresh source.
    for(unsigned test=0;test<26;++test){
        Source s;configure(s,fixtures,vectors);Storage st;Fake a;LoadRequest rq=q;Limits lim=limits();
        Data& root=s.files[narrow(q.name)];Data& dll=s.files["C:\\sys\\bin\\euser.dll"];
        bool material=false;
        switch(test){
        case 0:rq.requested.uid3=0xbad;break;
        case 1:rq.requested.version=0xb0000;break;
        case 2:rq.requestedSecureId=123;break;
        case 3:lim.imageBytes=1;break;
        case 4:lim.aggregateBytes=uint32_t(root.size());break;
        case 5:st.plan.nodeCapacity=1;break;
        case 6:st.plan.edgeCapacity=0;break;
        case 7:s.shortSize=true;break;
        case 8:put(root,136,0x80000000);seal(root);break;
        case 9:put(root,word(root,100)+word(root,96),0);break;
        case 10:put(root,word(root,100)+word(root,96),999);break;
        case 11:put(dll,word(dll,88),word(dll,72));dll[154]=1;dll[155]=0;seal(dll);break;
        case 12:put(root,word(root,100)+word(root,72)+12,9);material=true;break;
        case 13:root[word(root,100)+word(root,72)]=0xc3;material=true;break;
        case 14:a.invalidMapping=true;material=true;break;
        case 15:a.badStamp=true;material=true;break;
        case 16: // cycle: DLL imports itself
            dll=read(vectors+"/cycle.e32");break;
        case 17: // code relocation conflicts with stamp
            root[word(root,112)+16]=12;root[word(root,112)+17]=0x30;material=true;break;
        case 18: // semantically valid absent ordinal with matching bitmap
            put(dll,word(dll,88),0);dll[152]=1;dll[154]=1;dll[155]=0;seal(dll);break;
        case 19: // null but present ordinal (nonzero entry marker)
            put(dll,72,16);put(dll,word(dll,88),0);seal(dll);break;
        case 20: // same bytes under wrong candidate basename
            s.files["C:\\sys\\bin\\other.dll"]=dll;
            s.candidates["euser.dll"][0]="C:\\sys\\bin\\other.dll";break;
        case 21:put(dll,24,0xb0000);seal(dll);break;
        case 22:put(dll,136,0);seal(dll);break;
        case 23: // version decoration must agree with actual image
            s.files["C:\\sys\\bin\\euser{000b0000}.dll"]=dll;
            s.candidates["euser.dll"][0]="C:\\sys\\bin\\euser{000b0000}.dll";break;
        case 24: // no double writes at export/relocation
            root[word(root,112)+16]=80;root[word(root,112)+17]=0x30;material=true;break;
        case 25: // no double writes at IAT/relocation
            root[word(root,112)+16]=64;root[word(root,112)+17]=0x30;material=true;break;
        }
        Result r=BuildPlan(st.plan,s,rq,lim,d);
        if(material){assert(r==Success);LoadResult out;r=Materialise(st.plan,a,st.ledger,out,d);}
        if(!r){std::cerr<<"negative failed "<<test<<"\n";return 1;}
        ReleasePlan(st.plan);a.baseline();s.baseline();
    }
    // Highest compatible minor, wildcard and exact UID selection.
    for(unsigned mode=0;mode<3;++mode){
        Source s;configure(s,fixtures,vectors);Storage st;
        Data higher=s.files["C:\\sys\\bin\\euser.dll"];put(higher,24,0xa0002);seal(higher);
        s.files["C:\\versions\\euser.dll"]=higher;s.candidates["euser.dll"].push_back("C:\\versions\\euser.dll");
        if(mode==1){Data& root=s.files[narrow(q.name)];uint32_t off=word(root,108)+12;const char* name="euser{fffffffe}[100039e5].dll";std::memcpy(&root[off],name,std::strlen(name));}
        if(mode==2){put(s.files["C:\\sys\\bin\\euser.dll"],8,0x11111111);seal(s.files["C:\\sys\\bin\\euser.dll"]);}
        assert(BuildPlan(st.plan,s,q,limits(),d)==Success);assert(st.plan.nodes[1].image.moduleVersion==0xa0002);ReleasePlan(st.plan);s.baseline();
    }
    // Synthetic data target + initialized template relocation, BSS done at init.
    unsigned dataBoundaries=0;
    {
        Source s;configure(s,fixtures,vectors);s.files[narrow(q.name)]=read(vectors+"/data.e32");Storage st;Fake a;
        assert(BuildPlan(st.plan,s,q,limits(),d)==Success);LoadResult out;
        assert(Materialise(st.plan,a,st.ledger,out,d)==Success);
        assert(word(a.objects[0].code,36)==a.objects[0].m.dataRun+4);
        assert(word(a.objects[0].data,0)==a.objects[0].m.codeRun+32);
        assert(word(a.objects[0].runData,0)==a.objects[0].m.codeRun+32);
        for(unsigned i=8;i<a.objects[0].runData.size();++i)assert(a.objects[0].runData[i]==0);
        dataBoundaries=a.calls;s.baseline();
    }
    for(unsigned fail=1;fail<=dataBoundaries;++fail){
        Source s;configure(s,fixtures,vectors);s.files[narrow(q.name)]=read(vectors+"/data.e32");Storage st;Fake a;a.failAt=fail;
        assert(BuildPlan(st.plan,s,q,limits(),d)==Success);LoadResult out;
        assert(Materialise(st.plan,a,st.ledger,out,d)!=Success);Abort(st.ledger,a);a.baseline();s.baseline();
    }
    unsigned libraryBoundaries=0;
    for(unsigned fail=0;fail<=libraryBoundaries;++fail){
        Source s;configure(s,fixtures,vectors);s.files[narrow(q.name)]=read(vectors+"/dll.e32");Storage st;Fake a;a.failAt=fail;
        LoadRequest rq=q;rq.kind=LibraryLoad;rq.clientSecurity.caps[0]=0x123;rq.clientSecurity.caps[1]=0x456;
        assert(BuildPlan(st.plan,s,rq,limits(),d)==Success);LoadResult out;
        Result r=Materialise(st.plan,a,st.ledger,out,d);
        if(!fail){assert(!r && out.clientHandle==456);libraryBoundaries=a.calls;}
        else {assert(r);a.baseline();}
        Abort(st.ledger,a);s.baseline();
    }
    { // insufficient ledger consumes Ready plan without creating an object
        Source s;configure(s,fixtures,vectors);Storage st;Fake a;assert(!BuildPlan(st.plan,s,q,limits(),d));
        st.ledger.capacity=0;LoadResult out;assert(Materialise(st.plan,a,st.ledger,out,d)==Argument);s.baseline();a.baseline();
    }
    for(unsigned mode=0;mode<3;++mode){
        Source s;configure(s,fixtures,vectors);Storage st;Fake a;
        const char* file=mode==0?"general_data.e32":mode==1?"unaligned.e32":"diamond.e32";
        s.files[narrow(q.name)]=read(vectors+"/"+file);
        if(mode==2){s.files["C:\\sys\\bin\\second.dll"]=read(vectors+"/second.e32");s.candidates["second.dll"].push_back("C:\\sys\\bin\\second.dll");}
        assert(!BuildPlan(st.plan,s,q,limits(),d));
        if(mode==2){assert(st.plan.nodeCount==3 && st.plan.edgeCount==3);assert(st.plan.order[0]==2 && st.plan.order[1]==1 && st.plan.order[2]==0);}
        LoadResult out;assert(!Materialise(st.plan,a,st.ledger,out,d));s.baseline();
        if(mode==0)assert(word(a.objects[0].code,36)==a.objects[0].m.dataRun+4);
        if(mode==1)assert(word(a.objects[0].code,33)==a.objects[0].m.codeRun+32);
    }
    { // same boot-wide allocator, fresh snapshots/segments, distinct stamps
        Source s;configure(s,fixtures,vectors);s.files[narrow(q.name)]=read(vectors+"/dll.e32");Fake a;
        for(unsigned k=0;k<2;++k){Storage st;LoadRequest rq=q;rq.kind=LibraryLoad;assert(!BuildPlan(st.plan,s,rq,limits(),d));LoadResult out;assert(!Materialise(st.plan,a,st.ledger,out,d));s.baseline();}
        assert(word(a.objects[0].code,12)!=word(a.objects[1].code,12));
    }
    { // finite candidate limit accepts exactly N and rejects N+1
        Source s;configure(s,fixtures,vectors);Storage st;Limits lim=limits();lim.candidates=1;
        assert(!BuildPlan(st.plan,s,q,lim,d));ReleasePlan(st.plan);s.baseline();
        s.candidates["euser.dll"].push_back("C:\\sys\\bin\\euser.dll");
        assert(BuildPlan(st.plan,s,q,lim,d)==TooBig);s.baseline();
    }
    std::cout<<"PASS core; object failure boundaries "<<boundaries<<", data "<<dataBoundaries<<", library "<<libraryBoundaries<<", source "<<sourceCalls<<", 26 negatives, versions, data/BSS, limits\n";
}
