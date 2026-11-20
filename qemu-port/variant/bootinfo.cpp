// bootinfo.cpp — capture Multiboot §3.3 memory fields.
#include "bootinfo.h"

TUint BootInfo::flags = 0;
TUint BootInfo::memLowerKB = 0;
TUint BootInfo::memUpperKB = 0;
TUint BootInfo::infoAddr = 0;
TUint BootInfo::modsCount = 0;
TUint BootInfo::modStart = 0;
TUint BootInfo::modEnd = 0;

void BootInfo::Capture(TUint aInfoAddr)
    {
    infoAddr = aInfoAddr;
    modsCount = modStart = modEnd = 0;
    if (aInfoAddr < 0x1000u) return;   // sanity: never deref low BIOS area blindly
    TUint fl = *(volatile TUint*)aInfoAddr;
    flags = fl;
    if (fl & 0x01u)
        {
        memLowerKB = *(volatile TUint*)(aInfoAddr + 4);
        memUpperKB = *(volatile TUint*)(aInfoAddr + 8);
        }
    // Multiboot1 §3.3: flags bit 3 => mods_count(+20), mods_addr(+24).
    // Each module entry is { mod_start, mod_end, string, reserved } (16 B).
    if (fl & 0x08u)
        {
        TUint n = *(volatile TUint*)(aInfoAddr + 20);
        TUint a = *(volatile TUint*)(aInfoAddr + 24);
        modsCount = n;
        if (n && a >= 0x1000u)
            {
            modStart = *(volatile TUint*)a;
            modEnd = *(volatile TUint*)(a + 4);
            }
        }
    }
