// qemu_variant_asic.cpp — QEMU Asic variant for the full Kern image.
//
// Genuine contract: kernel/eka/include/kernel/x86/assp.h class Asic (11 pure
// virtuals) + TVariantInitialise2. This subclass drives i8259/PIC, i8254/PIT,
// COM1, CMOS RTC and VGA-text. Hardware init order mirrors genuine baseports:
// Init1 (early, IRQ masked) -> ... -> Init3 (unmask + spawn shell thread).

#include <x86.h>
#include <e32hal.h>
#include <kernel/kernel.h>
#include "qemu_variant.h"

// QEMU drivers (variant/qemu_assp.cpp, linked in): plain builtins here so the
// genuine e32def.h typedefs never clash textually (cf. genuine_glue.cpp).
namespace QemuUart
    {
    void Init();
    void PutC(char aC);
    void EnableRxIrq();
    }
namespace QemuPit
    {
    void Setup(unsigned aHz);
    }
namespace QemuIrq
    {
    void RemapPic(unsigned char aMasterBase, unsigned char aSlaveBase);
    void Enable(int aIrq);
    void Disable(int aIrq);
    }

// (VgaTeletype, X86IrqDispatcher, ShellSpawnFull shared via qemu_variant.h.)

static inline void Out8(unsigned short aPort, unsigned char aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
static inline unsigned char In8(unsigned short aPort)
    { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

class QemuVariantAsic : public Asic
    {
public:
    virtual TMachineStartupType StartupReason();
    virtual void Init1();
    virtual void Init3();
    virtual void DebugOutput(TUint aChar);
    virtual void Idle();
    virtual TInt MsTickPeriod();
    virtual TInt SystemTimeInSecondsFrom2000(TInt& aTime);
    virtual TInt SetSystemTimeInSecondsFrom2000(TInt aTime);
    virtual TUint32 NanoWaitCalibration();
    virtual TInt VariantHal(TInt aFunction, TAny* a1, TAny* a2);
    virtual TPtr8 MachineConfiguration();
    };

static QemuVariantAsic gAsic;
static TMachineConfig gMachineConfig;

TMachineStartupType QemuVariantAsic::StartupReason()
    {
    return EStartupColdReset;  // QEMU has no reset-cause register
    }

void QemuVariantAsic::Init1()
    {
    // Mask everything across the PIC remap (an IRQ through a half-built
    // mapping would land on the old bootcpu IDT with the wrong vector).
    Out8(0x21, 0xFF);
    Out8(0xA1, 0xFF);
    QemuIrq::RemapPic(0x30, 0x38);  // HW IRQs at 0x30+: 0x20/0x21 are the
                                    // genuine exec-call gates (vectors.cia)
    Out8(0x21, 0xFF);
    Out8(0xA1, 0xFF);
    QemuPit::Setup(1000);
    QemuIrqBindTickKbd();  // bind ISRs now; IRQ stays masked until Init3
    X86::SetIrqHandler((TLinAddr)&X86IrqDispatcher);
    // IRQ0/IRQ1 stay masked until Init3: NTimerQ::Init1 runs later in A::Init1
    // and a tick before then would hit an uninitialised timer queue.
    }

void QemuVariantAsic::Init3()
    {
    QemuUart::EnableRxIrq();  // RDA interrupt (ISR drains RBR to input ring)
    QemuIrq::Enable(0);       // PIT tick -> genuine NTimerQ::Tick
    QemuIrq::Enable(1);       // PS/2 keyboard
    QemuIrq::Enable(4);       // COM1 receive
    ShellSpawnFull();         // interactive console as genuine NThread
    }

void QemuVariantAsic::DebugOutput(TUint aChar)
    {
    char c = (char)(aChar & 0xFF);
    QemuUart::PutC(c);
    VgaTeletype(c);
    }

void QemuVariantAsic::Idle()
    {
    __asm__ __volatile__("sti; hlt");
    }

TInt QemuVariantAsic::MsTickPeriod()
    {
    return 1;  // PIT programmed for 1000Hz in Init1
    }

TUint32 QemuVariantAsic::NanoWaitCalibration()
    {
    // K::DoNanoWait loop iterations per microsecond-ish unit; emulated CPU is
    // slow, so keep waits short rather than accurate.
    return 10000;
    }

// --- CMOS RTC (MC146818): seconds since 2000-01-01 00:00:00 ---
static TInt CmosRead(TInt aReg)
    {
    Out8(0x70, (unsigned char)(aReg & 0x7F));  // bit7=0: NMI enabled
    return (TInt)In8(0x71);
    }

static void CmosWrite(TInt aReg, TInt aVal)
    {
    Out8(0x70, (unsigned char)(aReg & 0x7F));
    Out8(0x71, (unsigned char)aVal);
    }

static TInt Bcd(TInt aV) { return ((aV >> 4) * 10) + (aV & 0x0F); }

static TBool Leap(TInt aYear)  // full year, e.g. 2026
    { return (aYear % 4 == 0) && ((aYear % 100 != 0) || (aYear % 400 == 0)); }

static const TInt kMonthDays[12] =
    {31,28,31,30,31,30,31,31,30,31,30,31};

TInt QemuVariantAsic::SystemTimeInSecondsFrom2000(TInt& aTime)
    {
    // Wait for non-update, then snapshot all fields twice for consistency.
    for (int tries = 0; tries < 3; ++tries)
        {
        while (CmosRead(0x0A) & 0x80) { }
        TInt s1 = CmosRead(0x00), m1 = CmosRead(0x02), h1 = CmosRead(0x04);
        TInt d1 = CmosRead(0x07), mo1 = CmosRead(0x08), y1 = CmosRead(0x09);
        TInt c1 = CmosRead(0x32);
        TInt s2 = CmosRead(0x00), m2 = CmosRead(0x02), h2 = CmosRead(0x04);
        if (s1 != s2 || m1 != m2 || h1 != h2)
            continue;
        TBool bcd = !(CmosRead(0x0B) & 0x04);
        TInt sec = bcd ? Bcd(s1) : s1;
        TInt minute = bcd ? Bcd(m1) : m1;
        TInt hour = bcd ? Bcd(h1 & 0x7F) : (h1 & 0x7F);
        TInt day = bcd ? Bcd(d1) : d1;
        TInt mon = bcd ? Bcd(mo1) : mo1;
        TInt yr2 = bcd ? Bcd(y1) : y1;
        TInt cent = bcd ? Bcd(c1) : c1;
        TInt year = cent * 100 + yr2;
        if (year < 2000 || year > 2099 || mon < 1 || mon > 12 || day < 1 ||
            day > 31 || hour > 23 || minute > 59 || sec > 60)
            return KErrGeneral;
        TInt days = 0;
        for (TInt y = 2000; y < year; ++y)
            days += Leap(y) ? 366 : 365;
        for (TInt m = 1; m < mon; ++m)
            days += kMonthDays[m - 1] + ((m == 2 && Leap(year)) ? 1 : 0);
        days += day - 1;
        aTime = days * 86400 + hour * 3600 + minute * 60 + sec;
        return KErrNone;
        }
    return KErrGeneral;
    }

TInt QemuVariantAsic::SetSystemTimeInSecondsFrom2000(TInt aTime)
    {
    if (aTime < 0)
        return KErrArgument;
    TInt days = aTime / 86400;
    TInt rem = aTime % 86400;
    TInt hour = rem / 3600;
    rem %= 3600;
    TInt minute = rem / 60;
    TInt sec = rem % 60;
    TInt year = 2000;
    while (days >= (Leap(year) ? 366 : 365))
        { days -= Leap(year) ? 366 : 365; ++year; }
    TInt mon = 1;
    while (days >= kMonthDays[mon - 1] + ((mon == 2 && Leap(year)) ? 1 : 0))
        { days -= kMonthDays[mon - 1] + ((mon == 2 && Leap(year)) ? 1 : 0); ++mon; }
    TInt day = days + 1;
    // Hold update-in-progress while writing (status B bit 7).
    TInt b = CmosRead(0x0B);
    Out8(0x70, 0x8B);
    Out8(0x71, (unsigned char)(b | 0x80));
    CmosWrite(0x00, (sec / 10 << 4) | (sec % 10));
    CmosWrite(0x02, (minute / 10 << 4) | (minute % 10));
    CmosWrite(0x04, (hour / 10 << 4) | (hour % 10));
    CmosWrite(0x07, (day / 10 << 4) | (day % 10));
    CmosWrite(0x08, (mon / 10 << 4) | (mon % 10));
    CmosWrite(0x09, (((year % 100) / 10) << 4) | ((year % 100) % 10));
    CmosWrite(0x32, (((year / 100) / 10) << 4) | ((year / 100) % 10));
    Out8(0x70, 0x8B);
    Out8(0x71, (unsigned char)(b & ~0x80));
    return KErrNone;
    }

TInt QemuVariantAsic::VariantHal(TInt /*aFunction*/, TAny* /*a1*/, TAny* /*a2*/)
    {
    return KErrNotSupported;
    }

TPtr8 QemuVariantAsic::MachineConfiguration()
    {
    return TPtr8((TUint8*)&gMachineConfig, sizeof(gMachineConfig),
                 sizeof(gMachineConfig));
    }

// Shell `date` helper: RTC seconds since 2000, or -1 when unreadable.
extern "C" TInt QemuRtcSeconds()
    {
    TInt s = 0;
    if (gAsic.SystemTimeInSecondsFrom2000(s) != KErrNone)
        return -1;
    return s;
    }

// Shell `date ...` helper: set RTC; genuine Asic validates range.
extern "C" TInt QemuSetRtcSeconds(TInt aSeconds)
    {
    return gAsic.SetSystemTimeInSecondsFrom2000(aSeconds);
    }

// --- ROM variant entry points (called from P::CreateVariant path) ---
#include <kernel/kern_priv.h>

static SRamBank gRamBanks[2];

extern "C" TInt QemuVariantEntry(TInt /*aReason*/)
    {
    // SuperPage fields the genuine boot reads AFTER variant init (only
    // iActiveVariant is read before, and 0 is compatible with our images).
    TSuperPage& sp = TheSuperPage();
    QRomBlob* r = (QRomBlob*)::RomHeaderAddress;
    sp.iActiveVariant = 0;
    sp.iPrimaryEntry = (TLinAddr)&r->kernEntry;
    sp.iInitialHeapSize = kQemuInitialHeapSize;
    sp.iRamBase = 0;
    sp.iTotalRamSize = kQemuTotalRamSize;
    sp.iKernelLimit = kQemuKernDataAddress + kQemuKernRegionSize;
    sp.iRootDirList = (TLinAddr)&r->rootList;
    sp.iHwStartupReason = 0;
    gRamBanks[0].iBase = 0;
    gRamBanks[0].iSize = kQemuTotalRamSize;
    gRamBanks[1].iBase = 0;
    gRamBanks[1].iSize = 0;
    sp.iRamBootData = (TLinAddr)&gRamBanks[0];
    for (TUint i = 0; i < (TUint)KNumTraceMaskWords; ++i)
        sp.iDebugMask[i] = 0;
    sp.iDebugMask[0] = (1u << 1);  // KBOOT only (nk_trace.h: KBOOT=1):
                                   // boot progress without scheduler chatter
    return 0;  // aMode=0: no per-mode VariantInitialise calls
    }

extern "C" TAny* QemuVariantInitialise(TInt /*aMode*/)
    {
    return (TAny*)&gAsic;
    }
