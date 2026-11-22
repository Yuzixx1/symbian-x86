//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      Timer functions.
//

#include "i_timer.h"
#include "doomtype.h"

#include "doomgeneric.h"

#include <stdarg.h>

//#include <sys/time.h>
//#include <unistd.h>


//
// I_GetTime
// returns time in 1/35th second tics
//

static uint32_t basetime = 0;


int I_GetTicks(void)
{
	return DG_GetTicksMs();
}

int  I_GetTime (void)
{
    uint32_t ticks;

    ticks = I_GetTicks();

    if (basetime == 0)
        basetime = ticks;

    ticks -= basetime;

    return (ticks * TICRATE) / 1000;
}


//
// Same as I_GetTime, but returns time in milliseconds
//

int I_GetTimeMS(void)
{
    uint32_t ticks;

    ticks = I_GetTicks();

    if (basetime == 0)
        basetime = ticks;

    return ticks - basetime;
}

// Sleep for a specified number of ms.
//
// On E32, DG_SleepMs(1) is a fresh RTimer (exec 133) + TimerAfter (exec 53)
// + ipc_wait + close (exec 106): four kernel execs for ONE millisecond.
// The old D_Doom pacing loop slept ~28 times per frame (>4,000 execs/sec),
// starving the rest of the system.  Wait in ONE timer sleep for the bulk and
// use the cheap microsecond clock (exec 68, one exec per poll) only for the
// sub-millisecond tail.  A tight spin on DG_GetTicksMs() is bounded so a
// fast-trapping kernel cannot turn the tail into hundreds of execs.

#define I_SPIN_POLLS 4

// Wait until an absolute I_GetTimeMS() value (same basetime as I_GetTime).
void I_WaitUntil(int targetMs)
{
    int left = targetMs - I_GetTimeMS();
    int polls = 0;

    if (left <= 0)
        return;

    // Bulk of the wait: one genuine timer sleep, regardless of length.
    if (left > 1)
        DG_SleepMs((uint32_t)(left - 1));

    // Fine tail: a handful of high-resolution clock polls (one exec each),
    // then let the kernel timer finish if the clock has not ticked over.
    while ((left = targetMs - I_GetTimeMS()) > 0)
    {
        if (polls++ < I_SPIN_POLLS)
            continue;
        DG_SleepMs((uint32_t)left);
        break;
    }
}

void I_Sleep(int ms)
{
    if (ms <= 0)
        return;

    I_WaitUntil(I_GetTimeMS() + ms);
}

void I_WaitVBL(int count)
{
    //I_Sleep((count * 1000) / 70);
}


void I_InitTimer(void)
{
    // initialize timer

    //SDL_Init(SDL_INIT_TIMER);
}

