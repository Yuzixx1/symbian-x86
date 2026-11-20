// qemu_netbridge.cpp — kernel-side frame bridge between the polled e1000
// (variant/qemu_nic.cpp) and ring-3 E32 user processes.
//
// A single static struct nb_shared lives in low, identity-mapped RAM. Its two
// rings are the hand-off: userland posts raw Ethernet frames into tx and the
// kernel drains them with e1000_tx(); received frames from e1000_recv() are
// placed into rx for userland.  The buffer base/size are exposed through two
// dedicated slow executives (247/248) installed by patching two invalid slots
// of the genuine EpocSlowExecTable — the same proven mechanism SvcMgr uses for
// 268/269/270 (kernelhwsrv/ is read-only).
//
// Transport choice: two dedicated execs (rather than a named global chunk).
// A genuine RChunk ("!NetFrames") always allocates its own RAM via
// MM::AllocRegion; there is no API to back it with a caller-supplied buffer,
// so making it alias the e1000-facing rings would require invasive changes to
// read-only kernel code.  Low RAM is identity-mapped and U/S=1 in user PDs, so
// returning the physical base is sufficient and far less invasive.

#include <x86.h>
#include <e32err.h>
#include <nk_trace.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"
#include "../user/netbridge.h"

extern const TUint32 EpocSlowExecTable[];

namespace
    {
    __attribute__((aligned(4096))) static struct nb_shared gNetBridge;
    static TUint32 gBridgeBase = 0;
    static TBool gBridgeInit = EFalse;

    static inline void Fence()
        { __asm__ __volatile__("" : : : "memory"); }

    // exec 247: bridge physical (= virtual) base address.
    static TInt NetBridgeBaseExec(TInt, TInt)
        { return (TInt)gBridgeBase; }

    // exec 248: bridge byte size.
    static TInt NetBridgeSizeExec(TInt, TInt)
        { return (TInt)sizeof(gNetBridge); }

    static void NetBridgeExecInstall()
        {
        TUint32* tab = (TUint32*)EpocSlowExecTable;
        // Need slots 0..248; the table count is 271 in this build.
        if (tab[0] < 249u)
            {
            KPrintf("netbridge: exec table count %u, install skipped\r\n", tab[0]);
            return;
            }
        const TInt slots[2] = {(TInt)QE_EXEC_NETBRIDGE_BASE,
                               (TInt)QE_EXEC_NETBRIDGE_SIZE};
        TInt (*fns[2])(TInt, TInt) = {&NetBridgeBaseExec, &NetBridgeSizeExec};
        for (TInt i = 0; i < 2; ++i)
            {
            const TUint s = (TUint)slots[i];
            const TUint fnIdx = 4u + 2u * s;
            // Only repurpose slots still pointing at InvalidExecHandler (tab[1]).
            if (tab[fnIdx] != tab[1] && tab[fnIdx] != 0u)
                {
                KPrintf("netbridge: slot %d not invalid (%x), skip\r\n",
                    slots[i], tab[fnIdx]);
                continue;
                }
            tab[3u + 2u * s] = 0;                       // flags
            tab[fnIdx] = (TUint32)(TLinAddr)fns[i];      // handler
            }
        }
    }  // namespace

TInt netbridge_init()
    {
    if (gBridgeInit)
        return gBridgeBase ? KErrNone : KErrNotFound;
    gBridgeInit = ETrue;

    if (e1000_init() != KErrNone)
        return KErrNotFound;

    // Zero the whole bridge region (identity-mapped low RAM -> user visible).
    TUint8* p = (TUint8*)&gNetBridge;
    for (TUint i = 0; i < sizeof(gNetBridge); ++i)
        p[i] = 0;

    e1000_read_mac((TUint8*)gNetBridge.mac);
    gNetBridge.version = NB_VERSION;
    gNetBridge.tx.head = gNetBridge.tx.tail = 0;
    gNetBridge.rx.head = gNetBridge.rx.tail = 0;
    Fence();
    gNetBridge.magic = NB_MAGIC;          // publish last
    gBridgeBase = (TUint32)&gNetBridge;

    NetBridgeExecInstall();

    KPrintf("netbridge: base=%x size=%u mac=%02x:%02x:%02x:%02x:%02x:%02x "
        "(execs %u/%u)\r\n", gBridgeBase, (TUint)sizeof(gNetBridge),
        gNetBridge.mac[0], gNetBridge.mac[1], gNetBridge.mac[2],
        gNetBridge.mac[3], gNetBridge.mac[4], gNetBridge.mac[5],
        (TUint)QE_EXEC_NETBRIDGE_BASE, (TUint)QE_EXEC_NETBRIDGE_SIZE);
    return KErrNone;
    }

struct nb_ring* netbridge_tx_ring()
    { return gBridgeInit ? &gNetBridge.tx : 0; }

struct nb_ring* netbridge_rx_ring()
    { return gBridgeInit ? &gNetBridge.rx : 0; }

TUint32 netbridge_base()
    { return gBridgeBase; }

TUint netbridge_size()
    { return (TUint)sizeof(gNetBridge); }

TInt netbridge_poll()
    {
    if (!gBridgeInit)
        return KErrNotReady;

    TInt posted = 0, received = 0;

    // Drain user -> kernel TX ring: hand every ready frame to the NIC.
    struct nb_ring* tx = &gNetBridge.tx;
    for (TInt n = 0; n < (TInt)NB_RING_SLOTS; ++n)
        {
        const TUint i = tx->tail;
        struct nb_slot* s = &tx->slot[i];
        Fence();
        const TUint len = s->len;
        if (!len)
            break;                          // ordered ring empty
        if (len <= NB_FRAME_SIZE)
            if (e1000_tx((const TUint8*)s->data, len) == KErrNone)
                ++posted;
        s->len = 0;                          // release back to producer
        Fence();
        tx->tail = (i + 1u) % NB_RING_SLOTS;
        }

    // Fill kernel -> user RX ring from the NIC (one frame per poll).
    struct nb_ring* rx = &gNetBridge.rx;
    const TUint ri = rx->head;
    struct nb_slot* rs = &rx->slot[ri];
    Fence();
    if (!rs->len)                            // slot free
        {
        static TUint8 tmp[NB_FRAME_SIZE];
        TInt len = e1000_recv(tmp, sizeof(tmp));
        if (len > 0)
            {
            for (TInt k = 0; k < len; ++k)
                rs->data[k] = tmp[k];
            rs->len = (TUint)len;
            Fence();
            rx->head = (ri + 1u) % NB_RING_SLOTS;
            ++received;
            }
        }
    return posted + received;                // frames moved this poll
    }

void NetBridgePollLoop(TInt aMaxMs)
    {
    if (netbridge_init() != KErrNone)
        { KPrintf("netbridge: init failed\r\n"); return; }
    KPrintf("netbridge: polling up to %d ms (done=0)\r\n", aMaxMs);
    TInt moved = 0;
    for (TInt t = 0; t < aMaxMs && !gNetBridge.done; t += 10)
        {
        moved += netbridge_poll();
        NKern::Sleep(10);
        }
    KPrintf("netbridge: poll end moved=%d done=%u tx=%u/%u rx=%u/%u\r\n",
        moved, gNetBridge.done,
        gNetBridge.tx.head, gNetBridge.tx.tail,
        gNetBridge.rx.head, gNetBridge.rx.tail);
    }

// Boot hook: mirror e1000's .init_array constructor; e1000_init() is idempotent
// so ordering between the two translation units is irrelevant.
__attribute__((constructor)) static void NetBridgeBootInit()
    { netbridge_init(); }
