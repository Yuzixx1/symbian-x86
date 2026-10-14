// genuine_glue.cpp — variant/hardware glue for the genuine nanokernel.
//
// Phase 8 Step 2: satisfies the genuine link contract WITHOUT touching
// SymbianSource. Every definition here is either:
//   (a) a call into existing QEMU implementations (qemu_assp.o: COM1/PIC/PIT),
//   (b) a freestanding primitive (bump allocator, COM1 formatter, halt), or
//   (c) storage the genuine bootstrap owns (RomHeader/SuperPage addresses).
// No Symbian types, scheduler logic, or kernel algorithms live here.
//
// Prototypes come from the genuine headers (<x86.h> -> <nkern.h>/<nk_priv.h>)
// so mangled names match the genuine .o files exactly. QEMU methods are
// redeclared with plain unsigned/int (identical mangling to variant's TUint)
// because variant/qemu_assp.h's own TUint typedefs would clash textually
// with genuine e32def.h — qemu_assp.h is therefore never included here.

#include <x86.h>
#include <nk_trace.h>
#include <stdarg.h>

// QEMU implementations (defined in variant/qemu_assp.cpp, linked in).
// Declarations mirror qemu_assp.h with builtin types only.
class QemuAsic
    {
public:
    static void Init1();
    static void Init3();
    };
namespace QemuUart
    {
    void Init();
    void PutC(char aC);
    void Puts(const char* aS);
    }
namespace QemuPit
    {
    void Setup(unsigned aHz);
    }
namespace QemuIrq
    {
    void RemapPic(unsigned char aMasterBase, unsigned char aSlaveBase);
    void Enable(int aIrq);
    }

// Asm entry points: Irq0Stub (variant/irq_gas.S, re-linked for Phase 9),
// InstallIDTEntry (boot/ported/bootcpu_gas.S: EAX=vector, ECX=handler).
// Irq0Handler_C lives below; Irq1Handler_C in genuine_console.cpp.
extern "C"
    {
    void Irq0Stub(void);
    void InstallIDTEntry(void);
    void Irq0Handler_C(void);
    void* memclr(void* aTrg, unsigned aLength);
    }

// Dual display: every KPrintf char goes to COM1 and the VGA teletype
// (variant/genuine_console.cpp). Declared here, defined there.
void VgaTeletype(char aC);

static void Emit(char aC)
    {
    QemuUart::PutC(aC);
    VgaTeletype(aC);
    }

static void EmitS(const char* aS)
    {
    while (*aS) Emit(*aS++);
    }

// Storage owned by the genuine bootstrap (_E32Startup_Body fills the two
// addresses from the values ResetEntry pushes; reserves live below).
extern "C"
    {
    TLinAddr RomHeaderAddress;
    TLinAddr SuperPageAddress;
    }

// Push targets for ResetEntry (bootmain_gas.S): zeroed reserves until the
// ROM/SuperPage synthesis moves over from rom_image.cpp.
TUint8 qemu_rom_header_reserve[1024];
TUint8 qemu_superpage_reserve[4096];

// Freestanding bump allocator for nk_entry.cpp operator new/delete.
namespace
    {
    TUint8 gHeapPool[131072];
    TUint gHeapUsed = 0;
    }

extern "C"
    {
    TAny* malloc(TUint32 aSize)
        {
        TUint size = (TUint)((aSize + 7u) & ~7u);
        if (gHeapUsed + size > sizeof(gHeapPool))
            return 0;
        TAny* p = &gHeapPool[gHeapUsed];
        gHeapUsed += size;
        return p;
        }
    void free(TAny* /*aCell*/)
        {
        }
    }

// Genuine variant entry: runs the proven QEMU PIC/PIT/UART sequence.
extern "C" void HwInit0()
    {
    QemuUart::Init();
    QemuIrq::RemapPic(0x20, 0x28);
    QemuPit::Setup(1000);
    QemuAsic::Init3();
    }

// Minimal COM1 formatter: %s %c %d %i %u %x %X %p %% plus width/0-pad.
static void PutUDec(TUint aV)
    {
    char b[10];
    int n = 0;
    do
        {
        b[n++] = (char)('0' + aV % 10u);
        aV /= 10u;
        }
    while (aV && n < 10);
    while (n--)
        Emit(b[n]);
    }

static void PutHexPad(TUint aV, int aWidth, char aPad)
    {
    char b[8];
    int n = 0;
    do
        {
        TUint d = aV & 0xFu;
        b[n++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
        aV >>= 4;
        }
    while (aV && n < 8);
    int digits = n;                   /* print loop covers digits only ... */
    while (n < aWidth)                /* ... while pads emit immediately */
        {
        Emit(aPad);
        ++n;
        }
    n = digits;
    while (n--)
        Emit(b[n]);
    }

void KPrintf(const char* aFmt, ...)
    {
    va_list ap;
    va_start(ap, aFmt);
    for (const char* p = aFmt; *p; ++p)
        {
        if (*p != '%')
            {
            Emit(*p);
            continue;
            }
        ++p;
        char pad = ' ';
        int width = 0;
        if (*p == '0')
            {
            pad = '0';
            ++p;
            }
        while (*p >= '0' && *p <= '9')
            width = width * 10 + (*p++ - '0');
        switch (*p)
            {
        case 's':
            EmitS(va_arg(ap, const char*));
            break;
        case 'c':
            Emit((char)va_arg(ap, int));
            break;
        case 'd':
        case 'i':
            {
            int v = va_arg(ap, int);
            if (v < 0)
                {
                Emit('-');
                v = -v;
                }
            PutUDec((TUint)v);
            }
            break;
        case 'u':
            PutUDec(va_arg(ap, TUint));
            break;
        case 'x':
        case 'X':
        case 'p':
            PutHexPad(va_arg(ap, TUint), width, pad);
            break;
        case '%':
            Emit('%');
            break;
        default:
            Emit('%');
            Emit(*p);
            break;
            }
        }
    va_end(ap);
    }

static void HaltForever()
    {
    for (;;)
        {
        __asm__ __volatile__("cli; hlt");
        }
    }

void NKFault(const char* aFile, TInt aLine)
    {
    KPrintf("NKFault %s:%u\r\n", aFile, (TUint)aLine);
    HaltForever();
    }

void NKCrashHandler(TInt aPhase, const TAny* a0, TInt a1)
    {
    CrashState = 1;
    KPrintf("NKCrash phase=%x a0=%08x a1=%08x\r\n", aPhase, (TUint)a0, (TUint)a1);
    HaltForever();
    }

// Phase 9: genuine scheduler bring-up. Null (idle, prio 0) via NKern::Init,
// one worker (prio 10) via NKern::ThreadCreate, first switch via genuine
// TScheduler::Reschedule. PIT IRQ0 feeds NTimerQ::Tick (see Irq0Handler_C).
// (Phase 10: worker replaced by the shell thread; see genuine_console.cpp.)
static NThread gNullThread;

void KbdTakeover();
NThread* ShellThread();
void ShellSpawn();

extern "C" void Irq0Handler_C(void)
    {
    // Genuine tick intake: `this' is TheTimerQ (nk_timer.cpp:432-439).
    NTimerQ* q = (NTimerQ*)NTimerQ::TimerAddress();
    q->Tick();
    }

typedef void (*CtorFn)(void);
extern "C"
    {
    extern CtorFn __init_array_start[];
    extern CtorFn __init_array_end[];
    }

static void QemuRunStaticCtors()
    {
    // Freestanding replacement for CRT startup: runs TScheduler/TheTimerQ/
    // TDfc constructors that GCC16 emits into .init_array (win32crt covers
    // only the legacy __CTOR_LIST__, which we PROVIDE empty).
    for (CtorFn* p = __init_array_start; p != __init_array_end; ++p)
        (*p)();
    }

static void QemuInstallIrq0()
    {
    // IDT[0x20] = Irq0Stub (genuine-empty slot; cf. QemuTimerIrq::Takeover).
    __asm__ __volatile__(
        "movl $0x20, %%eax\n\t"
        "movl %0, %%ecx\n\t"
        "call InstallIDTEntry"
        :: "r"(&Irq0Stub) : "eax", "ecx", "edx", "memory");
    QemuIrq::Enable(0);
    }

// KernelMain: nanokernel-only bring-up (full Kern personality is future work).
// Runs as the null thread after _E32Startup_Body. The null thread doubles as
// the idle loop: whenever no higher-priority thread is ready, the CPU hlt's
// here (interrupts enabled, kernel unlocked) until the next IRQ.
extern "C" void KernelMain()
    {
    QemuRunStaticCtors();
    NKern::Init0(0);
    X86::DefaultCR0 = X86::GetCR0();   // mirrors vectors.cpp:123
    X86_IrqNestCount = -1;             // thread context (cf. vectors.cpp:122)
    NTimerQ::Init1(1);                 // 1ms tick (cf. A::Init1, cinit.cpp:87)

    SNThreadCreateInfo nullInfo;
    memclr(&nullInfo, sizeof(nullInfo));
    NKern::Init(&gNullThread, nullInfo);

    QemuInstallIrq0();                 // ticks now safe: null is current
    KbdTakeover();                     // IDT[0x21] = Irq1Stub + unmask PS/2
    QemuIrq::Enable(1);
    ShellSpawn();                      // genuine shell NThread, prio 10

    KPrintf("GENUINE starting scheduler...\r\n");
    NKern::Lock();
    ShellThread()->Resume();
    TScheduler::Reschedule();
    // Only reached if the shell thread ever exits: report and idle.
    KPrintf("GENUINE shell exited, idling tick=%u\r\n", NKern::TickCount());
    for (;;)
        {
        __asm__ __volatile__("hlt");
        }
    }

extern "C" void ExcFault(TAny* aInfo)
    {
    KPrintf("ExcFault %08x\r\n", (TUint)aInfo);
    HaltForever();
    }

// BTrace filtering lives in kernel/sbtrace.cpp (Kern personality). Link-test
// placeholders: filtering unsupported, nothing traced.
TBool SBTraceData::CheckFilter2(TUint32 /*aUid*/)
    {
    return EFalse;
    }

TBool BTrace::IsSupported(TUint /*aCategory*/)
    {
    return EFalse;
    }
