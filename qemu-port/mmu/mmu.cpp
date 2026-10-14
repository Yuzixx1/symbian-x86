// mmu.cpp — C++ side of the ported Direct-model MMU enablement.
#include "qemu_assp.h"

extern "C" TUint Mmu_BuildIdentityMap(void);
extern "C" TUint Mmu_Enable(void);   // returns CR0 after enable
extern "C" void Mmu_Status(void);    // EAX=CR0 EDX=CR3 ECX=CR4 (asm)

namespace QemuMmu
    {
    void BuildIdentityMap() { Mmu_BuildIdentityMap(); }
    TUint Enable() { return Mmu_Enable(); }
    void Status(TUint& aCr0, TUint& aCr3, TUint& aCr4)
        {
        TUint cr0, cr3, cr4;
        __asm__ __volatile__("call Mmu_Status" : "=a"(cr0), "=d"(cr3), "=c"(cr4) :: "memory");
        aCr0 = cr0; aCr3 = cr3; aCr4 = cr4;
        }
    // Touch test through paging: write/read-back a data word + stack probe.
    TInt TouchTest()
        {
        static volatile TUint sProbe = 0;
        sProbe = 0xA5A55A5Au;
        if (sProbe != 0xA5A55A5Au) return -1;
        volatile TUint stackProbe = 0x12345678u;
        if (stackProbe != 0x12345678u) return -2;
        return 0;
        }
    }
