// qemu_xhci.cpp — xHCI (USB3) detection for the Kern image: QEMU qemu-xhci,
// VirtualBox xHCI, real HW xHCI ports (where UHCI/OHCI are absent).
//
// Polled controller, command/event/transfer rings, and one direct root-port
// HID boot keyboard. No hubs, hotplug, mouse or multiple xHCI devices yet.
//
// References: xHCI 1.x capability-register model (all MMIO, LE).
// No genuine code involved; Variant-owned.

#include <x86.h>
#include <e32err.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

extern "C" void QemuKbdPushUsb(char aC);

namespace
    {
    TUint32 gBase = 0;  // xHCI capability base (BAR0), 0 = absent
    TBool gInit = EFalse;
    TBool gRunning = EFalse;
    TUint32 gOp, gRt, gDb;
    TUint gEventIndex = 0;
    TUint32 gEventCycle = 1;
    TUint gCommandIndex = 0;
    TUint32 gCommandCycle = 1;
    TUint gPorts = 0;
    TUint8 gPortSlot[32];
    TUint8 gLastSlot = 0;
    TUint32 gContextBytes = 32;
    TUint32 gScratchCount = 0;
    TUint gEpIndex = 0;
    TUint32 gEpCycle = 1;
    TUint8 gAddressedPort = 0xFF;
    TBool gKeyboardReady = EFalse;
    TUint8 gKeyboardEp = 0;
    TUint8 gKeyboardIface = 0;
    TBool gKeyPending = EFalse;
    TUint32 gKeyPointer = 0;
    TUint32 gKeyCycle = 1;
    TUint gKeyIndex = 0;
    TUint gKeyboardPackets = 0;
    TUint gKeyboardKeys = 0;
    TBool gKeyCompleted = EFalse;
    TUint32 gKeyCompletionStatus = 0;
    TUint gKeyboardPort = 0;

    struct Trb { TUint32 pLo, pHi, status, control; };
    struct Erst { TUint32 baseLo, baseHi, size, reserved; };
    __attribute__((aligned(64))) TUint32 gDcbaa[512]; // 256 64-bit pointers
    __attribute__((aligned(64))) TUint32 gScratchArray[32];
    __attribute__((aligned(4096))) TUint8 gScratch[16][4096];
    __attribute__((aligned(64))) Trb gCommands[16];
    __attribute__((aligned(64))) Trb gEvents[32];
    __attribute__((aligned(64))) Erst gErst;
    __attribute__((aligned(64))) TUint8 gInput[4096];
    __attribute__((aligned(64))) TUint8 gDevice[4096];
    __attribute__((aligned(64))) Trb gEp0[16];
    __attribute__((aligned(64))) TUint8 gTransfer[256];
    __attribute__((aligned(64))) Trb gKeyRing[16];
    __attribute__((aligned(64))) TUint8 gKeyReport[8];
    TUint8 gPreviousKeys[6];
    TUint8 gConfiguration = 1;

    TUint32 Rd32(TUint32 aAddr)
        { return *(volatile TUint32*)aAddr; }
    void Wr32(TUint32 aAddr, TUint32 aValue)
        { *(volatile TUint32*)aAddr = aValue; }
    TBool Wait(TUint32 aReg, TUint32 aMask, TUint32 aValue, TUint aMs)
        {
        TUint32 start = NKern::TickCount();
        while ((TUint32)(NKern::TickCount() - start) < aMs)
            if ((Rd32(aReg) & aMask) == aValue)
                return ETrue;
        return (Rd32(aReg) & aMask) == aValue;
        }

    void KeyCompletion(TUint32 aStatus)
        {
        if (gKeyPending)
            {
            gKeyPending = EFalse;
            gKeyCompletionStatus = aStatus;
            gKeyCompleted = ETrue;
            }
        }

    // Shared event consumer: port status changes can precede command and
    // transfer completions. Never leave them blocking the event ring.
    TInt NextEvent(TUint aType, TUint32 aPointer, TUint32& aStatus,
                   TUint32& aControl, TUint aTimeout)
        {
        TUint32 start = NKern::TickCount();
        while ((TUint32)(NKern::TickCount() - start) < aTimeout)
            {
            Trb& e = gEvents[gEventIndex];
            TUint32 ctl = e.control;
            if ((ctl & 1u) != gEventCycle) continue;
            TUint32 ptr = e.pLo;
            aStatus = e.status;
            aControl = ctl;
            ++gEventIndex;
            if (gEventIndex == 32)
                { gEventIndex = 0; gEventCycle ^= 1u; }
            Wr32(gRt + 0x38, (TUint32)&gEvents[gEventIndex] | 8u);
            if (((ctl >> 10) & 0x3Fu) == aType &&
                (ptr & ~0xFu) == aPointer)
                return KErrNone;
            if (((ctl >> 10) & 0x3Fu) == 32u &&
                gKeyPending && (ptr & ~0xFu) == gKeyPointer)
                KeyCompletion(aStatus);
            }
        return KErrTimedOut;
        }
    }

TInt XhciInit()
    {
    if (gInit)
        return gBase ? KErrNone : KErrNotFound;
    gInit = ETrue;
    // Scan bus 0 for 0C.03 with prog-if 0x30 (xHCI, not U/O/EHCI).
    for (unsigned dev = 0; dev < 32; ++dev)
        {
        if ((PciRead32(0, dev, 0, 0) & 0xFFFFu) == 0xFFFFu)
            continue;
        unsigned hdr = (PciRead32(0, dev, 0, 0x0C) >> 16) & 0xFFu;
        unsigned fns = (hdr & 0x80u) ? 8u : 1u;
        for (unsigned fn = 0; fn < fns; ++fn)
            {
            if ((PciRead32(0, dev, fn, 0) & 0xFFFFu) == 0xFFFFu)
                continue;
            unsigned cr = PciRead32(0, dev, fn, 8);
            if ((((cr >> 24) & 0xFFu) != 0x0Cu) ||
                (((cr >> 16) & 0xFFu) != 0x03u) ||
                (((cr >> 8) & 0xFFu) != 0x30u))
                continue;
            TUint32 bar0 = PciRead32(0, dev, fn, 0x10) & 0xFFFFFFF0u;
            if (!bar0 || (PciRead32(0, dev, fn, 0x10) & 1u))
                continue;
            TUint32 cmd = PciRead32(0, dev, fn, 0x04);
            PciWrite32(0, dev, fn, 0x04, cmd | 0x06u);  // MEM + bus master
            gBase = bar0;
            return KErrNone;
            }
        }
    return KErrNotFound;
    }

// Bring the HC to Running with one-command and one-event-ring operation.
// No USB ports are reset here: this path must be safe to probe even when
// there is no attached device, and must never claim keyboard support.
TInt XhciStart()
    {
    if (gRunning)
        return KErrNone;
    TInt r = XhciInit();
    if (r != KErrNone)
        return r;
    TUint32 caps = Rd32(gBase);
    TUint32 hcs1 = Rd32(gBase + 4);
    TUint32 hcs2 = Rd32(gBase + 8);
    TUint32 slots = hcs1 & 0xFFu;
    gPorts = (Rd32(gBase + 4) >> 24) & 0xFFu;
    gScratchCount = ((hcs2 >> 27) & 0x1Fu) << 5;
    gScratchCount |= (hcs2 >> 21) & 0x1Fu;
    if ((caps & 0xFFu) < 0x20u || !slots || slots > 255u)
        return KErrNotSupported;
    if (gScratchCount > 16) return KErrNotSupported;
    if (gPorts > 32) gPorts = 32;
    for (TUint i = 0; i < 32; ++i) gPortSlot[i] = 0;
    gOp = gBase + (caps & 0xFFu);
    gContextBytes = (Rd32(gBase + 0x10) & (1u << 2)) ? 64 : 32;
    gRt = gBase + (Rd32(gBase + 0x18) & ~0x1Fu);
    gDb = gBase + (Rd32(gBase + 0x14) & ~0x3u);
    // USBCMD.RS=0; USBSTS.HCH (bit 0) signals stopped.
    Wr32(gOp, Rd32(gOp) & ~1u);
    if (!Wait(gOp + 4, 1u, 1u, 1000))
        return KErrTimedOut;
    // Hardware reset; wait for CNR to drop before touching operational regs.
    Wr32(gOp, Rd32(gOp) | 2u);
    if (!Wait(gOp, 2u, 0u, 1000) ||
        !Wait(gOp + 4, 1u << 11, 0u, 1000))
        return KErrTimedOut;

    for (TUint i = 0; i < 512; ++i) gDcbaa[i] = 0;
    for (TUint i = 0; i < gScratchCount; ++i)
        {
        gScratchArray[2 * i] = (TUint32)&gScratch[i][0];
        gScratchArray[2 * i + 1] = 0;
        }
    if (gScratchCount) gDcbaa[0] = (TUint32)&gScratchArray[0];
    for (TUint i = 0; i < 16; ++i)
        gCommands[i].pLo = gCommands[i].pHi = gCommands[i].status = gCommands[i].control = 0;
    for (TUint i = 0; i < 32; ++i)
        gEvents[i].pLo = gEvents[i].pHi = gEvents[i].status = gEvents[i].control = 0;
    gEventIndex = 0;
    gEventCycle = 1;
    gCommandIndex = 0;
    gCommandCycle = 1;
    gEpIndex = 0;
    gEpCycle = 1;
    gAddressedPort = 0xFF;
    gKeyboardReady = EFalse;
    gKeyboardPort = 0;
    gKeyPending = EFalse;
    gKeyCompleted = EFalse;
    // Link at the end of the command ring, with Toggle Cycle (TC).
    gCommands[15].pLo = (TUint32)&gCommands[0];
    gCommands[15].control = (6u << 10) | (1u << 1) | gCommandCycle;
    gErst.baseLo = (TUint32)&gEvents[0];
    gErst.baseHi = 0;
    gErst.size = 32;
    gErst.reserved = 0;
    for (TUint i = 0; i < sizeof(gInput); ++i) gInput[i] = 0;
    for (TUint i = 0; i < sizeof(gDevice); ++i) gDevice[i] = 0;
    for (TUint i = 0; i < 16; ++i)
        gEp0[i].pLo = gEp0[i].pHi = gEp0[i].status = gEp0[i].control = 0;
    for (TUint i = 0; i < 16; ++i)
        gKeyRing[i].pLo = gKeyRing[i].pHi = gKeyRing[i].status = gKeyRing[i].control = 0;

    // Page size bit 0 means 4 KiB, as used by this identity-mapped port.
    if (!(Rd32(gOp + 8) & 1u))
        return KErrNotSupported;
    Wr32(gOp + 0x30, (TUint32)&gDcbaa[0]);  // DCBAAP low/high
    Wr32(gOp + 0x34, 0);
    Wr32(gOp + 0x18, (TUint32)&gCommands[0] | 1u); // CRCR + RCS
    Wr32(gOp + 0x1c, 0);
    Wr32(gOp + 0x38, slots);                // CONFIG.MaxSlotsEn
    // Interrupter 0. Disable interrupts: polling event cycle bits instead.
    TUint32 intr = gRt + 0x20;
    Wr32(intr, 0);                          // IMAN.IE=0
    Wr32(intr + 8, 1);                      // ERSTSZ
    Wr32(intr + 0x10, (TUint32)&gErst);     // ERSTBA low/high
    Wr32(intr + 0x14, 0);
    Wr32(intr + 0x18, (TUint32)&gEvents[0]); // ERDP low/high
    Wr32(intr + 0x1c, 0);
    Wr32(gOp, 1u);                          // Run, no INTE
    if (!Wait(gOp + 4, 1u, 0u, 1000))
        return KErrTimedOut;
    gRunning = ETrue;
    return KErrNone;
    }

// Submit one command TRB. A single outstanding command keeps the polled
// completion path deterministic; ring and event-cycle wrap are supported.
static TInt RunCommand(TUint aType, TUint32 aParameter, TUint32 aControl,
                       TUint8* aSlot)
    {
    TInt r = XhciStart();
    if (r != KErrNone)
        return r;
    Trb& command = gCommands[gCommandIndex];
    command.pLo = aParameter;
    command.pHi = 0;
    command.status = 0;
    command.control = (aType << 10) | aControl | gCommandCycle;
    TUint32 commandAddress = (TUint32)&command;
    ++gCommandIndex;
    if (gCommandIndex == 15)
        {
        gCommands[15].control = (6u << 10) | (1u << 1) | gCommandCycle;
        gCommandIndex = 0;
        gCommandCycle ^= 1u;
        }
    __asm__ __volatile__("" : : : "memory");
    Wr32(gDb, 0);                          // doorbell 0, command ring
    TUint32 status = 0, ctl = 0;
    r = NextEvent(33, commandAddress, status, ctl, 1000);
    if (r != KErrNone) return r;
    if (aSlot) *aSlot = (TUint8)(ctl >> 24);
    return ((status >> 24) & 0xFFu) == 1u ? KErrNone : KErrGeneral;
    }

TInt XhciNoOp()
    {
    return RunCommand(23, 0, 0, 0);
    }

// Only claim direct root-hub connections. Downstream hubs need route strings
// and TT handling; neither is present in this first enumeration slice.
TInt XhciPortReset(int aPort)
    {
    TInt r = XhciStart();
    if (r != KErrNone) return r;
    if (aPort < 0 || (TUint)aPort >= gPorts) return KErrArgument;
    TUint32 reg = gOp + 0x400 + 0x10u * (TUint)aPort;
    TUint32 v = Rd32(reg);
    if (!(v & 1u)) return KErrNotFound; // CCS
    if (v & 2u) return KErrNone;         // PED, already enabled (USB3)
    // Preserve power and wake bits. All PORTSC change bits are W1C: don't
    // echo them back, or unrelated connection/reset notifications are lost.
    Wr32(reg, (v & 0x0E00C000u) | (1u << 4)); // PR
    if (!Wait(reg, 1u << 4, 0, 1000)) return KErrTimedOut;
    v = Rd32(reg);
    return (v & 3u) == 3u ? KErrNone : KErrGeneral;
    }

TInt XhciEnableSlot(int aPort)
    {
    TInt r = XhciPortReset(aPort);
    if (r != KErrNone) return r;
    if (gPortSlot[aPort])
        { gLastSlot = gPortSlot[aPort]; return KErrNone; }
    TUint8 slot = 0;
    r = RunCommand(9, 0, 0, &slot);      // Enable Slot Command
    if (r != KErrNone) return r;
    if (!slot) return KErrGeneral;
    gPortSlot[aPort] = slot;
    gLastSlot = slot;
    return KErrNone;
    }

// Address Device (BSR=0) makes the controller perform SET_ADDRESS and
// establishes EP0. One device at a time while the transfer backend is built.
TInt XhciAddress(int aPort)
    {
    if (gAddressedPort == (TUint8)aPort) return KErrNone;
    if (gAddressedPort != 0xFF) return KErrInUse; // one device/context pair
    TInt r = XhciEnableSlot(aPort);
    if (r != KErrNone) return r;
    TUint slot = gPortSlot[aPort];
    if (!slot) return KErrGeneral;
    for (TUint i = 0; i < sizeof(gInput); ++i) gInput[i] = 0;
    for (TUint i = 0; i < sizeof(gDevice); ++i) gDevice[i] = 0;
    for (TUint i = 0; i < 16; ++i)
        gEp0[i].pLo = gEp0[i].pHi = gEp0[i].status = gEp0[i].control = 0;
    // Input control context: Add Slot Context and EP0 Context (A0,A1).
    ((TUint32*)gInput)[1] = 3;
    TUint32 portsc = Rd32(gOp + 0x400 + 0x10u * (TUint)aPort);
    TUint speed = (portsc >> 10) & 0xFu;
    TUint32* slotCtx = (TUint32*)(gInput + gContextBytes);
    slotCtx[0] = (speed << 20) | (1u << 27); // Speed, Context Entries=1
    slotCtx[1] = ((TUint32)(aPort + 1) << 16); // Root Hub Port Number
    TUint32* ep0 = (TUint32*)(gInput + 2 * gContextBytes);
    TUint maxPkt = speed == 4 ? 512 : speed == 3 ? 64 : 8;
    ep0[1] = (3u << 1) | (4u << 3) | (maxPkt << 16); // CErr=3, Control
    ep0[2] = (TUint32)&gEp0[0] | 1u;                 // dequeue, DCS=1
    ep0[3] = 0;
    ep0[4] = 8;                                     // average TRB length
    gDcbaa[2 * slot] = (TUint32)&gDevice[0];
    gDcbaa[2 * slot + 1] = 0;
    __asm__ __volatile__("" : : : "memory");
    r = RunCommand(11, (TUint32)&gInput[0], slot << 24, 0);
    if (r != KErrNone)
        KPrintf("xhci: address port %u slot %u failed %d\r\n", aPort + 1, slot, r);
    else
        gAddressedPort = (TUint8)aPort;
    return r;
    }

// Control transfer, at most 256 bytes. One request at a time, with a
// 3-TRB TD (Setup, optional Data, Status); the HC owns the data buffer
// until Status completion. Use TRT=2/3 for OUT/IN data, TRT=0 for no data.
static TInt Control(int aPort, TUint8 aRequestType, TUint8 aRequest,
                    TUint16 aValue, TUint16 aIndex, TUint aLength, TBool aIn)
    {
    if (aLength > sizeof(gTransfer)) return KErrArgument;
    TInt r = XhciAddress(aPort);
    if (r != KErrNone) return r;
    TUint slot = gPortSlot[aPort];
    if (aIn)
        for (TUint n = 0; n < aLength; ++n) gTransfer[n] = 0;
    TUint i = gEpIndex;
    Trb& setup = gEp0[i];
    setup.pLo = aRequestType | ((TUint32)aRequest << 8) | ((TUint32)aValue << 16);
    setup.pHi = aIndex | (aLength << 16);
    setup.status = 8;
    setup.control = (2u << 10) | (1u << 6) |
        ((aLength ? (aIn ? 3u : 2u) : 0u) << 16) | gEpCycle;
    if (aLength)
        {
        Trb& data = gEp0[i + 1];
        data.pLo = (TUint32)&gTransfer[0];
        data.pHi = 0;
        data.status = aLength;
        data.control = (3u << 10) | (aIn ? (1u << 16) : 0u) | gEpCycle;
        }
    Trb& statusTrb = gEp0[i + (aLength ? 2 : 1)];
    statusTrb.pLo = statusTrb.pHi = statusTrb.status = 0;
    statusTrb.control = (4u << 10) | (1u << 5) |
        (aIn || !aLength ? 0u : (1u << 16)) | gEpCycle;
    TUint32 completion = (TUint32)&statusTrb;
    gEpIndex += aLength ? 3 : 2;
    if (gEpIndex > 12) // leave room for the longest TD before Link
        {
        gEp0[15].pLo = (TUint32)&gEp0[0];
        gEp0[15].pHi = gEp0[15].status = 0;
        gEp0[15].control = (6u << 10) | (1u << 1) | gEpCycle;
        // Fill unused TRBs with No-Op transfer TRBs to reach the Link.
        for (TUint n = gEpIndex; n < 15; ++n)
            {
            gEp0[n].pLo = gEp0[n].pHi = gEp0[n].status = 0;
            gEp0[n].control = (8u << 10) | gEpCycle;
            }
        gEpIndex = 0;
        gEpCycle ^= 1u;
        }
    __asm__ __volatile__("" : : : "memory");
    Wr32(gDb + 4u * slot, 1); // EP0 DCI=1
    TUint32 eventStatus = 0, eventControl = 0;
    r = NextEvent(32, completion, eventStatus, eventControl, 1000);
    if (r != KErrNone) return r;
    return ((eventStatus >> 24) & 0xFFu) == 1u ? KErrNone : KErrGeneral;
    }

TInt XhciGetDescriptor(int aPort, TUint aType, TUint aLength)
    {
    TInt r = Control(aPort, 0x80, 6, aType << 8, 0, aLength, ETrue);
    if (r != KErrNone) return r;
    KPrintf("xhci: descriptor %u len %u: ", aType, aLength);
    for (TUint n = 0; n < aLength && n < 18; ++n)
        KPrintf("%02x ", gTransfer[n]);
    KPrintf("\r\n");
    return KErrNone;
    }

TInt XhciKeyboardInit(int aPort)
    {
    if (gKeyboardReady) return KErrNone;
    TInt r = Control(aPort, 0x80, 6, 0x100, 0, 18, ETrue);
    if (r != KErrNone) return r;
    if (gTransfer[0] != 18 || gTransfer[1] != 1) return KErrCorrupt;
    r = Control(aPort, 0x80, 6, 0x200, 0, 9, ETrue);
    if (r != KErrNone) return r;
    TUint length = gTransfer[2] | ((TUint)gTransfer[3] << 8);
    if (length < 9 || length > sizeof(gTransfer)) return KErrNotSupported;
    r = Control(aPort, 0x80, 6, 0x200, 0, length, ETrue);
    if (r != KErrNone) return r;
    gConfiguration = gTransfer[5];
    TUint8 iface = 0xFF, endpoint = 0, interval = 0;
    TUint16 maxPacket = 0;
    for (TUint o = 0; o + 2 <= length; )
        {
        TUint l = gTransfer[o], type = gTransfer[o + 1];
        if (l < 2 || o + l > length) return KErrCorrupt;
        if (type == 4 && l >= 9)
            iface = (gTransfer[o + 5] == 3 && gTransfer[o + 6] == 1 &&
                     gTransfer[o + 7] == 1) ? gTransfer[o + 2] : 0xFF;
        if (type == 5 && l >= 7 && iface != 0xFF &&
            (gTransfer[o + 2] & 0x80u) && (gTransfer[o + 3] & 3u) == 3u)
            {
            endpoint = gTransfer[o + 2] & 0xFu;
            maxPacket = gTransfer[o + 4] | ((TUint)gTransfer[o + 5] << 8);
            interval = gTransfer[o + 6];
            break;
            }
        o += l;
        }
    if (!endpoint || maxPacket < 8 || !interval || !gConfiguration)
        return KErrNotSupported;
    TUint slot = gPortSlot[aPort];
    TUint dci = 2 * endpoint + 1; // IN endpoint
    if (dci >= 32) return KErrNotSupported;
    // Configure Endpoint input: Drop=0, Add Slot + interrupt-IN context.
    for (TUint n = 0; n < sizeof(gInput); ++n) gInput[n] = 0;
    ((TUint32*)gInput)[1] = 1u | (1u << dci);
    TUint32* slotCtx = (TUint32*)(gInput + gContextBytes);
    TUint speed = (Rd32(gOp + 0x400 + 0x10u * (TUint)aPort) >> 10) & 0xFu;
    slotCtx[0] = (speed << 20) | (dci << 27);
    slotCtx[1] = ((TUint32)(aPort + 1) << 16);
    TUint32* ep = (TUint32*)(gInput + (1 + dci) * gContextBytes);
    ep[0] = (TUint32)(interval - 1) << 16;
    ep[1] = (3u << 1) | (7u << 3) | ((TUint32)maxPacket << 16);
    ep[2] = (TUint32)&gKeyRing[0] | 1u;
    ep[3] = 0;
    ep[4] = 8;
    r = RunCommand(12, (TUint32)&gInput[0], slot << 24, 0);
    if (r != KErrNone) return r;
    r = Control(aPort, 0, 9, gConfiguration, 0, 0, EFalse);
    if (r != KErrNone) return r;
    r = Control(aPort, 0x21, 0x0B, 0, iface, 0, EFalse); // boot protocol
    if (r != KErrNone) return r;
    gKeyboardEp = (TUint8)dci;
    gKeyboardIface = iface;
    gKeyIndex = 0; gKeyCycle = 1; gKeyPending = EFalse;
    gKeyCompleted = EFalse;
    for (TUint n = 0; n < 6; ++n) gPreviousKeys[n] = 0;
    gKeyboardReady = ETrue;
    gKeyboardPort = aPort + 1;
    KPrintf("xhci: HID boot keyboard port %u slot %u ep %u iface %u\r\n",
        aPort + 1, slot, endpoint, iface);
    return KErrNone;
    }

static char KeyChar(TUint8 aKey, TBool aShift)
    {
    if (aKey >= 4 && aKey <= 29)
        return (char)((aShift ? 'A' : 'a') + aKey - 4);
    if (aKey >= 30 && aKey <= 38)
        {
        const char* plain = "123456789";
        const char* shift = "!@#$%^&*(";
        return (aShift ? shift : plain)[aKey - 30];
        }
    switch (aKey)
        {
        case 39: return aShift ? ')' : '0';
        case 40: return '\n';
        case 42: return '\b';
        case 43: return '\t';
        case 44: return ' ';
        case 45: return aShift ? '_' : '-';
        case 46: return aShift ? '+' : '=';
        case 47: return aShift ? '{' : '[';
        case 48: return aShift ? '}' : ']';
        case 49: return aShift ? '|' : '\\';
        case 51: return aShift ? ':' : ';';
        case 52: return aShift ? '"' : '\'';
        case 53: return aShift ? '~' : '`';
        case 54: return aShift ? '<' : ',';
        case 55: return aShift ? '>' : '.';
        case 56: return aShift ? '?' : '/';
        default: return 0;
        }
    }

void XhciKeyboardPoll()
    {
    if (!gKeyboardReady || gAddressedPort == 0xFF) return;
    TUint slot = gPortSlot[gAddressedPort];
    if (!gKeyPending && !gKeyCompleted)
        {
        for (TUint n = 0; n < 8; ++n) gKeyReport[n] = 0;
        Trb& trb = gKeyRing[gKeyIndex];
        trb.pLo = (TUint32)&gKeyReport[0]; trb.pHi = 0;
        trb.status = 8;
        trb.control = (1u << 10) | (1u << 5) | gKeyCycle;
        gKeyPointer = (TUint32)&trb;
        gKeyPending = ETrue;
        ++gKeyIndex;
        if (gKeyIndex == 15)
            {
            gKeyRing[15].pLo = (TUint32)&gKeyRing[0];
            gKeyRing[15].pHi = gKeyRing[15].status = 0;
            gKeyRing[15].control = (6u << 10) | (1u << 1) | gKeyCycle;
            gKeyIndex = 0; gKeyCycle ^= 1u;
            }
        __asm__ __volatile__("" : : : "memory");
        Wr32(gDb + 4u * slot, gKeyboardEp);
        return;
        }
    if (!gKeyCompleted)
        {
        Trb& event = gEvents[gEventIndex];
        if ((event.control & 1u) != gEventCycle) return;
        TUint32 ctl = event.control, ptr = event.pLo, status = event.status;
        ++gEventIndex;
        if (gEventIndex == 32) { gEventIndex = 0; gEventCycle ^= 1u; }
        Wr32(gRt + 0x38, (TUint32)&gEvents[gEventIndex] | 8u);
        if (((ctl >> 10) & 0x3Fu) != 32u || (ptr & ~0xFu) != gKeyPointer)
            return;
        KeyCompletion(status);
        }
    TUint32 status = gKeyCompletionStatus;
    gKeyCompleted = EFalse;
    if (((status >> 24) & 0xFFu) != 1u) return;
    ++gKeyboardPackets;
    TBool shift = (gKeyReport[0] & 0x22u) != 0;
    for (TUint i = 0; i < 6; ++i)
        {
        TUint8 key = gKeyReport[i + 2];
        if (!key || key == 1) continue;
        TBool seen = EFalse;
        for (TUint j = 0; j < 6; ++j)
            if (gPreviousKeys[j] == key) seen = ETrue;
        if (!seen)
            {
            char c = KeyChar(key, shift);
            if (c) { QemuKbdPushUsb(c); ++gKeyboardKeys; }
            }
        }
    for (TUint i = 0; i < 6; ++i) gPreviousKeys[i] = gKeyReport[i + 2];
    }

TBool XhciKeyboardReady()
    { return gKeyboardReady; }

TInt XhciAutoKeyboard()
    {
    TInt r = XhciStart();
    if (r != KErrNone) return r;
    if (gKeyboardReady) return KErrNone;
    if (gAddressedPort != 0xFF) return KErrNotReady;
    for (TUint p = 0; p < gPorts; ++p)
        {
        TUint32 v = Rd32(gOp + 0x400 + 0x10u * p);
        if (!(v & 1u)) continue;
        r = XhciKeyboardInit(p);
        if (r == KErrNone) return r;
        // Only one pair of DMA contexts is provisioned. Do not try another
        // port after a slot has been allocated or addressed.
        if (gPortSlot[p]) return r;
        }
    return KErrNotFound;
    }

void XhciKeyboardInfo()
    {
    if (!gKeyboardReady) return;
    KPrintf("usb: xhci HID keyboard port %u slot %u, %u packet(s), %u key(s)\r\n",
        gKeyboardPort, (TUint)gPortSlot[gAddressedPort],
        gKeyboardPackets, gKeyboardKeys);
    }

void XhciPorts()
    {
    if (XhciStart() != KErrNone)
        { KPrintf("xhci: controller not available\r\n"); return; }
    for (TUint p = 0; p < gPorts; ++p)
        {
        TUint32 v = Rd32(gOp + 0x400 + 0x10u * p);
        if (!(v & 1u)) continue;
        KPrintf("xhci: port %u status=%x speed=%u slot=%u\r\n",
            p + 1, v, (v >> 10) & 0xFu, gPortSlot[p]);
        }
    if (gLastSlot)
        KPrintf("xhci: last allocated slot %u%s\r\n", gLastSlot,
            gAddressedPort != 0xFF ? " (addressed)" : " (not addressed)");
    }

void XhciInfo()
    {
    if (XhciInit() != KErrNone || !gBase)
        {
        KPrintf("xhci: no xHCI controller (PCI 0C.03.30)\r\n");
        return;
        }
    TUint8 capLen = (TUint8)(Rd32(gBase) & 0xFFu);
    TUint16 ver = (TUint16)((Rd32(gBase) >> 16) & 0xFFFFu);
    TUint32 hcs1 = Rd32(gBase + 4);
    TUint32 hcs2 = Rd32(gBase + 8);
    TUint32 hcc = Rd32(gBase + 0x10);
    TUint32 dboff = Rd32(gBase + 0x14);
    TUint32 rtsoff = Rd32(gBase + 0x18);
    KPrintf("xhci: cap=%x ver=%x.%02x slots=%u ports=%u\r\n",
        gBase, (ver >> 8) & 0xFFu, ver & 0xFFu,
        hcs1 & 0xFFu, (hcs1 >> 24) & 0xFFu);
    KPrintf("xhci: caplen=%u dboff=%x rtsoff=%x hcc=%x hcs2=%x\r\n",
        capLen, dboff, rtsoff, hcc, hcs2);
    KPrintf("xhci: controller %s\r\n", gRunning ? "running" : "not started");
    }
