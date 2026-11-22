/* e32_audio.h -- DOOM.E32 <-> AC97 PCM exec contract (exec 240).
 *
 * User-side copy of the ABI published by variant/qemu_variant.h and
 * variant/qemu_audio_exec.cpp.  kernelhwsrv/ is not touched.
 *
 *   exec2(240, E32_AUDIO_OP_PLAY,   &struct e32_pcm_req)  -> 0 on accept
 *   exec2(240, E32_AUDIO_OP_STATUS, &struct e32_pcm_stat) -> 0, fills status
 *   exec2(240, E32_AUDIO_OP_STOP,   0)                    -> 0
 *
 * The high bit of the op selector distinguishes PCM ops from the legacy tone
 * exec (a1 = Hz < 0x80000000, a2 = ms).  The kernel copies/converts the source
 * buffer into its own DMA pool, so a user buffer can be reused immediately
 * after a successful PLAY.
 */
#ifndef E32_DOOM_AUDIO_H
#define E32_DOOM_AUDIO_H

#define QE_EXEC_AUDIO         240u
#define E32_AUDIO_OP_PLAY     0x80000001u
#define E32_AUDIO_OP_STATUS   0x80000002u
#define E32_AUDIO_OP_STOP     0x80000003u

struct e32_pcm_req
    {
    unsigned iBuf;             /* linear address of 16-bit stereo PCM */
    unsigned iBytes;           /* source length in bytes */
    unsigned iRate;            /* 11025 / 22050 / 44100 / 48000 Hz */
    unsigned iFormat;          /* (bits<<16) | channels, e.g. (16<<16)|2 */
    };

struct e32_pcm_stat
    {
    unsigned iState;           /* 0 = idle, 1 = DMA busy */
    unsigned iPosition;        /* bytes consumed */
    unsigned iTotal;           /* bytes submitted */
    };

/* Public entry points used by the engine module and the platform layer. */
int  E32Snd_Init(void);
void E32Snd_Shutdown(void);
/* Submit one 16-bit stereo buffer.  Returns the exec result (0 = accepted). */
int  E32Snd_Submit(const short *stereo, unsigned frames, unsigned rate);
/* Non-zero while a PCM DMA is still draining. */
int  E32Snd_Busy(void);
/* Pump queued SFX/music buffers.  Safe to call from Mix_Flush() or a tic.
 * Both names are exported; the platform agent may call either one. */
void E32Snd_Pump(void);
void E32Snd_Flush(void);

#endif /* E32_DOOM_AUDIO_H */
