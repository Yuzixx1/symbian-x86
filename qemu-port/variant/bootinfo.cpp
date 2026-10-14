// bootinfo.cpp — capture Multiboot §3.3 memory fields.
#include "bootinfo.h"

TUint BootInfo::flags = 0;
TUint BootInfo::memLowerKB = 0;
TUint BootInfo::memUpperKB = 0;
TUint BootInfo::infoAddr = 0;

void BootInfo::Capture(TUint aInfoAddr)
    {
    infoAddr = aInfoAddr;
    if (aInfoAddr < 0x1000u) return;   // sanity: never deref low BIOS area blindly
    TUint fl = *(volatile TUint*)aInfoAddr;
    flags = fl;
    if (fl & 0x01u)
        {
        memLowerKB = *(volatile TUint*)(aInfoAddr + 4);
        memUpperKB = *(volatile TUint*)(aInfoAddr + 8);
        }
    }
