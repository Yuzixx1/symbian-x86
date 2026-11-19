#include "r1_fs_wire_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
using namespace R1FsWire;
static Packet Request(unsigned op) { Packet p; memset(&p,0,sizeof(p)); p.version=Version; p.op=op; return p; }
int main() {
    // Fixed 32-bit wire layout across host, MinGW and kernel builds.
    assert(sizeof(Packet)==4276);
#if defined(__GNUC__)
    assert(__builtin_offsetof(Packet,status)==48);
    assert(__builtin_offsetof(Packet,path)==52);
    assert(__builtin_offsetof(Packet,data)==180);
#endif
    Packet p=Request(Open);
    strcpy(p.path,"D:\\R1TEST\\A\\DATA0.BIN");
    assert(ValidRequest(p,Open));
    p.path[0]='d'; assert(!ValidRequest(p,Open)); p.path[0]='D';
    strcpy(p.path,"D:\\R1TEST\\..\\BAD.BIN"); assert(!ValidRequest(p,Open));
    strcpy(p.path,"D:\\SYS\\BIN\\APP.EXE"); assert(!ValidRequest(p,Open));
    strcpy(p.path,"D:\\R1TEST\\A\\TOOLONG99.BIN"); assert(!ValidRequest(p,Open));
    memset(p.path,'A',sizeof(p.path)); assert(!ValidRequest(p,Open));
    strcpy(p.path,"D:\\R1TEST\\A\\DATA0.BIN");
    p.disposition=2; assert(!ValidRequest(p,Open)); p.disposition=0;
    p.version=0; assert(!ValidRequest(p,Open)); p.version=Version;
    p.op=Read; assert(!ValidRequest(p,Open));
    p=Request(Write); p.slot=0; p.generation=3; p.length=DataBytes;
    assert(ValidRequest(p,Write));
    p.length=DataBytes+1; assert(!ValidRequest(p,Write)); p.length=0;
    p.generation=0; assert(!ValidRequest(p,Write)); p.generation=3;
    p.slot=Slots; assert(!ValidRequest(p,Write));
    p.slot=2; assert(MatchesKey(p,2,3));
    assert(!MatchesKey(p,2,4)); assert(!MatchesKey(p,Slots,3));
    Packet result=Request(Write); result.stable=1; result.done=512;
    assert(!Uncertain(result,0,Write,512));
    assert(Uncertain(result,-20,Write,512)); // published size unknown on error
    assert(!Uncertain(result,-1,Read,512)); // failed read is not mutation
    result.quarantined=1; assert(Uncertain(result,0,Read,512));
    result.quarantined=0; result.stable=0;
    assert(Uncertain(result,0,Write,512));
    result.stable=1; result.done=513;
    assert(Uncertain(result,0,Read,512));
    // PrepareResponse clears outputs and Open slot/generation only.
    Packet q=Request(Open); q.slot=9; q.generation=9; q.done=9; q.size=9; q.stable=9; q.quarantined=9; q.status=9;
    PrepareResponse(q);
    assert(q.slot==0 && q.generation==0 && q.done==0 && q.size==0 && q.stable==0 && q.quarantined==0 && q.status==0);
    Packet r=Request(Read); r.slot=3; r.generation=4; r.done=9; r.size=9; r.stable=9; r.quarantined=9; r.status=9;
    PrepareResponse(r);
    assert(r.slot==3 && r.generation==4 && r.done==0 && r.size==0 && r.stable==0 && r.quarantined==0 && r.status==0);
    // Storage quarantine is the kernel-reported device signal.
    Packet sq=Request(Read); sq.quarantined=1; assert(StorageUncertain(sq));
    sq.quarantined=0; assert(!StorageUncertain(sq));
    // Malformed success / transfer helpers.
    Packet ms=Request(Read); ms.status=0; ms.stable=0; assert(MalformedSuccess(ms));
    ms.stable=1; assert(!MalformedSuccess(ms));
    Packet mt=Request(Read); mt.done=513; assert(MalformedTransfer(mt,512));
    mt.done=512; assert(!MalformedTransfer(mt,512));
    int process1=1,process2=1;
    assert(!AuthorizedProcess(0,&process1));
    assert(AuthorizedProcess(&process1,&process1));
    assert(!AuthorizedProcess(&process1,&process2)); // identical IDs not authority
    puts("R1 private channel wire bounds PASS");
    return 0;
}
