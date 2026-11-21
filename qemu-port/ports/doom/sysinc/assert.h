/* Freestanding <assert.h>: assertions are compiled out in the port build.
 * Defining NDEBUG is not required; the engine never relies on assert side
 * effects (its I_Error path is the real diagnostic). */
#ifndef E32_DOOM_ASSERT_H
#define E32_DOOM_ASSERT_H

#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((void)0)
#endif

#endif /* E32_DOOM_ASSERT_H */
