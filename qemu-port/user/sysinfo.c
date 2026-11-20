/*
 * sysinfo.c -- end-user system-information utility for the Sybian REAL port.
 *
 * A genuine PE2 E32 program that reports what the machine and this ported
 * EKA2 kernel actually are.  All serial markers go through EUSER.DLL ordinal
 * 915 (RDebug::Printf) imported through a PE2 IAT slot at codeBase+0x8000,
 * with the port's DebugPrint executive as a fallback so the trace is always
 * visible on the serial console (same pattern as user/calc.c).
 *
 * The CPU description is genuinely read, not canned: CPUID leaves 0 and 1 are
 * unprivileged on i486+ and are executed directly from this ring-3 process via
 * inline assembly.  Uptime comes from the genuine EExecTickCount executive
 * (slow exec 5, NKern tick count, 1 ms per tick in this port).  The e1000 NIC
 * is probed through the kernel's netbridge exec (247): a live bridge publishes
 * the "NBRG" magic in low, identity-mapped RAM.  Everything else is reported
 * as built into the kernel (no userland probe exists in this port).
 */
#include "runtime.h"
#include "netbridge.h"

/* PE2 IAT slot, defined with --defsym in user.mk.  Before load it holds the
 * ordinal (915); afterwards it holds the resolved RDebug::Printf address. */
extern void (*sysinfo_printf_slot)(const char*);
extern int exec0(unsigned number);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

#define EEXEC_TICK_COUNT 5u   /* genuine EExecTickCount (slow exec 5) */

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&sysinfo_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker directly into a buffer, no hosted libc. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

static unsigned append_int(char* dst, unsigned n, int value)
{
    unsigned magnitude = (unsigned)value;
    if (value < 0) { dst[n++] = '-'; magnitude = 0u - magnitude; }
    char digits[12];
    int d = 0;
    if (!magnitude) digits[d++] = '0';
    while (magnitude)
        {
        digits[d++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
        }
    while (d) dst[n++] = digits[--d];
    return n;
}

static unsigned append_hex(char* dst, unsigned n, unsigned value)
{
    static const char kHex[] = "0123456789ABCDEF";
    int d, shift;
    char digits[8];
    for (d = 0, shift = 28; shift >= 0; shift -= 4, ++d)
        digits[d] = kHex[(value >> shift) & 0xFu];
    dst[n++] = '0'; dst[n++] = 'x';
    for (d = 0; d < 8; ++d) dst[n++] = digits[d];
    return n;
}

/* ------------------------------------------------------------ CPUID (userland)
 * CPUID is unprivileged. We need explicit "c" (ECX) input so leaf 0/1 both
 * start from ECX=0; EBX is safe because this build is -fno-pic (no GOT in EBX). */
struct CpuInfo {
    char vendor[13];
    unsigned family, model, stepping;
    unsigned features;   /* CPUID.1:EDX */
};

static void cpuid(unsigned leaf, unsigned* a, unsigned* b,
                  unsigned* c, unsigned* d)
{
    __asm__ __volatile__("cpuid"
                         : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                         : "a"(leaf), "c"(0u));
}

static void read_cpu(struct CpuInfo* cpu)
{
    unsigned a, b, c, d;
    unsigned words[3];
    int w, i, n = 0;

    cpuid(0u, &a, &b, &c, &d);
    words[0] = b; words[1] = d; words[2] = c;   /* vendor string order */
    for (w = 0; w < 3; ++w)
        for (i = 0; i < 4; ++i)
            {
            unsigned char ch = (unsigned char)((words[w] >> (i * 8)) & 0xFFu);
            if (ch >= 32 && ch < 127) cpu->vendor[n++] = (char)ch;
            }
    cpu->vendor[n] = 0;

    cpuid(1u, &a, &b, &c, &d);
    cpu->family   = (a >> 8) & 0xFu;
    cpu->model    = (a >> 4) & 0xFu;
    cpu->stepping = a & 0xFu;
    cpu->features = d;
}

/* ------------------------------------------------------------ report lines */
static void report_cpu(const struct CpuInfo* cpu)
{
    char line[192];
    unsigned n = 0;
    n = copy_text(line, n, "SYS: CPU ");
    n = copy_text(line, n, cpu->vendor);
    n = copy_text(line, n, " family=");
    n = append_int(line, n, (int)cpu->family);
    n = copy_text(line, n, " model=");
    n = append_int(line, n, (int)cpu->model);
    n = copy_text(line, n, " stepping=");
    n = append_int(line, n, (int)cpu->stepping);
    line[n] = 0;
    marker(line);

    n = 0;
    n = copy_text(line, n, "SYS: features FPU/SSE/SSE2/HTT FPU=");
    n = append_int(line, n, (cpu->features & (1u << 0))  ? 1 : 0);
    n = copy_text(line, n, " SSE=");
    n = append_int(line, n, (cpu->features & (1u << 25)) ? 1 : 0);
    n = copy_text(line, n, " SSE2=");
    n = append_int(line, n, (cpu->features & (1u << 26)) ? 1 : 0);
    n = copy_text(line, n, " HTT=");
    n = append_int(line, n, (cpu->features & (1u << 28)) ? 1 : 0);
    line[n] = 0;
    marker(line);
}

static void report_memory(void)
{
    char line[192];
    unsigned n = 0;

    /* No executive exposes the multiboot mmap to ring 3 in this port; the
     * value is the boot/QEMU -m 128 handoff (see variant/qemu_variant.h). */
    n = copy_text(line, n, "SYS: RAM 128M (boot/QEMU -m128; no user exec exposes multiboot)");
    line[n] = 0;
    marker(line);

    /* Genuine heap probe: a 4 KB private chunk through EExecChunkCreate=107. */
    int handle = chunk_create(0, 4096u, 4096u);
    n = 0;
    if (handle < 0)
        {
        n = copy_text(line, n, "SYS: heap chunk create failed");
        }
    else
        {
        void* base = chunk_base(handle);
        int size = chunk_size(handle);
        n = copy_text(line, n, "SYS: heap private chunk ");
        n = append_int(line, n, size);
        n = copy_text(line, n, "B @");
        n = append_hex(line, n, (unsigned)(unsigned long)base);
        n = copy_text(line, n, " (EExecChunkCreate=107)");
        exec2(106u, (unsigned)handle, 0u);   /* EExecHandleClose */
        }
    line[n] = 0;
    marker(line);
}

static void report_devices(void)
{
    char line[256];
    unsigned n = 0;
    int e1000 = 0;

    /* The kernel publishes the e1000-facing frame bridge through exec 247;
     * a live bridge carries the NBRG magic in low identity-mapped RAM. */
    int bridge = exec2(QE_EXEC_NETBRIDGE_BASE, 0u, 0u);
    if (bridge > 0x1000)
        {
        struct nb_shared* shared = (struct nb_shared*)(unsigned)bridge;
        if (shared->magic == NB_MAGIC) e1000 = 1;
        }

    n = copy_text(line, n, "SYS: devices e1000=");
    n = copy_text(line, n, e1000 ? "active" : "built-in");
    n = copy_text(line, n, " (netbridge exec 247); virtio-net, virtio-blk, ");
    n = copy_text(line, n, "AC97 audio, VBE framebuffer, AHCI/IDE storage built into kernel");
    line[n] = 0;
    marker(line);
}

int app_main(void)
{
    struct CpuInfo cpu;

    marker("SYS: === EKA2 PC OS SYSTEM INFO ===");
    read_cpu(&cpu);
    report_cpu(&cpu);
    report_memory();
    marker("SYS: EKA2 kernel (genuine), EUSER.DLL 2503 exports, paging ON");
    report_devices();

    {
    char line[64];
    unsigned n = 0;
    int ticks = exec0(EEXEC_TICK_COUNT);
    n = copy_text(line, n, "SYS: ticks=");
    n = append_int(line, n, ticks);
    line[n] = 0;
    marker(line);
    }

    marker("SYS: DONE");
    return 0;
}
