// genuine_console.cpp — interactive console on genuine NThreads (Phase 10).
//
// VGA 80x25 teletype at 0xB8000, PS/2 Set-1 decoder with ISR ring buffer,
// COM1 RX poll, and the EKA2> shell thread (prio 10, 8K stack). All output
// goes through KPrintf (COM1+VGA, see genuine_glue.cpp); logic follows the
// proven Phase-6 drivers, re-expressed with genuine EKA2 types.

#include <x86.h>
#include <nk_trace.h>

static inline void Out8(unsigned short aPort, TUint8 aVal)
    { __asm__ __volatile__("outb %0,%1" : : "a"(aVal), "Nd"(aPort)); }
static inline TUint8 In8(unsigned short aPort)
    { TUint8 v; __asm__ __volatile__("inb %1,%0" : "=a"(v) : "Nd"(aPort)); return v; }

extern "C" void InstallIDTEntry(void);  // bootcpu_gas.o: EAX=vector ECX=handler
extern "C" void Irq1Stub(void);         // variant/irq_gas.S

// ---------------------------------------------------------------- VGA teletype
// Called by KPrintf for every char (COM1 already handled there). Safe from
// the first KPrintf: position starts 0,0 and SeaBIOS leaves text mode on.
static volatile unsigned short* const kVram =
    reinterpret_cast<volatile unsigned short*>(0xB8000u);
static const TUint kCols = 80, kRows = 25, kAttr = 0x07;

namespace
    {
    TUint sCol = 0, sRow = 0;

    void MoveCursor()
        {
        TUint pos = sRow * kCols + sCol;
        Out8(0x3D4, 0x0F);
        Out8(0x3D5, (TUint8)(pos & 0xFF));
        Out8(0x3D4, 0x0E);
        Out8(0x3D5, (TUint8)((pos >> 8) & 0xFF));
        }

    void Scroll()
        {
        for (TUint r = 1; r < kRows; ++r)
            for (TUint c = 0; c < kCols; ++c)
                kVram[(r - 1) * kCols + c] = kVram[r * kCols + c];
        for (TUint c = 0; c < kCols; ++c)
            kVram[(kRows - 1) * kCols + c] = (unsigned short)((kAttr << 8) | ' ');
        }

    void NewLine()
        {
        sCol = 0;
        if (++sRow >= kRows)
            {
            sRow = kRows - 1;
            Scroll();
            }
        MoveCursor();
        }
    }

void VgaTeletype(char aC)
    {
    if (aC == '\n') { NewLine(); return; }
    if (aC == '\r') { sCol = 0; MoveCursor(); return; }
    if (aC == '\b')
        {
        if (sCol > 0)
            {
            --sCol;
            kVram[sRow * kCols + sCol] = (unsigned short)((kAttr << 8) | ' ');
            MoveCursor();
            }
        return;
        }
    kVram[sRow * kCols + sCol] = (unsigned short)((kAttr << 8) | (TUint8)aC);
    if (++sCol >= kCols)
        NewLine();
    else
        MoveCursor();
    }

static void VgaInit()
    {
    for (TUint i = 0; i < kCols * kRows; ++i)
        kVram[i] = (unsigned short)((kAttr << 8) | ' ');
    sCol = 0;
    sRow = 0;
    MoveCursor();
    }

// ------------------------------------------------------- PS/2 Set-1 keyboard
static const char kMap[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']', '\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

static char Shifted(char aC)
    {
    switch (aC)
        {
        case '1': return '!'; case '2': return '@'; case '3': return '#';
        case '4': return '$'; case '5': return '%'; case '6': return '^';
        case '7': return '&'; case '8': return '*'; case '9': return '(';
        case '0': return ')'; case '-': return '_'; case '=': return '+';
        case '[': return '{'; case ']': return '}'; case ';': return ':';
        case '\'': return '"'; case '`': return '~'; case '\\': return '|';
        case ',': return '<'; case '.': return '>'; case '/': return '?';
        default:
            if (aC >= 'a' && aC <= 'z') return (char)(aC - 32);
            return aC;
        }
    }

static volatile TUint8 sRing[256];
static volatile TUint sHead = 0, sTail = 0;  // single producer (ISR), single consumer
static volatile TUint8 sShift = 0;
static volatile TUint8 sE0 = 0;

static void Push(char aC)
    {
    TUint next = (sHead + 1) & 255u;
    if (next == sTail) return;  // full: drop oldest policy = drop newest
    sRing[sHead] = (TUint8)aC;
    sHead = next;
    }

// Runs in IRQ1 context. Drains the 8042 output buffer.
extern "C" void Irq1Handler_C(void)
    {
    for (int i = 0; i < 8; ++i)
        {
        if (!(In8(0x64) & 0x01)) break;
        TUint8 sc = In8(0x60);
        if (sc == 0xE0) { sE0 = 1; continue; }
        if (sE0) { sE0 = 0; continue; }  // ignore extended keys
        if (sc == 0x2A || sc == 0x36) { sShift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { sShift = 0; continue; }
        if (sc & 0x80) continue;         // key release
        if (sc >= 128) continue;
        char c = kMap[sc];
        if (c == 0) continue;
        if (sShift) c = Shifted(c);
        Push(c);
        }
    }

static int KbdPoll()
    {
    if (sHead == sTail) return -1;
    char c = (char)sRing[sTail];
    sTail = (sTail + 1) & 255u;
    return (int)(TUint8)c;
    }

static int SerialPoll()
    {
    if (!(In8(0x3FD) & 0x01)) return -1;  // LSR.DR
    return (int)In8(0x3F8);
    }

// ------------------------------------------------------------------- shell
extern "C" TUint qemu_cpu_features;  // bootcpu_gas.o BSS (CPUID report)

static char sLine[128];
static TUint sLen = 0;

static int StrEq(const char* a, const char* b)
    { while (*a && *a == *b) { ++a; ++b; } return *a == *b; }

static void CmdHelp()
    {
    KPrintf("commands: help version uptime reboot\r\n");
    }

static void CmdVersion()
    {
    KPrintf("Symbian EKA2 microkernel (genuine), x86 bare-metal on QEMU i386\r\n");
    TUint cr0;
    __asm__ __volatile__("mov %%cr0,%0" : "=r"(cr0));
    KPrintf("  feats=0x%x CR0=0x%x sched=TheScheduler tick=1ms\r\n", qemu_cpu_features, cr0);
    }

static void CmdUptime()
    {
    TUint ms = NKern::TickCount();
    KPrintf("uptime: %u ms (%u s)\r\n", ms, ms / 1000u);
    }

static void CmdReboot()
    {
    KPrintf("rebooting via 8042...\r\n");
    TUint32 t0 = NKern::TickCount();
    while ((TUint32)(NKern::TickCount() - t0) < 200) { }  // let serial drain
    Out8(0x64, 0xFE);                                    // pulse CPU reset line
    KPrintf("reboot failed (8042 ignored), halting\r\n");
    for (;;)
        __asm__ __volatile__("hlt");
    }

static void Exec()
    {
    sLine[sLen] = 0;
    char* cmd = sLine;
    while (*cmd == ' ') ++cmd;
    if (*cmd == 0) return;
    if (StrEq(cmd, "help")) CmdHelp();
    else if (StrEq(cmd, "version")) CmdVersion();
    else if (StrEq(cmd, "uptime")) CmdUptime();
    else if (StrEq(cmd, "reboot")) CmdReboot();
    else
        KPrintf("unknown command, try `help`\r\n");
    }

static void ShellMain(TAny* /*aPtr*/)
    {
    VgaInit();
    KPrintf("Symbian EKA2 x86 genuine nanokernel -- interactive console\r\n");
    KPrintf("VGA 80x25 + COM1, PS/2 IRQ1 + serial input, sched by TheScheduler\r\n");
    KPrintf("EKA2> ");
    for (;;)
        {
        int c = KbdPoll();
        if (c < 0) c = SerialPoll();
        if (c < 0)
            {
            // Sleep via tick checks: halt to next IRQ (tick or keystroke).
            __asm__ __volatile__("hlt");
            continue;
            }
        if (c == '\r') c = '\n';              // serial Enter
        if (c == '\n')
            {
            KPrintf("\n");
            Exec();
            sLen = 0;
            KPrintf("EKA2> ");
            }
        else if (c == '\b' || c == 127)
            {
            if (sLen > 0) { --sLen; KPrintf("\b"); }
            }
        else if (c == 27)                     // ESC clears the line
            {
            while (sLen > 0) { --sLen; KPrintf("\b"); }
            }
        else if (c >= 32 && c < 127)
            {
            if (sLen < sizeof(sLine) - 1)
                {
                sLine[sLen++] = (char)c;
                KPrintf("%c", c);
                }
            }
        }
    }

static NThread gShellThread;
static TUint8 gShellStack[8192];

void KbdTakeover()
    {
    // IDT[0x21] = Irq1Stub (genuine leaves 0x21 to the variant).
    __asm__ __volatile__(
        "movl $0x21, %%eax\n\t"
        "movl %0, %%ecx\n\t"
        "call InstallIDTEntry"
        :: "r"(&Irq1Stub) : "eax", "ecx", "edx", "memory");
    }

NThread* ShellThread()
    {
    return &gShellThread;
    }

void ShellSpawn()
    {
    // Genuine thread bring-up (cf. NKern::ThreadCreate contract: unlocked,
    // interrupts enabled, thread context — all true in null here).
    SNThreadCreateInfo info;
    for (TUint i = 0; i < sizeof(info); ++i)
        ((TUint8*)&info)[i] = 0;
    info.iFunction = ShellMain;
    info.iStackBase = gShellStack;
    info.iStackSize = sizeof(gShellStack);
    info.iPriority = 10;
    info.iTimeslice = 10;
    KPrintf("GENUINE spawning shell(pri10) on TheScheduler...\r\n");
    NKern::ThreadCreate(&gShellThread, info);
    }
