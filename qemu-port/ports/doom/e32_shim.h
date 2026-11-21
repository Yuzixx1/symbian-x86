/* e32_shim.h -- freestanding libc shims + serial marker API for DOOM.E32.
 *
 * malloc/realloc/free/calloc are supplied by user/malloc.c (the genuine
 * chunk-backed E32 user heap); everything else here is implemented in
 * e32_shim.c.  No hosted libc, no libm, no FPU-dependent math.
 */
#ifndef E32_DOOM_SHIM_H
#define E32_DOOM_SHIM_H

#include <stdarg.h>

/* Emit a serial marker.  Genuine PE2 import of EUSER.DLL ordinal 915 is used
 * when the IAT slot was bound; DebugPrint (exec 86) is the fallback. */
void doom_marker(const char *text);
void doom_marker_n(const char *prefix, unsigned value);
void doom_marker_2(const char *prefix, unsigned a, unsigned b);

/* Formatter shared with the platform layer (small, no float support). */
int e32_vformat(char *buf, int cap, const char *fmt, va_list ap);

/* Called once before doomgeneric_Create (resets errno, IAT sanity). */
void doom_shim_init(void);

#endif /* E32_DOOM_SHIM_H */
