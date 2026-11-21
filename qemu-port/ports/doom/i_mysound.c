#include "config.h"

#include <stdio.h>
#include "i_sound.h"

// SDL plays audio on a separate thread. So the game registers audio to play and
// the thread will pick it up and play it.
// Since I don't have threads I would like doom to issue an 'emit audio' every
// frame.
// On the other hand.. doom already emits audio every frame. How is this synced?
// I would assume during frame the data that doom sends is queued up and mixed
// and at the end of the frame, similar to render it's output.
// I guess it works like this:
// The sound system (kernel) writes data to the sound card every N ms.
// If there is no data it does nothing.
// Now's the question, when does the mixer mix?
// Assume I start a sound on channel 1 and another sound on channel 2.. I don't
// know how many more sounds will start playing. At some point we need to call
// it quits and flush to the kernel/device.

typedef uint16_t Uint16;
typedef uint8_t Uint8;
typedef uint32_t Uint32;
typedef int16_t Sint16;
typedef int bool;
typedef struct Mix_Chunk {
  int allocated;
  Uint8* abuf;
  Uint32 alen;
  Uint8 volume; /* Per-sample volume, 0-128 */
} Mix_Chunk;

typedef struct {
  int needed;
  Uint16 src_format;
  Uint16 dest_format;
  double rate_incr;
  Uint8* buf;
  int len;
  int len_cvt;
  int len_mult;
  double len_ratio;
  void (*filters[10])(struct SDL_AudioCVT* cvt, Uint16 format);
  int filter_index;
} SDL_AudioCVT;

typedef struct Mix_PlayingChunk {
  Mix_Chunk* chunk;
  Uint32 position;
} Mix_PlayingChunk;

#define NUM_CHANNELS 50
Mix_PlayingChunk playing[NUM_CHANNELS] = {};
unsigned int num_channels = 0;

typedef Uint32 SDL_InitFlags;

#define SDL_INIT_AUDIO 0x00000010u
#define MIX_MAX_VOLUME 128
// random
#define AUDIO_S16SYS 0x8010
// random
#define SDL_VERSIONNUM 124

int Mix_UnregisterAllEffects(int channel) {
  printf("My_UnregisterAllEffects: channel=%d\n", channel);
}

int Mix_SetPanning(int channel, Uint8 left, Uint8 right) {
  printf("My_SetPanning: channel=%d left=%d right=%d\n", channel, left, right);
}

int Mix_AllocateChannels(int numchans) {
  printf("My_AllocateChannels: numchans=%d\n", numchans);
  num_channels = numchans;
}

int Mix_PlayChannel(int channel, Mix_Chunk* chunk, int loops) {
  printf("My_PlayChannel: channel=%d loops=%d\n", channel, loops);
  // loops -1 is infinite
  //
}

int Mix_PlayChannelTimed(int channel, Mix_Chunk* chunk, int loops, int ticks) {
  printf("My_PlayChannelTimed: chan=%d loops=%d ticks=%d\n", channel, loops,
         ticks);

  playing[channel].chunk = chunk;
  playing[channel].position = 0;
  // loops -1 is infinite
  // ticks -1 means no maximum number of ticks. mix until done.
}

int Mix_HaltChannel(int channel) {
  printf("My_HaltChannel: channel=%d\n", channel);
  playing[channel].chunk = NULL;
  playing[channel].position = 0;
}

int Mix_Playing(int channel) {
  printf("My_Playing: channel=%d\n", channel);
  if (playing[channel].chunk == NULL) {
    return 0;
  }
  return 1;
}

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize) {
  printf("My_OpenAudio: freq=%d format=%d chans=%d chunksize=%d\n", frequency,
         format, channels, chunksize);
}

void Mix_CloseAudio(void) {
  printf("My_CloseAudio\n");
}

const char* Mix_GetError(void) {
  printf("My_GetError\n");
}

int Mix_QuerySpec(int* frequency, Uint16* format, int* channels) {
  printf("My_QuerySpec:\n");
  *frequency = 44100;
  *channels = 2;
  *format = AUDIO_S16SYS;
}

void SDL_QuitSubSystem(SDL_InitFlags flags) {
  printf("My_QuitSubSystem: flags=%d\n", flags);
}

bool SDL_Init(SDL_InitFlags flags) {
  printf("My_Init: flags=%d\n", flags);
}

void SDL_PauseAudio(int pause_on) {
  printf("My_PauseAudio: pause_on=%d\n", pause_on);
}

typedef struct {
  char current_index;
  char last_index;
} __attribute__((packed)) ac97_status;

typedef int16_t* sound_buffers[8192];

extern void snd_flush(int last_index);

void Mix_Flush() {
  // Perfect. We are hooked in. What do we need to do?
  // We need to mix ahead some buffers.
  // How long is a buffer? Somehow depends on the frequency.
  // - get pointer to descriptors
  // - map buffers to user space too.
  // - mix some buffers
  // - add syscall for 'set last valid buffer'
  // - call syscall

  // it would be nice if we woudln't need to call the syscall.. but for that the
  // sound stuff would need to be memory mapped.. i think there are some
  // permission bits to let user apps write to pci io ports. I want to try that.
  // ref: vol1 ch. 20
  // I would need to set the right bit in the IO map to 0. They are all zero by
  // default because I don't write the IO map. I also didn't set the IO map base
  // in the tss. I also noticed I only have one TSS. Probably every task needs
  // one.

  // for now I create a syscall because it's straight forward.

  // When do I mix? Every time it's called let's say I mix one buffer for
  // simplicity. No that might be too fast since I am called 100s of times per
  // second. I check if last - current is < 3 then I mix a buffer.

  // How do I mix? Doom issues 'PlayChannelTimed' and I register that this chunk
  // should play from now until t. chunk has buffer, length and volume. I need
  // to save how much of a chunk I already played. Every time I mix I iterate
  // over all scheduled chunks and add/multiply the waves together for however
  // long I mix in a loop. This is what I am writing to the buffer.

  // I was thinking. If 44100hz means 44100 samples per second so 441 samples
  // per ms. Let's say we make one buffer 441 samples long then each buffer is a
  // ms. Easy to think about. 60 fps is 16.6ms per frame. I don't know how much
  // fps we actually have but let's say we write 16 buffers. That would be quite
  // laggy. I guess this is why the sound should be on a separate thread. For
  // now it's ok.

  // 1ms is likely too little because it will cause an interrupt every ms. Let's
  // go with 10ms. 4410 samples. It seems the doom code wants 1024 for low
  // latency. That's around 2ms. I will ignore it. We probably need 4096 for
  // page alignment. Otherwise we waste memory?

  // case 1:
  // current = 5 and last = 10
  // no wrap, no fill, still 5 buffers available
  //
  // case 2:
  // current = 5 and last = 6
  // no wrap, fill because last < current+2
  //
  // case 3:
  // current = 30, last = 31, need to fill, last wraps around
  //
  // case 4:
  // current = 31, last = 1, should fill but condition fails

  // printf("Mix_Flush:\n");
  ac97_status* s = (ac97_status*)0x200000;

  int last_index = s->last_index;
  int target_index = (s->current_index + 4) % 32;
  // This might become negative but gives us the distance. Then we normalize.
  int num_buffers = (target_index - last_index + 32) % 32;

  // printf("user-sound: current_index=%d last_index=%d to_fill=%d\n",
  //        s->current_index, s->last_index, num_buffers);
  // printf("user-sound: chunk=%x", playing[0].chunk);

  int16_t (*bufs)[8192] = (int16_t (*)[8192])0x400000;

  for (int i = 0; i < num_buffers; i++) {
    last_index = (last_index + 1) % 32;

    // for (unsigned int j = 0; j < 8192; j++) {
    //   bufs[last_index][j] = (int16_t)(j * 100);  // A simple sawtooth wave
    // }
    // for (unsigned int j = 0; j < 8192; j += 2) {
    //   int16_t sample = ((j / 100) % 2) ? 5000 : -5000;
    //   bufs[last_index][j] = sample;
    //   bufs[last_index][j + 1] = sample;
    // }
    for (unsigned int j = 0; j < 8192; j += 2) {
      bufs[last_index][j] = 0;
      bufs[last_index][j + 1] = 0;
      int signal_left = 0;
      int signal_right = 0;
      for (unsigned int k = 0; k < num_channels; k++) {
        if (playing[k].chunk == NULL) {
          continue;
        }
        int16_t* samples = (int16_t*)playing[k].chunk->abuf;
        int total_frames = playing[k].chunk->alen / 4;

        if (playing[k].position >= total_frames) {
          playing[k].chunk = NULL;
          continue;
        }

        int frame = playing[k].position;
        int volume = playing[k].chunk->volume;

        int16_t left = samples[frame * 2];
        int16_t right = samples[frame * 2 + 1];

        signal_left += (left * volume) / 128;
        signal_right += (right * volume) / 128;
        playing[k].position++;
      }
      if (signal_left < -32767) {
        signal_left = -32767;
      } else if (signal_left > 32767) {
        signal_left = 32767;
      }
      if (signal_right < -32767) {
        signal_right = -32767;
      } else if (signal_right > 32767) {
        signal_right = 32767;
      }
      bufs[last_index][j] = signal_left;  //(j % 100 < 50) ? 4000 : -4000;
      bufs[last_index][j + 1] = signal_right;
    }
  }

  // update last_index if changed.
  if (last_index != s->last_index) {
    snd_flush(last_index);
  }
}
