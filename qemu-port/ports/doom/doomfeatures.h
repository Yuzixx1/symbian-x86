//
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
//     List of features which can be enabled/disabled to slim down the
//     program.
//

#ifndef DOOM_FEATURES_H
#define DOOM_FEATURES_H

// Enables wad merging (the '-merge' command line parameter)

#undef FEATURE_WAD_MERGE

// Enables dehacked support ('-deh')

#undef FEATURE_DEHACKED

// Enables multiplayer support (network games)

#undef FEATURE_MULTIPLAYER

// Enables sound output.  The E32 port supplies DG_sound_module /
// DG_music_module in i_e32sound.c (AC97 PCM exec 240).
#define FEATURE_SOUND 1

// EKA2 port-specific: crisp stop on movement key release.
//
// Vanilla DOOM coasts to a halt after a movement key is released: P_Thrust()
// adds planar momentum (momx/momy), which P_XYMovement() decays by FRICTION
// (0xe800 ~= 0.906 per tic) until it drops below STOPSPEED.  The walking view
// bob (P_CalcHeight) also lives off that momentum, so on the EKA2 port's slow
// guest clock the whole glide reads as "the key stayed pressed for ~a second".
// With this option, when the player supplies no planar input for a tic the
// leftover momx/momy are zeroed immediately, so motion and view bob stop on
// the very first tic after key-up.  Undefine (or set to 0) for authentic
// vanilla momentum; no other DOOM behavior is affected.
#define EKA2_CRISP_STOP 1

#endif /* #ifndef DOOM_FEATURES_H */


