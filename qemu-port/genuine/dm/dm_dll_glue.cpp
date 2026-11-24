/*
 * dm_dll_glue.cpp -- entry glue for the genuine domainmgr client DLLs
 * (domaincli.dll, domainpolicy.dll) built as E32 DLL images on the port.
 *
 * kernelhwsrv/ is untouched; this file lives in qemu-port/genuine/dm/.
 * --type dll skips the port startup, so this provides `_start`, which prints
 * a load marker through the EUSER IAT slot (ord 915) and exercises a piece
 * of the genuine client code before exiting via exec 115.
 *
 * DM_DLL_LABEL selects the marker prefix and the smoke call:
 *   DMPOLICY -> DmPolicy::GetPolicy() from genuine domainpolicy.cpp
 *   DMCLI    -> load marker only (RDmDomainManager needs RSessionBase, which
 *               the full multi-import pipeline binds to EUSER.DLL)
 */
#include <e32std.h>
#include <u32std.h>
#include <domainpolicy.h>
#include <domainmanager.h>

#ifndef DM_DLL_LABEL
#define DM_DLL_LABEL "DMCLI"
#endif
#define DM_STR2(x) #x
#define DM_STR(x) DM_STR2(x)

extern void (*dm_printf_slot)(const char*, ...);

#define dmarker(...) do { \
    unsigned fn_ = *(unsigned*)(void*)&dm_printf_slot; \
    if (fn_ && fn_ != 915u) \
        ((void (*)(const char*, ...))fn_)(__VA_ARGS__); \
} while (0)

static __attribute__((noreturn)) void dm_exit(int code)
{
    __asm__ volatile(
        "mov $115, %%eax\n\t"
        "mov %0, %%ebx\n\t"
        "mov $0xffff8001, %%ecx\n\t"
        "xor %%edx, %%edx\n\t"
        "xor %%esi, %%esi\n\t"
        "int $0x21\n\t"
        :: "r"(code) : "eax", "ebx", "ecx", "edx", "esi", "memory");
    for (;;) {}
}

extern "C" int _start(void)
{
    dmarker(DM_STR(DM_DLL_LABEL) ": genuine domainmgr client DLL loaded on x86 port\r\n");
#ifdef DM_DLL_IS_POLICY
    TDmHierarchyPolicy policy;
    TInt r = DmPolicy::GetPolicy(policy);   /* genuine domainpolicy.cpp */
    dmarker("DMPOLICY: DmPolicy::GetPolicy -> r=%d failurePolicy=%d\r\n",
           (int)r, (int)policy.iFailurePolicy);
    const TDmDomainSpec* specs = DmPolicy::GetDomainSpecs();
    dmarker("DMPOLICY: DmPolicy::GetDomainSpecs -> root id=%08x\r\n",
           (unsigned)(specs ? specs[0].iId : 0));
#endif
    dmarker(DM_STR(DM_DLL_LABEL) " SMOKE: done, exiting cleanly\r\n");
    dm_exit(0);
    return 0;
}
