#ifndef QPORT_VARIANT_QEMU_FBBLIT_H
#define QPORT_VARIANT_QEMU_FBBLIT_H

/* ---------------------------------------------------------------------------
 * Kernel exec for fast bulk framebuffer blits (variant/qemu_fbblit.cpp).
 *
 * WHY NOT NEW TABLE SLOTS 274/275
 *   EpocSlowExecTable (linked at 0x00190fc0) has 271 live entries (0..270).
 *   The only invalid in-range slots are 240, 247..251 and all six belong to
 *   other workers (audio, netbridge, GUI).  The 32-byte-aligned
 *   EpocFastExecTable starts at 0x00191860; the slow table's last live word
 *   is at 0x00191840, leaving exactly 7 zero words (0x00191844..0x0019185c).
 *   Windowsrv already took 6 of them for 271/272/273 and bumped the count
 *   to 274 (see qemu_windowsrv.h).  A slow exec N lives at table words
 *   [3+2N] (flags) and [4+2N] (handler); slot 274's handler word is index
 *   552 == 0x00191860 == EpocFastExecTable.iFastExecCount (verified with
 *   nm + a word dump of the linked ELF).  Writing 274/275 would destroy the
 *   fast-exec count and the first fast-exec handler, so it is NOT safe.
 *
 * ROUTING
 *   The blit/info capability is therefore installed as two new operations
 *   on slow exec 270 (the SvcMgr multiplexed control exec, implemented in
 *   e32load.cpp which this port owns).  It is still a genuine kernel exec:
 *   a real entry in EpocSlowExecTable, dispatched by int 0x21, running in
 *   ring 0 with direct LFB access.  kernelhwsrv/ is untouched.
 * ------------------------------------------------------------------------ */

/* Requested logical capability IDs (documentation / user-API naming). */
#define QE_EXEC_FB_BLIT          274u   /* logical: blit  -> slot 270 op 3 */
#define QE_EXEC_FB_INFO          275u   /* logical: info  -> slot 270 op 2 */

/* Physical routing (what user code actually calls). */
#define QE_EXEC_FBCTL            270u   /* SvcMgr ctl slow exec */
#define FBO_FBINFO               2u     /* ctl(aOp=2, aArg=struct fbblit_info*) */
#define FBO_FBBLIT               3u     /* ctl(aOp=3, aArg=struct fbblit_req*)  */

/* Plain-C ABI shared by the kernel handler and ring-3 E32 programs. */
struct fbblit_info
    {
    unsigned int base;              /* LFB physical/linear base            */
    unsigned int width;             /* pixels                              */
    unsigned int height;            /* pixels                              */
    unsigned int pitch;             /* bytes per scanline                  */
    unsigned int bpp;               /* bits per pixel                      */
    unsigned int size;              /* framebuffer byte size               */
    };

struct fbblit_req
    {
    unsigned int buf;               /* user buffer (identity-mapped RAM)   */
    unsigned int len;               /* bytes available in buf              */
    unsigned int src_bpp;           /* 8 or dst bpp (32)                   */
    int x, y;                       /* destination top-left                */
    int w, h;                       /* destination rectangle               */
    };

#ifdef __cplusplus
/* Kernel-side handlers (called by SvcMgrCtlExec in e32load.cpp). */
TInt fbblit_info_exec(TInt aArg);
TInt fbblit_blit_exec(TInt aArg);
TInt FbBlitInit();
#endif

#endif /* QPORT_VARIANT_QEMU_FBBLIT_H */
