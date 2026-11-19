#include "r1_fs_channel.h"
#include "r1_fs_backend.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace R1Fs;
using namespace R1FsWire;

struct FakeTransport : Transport {
    int32_t transportResult;
    int calls;
    Packet last;
    Packet queue[8];
    int qcount, qnext;
    FakeTransport() : transportResult(0), calls(0), qcount(0), qnext(0) {
        memset(&last,0,sizeof(last));
    }
    void Push(const Packet& p){ assert(qcount<8); queue[qcount++]=p; }
    int32_t Exchange(Packet& p) {
        ++calls; last=p;
        if (transportResult) return transportResult;
        if (qnext>=qcount) return -5;
        p=queue[qnext++];
        return 0;
    }
};

static Packet Resp(unsigned op,int32_t status) {
    Packet p; memset(&p,0,sizeof(p));
    p.version=Version; p.op=op; p.status=status; return p;
}
static Path MakePath(const char* s) {
    Path p; p.length=0;
    while (s[p.length]) { p.text[p.length]=(uint16_t)(unsigned char)s[p.length]; ++p.length; }
    return p;
}

static void TestBasic() {
    FakeTransport t; BackendCore b; assert(b.Attach(t)==Success);
    Packet r=Resp(Open,Success); r.slot=1; r.generation=7; r.size=42; r.stable=1;
    t.Push(r);
    Key k; Meta m; Path p=MakePath("D:\\R1TEST\\A\\DATA0.BIN");
    assert(b.Open(p,OpenExisting,k,m)==Success && k.slot==1 && k.generation==7 && m.size==42);
    assert(t.last.op==Open && t.last.disposition==0);
    assert(strcmp(t.last.path,"D:\\R1TEST\\A\\DATA0.BIN")==0);
    // ReplaceExisting is explicitly unsupported.
    assert(b.Open(p,ReplaceExisting,k,m)==NotSupported && t.calls==1);
    // Invalid path never reaches the transport.
    Path bad=MakePath("D:\\SYS\\BIN\\X.EXE");
    assert(b.Open(bad,OpenExisting,k,m)==BadName && t.calls==1);
    // Stale key read returns the semantic error without quarantining.
    Packet rs=Resp(Read,BadHandle); rs.stable=0;
    t.Push(rs);
    uint8_t buf[4];
    IoResult io=b.ReadAt(k,0,buf,4);
    assert(io.error==BadHandle && !io.stable && !b.Quarantined() && !b.Broken());
    // A later valid read succeeds.
    Packet rv=Resp(Read,Success); rv.stable=1; rv.done=4; rv.size=42;
    memset(rv.data,0x5a,4); t.Push(rv);
    io=b.ReadAt(k,0,buf,4);
    assert(io.error==Success && io.stable && io.committed==4 && io.size==42 && buf[0]==0x5a);
}

static void TestQuarantine() {
    FakeTransport t; BackendCore b; assert(b.Attach(t)==Success);
    Packet r=Resp(Open,Success); r.slot=2; r.generation=3; r.size=10; r.stable=1;
    t.Push(r);
    Key k; Meta m; Path p=MakePath("D:\\R1TEST\\A\\DATA0.BIN");
    assert(b.Open(p,OpenExisting,k,m)==Success);
    Packet rw=Resp(Write,DiskFull); rw.done=2; rw.stable=0; rw.quarantined=1;
    t.Push(rw);
    uint8_t data[4]={1,2,3,4};
    IoResult io=b.WriteAt(k,0,data,4);
    assert(io.error==DiskFull && !io.stable && io.committed==2 && b.Quarantined());
    int calls=t.calls;
    io=b.ReadAt(k,0,data,4);
    assert(io.error==Corrupt && !io.stable && t.calls==calls); // no transport
    assert(b.Open(p,OpenExisting,k,m)==Corrupt && t.calls==calls);
    // Close still reaches the transport and does not clear quarantine.
    Packet rc=Resp(Close,Success); t.Push(rc);
    assert(b.Close(k)==Success && t.calls==calls+1);
    assert(b.Quarantined());
    // Quarantine survives detach/attach to a fresh transport.
    b.Detach();
    FakeTransport t2; assert(b.Attach(t2)==Success);
    assert(b.Quarantined());
    assert(b.Open(p,OpenExisting,k,m)==Corrupt && t2.calls==0);
}

static void TestTransportFault() {
    FakeTransport t; BackendCore b; assert(b.Attach(t)==Success);
    Packet r=Resp(Open,Success); r.slot=1; r.generation=1; r.size=1; r.stable=1;
    t.Push(r);
    Key k; Meta m; Path p=MakePath("D:\\R1TEST\\A\\DATA0.BIN");
    assert(b.Open(p,OpenExisting,k,m)==Success);
    t.transportResult=-22;
    uint8_t buf[1];
    IoResult io=b.ReadAt(k,0,buf,1);
    assert(io.error==-22 && b.Broken() && !b.Quarantined());
    io=b.ReadAt(k,0,buf,1);
    assert(io.error==NotReady && t.calls==2); // fail-closed, no new call
    assert(b.Close(k)==NotReady);
}

static void TestMalformed() {
    FakeTransport t; BackendCore b; assert(b.Attach(t)==Success);
    Packet r=Resp(Open,Success); r.slot=1; r.generation=1; r.size=1; r.stable=1;
    t.Push(r);
    Key k; Meta m; Path p=MakePath("D:\\R1TEST\\A\\DATA0.BIN");
    assert(b.Open(p,OpenExisting,k,m)==Success);
    Packet bad=Resp(Read,Success); bad.stable=1; bad.done=5;
    t.Push(bad);
    uint8_t buf[4];
    IoResult io=b.ReadAt(k,0,buf,4);
    assert(io.error==Corrupt && b.Broken() && !b.Quarantined());
}

static void TestLookup() {
    FakeTransport t; BackendCore b; assert(b.Attach(t)==Success);
    Packet r=Resp(Open,Success); r.slot=1; r.generation=1; r.size=9; r.stable=1;
    t.Push(r);
    Packet rc=Resp(Close,Success); t.Push(rc);
    Meta m; Path p=MakePath("D:\\R1TEST\\A\\DATA0.BIN");
    assert(b.Lookup(p,m)==Success && m.size==9);
    assert(t.calls==2);
}

static void TestNotConnected() {
    BackendCore b;
    Key k; Meta m; Path p=MakePath("D:\\R1TEST\\A\\DATA0.BIN");
    assert(b.Open(p,OpenExisting,k,m)==NotReady);
    uint8_t buf[1]; IoResult io=b.ReadAt(k,0,buf,1);
    assert(io.error==NotReady);
    assert(b.Flush(k)==NotReady);
}

int main() {
    TestBasic();
    TestQuarantine();
    TestTransportFault();
    TestMalformed();
    TestLookup();
    TestNotConnected();
    puts("R1 channel adapter PASS");
    return 0;
}
