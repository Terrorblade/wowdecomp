#define lbitlib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


typedef int Integer;
typedef unsigned int UInteger;

/* Numbers convert through long long, as the 12340 client's _ftol2 does: truncate to 64 bits and
   keep the low 32, so bit.band(x, 0xFFFFFFFF) and negative masks wrap instead of saturating. */


// VERIFY: d6a77d71 manual
// OFFSET: 0x00850CC0
static int bit_bnot (lua_State *L) {
  lua_pushnumber(L, (UInteger)~(Integer)(long long)luaL_checknumber(L, 1));
  return 1;
}


// VERIFY: e75ae7e6 manual
// OFFSET: 0x00850D00
static int bit_band (lua_State *L) {
  int n = lua_gettop(L), i;
  UInteger w = (Integer)(long long)luaL_checknumber(L, 1);
  for (i = 2; i <= n; i++)
    w &= (Integer)(long long)luaL_checknumber(L, i);
  lua_pushnumber(L, w);
  return 1;
}


// VERIFY: 2eb41a74 manual
// OFFSET: 0x00850D80
static int bit_bor (lua_State *L) {
  int n = lua_gettop(L), i;
  UInteger w = (Integer)(long long)luaL_checknumber(L, 1);
  for (i = 2; i <= n; i++)
    w |= (Integer)(long long)luaL_checknumber(L, i);
  lua_pushnumber(L, w);
  return 1;
}


// VERIFY: 6aea1192 manual
// OFFSET: 0x00850E00
static int bit_bxor (lua_State *L) {
  int n = lua_gettop(L), i;
  UInteger w = (Integer)(long long)luaL_checknumber(L, 1);
  for (i = 2; i <= n; i++)
    w ^= (Integer)(long long)luaL_checknumber(L, i);
  lua_pushnumber(L, w);
  return 1;
}


// VERIFY: 5453693c auto
// OFFSET: 0x00850E80
static int bit_lshift (lua_State *L) {
  lua_pushnumber(L, (UInteger)((Integer)(long long)luaL_checknumber(L, 1) << (UInteger)(long long)luaL_checknumber(L, 2)));
  return 1;
}


// VERIFY: 99a627b4 auto
// OFFSET: 0x00850EE0
static int bit_rshift (lua_State *L) {
  lua_pushnumber(L, (Integer)((UInteger)(long long)luaL_checknumber(L, 1) >> (UInteger)(long long)luaL_checknumber(L, 2)));
  return 1;
}


// VERIFY: b56ebf4a auto
// OFFSET: 0x00850F40
static int bit_arshift (lua_State *L) {
  lua_pushnumber(L, (Integer)(long long)luaL_checknumber(L, 1) >> (UInteger)(long long)luaL_checknumber(L, 2));
  return 1;
}


// VERIFY: 8e98a529 auto
// OFFSET: 0x00850F90
static int bit_mod (lua_State *L) {
  if ((Integer)(long long)luaL_checknumber(L, 2) == 0) {
    lua_pushnumber(L, 1 / lua_tonumber(L, 2));
    return 1;
  }
  lua_pushnumber(L, (Integer)(long long)luaL_checknumber(L, 1) % (Integer)(long long)luaL_checknumber(L, 2));
  return 1;
}


static const struct luaL_Reg bitlib[] = {
  {"bnot", bit_bnot},
  {"band", bit_band},
  {"bor", bit_bor},
  {"bxor", bit_bxor},
  {"lshift", bit_lshift},
  {"rshift", bit_rshift},
  {"arshift", bit_arshift},
  {"mod", bit_mod},
  {NULL, NULL}
};


// VERIFY: 23d41808 auto
// OFFSET: 0x00851010
LUALIB_API int luaopen_bit (lua_State *L) {
  luaL_openlib(L, "bit", bitlib, 0);
  return 1;
}
