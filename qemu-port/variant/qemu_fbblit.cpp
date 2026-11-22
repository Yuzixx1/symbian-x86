// qemu_fbblit.cpp -- kernel-context bulk framebuffer blits for ring-3 E32 code.
//
// The VBE LFB (0xFD000000) is mapped U/S=0 by mmu/pd.cpp, so a user program
// cannot write VRAM directly.  exec-251 display lists cost one int 0x21 per
// GC_RECT, far too slow for a 320x200 game at 35 fps.  This module copies a
// whole frame in one kernel exec instead.
//
// Slot safety (verified with nm + a word dump of build/kernfull.elf):
//   EpocSlowExecTable @ 0x00190fc0, 271 live words (0..270).  The only free
//   in-range slots (240, 247..251) are taken by audio/netbridge/GUI.  The
//   32-byte-aligned EpocFastExecTable @ 0x00191860 leaves 7 zero pad words
//   (0x00191844..0x0019185c); windowsrv consumes 6 for 271/272/273.  Exec N's
//   handler word is table index 4+2N, so slot 274 would be index 552 ==
//   0x00191860 == EpocFastExecTable.iFastExecCount.  Writing 274/275 would
//   clobber the fast-exec table.  We therefore expose blit/info as new ops
//   on the genuine slow exec 270 (SvcMgr ctl) implemented in e32load.cpp.
//   kernelhwsrv/ is untouched; no table entry is overwritten here.

#include <x86.h>
#include <e32err.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"
#include "qemu_fbblit.h"

extern const TUint32 EpocSlowExecTable[];

namespace
    {
    static TBool gInit = EFalse;

    // Identity-mapped low RAM ends at 0x08000000 (MM::UserDataSectionEnd).
    const TUint kUserLimit = 0x08000000u;

    static inline TBool UserRange(TUint aAddr, TUint aLen)
        {
        return aAddr != 0u && aLen != 0u &&
               aAddr < kUserLimit && aLen <= kUserLimit - aAddr;
        }

    // Fixed 8-bit -> RGBX palette for DOOM-style indexed frames: index 0 is
    // black, every other index is (red=255, green=index, blue=0).  Deterministic
    // so a pixel readback/screendump test can check exact colours.
    static inline TUint32 Expand8(TUint8 aIndex)
        {
        return aIndex ? (0x00FF0000u | ((TUint32)aIndex << 8)) : 0u;
        }

    static void UserCopy(void* aDst, const void* aSrc, TUint aLen)
        {
        TUint8* d = (TUint8*)aDst;
        const TUint8* s = (const TUint8*)aSrc;
        for (TUint i = 0; i < aLen; ++i)
            d[i] = s[i];
        }
    }

// Minimal pitch getter.  qemu_display.cpp keeps gPitch in an anonymous
// namespace (no accessor exists), but every assignment there is
// `gPitch = gWidth * (gBpp >> 3)` (mode init, mode-13 fallback, VBE switch),
// so deriving it from the public geometry accessor is exact for this port.
TUint display_pitch()
    {
    TUint w = 0, h = 0, b = 0;
    display_geometry(w, h, b);
    return w * (b >> 3);
    }

// ctl op 2: return framebuffer geometry to a user struct.
TInt fbblit_info_exec(TInt aArg)
    {
    if (!UserRange((TUint)aArg, sizeof(struct fbblit_info)))
        return KErrArgument;
    struct fbblit_info info;
    for (TUint i = 0; i < sizeof(info); ++i)
        ((TUint8*)&info)[i] = 0;
    TUint w = 0, h = 0, b = 0;
    display_geometry(w, h, b);
    info.base = (unsigned int)display_lfb();
    info.width = (unsigned int)w;
    info.height = (unsigned int)h;
    info.pitch = (unsigned int)display_pitch();
    info.bpp = (unsigned int)b;
    info.size = (unsigned int)display_fb_size();
    UserCopy((void*)(TUint)aArg, &info, sizeof(info));
    return KErrNone;
    }

// ctl op 3: copy a user frame into the LFB rectangle (x,y,w,h).  This runs in
// kernel context from exec 270, so the U/S=0 framebuffer is writable.
TInt fbblit_blit_exec(TInt aArg)
    {
    if (!UserRange((TUint)aArg, sizeof(struct fbblit_req)))
        return KErrArgument;
    struct fbblit_req r;
    UserCopy(&r, (const void*)(TUint)aArg, sizeof(r));

    TUint w = 0, h = 0, b = 0;
    display_geometry(w, h, b);
    const TUint base = (TUint)display_lfb();
    if (!base || !w || !h || !b)
        return KErrNotReady;

    if (r.x < 0 || r.y < 0 || r.w <= 0 || r.h <= 0)
        return KErrArgument;
    if ((TUint)r.x >= w || (TUint)r.y >= h ||
        (TUint)w - (TUint)r.x < (TUint)r.w ||
        (TUint)h - (TUint)r.y < (TUint)r.h)
        return KErrArgument;

    const TUint dstBytes = b >> 3;
    const TUint srcBytes = r.src_bpp >> 3;
    if (srcBytes != 1u && srcBytes != dstBytes)
        return KErrArgument;

    const TUint rowBytes = (TUint)r.w * srcBytes;
    const TUint needed = rowBytes * (TUint)r.h;
    // Cap the transfer at one frame's worth of destination bytes: a bogus
    // len can never make us read past the rectangle or write past the LFB.
    const TUint cap = (TUint)r.w * (TUint)r.h * dstBytes;
    TUint len = r.len < needed ? r.len : needed;
    if (len > cap)
        len = cap;
    if (!UserRange(r.buf, len))
        return KErrArgument;

    const TUint pitch = display_pitch();
    volatile TUint8* dst = (TUint8*)(base + (TUint)r.y * pitch +
                                     (TUint)r.x * dstBytes);
    const TUint8* src = (const TUint8*)(TUint)r.buf;
    TUint remain = len;
    for (TUint row = 0; row < (TUint)r.h && remain != 0u; ++row)
        {
        TUint n = rowBytes < remain ? rowBytes : remain;
        if (srcBytes == dstBytes)
            {
            // Bulk copy the scanline.  The old byte-by-byte volatile loop
            // emitted one 8-bit load/store per byte (1,024,000 iterations for
            // DOOM's 640x400x32 frame).  The kernel's machine-coded memcpy is
            // an opaque external call built on `rep movsl`, so it copies one
            // dword per iteration (256,000) and the LFB stores cannot be
            // dead-store-eliminated by the compiler.  Row-by-row preserves the
            // exact destination addresses when pitch != width*dstBytes (e.g.
            // a 640x400 blit into a 1024x768, pitch=4096 LFB).
            memcpy((void*)dst, (const void*)src, n);
            }
        else
            {
            // 8bpp indexed source -> 32bpp RGBX destination.  Already dword
            // granular: one TUint32 store per source byte.  Keep as-is.
            TUint32* out = (TUint32*)dst;
            for (TUint i = 0; i < n; ++i)
                out[i] = Expand8(src[i]);
            }
        src += rowBytes;
        dst += pitch;
        if (remain > n)
            remain -= n;
        else
            remain = 0;
        }
    return KErrNone;
    }

TInt FbBlitInit()
    {
    if (gInit)
        return KErrNone;
    gInit = ETrue;

    const TUint32* tab = EpocSlowExecTable;
    // Safety proof in code: show exactly why 274/275 cannot be table slots.
    if (((TUint)&tab[552]) == (TUint)EpocFastExecTable)
        KPrintf("FBBLIT: slots 274/275 overlap EpocFastExecTable; routed\r\n");

    TUint w = 0, h = 0, b = 0;
    display_geometry(w, h, b);
    KPrintf("FBBLIT: ready execs=274/275\r\n");
    KPrintf("FBBLIT: exec 270 ops %u/%u LFB=0x%08x %ux%ux%u pitch=%u\r\n",
        (TUint)FBO_FBINFO, (TUint)FBO_FBBLIT, (TUint)display_lfb(),
        w, h, b, display_pitch());
    return KErrNone;
    }

__attribute__((constructor)) static void FbBlitBootInit()
    { FbBlitInit(); }
