// qemu_ata.cpp — ATA PIO driver (primary+secondary IDE) for the Kern image.
//
// Covers what the storage phase needs: device detection (IDENTIFY, ATA vs
// ATAPI), LBA28 sector read/write with polled status (no IRQ wiring), model
// strings for the shell. PIO polling is the bring-up-correct choice; UDMA
// can replace the transfer loops later without touching callers.
//
// References: ATA-ATAPI-6 register model (status bits, IDENTIFY layout).
// No genuine code involved; Variant-owned driver.

#include <x86.h>
#include <e32err.h>
#include "qemu_variant.h"

static inline void Out8(unsigned aPort, unsigned char aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "d"((unsigned short)aPort)); }
static inline void Out16(unsigned aPort, unsigned short aVal)
    { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "d"((unsigned short)aPort)); }
static inline unsigned char In8(unsigned aPort)
    { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "d"((unsigned short)aPort)); return v; }
static inline unsigned short In16(unsigned aPort)
    { unsigned short v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "d"((unsigned short)aPort)); return v; }

namespace
    {
    struct Chan { unsigned cmd, ctl; };
    const Chan kChans[2] = { {0x1F0, 0x3F6}, {0x170, 0x376} };

    enum
        {
        kStErr = 0x01, kStDrq = 0x08, kStDf = 0x20,
        kStRdy = 0x40, kStBsy = 0x80,
        kCmdIdentify = 0xEC, kCmdRead = 0x20, kCmdWrite = 0x30,
        kCmdFlush = 0xE7,
        };

    // Generous iteration budgets: instant under QEMU, seconds on real iron.
    const TInt kSpinBusy = 30000000;
    const TInt kSpinDrq = 10000000;

    void Delay400ns(unsigned aCtl)
        {
        In8(aCtl); In8(aCtl); In8(aCtl); In8(aCtl);
        }

    TInt WaitNotBusy(unsigned aCmd)
        {
        for (TInt i = 0; i < kSpinBusy; ++i)
            if (!(In8(aCmd + 7) & kStBsy))
                return KErrNone;
        return KErrTimedOut;
        }

    // buf Words must already be selected; returns when DRQ set or error.
    TInt WaitDrq(unsigned aCmd, unsigned aCtl)
        {
        Delay400ns(aCtl);
        for (TInt i = 0; i < kSpinDrq; ++i)
            {
            unsigned char s = In8(aCmd + 7);
            if (s & kStBsy) continue;
            if (s & (kStErr | kStDf)) return KErrGeneral;
            if (s & kStDrq) return KErrNone;
            }
        return KErrTimedOut;
        }

    void Select(unsigned aCmd, int aSlave, TUint32 aLbaTop)
        {
        Out8(aCmd + 6, (unsigned char)(0xE0 | (aSlave ? 0x10 : 0) | (aLbaTop & 0x0F)));
        Delay400ns(aCmd == kChans[0].cmd ? kChans[0].ctl : kChans[1].ctl);
        }

    struct AtaDeviceG
        {
        TBool present;
        TBool atapi;
        TUint32 sectors;
        char model[41];
        char serial[21];
        };
    AtaDeviceG gDevs[4];
    TInt gProbed = 0;

    void SwapStr(char* aDst, const unsigned short* aSrc, TInt aWords, TInt aMax)
        {
        TInt k = 0;
        for (TInt i = 0; i < aWords && k < aMax - 1; ++i)
            {
            unsigned short w = aSrc[i];
            char hi = (char)(w >> 8), lo = (char)(w & 0xFF);
            if (hi == 0) hi = ' ';
            if (lo == 0) lo = ' ';
            aDst[k++] = hi;
            if (k < aMax - 1) aDst[k++] = lo;
            }
        while (k > 0 && aDst[k - 1] == ' ')
            --k;
        aDst[k] = 0;
        }
    }

TInt AtaInit()
    {
    for (int d = 0; d < 4; ++d)
        {
        AtaDeviceG& dev = gDevs[d];
        dev.present = EFalse;
        dev.atapi = EFalse;
        dev.sectors = 0;
        dev.model[0] = 0;
        dev.serial[0] = 0;
        int ch = d / 2, slave = d % 2;
        unsigned cmd = kChans[ch].cmd, ctl = kChans[ch].ctl;
        Select(cmd, slave, 0);
        Out8(ctl, 0x02);  // nIEN off, SRST off (plain selection)
        if (WaitNotBusy(cmd) != KErrNone)
            continue;
        Out8(cmd + 7, kCmdIdentify);
        Delay400ns(ctl);
        unsigned char s = In8(cmd + 7);
        if (s == 0)
            continue;  // nothing there
        // ATAPI signature check (IDENTIFY aborts on packet devices).
        unsigned char cl = In8(cmd + 4), chh = In8(cmd + 5);
        if (s & kStErr)
            {
            if (cl == 0x14 && chh == 0xEB)
                {
                dev.present = ETrue;
                dev.atapi = ETrue;
                }
            continue;
            }
        if (WaitDrq(cmd, ctl) != KErrNone)
            continue;
        unsigned short id[256];
        for (int i = 0; i < 256; ++i)
            id[i] = In16(cmd + 0);
        dev.present = ETrue;
        dev.sectors = (TUint32)id[60] | ((TUint32)id[61] << 16);
        SwapStr(dev.model, id + 27, 20, (TInt)sizeof(dev.model));
        SwapStr(dev.serial, id + 10, 10, (TInt)sizeof(dev.serial));
        }
    gProbed = 1;
    int n = 0;
    for (int d = 0; d < 4; ++d)
        if (gDevs[d].present && !gDevs[d].atapi)
            ++n;
    return n;
    }

TInt AtaCount()
    {
    if (!gProbed)
        AtaInit();
    int n = 0;
    for (int d = 0; d < 4; ++d)
        if (gDevs[d].present && !gDevs[d].atapi)
            ++n;
    return n + AhciCount();  // dev 4..7 = AHCI ports 0..3
    }

TBool AtaPresent(int aDev)
    {
    if (aDev >= 4)
        return AhciPresent(aDev - 4);
    if (aDev < 0 || aDev > 3 || !gProbed)
        return EFalse;
    return (TBool)(gDevs[aDev].present && !gDevs[aDev].atapi);
    }

TUint32 AtaSectors(int aDev)
    {
    if (aDev >= 4)
        return AhciSectors(aDev - 4);
    if (!AtaPresent(aDev))
        return 0;
    return gDevs[aDev].sectors;
    }

const char* AtaModel(int aDev)
    {
    if (aDev >= 4)
        return AhciModel(aDev - 4);
    if (!AtaPresent(aDev))
        return "(absent)";
    return gDevs[aDev].model;
    }

TInt AtaRead(int aDev, TUint32 aLba, TUint aCount, TUint8* aBuf)
    {
    if (aDev >= 4)
        return AhciRead(aDev - 4, aLba, aCount, aBuf);
    if (!AtaPresent(aDev) || !aBuf || !aCount)
        return KErrArgument;
    if (aLba + aCount < aLba || aLba + aCount > gDevs[aDev].sectors)
        return KErrArgument;  // LBA28 range, no wrap
    int ch = aDev / 2, slave = aDev % 2;
    unsigned cmd = kChans[ch].cmd, ctl = kChans[ch].ctl;
    while (aCount)
        {
        TUint n = aCount > 256 ? 256 : aCount;
        Select(cmd, slave, aLba >> 24);
        if (WaitNotBusy(cmd) != KErrNone)
            return KErrTimedOut;
        Out8(cmd + 2, (unsigned char)(n == 256 ? 0 : n));
        Out8(cmd + 3, (unsigned char)aLba);
        Out8(cmd + 4, (unsigned char)(aLba >> 8));
        Out8(cmd + 5, (unsigned char)(aLba >> 16));
        Out8(cmd + 7, kCmdRead);
        for (TUint s = 0; s < (n == 256 ? 256u : n); ++s)
            {
            TInt r = WaitDrq(cmd, ctl);
            if (r != KErrNone)
                return r;
            unsigned short* w = (unsigned short*)aBuf;
            for (int i = 0; i < 256; ++i)
                w[i] = In16(cmd + 0);
            aBuf += 512;
            }
        aLba += (n == 256 ? 256u : n);
        aCount -= (n == 256 ? 256u : n);
        }
    return KErrNone;
    }

TInt AtaWrite(int aDev, TUint32 aLba, TUint aCount, const TUint8* aBuf)
    {
    if (aDev >= 4)
        return AhciWrite(aDev - 4, aLba, aCount, aBuf);
    if (!AtaPresent(aDev) || !aBuf || !aCount)
        return KErrArgument;
    if (aLba + aCount < aLba || aLba + aCount > gDevs[aDev].sectors)
        return KErrArgument;
    int ch = aDev / 2, slave = aDev % 2;
    unsigned cmd = kChans[ch].cmd, ctl = kChans[ch].ctl;
    while (aCount)
        {
        TUint n = aCount > 256 ? 256 : aCount;
        Select(cmd, slave, aLba >> 24);
        if (WaitNotBusy(cmd) != KErrNone)
            return KErrTimedOut;
        Out8(cmd + 2, (unsigned char)(n == 256 ? 0 : n));
        Out8(cmd + 3, (unsigned char)aLba);
        Out8(cmd + 4, (unsigned char)(aLba >> 8));
        Out8(cmd + 5, (unsigned char)(aLba >> 16));
        Out8(cmd + 7, kCmdWrite);
        for (TUint s = 0; s < (n == 256 ? 256u : n); ++s)
            {
            TInt r = WaitDrq(cmd, ctl);
            if (r != KErrNone)
                return r;
            const unsigned short* w = (const unsigned short*)aBuf;
            for (int i = 0; i < 256; ++i)
                Out16(cmd + 0, w[i]);
            aBuf += 512;
            }
        // Cache flush so VirtualBox/real iron can't reorder past us.
        Out8(cmd + 7, kCmdFlush);
        if (WaitNotBusy(cmd) != KErrNone)
            return KErrTimedOut;
        aLba += (n == 256 ? 256u : n);
        aCount -= (n == 256 ? 256u : n);
        }
    return KErrNone;
    }
