/*
 * doome32.c -- launch the genuine doomgeneric engine as a PE2 E32 process.
 *
 * The engine is copied verbatim into ports/doom/; this file only supplies the
 * argv vector, locates the staged Freedoom IWAD, and drives the standard
 * doomgeneric main loop (doomgeneric_Create -> doomgeneric_Tick forever).
 * All serial diagnostics use the genuine EUSER.DLL ordinal-915 PE2 import
 * with DebugPrint fallback (see ports/doom/e32_shim.c).
 */
#include "doomgeneric.h"
#include "e32_shim.h"
#include "runtime.h"

#include <stdio.h>
#include <string.h>

extern const char *doom_iwad_path;

/* Candidate IWAD paths, ordered by in-guest ground truth from WADPROBE.E32.
 *
 * The probe (tools/r1_wadprobe_test.py) showed that the large-file API works
 * perfectly on the AHCI-backed C: volume (device 4): file_open() returns a
 * valid handle, file_size() reports 28795076 and file_read_at(0) returns the
 * IWAD magic "49 57 41 44".  On a plain ISO boot X: is the GRUB RAMDISK at
 * device 8, and EVERY open on it fails with KErrCorrupt (-20) because the
 * kernel's R1Fat StorageFailed() quarantine array only covers devices 0..7
 * (variant/fat32.cpp:21-24, :1598).  That is a kernel-side bug, not a path
 * spelling issue -- none of the nine spellings work on device 8.
 *
 * Therefore prefer C: (the AHCI disk the test harness attaches).  X: still
 * works when it is an alias of the AHCI volume (--disk-boot: no RAMDISK), and
 * is kept as a fallback so the same binary works on either boot layout.
 * Every candidate reaches the large-file API through the shim's fopen()
 * (file_open/file_size/file_read_at), so the 28 MB WAD is read in full. */
static const char *const kWadPaths[] = {
    "C:/FDOOM1.WAD",       /* primary: AHCI device 4, verified working */
    "C:\\FDOOM1.WAD",      /* literal backslash spelling (same volume) */
    "FDOOM1.WAD",          /* bare name on the active drive */
    "/FDOOM1.WAD",         /* root-relative on the active drive */
    "X:/FDOOM1.WAD",       /* X: alias of the AHCI volume (--disk-boot) */
    "X:\\FDOOM1.WAD",      /* same X: file, literal backslash spelling */
    "X:/fdoom1.wad",       /* case-insensitive lookup on the FAT volume */
    "X:/FREEDOOM1.WAD",    /* long-name aliases (8.3 normalised by runtime) */
    "FREEDO~1.WAD",        /* FREEDOOM1.WAD folds to first 6 + ~1 in 8.3 */
};

static char sWadPath[64];

static int find_iwad(void)
{
    unsigned i;
    for (i = 0; i < sizeof(kWadPaths) / sizeof(kWadPaths[0]); ++i)
        {
        FILE *f = fopen(kWadPaths[i], "rb");
        if (f)
            {
            /* ftell() on a freshly opened stream is 0; seek to the end so
             * the FILE-layer offset field reports the real (28 MB) length. */
            long size;
            fseek(f, 0, SEEK_END);
            size = ftell(f);
            fclose(f);
            if (size > 4096)
                {
                unsigned n = 0;
                while (kWadPaths[i][n] && n < sizeof(sWadPath) - 1u)
                    { sWadPath[n] = kWadPaths[i][n]; ++n; }
                sWadPath[n] = 0;
                doom_iwad_path = sWadPath;
                {
                char line[128];
                snprintf(line, sizeof(line), "DOOM: WAD %s %u",
                         sWadPath, (unsigned)size);
                doom_marker(line);
                doom_marker("DOOM: WAD OK");
                }
                return 1;
                }
            }
        }
    return 0;
}

int app_main(void)
{
    char *argv[4];
    unsigned ticks = 0;

    doom_shim_init();
    doom_marker("DOOM: start");

    if (!find_iwad())
        {
        doom_marker("DOOM: WAD missing");
        doom_marker("DOOM: DONE");
        process_exit(2);
        }

    argv[0] = (char *)"doom";
    argv[1] = (char *)"-iwad";
    argv[2] = sWadPath;
    argv[3] = 0;

    /* doomgeneric_Create saves argv, allocates DG_ScreenBuffer, calls
     * DG_Init(), then runs D_DoomMain() (which paints the first frame). */
    doomgeneric_Create(3, argv);

    /* Standard doomgeneric platform loop.  Bound the run so a serial test
     * always observes an orderly DOOM: DONE rather than a killed process. */
    for (;;)
        {
        doomgeneric_Tick();
        if (++ticks >= 420u) break;          /* ~12 s at 35 tics/s */
        }

    doom_marker("DOOM: DONE");
    process_exit(0);
    return 0;
}
