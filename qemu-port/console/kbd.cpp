// kbd.cpp — PS/2 Set-1 translation + ring buffer.
#include "kbd.h"

typedef unsigned short TUint16;

static inline void Out8(TUint16 aPort, TUint8 aVal)
    { __asm__ __volatile__("outb %0,%1" : : "a"(aVal), "Nd"(aPort)); }
static inline TUint8 In8(TUint16 aPort)
    { TUint8 v; __asm__ __volatile__("inb %1,%0" : "=a"(v) : "Nd"(aPort)); return v; }

// Set-1 make codes -> ASCII (unshifted). 0 = non-printable/ignored.
static const char kMap[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', '\t',
    'q','w','e','r','t','y','u','i','o','p','[',']', '\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
    'z','x','c','v','b','n','m',',','.','/', 0, '*', 0, ' ',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};
// Shifted digit/punct row for codes 2..53.
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
static volatile TUint sDropped = 0;
static volatile TUint8 sShift = 0;
static volatile TUint8 sE0 = 0;

static void Push(char aC)
    {
    TUint next = (sHead + 1) & 255u;
    if (next == sTail) { sDropped += 1; return; }  // full: drop, count
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
        if (sE0) { sE0 = 0; continue; }  // ignore extended keys (arrows etc.)
        if (sc == 0x2A || sc == 0x36) { sShift = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { sShift = 0; continue; }
        if (sc & 0x80) continue;         // key release: ignore
        if (sc >= 128) continue;
        char c = kMap[sc];
        if (c == 0) continue;
        if (sShift) c = Shifted(c);
        Push(c);
        }
    }

int Kbd::Poll()
    {
    if (sHead == sTail) return -1;
    char c = (char)sRing[sTail];
    sTail = (sTail + 1) & 255u;
    return (int)(TUint8)c;
    }

int Kbd::SerialPoll()
    {
    if (!(In8(0x3FD) & 0x01)) return -1;  // LSR.DR
    return (int)In8(0x3F8);
    }

TUint Kbd::Dropped() { return sDropped; }

extern "C" void InstallIDTEntry(void);
extern "C" void Irq1Stub(void);

void Kbd::Takeover()
    {
    __asm__ __volatile__(
        "movl $0x21, %%eax\n\t"
        "movl %0, %%ecx\n\t"
        "call InstallIDTEntry"
        :: "r"(&Irq1Stub) : "eax", "ecx", "edx", "memory");
    QemuIrq::Enable(1);  // unmask PS/2 keyboard
    }
