/*
 * music.c -- MUSIC SCALES tool for the Sybian REAL port (PE2 E32, "music").
 *
 * An end-user educational app that prints and optionally plays major scales.
 * It carries a built-in equal-temperament table for the chromatic notes
 * C4..B4 (A4 = 440 Hz, frequencies rounded to whole Hz) and derives a major
 * scale from any root by the 0/2/4/5/7/9/11 semitone step pattern.
 *
 *   exec 240 (QE_EXEC_AUDIO_TONE): a1 = frequency (Hz), a2 = duration (ms)
 *       -> a Symbian error code (0 = the tone was DMA'd to the codec).
 *       Host side: variant/qemu_audio_exec.cpp (slot 240).  A negative return
 *       means the exec/codec is unavailable; the tool degrades gracefully
 *       (it never faults the guest).
 *
 * Commands are read from PortCon through read_line():
 *   scale <root>    print "<root> major: <note names>" then "<note>=<freq>Hz"
 *   play <root>     play the seven scale notes for 150 ms each via exec 240
 *   selftest        print the C-major scale and try to play it
 *   help            list commands
 *   quit            exit
 *
 * Roots may be natural (C, D, ...), sharp (C#, F#, ...) or flat (Db, Eb, ...)
 * and are case-insensitive.  All serial markers go through a genuine PE2
 * import of EUSER.DLL ordinal 915 (RDebug::Printf), bound by E32Run into the
 * IAT slot at codeBase+0x8000; the port's DebugPrint executive is the fallback
 * so the trace is always visible.
 */
#include "runtime.h"

/* PE2 IAT slot, defined with --defsym in user.mk (like morse.c/soundplr.c). */
extern void (*music_printf_slot)(const char*);
extern int exec2(unsigned number, unsigned arg1, unsigned arg2);

#define QE_EXEC_AUDIO_TONE 240u
#define MUSIC_NOTE_MS      150u

/* Markers are plain strings (no % specifiers) so the variadic EUSER printf
 * receives exactly one argument.  RDebug::Printf supplies the newline. */
static void marker(const char* text)
{
    unsigned fn = *(unsigned*)(void*)&music_printf_slot;
    if (fn != 915u && fn != 0u)
        ((void (*)(const char*))fn)(text);   /* EUSER.DLL ordinal 915 */
    print(text);                              /* DebugPrint fallback */
}

/* ------------------------------------------------------------ text helpers
 * Freestanding: build each marker into a buffer, no hosted libc. */
static unsigned copy_text(char* dst, unsigned n, const char* src)
{
    while (*src) dst[n++] = *src++;
    return n;
}

static unsigned append_int(char* dst, unsigned n, int value)
{
    unsigned magnitude = (unsigned)value;
    if (value < 0) { dst[n++] = '-'; magnitude = 0u - magnitude; }
    char digits[12];
    int d = 0;
    if (!magnitude) digits[d++] = '0';
    while (magnitude)
        {
        digits[d++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
        }
    while (d) dst[n++] = digits[--d];
    return n;
}

/* ------------------------------------------------------------ note table
 * One octave of equal temperament, A4 = 440 Hz, rounded to whole Hz.  The
 * major scale is the same table indexed by root + [0,2,4,5,7,9,11] mod 12. */
struct music_note { const char* name; unsigned freq; };
static const struct music_note kNotes[12] = {
    { "C",  262u }, { "C#", 277u }, { "D",  294u }, { "D#", 311u },
    { "E",  330u }, { "F",  349u }, { "F#", 370u }, { "G",  392u },
    { "G#", 415u }, { "A",  440u }, { "A#", 466u }, { "B",  494u }
};
static const unsigned kMajorSteps[7] = { 0u, 2u, 4u, 5u, 7u, 9u, 11u };
#define NOTE_N  ((unsigned)(sizeof(kNotes) / sizeof(kNotes[0])))
#define SCALE_N ((unsigned)(sizeof(kMajorSteps) / sizeof(kMajorSteps[0])))

/* Resolve a root name to a chromatic index (0=C ... 11=B), or -1 if unknown.
 * "C", "c#", "DB", "bb" are all accepted. */
static int root_index(const char* text)
{
    char a, b;
    unsigned i;
    while (*text == ' ') ++text;
    a = *text++;
    b = *text++;
    if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
    if (b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
    for (i = 0; i < NOTE_N; ++i)
        if (kNotes[i].name[0] == a && kNotes[i].name[1] == b)
            return (int)i;                    /* natural or sharp */
    if (b == 'B')                             /* flat spellings */
        {
        if (a == 'D') return 1;               /* Db = C# */
        if (a == 'E') return 3;               /* Eb = D# */
        if (a == 'G') return 6;               /* Gb = F# */
        if (a == 'A') return 8;               /* Ab = G# */
        if (a == 'B') return 10;              /* Bb = A# */
        }
    return -1;
}

/* ---------------------------------------------------------------- commands */
/* Print a scale: one "<root> major: C D E F G A B" marker followed by seven
 * "<note>=<freq>Hz" markers. */
static void scale_report(unsigned root)
{
    char line[128];
    unsigned n = 0, i;
    n = copy_text(line, n, "MU: ");
    n = copy_text(line, n, kNotes[root].name);
    n = copy_text(line, n, " major: ");
    for (i = 0; i < SCALE_N; ++i)
        {
        unsigned idx = (root + kMajorSteps[i]) % NOTE_N;
        if (i) n = copy_text(line, n, " ");
        n = copy_text(line, n, kNotes[idx].name);
        }
    line[n] = 0;
    marker(line);

    for (i = 0; i < SCALE_N; ++i)
        {
        unsigned idx = (root + kMajorSteps[i]) % NOTE_N;
        char freq[64];
        unsigned k = 0;
        k = copy_text(freq, k, "MU: ");
        k = copy_text(freq, k, kNotes[idx].name);
        k = copy_text(freq, k, "=");
        k = append_int(freq, k, (int)kNotes[idx].freq);
        k = copy_text(freq, k, "Hz");
        freq[k] = 0;
        marker(freq);
        }
}

/* Play a scale via exec 240.  Returns 1 if the audio exec reported an error,
 * in which case a single fallback marker is emitted and playback stops. */
static int play_scale(unsigned root)
{
    char line[64];
    unsigned n = 0, i;
    n = copy_text(line, n, "MU: playing ");
    n = append_int(line, n, (int)SCALE_N);
    n = copy_text(line, n, " notes");
    line[n] = 0;
    marker(line);

    for (i = 0; i < SCALE_N; ++i)
        {
        unsigned idx = (root + kMajorSteps[i]) % NOTE_N;
        int r = exec2(QE_EXEC_AUDIO_TONE, kNotes[idx].freq, MUSIC_NOTE_MS);
        if (r < 0)
            {
            marker("MU: audio exec unavailable");
            return 1;                          /* graceful fallback */
            }
        }
    return 0;
}

static void selftest(void)
{
    marker("MU: SELFTEST");
    scale_report(0u);                          /* C major: names + frequencies */
    play_scale(0u);                            /* graceful if exec missing */
    marker("MU: SELFTEST OK");
}

/* Case-insensitive compare of a NUL-terminated command word to a literal. */
static int word_is(const char* word, unsigned length, const char* literal)
{
    unsigned i = 0;
    while (i < length && literal[i])
        {
        char a = word[i], b = literal[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (a != b) return 0;
        ++i;
        }
    return (i == length && literal[i] == 0) ? 1 : 0;
}

/* Returns 1 when the user asked to quit. */
static int run_command(const char* input)
{
    const char* word;
    const char* arg;
    unsigned length;
    int root;

    while (*input == ' ') ++input;
    word = input;
    while (*word && *word != ' ') ++word;
    length = (unsigned)(word - input);
    arg = word;
    while (*arg == ' ') ++arg;

    if      (word_is(input, length, "scale"))
        {
        root = root_index(arg);
        if (root < 0) marker("MU: scale: unknown root");
        else scale_report((unsigned)root);
        }
    else if (word_is(input, length, "play"))
        {
        root = root_index(arg);
        if (root < 0) marker("MU: play: unknown root");
        else (void)play_scale((unsigned)root);
        }
    else if (word_is(input, length, "selftest")) selftest();
    else if (word_is(input, length, "help") ||
             word_is(input, length, "?"))      marker("MU: commands: scale <root>  play <root>  selftest  help  quit");
    else if (word_is(input, length, "quit") ||
             word_is(input, length, "q"))      return 1;
    else marker("MU: unknown command");
    return 0;
}

static char line[128];

int app_main(void)
{
    if (con_acquire() < 0) { marker("MU: console busy"); return 1; }

    marker("MU: MUSIC SCALES");

    for (;;)
        {
        print("MU> ");
        if (read_line(line, sizeof(line), 60000) < 0) break;
        if (run_command(line)) break;               /* quit */
        }

    marker("MU: DONE");
    con_release();
    return 0;
}
