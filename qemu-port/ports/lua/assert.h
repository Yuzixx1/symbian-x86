/* Freestanding <assert.h>: Lua's internal assertions are disabled by default
 * (LUAI_ASSERT is not defined), so this is only a source-level placeholder. */
#ifndef QPORT_LUA_ASSERT_H
#define QPORT_LUA_ASSERT_H
#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((void)0)
#endif
#endif
