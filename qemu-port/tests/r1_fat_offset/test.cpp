// Actual fat32.cpp compiled once into this TU; no model/reimplementation.
#include <x86.h>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#define __QEMU_VARIANT_H__
TInt AtaInit(); TBool AtaPresent(int); TUint32 AtaSectors(int);
TInt AtaRead(int,TUint32,TUint,TUint8*);
TInt AtaWrite(int,TUint32,TUint,const TUint8*);
TInt AtaFlush(int);
void VgaTeletype(char); void KPrintf(const char*,...); TInt QemuRtcSeconds();
TInt FatCd(const char*); TInt FatUse(char); TInt FatMount(int,int);
TInt FatCdPath(const char*);
#include "../../variant/fat32.cpp"
#undef AtaWrite

static std::vector<TUint8> disk[2];
static int faultRead=-1,faultWrite=-1,reads=0,writes=0,flushDev=-1,flushResult=0;
static TUint32 badReadLba=0xffffffffu,badWriteLba=0xffffffffu;
static void Check(bool x,int line) { if(!x) { std::fprintf(stderr,"FAIL line %d\n",line);std::exit(1); } }
#define CHECK(x) Check((x),__LINE__)
TInt AtaInit(){return 0;} TBool AtaPresent(int d){return d>=0&&d<2;}
TUint32 AtaSectors(int d){return disk[d].size()/512;}
TInt AtaRead(int d,TUint32 l,TUint n,TUint8* p)
    {
    CHECK(d>=0&&d<2);CHECK((l+n)*512<=disk[d].size());
    if(reads++==faultRead || l==badReadLba) return KErrGeneral;
    std::memcpy(p,&disk[d][l*512],n*512);return 0;
    }
TInt AtaWrite(int d,TUint32 l,TUint n,const TUint8* p)
    {
    CHECK(d>=0&&d<2);CHECK((l+n)*512<=disk[d].size());
    if(writes++==faultWrite || l==badWriteLba) return KErrGeneral;
    std::memcpy(&disk[d][l*512],p,n*512);return 0;
    }
void VgaTeletype(char){} void QemuUart::PutC(char){}
void KPrintf(const char*,...){} TInt QemuRtcSeconds(){return -1;}
static TInt FlushDrive(TInt d){flushDev=d;return flushResult;}
TInt AtaFlush(TInt d){return FlushDrive(d);}
static void Reset(TUint spc=1,TUint clusters=200)
    {
    faultRead=faultWrite=-1;reads=writes=0;badReadLba=badWriteLba=0xffffffffu;
    flushResult=0;flushDev=-1;
    for(int d=0;d<2;++d)
        {
        disk[d].assign((32+spc*clusters)*512,0);
        VolState v={};v.mounted=true;v.letter='C'+d;v.dev=d;v.fatLba=3;
        v.partLba=1;v.fatSz=2;v.dataLba=8;v.secPerClus=spc;v.numFats=2;
        v.rootClus=v.cwdClus=2;v.totalClus=clusters;
        sVols[d]=v;sStorageFailed[d]=false;sWriteAttempts[d]=0;
        for(int f=0;f<2;++f) Wr32(&disk[d][(3+f*2)*512+8],kEoc);
        Wr32(&disk[d][2*512],0x41615252u);Wr32(&disk[d][2*512+484],0x61417272u);
        Wr32(&disk[d][2*512+488],clusters-1);
        }
    for(int i=2;i<8;++i) sVols[i].mounted=false;
    sCur=0;LoadCur();
    }
static R1Fat::File Create(char drive='D',const char* path="/TEST.BIN")
    { R1Fat::File f;CHECK(R1Fat::Open(drive,path,true,f)==0);CHECK(sCur==0&&sDev==0);return f; }
static void Basic(TUint spc)
    {
    Reset(spc); R1Fat::File f=Create();
    std::vector<TUint8> data(65537),out(65537);
    for(TUint i=0;i<data.size();++i) data[i]=(TUint8)(i*17+9);
    TUint done=999;CHECK(R1Fat::Write(f,900,data.empty()?0:&data[0],data.size(),done)==0);
    CHECK(done==65537);CHECK(sCur==0&&sDev==0);
    CHECK(R1Fat::Read(f,0,&out[0],out.size(),done)==0&&done==65537&&data==out);
    const TUint points[]={0,1,511,512,513,1023,1024,65535,65536,65537};
    for(TUint j=0;j<sizeof(points)/sizeof(points[0]);++j)
        {
        TUint pos=points[j];TUint8 buf[5]={};
        CHECK(R1Fat::Read(f,pos,buf,5,done)==0);
        TUint expect=65537-pos;if(expect>5)expect=5;CHECK(done==expect);
        for(TUint i=0;i<done;++i)CHECK(buf[i]==data[pos+i]);
        }
    TUint8 patch[3]={1,2,3};CHECK(R1Fat::Write(f,511,patch,3,done)==0&&done==3);
    CHECK(R1Fat::Read(f,510,&out[0],5,done)==0&&out[0]==data[510]&&out[1]==1&&out[3]==3&&out[4]==data[514]);
    CHECK(R1Fat::Resize(f,513)==0);TUint32 size=0;CHECK(R1Fat::Size(f,size)==0&&size==513);
    CHECK(R1Fat::Resize(f,65537)==0);CHECK(R1Fat::Size(f,size)==0&&size==65537);
    CHECK(R1Fat::Read(f,0,&out[0],511,done)==0&&done==511);
    for(TUint i=0;i<511;++i)CHECK(out[i]==data[i]);
    CHECK(R1Fat::Flush(f)==0&&flushDev==1);CHECK(R1Fat::Flush(f,FlushDrive)==0&&flushDev==1);
    CHECK(R1Fat::Resize(f,0)==0);CHECK(R1Fat::Read(f,0,&out[0],1,done)==0&&done==0);
    CHECK(R1Fat::Write(f,0xffffffffu,patch,3,done)==0&&done==3); // EOF clamp
    CHECK(R1Fat::Size(f,size)==0&&size==3);
    R1Fat::Close(f);CHECK(R1Fat::Size(f,size)==KErrBadHandle);
    CHECK(R1Fat::Open('D',"/TEST.BIN",false,f)==0);CHECK(R1Fat::Size(f,size)==0&&size==3);
    CHECK(R1Fat::Open('D',"/X",true,f)==KErrInUse);R1Fat::Close(f);
    CHECK(R1Fat::Open('D',"/TEST.BIN",true,f)==KErrAlreadyExists);
    CHECK(R1Fat::Open('D',"/../X",true,f)==KErrArgument);
    CHECK(R1Fat::Open('D',"/BAD NAME",true,f)==KErrArgument);
    CHECK(R1Fat::Open('D',"/MISSING/X",false,f)==KErrNotFound);
    CHECK(sCur==0&&sDev==0);
    }
static void Failures()
    {
    Reset();R1Fat::File f=Create();TUint8 data[1024];std::memset(data,7,sizeof(data));TUint n;
    CHECK(R1Fat::Write(f,0,data,1024,n)==0);
    // Partial data read: first sector copied, second fails.
    badReadLba=ClusLba(4); // C and D geometry same, file allocated 3,4
    CHECK(R1Fat::Read(f,0,data,1024,n)==KErrGeneral&&n==512);
    badReadLba=0xffffffffu;CHECK(sCur==0&&sDev==0);
    // Metadata failure after data written: returned bytes do not mean published.
    badWriteLba=ClusLba(2);
    CHECK(R1Fat::Write(f,0,data,1,n)==KErrGeneral&&n==1);
    badWriteLba=0xffffffffu;TUint32 size;
    CHECK(R1Fat::Size(f,size)==KErrCorrupt); // volume fail-stop, including other handles
    CHECK(R1Fat::Flush(f,FlushDrive)==KErrCorrupt&&flushDev==-1);
    CHECK(sCur==0&&sDev==0);
    Reset();f=Create();flushResult=KErrGeneral;
    CHECK(R1Fat::Flush(f,FlushDrive)==KErrGeneral&&flushDev==1);
    CHECK(R1Fat::Size(f,size)==KErrCorrupt);
    Reset(1,4);f=Create(); // root consumes one; only 3 data clusters free
    CHECK(R1Fat::Write(f,0,data,1024,n)==0);
    CHECK(R1Fat::Write(f,1024,data,1024,n)==KErrDiskFull&&n==512);
    CHECK(sCur==0&&sDev==0);
    Reset();f=Create();badWriteLba=5; // second FAT copy
    CHECK(R1Fat::Resize(f,1)==KErrGeneral);CHECK(R1Fat::Size(f,size)==KErrCorrupt);
    Reset();f=Create();CHECK(R1Fat::Resize(f,1024)==0);
    badWriteLba=3; // detach/free FAT failure after shrink publication
    CHECK(R1Fat::Resize(f,1)==KErrGeneral);CHECK(R1Fat::Size(f,size)==KErrCorrupt);
    Reset();f=Create();faultRead=reads;
    CHECK(R1Fat::Write(f,0,data,1,n)==KErrGeneral&&n==0);faultRead=-1;
    CHECK(R1Fat::Size(f,size)==0&&size==0); // read-only preflight error not poison
    Reset();f=Create();badWriteLba=2; // FSInfo invalidation must propagate
    CHECK(R1Fat::Resize(f,1)==KErrGeneral);CHECK(R1Fat::Size(f,size)==KErrCorrupt);
    }
static void Legacy()
    {
    Reset(1,4);const TUint8 old[]={4,5,6};TUint8 out[3];TUint n;
    CHECK(FatWrite("OLD.BIN",old,3)==0);
    TUint8 huge[2048]={};CHECK(FatWrite("OLD.BIN",huge,2048)==KErrNoMemory);
    CHECK(FatRead("OLD.BIN",out,3,n)==0&&n==3&&std::memcmp(out,old,3)==0);
    CHECK(FatRead("OLD.BIN",out,2,n)==KErrTooBig);
    }
static void More()
    {
    Reset();CHECK(FatMkdir("SUB")==0);CHECK(FatCd("SUB")==0);SaveCur();
    TUint32 cwd=sCwdClus;R1Fat::File f=Create('C',"/SUB/ONE.BIN");
    CHECK(sCwdClus==cwd&&sVols[0].cwdClus==cwd);
    R1Fat::Close(f);CHECK(R1Fat::Open('D',"/SUB/ONE.BIN",false,f)==KErrNotFound);
    CHECK(sCur==0&&sCwdClus==cwd);
    Reset(); // force directory extension, and preserve logical end over garbage
    for(int i=0;i<20;++i)
        {char path[13];std::sprintf(path,"/F%d.BIN",i);f=Create('D',path);R1Fat::Close(f);}
    CHECK(R1Fat::Open('D',"/F19.BIN",false,f)==0);R1Fat::Close(f);
    Reset();f=Create();TUint n;TUint8 data[1024]={};CHECK(R1Fat::Write(f,0,data,1024,n)==0);
    CHECK(Rd32(&disk[1][2*512+488])==0xffffffffu);
    badWriteLba=ClusLba(4);CHECK(R1Fat::Write(f,0,data,1024,n)==KErrGeneral&&n==512);
    CHECK(sCur==0);
    // Actual corrupt chain cycle rejected in bounded preflight, not a hang.
    Reset();f=Create();CHECK(R1Fat::Resize(f,1)==0);
    for(int i=0;i<2;++i)Wr32(&disk[1][(3+2*i)*512+3*4],3);
    TUint32 size;CHECK(R1Fat::Size(f,size)==KErrCorrupt);
    // Exhaust each write-failure position in a growth operation. Every error
    // must poison the API volume and restore the selected drive/CWD.
    for(int fail=0;fail<16;++fail)
        {
        Reset();f=Create();faultWrite=writes+fail;
        TInt r=R1Fat::Write(f,0,data,1024,n);
        CHECK(sCur==0&&sDev==0&&n<=1024);
        if(r) {CHECK(r==KErrGeneral);CHECK(R1Fat::Size(f,size)==KErrCorrupt);}
        else CHECK(n==1024);
        }
    }
static void Quarantine()
    {
    Reset();R1Fat::File f=Create();TUint8 data[1]={9};TUint n;
    flushResult=KErrGeneral;CHECK(R1Fat::Flush(f)==KErrGeneral);
    CHECK(sCur==0);int before=writes;
    CHECK(FatUse('D')==0);
    CHECK(FatWrite("X",data,1)==KErrCorrupt);
    CHECK(FatDel("TEST.BIN")==KErrCorrupt);
    CHECK(FatRename("TEST.BIN","NEW.BIN")==KErrCorrupt);
    CHECK(FatMkdir("SUB")==KErrCorrupt);CHECK(FatFormat(1)==KErrCorrupt);
    CHECK(writes==before);
    // A new letter/partition alias on the same device cannot escape quarantine.
    sVols[2]=sVols[1];sVols[2].letter='E';sVols[2].partLba=2;
    CHECK(FatUse('E')==0);CHECK(FatWritePath("/X",data,1)==KErrCorrupt);
    FatDropDev(1);CHECK(sStorageFailed[1]);
    sVols[1]=sVols[2];sVols[1].mounted=true;sVols[1].letter='D';
    CHECK(R1Fat::Open('D',"/X",true,f)==KErrInUse);R1Fat::Close(f);
    CHECK(R1Fat::Open('D',"/X",true,f)==KErrCorrupt);
    CHECK(FatFormat(1)==KErrCorrupt&&writes==before);
    CHECK(FatUse('C')==0);CHECK(FatWrite("OK",data,1)==0); // healthy device
    // Create completes writes then fails its final entry read: poison persists.
    Reset();R1Fat::File x;
    // Sweep all read positions, checking any failed operation after an attempted
    // write is quarantined (including end-marker and directory extension cases).
    for(int extension=0;extension<2;++extension)
        for(int fail=0;fail<32;++fail)
            {
            Reset();
            if(extension) for(int i=0;i<16;++i)
                {char p[13];std::sprintf(p,"/F%d",i);x=Create('D',p);R1Fat::Close(x);}
            int wb=writes;faultRead=reads+fail;
            TInt r=R1Fat::Open('D',"/LATE",true,x);
            CHECK(sCur==0);
            if(r && writes>wb) {CHECK(sStorageFailed[1]);CHECK(FatUse('D')==0);
                CHECK(FatWrite("NO",data,1)==KErrCorrupt);}
            R1Fat::Close(x);
            }
    Reset();f=Create();CHECK(R1Fat::Write(f,0,data,1,n)==0);
    CHECK(!sStorageFailed[1]); // successful metadata writes must not poison
    }
int main(){Basic(1);Basic(2);Failures();Legacy();More();Quarantine();std::puts("PASS actual fat32.cpp offset/resize/fault/legacy/quarantine tests");}
