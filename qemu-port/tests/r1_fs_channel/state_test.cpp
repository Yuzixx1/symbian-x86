#include "r1_fs_channel_state.h"
#include "r1_fs_wire_logic.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
using namespace R1Fs;
using namespace R1FsWire;

struct FakeFile { int id; };
struct FakeOps {
    static int32_t openResult, sizeResult, readResult, writeResult, resizeResult, flushResult;
    static int closeCount, sizeCalls, writeCalls;
    static bool sizeFailsAfterWrite;
    static bool wrote;
    static uint32_t sizeValue;
    static int32_t Open(char, const char*, bool, FakeFile& f) {
        if (openResult) return openResult;
        f.id=1; return 0;
    }
    static int32_t Size(FakeFile&, uint32_t& s) {
        ++sizeCalls;
        if (sizeFailsAfterWrite && wrote) return Corrupt;
        if (sizeResult) return sizeResult;
        s=sizeValue; return 0;
    }
    static int32_t Read(FakeFile&, uint32_t, uint8_t*, uint32_t, uint32_t& done) {
        if (readResult) { done=0; return readResult; }
        done=4; return 0;
    }
    static int32_t Write(FakeFile&, uint32_t, const uint8_t*, uint32_t, uint32_t& done) {
        ++writeCalls; wrote=true; done=2;
        return writeResult;
    }
    static int32_t Resize(FakeFile&, uint32_t) { return resizeResult; }
    static int32_t Flush(FakeFile&) { return flushResult; }
    static void Close(FakeFile&) { ++closeCount; }
};
int32_t FakeOps::openResult=0, FakeOps::sizeResult=0, FakeOps::readResult=0,
    FakeOps::writeResult=0, FakeOps::resizeResult=0, FakeOps::flushResult=0;
int FakeOps::closeCount=0, FakeOps::sizeCalls=0, FakeOps::writeCalls=0;
bool FakeOps::sizeFailsAfterWrite=false, FakeOps::wrote=false;
uint32_t FakeOps::sizeValue=100;

typedef ChannelState<FakeFile,FakeOps> State;

struct FakeLock { static int enters, leaves;
    static void Enter(){++enters;} static void Leave(){++leaves;} };
int FakeLock::enters=0, FakeLock::leaves=0;

struct FakeAuth { const bool* ok; bool operator()() const { return *ok; } };
struct FakeCopy { int32_t result; static int calls; Packet seen;
    FakeCopy(int32_t r):result(r){memset(&seen,0,sizeof(seen));}
    int32_t operator()(const Packet& p){ ++calls; seen=p; return result; } };
int FakeCopy::calls=0;

static Packet OpenPacket(const char* path, unsigned disp) {
    Packet p; memset(&p,0,sizeof(p)); p.version=Version; p.op=Open; p.disposition=disp;
    strcpy(p.path,path); return p;
}
static Packet ReadPacket(Key key) {
    Packet p; memset(&p,0,sizeof(p)); p.version=Version; p.op=Read;
    p.slot=key.slot; p.generation=key.generation; p.length=4; return p;
}
static Packet WritePacket(Key key) {
    Packet p; memset(&p,0,sizeof(p)); p.version=Version; p.op=Write;
    p.slot=key.slot; p.generation=key.generation; p.length=4; return p;
}

static void Reset() {
    FakeOps::openResult=0; FakeOps::sizeResult=0; FakeOps::readResult=0;
    FakeOps::writeResult=0; FakeOps::resizeResult=0; FakeOps::flushResult=0;
    FakeOps::closeCount=0; FakeOps::sizeCalls=0; FakeOps::writeCalls=0;
    FakeOps::sizeFailsAfterWrite=false; FakeOps::wrote=false; FakeOps::sizeValue=100;
    FakeLock::enters=0; FakeLock::leaves=0;
    FakeCopy::calls=0;
}

struct RevokingState {
    State& inner; bool* auth;
    int32_t Dispatch(Packet& p){ int32_t r=inner.Dispatch(p); *auth=false; return r; }
    bool AbandonNewKey(const Packet& p){ return inner.AbandonNewKey(p); }
};

int main() {
    Reset();
    State st;
    Packet p=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    int32_t r=st.Dispatch(p);
    assert(r==Success && p.slot==0 && p.generation==1 && p.size==100 && p.stable==1);
    assert(FakeOps::closeCount==0);
    assert(st.AbandonNewKey(p)==true && FakeOps::closeCount==1);
    FakeOps::openResult=0; FakeOps::sizeValue=101;
    Packet p2=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    assert(st.Dispatch(p2)==Success && p2.generation==2);
    Packet stale=p; stale.generation=1;
    assert(st.AbandonNewKey(stale)==false && FakeOps::closeCount==1);

    Reset();
    State st2;
    FakeOps::openResult=NotFound;
    Packet po=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    po.slot=7; po.generation=7; po.done=7; po.size=7; po.stable=7; po.quarantined=7; po.status=7;
    PrepareResponse(po);
    assert(po.slot==0 && po.generation==0 && po.done==0 && po.size==0 && po.stable==0 && po.quarantined==0 && po.status==0);
    r=st2.Dispatch(po);
    assert(r==NotFound && FakeOps::closeCount==0);
    assert(po.slot==0 && po.generation==0 && po.size==0 && po.stable==0 && po.quarantined==0);

    Reset();
    State st3;
    Packet ok=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    assert(st3.Dispatch(ok)==Success);
    Key key8={ok.slot,ok.generation};
    Packet bad=ReadPacket(key8); bad.generation=ok.generation+1;
    assert(st3.Dispatch(bad)==BadHandle);

    Reset();
    State st4;
    Packet o=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    assert(st4.Dispatch(o)==Success);
    FakeOps::writeResult=DiskFull;
    FakeOps::sizeFailsAfterWrite=true; FakeOps::wrote=false;
    Key key4={o.slot,o.generation};
    Packet w=WritePacket(key4);
    r=st4.Dispatch(w);
    assert(r==DiskFull && w.stable==0 && w.quarantined==1 && w.size==0);

    // RunRequest: post-dispatch revoke closes the newly acquired key.
    Reset();
    State st5;
    bool auth=true;
    FakeAuth a = { &auth };
    // use a real copy that succeeds
    FakeCopy c2(0);
    Packet po2=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    RevokingState rs = { st5, &auth };
    FakeLock::enters=0; FakeLock::leaves=0;
    int32_t rr=RunRequest<RevokingState,FakeLock>(rs,po2,Open,a,c2);
    assert(rr==AccessDenied && FakeCopy::calls==0 && FakeOps::closeCount==1);
    assert(FakeLock::enters==FakeLock::leaves);

    // RunRequest: copyout failure closes the newly acquired key, preserves error.
    Reset();
    State st6;
    bool auth2=true; FakeAuth a2 = { &auth2 }; FakeCopy c3(-22);
    Packet po3=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    rr=RunRequest<State,FakeLock>(st6,po3,Open,a2,c3);
    assert(rr==-22 && FakeCopy::calls==1 && FakeOps::closeCount==1);

    // RunRequest: successful copyout keeps the key.
    Reset();
    State st7;
    bool auth3=true; FakeAuth a3 = { &auth3 }; FakeCopy c4(0);
    Packet po4=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    rr=RunRequest<State,FakeLock>(st7,po4,Open,a3,c4);
    assert(rr==0 && FakeCopy::calls==1 && FakeOps::closeCount==0);

    // RunRequest: existing-key copyout failure does NOT close the key.
    Reset();
    State st8;
    bool auth4=true; FakeAuth a4 = { &auth4 }; FakeCopy c5(-22);
    Packet o8=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    assert(st8.Dispatch(o8)==Success);
    FakeOps::closeCount=0;
    Key key8b={o8.slot,o8.generation};
    Packet r8=ReadPacket(key8b);
    rr=RunRequest<State,FakeLock>(st8,r8,Read,a4,c5);
    assert(rr==-22 && FakeCopy::calls==1 && FakeOps::closeCount==0);

    // RunRequest: failed Open still copies out the semantic error.
    Reset();
    FakeOps::openResult=NotFound;
    State st9;
    bool auth5=true; FakeAuth a5 = { &auth5 }; FakeCopy c6(0);
    Packet p9=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    rr=RunRequest<State,FakeLock>(st9,p9,Open,a5,c6);
    assert(rr==0 && FakeCopy::calls==1 && p9.status==NotFound);
    assert(p9.slot==0 && p9.generation==0 && p9.size==0 && p9.stable==0);

    // Auth failure at start: no dispatch, no copy.
    Reset();
    State st10;
    bool auth6=false; FakeAuth a6 = { &auth6 }; FakeCopy c7(0);
    Packet p10=OpenPacket("D:\\R1TEST\\A\\DATA0.BIN",0);
    rr=RunRequest<State,FakeLock>(st10,p10,Open,a6,c7);
    assert(rr==AccessDenied && FakeCopy::calls==0 && FakeLock::enters==FakeLock::leaves);

    puts("R1 channel state and cleanup PASS");
    return 0;
}
