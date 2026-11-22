/*
 * audiopcm.c -- end-to-end proof of the AC'97 PCM sample-playback exec.
 *
 * Builds a 1-second, 440 Hz, 16-bit stereo PCM buffer in user low RAM, submits
 * it with exec 240 op AU_PCM_PLAY, then polls AU_PCM_STATUS until the kernel's
 * PCM-OUT DMA reports completion.  A final AU_PCM_STOP proves the stop op.
 *
 *   exec2(240, AU_PCM_PLAY,   &au_pcm_req)    -> 0 on success
 *   exec2(240, AU_PCM_STATUS, &au_pcm_stat)   -> 0, fills iState/iPosition
 *   exec2(240, AU_PCM_STOP,   0)              -> 0
 *
 * Host side: variant/qemu_audio_exec.cpp + qemu_audio.cpp.  Markers use a
 * genuine PE2 import of EUSER.DLL ordinal 915 (RDebug::Printf) at IAT slot
 * audiopcm_printf_slot == 0x408000; no libc / FPU dependency in userland.
 */
#include "runtime.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int audiopcm_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

#define QE_EXEC_AUDIO     240u
#define AU_PCM_PLAY       0x80000001u
#define AU_PCM_STATUS     0x80000002u
#define AU_PCM_STOP       0x80000003u

struct au_pcm_req  { unsigned iBuf, iBytes, iRate, iFormat; };
struct au_pcm_stat { unsigned iState, iPosition, iTotal; };

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&audiopcm_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);
    else print(text);
}

static unsigned append(char* p, unsigned k, const char* s)
{
    while (*s) p[k++] = *s++;
    return k;
}
static unsigned dec(char* p, unsigned k, unsigned v)
{
    char t[12];
    int n = 0;
    if (!v) t[n++] = '0';
    while (v) { t[n++] = (char)('0' + v % 10u); v /= 10u; }
    while (n) p[k++] = t[--n];
    return k;
}

/* 1 s of 44.1 kHz, 16-bit stereo = 88200 words = 176400 bytes. */
#define RATE   44100u
#define FRAMES 44100u
static short gPcm[FRAMES * 2];

/* 48-entry integer sine table (one 1 kHz period at 48 kHz), amplitude 9000. */
static const short kSine[48] = {
     0, 1175, 2329, 3444, 4500, 5479, 6364, 7140,
  7794, 8315, 8693, 8923, 9000, 8923, 8693, 8315,
  7794, 7140, 6364, 5479, 4500, 3444, 2329, 1175,
     0,-1175,-2329,-3444,-4500,-5479,-6364,-7140,
 -7794,-8315,-8693,-8923,-9000,-8923,-8693,-8315,
 -7794,-7140,-6364,-5479,-4500,-3444,-2329,-1175
};

int app_main(void)
{
    char line[160];
    struct au_pcm_req rq;
    struct au_pcm_stat st;
    unsigned i, r, spins = 0;
    const unsigned step = (48u * 440u * 65536u) / RATE; /* 16.16 entries/sample */
    unsigned phase = 0;

    marker("APCM: START");
    for (i = 0; i < FRAMES; ++i)
        {
        const unsigned idx = phase >> 16;
        const int frac = (int)(phase & 0xFFFFu);
        const int s0 = kSine[idx];
        const int s1 = kSine[(idx + 1u) % 48u];
        const short v = (short)(s0 + ((s1 - s0) * frac >> 16));
        gPcm[2 * i] = v;
        gPcm[2 * i + 1] = v;
        phase += step;
        if (phase >= (48u << 16)) phase -= (48u << 16);
        }

    rq.iBuf    = (unsigned)gPcm;
    rq.iBytes  = sizeof(gPcm);
    rq.iRate   = RATE;
    rq.iFormat = (16u << 16) | 2u;          /* 16-bit signed stereo */
    r = (unsigned)exec2(QE_EXEC_AUDIO, AU_PCM_PLAY, (unsigned)&rq);
    {
        unsigned k = append(line, 0, "APCM: play r=");
        k = dec(line, k, r);
        k = append(line, k, " bytes=");
        k = dec(line, k, rq.iBytes);
        k = append(line, k, " Hz=");
        k = dec(line, k, RATE);
        line[k] = 0;
        marker(line);
    }
    if (r != 0u) { marker("APCM: FAIL play exec"); return 1; }

    /* Poll the kernel until DMA completion (iState drops to 0). */
    for (i = 0; i < 200000u; ++i)
        {
        st.iState = st.iPosition = st.iTotal = 0u;
        if (exec2(QE_EXEC_AUDIO, AU_PCM_STATUS, (unsigned)&st) != 0)
            { marker("APCM: FAIL status exec"); return 1; }
        ++spins;
        if (!st.iState) break;
        }
    {
        unsigned k = append(line, 0, "APCM: done spins=");
        k = dec(line, k, spins);
        k = append(line, k, " state=");
        k = dec(line, k, st.iState);
        k = append(line, k, " pos=");
        k = dec(line, k, st.iPosition);
        k = append(line, k, " total=");
        k = dec(line, k, st.iTotal);
        line[k] = 0;
        marker(line);
    }
    if (st.iState != 0u || st.iTotal != rq.iBytes ||
        st.iPosition != rq.iBytes)
        { marker("APCM: FAIL DMA completion"); return 1; }

    r = (unsigned)exec2(QE_EXEC_AUDIO, AU_PCM_STOP, 0u);
    {
        unsigned k = append(line, 0, "APCM: stop r=");
        k = dec(line, k, r);
        line[k] = 0;
        marker(line);
    }
    marker("APCM: PASS");
    return 0;
}
