// kern_shell.cpp — VGA + PS/2 + EKA2> shell for the full Kern image.
//
// Adapted from the Phase-10 genuine_console.cpp (proven Set-1 map, VGA
// teletype, line editor), minus everything the genuine kernel now owns:
// no NKern::Init, no IDT surgery (genuine X86::Init1Interrupts owns the IDT),
// no KPrintf (genuine cutils.cia -> Asic::DebugOutput, which prints here).
// The shell runs as a plain genuine NThread spawned from Asic::Init3.

#define __INCLUDE_NTHREADBASE_DEFINES__
#include <x86.h>
#include <nk_trace.h>
#include <kernel/kern_priv.h>
#include <memmodel.h>
#include "qemu_variant.h"

static inline void Out8(unsigned short aPort, unsigned char aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
static inline unsigned char In8(unsigned short aPort)
    { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

namespace QemuUart
    {
    void PutC(char aC);
    }

void VgaTeletype(char aC);

// Raw char output: COM1 + VGA with NO line ending. Genuine Kern::Printf
// appends \r\n per call (A::DebugPrint), so per-char echo must bypass it
// or the prompt/input stacks vertically.
static void EmitRaw(char aC)
    {
    QemuUart::PutC(aC);
    VgaTeletype(aC);
    }

static void EmitRawS(const char* aS)
    {
    while (*aS) EmitRaw(*aS++);
    }

// ---------------------------------------------------------------- VGA teletype
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
        Out8(0x3D5, (unsigned char)(pos & 0xFF));
        Out8(0x3D4, 0x0E);
        Out8(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
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
    kVram[sRow * kCols + sCol] = (unsigned short)((kAttr << 8) | (unsigned char)aC);
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

// ------------------------------------------------------------- keyboard ring
static volatile unsigned char sRing[256];
static volatile TUint sHead = 0, sTail = 0;

extern "C" void QemuKbdPush(char aC)
    {
    TUint next = (sHead + 1) & 255u;
    if (next == sTail) return;
    sRing[sHead] = (unsigned char)aC;
    sHead = next;
    }

// Separate USB ring: source-tagged so USB delivery is measurable
// (the `N key(s) from USB HID` counter in `usb` output).
static volatile unsigned char sURing[256];
static volatile TUint sUHead = 0, sUTail = 0;

extern "C" void QemuKbdPushUsb(char aC)
    {
    TUint next = (sUHead + 1) & 255u;
    if (next == sUTail) return;
    sURing[sUHead] = (unsigned char)aC;
    sUHead = next;
    }

static int UsbKbdPoll()
    {
    if (sUHead == sUTail) return -1;
    char c = (char)sURing[sUTail];
    sUTail = (sUTail + 1) & 255u;
    return (int)(unsigned char)c;
    }

static int KbdPoll()
    {
    if (sHead == sTail) return -1;
    char c = (char)sRing[sTail];
    sTail = (sTail + 1) & 255u;
    return (int)(unsigned char)c;
    }

static int SerialPoll()
    {
    if (!(In8(0x3FD) & 0x01)) return -1;
    return (int)In8(0x3F8);
    }

// Shared input stream for the PortCon console service (variant/usercon.cpp).
// Identical poll order to the shell loop, including the USB feeds that move
// controller data into the USB ring. Callers must hold the input lease so
// the shell and a program never split the stream.
int ConPollChar()
    {
    int c = KbdPoll();
    if (c < 0) c = SerialPoll();
    UsbPoll();
    XhciKeyboardPoll();
    if (c < 0)
        c = UsbKbdPoll();
    return c;
    }

// ------------------------------------------------------------------- shell
extern "C" TUint qemu_cpu_features;  // bootcpu_gas.o BSS (CPUID report)

static char sLine[128];
static TUint sLen = 0;

// Multi-line `write` state (4KB static buffer, BSS).
static TUint8 wBuf[4096];
static TUint wLen = 0;
static char wName[128];
static char wDrive = 0;
static TBool wMode = EFalse;

static void StartMultiline(const char* aName, char aDrive)
    {
    wLen = 0;
    wMode = ETrue;
    int k = 0;
    while (aName[k] && k < 127)
        {
        wName[k] = aName[k];
        ++k;
        }
    wName[k] = 0;
    wDrive = aDrive;
    KPrintf("type lines, lone . ends\r\n");
    }

static int StrEq(const char* a, const char* b)
    { while (*a && *a == *b) { ++a; ++b; } return *a == *b; }

static char Up(char c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }
static TBool Same(const char* a, const char* b)
    {
    while (*a && Up(*a) == Up(*b)) { ++a; ++b; }
    return *a == *b;
    }

struct TCommandHelp { const char* name; const char* usage; const char* detail; };
static const TCommandHelp kHelp[] =
    {
    {"help", "HELP [command]", "Show the command list or detailed command help."},
    {"dir", "DIR [/W] [X:][path [pattern]]", "List a directory without changing the prompt. *. * and ? filter names; shows sizes plus file/dir totals."},
    {"cd", "CD [X:][path|..|\\]", "Change directory. CD alone returns to the root."},
    {"type", "TYPE [X:]path", "Display a text file in this or another directory."},
    {"write", "WRITE [X:]path [text...]", "Replace a file; omit text for multiline input, end with a lone dot."},
    {"copy", "COPY [/Y] [X:]source [Y:]dest", "Copy a file (up to 64 KiB). /Y permits overwrite; output names must be 8.3."},
    {"ren", "REN [X:]old [X:]new", "Rename an 8.3 file in its directory; no overwrite or data copying."},
    {"del", "DEL [X:]file", "Delete one file or empty directory (ERASE is an alias)."},
    {"mkdir", "MKDIR [X:]path", "Create an 8.3 directory at the given path."},
    {"rmdir", "RMDIR [X:]name", "Remove an empty directory (RD is an alias)."},
    {"mount", "MOUNT [device [partition]]", "Mount all FAT32 partitions, or one device (0-7) and partition (0-3)."},
    {"vol", "VOL", "Show mounted drives and their backing devices."},
    {"drive", "DRIVE [X]", "Show drives or switch to a mounted drive; X: also switches."},
    {"format", "FORMAT device yes", "DESTROYS the whole specified disk and creates one FAT32 partition."},
    {"exec", "EXEC [X:]file.e32 [args...]", "Start an E32 user-mode process (64 KiB file limit). Trailing words become its command line."},
    {"args", "ARGS id", "Show a process's command line (see PS for IDs)."},
    {"kill", "KILL id", "Terminate a user process; use PS to find its ID."},
    {"reap", "REAP", "Release dead user-image code segments."},
    {"exits", "EXITS", "Show recent user-process exit records (survives reaping)."},
    {"version", "VERSION", "Show kernel and CPU information."},
    {"uptime", "UPTIME", "Show milliseconds since boot."},
    {"date", "DATE [YYYY-MM-DD HH:MM:SS]", "Show RTC date and time; with both arguments, set the CMOS clock (2000-2099) and read it back."},
    {"meminfo", "MEMINFO", "Show heap, RAM and object usage."},
    {"ps", "PS", "List processes and IDs."},
    {"threads", "THREADS", "List kernel threads."},
    {"pci", "PCI", "List PCI devices."},
    {"ide", "IDE", "List ATA/AHCI disks and FAT32 partitions."},
    {"ahci", "AHCI", "Show SATA controller details."},
    {"usb", "USB [POLL]", "Probe HID keyboards; POLL displays one debug report."},
    {"xhci", "XHCI", "Show USB3 controller status; XHCIPORTS lists ports."},
    {"xhciports", "XHCIPORTS", "List USB3 root-port status."},
    {"reboot", "REBOOT", "Restart via the 8042 controller."},
    {"segs", "SEGS", "List code segments (diagnostic)."},
    {"pzomb", "PZOMB id", "Inspect a lingering process (diagnostic)."},
    {"heaptest", "HEAPTEST", "Test kernel heap allocation (diagnostic)."},
    {"xhcistart", "XHCISTART", "Start USB3 controller (diagnostic)."},
    {"xhcinoop", "XHCINOOP", "Send USB3 no-op (diagnostic)."},
    {"xhcislot", "XHCISLOT port", "Enable USB3 slot on root port 1-8."},
    {"xhciaddr", "XHCIADDR port", "Address a USB3 device on root port 1-8."},
    {"xhcidesc", "XHCIDESC port", "Read USB3 device descriptor on port 1-8."},
    {"xhcikbd", "XHCIKBD port", "Initialize a direct USB3 keyboard on port 1-8."}
    };

static void PrintHelp(const char* aName)
    {
    if (!aName)
        {
        KPrintf("EKA2 DOS-style command shell  (HELP command for details)\r\n");
        KPrintf("Files:    DIR  CD  TYPE  WRITE  COPY  REN  DEL  MKDIR  RMDIR  CLS\r\n");
        KPrintf("Drives:   MOUNT  VOL  DRIVE  X:  FORMAT\r\n");
        KPrintf("System:   VERSION  DATE  UPTIME  MEMINFO  PS  THREADS  EXEC  KILL  REAP  EXITS  REBOOT\r\n");
        KPrintf("Hardware: PCI  IDE  AHCI  USB  XHCI  XHCIPORTS\r\n");
        KPrintf("Paths: X:name selects a mounted drive; quote names containing spaces.\r\n");
        return;
        }
    if (Same(aName, "md")) aName = "mkdir";
    if (Same(aName, "rd")) aName = "rmdir";
    if (Same(aName, "erase")) aName = "del";
    if (Same(aName, "rename")) aName = "ren";
    if (Same(aName, "cls"))
        { KPrintf("CLS\r\n  Clear the VGA screen and serial terminal.\r\n"); return; }
    for (TUint i = 0; i < sizeof(kHelp) / sizeof(kHelp[0]); ++i)
        if (Same(aName, kHelp[i].name))
            { KPrintf("%s\r\n  %s\r\n", kHelp[i].usage, kHelp[i].detail); return; }
    KPrintf("help: unknown command %s\r\n", aName);
    }

// Optional "X:" drive prefix on a path arg: switches to that volume and
// returns the inner path. *aPrev gets the previous drive (0 if none) for
// VolRestore. Returns 0 when the prefix names no mounted volume.
static const char* VolPrefix(const char* aPath, char* aPrev)
    {
    *aPrev = FatCurLetter();
    if (aPath && aPath[0] && aPath[1] == ':')
        {
        char l = aPath[0];
        if (l >= 'a' && l <= 'z')
            l -= 32;
        if (l < 'A' || l > 'Z')
            return 0;
        if (FatUse(l) != KErrNone)
            return 0;
        return aPath + 2;
        }
    return aPath;
    }

static void VolRestore(char aPrev)
    {
    if (aPrev)
        FatUse(aPrev);
    }

static TInt WriteMultiline()
    {
    UserFilesLock();
    char prev = FatCurLetter();
    TInt r = FatUse(wDrive);
    if (r == KErrNone) r = FatWritePath(wName, wBuf, wLen);
    VolRestore(prev);
    UserFilesUnlock();
    return r;
    }

static const char* ErrName(TInt r)
    {
    switch (r)
        {
    case KErrNone: return "ok";
    case KErrNotFound: return "not found";
    case KErrGeneral: return "general error";
    case KErrNoMemory: return "disk full";
    case KErrNotSupported: return "not supported";
    case KErrArgument: return "bad argument";
    case KErrTimedOut: return "timed out";
    case KErrNotReady: return "not mounted";
    case KErrCorrupt: return "corrupt";
    case KErrInUse: return "in use (dir not empty?)";
    case KErrAlreadyExists: return "already exists";
    case KErrOverflow: return "too deep";
    case KErrTooBig: return "too big";
    default: return "error";
        }
    }

// The existing FAT reader and E32 loader use bounded buffers. Keep COPY
// bounded as well; do not truncate a source file silently.
static TUint8 sCopyBuf[65536];

static void CopyFile(int argc, char** argv, TBool aRename)
    {
    const char* verb = aRename ? "ren" : "copy";
    int first = 1;
    TBool overwrite = EFalse;
    if (!aRename && argc > 1 && Same(argv[1], "/Y"))
        { overwrite = ETrue; first = 2; }
    if (argc != first + 2)
        { PrintHelp(verb); return; }
    char oldDrive = FatCurLetter(), fromDrive = 0, toDrive = 0;
    const char* src = VolPrefix(argv[first], &fromDrive);
    if (!src || !*src)
        { KPrintf("%s: no such source drive\r\n", verb); VolRestore(oldDrive); return; }
    char srcLetter = FatCurLetter();
    TInt r = FatExistsPath(src);
    if (r != KErrNone)
        { KPrintf("%s: source %s\r\n", verb, r == KErrAlreadyExists ? "is a directory" : ErrName(r));
          VolRestore(oldDrive); return; }
    TUint n = 0;
    if (!aRename)
        {
        r = FatReadPath(src, sCopyBuf, sizeof(sCopyBuf), n);
        if (r != KErrNone)
            { KPrintf("%s: read %s\r\n", verb, ErrName(r)); VolRestore(oldDrive); return; }
        }
    const char* dst = VolPrefix(argv[first + 1], &toDrive);
    if (!dst || !*dst)
        { KPrintf("%s: no such destination drive\r\n", verb); VolRestore(oldDrive); return; }
    if (aRename && srcLetter != FatCurLetter())
        { KPrintf("ren: source and destination must be on the same drive\r\n"); VolRestore(oldDrive); return; }
    if (srcLetter == FatCurLetter() && Same(src, dst))
        { KPrintf("%s: source and destination are the same\r\n", verb); VolRestore(oldDrive); return; }
    if (aRename)
        {
        // FatRenamePath resolves both paths relative to this drive's CWD.
        // The caller may have switched drives to read the source; a rename
        // always requires the same drive and directory.
        r = FatRenamePath(argv[first][1] == ':' ? argv[first] + 2 : argv[first], dst);
        KPrintf("ren: %s\r\n", r == KErrNone ? "done" : ErrName(r));
        VolRestore(oldDrive);
        return;
        }
    r = FatExistsPath(dst);
    if (r == KErrAlreadyExists || (r == KErrNone && !overwrite))
        { KPrintf("%s: destination already exists%s\r\n", verb,
                  aRename ? "" : " (use COPY /Y to overwrite)"); VolRestore(oldDrive); return; }
    if (r != KErrNotFound && r != KErrNone)
        { KPrintf("%s: destination %s\r\n", verb, ErrName(r)); VolRestore(oldDrive); return; }
    r = FatWritePath(dst, sCopyBuf, n);
    if (r != KErrNone)
        { KPrintf("%s: write %s\r\n", verb, ErrName(r)); VolRestore(oldDrive); return; }
    KPrintf("copy: %u bytes\r\n", n);
    VolRestore(oldDrive);
    }

// Two-digit raw emitter for `date` (genuine %02d support is unconfirmed,
// so pad manually through the newline-free path).
static void Emit2(TInt aV)
    {
    EmitRaw((char)('0' + (aV / 10) % 10));
    EmitRaw((char)('0' + aV % 10));
    }

// Four-digit raw emitter (year: genuine KPrintf ends the line per call).
static void Emit4(TInt aV)
    {
    EmitRaw((char)('0' + (aV / 1000) % 10));
    EmitRaw((char)('0' + (aV / 100) % 10));
    Emit2(aV % 100);
    }

static TBool LeapYear(TInt aYear)
    { return (aYear % 4 == 0) && ((aYear % 100 != 0) || (aYear % 400 == 0)); }

static TInt MonthDays(TInt aMon, TInt aYear)
    {
    static const TInt mdays[12] =
        {31,28,31,30,31,30,31,31,30,31,30,31};
    if (aMon < 1 || aMon > 12)
        return 0;
    return mdays[aMon - 1] + ((aMon == 2 && LeapYear(aYear)) ? 1 : 0);
    }

static void PrintDate(TInt aSeconds)
    {
    TInt days = aSeconds / 86400;
    TInt rem = aSeconds % 86400;
    TInt year = 2000;
    for (;;)
        {
        TInt ydays = LeapYear(year) ? 366 : 365;
        if (days < ydays) break;
        days -= ydays;
        ++year;
        }
    TInt mon = 1;
    for (;;)
        {
        TInt md = MonthDays(mon, year);
        if (days < md) break;
        days -= md;
        ++mon;
        }
    Emit4(year);
    EmitRaw('-');
    Emit2(mon);
    EmitRaw('-');
    Emit2(days + 1);
    EmitRaw(' ');
    Emit2(rem / 3600);
    EmitRaw(':');
    Emit2((rem % 3600) / 60);
    EmitRaw(':');
    Emit2(rem % 60);
    EmitRawS("\r\n");
    }

static TBool ParseDigits(const char* aText, TInt aCount, TInt& aOut)
    {
    TInt v = 0;
    for (TInt i = 0; i < aCount; ++i)
        {
        char c = aText[i];
        if (c < '0' || c > '9')
            return EFalse;
        v = v * 10 + (c - '0');
        }
    aOut = v;
    return ETrue;
    }

// Strict YYYY-MM-DD, CMOS range 2000-2099, real calendar day.
static TBool ParseDate(const char* aText, TInt& aYear, TInt& aMon, TInt& aDay)
    {
    if (!aText)
        return EFalse;
    TInt n = 0;
    while (aText[n]) ++n;
    if (n != 10 || aText[4] != '-' || aText[7] != '-')
        return EFalse;
    if (!ParseDigits(aText, 4, aYear) || !ParseDigits(aText + 5, 2, aMon) ||
        !ParseDigits(aText + 8, 2, aDay))
        return EFalse;
    if (aYear < 2000 || aYear > 2099 || aMon < 1 || aMon > 12)
        return EFalse;
    TInt md = MonthDays(aMon, aYear);
    return (aDay >= 1 && aDay <= md) ? ETrue : EFalse;
    }

// Strict HH:MM:SS.
static TBool ParseTime(const char* aText, TInt& aHour, TInt& aMin, TInt& aSec)
    {
    if (!aText)
        return EFalse;
    TInt n = 0;
    while (aText[n]) ++n;
    if (n != 8 || aText[2] != ':' || aText[5] != ':')
        return EFalse;
    if (!ParseDigits(aText, 2, aHour) || !ParseDigits(aText + 3, 2, aMin) ||
        !ParseDigits(aText + 6, 2, aSec))
        return EFalse;
    return (aHour <= 23 && aMin <= 59 && aSec <= 59) ? ETrue : EFalse;
    }

static void Exec()
    {
    sLine[sLen] = 0;
    // Keep EXEC's command line verbatim before the generic tokenizer
    // inserts NULs and reaches its eight-word limit.
    static char original[sizeof(sLine)];
    memcpy(original, sLine, sLen + 1);
    const char* execTail = "";
    // Tokenize into argv (in place; `write` rejoins the tail).
    // Double quotes group spaces: type "long name.txt".
    char* argv[8];
    int argc = 0;
    for (char* p = sLine; *p && argc < 8;)
        {
        while (*p == ' ' || *p == '\t')
            { *p = 0; ++p; }
        if (!*p)
            break;
        if (*p == '"')
            {
            *p = 0;
            ++p;
            argv[argc++] = p;
            while (*p && *p != '"')
                ++p;
            if (*p == '"')
                { *p = 0; ++p; }
            }
        else
            {
            argv[argc++] = p;
            while (*p && *p != ' ' && *p != '\t')
                ++p;
            }
        if (argc == 2)
            {
            execTail = original + (p - sLine);
            while (*execTail == ' ' || *execTail == '\t')
                ++execTail;
            }
        }
    if (!argc)
        return;
    for (char* p = argv[0]; *p; ++p)
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    const char* cmd = argv[0];
    if (argc == 2 && Same(argv[1], "/?"))
        { PrintHelp(cmd); return; }
    // Bare "X:" switches to drive X (DOS-style), e.g. `d:`.
    if (cmd[0] && cmd[1] == ':' && !cmd[2] &&
        ((cmd[0] >= 'A' && cmd[0] <= 'Z') || (cmd[0] >= 'a' && cmd[0] <= 'z')))
        {
        char l = (char)(cmd[0] & ~0x20);
        TInt r = FatUse(l);
        if (r != KErrNone)
            KPrintf("%c: no such drive\r\n", l);
        return;
        }
    if (StrEq(cmd, "help"))
        PrintHelp(argc > 1 ? argv[1] : 0);
    else if (StrEq(cmd, "cls"))
        { VgaInit(); EmitRawS("\033[2J\033[H"); }
    else if (StrEq(cmd, "copy"))
        CopyFile(argc, argv, EFalse);
    else if (StrEq(cmd, "ren") || StrEq(cmd, "rename"))
        CopyFile(argc, argv, ETrue);
    else if (StrEq(cmd, "version"))
        {
        KPrintf("Symbian EKA2 full kernel (genuine), x86 bare-metal on QEMU i386\r\n");
        TUint cr0;
        __asm__ __volatile__("mov %0, cr0" : "=r"(cr0));
        KPrintf("  feats=0x%x CR0=0x%x tick=1ms\r\n", qemu_cpu_features, cr0);
        }
    else if (StrEq(cmd, "uptime"))
        {
        TUint ms = NKern::TickCount();
        KPrintf("uptime: %u ms (%u s)\r\n", ms, ms / 1000u);
        }
    else if (StrEq(cmd, "meminfo"))
        {
        // NB: RHeapK::Available() faults KErrNotSupported by design on the
        // fixed kernel heap; report totals from HeapInfo + RHeapK instead.
        TInt ramFreeBlocks = MM::RamAllocator ? MM::RamAllocator->Avail() : -1;
        KPrintf("heap: base=0x%x max=%d total=%d\r\n",
            (TUint)K::HeapInfo.iBase, K::HeapInfo.iMaxSize,
            ((RHeapK*)K::Allocator)->TotalAllocSize());
        KPrintf("ram: free=%d blocks (%d bytes) user pool\r\n",
            ramFreeBlocks, ramFreeBlocks * 4096);
        KPrintf("objs: threads=%d procs=%d chunks=%d timers=%d\r\n",
            K::Containers[EThread]->Count(), K::Containers[EProcess]->Count(),
            K::Containers[EChunk]->Count(), K::Containers[ETimer]->Count());
        KPrintf("msgpool: free=%d\r\n", K::MsgInfo.iFreeMessageCount);
        }
    else if (StrEq(cmd, "heaptest"))
        {
        // RHeapK alloc/free sanity: total must rise then fall back.
        RHeapK* hk = (RHeapK*)K::Allocator;
        TInt b0 = hk->TotalAllocSize();
        TUint8* p = (TUint8*)Kern::AllocZ(2268);
        TInt b1 = hk->TotalAllocSize();
        if (p)
            Kern::Free(p);
        TInt b2 = hk->TotalAllocSize();
        KPrintf("heaptest: %d -> %d -> %d\r\n", b0, b1, b2);
        }
    else if (StrEq(cmd, "pci"))
        {
        PciList();
        }
    else if (StrEq(cmd, "threads"))
        {
        DObjectCon* c = K::Containers[EThread];
        c->Wait();
        TInt n = c->Count();
        KPrintf("%d threads:\r\n", n);
        for (TInt i = 0; i < n; ++i)
            {
            DThread* t = (DThread*)(*c)[i];
            TBuf<48> name;
            t->FullName(name);
            char narrow[48];
            TInt k = 0;
            for (; k < name.Length() && k < 47; ++k)
                narrow[k] = (char)(name[k] & 0x7F);
            narrow[k] = 0;
            const char* st = "?";
            switch (t->iNThread.iNState)
                {
            case NThreadBase::EReady: st = "READY"; break;
            case NThreadBase::ESuspended: st = "SUSP"; break;
            case NThreadBase::EWaitFastSemaphore: st = "WSEM"; break;
            case NThreadBase::ESleep: st = "SLEEP"; break;
            case NThreadBase::EBlocked: st = "BLOCK"; break;
            case NThreadBase::EDead: st = "DEAD"; break;
            case NThreadBase::EWaitDfc: st = "DFC"; break;
                }
            KPrintf("  %s pri=%d %s\r\n", narrow,
                (TInt)t->iNThread.iPriority, st);
            }
        c->Signal();
        }
    else if (StrEq(cmd, "ps"))
        {
        KPrintf("PortFile channels: %d\r\n", UserFilesChannelCount());
        DObjectCon* c = K::Containers[EProcess];
        c->Wait();
        TInt n = c->Count();
        KPrintf("%d processes:\r\n", n);
        for (TInt i = 0; i < n; ++i)
            {
            DProcess* p = (DProcess*)(*c)[i];
            TBuf<48> name;
            p->FullName(name);
            char narrow[48];
            TInt k = 0;
            for (; k < name.Length() && k < 47; ++k)
                narrow[k] = (char)(name[k] & 0x7F);
            narrow[k] = 0;
            KPrintf("  %s id=%d\r\n", narrow, (TInt)p->iId);
            }
        c->Signal();
        }
    else if (StrEq(cmd, "args"))
        {
        // Show a process's genuine command line (DProcess::iCommandLine).
        if (argc < 2)
            PrintHelp("args");
        else
            {
            TUint id = 0;
            for (const char* p = argv[1]; *p >= '0' && *p <= '9'; ++p)
                id = id * 10 + (TUint)(*p - '0');
            NKern::ThreadEnterCS();
            DObjectCon* c = K::Containers[EProcess];
            c->Wait();
            DProcess* found = NULL;
            for (TInt i = 0; i < c->Count(); ++i)
                {
                DProcess* p = (DProcess*)(*c)[i];
                if ((TUint)p->iId == id)
                    { found = p; break; }
                }
            if (!found)
                KPrintf("args: no such process\r\n");
            else if (!found->iCommandLine || !found->iCommandLine->Length())
                KPrintf("args: id=%u (empty)\r\n", id);
            else
                {
                KPrintf("args: id=%u len=%d ", id,
                    found->iCommandLine->Length() / 2);
                const TUint8* t = found->iCommandLine->Ptr();
                for (TInt k = 0; k + 1 < found->iCommandLine->Length(); k += 2)
                    EmitRaw((char)t[k]);
                EmitRawS("\r\n");
                }
            c->Signal();
            NKern::ThreadLeaveCS();
            }
        }
    else if (StrEq(cmd, "pzomb"))
        {
        // Zombie inspector: why does a dead-thread process object linger?
        if (argc < 2)
            PrintHelp("pzomb");
        else
            {
            TUint id = 0;
            for (const char* p = argv[1]; *p >= '0' && *p <= '9'; ++p)
                id = id * 10 + (TUint)(*p - '0');
            NKern::ThreadEnterCS();
            DObjectCon* c = K::Containers[EProcess];
            c->Wait();
            DProcess* found = NULL;
            for (TInt i = 0; i < c->Count(); ++i)
                {
                DProcess* p = (DProcess*)(*c)[i];
                if ((TUint)p->iId == id)
                    { found = p; break; }
                }
            if (!found)
                KPrintf("pzomb: no such process\r\n");
            else
                {
                DThread* ft = found->iThreadQ.IsEmpty() ? NULL
                    : found->FirstThread();
                KPrintf("pzomb id=%u obj=%x thread=%x access=%d exit=%d threadsempty=%d handles=%d/%d\r\n",
                    id, found, ft, found->AccessCount(), (TInt)found->iExitType,
                    (TInt)found->iThreadQ.IsEmpty(),
                    found->iHandles.Count(), found->iHandles.ActiveCount());
                // NOTE: all dereferences above and below happen while the
                // process container lock is held, pinning `found` against
                // destruction (cf. E32Kill which Opens before unlock).
                DMutex* m = c->Lock();
                TThreadMutexCleanup& cu = m->iCleanup;
                DThread* holder = cu.iThread;
                TBuf<32> hn;
                if (holder)
                    holder->FullName(hn);
                char nh[32];
                TInt k = 0;
                for (; k < hn.Length() && k < 31; ++k)
                    nh[k] = (char)(hn[k] & 0x7F);
                nh[k] = 0;
                KPrintf("pzomb conmutex heldby=%s hold=%d wait=%d resetting=%d\r\n",
                    holder ? nh : "(free)", m->iHoldCount, m->iWaitCount,
                    (TInt)m->iResetting);
                DCodeSeg* sg = found->CodeSeg();
                if (sg)
                    KPrintf("pzomb seg=%x access=%d attach=%x markloaded=%d\r\n",
                        sg, sg->iAccessCount, sg->iAttachProcess,
                        (TInt)((sg->iMark & 0x10) != 0));
                else
                    KPrintf("pzomb seg=(none)\r\n");
                }
            c->Signal();
            NKern::ThreadLeaveCS();
            }
        }
    else if (StrEq(cmd, "date"))
        {
        if (argc == 1)
            {
            TInt s = QemuRtcSeconds();
            if (s < 0)
                {
                KPrintf("rtc unreadable\r\n");
                }
            else
                {
                PrintDate(s);
                }
            }
        else if (argc == 3)
            {
            // DATE YYYY-MM-DD HH:MM:SS: validate fully, convert with the
            // same Gregorian rules as the RTC reader, set CMOS, read back.
            TInt year, mon, day, hour, minute, sec;
            if (!ParseDate(argv[1], year, mon, day) ||
                !ParseTime(argv[2], hour, minute, sec))
                {
                KPrintf("date: use DATE YYYY-MM-DD HH:MM:SS\r\n");
                }
            else
                {
                TInt days = 0;
                for (TInt y = 2000; y < year; ++y)
                    days += LeapYear(y) ? 366 : 365;
                for (TInt m = 1; m < mon; ++m)
                    days += MonthDays(m, year);
                days += day - 1;
                TInt want = days * 86400 + hour * 3600 + minute * 60 + sec;
                TInt r = QemuSetRtcSeconds(want);
                if (r != KErrNone)
                    KPrintf("date: %s\r\n", ErrName(r));
                else
                    {
                    TInt back = QemuRtcSeconds();
                    if (back == want)
                        PrintDate(back);
                    else
                        KPrintf("date: set failed (read %d, want %d)\r\n", back, want);
                    }
                }
            }
        else
            PrintHelp("date");
        }
    else if (StrEq(cmd, "ide"))
        {
        int n = AtaCount();
        KPrintf("%d ATA disk(s):\r\n", n);
        for (int d = 0; d < 8; ++d)
            {
            if (!AtaPresent(d))
                continue;
            KPrintf("  dev %d: %s %u sectors (%uMB)%s\r\n", d, AtaModel(d),
                AtaSectors(d), AtaSectors(d) / 2048,
                d >= 4 ? " [AHCI]" : "");
            FatParts(d);
            }
        }
    else if (StrEq(cmd, "ahci"))
        {
        AhciInfo();
        }
    else if (StrEq(cmd, "xhci"))
        {
        XhciInfo();
        }
    else if (StrEq(cmd, "xhcistart"))
        {
        KPrintf("xhci: start %s\r\n", ErrName(XhciStart()));
        }
    else if (StrEq(cmd, "xhcinoop"))
        {
        KPrintf("xhci: no-op %s\r\n", ErrName(XhciNoOp()));
        }
    else if (StrEq(cmd, "xhciports"))
        {
        XhciPorts();
        }
    else if (StrEq(cmd, "xhcislot"))
        {
        if (argc < 2 || argv[1][0] < '1' || argv[1][0] > '8' || argv[1][1])
            PrintHelp("xhcislot");
        else
            KPrintf("xhci: enable slot %s\r\n",
                ErrName(XhciEnableSlot(argv[1][0] - '1')));
        }
    else if (StrEq(cmd, "xhciaddr"))
        {
        if (argc < 2 || argv[1][0] < '1' || argv[1][0] > '8' || argv[1][1])
            PrintHelp("xhciaddr");
        else
            KPrintf("xhci: address %s\r\n",
                ErrName(XhciAddress(argv[1][0] - '1')));
        }
    else if (StrEq(cmd, "xhcidesc"))
        {
        if (argc < 2 || argv[1][0] < '1' || argv[1][0] > '8' || argv[1][1])
            PrintHelp("xhcidesc");
        else
            KPrintf("xhci: get descriptor %s\r\n",
                ErrName(XhciGetDescriptor(argv[1][0] - '1', 1, 8)));
        }
    else if (StrEq(cmd, "xhcikbd"))
        {
        if (argc < 2 || argv[1][0] < '1' || argv[1][0] > '8' || argv[1][1])
            PrintHelp("xhcikbd");
        else
            KPrintf("xhci: keyboard %s\r\n",
                ErrName(XhciKeyboardInit(argv[1][0] - '1')));
        }
    else if (StrEq(cmd, "usb"))
        {
        if (argc > 1 && StrEq(argv[1], "poll"))
            {
            UsbDebugPoll();
            return;
            }
        TInt r = UsbInit();
        if (r == KErrNotSupported)
            {
            TInt xr = XhciAutoKeyboard();
            if (xr != KErrNone && xr != KErrNotFound)
                KPrintf("usb: xhci init %s\r\n", ErrName(xr));
            if (xr == KErrNotFound)
                KPrintf("usb: no USB HID keyboard (xhciports for ports)\r\n");
            }
        else if (r != KErrNone && r != KErrNotFound)
            KPrintf("usb: init %s\r\n", ErrName(r));
        UsbList();
        XhciKeyboardInfo();
        }
    else if (StrEq(cmd, "mount"))
        {
        int dev = (argc > 1) ? (argv[1][0] - '0') : -1;
        int part = (argc > 2) ? (argv[2][0] - '0') : -1;
        if (dev < 0)
            {
            // No args: mount every FAT32 partition found, then list.
            AtaInit();
            for (int d = 0; d < 8; ++d)
                {
                if (!AtaPresent(d))
                    continue;
                for (int p = 0; p < 4; ++p)
                    {
                    if (FatMount(d, p) == KErrNone)
                        KPrintf("mounted %c: dev %d part %d\r\n",
                            FatCurLetter(), d, p);
                    }
                }
            FatList();
            }
        else
            {
            TInt r = FatMount(dev, part);
            if (r == KErrNone)
                KPrintf("mounted %c:\r\n", FatCurLetter());
            else
                KPrintf("mount: %s\r\n", ErrName(r));
            }
        }
    else if (StrEq(cmd, "vol"))
        {
        FatList();
        }
    else if (StrEq(cmd, "drive"))
        {
        if (argc < 2)
            {
            char l = FatCurLetter();
            if (l)
                KPrintf("current drive %c:\r\n", l);
            FatList();
            }
        else
            {
            char l = argv[1][0];
            if (l >= 'a' && l <= 'z')
                l -= 32;
            TInt r = FatUse(l);
            if (r != KErrNone)
                KPrintf("drive: no such drive %c:\r\n", l);
            }
        }
    else if (StrEq(cmd, "dir"))
        {
        TBool wide = EFalse;
        const char* path = 0;
        for (int i = 1; i < argc; ++i)
            {
            if (Same(argv[i], "/W")) wide = ETrue;
            else if (!path) path = argv[i];
            else { PrintHelp("dir"); return; }
            }
        char prev = 0;
        const char* arg = path ? VolPrefix(path, &prev) : 0;
        if (path && !arg)
            KPrintf("dir: no such drive\r\n");
        else
            {
            TInt r = FatDirPath(arg, wide);
            if (r != KErrNone)
                KPrintf("dir: %s\r\n", ErrName(r));
            }
        VolRestore(prev);
        }
    else if (StrEq(cmd, "cd"))
        {
        // `cd D:` / `cd D:dir` switches drives (stays switched); plain cd is local.
        char prev = 0;
        const char* arg = (argc > 1) ? VolPrefix(argv[1], &prev) : 0;
        if (argc > 1 && !arg)
            KPrintf("cd: no such drive\r\n");
        else
            {
            TInt r = FatCdPath(arg);
            if (r != KErrNone)
                {
                KPrintf("cd: %s\r\n", ErrName(r));
                VolRestore(prev);
                }
            }
        }
    else if (StrEq(cmd, "type"))
        {
        if (argc < 2)
            PrintHelp("type");
        else
            {
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("type: no such drive\r\n");
            else
                {
                TInt r = FatTypePath(name);
                if (r != KErrNone)
                    KPrintf("type: %s\r\n", ErrName(r));
                }
            VolRestore(prev);
            }
        }
    else if (StrEq(cmd, "write"))
        {
        if (argc == 2)
            {
            // Multi-line mode: lines accumulate until a lone ".".
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("write: no such drive\r\n");
            else
                StartMultiline(name, FatCurLetter());
            VolRestore(prev);
            }
        else if (argc < 3)
            PrintHelp("write");
        else
            {
            // Rejoin the tail with single spaces + CRLF.
            char text[512];
            int k = 0;
            for (int i = 2; i < argc && k < 498; ++i)
                {
                if (i > 2 && k < 498)
                    text[k++] = ' ';
                for (const char* p = argv[i]; *p && k < 498; ++p)
                    text[k++] = *p;
                }
            text[k++] = '\r';
            text[k++] = '\n';
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("write: no such drive\r\n");
            else
                {
                TInt r = FatWritePath(name, (const TUint8*)text, (TUint)k);
                if (r == KErrNone)
                    KPrintf("wrote %d bytes\r\n", k);
                else
                    KPrintf("write: %s\r\n", ErrName(r));
                }
            VolRestore(prev);
            }
        }
    else if (StrEq(cmd, "del") || StrEq(cmd, "erase") ||
             StrEq(cmd, "rmdir") || StrEq(cmd, "rd"))
        {
        TBool dirOnly = StrEq(cmd, "rmdir") || StrEq(cmd, "rd");
        if (argc < 2)
            PrintHelp(dirOnly ? "rmdir" : "del");
        else
            {
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("del: no such drive\r\n");
            else
                {
                TInt r = dirOnly ? FatExistsPath(name) : KErrAlreadyExists;
                if (dirOnly && r == KErrNone)
                    KPrintf("rmdir: not a directory\r\n");
                else if (dirOnly && r != KErrAlreadyExists)
                    KPrintf("rmdir: %s\r\n", ErrName(r));
                else
                    {
                    r = FatDelPath(name);
                if (r == KErrNone)
                    KPrintf("deleted\r\n");
                else
                    KPrintf("del: %s\r\n", ErrName(r));
                    }
                }
            VolRestore(prev);
            }
        }
    else if (StrEq(cmd, "mkdir") || StrEq(cmd, "md"))
        {
        if (argc < 2)
            PrintHelp("mkdir");
        else
            {
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("mkdir: no such drive\r\n");
            else
                {
                TInt r = FatMkdirPath(name);
                if (r == KErrNone)
                    KPrintf("created\r\n");
                else
                    KPrintf("mkdir: %s\r\n", ErrName(r));
                }
            VolRestore(prev);
            }
        }
    else if (StrEq(cmd, "format"))
        {
        if (argc < 3 || !Same(argv[2], "yes"))
            PrintHelp("format");
        else
            {
            int dev = argv[1][0] - '0';
            KPrintf("formatting dev %d...\r\n", dev);
            TInt r = FatFormat(dev);
            if (r == KErrNone)
                KPrintf("formatted + mounted %c:\r\n", FatCurLetter());
            else
                KPrintf("format: %s\r\n", ErrName(r));
            }
        }
    else if (StrEq(cmd, "exec"))
        {
        // Load an E32 from FAT32 and run it as a genuine user process.
        // Extra words become the process command line (UTF-16 in HBuf,
        // readable via User::CommandLine or the `args` verb).
        if (argc < 2)
            PrintHelp("exec");
        else
            {
            char prev = 0;
            const char* name = VolPrefix(argv[1], &prev);
            if (!name || !name[0])
                KPrintf("exec: no such drive\r\n");
            else
                {
                static TUint8 eBuf[65536];
                TUint got = 0;
                TInt r = FatReadPath(name, eBuf, sizeof(eBuf), got);
                KPrintf("exec: read %u bytes r=%d\r\n", got, r);
                if (r != KErrNone)
                    KPrintf("exec: %s\r\n", ErrName(r));
                else
                    {
                    TInt reaped = E32Reap();
                    if (reaped)
                        KPrintf("exec: reaped %d dead user image(s)\r\n", reaped);
                    r = E32Run(eBuf, got, name, execTail);
                    if (r == KErrNone)
                        KPrintf("exec: %s started (user process)\r\n", name);
                    else
                        KPrintf("exec: %s (%s)\r\n", name, ErrName(r));
                    }
                }
            VolRestore(prev);
            }
        }
    else if (StrEq(cmd, "kill"))
        {
        // Kill a user process by container id (see ps). Never the kernel.
        if (argc < 2)
            PrintHelp("kill");
        else
            {
            TUint id = 0;
            for (const char* p = argv[1]; *p >= '0' && *p <= '9'; ++p)
                id = id * 10 + (TUint)(*p - '0');
            TInt r = E32Kill(id);
            if (r == KErrNone)
                KPrintf("killed %u\r\n", id);
            else
                KPrintf("kill: %s\r\n", ErrName(r));
            }
        }
    else if (StrEq(cmd, "reap"))
        {
        // Post-mortem code-seg cleanup (also runs automatically on exec).
        TInt n = E32Reap();
        KPrintf("reaped %d\r\n", n);
        }
    else if (StrEq(cmd, "exits"))
        {
        // Recent exit records: survive reaping, unlike `ps` rows.
        E32PrintExits();
        }
    else if (StrEq(cmd, "segs"))
        {
        // Code-seg census: every live DCodeSeg with its refcount.
        DCodeSeg::Wait();
        TInt n = 0;
        for (SDblQueLink* pL = DCodeSeg::GlobalList.First();
             pL != &DCodeSeg::GlobalList.iA; pL = pL->iNext)
            {
            DCodeSeg* pS = _LOFF(pL, DCodeSeg, iLink);
            char narrow[40];
            TInt k = 0;
            if (pS->iFileName)
                {
                const TText* fn = pS->iFileName->Ptr();
                TInt fl = pS->iFileName->Length();
                for (; k < fl && k < 39; ++k)
                    narrow[k] = (char)(fn[k] & 0x7F);
                }
            narrow[k] = 0;
            KPrintf("  %s access=%d attach=%d\r\n",
                narrow, pS->iAccessCount,
                pS->iAttachProcess ? (TInt)pS->iAttachProcess->iId : -1);
            ++n;
            }
        DCodeSeg::Signal();
        KPrintf("%d code segs\r\n", n);
        }
    else if (StrEq(cmd, "reboot"))
        {
        KPrintf("rebooting via 8042...\r\n");
        TUint32 t0 = NKern::TickCount();
        while ((TUint32)(NKern::TickCount() - t0) < 200) { }
        Out8(0x64, 0xFE);
        KPrintf("reboot failed (8042 ignored), halting\r\n");
        for (;;)
            __asm__ __volatile__("hlt");
        }
    else
        KPrintf("unknown command, try `help`\r\n");
    }

static void ShellMain(TAny* /*aPtr*/)
    {
    VgaInit();
    TInt files = UserFilesInit();
    if (files != KErrNone)
        { KPrintf("PortFile init failed %d\r\n", files); return; }
    TInt con = UserConInit();
    if (con != KErrNone)
        { KPrintf("PortCon init failed %d\r\n", con); return; }
    KPrintf("Symbian EKA2 x86 full kernel -- interactive console\r\n");
    KPrintf("sched by TheScheduler, IRQs via genuine vectors + variant Bind\r\n");
    FatPrompt();
    for (;;)
        {
        // While a user program leases console input, the shell must not
        // touch the stream (it would steal program keystrokes). A dead
        // owner is reclaimed here so a crash without release cannot wedge
        // input forever; the next keystroke then reaches the shell.
        if (ConInputLeased())
            {
            if (!ConOwnerAlive())
                {
                ConForceRelease();
                KPrintf("\r\nconsole lease released (owner died)\r\n");
                FatPrompt();
                }
            else
                {
                __asm__ __volatile__("hlt");
                continue;
                }
            }
        int c = KbdPoll();
        if (c < 0) c = SerialPoll();  // fallback: IRQ4 normally fills the ring
        UsbPoll();  // USB HID keyboards feed the USB ring via QemuKbdPushUsb
        XhciKeyboardPoll();
        if (c < 0)
            c = UsbKbdPoll();
        if (c < 0)
            {
            __asm__ __volatile__("hlt");
            continue;
            }
        if (c == '\r') c = '\n';
        if (c == '\n')
            {
            KPrintf("\n");
            if (wMode)
                {
                sLine[sLen] = 0;
                if (sLen == 1 && sLine[0] == '.')
                    {
                    TInt r = WriteMultiline();
                    if (r == KErrNone)
                        KPrintf("wrote %u bytes\r\n", wLen);
                    else
                        KPrintf("write: %s\r\n", ErrName(r));
                    wMode = EFalse;
                    }
                else if (wLen + sLen + 2 < sizeof(wBuf))
                    {
                    for (TUint i = 0; i < sLen; ++i)
                        wBuf[wLen++] = (TUint8)sLine[i];
                    wBuf[wLen++] = '\r';
                    wBuf[wLen++] = '\n';
                    }
                else
                    {
                    TInt r = WriteMultiline();
                    if (r == KErrNone)
                        KPrintf("buffer full, wrote %u bytes\r\n", wLen);
                    else
                        KPrintf("write: %s\r\n", ErrName(r));
                    wMode = EFalse;
                    }
                }
            else
                {
                UserFilesLock();
                Exec();
                UserFilesUnlock();
                }
            sLen = 0;
            FatPrompt();
            }
        else if (c == '\b' || c == 127)
            {
            if (sLen > 0) { --sLen; EmitRaw('\b'); }
            }
        else if (c == 27)
            {
            while (sLen > 0) { --sLen; EmitRaw('\b'); }
            }
        else if (c >= 32 && c < 127)
            {
            if (sLen < sizeof(sLine) - 1)
                {
                sLine[sLen++] = (char)c;
                EmitRaw((char)c);
                }
            }
        }
    }

extern "C" void* memclr(void* aTrg, unsigned aLength);

_LIT(KLitShellName, "Shell");

static TInt ShellDThreadFn(TAny* /*aPtr*/)
    {
    ShellMain(0);
    return 0;
    }

void ShellSpawnFull()
    {
    // Spawned from Asic::Init3 on the supervisor thread as a genuine DThread
    // (EThreadSupervisor, kernel process): DThreads can hold container
    // mutexes, which the `threads`/`ps` commands need. Uses the sanctioned
    // Kern::ThreadCreate path (cf. device drivers), not raw NKern threads.
    SThreadCreateInfo info;
    memclr(&info, sizeof(info));
    info.iType = EThreadSupervisor;
    info.iFunction = ShellDThreadFn;
    info.iPtr = 0;
    info.iSupervisorStack = 0;  // auto-allocate (cf. supervisor thread)
    info.iSupervisorStackSize = 0;
    info.iInitialThreadPriority = 10;
    info.iName.Set(KLitShellName);
    info.iTotalSize = sizeof(info);
    KPrintf("KERN spawning shell DThread...\r\n");
    NKern::ThreadEnterCS();
    TInt r = Kern::ThreadCreate(info);
    DThread* pT = (DThread*)info.iHandle;
    if (r == KErrNone)
        Kern::ThreadResume(*pT);
    NKern::ThreadLeaveCS();
    if (r != KErrNone)
        KPrintf("KERN shell spawn failed: %d\r\n", r);
    }
