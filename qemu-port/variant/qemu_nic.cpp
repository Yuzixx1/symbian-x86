// qemu_nic.cpp — polled e1000 (82540EM, PCI 8086:100E) Ethernet for the
// full Kern image on QEMU q35. Variant-owned; no genuine code involved.
//
// Discovery scans bus 0 by vendor/device (BAR0 = 32-bit memory), enables
// MEM|MASTER, resets the MAC, reads the MAC from EEPROM words 0..2 via EERD,
// and sets up 8-entry legacy TX/RX descriptor rings with 2048-byte buffers.
// Identity-mapped RAM => DMA addresses are direct virtual addresses of the
// static rings/buffers. Polled only: no PCI IRQ/ISR bound yet; e1000_irq()
// just acknowledges ICR and refreshes cached link state.
//
// Public API (qemu_variant.h): e1000_init / e1000_send / e1000_recv /
// e1000_link_up. Bring-up helpers: e1000_reset / e1000_read_mac /
// e1000_init_tx / e1000_init_rx / e1000_tx / e1000_irq. A static constructor
// calls e1000_init() at boot from HwInit0's __init_array walk.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>
#include "qemu_variant.h"

namespace
    {
    enum
        {
        kVendor = 0x8086u,
        kDevice = 0x100Eu,

        kCtrl        = 0x00000u, kStatus      = 0x00008u, kIcr        = 0x000C0u,
        kEerd        = 0x00014u,
        kRctl        = 0x00100u, kTctl        = 0x00400u,
        kRdbal       = 0x02800u, kRdbah       = 0x02804u, kRdlen      = 0x02808u,
        kRdh         = 0x02810u, kRdt         = 0x02818u,
        kTdbal       = 0x03800u, kTdbah       = 0x03804u, kTdlen      = 0x03808u,
        kTdh         = 0x03810u, kTdt         = 0x03818u,
        kRa          = 0x05400u, kRah         = 0x05404u,

        kCtrlRst     = 0x04000000u,
        kCtrlSlu     = 0x00000040u,
        kCtrlAsde    = 0x00000020u,
        kStatusLu    = 0x00000002u,
        kIcrLsc      = 0x00000004u,   // link status change
        kIcrRxt      = 0x00000040u,   // receive timer / RX packet pending

        kEerdStart   = 0x00000001u,
        kEerdDone    = 0x00000010u,   // QEMU flash_eerd_read(): DONE is bit 4
        kEerdAddrSh  = 8,
        kEerdDataSh  = 16,

        kRctlEn      = 0x00000002u,
        kRctlBam     = 0x00008000u,
        kRctlSecrc   = 0x04000000u,

        kTctlEn      = 0x00000002u,
        kTctlPsp     = 0x00000008u,
        kTctlCt      = 0x00000ff0u,
        kTctlCold    = 0x003ff000u,

        kTxdEop      = 0x01000000u,
        kTxdIfcs     = 0x02000000u,
        kTxdRs       = 0x08000000u,

        kRxdDd       = 0x01u,
        kRxdEop      = 0x02u,

        kRahAv       = 0x80000000u,

        kNumDesc     = 8,
        kBufSize     = 2048,
        kDescSize    = 16,             // legacy descriptor size
        };

    const TInt kSpinReset  = 100000;
    const TInt kSpinEeprom = 100000;
    const TInt kSpinTx     = 1000000;

    struct TxDesc
        {
        volatile TUint64 iAddr;
        volatile TUint32 iLower;  // length[15:0] | cso[23:16] | cmd[31:24]
        volatile TUint32 iUpper;  // status[7:0] | css | special
        };
    struct RxDesc
        {
        volatile TUint64 iAddr;
        volatile TUint16 iLength;
        volatile TUint16 iCsum;
        volatile TUint8  iStatus;
        volatile TUint8  iErrors;
        volatile TUint16 iSpecial;
        };

    __attribute__((aligned(16))) static TxDesc gTxRing[kNumDesc];
    __attribute__((aligned(16))) static RxDesc gRxRing[kNumDesc];
    __attribute__((aligned(16))) static TUint8 gTxBuf[kNumDesc][kBufSize];
    __attribute__((aligned(16))) static TUint8 gRxBuf[kNumDesc][kBufSize];

    static TUint32 gBase = 0;
    static TUint8  gMac[6];
    static TUint   gRxNext = 0;        // our RX consume cursor
    static TBool   gLinkUp = EFalse;
    static TBool   gInit = EFalse;

    static inline TUint32 Rd32(TUint32 aOff)
        { return *(volatile TUint32*)(gBase + aOff); }
    static inline void Wr32(TUint32 aOff, TUint32 aVal)
        { *(volatile TUint32*)(gBase + aOff) = aVal; }
    static inline void Barrier()
        { __asm__ __volatile__("" : : : "memory"); }

    // EERD bit-bang-free read (82540EM): write (word<<8)|START, poll DONE.
    TUint16 EepromRead(TUint aWord)
        {
        Wr32(kEerd, (aWord << kEerdAddrSh) | kEerdStart);
        for (TInt i = 0; i < kSpinEeprom; ++i)
            {
            TUint32 v = Rd32(kEerd);
            if (v & kEerdDone)
                return (TUint16)((v >> kEerdDataSh) & 0xFFFFu);
            }
        return 0;
        }
    }  // namespace

TInt e1000_reset()
    {
    if (!gBase)
        return KErrNotReady;
    // Global reset (RST is self-clearing).
    Wr32(kCtrl, kCtrlRst);
    for (TInt i = 0; i < kSpinReset; ++i)
        if (!(Rd32(kCtrl) & kCtrlRst))
            break;
    // Force link up and enable address-space decoding.
    Wr32(kCtrl, kCtrlSlu | kCtrlAsde);
    gLinkUp = TBool((Rd32(kStatus) & kStatusLu) != 0);
    return KErrNone;
    }

void e1000_read_mac(TUint8* aMac)
    {
    if (!aMac)
        return;
    // EEPROM words 0..2, low byte first (QEMU e1000 template).
    for (TUint i = 0; i < 3; ++i)
        {
        TUint16 w = EepromRead(i);
        aMac[2 * i]     = (TUint8)(w & 0xFFu);
        aMac[2 * i + 1] = (TUint8)(w >> 8);
        }
    }

TInt e1000_init_tx()
    {
    if (!gBase)
        return KErrNotReady;
    for (TUint i = 0; i < kNumDesc; ++i)
        {
        TxDesc& t = gTxRing[i];
        t.iAddr = 0; t.iLower = 0; t.iUpper = 0;
        }
    Barrier();
    Wr32(kTdbal, (TUint32)gTxRing); Wr32(kTdbah, 0);
    Wr32(kTdlen, kNumDesc * kDescSize);
    Wr32(kTdh, 0); Wr32(kTdt, 0);
    Wr32(kTctl, kTctlEn | kTctlPsp | kTctlCt | kTctlCold);
    return KErrNone;
    }

TInt e1000_init_rx()
    {
    if (!gBase)
        return KErrNotReady;
    for (TUint i = 0; i < kNumDesc; ++i)
        {
        RxDesc& r = gRxRing[i];
        r.iAddr = (TUint64)(TUint32)gRxBuf[i];
        r.iLength = 0; r.iCsum = 0; r.iStatus = 0;
        r.iErrors = 0; r.iSpecial = 0;
        }
    Barrier();
    Wr32(kRdbal, (TUint32)gRxRing); Wr32(kRdbah, 0);
    Wr32(kRdlen, kNumDesc * kDescSize);
    Wr32(kRdh, 0);
    // Tail = last descriptor: hardware gets indices 0..6 initially.
    Wr32(kRdt, kNumDesc - 1);
    Wr32(kRctl, kRctlEn | kRctlBam | kRctlSecrc);
    gRxNext = 0;
    return KErrNone;
    }

TInt e1000_init()
    {
    if (gInit)
        return gBase ? KErrNone : KErrNotFound;
    gInit = ETrue;

    // Discover 82540EM by vendor/device ID on bus 0.
    TUint bus = 0, dev = 0, fn = 0;
    TBool found = EFalse;
    for (TUint d = 0; d < 32 && !found; ++d)
        {
        if ((PciRead32(0, d, 0, 0) & 0xFFFFu) == 0xFFFFu)
            continue;
        TUint fns = ((PciRead32(0, d, 0, 0x0C) >> 16) & 0x80u) ? 8u : 1u;
        for (TUint f = 0; f < fns; ++f)
            {
            TUint id = PciRead32(0, d, f, 0);
            if ((id & 0xFFFFu) == kVendor && ((id >> 16) & 0xFFFFu) == kDevice)
                { bus = 0; dev = d; fn = f; found = ETrue; break; }
            }
        }
    if (!found)
        {
        KPrintf("e1000: no PCI 8086:100E found\r\n");
        return KErrNotFound;
        }

    TUint32 bar0 = PciRead32(bus, dev, fn, 0x10) & 0xFFFFFFF0u;
    if (!bar0)
        { KPrintf("e1000: BAR0 unassigned\r\n"); return KErrGeneral; }
    PciWrite32(bus, dev, fn, 0x04, PciRead32(bus, dev, fn, 0x04) | 0x06u);
    gBase = bar0;

    TInt r = e1000_reset();
    if (r != KErrNone)
        return r;
    e1000_read_mac(gMac);

    // Receive address (RAL/RAH), AV bit set.
    TUint32 raLo = 0, raHi = kRahAv;
    for (TUint i = 0; i < 4; ++i)
        {
        raLo |= (TUint32)gMac[i] << (8 * i);
        if (i < 2)
            raHi |= (TUint32)gMac[i + 4] << (8 * i);
        }
    Wr32(kRa, raLo); Wr32(kRah, raHi);

    r = e1000_init_tx();
    if (r != KErrNone)
        return r;
    r = e1000_init_rx();
    if (r != KErrNone)
        return r;

    KPrintf("e1000: BAR0=%x MAC=%02x:%02x:%02x:%02x:%02x:%02x link=%d\r\n",
        gBase, gMac[0], gMac[1], gMac[2], gMac[3], gMac[4], gMac[5],
        (TInt)gLinkUp);
    return KErrNone;
    }

TInt e1000_tx(const TUint8* aBuf, TUint aLen)
    {
    if (!gBase && e1000_init() != KErrNone)
        return KErrNotReady;
    if (!aBuf || !aLen || aLen > kBufSize)
        return KErrArgument;

    // Wait until the previously posted descriptor has been reclaimed.
    TInt spin = 0;
    for (; spin < kSpinTx; ++spin)
        if (Rd32(kTdh) == Rd32(kTdt))
            break;
    if (spin == kSpinTx)
        return KErrTimedOut;

    TUint idx = (TUint)(Rd32(kTdt) & 0xFFFFu);
    if (idx >= kNumDesc)
        idx = 0;
    for (TUint i = 0; i < aLen; ++i)
        gTxBuf[idx][i] = aBuf[i];
    TxDesc& d = gTxRing[idx];
    d.iAddr  = (TUint64)(TUint32)gTxBuf[idx];
    d.iLower = (aLen & 0xFFFFu) | kTxdEop | kTxdIfcs | kTxdRs;
    d.iUpper = 0;
    Barrier();
    // Writing TDT kicks QEMU's start_xmit(). Caller supplies the frame
    // (an ARP request during NIC bring-up); no protocol is built here.
    Wr32(kTdt, (idx + 1) & 0xFFFFu);
    return KErrNone;
    }

TInt e1000_send(const TUint8* aBuf, TUint aLen)
    { return e1000_tx(aBuf, aLen); }

TInt e1000_recv(TUint8* aBuf, TUint aMax)
    {
    if (!gBase && e1000_init() != KErrNone)
        return 0;
    if (!aBuf || !aMax)
        return 0;

    Barrier();
    RxDesc& d = gRxRing[gRxNext];
    if (!(d.iStatus & kRxdDd))
        return 0;                       // nothing ready (non-blocking)

    TUint len = d.iLength;
    if (len > kBufSize)
        len = kBufSize;
    if (len > aMax)
        len = aMax;
    for (TUint i = 0; i < len; ++i)
        aBuf[i] = gRxBuf[gRxNext][i];

    // Recycle the descriptor and hand it back to the hardware. QEMU's e1000
    // treats RDT as an exclusive tail: RDH == RDT means "no RX buffers", so
    // the value written must be the index of the descriptor we just consumed
    // (not the next cursor). Writing the next cursor would stall RX after the
    // first packet.
    TUint consumed = gRxNext;
    d.iAddr = (TUint64)(TUint32)gRxBuf[consumed];
    d.iLength = 0; d.iCsum = 0; d.iStatus = 0; d.iErrors = 0; d.iSpecial = 0;
    gRxNext = (consumed + 1) % kNumDesc;
    Barrier();
    Wr32(kRdt, consumed);
    return (TInt)len;
    }

TBool e1000_link_up()
    {
    if (!gBase && e1000_init() != KErrNone)
        return EFalse;
    return TBool((Rd32(kStatus) & kStatusLu) != 0);
    }

TInt e1000_irq()
    {
    if (!gBase && e1000_init() != KErrNone)
        return 0;
    // No PCI IRQ is bound yet (polled discipline, like qemu_ata/qemu_ahci).
    // A poll therefore acts as the ISR: read ICR, ACK by writing it back,
    // and refresh cached link state on LSC. Returns the raw ICR bits.
    TUint32 icr = Rd32(kIcr);
    if (icr)
        Wr32(kIcr, icr);
    if (icr & kIcrLsc)
        gLinkUp = TBool((Rd32(kStatus) & kStatusLu) != 0);
    return (TInt)icr;
    }

// Boot hook: HwInit0 walks the .init_array after paging + TheAsic are ready.
__attribute__((constructor)) static void E1000BootInit()
    { e1000_init(); }
