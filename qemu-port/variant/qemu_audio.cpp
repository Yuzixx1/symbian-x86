// qemu_audio.cpp — AC'97 (Intel 82801AA, PCI class 04.01) PCM-OUT driver for
// the full Kern image on QEMU q35. Variant-owned; no genuine code involved.
//
// Covers one self-contained function: play a ~0.1 s 1 kHz tone through the
// PCM-OUT DMA engine, purely polled (no PCI IRQ is bound — same discipline as
// qemu_ata / qemu_ahci / qemu_nic). DMA buffers live in identity-mapped low
// RAM, so their virtual addresses are also their 32-bit physical addresses.
//
// QEMU detail: the AC97 BAR0/BAR1 are I/O-space BARs (see hw/audio/ac97.c),
// not MMIO. The register helpers below select in/out vs. MMIO from the BAR
// type bit, so the same driver also works on a memory-mapped controller.
//
// References: Intel AC'97 bus-master register map (NAB offsets 00h..3Fh) and
// the 8-byte buffer-descriptor entry (addr + ctl_len; IOC bit 31, BUP bit 30).

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
        kCodecReady        = 0x8000u,  // reset-register bit 15 on real silicon

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
        // audio_tone() buffer: up to 500 ms. 48000 words fits in the BDL's
        // 16-bit length field; long notes are clamped rather than truncated
        // by the hardware descriptor mask.
        kMaxToneFrames = kRateHz / 2,            // 24000 frames (500 ms)
        kToneWords     = kMaxToneFrames * kWordsPerFrame,
        kSpinCodec     = 100000,
        kSpinPlay      = 400000000u,
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

    __attribute__((aligned(4))) static TUint16 gPcm[kTotalWords];
    __attribute__((aligned(4))) static TUint16 gTonePcm[kToneWords];
    __attribute__((aligned(256))) static BdEntry gBdl[2];

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
    // QEMU exposes both BARs as I/O space; enable I/O *and* MEM plus bus
    // master so a memory-mapped AC97 would work too.
    PciWrite32(bus, dev, fn, kPciCommand,
               PciRead32(bus, dev, fn, kPciCommand) |
               (kPciIoSpace | kPciMemSpace | kPciBusMaster));

    // Reset the PCM-OUT bus-master engine, then issue a cold reset request
    // (QEMU ignores GC_CR but a real codec honours it).
    NabWr8(kPoCr, kCrRr);
    NabWr32(kGlobCnt, kGcCr);

    // Codec reset: writing 0 to the NAM reset register makes QEMU's
    // mixer_reset() reload the vendor IDs and the 48 kHz sample rates.
    NamWr16(kNamReset, 0);
    for (TInt i = 0; i < kSpinCodec; ++i)
        {
        gCodecStat = NamRd16(kNamReset);
        gVendor1 = NamRd16(kVendorId1);
        gVendor2 = NamRd16(kVendorId2);
        // Real codecs set the ready bit; QEMU reports 0x0000 there but always
        // returns a valid vendor ID once reset has completed.
        if ((gCodecStat & kCodecReady) ||
            (gVendor1 && gVendor1 != 0xFFFFu))
            break;
        }
    // Explicit 48 kHz front DAC; unmute master + PCM-out (mute is bit 15).
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

    // Precompute one period of a 1 kHz sine, duplicated to both stereo
    // channels. Integer table: no libm / FPU dependency in the kernel.
    for (TUint i = 0; i < kFrames; ++i)
        {
        TUint16 s = (TUint16)kSine[i % kPeriodFrames];
        gPcm[2 * i] = s;
        gPcm[2 * i + 1] = s;
        }

    BdEntry& bd = gBdl[0];
    bd.iAddr = (TUint32)gPcm;
    bd.iCtlLen = (TUint32)kTotalWords | kBdIoc;
    Barrier();

    // Program one descriptor (index 0), clear stale status, then start.
    // BDBAR must be written before CR: QEMU fetches descriptor 0 on the
    // CR run/stop write that sets RPBM.
    NabWr8(kPoCr, kCrRr);
    NabWr32(kPoBdbar, (TUint32)gBdl);
    NabWr8(kPoLvi, 0);
    NabWr16(kPoSr, kSrWclear);
    Barrier();
    NabWr8(kPoCr, kCrRpbm);

    KPrintf("audio play: 1kHz stereo 48kHz %u words (%u bytes)\r\n",
        (TUint)kTotalWords, (TUint)(kTotalWords * 2));

    // Polled completion: QEMU sets DCH (DMA halted) when PICB drains to zero
    // and CIV == LVI, i.e. the last descriptor has been consumed.
    TUint32 spin = 0;
    for (; spin < kSpinPlay; ++spin)
        {
        if ((NabRd8(kPoSr) & kSrDch) && NabRd16(kPoPicb) == 0)
            break;
        __asm__ __volatile__("pause");
        }
    TUint8 sr = NabRd8(kPoSr);
    TUint16 picb = NabRd16(kPoPicb);
    if (spin == kSpinPlay || !(sr & kSrDch) || picb != 0)
        {
        KPrintf("audio play: timeout sr=%x picb=%u civ=%u\r\n",
            sr, (TUint)picb, (TUint)NabRd8(kPoCiv));
        return KErrTimedOut;
        }
    KPrintf("audio play: PCM play done (%u bytes, sr=%x, spins=%u)\r\n",
        (TUint)(kTotalWords * 2), sr, spin);
    return KErrNone;
    }

// Play one arbitrary-frequency note. Phase-accumulate through the existing
// 48-entry 1 kHz sine table with 16.16 fractional interpolation: no FPU or
// libm in the kernel, and non-1 kHz notes stay in tune. Exposed to ring-3
// E32 programs through the dedicated slow exec in qemu_audio_exec.cpp.
TInt audio_tone(TInt aFreq, TInt aMs)
    {
    if (audio_init() != KErrNone)
        return KErrNotReady;
    if (aFreq < 20 || aFreq > 20000 || aMs < 1 || aMs > 500)
        return KErrArgument;

    TUint frames = (TUint)((kRateHz * aMs) / 1000);
    if (frames > kMaxToneFrames)
        frames = kMaxToneFrames;
    // Table entries advanced per output frame: 48 entries/period at 48 kHz
    // means freq/1000 entries per sample; keep it in 16.16 fixed point.
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

    BdEntry& bd = gBdl[0];
    bd.iAddr = (TUint32)gTonePcm;
    bd.iCtlLen = (TUint32)words | kBdIoc;
    Barrier();

    NabWr8(kPoCr, kCrRr);
    NabWr32(kPoBdbar, (TUint32)gBdl);
    NabWr8(kPoLvi, 0);
    NabWr16(kPoSr, kSrWclear);
    Barrier();
    NabWr8(kPoCr, kCrRpbm);

    KPrintf("audio tone: %dHz %dms %u frames (%u bytes)\r\n",
        aFreq, aMs, frames, (TUint)(words * 2));

    TUint32 spin = 0;
    for (; spin < kSpinPlay; ++spin)
        {
        if ((NabRd8(kPoSr) & kSrDch) && NabRd16(kPoPicb) == 0)
            break;
        __asm__ __volatile__("pause");
        }
    const TUint8 sr = NabRd8(kPoSr);
    const TUint16 picb = NabRd16(kPoPicb);
    if (spin == kSpinPlay || !(sr & kSrDch) || picb != 0)
        {
        KPrintf("audio tone: timeout sr=%x picb=%u civ=%u\r\n",
            sr, (TUint)picb, (TUint)NabRd8(kPoCiv));
        return KErrTimedOut;
        }
    KPrintf("audio tone: done %dHz %dms (sr=%x, spins=%u)\r\n",
        aFreq, aMs, sr, spin);
    return KErrNone;
    }

// Boot hook: HwInit0 walks the .init_array after paging + TheAsic are ready.
__attribute__((constructor)) static void AudioBootInit()
    { audio_init(); }
