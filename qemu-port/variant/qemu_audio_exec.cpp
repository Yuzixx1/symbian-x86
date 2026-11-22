// qemu_audio_exec.cpp — expose the AC'97 audio engine to ring-3 E32 programs.
//
// A single dedicated slow exec, slot 240, carries the whole audio ABI.  The
// high bit of a1 selects the new PCM ops; legacy calls (a1 = Hz, a2 = ms) keep
// driving audio_tone() unchanged, so MORSE/MUSIC/SOUNDPLR still work:
//
//   exec2(240, freq, ms)                       -> audio_tone (legacy)
//   exec2(240, AU_PCM_PLAY,   &au_pcm_req)     -> async PCM DMA (queued/copied)
//   exec2(240, AU_PCM_STATUS, &au_pcm_stat)    -> busy / position / total
//   exec2(240, AU_PCM_STOP,   0)               -> halt DMA + flush queue
//   exec2(240, AU_PCM_SELFTEST, 0)             -> kernel 1 s 440 Hz self-test
//   exec2(240, AU_PCM_STATS,  &au_pcm_stats)   -> played/dropped/busy/completed
//
// This mirrors the netbridge (247/248) and SvcMgr (268..270) capability-exposure
// pattern: patch an invalid entry of the genuine EpocSlowExecTable at boot,
// with kernelhwsrv/ left untouched.  Slot 240 is the sole still-invalid entry
// that is not already claimed by netbridge (247/248), GUI (249..251), the ps
// census (241) or SvcMgr/windowsrv (268..275).

#include <x86.h>
#include <e32err.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

#define QE_EXEC_AUDIO 240u

extern const TUint32 EpocSlowExecTable[];

namespace
    {
    static TBool gAudioExecInstalled = EFalse;

    // All transport structs live in low, identity-mapped RAM (U/S=1 below
    // 0x80000000).  Reject anything else, plus wrap-around, before deref.
    static TBool LowRamOk(const void* aPtr, TUint aBytes)
        {
        const TUint32 p = (TUint32)aPtr;
        return aBytes != 0 && p >= 0x1000u && p < 0x80000000u &&
               aBytes <= 0x80000000u - p;
        }

    // Slow exec ABI: TInt handler(TInt a1, TInt a2) -> Symbian error code.
    static TInt AudioExec(TInt a1, TInt a2)
        {
        if ((TUint)a1 < 0x80000000u)       // legacy tone: a1=Hz, a2=ms
            return audio_tone(a1, a2);

        switch (a1)
            {
            case AU_PCM_PLAY:
                {
                const au_pcm_req* rq = (const au_pcm_req*)a2;
                if (!LowRamOk(rq, sizeof(*rq)))
                    return KErrArgument;
                // The canonical audio_pcm_play() revalidates iBuf/iBytes,
                // copies small buffers into the 8-slot queue (so the caller
                // may reuse its buffer immediately), converts the format and
                // arms the one AC97 DMA engine asynchronously.
                return audio_pcm_play((const void*)rq->iBuf, rq->iBytes,
                                      rq->iRate, rq->iFormat);
                }
            case AU_PCM_STATUS:
                {
                au_pcm_stat* st = (au_pcm_stat*)a2;
                if (!LowRamOk(st, sizeof(*st)))
                    return KErrArgument;
                return audio_pcm_status(st);
                }
            case AU_PCM_STOP:
                return audio_pcm_stop();
            case AU_PCM_SELFTEST:
                return audio_pcm_selftest();
            case AU_PCM_STATS:
                {
                au_pcm_stats* st = (au_pcm_stats*)a2;
                if (!LowRamOk(st, sizeof(*st)))
                    return KErrArgument;
                TUint played = 0, dropped = 0, busy = 0, completed = 0;
                audio_pcm_stats(played, dropped, busy, completed);
                st->iPlayed = played;
                st->iDropped = dropped;
                st->iBusy = busy;
                st->iCompleted = completed;
                return KErrNone;
                }
            default:
                return KErrNotSupported;
            }
        }

    static void AudioExecInstall()
        {
        TUint32* tab = (TUint32*)EpocSlowExecTable;
        // Need entries 0..240; this build's table count is 271.
        if (tab[0] < QE_EXEC_AUDIO + 1u)
            {
            KPrintf("audio exec: table count %u, install skipped\r\n", tab[0]);
            return;
            }
        const TUint s = QE_EXEC_AUDIO;
        const TUint fnIdx = 4u + 2u * s;
        // Only take a slot still pointing at InvalidExecHandler (tab[1]).
        if (tab[fnIdx] != tab[1] && tab[fnIdx] != 0u)
            {
            KPrintf("audio exec: slot %u not invalid (%x), skip\r\n", s, tab[fnIdx]);
            return;
            }
        tab[3u + 2u * s] = 0;                          // flags
        tab[fnIdx] = (TUint32)(TLinAddr)&AudioExec;     // handler
        gAudioExecInstalled = ETrue;
        KPrintf("audio exec: tone installed at slot %u\r\n", s);
        KPrintf("audio exec: PCM ops at slot %u (play/status/stop/selftest/stats)\r\n", s);
        }
    }  // namespace

// Boot hook: HwInit0 walks the .init_array after paging + TheAsic are ready.
__attribute__((constructor)) static void AudioExecBootInit()
    { AudioExecInstall(); }
