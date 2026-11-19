#include "r1_fs_core.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace R1Fs;
struct Fake : Backend {
    bool exists, opened, failFlush, unstable, shortSuccess;
    unsigned opens, closes, lookups, partial, reads;
    uint64_t length;
    uint8_t bytes[70000];
    Fake() : exists(false), opened(false), failFlush(false), unstable(false),
             shortSuccess(false),
             opens(0), closes(0), lookups(0), partial(0), reads(0), length(0) {
        memset(bytes,0,sizeof(bytes));
    }
    int32_t Lookup(const Path&, Meta& m) {
        ++lookups; if (!exists) return NotFound;
        m.size=length; m.identity=1; return Success;
    }
    int32_t Open(const Path& p,Disposition d,Key& key,Meta& m) {
        assert(p.length>10 && (p.text[0]=='D' || p.text[0]=='C'));
        if (d==CreateNew && exists) return AlreadyExists;
        if (d==OpenExisting && !exists) return NotFound;
        if (d==ReplaceExisting || d==CreateNew) { exists=true; length=0; }
        ++opens; opened=true; key.slot=opens; key.generation=1;
        m.size=length; m.identity=1; return Success;
    }
    IoResult ReadAt(Key,uint64_t pos,uint8_t* dst,uint32_t count) {
        ++reads; IoResult r={Success,count,length,true};
        assert(pos+count<=length && pos+count<=sizeof(bytes));
        memcpy(dst,bytes+pos,count); return r;
    }
    IoResult WriteAt(Key,uint64_t pos,const uint8_t* src,uint32_t count) {
        IoResult r={Success,count,length,true};
        if (partial) { r.committed=count>partial?partial:count; r.error=DiskFull; partial=0; }
        if (shortSuccess && r.committed) --r.committed;
        assert(pos+r.committed<=sizeof(bytes));
        memcpy(bytes+pos,src,r.committed);
        if (length<pos+r.committed) length=pos+r.committed;
        r.size=length; r.stable=!unstable; return r;
    }
    IoResult Resize(Key,uint64_t n) {
        IoResult r={Success,0,n,true};
        assert(n<=sizeof(bytes)); length=n; r.stable=!unstable; return r;
    }
    int32_t Flush(Key) { return failFlush?DiskFull:Success; }
    int32_t Close(Key) { ++closes; opened=false; return Success; }
};
static void U16(const char* src,uint16_t* dst,uint32_t& n) {
    n=0; while (*src) { dst[n++]=(unsigned char)*src++; }
}
static uint32_t OpenFile(Core& c,uint32_t s,const char* name,uint32_t mode,
                         Disposition how,int32_t expected=Success) {
    uint16_t path[256]; uint32_t n; U16(name,path,n); uint32_t h=0;
    assert(c.Open(s,path,n,mode,how,h)==expected);
    if (expected!=Success) { assert(!h); return 0; }
    assert(h && c.Commit(s,h)==Success); return h;
}
static Frame FrameFor(uint32_t op) {
    Frame f=Frame(); f.function=op; return f;
}
static void TestDecode() {
    Frame f=FrameFor(FileOpen); Decoded d;
    assert(Decode(f,d)==Argument);
    f.descriptor[0]=1; f.descriptor[3]=1; f.capacity[3]=3;
    assert(Decode(f,d)==Success && d.resultLength==4);
    f.capacity[3]=4; assert(Decode(f,d)==Success && d.resultLength==4);
    f.function=FileSeek|D2; f.descriptor[2]=1; f.capacity[2]=7;
    assert(Decode(f,d)==Overflow);
    f.capacity[2]=8; assert(Decode(f,d)==Success && !d.widePosition);
    f.function|=D0; f.descriptor[0]=1; f.length[0]=7;
    assert(Decode(f,d)==Argument);
    f.length[0]=8; assert(Decode(f,d)==Success && d.widePosition);
    f.function=FileSetSize|D0; assert(Decode(f,d)==Success);
    f.function=FileRead|D2; f.descriptor[2]=1; f.length[2]=4;
    assert(Decode(f,d)==Argument);
    f.length[2]=8; f.a[1]=-1; assert(Decode(f,d)==Argument);
    f.function=FileRead; f.a[1]=12; f.capacity[0]=11;
    assert(Decode(f,d)==Overflow);
    f.function=16; assert(Decode(f,d)==NotSupported);
    f.function=FileOpen|0x80000000U; assert(Decode(f,d)==NotSupported);
}
static void TestPaths(Core& c,uint32_t s) {
    uint16_t p[256]; uint32_t n; Path cwd;
    U16("D:\\R1TEST\\A\\",p,n);
    assert(c.SetPath(s,p,n)==Success && c.GetPath(s,cwd)==Success);
    assert(cwd.length==12 && cwd.text[0]=='D');
    U16("D:\\R1TEST\\MISSING\\",p,n);
    assert(c.SetPath(s,p,n)==Success); // SetSessionPath does not stat
    U16("D:\\R1TEST\\A\\",p,n); assert(c.SetPath(s,p,n)==Success);
    U16("D:\\sys\\bin\\evil.exe",p,n); assert(c.SetPath(s,p,n)==AccessDenied);
    U16("..\\escape.bin",p,n); assert(c.SetPath(s,p,n)==BadName);
    U16("\\R1TEST\\A\\",p,n); assert(c.SetPath(s,p,n)==Success);
}
static void TestState() {
    Fake f; Core c(f); assert(c.SetDrive('D')==Success);
    uint32_t s,t; assert(c.Connect(0x1234abcd,s)==Success);
    Path defaultPath; assert(c.GetPath(s,defaultPath)==Success);
    assert(defaultPath.text[3]=='P' && defaultPath.text[11]=='1');
    assert(c.SetDrive('E')==InUse);
    TestPaths(c,s);
    assert(c.Connect(0xabcd1234,t)==Success);
    uint32_t missing=OpenFile(c,s,"D:\\R1TEST\\A\\NONE.BIN",ShareAny,OpenExisting,NotFound);
    assert(!missing && f.opens==0);
    uint32_t x=OpenFile(c,s,"DATA0.BIN",ShareAny|ModeWrite,CreateNew);
    uint32_t y=OpenFile(c,t,"D:\\R1TEST\\A\\DATA0.BIN",ShareFlexible,OpenExisting);
    uint64_t cross=0; assert(c.Size(t,x,cross)==BadHandle);
    assert(c.CheckShare(s,x)==Success && c.CheckShare(t,x)==BadHandle);
    uint32_t committed=0; uint8_t raw[256]; memset(raw,7,sizeof(raw));
    assert(c.Write(s,x,0,true,raw,200,committed)==Success && committed==200);
    uint64_t size=0,pos=0; assert(c.Size(t,y,size)==Success && size==200);
    assert(c.Seek(s,x,150,1,pos)==Success && pos==150);
    assert(c.Resize(s,x,100)==Success && c.Size(t,y,size)==Success && size==100);
    assert(c.Resize(s,x,200)==Success);
    uint8_t seed=0x55;
    uint32_t wrote=0; // separate writable compatible share seeds position 150
    uint32_t z=OpenFile(c,t,"D:\\R1TEST\\A\\DATA0.BIN",ShareAny|ModeWrite,OpenExisting);
    assert(c.Write(t,z,150,false,&seed,1,wrote)==Success && wrote==1);
    uint8_t value=0; uint32_t got=0;
    assert(c.Read(s,x,0,true,&value,1,got)==Success && got==1 && value==seed);
    assert(c.Seek(s,x,0,2,pos)==Success && pos==151);
    assert(c.Seek(s,x,210,1,pos)==Success && pos==210);
    assert(c.Write(s,x,0,true,raw,1,committed)==Success && committed==1);
    assert(c.Size(s,x,size)==Success && size==201);
    assert(c.Seek(s,x,1000,1,pos)==Success);
    assert(c.Read(s,x,0,true,raw,1,got)==Success && got==0 &&
           c.Seek(s,x,0,2,pos)==Success && pos==201);
    f.partial=2; assert(c.Write(s,x,0,true,raw,5,committed)==DiskFull && committed==2);
    assert(c.Size(t,y,size)==Success && size==203);
    f.failFlush=true; assert(c.Flush(s,x)==DiskFull);
    f.failFlush=false;
    assert(c.Flush(s,x)==Success);
    assert(c.Close(t,z)==Success && c.Close(t,y)==Success);
    assert(c.Close(s,x)==Success);
    assert(c.Close(s,x)==BadHandle);
    assert(c.Disconnect(s)==Success && c.Disconnect(t)==Success);
    assert(f.opens==f.closes);
}
static void TestShareOrders() {
    for (unsigned pass=0;pass<2;++pass) {
        Fake f; Core c(f); uint32_t a,b; c.Connect(1,a); c.Connect(2,b);
        uint32_t w=OpenFile(c,a,"C:\\R1TEST\\A\\DATA.BIN",ShareFlexible|ModeWrite,CreateNew);
        OpenFile(c,b,"C:\\R1TEST\\A\\DATA.BIN",ShareReaders,OpenExisting,InUse);
        uint32_t r=OpenFile(c,b,"C:\\R1TEST\\A\\DATA.BIN",ShareFlexible,OpenExisting);
        if (!pass) { c.Close(a,w); c.Close(b,r); }
        else { c.Close(b,r); c.Close(a,w); }
        uint32_t only=OpenFile(c,a,"C:\\R1TEST\\A\\DATA.BIN",ShareReaders,OpenExisting);
        OpenFile(c,b,"C:\\R1TEST\\A\\DATA.BIN",ShareFlexible|ModeWrite,OpenExisting,InUse);
        c.Close(a,only); c.Disconnect(a); c.Disconnect(b);
    }
    {
        Fake f; Core c(f); uint32_t a,b; c.Connect(1,a); c.Connect(2,b);
        const char* name="C:\\R1TEST\\A\\PROMOTE.BIN";
        uint32_t w=OpenFile(c,a,name,ShareFlexible,CreateNew);
        uint32_t p=OpenFile(c,b,name,ShareAny,OpenExisting);
        OpenFile(c,a,name,ShareReaders,OpenExisting,InUse);
        uint32_t flexible=OpenFile(c,a,name,ShareFlexible|ModeWrite,OpenExisting);
        assert(c.Close(b,p)==Success);
        OpenFile(c,b,name,ShareReaders,OpenExisting,InUse);
        assert(c.Close(a,flexible)==Success && c.Close(a,w)==Success);
        p=OpenFile(c,b,name,ShareReaders,OpenExisting);
        OpenFile(c,b,name,ShareAny,OpenExisting,InUse);
        assert(c.Close(b,p)==Success);
        uint32_t reader=OpenFile(c,b,name,ShareReaders,OpenExisting);
        uint32_t follower=OpenFile(c,a,name,ShareFlexible,OpenExisting);
        OpenFile(c,a,name,ShareFlexible|ModeWrite,OpenExisting,InUse);
        c.Close(b,reader); c.Close(a,follower);
        c.Disconnect(a); c.Disconnect(b);
        assert(f.opens==f.closes);
    }
}
static void TestFailureAndBounds() {
    Fake f; Core c(f); uint32_t s,t;
    assert(c.Connect(1,s)==Success && c.Connect(2,t)==Success);
    uint16_t path[256]; uint32_t n,h=0;
    U16("C:\\R1TEST\\A\\X.BIN",path,n);
    assert(c.Open(s,path,n,ShareAny|ModeAsyncAll,CreateNew,h)==NotSupported);
    assert(c.Open(s,path,n,ShareReaders|ModeWrite,CreateNew,h)==Argument);
    assert(f.opens==0 && f.closes==0);
    assert(c.Open(s,path,n,ShareAny|ModeWrite,CreateNew,h)==Success);
    assert(c.CheckShare(s,h)==BadHandle && c.CheckShare(t,h)==BadHandle);
    assert(c.Abort(s,h)==Success && f.opens==f.closes);
    assert(c.Abort(s,h)==BadHandle);
    uint32_t w=OpenFile(c,s,"C:\\R1TEST\\A\\X.BIN",ShareAny|ModeWrite,OpenExisting);
    uint32_t r=OpenFile(c,t,"C:\\R1TEST\\A\\X.BIN",ShareFlexible,OpenExisting);
    uint8_t data[4096]; memset(data,0xab,sizeof(data)); uint32_t done=0;
    assert(c.Write(s,w,0,true,data,sizeof(data)+1,done)==Argument);
    assert(c.Write(t,r,0,true,data,1,done)==AccessDenied);
    assert(c.Resize(t,r,2)==AccessDenied);
    uint64_t pos=0;
    assert(c.Seek(s,w,0,0,pos)==NotSupported);
    assert(c.Resize(s,w,-1)==Argument);
    f.partial=1; assert(c.Write(s,w,0,true,data,4,done)==DiskFull && done==1);
    uint64_t size=0;
    assert(c.Size(t,r,size)==Success && size==1);
    assert(c.Seek(s,w,0,2,pos)==Success && pos==1);
    f.unstable=true;
    assert(c.Write(s,w,0,true,data,1,done)==Corrupt);
    assert(c.Size(t,r,size)==Corrupt && c.Flush(s,w)==Corrupt);
    assert(c.Close(s,w)==Success && c.Disconnect(t)==Success && f.opens==f.closes);
    Fake broken; Core bc(broken); uint32_t b; bc.Connect(3,b);
    uint32_t h2=OpenFile(bc,b,"C:\\R1TEST\\A\\SHORT.BIN",ShareAny|ModeWrite,CreateNew);
    broken.shortSuccess=true;
    assert(bc.Write(b,h2,0,true,data,4,done)==Corrupt);
    assert(bc.Flush(b,h2)==Corrupt);
    bc.Disconnect(b);
}
static void TestBoundary() {
    Fake f; Core c(f); uint32_t s; c.Connect(7,s);
    uint32_t h=OpenFile(c,s,"C:\\R1TEST\\B\\EDGE.BIN",ShareAny|ModeWrite,CreateNew);
    uint8_t bytes[4096], out[4096]; memset(bytes,0x5c,sizeof(bytes));
    uint32_t done=0, got=0;
    for (uint32_t offset=0;offset<65536;offset+=4096)
        assert(c.Write(s,h,offset,false,bytes,sizeof(bytes),done)==Success && done==sizeof(bytes));
    assert(c.Write(s,h,65536,false,bytes,1,done)==Success && done==1);
    assert(c.Read(s,h,65535,false,out,2,got)==Success && got==2 &&
           out[0]==0x5c && out[1]==0x5c);
    uint64_t size=0; assert(c.Size(s,h,size)==Success && size==65537);
    assert(c.Close(s,h)==Success && c.Disconnect(s)==Success && f.opens==f.closes);
}
int main() { TestDecode(); TestState(); TestShareOrders();
    TestFailureAndBounds(); TestBoundary();
    puts("R1-FS-CORE portable decoder/state PASS"); return 0; }
