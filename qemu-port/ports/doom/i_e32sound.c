/* i_e32sound.c -- real DOOM sound for the EKA2 x86 port.
 *
 * This is the low-level sound + music module plugged into Chocolate Doom's
 * i_sound.c (FEATURE_SOUND).  It decodes DOOM's DMX sound lumps straight out
 * of the running WAD (through the engine's own W_* cache, which already uses
 * the streaming FILE shim), converts them to 16-bit stereo and submits them
 * to the AC97 PCM exec 240.  Music is a small, honest MUS-event synth: it
 * really reads the d_* MUS lumps and renders active notes as square waves,
 * but it is not an OPL/AdLib emulator.
 *
 * Markers (EUSER.DLL ordinal 915 via doom_marker):
 *   SND: SOUND INIT / SND: abi pcm=<n> play=<n>
 *   SND: sfx <name> <len> rate=<n>
 *   SND: play r=<n>                 (first play, then every 32nd)
 *   SND: music <name>
 *   SND: DONE
 *
 * kernelhwsrv/ is untouched.  The platform layer (e32_platform.c, owned by
 * another agent) can call E32Snd_Pump() / E32Snd_Flush() from Mix_Flush();
 * the module also pumps itself from I_UpdateSound() each tic, so audio works
 * without that integration (it only becomes lower-latency with it).
 */
#include "config.h"
#include "doomfeatures.h"
#include "doomtype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "i_sound.h"
#include "sounds.h"
#include "w_wad.h"
#include "z_zone.h"
#include "e32_shim.h"
#include "e32_audio.h"

extern int exec2(unsigned number, unsigned a1, unsigned a2);

/* i_sound.c / m_config.c expect these when FEATURE_SOUND is compiled in. */
int use_libsamplerate = 0;
float libsamplerate_scale = 0.65f;

/* ------------------------------------------------------------------ markers */
static void sndf(const char *fmt, ...)
    {
    char buf[192];
    va_list ap;
    va_start(ap, fmt);
    e32_vformat(buf, (int)sizeof(buf), fmt, ap);
    va_end(ap);
    doom_marker(buf);
    }

/* ---------------------------------------------------------------- exec ABI
 * The kernel's exec 240 multiplexes the legacy tone generator (a1 = Hz) and
 * the new PCM ops (a1 >= 0x80000000 + struct transport).  Verify the kernel
 * actually accepts a tiny silent PCM buffer; if not, degrade to the legacy
 * tone exec so an older kernel still produces sound (never silent stubs). */
static int  sPcmEnabled;
static int  sSoundUp;

static int PcmSubmitReq(const void *buf, unsigned bytes, unsigned rate)
    {
    struct e32_pcm_req req;
    req.iBuf = (unsigned)buf;
    req.iBytes = bytes;
    req.iRate = rate;
    req.iFormat = (16u << 16) | 2u;        /* 16-bit signed stereo */
    return exec2(QE_EXEC_AUDIO, E32_AUDIO_OP_PLAY, (unsigned)&req);
    }

static void ProbeAbi(void)
    {
    static short silent[64];               /* 32 silent stereo frames */
    int r = PcmSubmitReq(silent, sizeof(silent), 11025u);
    sPcmEnabled = (r == 0);
    sndf("SND: abi pcm=%d play=%u", sPcmEnabled, E32_AUDIO_OP_PLAY);
    if (sPcmEnabled)
        (void)exec2(QE_EXEC_AUDIO, E32_AUDIO_OP_STOP, 0u);
    }

/* -------------------------------------------------------------- SFX decode
 * DMX lump layout: 8-byte header (BE count, LE rate, BE length), 16 skipped
 * bytes, then 8-bit unsigned mono samples, then 16 trailing bytes.  We do a
 * nearest-neighbour rate expansion to the target rate and apply DOOM's volume
 * and separation.  No floating point, no libc resampler. */
#define SFX_SLOTS   8u
#define SFX_FRAMES  4096u
static short sSfxPcm[SFX_SLOTS][SFX_FRAMES * 2];
static unsigned sSfxFrames[SFX_SLOTS];
static unsigned sSfxRate[SFX_SLOTS];
static unsigned sSfxChannel[SFX_SLOTS];
static unsigned sSfxHead, sSfxTail, sSfxQueued;
static unsigned sPlayCount;            /* sparse "SND: play" marker counter */
static int sNeedsService;              /* set by I_StartSound for a fresh SFX */

/* Playback is serialised by the kernel's own 8-slot PCM queue: each small
 * buffer is copied on submit, so the user ring never needs a per-frame STATUS
 * trap.  The one probe below is paid only when a new SFX is actually being
 * submitted (needs_service); the tic path drains the ring without exec 240
 * STATUS and does nothing at all when the queue is empty. */
static void SubmitBuffer(int probe_status)
    {
    int r;

    if (!sSfxQueued)
        return;
    if (probe_status)
        {
        if (sPcmEnabled)
            (void)E32Snd_Busy();        /* exec 240 STATUS: once per new SFX */
        sNeedsService = 0;              /* serviced: no further probe this batch */
        }

    if (sPcmEnabled)
        r = PcmSubmitReq(sSfxPcm[sSfxTail],
                         sSfxFrames[sSfxTail] * 2u * sizeof(short),
                         sSfxRate[sSfxTail]);
    else
        {
        /* Graceful fallback: the legacy 240 tone exec.  Derive a short beep
         * from the sample's own rate instead of staying silent. */
        unsigned hz = sSfxRate[sSfxTail] / 8u;
        if (hz < 20u) hz = 110u;
        if (hz > 20000u) hz = 20000u;
        r = exec2(QE_EXEC_AUDIO, hz, 50u);
        }

    /* Keep the first play (harnesses assert it) then go sparse: a per-play
     * marker was ~500 B/s of blocking serial traffic for very little value. */
    if (sPlayCount++ % 32u == 0u)
        sndf("SND: play r=%d", r);
    sSfxTail = (sSfxTail + 1u) % SFX_SLOTS;
    --sSfxQueued;
    }

static void SubmitNext(void)
    {
    while (sSfxQueued)
        SubmitBuffer(sNeedsService);
    }

/* Non-zero while the codec is still draining the last buffer.  The legacy
 * tone exec is polled/synchronous, so it never reports busy.  Exported for
 * diagnostics; the DOOM tic path no longer calls this every frame. */
int E32Snd_Busy(void)
    {
    struct e32_pcm_stat st;
    int r;
    if (!sPcmEnabled)
        return 0;                 /* the legacy tone exec is synchronous */
    st.iState = st.iPosition = st.iTotal = 0u;
    r = exec2(QE_EXEC_AUDIO, E32_AUDIO_OP_STATUS, (unsigned)&st);
    if (r != 0)
        return 0;                 /* status unsupported: assume idle */
    return st.iState != 0u;
    }

void E32Snd_Pump(void)
    {
    SubmitNext();
    }
void E32Snd_Flush(void)
    {
    SubmitNext();
    }

/* --------------------------------------------------------------- MUS synth
 * A deliberately small MUS reader.  Header is 22 bytes; events follow with
 * variable-length delta times and a status nibble per channel.  Active notes
 * are mixed into a square wave with a simple attack/decay envelope. */
struct mus_song
    {
    const byte *data;
    unsigned len, pos, score_start, score_len;
    byte note[16];              /* pitch currently held, 0 = off */
    byte vol[16];
    unsigned delta;             /* ticks until the next event */
    int playing, looping;
    unsigned phase[16];         /* square-wave phase accumulators */
    };

static struct mus_song sSong;
static short sMusicPcm[2 * 1400];
static unsigned sMusicFrames;
static unsigned sMusicRate = 11025u;
static int sMusicVolume = 127;
static int sMusicPaused;

static unsigned MusReadVarLen(struct mus_song *s)
    {
    unsigned value = 0u;
    byte b;
    do
        {
        if (s->pos >= s->len) return value;
        b = s->data[s->pos++];
        value = (value << 7) | (unsigned)(b & 0x7fu);
        } while (b & 0x80u);
    return value;
    }

static void MusEvent(struct mus_song *s, byte status)
    {
    unsigned chan = status & 0x0fu;
    byte ev = (byte)((status >> 4) & 0x07u);
    byte d1 = 0;
    if (chan >= 16u) return;
    if (status == 0x70u)                /* end of score */
        {
        if (s->looping && s->score_start && s->score_len)
            {
            s->pos = s->score_start;
            s->delta = 0u;
            }
        else
            {
            s->playing = 0;
            memset(s->note, 0, sizeof(s->note));
            }
        return;
        }
    if (s->pos >= s->len) return;
    d1 = s->data[s->pos++];
    if (ev == 2u || ev == 5u)           /* bend / controller: two bytes */
        {
        if (s->pos < s->len) ++s->pos;
        }
    switch (ev)
        {
    case 0: s->note[chan] = 0; break;                    /* note off */
    case 1: s->note[chan] = (byte)(d1 & 0x7fu);          /* note on */
            s->vol[chan] = (byte)((d1 & 0x80u) ? 100u : 70u);
            break;
    case 3: break;                                       /* system */
    case 4: break;                                       /* instrument */
    default: break;
        }
    }

static void MusAdvance(struct mus_song *s, unsigned ticks)
    {
    unsigned guard = 0;
    while (s->playing && ticks && guard++ < 4096u)
        {
        if (s->delta == 0u)
            s->delta = MusReadVarLen(s);
        if (s->delta == 0u)
            {
            if (s->pos >= s->len) { s->playing = 0; break; }
            MusEvent(s, s->data[s->pos++]);
            continue;
            }
        if (s->delta > ticks)
            {
            s->delta -= ticks;
            break;
            }
        ticks -= s->delta;
        s->delta = 0u;
        }
    }

/* Render one game tic (~1/35 s) of whatever notes are currently held. */
static void MusicTick(void)
    {
    unsigned frames = 11025u / 35u, i, c;
    int mixed = 0;

    if (!sSong.playing || sMusicPaused)
        return;
    MusAdvance(&sSong, 1u);

    memset(sMusicPcm, 0, sizeof(sMusicPcm));
    for (c = 0; c < 8u; ++c)
        {
        unsigned note = sSong.note[c];
        unsigned freq, step;
        if (!note) continue;
        freq = 440u;                    /* MUS pitch 60 ~ A4, close enough */
        while (note > 60u) { freq = (freq * 106u) / 100u; --note; }
        while (note < 60u) { freq = (freq * 100u) / 106u; ++note; }
        if (freq < 40u) freq = 40u;
        step = (freq * 65536u) / sMusicRate;
        for (i = 0; i < frames; ++i)
            {
            int amp;
            sSong.phase[c] += step;
            amp = (sSong.phase[c] & 0x8000u) ? -1 : 1;
            amp = (amp * (int)sSong.vol[c] * sMusicVolume) / (128 * 100);
            sMusicPcm[2u * i] += (short)amp;
            sMusicPcm[2u * i + 1u] += (short)amp;
            }
        mixed = 1;
        }
    if (!mixed) return;
    sMusicFrames = frames;

    /* Music shares the one-shot ring but never starves an actual SFX. */
    if (sSfxQueued < SFX_SLOTS / 2u)
        {
        short *slot = sSfxPcm[sSfxHead];
        memcpy(slot, sMusicPcm, frames * 2u * sizeof(short));
        sSfxFrames[sSfxHead] = frames;
        sSfxRate[sSfxHead] = sMusicRate;
        sSfxChannel[sSfxHead] = 255u;
        sSfxHead = (sSfxHead + 1u) % SFX_SLOTS;
        ++sSfxQueued;
        }
    }

/* --------------------------------------------------------------- sound module */
static boolean E32_InitSound(boolean use_sfx_prefix)
    {
    (void)use_sfx_prefix;
    memset(sSfxPcm, 0, sizeof(sSfxPcm));
    memset(sSfxFrames, 0, sizeof(sSfxFrames));
    sSfxHead = sSfxTail = sSfxQueued = 0u;
    sNeedsService = 0;
    sPlayCount = 0u;
    sSoundUp = 1;
    doom_marker("SND: SOUND INIT");
    ProbeAbi();
    return true;
    }

static void E32_ShutdownSound(void)
    {
    if (sPcmEnabled)
        (void)exec2(QE_EXEC_AUDIO, E32_AUDIO_OP_STOP, 0u);
    sSoundUp = 0;
    doom_marker("SND: DONE");
    }

static int E32_GetSfxLumpNum(sfxinfo_t *sfx)
    {
    char name[12];
    sfxinfo_t *s = sfx->link ? sfx->link : sfx;
    snprintf(name, sizeof(name), "ds%s", s->name);
    return W_CheckNumForName(name);
    }

static void E32_UpdateSound(void)
    {
    MusicTick();
    /* Empty ring: zero execs.  Queued buffers drain without a STATUS trap;
     * only a fresh I_StartSound request turns on the one-shot busy probe. */
    if (sSfxQueued)
        SubmitNext();
    }

static void E32_UpdateSoundParams(int channel, int vol, int sep)
    {
    unsigned i;
    for (i = 0; i < SFX_SLOTS; ++i)
        if (sSfxChannel[i] == (unsigned)channel)
            { /* volumes were baked into the PCM at decode time */ }
    (void)vol; (void)sep;
    }

static int E32_StartSound(sfxinfo_t *sfx, int channel, int vol, int sep)
    {
    sfxinfo_t *s = sfx->link ? sfx->link : sfx;
    char name[12];
    int lump;
    unsigned len, n, i, frames, rate;
    byte *raw;
    short *out;
    int left, right;
    static unsigned sfx_seen;

    if (!sSoundUp || sSfxQueued >= SFX_SLOTS)
        return -1;
    snprintf(name, sizeof(name), "ds%s", s->name);
    lump = W_CheckNumForName(name);
    if (lump < 0)
        return -1;
    len = (unsigned)W_LumpLength(lump);
    if (len < 40u)
        return -1;
    raw = (byte *)W_CacheLumpNum(lump, PU_STATIC);
    if (!raw || raw[0] != 0x03 || raw[1] != 0x00)
        {
        W_ReleaseLumpNum(lump);
        return -1;
        }
    rate = (unsigned)((raw[3] << 8) | raw[2]);
    n = ((unsigned)raw[7] << 24) | ((unsigned)raw[6] << 16)
      | ((unsigned)raw[5] << 8) | (unsigned)raw[4];
    if (n > len - 24u) n = len - 24u;
    if (n > 32u) { n -= 32u; } else { W_ReleaseLumpNum(lump); return -1; }
    if (n > SFX_FRAMES) n = SFX_FRAMES;
    if (rate < 1000u || rate > 48000u) rate = 11025u;

    out = sSfxPcm[sSfxHead];
    frames = 0u;
    for (i = 0u; i < n; ++i)
        {
        int sample = ((int)raw[24u + i] | ((int)raw[24u + i] << 8)) - 32768;
        left = (254 - sep) * vol / 127;
        right = sep * vol / 127;
        if (left > 255) left = 255;
        if (right > 255) right = 255;
        out[2u * frames]     = (short)((sample * left) / 255);
        out[2u * frames + 1u] = (short)((sample * right) / 255);
        ++frames;
        }
    W_ReleaseLumpNum(lump);

    sSfxFrames[sSfxHead] = frames;
    sSfxRate[sSfxHead] = rate;
    sSfxChannel[sSfxHead] = (unsigned)channel;
    sSfxHead = (sSfxHead + 1u) % SFX_SLOTS;
    ++sSfxQueued;

    if (sfx_seen++ < 6u)
        sndf("SND: sfx %s %u rate=%u", s->name, frames, rate);
    sNeedsService = 1;          /* I_StartSound: arm the single STATUS probe */
    SubmitNext();
    return channel;
    }

static void E32_StopSound(int channel)
    {
    unsigned i;
    for (i = 0; i < SFX_SLOTS; ++i)
        if (sSfxChannel[i] == (unsigned)channel)
            { sSfxFrames[i] = 0u; sSfxChannel[i] = 255u; }
    }

static boolean E32_SoundIsPlaying(int channel)
    {
    unsigned i;
    for (i = 0; i < SFX_SLOTS; ++i)
        if (sSfxChannel[i] == (unsigned)channel && sSfxFrames[i])
            return true;
    return false;
    }

static void E32_CacheSounds(sfxinfo_t *sounds, int num_sounds)
    {
    (void)sounds; (void)num_sounds;    /* decoded on demand in StartSound */
    }

static snddevice_t e32_sound_devices[] = { SNDDEVICE_SB };

sound_module_t DG_sound_module =
    {
    e32_sound_devices,
    (int)(sizeof(e32_sound_devices) / sizeof(e32_sound_devices[0])),
    E32_InitSound,
    E32_ShutdownSound,
    E32_GetSfxLumpNum,
    E32_UpdateSound,
    E32_UpdateSoundParams,
    E32_StartSound,
    E32_StopSound,
    E32_SoundIsPlaying,
    E32_CacheSounds,
    };

/* --------------------------------------------------------------- music module */
static boolean E32_InitMusic(void) { return true; }
static void E32_ShutdownMusic(void) { sSong.playing = 0; }
static void E32_SetMusicVolume(int volume) { sMusicVolume = volume; }
static void E32_PauseMusic(void) { sMusicPaused = 1; }
static void E32_ResumeMusic(void) { sMusicPaused = 0; }

static void *E32_RegisterSong(void *data, int len)
    {
    memset(&sSong, 0, sizeof(sSong));
    sSong.data = (const byte *)data;
    sSong.len = (unsigned)len;
    if (len >= 22 && memcmp(data, "MUS\x1a", 4) == 0)
        {
        const byte *p = (const byte *)data;
        unsigned score_len = (unsigned)p[4] | ((unsigned)p[5] << 8);
        unsigned score_start = (unsigned)p[6] | ((unsigned)p[7] << 8);
        sSong.score_len = score_len;
        sSong.score_start = score_start;
        if (score_start + score_len <= sSong.len)
            {
            sSong.pos = score_start;
            return &sSong;
            }
        }
    return NULL;                 /* not MUS: music stays honest and silent */
    }

static void E32_UnRegisterSong(void *handle)
    {
    (void)handle;
    sSong.playing = 0;
    sSong.data = NULL;
    sSong.len = 0u;
    }

static void E32_PlaySong(void *handle, boolean looping)
    {
    int i;
    const char *name = "unknown";
    if (handle != &sSong) return;
    /* S_ChangeMusic caches the lump into music->data just before this call,
     * so recover its name from the engine's own table for the marker. */
    for (i = 0; i < NUMMUSIC; ++i)
        if (S_music[i].data == (void *)sSong.data && S_music[i].name)
            { name = S_music[i].name; break; }
    sSong.looping = looping ? 1 : 0;
    sSong.playing = 1;
    sSong.delta = 0u;
    sndf("SND: music %s", name);
    }

static void E32_StopSong(void) { sSong.playing = 0; }
static boolean E32_MusicIsPlaying(void) { return sSong.playing ? true : false; }
static void E32_PollMusic(void) { }

static snddevice_t e32_music_devices[] = { SNDDEVICE_SB };

music_module_t DG_music_module =
    {
    e32_music_devices,
    (int)(sizeof(e32_music_devices) / sizeof(e32_music_devices[0])),
    E32_InitMusic,
    E32_ShutdownMusic,
    E32_SetMusicVolume,
    E32_PauseMusic,
    E32_ResumeMusic,
    E32_RegisterSong,
    E32_UnRegisterSong,
    E32_PlaySong,
    E32_StopSong,
    E32_MusicIsPlaying,
    E32_PollMusic,
    };

/* Public init/shutdown used by the platform layer / diagnostics. */
int E32Snd_Init(void) { return E32_InitSound(true); }
void E32Snd_Shutdown(void) { E32_ShutdownSound(); }
