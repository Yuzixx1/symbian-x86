// qemu_audio_exec.cpp — expose the AC'97 tone generator to ring-3 E32 programs.
//
// A single dedicated slow exec, QE_EXEC_AUDIO_TONE, calls audio_tone(freq, ms)
// from qemu_audio.cpp.  This mirrors the netbridge (247/248) and SvcMgr
// (268/269/270) capability-exposure pattern: patch an invalid entry of the
// genuine EpocSlowExecTable at boot, with kernelhwsrv/ left untouched.
//
// Slot choice: 251 is NOT used here because the GUI agent's qemu_gui_exec.cpp
// already repurposes 249/250/251, and 247/248 belong to netbridge.  Dumping
// the linked EpocSlowExecTable shows the only still-invalid entries are
// 240, 247..251; 240 is the sole free one (252..267 are genuine execs and
// 268..270 are SvcMgr's).  Taking 240 avoids stealing another worker's exec.

#include <x86.h>
#include <e32err.h>
#include <kernel/kern_priv.h>
#include "qemu_variant.h"

#define QE_EXEC_AUDIO_TONE 240u

extern const TUint32 EpocSlowExecTable[];

namespace
    {
    static TBool gAudioExecInstalled = EFalse;

    // Slow exec ABI: TInt handler(TInt a1, TInt a2) -> Symbian error code.
    // a1 = frequency in Hz, a2 = duration in milliseconds.
    static TInt AudioToneExec(TInt aFreq, TInt aMs)
        { return audio_tone(aFreq, aMs); }

    static void AudioExecInstall()
        {
        TUint32* tab = (TUint32*)EpocSlowExecTable;
        // Need entries 0..240; this build's table count is 271.
        if (tab[0] < QE_EXEC_AUDIO_TONE + 1u)
            {
            KPrintf("audio exec: table count %u, install skipped\r\n", tab[0]);
            return;
            }
        const TUint s = QE_EXEC_AUDIO_TONE;
        const TUint fnIdx = 4u + 2u * s;
        // Only take a slot still pointing at InvalidExecHandler (tab[1]).
        if (tab[fnIdx] != tab[1] && tab[fnIdx] != 0u)
            {
            KPrintf("audio exec: slot %u not invalid (%x), skip\r\n", s, tab[fnIdx]);
            return;
            }
        tab[3u + 2u * s] = 0;                          // flags
        tab[fnIdx] = (TUint32)(TLinAddr)&AudioToneExec; // handler
        gAudioExecInstalled = ETrue;
        KPrintf("audio exec: tone installed at slot %u\r\n", s);
        }
    }  // namespace

// Boot hook: HwInit0 walks the .init_array after paging + TheAsic are ready.
__attribute__((constructor)) static void AudioExecBootInit()
    { AudioExecInstall(); }
