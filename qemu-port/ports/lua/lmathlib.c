/*
** lmathlib.c -- reduced freestanding math library for Lua 5.4.6 on E32.
**
** This is the genuine Lua C API; only the platform-dependent implementation
** changed.  Functions that need a hosted libm or libc (randomseed from time,
** atan2/asin/acos, powers with negative bases) are either mapped to the x87
** shims in e32_shim.c or omitted.  The Lua language and VM are untouched.
*/
#define lmathlib_c
#define LUA_LIB

#include "lprefix.h"

#include <math.h>
#include <stddef.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "runtime.h"   /* random32(), time_now() */

#define PI 3.14159265358979323846

static int math_abs (lua_State *L) {
  if (lua_isinteger(L, 1)) {
    lua_Integer n = lua_tointeger(L, 1);
    lua_pushinteger(L, (n < 0) ? (lua_Integer)(-(lua_Unsigned)n) : n);
  }
  else lua_pushnumber(L, fabs(luaL_checknumber(L, 1)));
  return 1;
}

static int math_floor (lua_State *L) {
  if (lua_isinteger(L, 1)) lua_pushvalue(L, 1);
  else lua_pushinteger(L, (lua_Integer)floor(luaL_checknumber(L, 1)));
  return 1;
}

static int math_ceil (lua_State *L) {
  if (lua_isinteger(L, 1)) lua_pushvalue(L, 1);
  else lua_pushinteger(L, (lua_Integer)ceil(luaL_checknumber(L, 1)));
  return 1;
}

static int math_sqrt (lua_State *L) {
  lua_pushnumber(L, sqrt(luaL_checknumber(L, 1)));
  return 1;
}

static int math_fmod (lua_State *L) {
  lua_pushnumber(L, fmod(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
  return 1;
}

static int math_modf (lua_State *L) {
  lua_Number n = luaL_checknumber(L, 1);
  lua_Number ip = (n < 0) ? ceil(n) : floor(n);
  lua_pushnumber(L, ip);
  lua_pushnumber(L, n - ip);
  return 2;
}

static int math_max (lua_State *L) {
  int n = lua_gettop(L), i;
  luaL_argcheck(L, n > 0, 1, "value expected");
  for (i = 2; i <= n; ++i)
    if (lua_compare(L, i, 1, LUA_OPLT)) lua_pushvalue(L, i), lua_replace(L, 1);
  lua_pushvalue(L, 1);
  return 1;
}

static int math_min (lua_State *L) {
  int n = lua_gettop(L), i;
  luaL_argcheck(L, n > 0, 1, "value expected");
  for (i = 2; i <= n; ++i)
    if (lua_compare(L, 1, i, LUA_OPLT)) lua_pushvalue(L, i), lua_replace(L, 1);
  lua_pushvalue(L, 1);
  return 1;
}

static unsigned int rng_state = 0x9e3779b9u;

static int math_randomseed (lua_State *L) {
  lua_Integer x = luaL_checkinteger(L, 1);
  rng_state = (unsigned int)x;
  if (rng_state == 0) rng_state = 0x9e3779b9u;
  return 0;
}

static lua_Integer next_random (void) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return (lua_Integer)(rng_state & 0x7fffffffu);
}

static int math_random (lua_State *L) {
  lua_Integer r = next_random();
  switch (lua_gettop(L)) {
    case 0:
      lua_pushnumber(L, (lua_Number)r / 2147483648.0);
      break;
    case 1: {
      lua_Integer m = luaL_checkinteger(L, 1);
      luaL_argcheck(L, m > 0, 1, "interval is empty");
      lua_pushinteger(L, r % m + 1);
      break;
    }
    default: {
      lua_Integer lo = luaL_checkinteger(L, 1);
      lua_Integer hi = luaL_checkinteger(L, 2);
      luaL_argcheck(L, lo <= hi, 2, "interval is empty");
      lua_pushinteger(L, lo + r % (hi - lo + 1));
      break;
    }
  }
  return 1;
}

static int math_type (lua_State *L) {
  luaL_checkany(L, 1);
  if (lua_type(L, 1) != LUA_TNUMBER) lua_pushliteral(L, "nil");
  else lua_pushstring(L, lua_isinteger(L, 1) ? "integer" : "float");
  return 1;
}

static int math_tointeger (lua_State *L) {
  int isnum;
  lua_Integer n = lua_tointegerx(L, 1, &isnum);
  if (isnum) lua_pushinteger(L, n);
  else lua_pushnil(L);
  return 1;
}

static int math_sin (lua_State *L) { lua_pushnumber(L, sin(luaL_checknumber(L,1))); return 1; }
static int math_cos (lua_State *L) { lua_pushnumber(L, cos(luaL_checknumber(L,1))); return 1; }
static int math_tan (lua_State *L) { lua_pushnumber(L, tan(luaL_checknumber(L,1))); return 1; }
static int math_exp (lua_State *L) { lua_pushnumber(L, exp(luaL_checknumber(L,1))); return 1; }
static int math_log (lua_State *L) { lua_pushnumber(L, log(luaL_checknumber(L,1))); return 1; }
static int math_log10 (lua_State *L) { lua_pushnumber(L, log10(luaL_checknumber(L,1))); return 1; }

static const luaL_Reg mathlib[] = {
  {"abs",     math_abs},
  {"ceil",    math_ceil},
  {"floor",   math_floor},
  {"fmod",    math_fmod},
  {"modf",    math_modf},
  {"sqrt",    math_sqrt},
  {"sin",     math_sin},
  {"cos",     math_cos},
  {"tan",     math_tan},
  {"exp",     math_exp},
  {"log",     math_log},
  {"log10",   math_log10},
  {"max",     math_max},
  {"min",     math_min},
  {"random",  math_random},
  {"randomseed", math_randomseed},
  {"tointeger", math_tointeger},
  {"type",    math_type},
  {NULL, NULL}
};

LUAMOD_API int luaopen_math (lua_State *L) {
  luaL_newlib(L, mathlib);
  lua_pushnumber(L, PI);
  lua_setfield(L, -2, "pi");
  lua_pushnumber(L, HUGE_VAL);
  lua_setfield(L, -2, "huge");
  lua_pushinteger(L, LUA_MAXINTEGER);
  lua_setfield(L, -2, "maxinteger");
  lua_pushinteger(L, LUA_MININTEGER);
  lua_setfield(L, -2, "mininteger");
  return 1;
}
