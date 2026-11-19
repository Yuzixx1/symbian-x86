#include <x86.h>
#include <cstdio>
#include <cstdlib>
#define __QEMU_VARIANT_H__
#define R1_STORAGE_HOST_TEST
TBool PciFindClass(unsigned,unsigned,unsigned*,unsigned*,unsigned*);
TUint32 PciRead32(unsigned,unsigned,unsigned,unsigned);
void PciWrite32(unsigned,unsigned,unsigned,unsigned,TUint32);
void KPrintf(const char*,...);
#include "../../variant/qemu_ahci.cpp"
static int mode=0,issued=0;static TUint issuedPort=99;
static void Check(bool ok,int n){if(!ok){std::fprintf(stderr,"AHCI fail %d\n",n);std::exit(1);}}
#define CHECK(x) Check((x),__LINE__)
namespace {
static TUint32 Rd32(TUint32 addr)
    {
    TUint off=(addr-gAbar-kPortBase)%kPortStride;
    if(off==kPxCi)return mode==1?2:issued&&mode==2?2:0;
    if(off==kPxSact)return mode==3?1:0;
    if(off==kPxTfd){if(!issued)return mode==4?0x80:0x40;
        return mode==5?0x41:mode==6?0x60:mode==7?0x80:0x40;}
    if(off==kPxIs)return issued&&mode==8?0x40000000u:issued&&mode==9?0x20000000u:0;
    return 0;
    }
static void Wr32(TUint32 addr,TUint32 val)
    {
    TUint off=(addr-gAbar-kPortBase)%kPortStride;
    if(off==kPxCi){CHECK(val==2);++issued;issuedPort=(addr-gAbar-kPortBase)/kPortStride;}
    }
}
TBool PciFindClass(unsigned,unsigned,unsigned*,unsigned*,unsigned*){return false;}
TUint32 PciRead32(unsigned,unsigned,unsigned,unsigned){return 0;}
void PciWrite32(unsigned,unsigned,unsigned,unsigned,TUint32){}
void KPrintf(const char*,...){}
int main()
    {
    gInit=true;gAbar=0x1000;
    for(int d=0;d<4;++d){gPorts[d].present=true;gPorts[d].port=d;gPorts[d].lba48=(d&1)!=0;}
    for(int d=0;d<4;++d){mode=0;issued=0;CHECK(AhciFlush(d)==0&&issued==1&&issuedPort==(TUint)d);
        CHECK(gCt[d][2]==((d&1)?0xea:0xe7));CHECK(gClb[d][32+2]==0&&!(gClb[d][32]&0x40));}
    for(int m=1;m<=9;++m){mode=m;issued=0;TInt r=AhciFlush(0);
        CHECK(r==((m==1||m==3)?KErrInUse:(m==2||m==4||m==7)?KErrTimedOut:KErrGeneral));}
    mode=0;issued=0;gPorts[0].present=false;CHECK(AhciFlush(0)==KErrNotReady&&issued==0);
    CHECK(AhciFlush(-1)==KErrArgument&&AhciFlush(4)==KErrArgument);
    std::puts("PASS actual AHCI flush FIS/port/completion/errors/timeouts");
    }
