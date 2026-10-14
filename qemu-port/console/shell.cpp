// shell.cpp — EKA2> interactive shell NThread + idle NThread (Phase 6).
#include "shell.h"
#include "vga.h"
#include "kbd.h"
#include "../nk/nk_sched.h"
#include "../rom/rom_image.h"
#include "../variant/bootinfo.h"

typedef unsigned short TUint16;
static inline void Out8(TUint16 aPort, TUint8 aVal)
    { __asm__ __volatile__("outb %0,%1" : : "a"(aVal), "Nd"(aPort)); }
static inline TUint ReadCr0()
    { TUint v; __asm__ __volatile__("mov %%cr0,%0" : "=r"(v)); return v; }
extern "C" TUint qemu_cpu_features;

static TUint8 sShellStack[16384] __attribute__((aligned(16)));
static TUint8 sIdleStack[4096] __attribute__((aligned(16)));
static NThread sShellThread = {3, 10, EReady, 0, 0, 0, 10, 0};
static NThread sIdleThread = {4, 0, EReady, 0, 0, 0, 10, 0};

static void IdleMain(void* /*aArg*/)
    {
    for (;;)
        {
        __asm__ __volatile__("sti");
        __asm__ __volatile__("hlt");   // sleep until next IRQ, then yield home
        NKern::YieldTimeslice();
        }
    }

static char sLine[128];
static TUint sLen = 0;

static void Prompt() { Vga::SetAttr(Vga::kPrompt); Vga::Puts("EKA2> "); Vga::SetAttr(Vga::kText); }

static int StrEq(const char* a, const char* b)
    { while (*a && *a == *b) { ++a; ++b; } return *a == *b; }

static void CmdHelp()
    {
    Vga::Puts("commands: help version uptime threads meminfo reboot\r\n");
    }
static void CmdVersion()
    {
    Vga::Puts("Symbian EKA2 microkernel, x86 bare-metal research port (QEMU i386)\r\n");
    Vga::Puts("  cpu_features=0x"); Vga::PutHex(qemu_cpu_features);
    Vga::Puts(" CR0=0x"); Vga::PutHex(ReadCr0());
    Vga::Puts(" tick=1ms direct-mmap phases 1-6\r\n");
    }
static void CmdUptime()
    {
    TUint ms = NKern::TickCount();
    Vga::Puts("uptime: "); Vga::PutDec(ms);
    Vga::Puts(" ms ("); Vga::PutDec(ms / 1000u); Vga::Puts(" s)\r\n");
    }
static void CmdThreads()
    {
    Vga::Puts("id pri state   switches stackTop\r\n");
    for (TUint i = 0; ; ++i)
        {
        NThread* t = TScheduler::ThreadAt(i);
        if (!t) break;
        Vga::PutDec((TUint)t->iId); Vga::Puts("  ");
        Vga::PutDec((TUint)t->iPriority); Vga::Puts("   ");
        Vga::Puts(TScheduler::StateName(t->iState)); Vga::Puts("  0x");
        Vga::PutHex(t->iSwitches); Vga::Puts(" 0x");
        Vga::PutHex(t->iStackTop); Vga::Puts("\r\n");
        }
    }
static void CmdMeminfo()
    {
    TUint cr0 = ReadCr0();
    TUint cr3, cr4;
    __asm__ __volatile__("mov %%cr3,%0" : "=r"(cr3));
    __asm__ __volatile__("mov %%cr4,%0" : "=r"(cr4));
    Vga::Puts("RAM base=0x"); Vga::PutHex(0);
    Vga::Puts(" size=0x"); Vga::PutHex((TUint)QemuRom::SuperPage.iTotalRamSize);
    Vga::Puts(" ("); Vga::PutDec((TUint)QemuRom::SuperPage.iTotalRamSize >> 20); Vga::Puts("M)\r\n");
    Vga::Puts("multiboot: lower="); Vga::PutDec(BootInfo::memLowerKB);
    Vga::Puts("K upper="); Vga::PutDec(BootInfo::memUpperKB);
    Vga::Puts("K flags=0x"); Vga::PutHex(BootInfo::flags); Vga::Puts("\r\n");
    Vga::Puts("paging: CR3=0x"); Vga::PutHex(cr3);
    Vga::Puts(cr0 & 0x80000000u ? " PG=on" : " PG=OFF");
    Vga::Puts((cr4 & 0x10u) ? " PSE=on" : " PSE=off");
    Vga::Puts((cr4 & 0x80u) ? " PGE=on" : " PGE=off");
    Vga::Puts(" feats=0x"); Vga::PutHex(qemu_cpu_features); Vga::Puts("\r\n");
    Vga::Puts("kernData=0x"); Vga::PutHex(QemuRom::RomHeader.iKernDataAddress);
    Vga::Puts(" kernLimit=0x"); Vga::PutHex(QemuRom::RomHeader.iKernelLimit);
    Vga::Puts(" initHeap="); Vga::PutDec((TUint)QemuRom::SuperPage.iInitialHeapSize);
    Vga::Puts("B\r\n");
    Vga::Puts("direct map: 32x4M identity 0x0-0x8000000, VGA hole 0xA0000-0x100000 excluded\r\n");
    }

static void CmdReboot()    {
    Vga::SetAttr(Vga::kError);
    Vga::Puts("rebooting via 8042...\r\n");
    TUint t0 = NKern::TickCount();
    while (NKern::TickCount() - t0 < 200) { }  // let serial drain
    Out8(0x64, 0xFE);                          // pulse CPU reset line
    Vga::Puts("reboot failed (8042 ignored), halting\r\n");
    for (;;) __asm__ __volatile__("hlt");
    }

static void Exec()
    {
    sLine[sLen] = 0;
    // first token
    char* cmd = sLine;
    while (*cmd == ' ') ++cmd;
    if (*cmd == 0) return;
    if (StrEq(cmd, "help")) CmdHelp();
    else if (StrEq(cmd, "version")) CmdVersion();
    else if (StrEq(cmd, "uptime")) CmdUptime();
    else if (StrEq(cmd, "threads")) CmdThreads();
    else if (StrEq(cmd, "meminfo")) CmdMeminfo();
    else if (StrEq(cmd, "reboot")) CmdReboot();
    else
        {
        Vga::SetAttr(Vga::kError);
        Vga::Puts("unknown command, try `help`\r\n");
        Vga::SetAttr(Vga::kText);
        }
    }

static void ShellMain(void* /*aArg*/)
    {
    Vga::Init();
    Vga::SetAttr(Vga::kBanner);
    Vga::Puts("Symbian EKA2 x86 bare-metal port -- interactive console\r\n");
    Vga::Puts("VGA 80x25 + COM1 mirror, PS/2 IRQ1 + serial input\r\n");
    Vga::SetAttr(Vga::kText);
    Prompt();
    for (;;)
        {
        int c = Kbd::Poll();
        if (c < 0) c = Kbd::SerialPoll();
        if (c < 0) { NKern::YieldTimeslice(); continue; }  // idle sleeps
        if (c == '\r') c = '\n';                            // serial Enter
        if (c == '\n')
            {
            Vga::PutC('\n');
            Exec();
            sLen = 0;
            Prompt();
            }
        else if (c == '\b' || c == 127)
            {
            if (sLen > 0) { --sLen; Vga::PutC('\b'); }
            }
        else if (c == 27)  // ESC clears the line
            {
            while (sLen > 0) { --sLen; Vga::PutC('\b'); }
            }
        else if (c >= 32 && c < 127)
            {
            if (sLen < sizeof(sLine) - 1)
                { sLine[sLen++] = (char)c; Vga::PutC((char)c); }
            }
        }
    }

void Shell::SpawnAndEnter()
    {
    QemuUart::Puts("EKA2-QEMU NKern: spawning shell(pri10) + idle(pri0) threads...\r\n");
    sShellThread.iStackBase = (TUint)sShellStack;
    sShellThread.iStackTop = (TUint)(sShellStack + sizeof(sShellStack));
    sIdleThread.iStackBase = (TUint)sIdleStack;
    sIdleThread.iStackTop = (TUint)(sIdleStack + sizeof(sIdleStack));
    sShellThread.iSavedEsp = NThread_InitStack(sShellThread.iStackTop, ShellMain, 0);
    sIdleThread.iSavedEsp = NThread_InitStack(sIdleThread.iStackTop, IdleMain, 0);
    TScheduler::Register(&sShellThread);
    TScheduler::Register(&sIdleThread);
    TScheduler::AddReady(&sShellThread);
    TScheduler::AddReady(&sIdleThread);
    Kbd::Takeover();   // IDT 0x21 gate + unmask IRQ1
    QemuUart::Puts("EKA2-QEMU NKern: keyboard on IRQ1 vec 0x21, entering shell...\r\n");
    NThread* prev = TScheduler::iCurrentThread;  // main
    TScheduler::iCurrentThread = &sShellThread;
    ++sShellThread.iSwitches;
    NThread_Switch(&prev->iSavedEsp, sShellThread.iSavedEsp);
    }
