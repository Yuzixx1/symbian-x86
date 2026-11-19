#include <x86.h>
#include <cstdio>
#include <cstdlib>
#define __QEMU_VARIANT_H__
#define R1_STORAGE_HOST_TEST
TBool AhciPresent(int);TInt AhciCount();TUint32 AhciSectors(int);const char* AhciModel(int);
TInt AhciRead(int,TUint32,TUint,TUint8*);TInt AhciWrite(int,TUint32,TUint,const TUint8*);
TInt AhciFlush(int);
#include "../../variant/qemu_ata.cpp"
static int mode=0,issued=0,selected=-1,route=-1,routeResult=0;
static unsigned channel=0;static int polls=0;
static void Check(bool ok,int n){if(!ok){std::fprintf(stderr,"ATA fail %d\n",n);std::exit(1);}}
#define CHECK(x) Check((x),__LINE__)
static void Out8(unsigned port,unsigned char v)
    {
    if(port==0x1f6||port==0x176){selected=(v&0x10)?1:0;channel=port-6;}
    if(port==0x1f7||port==0x177){CHECK(v==0xe7);++issued;polls=0;}
    }
static void Out16(unsigned,unsigned short){CHECK(false);}
static unsigned short In16(unsigned){CHECK(false);return 0;}
static unsigned char In8(unsigned port)
    {
    if(port==0x3f6||port==0x376)return 0x40;
    if(mode==5)return 0x80; // busy before issue
    if(!issued)return mode==6?0:0x40;
    if(mode==1)return 0x41;
    if(mode==2)return 0x60;
    if(mode==3)return 0x80;
    if(mode==4)return 0x48;
    return polls++<3?0x80:0x40;
    }
TInt AhciFlush(int d){route=d;return routeResult;}
TBool AhciPresent(int){return false;}TInt AhciCount(){return 0;}
TUint32 AhciSectors(int){return 0;}const char* AhciModel(int){return "";}
TInt AhciRead(int,TUint32,TUint,TUint8*){return -1;}
TInt AhciWrite(int,TUint32,TUint,const TUint8*){return -1;}
int main()
    {
    gProbed=1;for(int i=0;i<4;++i){gDevs[i].present=true;gDevs[i].atapi=false;}
    for(int d=0;d<4;++d){issued=0;mode=0;CHECK(AtaFlush(d)==0);CHECK(issued==1&&selected==d%2&&channel==(d/2?0x170u:0x1f0u));}
    for(int m=1;m<=6;++m){mode=m;issued=0;TInt r=AtaFlush(0);
        CHECK(r==(m<=2?KErrGeneral:m==6?KErrNotReady:KErrTimedOut));}
    mode=0;issued=0;gDevs[0].atapi=true;CHECK(AtaFlush(0)==KErrNotReady&&issued==0);
    for(int d=4;d<8;++d){routeResult=KErrGeneral;CHECK(AtaFlush(d)==KErrGeneral&&route==d-4);}
    CHECK(AtaFlush(-1)==KErrArgument&&AtaFlush(8)==KErrArgument);
    std::puts("PASS actual ATA flush selection/routing/ERR/DF/timeouts");
    }
