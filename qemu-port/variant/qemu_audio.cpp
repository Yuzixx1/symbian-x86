// qemu_audio.cpp — AC'97 (Intel 82801AA, PCI class 04.01) PCM-OUT driver for
// the full Kern image on QEMU q35. Variant-owned; no genuine code involved.
//
// UNIFIED DESIGN (reconciles two overlapping PCM implementations):
//   * ONE DMA engine: gPcmDma (192 KiB 16-bit stereo conversion buffer) plus
//     gPcmBdl[32] (multi-descriptor BDL). PcmDmaStart() is the ONLY place that
//     writes PO_BDBAR / PO_LVI / PO_CR for playback. The old second BDL
//     (gBdl[2]) and the pool's private BDL (gAudioPoolBdl[1]) are GONE.
//   * ONE queue: 8 x 16 KiB low-RAM slots (gPcmSlots). They hold copies of
//     caller PCM so a user buffer may be reused right after PLAY returns
//     (DOOM's SFX ring relies on this). One slot is in flight; the rest wait.
//     Oversized one-shot buffers (e.g. the 176400-byte AUDIOPCM test) skip
//     the copy and are converted straight into gPcmDma when the engine is idle.
//   * ONE completion poller: PcmPoll() reclaims the finished slot and kicks
//     the next queued one. audio_pcm_status/stats/busy all drive it.
//   * ONE canonical play API: audio_pcm_play(buf,bytes,rate,format) — the
//     ambiguous 2-arg overload was removed.
//
// Legacy tone (audio_play_test / audio_tone) now uses the SAME BDL engine via
// PcmArm(); it just blocks until completion, so MORSE/MUSIC/SOUNDPLR keep
// working with their original markers and unchanged exec-240 behaviour.
//
// QEMU detail: AC97 BAR0/BAR1 are I/O-space BARs (hw/audio/ac97.c); the helpers
// below select in/out vs. MMIO from the BAR type bit. DMA buffers are in
// identity-mapped low RAM, so virtual == 32-bit physical address.

#include <x86.h>
#include <e32err.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

namespace
    {
    // ------------------------------------------------------------ port I/O
    static inline void Out8(unsigned short aPort, unsigned char aVal)
        { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
    static inline unsigned char In8(unsigned short aPort)
        { unsigned char v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }
    static inline void Out16(unsigned short aPort, unsigned short aVal)
        { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
    static inline unsigned short In16(unsigned short aPort)
        { unsigned short v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }
    static inline void Out32(unsigned short aPort, unsigned int aVal)
        { __asm__ __volatile__("out %1, %0" : : "a"(aVal), "Nd"(aPort)); }
    static inline unsigned int In32(unsigned short aPort)
        { unsigned int v; __asm__ __volatile__("in %0, %1" : "=a"(v) : "Nd"(aPort)); return v; }

    enum
        {
        // PCI configuration space.
        kPciCommand   = 0x04,
        kPciBar0      = 0x10, kPciBar1 = 0x14,
        kPciIoSpace   = 0x01u, kPciMemSpace = 0x02u, kPciBusMaster = 0x10u,

        // Native Audio Mixer (NAM, BAR0) codec registers — 16-bit accesses.
        kNamReset          = 0x00,
        kNamMasterVolume   = 0x02,
        kNamPcmOutVolume   = 0x18,
        kNamExtAudioCtrl   = 0x2A,
        kNamFrontDacRate   = 0x2C,
        kVendorId1         = 0x7C, kVendorId2 = 0x7E,
        kCodecReady        = 0x8000u,

        // Native Audio Bus Master (NAB, BAR1). PCM OUT = index 1 -> +0x10.
        kPoBdbar = 0x10,  // dword: buffer-descriptor list base (phys)
        kPoCiv   = 0x14,  // byte : current index value (RO)
        kPoLvi   = 0x15,  // byte : last valid index
        kPoSr    = 0x16,  // byte/word: status
        kPoPicb  = 0x18,  // word : position in current buffer (RO, in words)
        kPoCr    = 0x1B,  // byte : control
        kGlobCnt = 0x2C,  // dword: global control
        kGlobSta = 0x30,  // dword: global status

        kCrRpbm  = 0x01u, kCrRr = 0x02u,      // run, reset
        kSrDch   = 0x01u, kSrWclear = 0x1Cu,  // DMA halted; write-1-clear bits
        kBdIoc   = 0x80000000u,               // interrupt on completion
        kGcWr    = 0x04u, kGcCr = 0x02u,      // warm / cold reset requests

        // 16-bit stereo @ 48 kHz, 1 kHz tone => 48 frames per period.
        kRateHz        = 48000,
        kToneHz        = 1000,
        kPeriodFrames  = kRateHz / kToneHz,      // 48
        kFrames        = kRateHz / 10,           // 0.1 s -> 4800 frames
        kWordsPerFrame = 2,                      // stereo, signed 16-bit
        kTotalWords    = kFrames * kWordsPerFrame,
        kMaxToneFrames = kRateHz / 2,            // 24000 frames (500 ms)
        kToneWords     = kMaxToneFrames * kWordsPerFrame,
        kSpinCodec     = 100000,
        kSpinPlay      = 400000000u,

        // --- unified PCM engine -------------------------------------------
        kPcmMaxBytes   = 192000,                    // 1 s @ 48 kHz stereo 16-bit
        kPcmDmaWords   = kPcmMaxBytes / 2,           // 96000 output words
        kPcmMaxBd      = 32,                        // LVI is 5 bits (0..31)
        kPcmSpinStatus = 120000000u,                // selftest completion cap

        // --- 8-slot queue feeding the engine ------------------------------
        kPcmSlots      = 8,
        kPcmSlotBytes  = 16 * 1024,
        kPcmNoSlot     = 0xFFu,
        kPcmFree       = 0,
        kPcmQueued     = 1,
        kPcmActive     = 2
        };

    struct BdEntry
        {
        volatile TUint32 iAddr;
        volatile TUint32 iCtlLen;
        };

    static const TInt16 kSine[kPeriodFrames] =
        {
             0,  1175,  2329,  3444,  4500,  5479,  6364,  7140,
          7794,  8315,  8693,  8923,  9000,  8923,  8693,  8315,
          7794,  7140,  6364,  5479,  4500,  3444,  2329,  1175,
             0, -1175, -2329, -3444, -4500, -5479, -6364, -7140,
          -7794, -8315, -8693, -8923, -9000, -8923, -8693, -8315,
          -7794, -7140, -6364, -5479, -4500, -3444, -2329, -1175
        };

    // Legacy tone source buffers (NOT BDLs — the DMA engine is shared below).
    __attribute__((aligned(4))) static TUint16 gPcm[kTotalWords];
    __attribute__((aligned(4))) static TUint16 gTonePcm[kToneWords];

    // The one and only DMA engine: conversion buffer + multi-descriptor BDL.
    __attribute__((aligned(4))) static TUint16 gPcmDma[kPcmDmaWords];
    __attribute__((aligned(256))) static BdEntry gPcmBdl[kPcmMaxBd];
    static TUint16 gPcmBdWords[kPcmMaxBd];
    static TUint gPcmBdCount = 0;
    static TUint gPcmTotalWords = 0;
    static volatile TBool gPcmBusy = EFalse;
    static TUint gPcmMarker = 0;
    __attribute__((aligned(4))) static TUint16 gPcmTest[44100 * 2]; // selftest src

    // The one and only queue: caller data is copied into a slot so the user
    // buffer can be reused immediately after a successful PLAY.
    struct PcmSlot
        {
        TUint8 iSrc[kPcmSlotBytes];   // raw source copy (<= slot size)
        TUint  iBytes;
        TUint  iRate;
        TUint  iFormat;               // (bits<<16) | channels
        TUint8 iState;
        };
    __attribute__((aligned(16))) static PcmSlot gPcmSlots[kPcmSlots];
    static TUint  gPcmSlotNext = 0;
    static TUint  gActiveSlot = kPcmNoSlot;
    static TBool  gPoolInited = EFalse;
    static TUint  gPcmPlayed = 0, gPcmDropped = 0, gPcmCompleted = 0;

    static TUint32 gNam = 0, gNabm = 0;
    static TBool gNamIo = ETrue, gNabmIo = ETrue;
    static TBool gInit = EFalse;
    static TUint16 gVendor1 = 0, gVendor2 = 0, gCodecStat = 0;
    static TUint gBus = 0, gDev = 0, gFn = 0;

    static inline void Barrier()
        { __asm__ __volatile__("" : : : "memory"); }

    static inline void NamWr16(TUint32 aReg, TUint16 aVal)
        {
        if (gNamIo) Out16((unsigned short)(gNam + aReg), aVal);
        else *(volatile TUint16*)(gNam + aReg) = aVal;
        }
    static inline TUint16 NamRd16(TUint32 aReg)
        {
        return gNamIo ? In16((unsigned short)(gNam + aReg))
                      : *(volatile TUint16*)(gNam + aReg);
        }

    static inline void NabWr8(TUint32 aReg, TUint8 aVal)
        {
        if (gNabmIo) Out8((unsigned short)(gNabm + aReg), aVal);
        else *(volatile TUint8*)(gNabm + aReg) = aVal;
        }
    static inline TUint8 NabRd8(TUint32 aReg)
        {
        return gNabmIo ? In8((unsigned short)(gNabm + aReg))
                       : *(volatile TUint8*)(gNabm + aReg);
        }
    static inline void NabWr16(TUint32 aReg, TUint16 aVal)
        {
        if (gNabmIo) Out16((unsigned short)(gNabm + aReg), aVal);
        else *(volatile TUint16*)(gNabm + aReg) = aVal;
        }
    static inline TUint16 NabRd16(TUint32 aReg)
        {
        return gNabmIo ? In16((unsigned short)(gNabm + aReg))
                       : *(volatile TUint16*)(gNabm + aReg);
        }
    static inline void NabWr32(TUint32 aReg, TUint32 aVal)
        {
        if (gNabmIo) Out32((unsigned short)(gNabm + aReg), aVal);
        else *(volatile TUint32*)(gNabm + aReg) = aVal;
        }
    static inline TUint32 NabRd32(TUint32 aReg)
        {
        return gNabmIo ? In32((unsigned short)(gNabm + aReg))
                       : *(volatile TUint32*)(gNabm + aReg);
        }

    static TBool LowRamOk(const void* aPtr, TUint aBytes)
        {
        const TUint32 p = (TUint32)aPtr;
        return aBytes != 0 && p >= 0x1000u && p < 0x80000000u &&
               aBytes <= 0x80000000u - p;
        }

    // ------------------------------------------------ the DMA engine (one)
    // Convert any supported source format into 16-bit stereo words in gPcmDma.
    static TInt PcmConvert(const void* aSrc, TUint aBytes, TUint aBits, TUint aCh,
                           TUint& aWordsOut)
        {
        if (aBits != 8 && aBits != 16)
            return KErrArgument;
        if (aCh != 1 && aCh != 2)
            return KErrArgument;
        const TUint bps = aBits >> 3;
        const TUint frameBytes = bps * aCh;
        if (aBytes < frameBytes || (aBytes % frameBytes) != 0)
            return KErrArgument;
        const TUint frames = aBytes / frameBytes;
        const TUint words = frames * 2u;              // stereo out
        if (words == 0 || words > kPcmDmaWords)
            return KErrArgument;

        const TUint8* s = (const TUint8*)aSrc;
        TUint si = 0;
        for (TUint i = 0; i < frames; ++i)
            {
            TInt l = 0, r = 0;
            if (aBits == 8)                           // unsigned -> signed 16
                {
                l = ((TInt)s[si++] - 128) << 8;
                r = (aCh == 2) ? ((TInt)s[si++] - 128) << 8 : l;
                }
            else                                      // little-endian signed 16
                {
                const TUint u = (TUint)s[si++] | ((TUint)s[si++] << 8);
                l = (u & 0x8000u) ? ((TInt)u - 0x10000) : (TInt)u;
                if (aCh == 2)
                    {
                    const TUint v = (TUint)s[si++] | ((TUint)s[si++] << 8);
                    r = (v & 0x8000u) ? ((TInt)v - 0x10000) : (TInt)v;
                    }
                else
                    r = l;                             // mono -> both channels
                }
            gPcmDma[2 * i]     = (TUint16)l;
            gPcmDma[2 * i + 1] = (TUint16)r;
            }
        aWordsOut = words;
        return KErrNone;
        }

    // Split the converted words into frame-aligned, 64 KiB-safe descriptors.
    static void PcmBuildBdl(TUint aWords, TUint aFrameWords)
        {
        const TUint maxWords = (0xFFFFu / aFrameWords) * aFrameWords;
        TUint off = 0, n = 0;
        while (off < aWords && n < kPcmMaxBd)
            {
            const TUint32 addr = (TUint32)(gPcmDma + off);
            TUint chunk = aWords - off;
            if (chunk > maxWords)
                chunk = maxWords;
            const TUint toEdge = (0x10000u - (addr & 0xFFFFu)) / 2u;
            if (chunk > toEdge)
                chunk = toEdge;
            chunk = (chunk / aFrameWords) * aFrameWords;  // frame-aligned
            if (chunk == 0)
                break;
            gPcmBdl[n].iAddr   = addr;
            gPcmBdl[n].iCtlLen = chunk | kBdIoc;
            gPcmBdWords[n]     = chunk;
            off += chunk;
            ++n;
            }
        gPcmBdCount = n;
        }

    // The ONLY routine that arms PO_BDBAR / PO_LVI / PO_CR for playback.
    static void PcmDmaStart(TUint aRate, TUint aCount)
        {
        NamWr16(kNamFrontDacRate, (TUint16)aRate);
        NabWr8(kPoCr, kCrRr);
        NabWr32(kPoBdbar, (TUint32)gPcmBdl);
        NabWr8(kPoLvi, (TUint8)(aCount - 1u));
        NabWr16(kPoSr, kSrWclear);
        Barrier();
        NabWr8(kPoCr, kCrRpbm);
        }

    static TBool PcmDmaDone()
        {
        return (NabRd8(kPoSr) & kSrDch) != 0 && NabRd16(kPoPicb) == 0;
        }

    // Bytes consumed by the current in-flight BDL (for STATUS).
    static TUint PcmDmaPosition()
        {
        if (!gPcmBusy)
            return gPcmTotalWords * 2u;
        const TUint civ = NabRd8(kPoCiv);
        TUint left = NabRd16(kPoPicb);    // words left in current descriptor
        TUint doneWords = 0;
        if (civ < gPcmBdCount)
            {
            for (TUint i = 0; i < civ; ++i)
                doneWords += gPcmBdWords[i];
            if (left > gPcmBdWords[civ])
                left = gPcmBdWords[civ];
            doneWords += gPcmBdWords[civ] - left;
            }
        else
            doneWords = gPcmTotalWords;
        TUint pos = doneWords * 2u;
        const TUint total = gPcmTotalWords * 2u;
        return pos > total ? total : pos;
        }

    // ------------------------------------------------ the queue (one)
    static TUint PcmBusyCount()
        {
        TUint n = 0;
        for (TUint i = 0; i < kPcmSlots; ++i)
            if (gPcmSlots[i].iState != kPcmFree)
                ++n;
        if (gPcmBusy && gActiveSlot == kPcmNoSlot)
            ++n;                          // oversized one-shot in flight
        return n;
        }

    static TInt PcmAcquireSlot()
        {
        for (TUint n = 0; n < kPcmSlots; ++n)
            {
            const TUint i = (gPcmSlotNext + n) % kPcmSlots;
            if (gPcmSlots[i].iState == kPcmFree)
                {
                gPcmSlotNext = (i + 1) % kPcmSlots;
                return (TInt)i;
                }
            }
        return -1;
        }

    // Low-level: convert source -> gPcmDma, build BDL, arm engine, return
    // immediately (async). aSlot == kPcmNoSlot for oversized/legacy tone.
    static TInt PcmArm(const void* aSrc, TUint aBytes, TUint aBits, TUint aCh,
                       TUint aRate, TUint aSlot)
        {
        TUint words = 0;
        const TInt r = PcmConvert(aSrc, aBytes, aBits, aCh, words);
        if (r != KErrNone)
            return r;
        PcmBuildBdl(words, 2u);            // 16-bit stereo => 2 words/frame
        if (gPcmBdCount == 0)
            return KErrArgument;
        gPcmTotalWords = words;
        gActiveSlot = (TUint8)aSlot;
        Barrier();
        gPcmBusy = ETrue;
        PcmDmaStart(aRate, gPcmBdCount);

        if ((gPcmMarker++ % 16u) == 0u)
            KPrintf("AU: pcm play %u bytes %uHz (%u-bit %s, %u frames, %u bds)\r\n",
                    aBytes, aRate, aBits, aCh == 2 ? "stereo" : "mono",
                    words / 2u, gPcmBdCount);
        return KErrNone;
        }

    // Arm the first queued slot when the engine is idle. Never blocks.
    static void PcmTryStartQueued()
        {
        if (gPcmBusy)
            return;
        for (TUint i = 0; i < kPcmSlots; ++i)
            {
            if (gPcmSlots[i].iState != kPcmQueued)
                continue;
            PcmSlot& s = gPcmSlots[i];
            const TInt r = PcmArm(s.iSrc, s.iBytes, s.iFormat >> 16,
                                  s.iFormat & 0xFFFFu, s.iRate, i);
            if (r != KErrNone)
                {
                s.iState = kPcmFree;
                ++gPcmDropped;
                continue;
                }
            s.iState = kPcmActive;
            ++gPcmPlayed;
            return;
            }
        }

    // The single completion poller. Returns 1 if a buffer just completed.
    static TBool PcmPoll()
        {
        if (!gPcmBusy)
            {
            PcmTryStartQueued();
            return EFalse;
            }
        if (!PcmDmaDone())
            return EFalse;
        const TUint i = gActiveSlot;
        if (i != kPcmNoSlot)
            gPcmSlots[i].iState = kPcmFree;
        gActiveSlot = kPcmNoSlot;
        gPcmBusy = EFalse;
        ++gPcmCompleted;
        KPrintf("AU: pcm done slot=%u completed=%u busy=%u\r\n",
                i == kPcmNoSlot ? (TUint)0xFF : i, gPcmCompleted, PcmBusyCount());
        PcmTryStartQueued();
        return ETrue;
        }

    // Stop the engine and release every queued/in-flight slot.
    static void PcmFlushQueue()
        {
        NabWr8(kPoCr, kCrRr);
        NabWr16(kPoSr, kSrWclear);
        gPcmBusy = EFalse;
        gActiveSlot = kPcmNoSlot;
        for (TUint i = 0; i < kPcmSlots; ++i)
            gPcmSlots[i].iState = kPcmFree;
        }

    // Synchronous convenience for the legacy tone/play_test paths: they reuse
    // the one engine, then spin on the one completion predicate.
    static TInt TonePlay(const void* aSrc, TUint aBytes, TUint aRate)
        {
        PcmFlushQueue();
        TInt r = PcmArm(aSrc, aBytes, 16u, 2u, aRate, kPcmNoSlot);
        if (r != KErrNone)
            return r;
        TUint32 spin = 0;
        for (; spin < kSpinPlay; ++spin)
            {
            if (PcmDmaDone())
                break;
            __asm__ __volatile__("pause");
            }
        const TUint8 sr = NabRd8(kPoSr);
        const TUint16 picb = NabRd16(kPoPicb);
        gPcmBusy = EFalse;
        if (spin == kSpinPlay || !(sr & kSrDch) || picb != 0)
            {
            KPrintf("audio: tone timeout sr=%x picb=%u civ=%u\r\n",
                    sr, (TUint)picb, (TUint)NabRd8(kPoCiv));
            return KErrTimedOut;
            }
        return KErrNone;
        }
    }  // namespace

TInt audio_init()
    {
    if (gInit)
        return (gNam && gNabm) ? KErrNone : KErrNotFound;
    gInit = ETrue;

    unsigned bus = 0, dev = 0, fn = 0;
    if (!PciFindClass(0x04, 0x01, &bus, &dev, &fn))
        {
        KPrintf("audio: no AC97 controller (PCI 04.01)\r\n");
        return KErrNotFound;
        }
    gBus = bus; gDev = dev; gFn = fn;

    TUint32 bar0 = PciRead32(bus, dev, fn, kPciBar0);
    TUint32 bar1 = PciRead32(bus, dev, fn, kPciBar1);
    gNamIo  = (bar0 & 1u) != 0;
    gNabmIo = (bar1 & 1u) != 0;
    gNam  = bar0 & (gNamIo  ? ~3u : ~0xFu);
    gNabm = bar1 & (gNabmIo ? ~3u : ~0xFu);
    if (!gNam || !gNabm)
        {
        KPrintf("audio: AC97 BARs unassigned (BAR0=%x BAR1=%x)\r\n", bar0, bar1);
        return KErrGeneral;
        }
    PciWrite32(bus, dev, fn, kPciCommand,
               PciRead32(bus, dev, fn, kPciCommand) |
               (kPciIoSpace | kPciMemSpace | kPciBusMaster));

    NabWr8(kPoCr, kCrRr);
    NabWr32(kGlobCnt, kGcCr);

    NamWr16(kNamReset, 0);
    for (TInt i = 0; i < kSpinCodec; ++i)
        {
        gCodecStat = NamRd16(kNamReset);
        gVendor1 = NamRd16(kVendorId1);
        gVendor2 = NamRd16(kVendorId2);
        if ((gCodecStat & kCodecReady) ||
            (gVendor1 && gVendor1 != 0xFFFFu))
            break;
        }
    NamWr16(kNamFrontDacRate, (TUint16)kRateHz);
    NamWr16(kNamMasterVolume, 0);
    NamWr16(kNamPcmOutVolume, 0);

    KPrintf("audio: AC97 %02x:%02x.%x BAR0=%x(%s) BAR1=%x(%s)\r\n",
        bus, dev, fn, gNam, gNamIo ? "IO" : "MEM",
        gNabm, gNabmIo ? "IO" : "MEM");
    KPrintf("audio: vendor ID %04x:%04x codec ready\r\n", gVendor1, gVendor2);
    return KErrNone;
    }

void audio_info()
    {
    if (audio_init() != KErrNone)
        return;
    KPrintf("audio: AC97 %02x:%02x.%x BAR0=%x(%s) BAR1=%x(%s)\r\n",
        gBus, gDev, gFn, gNam, gNamIo ? "IO" : "MEM",
        gNabm, gNabmIo ? "IO" : "MEM");
    KPrintf("audio: vendor ID %04x:%04x codec ready (reset=%x)\r\n",
        gVendor1, gVendor2, gCodecStat);
    KPrintf("audio: GLOB_STA=%x PO_CR=%x PO_SR=%x PO_CIV=%u PO_PICB=%u\r\n",
        NabRd32(kGlobSta), NabRd8(kPoCr), NabRd8(kPoSr),
        (TUint)NabRd8(kPoCiv), (TUint)NabRd16(kPoPicb));
    }

TInt audio_play_test()
    {
    if (audio_init() != KErrNone)
        return KErrNotReady;

    for (TUint i = 0; i < kFrames; ++i)
        {
        TUint16 s = (TUint16)kSine[i % kPeriodFrames];
        gPcm[2 * i] = s;
        gPcm[2 * i + 1] = s;
        }

    KPrintf("audio play: 1kHz stereo 48kHz %u words (%u bytes)\r\n",
        (TUint)kTotalWords, (TUint)(kTotalWords * 2));

    TInt r = TonePlay(gPcm, kTotalWords * 2u, kRateHz);
    if (r != KErrNone)
        return r;
    KPrintf("audio play: PCM play done (%u bytes)\r\n",
        (TUint)(kTotalWords * 2));
    return KErrNone;
    }

// Play one arbitrary-frequency note (legacy soundplr/music path). Phase
// accumulate through the 48-entry 1 kHz table with 16.16 interpolation.
TInt audio_tone(TInt aFreq, TInt aMs)
    {
    if (audio_init() != KErrNone)
        return KErrNotReady;
    if (aFreq < 20 || aFreq > 20000 || aMs < 1 || aMs > 500)
        return KErrArgument;

    TUint frames = (TUint)((kRateHz * aMs) / 1000);
    if (frames > kMaxToneFrames)
        frames = kMaxToneFrames;
    const TUint step = (TUint)((aFreq << 16) / 1000);
    const TUint wrap = (TUint)kPeriodFrames << 16;
    TUint phase = 0;
    for (TUint i = 0; i < frames; ++i)
        {
        const TUint idx = phase >> 16;
        const TUint frac = phase & 0xFFFFu;
        const TInt s0 = kSine[idx];
        const TInt s1 = kSine[(idx + 1) % kPeriodFrames];
        const TUint16 s = (TUint16)(s0 + ((s1 - s0) * (TInt)frac >> 16));
        gTonePcm[2 * i] = s;
        gTonePcm[2 * i + 1] = s;
        phase += step;
        if (phase >= wrap)
            phase -= wrap;
        }
    const TUint words = frames * kWordsPerFrame;

    KPrintf("audio tone: %dHz %dms %u frames (%u bytes)\r\n",
        aFreq, aMs, frames, (TUint)(words * 2));

    TInt r = TonePlay(gTonePcm, words * 2u, kRateHz);
    if (r != KErrNone)
        return r;
    KPrintf("audio tone: done %dHz %dms\r\n", aFreq, aMs);
    return KErrNone;
    }

// ---------------------------------------------------------------------------
// Canonical PCM API (the only audio_pcm_play overload). Copies/converts the
// source, arms the one engine and returns IMMEDIATELY (async). Small buffers
// (<= 16 KiB — all DOOM SFX/music) are queued in an 8-slot pool; oversized
// one-shots (AUDIOPCM's 176400-byte test) go straight to the engine when idle.
// ---------------------------------------------------------------------------
TInt audio_pcm_play(const void* aBuf, TUint aBytes, TUint aRate, TUint aChannels)
    {
    if (audio_pool_init() != KErrNone)
        return KErrNotReady;
    TUint bits = aChannels >> 16;
    const TUint ch = aChannels & 0xFFFFu;
    if (bits == 0)
        bits = 16;
    if (aRate != 11025u && aRate != 22050u && aRate != 44100u && aRate != 48000u)
        return KErrArgument;
    if (!LowRamOk(aBuf, aBytes))
        return KErrArgument;

    PcmPoll();                           // reclaim completed, kick next

    if (aBytes <= kPcmSlotBytes)
        {
        const TInt i = PcmAcquireSlot();
        if (i < 0)
            {
            ++gPcmDropped;
            KPrintf("AU: pcm drop played=%u dropped=%u\r\n",
                    gPcmPlayed, gPcmDropped);
            return KErrInUse;            // drop-on-saturation
            }
        PcmSlot& s = gPcmSlots[i];
        for (TUint b = 0; b < aBytes; ++b)
            s.iSrc[b] = ((const TUint8*)aBuf)[b];
        s.iBytes = aBytes;
        s.iRate = aRate;
        s.iFormat = aChannels;
        s.iState = kPcmQueued;
        PcmTryStartQueued();             // start now if the engine is idle
        return KErrNone;
        }

    // Oversized one-shot: only when the engine is idle and nothing is queued.
    if (gPcmBusy || PcmBusyCount() != 0)
        {
        ++gPcmDropped;
        return KErrInUse;
        }
    const TInt r = PcmArm(aBuf, aBytes, bits, ch, aRate, kPcmNoSlot);
    if (r == KErrNone)
        ++gPcmPlayed;
    return r;
    }

TInt audio_pcm_status(au_pcm_stat* aStat)
    {
    if (audio_init() != KErrNone)
        return KErrNotReady;
    if (!aStat || !LowRamOk(aStat, sizeof(*aStat)))
        return KErrArgument;

    PcmPoll();                           // single completion poller
    aStat->iState    = gPcmBusy ? 1u : 0u;
    aStat->iPosition = PcmDmaPosition();
    aStat->iTotal    = gPcmTotalWords * 2u;
    return KErrNone;
    }

TInt audio_pcm_stop()
    {
    if (audio_init() != KErrNone)
        return KErrNotReady;
    PcmFlushQueue();
    return KErrNone;
    }

TInt audio_pcm_selftest()
    {
    if (audio_init() != KErrNone)
        return KErrNotReady;
    const TUint rate = 44100u, frames = 44100u;      // 1 s of audio
    const TUint wrap = (TUint)kPeriodFrames << 16;
    const TUint step = (TUint)(((TUint)kPeriodFrames * 440u * 65536u) / rate);
    TUint phase = 0;
    for (TUint i = 0; i < frames; ++i)
        {
        const TUint idx   = phase >> 16;
        const TInt  frac  = (TInt)(phase & 0xFFFFu);
        const TInt  s0    = kSine[idx];
        const TInt  s1    = kSine[(idx + 1u) % kPeriodFrames];
        const TUint16 v   = (TUint16)(s0 + ((s1 - s0) * frac >> 16));
        gPcmTest[2 * i]     = v;
        gPcmTest[2 * i + 1] = v;
        phase += step;
        if (phase >= wrap)
            phase -= wrap;
        }
    TInt r = audio_pcm_play(gPcmTest, frames * 4u, rate, 2u | (16u << 16));
    if (r != KErrNone)
        return r;
    au_pcm_stat st;
    TUint32 spin = 0;
    for (; spin < kPcmSpinStatus; ++spin)
        {
        if (audio_pcm_status(&st) != KErrNone)
            return KErrGeneral;
        if (!st.iState)
            break;
        __asm__ __volatile__("pause");
        }
    KPrintf("AU: pcm selftest %s (%u bytes, spins=%u)\r\n",
            spin < kPcmSpinStatus ? "done" : "TIMEOUT", frames * 4u, spin);
    return spin < kPcmSpinStatus ? KErrNone : KErrTimedOut;
    }

// ---------------------------------------------------------------------------
// Pool/queue diagnostics + exec-240 STATS transport.
// ---------------------------------------------------------------------------
TInt audio_pool_init()
    {
    if (gPoolInited)
        return KErrNone;
    for (TUint i = 0; i < kPcmSlots; ++i)
        {
        gPcmSlots[i].iState = kPcmFree;
        gPcmSlots[i].iBytes = 0;
        gPcmSlots[i].iRate = 0;
        gPcmSlots[i].iFormat = 0;
        }
    gPcmSlotNext = 0;
    gActiveSlot = kPcmNoSlot;
    gPcmPlayed = gPcmDropped = gPcmCompleted = 0;
    gPoolInited = ETrue;
    TInt r = audio_init();
    if (r != KErrNone)
        return r;
    KPrintf("AU: pool init %u x %u bytes\r\n",
        (TUint)kPcmSlots, (TUint)kPcmSlotBytes);
    return KErrNone;
    }

TInt audio_pool_poll()
    {
    if (!gPoolInited)
        audio_pool_init();
    return PcmPoll() ? 1 : 0;
    }

void audio_pcm_stats(TUint& aPlayed, TUint& aDropped, TUint& aBusy,
                     TUint& aCompleted)
    {
    PcmPoll();
    aPlayed = gPcmPlayed;
    aDropped = gPcmDropped;
    aBusy = PcmBusyCount();
    aCompleted = gPcmCompleted;
    }

TUint audio_pcm_busy()
    {
    PcmPoll();
    return PcmBusyCount();
    }

void audio_pool_info()
    {
    TUint played = 0, dropped = 0, busy = 0, completed = 0;
    audio_pcm_stats(played, dropped, busy, completed);
    KPrintf("AU: stats played=%u dropped=%u busy=%u/%u completed=%u\r\n",
        played, dropped, busy, (TUint)kPcmSlots, completed);
    }

// Boot hook: HwInit0 walks .init_array after paging + TheAsic are ready. One
// constructor initialises the controller and the queue (both idempotent).
__attribute__((constructor)) static void AudioBootInit()
    { audio_init(); audio_pool_init(); }
