// qemu_windowsrv.cpp — kernel-side shared memory + slow execs for a window server.
//
// A ring-3 window-server E32 process owns the VBE framebuffer (via execs
// 249/250/251 in qemu_gui_exec.cpp), composites client windows from the 16
// low-RAM content buffers here, and consumes input from the event ring below.
// kernelhwsrv/ is untouched: the appended exec slots 271/272/273 live in the
// verified zero padding between EpocSlowExecTable and EpocFastExecTable (see
// qemu_windowsrv.h for the full slot-safety argument).

#include <x86.h>
#include <e32err.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"
#include "qemu_windowsrv.h"

extern const TUint32 EpocSlowExecTable[];

namespace
    {
    // Static region at a fixed physical address (NOT .bss / NOT kernel heap).
    static struct ws_shared* const gWs = (struct ws_shared*)WS_SHARED_BASE;
    static TBool gInstalled = EFalse;

    static inline void Fence()
        { __asm__ __volatile__("" : : : "memory"); }

    // Short IRQ-safe critical section: mouse IRQ12 and keyboard IRQ1 can both
    // produce events, and the server can push/pop from user context.
    static inline unsigned WsCli()
        {
        unsigned f;
        __asm__ __volatile__("pushf\n\tpop %0\n\tcli" : "=r"(f));
        return f;
        }
    static inline void WsSti(unsigned f)
        { __asm__ __volatile__("push %0\n\tpopf" : : "r"(f) : "memory"); }

    // --- event ring (called with interrupts disabled) -----------------------
    static void WsPushEventLocked(TUint aType, TInt aX, TInt aY,
                                  TUint aKey, TInt aTarget)
        {
        struct ws_shared_header* h = &gWs->hdr;
        TUint next = (h->ev_tail + 1u) % WS_EVENT_RING;
        if (next == h->ev_head)
            { ++h->events_dropped; return; }  // full: never block an ISR
        struct ws_event* e = &h->events[h->ev_tail];
        e->type = aType;
        e->x = aX;
        e->y = aY;
        e->key = aKey;
        e->target_wid = aTarget;
        Fence();
        h->ev_tail = next;
        }

    static void WsZeroContent(TUint aWid)
        {
        if (aWid >= WS_MAX_WINDOWS)
            return;
        volatile unsigned int* p = (volatile unsigned int*)
            ((unsigned char*)gWs + WS_CONTENT_OFFSET(aWid));
        for (TUint i = 0; i < WS_CONTENT_BYTES / sizeof(unsigned int); ++i)
            p[i] = 0u;
        }

    // --- exec 271: shared base ----------------------------------------------
    static TInt WsBaseExec(TInt, TInt)
        { return (TInt)WS_SHARED_BASE; }

    // --- exec 272: shared byte size -----------------------------------------
    static TInt WsSizeExec(TInt, TInt)
        { return (TInt)sizeof(struct ws_shared); }

    static TBool WsUserPtr(TInt aArg)
        {
        // Direct model: low RAM is identity mapped and reachable from ring 0.
        // Refuse NULL and anything outside the low 128 MiB identity window.
        return aArg > 0 && (TUint)aArg < 0x08000000u;
        }

    // --- exec 273: multiplexed control --------------------------------------
    static TInt WsCtlExec(TInt aOp, TInt aArg)
        {
        struct ws_shared_header* h = &gWs->hdr;
        switch ((TUint)aOp)
            {
        case WSO_REGISTER_SERVER:
            if (aArg <= 0)
                return KErrArgument;
            if (h->server_pid && h->server_pid != (TUint)aArg)
                return KErrAccessDenied;   // one compositor at a time
            if (!h->server_pid)
                {
                h->ev_head = 0;
                h->ev_tail = 0;
                h->events_dropped = 0;
                h->last_buttons = 0;
                h->frame = 0;
                h->focus_wid = -1;
                Fence();
                h->server_pid = (TUint)aArg;
                KPrintf("WSRV: server pid=%u registered\r\n", (TUint)aArg);
                }
            return KErrNone;

        case WSO_CREATE_WINDOW:
            {
            if (!WsUserPtr(aArg))
                return KErrArgument;
            const struct ws_create_params* p =
                (const struct ws_create_params*)(TLinAddr)aArg;
            TUint w = (TUint)p->w, ht = (TUint)p->h;
            if (!w || !ht || w > WS_CONTENT_W || ht > WS_CONTENT_H)
                return KErrArgument;
            for (TUint i = 0; i < WS_MAX_WINDOWS; ++i)
                {
                struct ws_window* win = &h->wins[i];
                if (win->pid)
                    continue;
                WsZeroContent(i);
                win->pid = (unsigned int)p->pid;
                win->x = p->x;
                win->y = p->y;
                win->w = (int)w;
                win->h = (int)ht;
                win->z = p->z;
                for (TUint k = 0; k < WS_TITLE_LEN; ++k)
                    win->title[k] = p->title[k];
                win->title[WS_TITLE_LEN - 1] = 0;
                win->dirty = 0u;
                win->content_offset = WS_CONTENT_OFFSET(i);
                win->visible = 1;
                Fence();
                return (TInt)i;             // window id == slot index
                }
            return KErrOverflow;            // all 16 slots in use
            }

        case WSO_DESTROY_WINDOW:
            if (aArg < 0 || (TUint)aArg >= WS_MAX_WINDOWS)
                return KErrArgument;
            {
            struct ws_window* win = &h->wins[aArg];
            win->visible = 0;
            win->pid = 0;
            win->dirty = 0;
            WsZeroContent((TUint)aArg);
            if (h->focus_wid == aArg)
                h->focus_wid = -1;
            Fence();
            }
            return KErrNone;

        case WSO_SET_DIRTY:
            if (aArg < 0 || (TUint)aArg >= WS_MAX_WINDOWS)
                return KErrArgument;
            return (TInt)++h->wins[aArg].dirty;

        case WSO_PUSH_EVENT:
            {
            if (!WsUserPtr(aArg))
                return KErrArgument;
            const struct ws_event* e =
                (const struct ws_event*)(TLinAddr)aArg;
            TUint type = e->type;
            TInt x = e->x, y = e->y, target = e->target_wid;
            TUint key = e->key;
            if (!type || type > WSE_KEY)
                return KErrArgument;
            unsigned f = WsCli();
            WsPushEventLocked(type, x, y, key, target);
            WsSti(f);
            }
            return KErrNone;

        case WSO_POP_EVENT:
            {
            if (!WsUserPtr(aArg))
                return KErrArgument;
            struct ws_event* dst = (struct ws_event*)(TLinAddr)aArg;
            unsigned f = WsCli();
            if (h->ev_head == h->ev_tail)
                { WsSti(f); return KErrNotFound; }
            const struct ws_event* src = &h->events[h->ev_head];
            TUint type = src->type;
            TInt x = src->x, y = src->y, target = src->target_wid;
            TUint key = src->key;
            h->ev_head = (h->ev_head + 1u) % WS_EVENT_RING;
            WsSti(f);
            dst->type = type;
            dst->x = x;
            dst->y = y;
            dst->key = key;
            dst->target_wid = target;
            }
            return KErrNone;

        case WSO_SET_FOCUS:
            if (aArg >= 0 && (TUint)aArg < WS_MAX_WINDOWS)
                h->focus_wid = aArg;
            else if (aArg == -1)
                h->focus_wid = -1;
            else
                return KErrArgument;
            return KErrNone;

        case WSO_FRAME_TICK:
            return (TInt)++h->frame;

        default:
            return KErrArgument;
            }
        }

    static void WsExecInstall()
        {
        TUint32* tab = (TUint32*)EpocSlowExecTable;
        const TUint slots[3] = {QE_EXEC_WS_BASE, QE_EXEC_WS_SIZE,
                                QE_EXEC_WS_CTL};
        TInt (*fns[3])(TInt, TInt) = {&WsBaseExec, &WsSizeExec, &WsCtlExec};
        if (tab[0] == 274u)
            {
            // Already extended (idempotent): make sure our handlers survived.
            TBool ok = ETrue;
            for (TUint i = 0; i < 3; ++i)
                if (tab[4u + 2u * slots[i]] != (TUint32)(TLinAddr)fns[i])
                    ok = EFalse;
            if (!ok)
                KPrintf("WSRV: exec handlers clobbered, reinstall\r\n");
            else
                return;
            }
        else if (tab[0] != 271u)
            {
            KPrintf("WSRV: unexpected exec table count %u, install skipped\r\n",
                tab[0]);
            return;
            }
        // Slots 271..273 must still be the zero padding we verified in the
        // linked image.  If anything else claimed them, do not overwrite it.
        for (TUint i = 0; i < 3; ++i)
            {
            const TUint s = slots[i];
            if (tab[3u + 2u * s] || tab[4u + 2u * s])
                {
                KPrintf("WSRV: slot %u in use, install aborted\r\n", s);
                return;
                }
            }
        for (TUint i = 0; i < 3; ++i)
            {
            const TUint s = slots[i];
            tab[3u + 2u * s] = 0u;                         // flags
            tab[4u + 2u * s] = (TUint32)(TLinAddr)fns[i];  // handler
            }
        Fence();
        tab[0] = 274u;                                     // publish last
        }
    }  // namespace

TInt windowsrv_init()
    {
    if (gInstalled)
        return KErrNone;
    gInstalled = ETrue;

    volatile unsigned int* p = (volatile unsigned int*)gWs;
    for (TUint i = 0; i < sizeof(struct ws_shared) / sizeof(unsigned int); ++i)
        p[i] = 0u;                        // ~3.9 MiB zero: static, no heap
    Fence();
    gWs->hdr.magic = WS_MAGIC;
    gWs->hdr.version = WS_VERSION;
    gWs->hdr.focus_wid = -1;

    WsExecInstall();

    KPrintf("WSRV: shared base=0x%08x size=%u execs=%u/%u/%u tabcount=%u\r\n",
        (TUint)WS_SHARED_BASE, (TUint)sizeof(struct ws_shared),
        (TUint)QE_EXEC_WS_BASE, (TUint)QE_EXEC_WS_SIZE,
        (TUint)QE_EXEC_WS_CTL,
        ((const TUint32*)EpocSlowExecTable)[0]);
    return KErrNone;
    }

TUint32 windowsrv_base() { return (TUint32)WS_SHARED_BASE; }
TUint windowsrv_size() { return (TUint)sizeof(struct ws_shared); }

// --- input bridges, called from the PS/2 ISRs -------------------------------
// Both are no-ops until a window server registers (WSO_REGISTER_SERVER), so
// the existing shell/GUI input path is completely unaffected.

void wsrv_publish_mouse(TInt aX, TInt aY, TUint aButtons, TUint aSeq)
    {
    struct ws_shared_header* h = &gWs->hdr;
    if (!gInstalled || !h->server_pid)
        return;
    unsigned f = WsCli();
    TUint prev = h->last_buttons;
    if (aButtons != prev)
        {
        for (TUint bit = 1u; bit <= 4u; bit <<= 1)
            {
            if ((aButtons & bit) && !(prev & bit))
                WsPushEventLocked(WSE_DOWN, aX, aY, bit, h->focus_wid);
            else if (!(aButtons & bit) && (prev & bit))
                WsPushEventLocked(WSE_UP, aX, aY, bit, h->focus_wid);
            }
        h->last_buttons = aButtons;
        }
    WsPushEventLocked(WSE_MOVE, aX, aY, aSeq, h->focus_wid);
    WsSti(f);
    }

void wsrv_publish_key(TUint aKey)
    {
    struct ws_shared_header* h = &gWs->hdr;
    if (!gInstalled || !h->server_pid || !aKey)
        return;
    unsigned f = WsCli();
    WsPushEventLocked(WSE_KEY, -1, -1, aKey, h->focus_wid);
    WsSti(f);
    }

// Boot hook: HwInit0 walks .init_array after paging + ROM are ready.
__attribute__((constructor)) static void WsBootInit()
    { windowsrv_init(); }
