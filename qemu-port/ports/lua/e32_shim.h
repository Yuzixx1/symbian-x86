/* e32_shim.h -- freestanding C runtime shims for Lua 5.4.6 on PE2/E32.
 *
 * All output from the Lua core (lua_writestring / lua_writeline) is buffered
 * here and flushed as whole lines through user/runtime.h print_n(), which is
 * backed by the genuine EUSER ordinal 915 (RDebug::Printf) bridge.  Because
 * that executive appends its own CR/LF to every call, output is emitted one
 * line per print_n() call: no half-lines and no stacked prompts.
 */
#ifndef QPORT_LUA_E32_SHIM_H
#define QPORT_LUA_E32_SHIM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Append a chunk to the line-oriented stdout buffer. */
void e32_lua_write(const char *s, size_t n);
/* Emit every complete line in the buffer via print_n(); keep any tail. */
void e32_lua_flush_lines(void);
/* Emit the whole buffer (adding a CR/LF via print_n() if a tail exists). */
void e32_lua_flush(void);
/* Discard the buffer (used on fatal errors). */
void e32_lua_reset(void);

/* The lua_Alloc callback handed to lua_newstate().  Uses the genuine
   EUSER chunk-backed user heap in user/malloc.c; realloc is emulated with
   malloc + memcpy + free. */
void *e32_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize);

#ifdef __cplusplus
}
#endif

#endif /* QPORT_LUA_E32_SHIM_H */
