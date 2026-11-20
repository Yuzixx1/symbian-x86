/*
 * soundplr.c -- PE2 E32 sound player: play a short melody through the AC'97
 * PCM-OUT driver via a dedicated kernel slow exec.
 *
 * exec 240 (QE_EXEC_AUDIO_TONE): a1 = frequency (Hz), a2 = duration (ms) -> a
 * Symbian error code (0 = the note was DMA'd to the codec).  The kernel host is
 * variant/qemu_audio_exec.cpp; slot 240 is used because 247/248 (netbridge),
 * 249/250/251 (GUI) and 268/269/270 (SvcMgr) are already taken.
 *
 * Markers use genuine EUSER.DLL ordinal 915 (RDebug::Printf), bound by E32Run
 * from the PE2 IAT at 0x408000.  No libc / FPU dependency in userland.
 */
#include "runtime.h"

extern int exec2(unsigned number, unsigned arg1, unsigned arg2);
extern unsigned int soundplr_printf_slot;
typedef void (*NbPrintf)(const char*, ...);

#define QE_EXEC_AUDIO_TONE 240u

static void marker(const char* text)
{
    unsigned int fn = *(volatile unsigned int*)&soundplr_printf_slot;
    if (fn != 915u && fn != 0u) ((NbPrintf)fn)(text);
    else print(text);                         /* no EUSER.DLL bound */
}

/* ---- tiny formatters (no libc) ---- */
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

/* C4 - E4 - G4 - C5 (equal-temperament, rounded to whole Hz), 200 ms each. */
struct sound_note { unsigned freq; unsigned ms; };
static const struct sound_note kMelody[] =
    {
    { 262u, 200u }, { 330u, 200u }, { 392u, 200u }, { 523u, 200u }
    };

int app_main(void)
{
    char line[160];
    unsigned i;

    marker("SND: SOUND PLAYER START");
    for (i = 0; i < sizeof(kMelody) / sizeof(kMelody[0]); ++i)
        {
        const unsigned freq = kMelody[i].freq;
        const unsigned ms = kMelody[i].ms;
        const int r = exec2(QE_EXEC_AUDIO_TONE, freq, ms);
        unsigned k = append(line, 0, "SND: note ");
        k = dec(line, k, freq);
        k = append(line, k, "Hz ");
        k = dec(line, k, ms);
        k = append(line, k, "ms r=");
        if (r < 0) { line[k++] = '-'; k = dec(line, k, (unsigned)(-r)); }
        else k = dec(line, k, (unsigned)r);
        line[k] = 0;
        marker(line);

        if (r < 0)                    /* exec missing or codec not ready */
            {
            marker("SND: audio exec unavailable");
            marker("SND: DONE");
            return 1;
            }
        }
    marker("SND: melody done");
    marker("SND: DONE");
    return 0;
}
