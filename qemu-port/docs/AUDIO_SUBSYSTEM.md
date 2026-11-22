# The EKA2 audio subsystem (AC97 PCM-OUT)

This document describes the audio stack of the QEMU port: the emulated AC'97
controller, the kernel driver in `variant/qemu_audio.cpp`, the ring-3 exec
bridge in `variant/qemu_audio_exec.cpp`, and how the DOOM port consumes it.
Everything here is **Variant-owned** — `kernelhwsrv/` is deliberately left
untouched. The older, narrower description lives in
[`GUI_AUDIO.md`](GUI_AUDIO.md) §6; this page is the full treatment including the
new PCM playback path added for DOOM.

Audio exists on the **full Kern image only** (`build/kernfull.elf`). The
minimal console images do not link `qemu_audio.cpp` or the exec patch, so the
`audio` shell verb and the audio execs are absent there.

## 1. Hardware: what QEMU actually emulates

QEMU's `-device AC97` is an Intel 82801AA-style AC'97 controller. The driver
finds it by PCI class, not by vendor/product ID:

* PCI class **04.01** (subclass 0x04 "audio device", prog-if 0x01) via
  `PciFindClass(0x04, 0x01, ...)`. `audio_init()` remembers the bus/dev/fn.
* Codec vendor ID registers at NAM offsets `0x7C`/`0x7E`. QEMU reports
  **Sigmatel STAC9700, `8384:7600`** — this is the value the boot banner prints
  and the value the tests assert. A real codec sets the ready bit (bit 15) in
  the reset register; QEMU leaves that bit clear but always returns a valid
  vendor ID once its `mixer_reset()` has run, so the driver polls until either
  the ready bit appears or a plausible vendor ID is seen.

Two PCI BARs describe the two halves of the controller:

| BAR | Name | Function | QEMU type |
|-----|------|----------|-----------|
| BAR0 (`0x10`) | NAM — Native Audio Mixer | codec control / volume / rate | I/O space (bit 0 set) |
| BAR1 (`0x14`) | NAB — Native Audio Bus Master | PCM-OUT DMA engine | I/O space (bit 0 set) |

The register helpers in `qemu_audio.cpp` select `in/out` versus MMIO from each
BAR's type bit, so the same code would drive a memory-mapped AC97 too. The
driver enables I/O + MEM + bus-mastering in the PCI command register
(`0x04`) before touching either BAR.

### NAM registers used

`kNamReset = 0x00` (write 0 to reset), `kNamMasterVolume = 0x02`,
`kNamPcmOutVolume = 0x18` (mute is bit 15, so 0 unmutes),
`kNamExtAudioCtrl = 0x2A`, `kNamFrontDacRate = 0x2C` (set to 48000), plus the
vendor-ID pair. All are **16-bit** accesses.

### NAB / PCM-OUT DMA registers used

PCM-OUT is bus-master index 1, so its registers begin at BAR1 + `0x10`:

| Offset | Name | Meaning |
|--------|------|---------|
| `0x10` | `PO_BDBAR` | physical base of the buffer-descriptor list (BDL) |
| `0x14` | `PO_CIV` | current index value (RO) |
| `0x15` | `PO_LVI` | last valid index (write to arm the next descriptor) |
| `0x16` | `PO_SR` | status; DCH (bit 0) = DMA halted |
| `0x18` | `PO_PICB` | position in current buffer, in 16-bit words (RO) |
| `0x1B` | `PO_CR` | control; RPBM (bit 0) = run, RR (bit 1) = reset |
| `0x2C` | `GLOB_CNT` | global control (cold/warm reset requests) |
| `0x30` | `GLOB_STA` | global status |

A **buffer descriptor** is two aligned dwords: `{ addr, ctl_len }`. `ctl_len`
carries the buffer length and the IOC (interrupt-on-completion, bit 31) and
BUP (buffer-underrun, bit 30) flags. The driver uses a 2-entry BDL but normally
arms only descriptor 0 (`LVI = 0`). `BDBAR` must be written *before* the `CR`
run write, because QEMU fetches descriptor 0 at the moment RPBM is set.

## 2. Buffer format, rate and channels

The driver's own tone/PCM buffers are a fixed **stereo, signed 16-bit, 48 kHz**
format: `kWordsPerFrame = 2` (left + right), `kRateHz = 48000`. Samples are
stored as `TUint16` words in a buffer whose virtual address equals its physical
address — the kernel runs with a flat identity map, so the DMA engine can read
the static arrays directly. BDL entries are 256-byte aligned.

A single descriptor's `ctl_len` length field is 16 bits, so a buffer can hold at
most 65535 words (~131 KiB, ~0.68 s at 48 kHz stereo). Long notes are clamped
to `kMaxToneFrames = 24000` (500 ms) rather than overflowing the descriptor.
DOOM's PCM path works around this with a **ring of buffers** (see §5) instead
of one oversized descriptor.

## 3. Kernel API

All entry points live in `variant/qemu_audio.cpp` and are declared in
`variant/qemu_variant.h`:

* `audio_init()` — idempotent. Enumerates the controller, enables bus master,
  resets PCM-OUT and the codec, sets 48 kHz / unmutes, then prints the banner:
  `audio: AC97 <bus>:<dev>.<fn> BAR0=...(IO) BAR1=...(IO)` and
  `audio: vendor ID 8384:7600 codec ready`. A boot constructor
  (`AudioBootInit`) calls it after paging and `TheAsic` are ready.
* `audio_info()` — shell verb `audio`; reports BARs, vendor ID, reset status,
  `GLOB_STA`, `PO_CR`, `PO_SR`, `PO_CIV` and `PO_PICB`.
* `audio_play_test()` — shell verb `audio play`. Duplicates the 48-entry
  integer 1 kHz sine table to both channels for 0.1 s (4800 frames, 19200
  bytes), arms one descriptor and polls to completion. Prints
  `audio play: PCM play done (19200 bytes, ...)`.
* `audio_tone(freq, ms)` — the general tone generator (20 Hz–20 kHz, 1–500 ms).
  It phase-accumulates the same sine table in **16.16 fixed point** (no FPU or
  libm in the kernel): `step = (freq << 16) / 1000`, wrapping at
  `48 << 16`. This keeps non-1 kHz notes in tune without floating point.
  Prints `audio tone: ...` then `audio tone: done ...`.

Both playback routines poll for completion: they spin until `PO_SR.DCH` is set
and `PO_PICB == 0`, then print a result line (or `... timeout ...`). There is
no audio IRQ and no callback; the driver is **synchronous/polled**, exactly
like `qemu_ata`, `qemu_ahci` and `qemu_nic`.

## 4. Ring-3 bridge: the tone exec (240) and the new PCM ops

`variant/qemu_audio_exec.cpp` exposes the kernel generator to user E32 programs
through the slow-exec table. It patches a genuine `EpocSlowExecTable` entry at
boot, mirroring the netbridge (247/248), GUI (249/250/251) and SvcMgr
(268/269/270) patterns, with `kernelhwsrv/` untouched.

* **Exec 240, `QE_EXEC_AUDIO_TONE`** — `TInt handler(TInt aFreq, TInt aMs)`,
  a thin wrapper over `audio_tone()`. Slot 240 is used because it is the only
  still-invalid entry left after the other agents claimed 247–251; stealing a
  claimed slot (e.g. 251) would break the GUI agent. The installer checks the
  table count, verifies the slot still points at `InvalidExecHandler`, sets the
  flags word to 0 and writes the handler address. Success prints
  `audio exec: tone installed at slot 240`. Callers of exec 240:
  `user/soundplr.c` (C4–E4–G4–C5), `user/morse.c` (600 Hz dot/dash) and
  `user/music.c` (equal-temperament C4–B4 scale).

* **New PCM ops (this wave).** Tones are fine for demos but DOOM needs to
  stream arbitrary sample buffers. The new path adds PCM enqueue/flush execs
  that let a ring-3 program hand the kernel a set of filled buffers plus a
  "last valid index" to arm, mirroring `i_mysound.c`'s `snd_flush()` hook.
  The user side owns a 32-entry ring of 16-bit stereo buffers and publishes a
  small status struct (current/last index); the kernel arms the ring's BDL and
  drives PCM-OUT. The serial markers from this path use the `AU:` prefix
  (e.g. `AU: pcm play ...`) so they are distinguishable from the older
  `audio:` / `audio play:` lines; `SND:` markers come from the DOOM side.

Because exec numbers are a scarce shared resource, the audio exec installer is
deliberately conservative: it only takes a slot it can prove is invalid and
prints a diagnostic instead of overwriting another worker's handler.

## 5. How DOOM produces sound

`ports/doom/i_mysound.c` is a freestanding SDL_mixer shim. DOOM's `s_sound.c`
calls `Mix_PlayChannelTimed()` for every SFX and `I_StartMusic()` /
`I_UpdateMusic()` for music; the shim records active chunks in a small channel
array (`NUM_CHANNELS = 50` in the current sketch). There is **no audio thread**
on this OS, so mixing is done lazily from `Mix_Flush()`:

1. A `ac97_status` struct at a fixed low address (`0x200000`) exposes the
   ring's `current_index` and `last_index`.
2. A ring of 32 buffers of 8192 signed 16-bit words each lives at `0x400000`;
   each buffer is one ~10 ms chunk at 44.1 kHz stereo.
3. On each flush the shim computes how many buffers are free ahead of the
   codec's current index, sums every active channel into them with per-channel
   volume (`(sample * volume) / 128`), clamps to `[-32767, 32767]`, and calls
   `snd_flush(last_index)` to hand the new tail of the ring to the kernel.

Music arrives as a MIDI-style sequence: DOOM's `mus2mid`/`I_UpdateMusic`
produces note events that must be rendered into PCM by a tiny software
synth before they can enter the same ring. On this port that synthesiser is
intentionally simple (a few oscillators/envelopes driving 16-bit stereo), so
music is qualitatively much poorer than a general MIDI/Adlib emulation — it is
a proof that the data path works, not a faithful rendition.

## 6. Synchronous vs asynchronous — why `-audiodev none` still works

QEMU's audio backends are configured with `-audiodev`. This test uses:

```
-audiodev none,id=a0 -device AC97,audiodev=a0
```

`none` is QEMU's null audio backend: it **does not open a host speaker or
PulseAudio/ALSA sink**, so the harness is headless and CI-safe. Crucially, it
does *not* disable the emulated device: PCI enumeration, mixer reset, vendor
IDs, register reads/writes and bus-master DMA all still happen exactly as with
a real backend. The guest believes it is driving real hardware; only the final
digital-to-analogue conversion is discarded. The kernel driver is synchronous
(polled completion), so no host-side timing or audio-thread scheduling is
needed for a deterministic test — the log markers are the source of truth.

Use a real backend (`-audiodev pa,id=a0` / `alsa` / `sdl`) only for manual,
audible verification on a desktop; never in automated tests.

## 7. Memory budget

| Item | Size | Notes |
|------|------|-------|
| `gPcm` (`audio_play_test`) | 4800 frames × 2 words = 9600 words = 19200 B | 0.1 s tone |
| `gTonePcm` (`audio_tone`) | 24000 frames × 2 words = 48000 words = 96000 B | 500 ms max note |
| `gBdl` | 2 × 8 B = 16 B, 256-byte aligned | descriptor list |
| DOOM ring | 32 × 8192 words = 262144 words = 524288 B | ~512 KiB at 0x400000 |
| status struct | a few bytes at 0x200000 | current/last index |

Total static audio RAM is comfortably under 1 MiB (dominated by DOOM's 512 KiB
ring). All buffers must stay in identity-mapped low RAM; no allocation from
the kernel heap is required after boot.

## 8. Current limitations

* **No real mixer in the kernel.** Mixing/volume is a user-space loop in
  `i_mysound.c`; the kernel only plays the buffers it is handed.
* **No simultaneous kernel streams.** There is one PCM-OUT engine, one BDL and
  one polled transfer at a time. The GUI tone execs and DOOM's PCM path cannot
  play concurrently; a second `audio_tone()` while a PCM ring is active will
  re-arm the same descriptor.
* **Polled, not interrupt-driven.** Playback burns CPU in a spin loop and the
  caller blocks until the buffer drains. Long buffers therefore stall the
  caller.
* **Music quality is poor.** No ADLIB/OPL/GUS emulation, no general MIDI; the
  small software synth only approximates DOOM's music.
* **44.1 kHz (DOOM) vs 48 kHz (kernel tone).** The two paths use different
  nominal rates; the ring assumes 44.1 kHz while the DAC is programmed for
  48 kHz, so pitch/tempo may drift slightly. A single canonical rate is a
  future cleanup.
* **Headless verification cannot be audible.** With `-audiodev none` the tests
  can only prove register programming, DMA completion and marker emission —
  they cannot prove that a human would hear the sound. Audible checks need a
  manual run with a real backend.
* **Volume/rate changes mid-stream and capture (MIC-IN) are not implemented.**

## 9. How to test

1. **Kernel/driver only** — `python3 tools/r1_qemu_audio_test.py`. Boots
   `build/kernfull.elf` with an AC97 device, feeds `audio` and `audio play`,
   and asserts the vendor ID, codec ready, the 19200-byte PCM transfer and a
   clean log.
2. **Tone exec / user apps** — `r1_soundplr_test.py`, `r1_morse_test.py`,
   `r1_music_test.py`: stage the E32 + `EUSER.DLL`, boot the GRUB ISO with the
   AC97 flags, and assert the ord-915 + exec-240 marker families.
3. **DOOM end-to-end** — `python3 tools/r1_doom_audio_test.py`. Stages
   `DOOM.E32`, `EUSER.DLL` and `FDOOM1.WAD` on the 64 MB/48 MB FAT disk, boots
   the GRUB ISO with `-audiodev none,id=a0 -device AC97,audiodev=a0`, launches
   `doom`, presses Enter to start the game, and asserts the AC97 banner,
   `SND:` and `AU:` marker families, `DOOM: TITLE`/`FRAME` and no faults. It
   skips cleanly if any asset is missing and takes a best-effort QMP
   screendump that never gates the result. A longer gameplay-only check is
   `r1_doom_playtest.py`.

All harnesses write logs under `/tmp/opencode/` and exit 0 on a clean skip.
They must never fail because QMP/graphics were unavailable — audio correctness
is judged from the serial markers.
