/*
** linit.c -- library list for the E32 Lua port.  Only the libraries whose
** dependencies can be satisfied freestanding are registered: base, table,
** string and the reduced math library.  io/os/package/debug/coroutine/utf8
** are intentionally absent (no hosted file system, processes or dynamic
** loader in this build).
*/
#define linit_c
#define LUA_LIB

#include "lprefix.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

static const luaL_Reg loadedlibs[] = {
  {"_G", luaopen_base},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {NULL, NULL}
};

LUALIB_API void luaL_openlibs (lua_State *L) {
  const luaL_Reg *lib;
  for (lib = loadedlibs; lib->func; lib++) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);  /* remove lib */
  }
}
