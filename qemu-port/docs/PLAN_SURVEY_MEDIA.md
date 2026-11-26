# PLAN SURVEY — media/codec/audio sources in kernelhwsrv vs qemu-port

Scope: read-only survey of `kernelhwsrv/` for genuine Symbian media/codec/audio/video
server code, compared against qemu-port's AC'97 bridge (`variant/qemu_audio.cpp`,
`variant/qemu_audio_exec.cpp`) and the DOOM audio path (`ports/doom/i_e32sound.c`).
No changes were made to `kernelhwsrv/`.

## Present sources

Genuine Symbian code in kernelhwsrv (all read-only reference, none wired into qemu-port):

- **Sound LDD (the real DevSound low-level driver)** —
  `kernelhwsrv/kernel/eka/drivers/soundsc/soundldd.cpp` (3000 lines): full
  `DSoundScLdd` with `PlayData()` / `RecordData()` (async, shared-chunk buffers,
  transfer IDs, EOF timer), config/volume setters, power handler.
  Kernel-side header `kernel/eka/include/drivers/soundsc.h` (621 lines) and
  user-side `kernel/eka/include/d32soundsc.h` (620 lines) define the
  `RSoundSc` / "SoundSc" channel ABI: 15 sample rates 7350–48000 Hz,
  8/16-bit PCM, interleaved/non-interleaved, mono/stereo caps.
- **Template PDD** — `kernelhwsrv/bsptemplate/asspandvariant/template_variant/specific/soundsc_tx.cpp`
  and `soundsc_rx.cpp`: stub physical driver for a `SoundSc.Template` device
  (TO-DO placeholders, not a real AC'97 PDD).
- **Tests only** — `kernelhwsrv/kerneltest/e32test/multimedia/` (t_sound_api,
  t_sound2, t_soundwav, t_soundmchan) exercise the SoundSc API; `.../video/`
  has wsrv video-mode tests. No production server code.
- **Storage "media" drivers** (`drivers/locmedia`, `drivers/media`, `medlfs`,
  `pbus`) are flash/disk block drivers, not multimedia — easy to confuse by name.
- **Bitmap/framebuffer bits** exist only as EKA2 primitives
  (`kernel/eka/euser/cbase/ub_bma.cpp`, `include/pixelformats.h`, ewsrv
  `ws_dat.cpp`/`ws_win.cpp`); no image codec plugins.

qemu-port today (variant-owned, kernelhwsrv untouched):

- `qemu-port/variant/qemu_audio.cpp` (764 lines): AC'97 (82801AA, PCI 04.01)
  PCM-OUT DMA driver — one BDL engine, 8x16 KiB queue, canonical
  `audio_pcm_play(buf,bytes,rate,format)`, legacy tone on the same engine.
- `qemu-port/variant/qemu_audio_exec.cpp`: exec 240 (`QE_EXEC_AUDIO`)
  multiplexing tone + PCM PLAY/STATUS/STOP ops.
- `qemu-port/ports/doom/i_e32sound.c` + `e32_audio.h`: DOOM decodes DMX SFX lumps
  from the WAD to 16-bit stereo and submits via exec 240; MUS music is a
  square-wave synth, not OPL. Test players: `user/soundplr.c`, `user/audiopcm.c`.

## Missing pieces

- No **DevSound server** (the user-mode `CDevSound`/`MMF` server that owns
  `RSoundSc` sessions, mixing, codecs) — that lives in the separate Symbian
  `mmf` package, absent from kernelhwsrv entirely.
- No **MMF codec plugins** (MP3/AAC/WMA decode, WAV parse, MIDI) anywhere in
  kernelhwsrv; no image codecs (JPEG/PNG/GIF) or video decode.
- No **media player server** of any kind (no `CMdaAudioPlayerUtility`-style
  server in this tree).
- No AC'97-specific PDD — the template PDD is a stub; qemu-port's bridge is
  variant-owned and does not reuse `soundldd.cpp`.
- No record path on the qemu side (AC'97 PCM-IN unused); genuine LDD has full
  `RecordData()`.

## Hardware/bridge gaps

- Genuine LDD is built around **shared-chunk ring buffers + async request
  status + PDD callback model**; qemu-port uses a **polled 8-slot queue with
  copied buffers** behind exec 240. ABI shapes differ, so direct reuse means
  porting the LDD's buffer manager, not just the PCI code.
- Genuine driver assumes a variant PDD providing `Caps/SetConfig/PlayData`
  against real hardware; on QEMU the "PDD" would be the AC'97 NAB DMA engine
  already in `qemu_audio.cpp` — that part maps 1:1 (BDL ≈ LDD transfer list).
- Rate/format coverage: LDD advertises 7350–48000 Hz 8/16-bit; qemu bridge fixes
  48 kHz output with software conversion — fine for playback, but no hardware
  rate switching or mixer controls beyond NAM volume registers.
- Record/mic and any video-out path have no QEMU device backing at all
  (q35 AC97 has PCM-IN, but nothing drives it).

## Feasibility verdict

**Feasible for audio playback; not for a full media player.** A media-player
workload needs only PCM-out + a file decoder, and both halves exist: genuine
`soundldd.cpp` shows the exact driver contract, and qemu-port already has a
working AC'97 PCM path that DOOM uses. What does NOT exist anywhere in the tree
is the codec/player layer (MMF server, MP3/WAV decode) — that must be written
or ported (e.g. reuse DOOM's WAV/DMX decode patterns), not found. Video is
out of reach: no genuine video server and no codec. Verdict: **audio player =
green, image/video player = red**.

## First 3 concrete steps

1. Write a thin AC'97 PDD under qemu-port `variant/` implementing the
   `soundsc` PDD interface (`Caps`, `SetConfig`, play/record transfer) on top
   of the existing `gPcmDma`/BDL engine, so `DSoundScLdd` from
   `kernelhwsrv/.../soundldd.cpp` can be compiled against QEMU instead of the
   template stub.
2. Add a user-side `RSoundSc` shim (matching `d32soundsc.h`) backed by exec 240,
   exposing Play/Record/SetConfig/Volume so apps talk the genuine ABI while
   the variant services it.
3. Stand up a minimal player workload: a WAV/MP3 PCM decoder in
   `qemu-port/user/` (start with WAV — `kerneltest/.../t_soundwav.cpp` shows
   the expected format handling) feeding `audio_pcm_play`, proving end-to-end
   media playback before any MMF-style server is attempted.
