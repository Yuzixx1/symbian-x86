#ifndef QPORT_VARIANT_QEMU_WINDOWSRV_H
#define QPORT_VARIANT_QEMU_WINDOWSRV_H

/* ---------------------------------------------------------------------------
 * Kernel-side foundation for a real window server (variant/qemu_windowsrv.cpp).
 *
 * Layout is deliberately expressed in plain C types so a future ring-3 E32
 * window server can map this by physical address.  Low RAM is identity-mapped
 * and U/S=1 in our PDs (mmu/pd.cpp), so returning the base is sufficient.
 *
 * WHY THESE EXEC SLOTS (271/272/273, appended):
 *   EpocSlowExecTable has 271 live entries (0..270).  Dumping the linked table
 *   shows ONLY 240 and 247..251 are InvalidExecHandler, and all six are taken
 *   (240 audio, 247/248 netbridge, 249/250/251 GUI).  241 is a repurposed
 *   BTrace stub (ps), 268..270 belong to SvcMgr, and 242..246 / 252..267 are
 *   REAL EUSER.DLL stubs (263 distinct int 0x21 numbers were scanned; 242/243
 *   are NotifyOnIdle / CancelMiscNotifier with live handlers).  There is no
 *   free in-range slot.  The linked ELF has 28 zero bytes between the end of
 *   the slow table (0x190d44) and the 32-byte-aligned EpocFastExecTable
 *   (0x190d60): slots 271..273 need exactly 24 bytes of that pad.  Bumping the
 *   count word (tab[0]) 271 -> 274 exposes them; the kernel dispatcher only
 *   uses that count as the upper bound (nkern/x86/vectors.cia:205).  No
 *   genuine exec, EUSER stub or other worker uses >=271.
 *
 * MEMORY COST: header 4 KiB (padded) + 16 * (320*200*4 = 256,000 B = 250 KiB)
 * = 4,096,000 B (~3.9 MiB) content, 4,100,096 bytes total at fixed physical
 * 0x00800000 .. 0x00BE8FFF.  This is a
 * static region, NOT .bss and NOT kernel heap: the kernel image .bss currently
 * ends near 0x3ebb10, the kernel data/heap clear covers 0x1000000..0x1200000
 * (kernfull_glue.cpp:120) and the user pool starts at 0x2000000.  0x800000
 * sits in the 12 MiB gap below the kernel data region, leaves ~4 MiB of BSS
 * headroom above the image, is identity-mapped with U/S=1, and is far from the
 * high-RAM ramdisk snapshot.  Total guest RAM is 128 MiB (QEMU -m 128).
 * ------------------------------------------------------------------------ */

#define QE_EXEC_WS_BASE         271u   /* return ws_shared physical base     */
#define QE_EXEC_WS_SIZE         272u   /* return sizeof(struct ws_shared)    */
#define QE_EXEC_WS_CTL          273u   /* multiplexed control (aOp, aArg)    */

#define WS_MAGIC                0x57533130u   /* "WS10" */
#define WS_VERSION              1u

#define WS_SHARED_BASE          0x00800000u
#define WS_MAX_WINDOWS          16u
#define WS_TITLE_LEN            24u
#define WS_EVENT_RING           64u

#define WS_CONTENT_W            320u
#define WS_CONTENT_H            200u
#define WS_CONTENT_BPP          4u
#define WS_CONTENT_BYTES        (WS_CONTENT_W * WS_CONTENT_H * WS_CONTENT_BPP) /* 256 KiB */
#define WS_CONTENT_BASE         4096u   /* byte offset of content[] from base */
#define WS_SHARED_SIZE          (WS_CONTENT_BASE + WS_MAX_WINDOWS * WS_CONTENT_BYTES)

/* Event ring opcodes. */
#define WSE_MOVE                1u
#define WSE_DOWN                2u
#define WSE_UP                  3u
#define WSE_KEY                 4u

/* Multiplexed exec 273 operations (aOp). */
#define WSO_REGISTER_SERVER     1u   /* aArg = server pid; once only         */
#define WSO_CREATE_WINDOW       2u   /* aArg = struct ws_create_params*      */
#define WSO_DESTROY_WINDOW      3u   /* aArg = window id                     */
#define WSO_SET_DIRTY           4u   /* aArg = window id; returns dirty      */
#define WSO_PUSH_EVENT          5u   /* aArg = struct ws_event*              */
#define WSO_POP_EVENT           6u   /* aArg = struct ws_event* (out)        */
#define WSO_SET_FOCUS           7u   /* aArg = window id                     */
#define WSO_FRAME_TICK          8u   /* aArg ignored; returns frame counter  */

/* aArg block for WSO_CREATE_WINDOW (in low user RAM, read by the kernel). */
struct ws_create_params
    {
    unsigned int pid;
    int x, y, w, h;
    int z;
    char title[WS_TITLE_LEN];
    };

struct ws_window
    {
    volatile unsigned int pid;      /* 0 = free slot */
    volatile int x, y, w, h;
    volatile char title[WS_TITLE_LEN];
    volatile unsigned int dirty;    /* client bumps, server clears */
    volatile unsigned int content_offset; /* byte offset from ws_shared base */
    volatile int visible;
    volatile int z;                 /* stacking order; higher is on top */
    };

struct ws_event
    {
    volatile unsigned int type;     /* WSE_* */
    volatile int x, y;              /* MOVE/DOWN/UP: pointer position */
    volatile unsigned int key;      /* KEY: ASCII byte; DOWN/UP: button bit  */
    volatile int target_wid;        /* -1 = broadcast / current focus */
    };

/* Fixed-size header: everything the server and clients poll lives here. */
struct ws_shared_header
    {
    volatile unsigned int magic;          /* WS_MAGIC once init ran        */
    volatile unsigned int version;
    volatile unsigned int server_pid;     /* 0 until WSO_REGISTER_SERVER   */
    volatile unsigned int frame;          /* compositor frame counter      */
    volatile int focus_wid;
    struct ws_window wins[WS_MAX_WINDOWS];
    struct ws_event events[WS_EVENT_RING];
    volatile unsigned int ev_head, ev_tail;
    volatile unsigned int events_dropped;
    volatile unsigned int last_buttons;   /* for DOWN/UP edge synthesis     */
    volatile unsigned int reserved[6];
    };
typedef char ws_header_fits_in_4k[
    (WS_CONTENT_BASE > sizeof(struct ws_shared_header)) ? 1 : -1];

struct ws_shared
    {
    struct ws_shared_header hdr;
    unsigned char _hdr_pad[WS_CONTENT_BASE - sizeof(struct ws_shared_header)];
    unsigned char content[WS_MAX_WINDOWS][WS_CONTENT_BYTES];
    };
typedef char ws_shared_size_matches[
    (sizeof(struct ws_shared) == WS_SHARED_SIZE) ? 1 : -1];

/* Byte offset of window 'wid' content buffer, relative to the ws_shared base. */
#define WS_CONTENT_OFFSET(wid) (WS_CONTENT_BASE + (unsigned int)(wid) * WS_CONTENT_BYTES)

#endif /* QPORT_VARIANT_QEMU_WINDOWSRV_H */
