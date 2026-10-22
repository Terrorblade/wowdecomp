#include <cstdint>
#include <cstring>
#include <ctime>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// STATIC: 0x00D3F770
static int64_t s_debugProfileStart;

// VERIFY: b4a5451d auto
// OFFSET: 0x008168D0
static int32_t Script_setglobal(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_settop(L, 2);
    lua_setfield(L, LUA_GLOBALSINDEX, name);

    return 0;
}

// VERIFY: 5a22310d auto
// OFFSET: 0x00816910
static int32_t Script_getglobal(lua_State* L) {
    if (lua_isstring(L, 1)) {
        lua_getfield(L, LUA_GLOBALSINDEX, lua_tostring(L, 1));
    } else {
        lua_pushnil(L);
    }

    return 1;
}

// VERIFY: b949915a manual
// OFFSET: 0x00816960
static int32_t Script_strtrim(lua_State* L) {
    const char* string = luaL_checkstring(L, 1);
    const char* chars = luaL_optstring(L, 2, " \t\r\n");
    size_t length = SStrLen(string);
    lua_settop(L, 1);

    if (!length) {
        return 1;
    }

    const char* c;

    for (c = chars; *c && *c != *string; c++) {
    }

    if (!*c) {
        for (c = chars; *c && *c != string[length - 1]; c++) {
        }

        if (!*c) {
            return 1;
        }
    }

    while (true) {
        for (c = chars; *c && *c != *string; c++) {
        }

        if (!*c) {
            break;
        }

        string++;
        length--;
    }

    while (length) {
        for (c = chars; *c && *c != string[length - 1]; c++) {
        }

        if (!*c) {
            break;
        }

        length--;
    }

    lua_pushlstring(L, string, length);

    return 1;
}

// VERIFY: 11413fb1 manual
// OFFSET: 0x00816A60
static int32_t Script_strsplit(lua_State* L) {
    const char* delimiters = luaL_checkstring(L, 1);
    const char* string = luaL_checkstring(L, 2);
    int32_t limit = luaL_optinteger(L, 3, 0);
    lua_settop(L, 0);

    int32_t count = 0;

    if (!limit || limit > 1) {
        for (const char* c = string; *c; c++) {
            const char* d;

            for (d = delimiters; *d && *d != *c; d++) {
            }

            if (!*d) {
                continue;
            }

            if (!lua_checkstack(L, count + 1)) {
                luaL_error(L, "strsplit(): Stack overflow");
            }

            lua_pushlstring(L, string, c - string);
            count++;
            string = c + 1;

            if (count == limit - 1) {
                break;
            }
        }
    }

    if (!lua_checkstack(L, count + 1)) {
        luaL_error(L, "strsplit(): Stack overflow");
    }

    lua_pushstring(L, string);

    return count + 1;
}

// VERIFY: cad68086 auto
// OFFSET: 0x00816B60
static int32_t Script_strjoin(lua_State* L) {
    size_t length;
    const char* separator = luaL_checklstring(L, 1, &length);
    int32_t top = lua_gettop(L);
    int32_t count = top - 1;

    if (!length) {
        lua_concat(L, top);
        return 1;
    }

    if (top == 1) {
        lua_pushstring(L, "");
        return 1;
    }

    if (!lua_checkstack(L, 2 * count)) {
        luaL_error(L, "strjoin(): Stack overflow");
        return 0;
    }

    for (int32_t i = 1; i < count; i++) {
        lua_pushlstring(L, separator, length);
        lua_insert(L, 2 * i + 1);
    }

    lua_concat(L, 2 * count - 1);

    return 1;
}

// VERIFY: e631ca5d auto
// OFFSET: 0x00816C40
static int32_t Script_strreplace(lua_State* L) {
    size_t length;
    size_t searchLength;
    size_t replaceLength;
    const char* string = luaL_checklstring(L, 1, &length);
    const char* search = luaL_checklstring(L, 2, &searchLength);
    const char* replace = luaL_checklstring(L, 3, &replaceLength);
    int32_t limit = luaL_optinteger(L, 4, 0);

    char buffer[4096];
    const char* source = string;
    int32_t count = 0;
    char* dest = buffer;
    size_t space = sizeof(buffer);

    while (!limit || count < limit) {
        const char* found = SStrStr(source, search);

        if (!found) {
            break;
        }

        size_t bytes = found - source;

        if (bytes > space) {
            bytes = space;
        }

        memcpy(dest, source, bytes);
        space -= bytes;
        dest += bytes;

        bytes = replaceLength;

        if (bytes > space) {
            bytes = space;
        }

        memcpy(dest, replace, replaceLength);
        dest += bytes;
        space -= bytes;

        count++;
        source = found + searchLength;
    }

    if (count > 0) {
        size_t bytes = string + length - source;

        if (bytes > space) {
            bytes = space;
        }

        memcpy(dest, source, bytes);
        lua_pushlstring(L, buffer, dest + bytes - buffer);
    } else {
        lua_pushlstring(L, string, length);
    }

    lua_pushinteger(L, count);

    return 2;
}

// VERIFY: ff6f3541 auto
// OFFSET: 0x00816D80
static int32_t Script_strconcat(lua_State* L) {
    lua_concat(L, lua_gettop(L));

    return 1;
}

// VERIFY: 8c9d9de7 auto
// OFFSET: 0x00816DA0
static int32_t Script_issecure(lua_State* L) {
    if (lua_tainted) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, 1.0);
    }

    return 1;
}

// VERIFY: ed335c44 auto
// OFFSET: 0x00816DE0
static int32_t Script_issecurevariable(lua_State* L) {
    if (lua_type(L, 1) != LUA_TTABLE) {
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        lua_insert(L, 1);
    }

    if (!lua_isstring(L, 2)) {
        luaL_error(L, "Usage: issecurevariable([table,] \"variable\")");
        return 0;
    }

    const char* name = lua_tostring(L, 2);

    const char* taint = lua_tainted;

    if (++lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = nullptr;
    }

    lua_pushstring(L, name);
    lua_gettable(L, 1);
    const char* variableTaint = lua_tainted;
    lua_pop(L, 2);

    if (lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = taint;
    }

    if (--lua_taintexpected <= 0) {
        lua_taintexpected = 0;
    }

    if (variableTaint) {
        lua_pushnil(L);
        lua_pushstring(L, variableTaint);
    } else {
        lua_pushnumber(L, 1.0);
        lua_pushstring(L, nullptr);
    }

    return 2;
}

// VERIFY: 5259e07f auto
// OFFSET: 0x00816F00
static int32_t Script_forceinsecure(lua_State* L) {
    if (!lua_tainted && lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = "*** TaintForced ***";
    }

    return 0;
}

// VERIFY: 5ffaf535 auto
// OFFSET: 0x00816F30
static int32_t Script_securecall(lua_State* L) {
    ++lua_taintexpected;
    const char* taint = lua_tainted;

    if (lua_isstring(L, 1)) {
        lua_pushstring(L, lua_tostring(L, 1));
        lua_rawget(L, LUA_GLOBALSINDEX);
        lua_remove(L, 1);
        lua_insert(L, 1);
    }

    if (!lua_gettop(L)) {
        lua_pushnil(L);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, s_errorHandlerRef);
    lua_insert(L, 1);

    if (lua_pcall(L, lua_gettop(L) - 2, LUA_MULTRET, 1)) {
        lua_pop(L, 2);
    } else {
        lua_remove(L, 1);
    }

    int32_t top = lua_gettop(L);

    for (int32_t i = 1; i <= top; i++) {
        lua_settaint(L, i, taint);
    }

    if (lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = taint;
    }

    if (--lua_taintexpected <= 0) {
        lua_taintexpected = 0;
    }

    return top;
}

// VERIFY: 458bba82 auto
// OFFSET: 0x00817170
static int32_t Script_hooksecurefunc(lua_State* L) {
    if (lua_type(L, 1) != LUA_TTABLE) {
        lua_pushvalue(L, LUA_GLOBALSINDEX);
        lua_insert(L, 1);
    }

    if (!lua_isstring(L, 2) || lua_type(L, 3) != LUA_TFUNCTION) {
        luaL_error(L, "Usage: hooksecurefunc([table,] \"function\", hookfunc)");
        return 0;
    }

    lua_settop(L, 3);
    const char* name = lua_tostring(L, 2);
    lua_remove(L, 2);

    const char* taint = lua_tainted;

    if (++lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = nullptr;
    }

    lua_pushstring(L, name);
    lua_gettable(L, 1);

    bool hooked;

    if (lua_type(L, -1) == LUA_TFUNCTION) {
        const char* functionTaint = lua_tainted;

        ++lua_taintexpected;
        const char* savedTaint = lua_tainted;

        lua_insert(L, 2);
        lua_settaint(L, 2, functionTaint);
        lua_settaint(L, 3, taint);
        lua_pushcclosure(L, FrameScript_HookWrapper, 2);
        lua_settaint(L, 2, functionTaint);

        if (lua_taintexpected && !lua_taintedclosure) {
            lua_tainted = savedTaint;
        }

        if (--lua_taintexpected <= 0) {
            lua_taintexpected = 0;
        }

        lua_pushstring(L, name);
        lua_insert(L, 2);
        lua_rawset(L, 1);
        hooked = true;
    } else {
        lua_pop(L, 2);
        hooked = false;
    }

    lua_pop(L, 1);

    if (lua_taintexpected && !lua_taintedclosure) {
        lua_tainted = taint;
    }

    if (--lua_taintexpected <= 0) {
        lua_taintexpected = 0;
    }

    if (!hooked) {
        luaL_error(L, "hooksecurefunc(): %s is not a function", name);
        return 0;
    }

    return 0;
}

// VERIFY: aa0b2992 auto
// OFFSET: 0x00817350
static int32_t Script_debugprofilestart(lua_State* L) {
    s_debugProfileStart = OsGetAsyncTimeClocks();

    return 0;
}

// VERIFY: 8e488cf3 auto
// OFFSET: 0x00817370
static int32_t Script_debugprofilestop(lua_State* L) {
    lua_pushnumber(L, (static_cast<int64_t>(OsGetAsyncTimeClocks()) - s_debugProfileStart) * s_scriptTimeDivisor);

    return 1;
}

// VERIFY: 712665fd manual
// OFFSET: 0x008173C0
static int32_t Script_seterrorhandler(lua_State* L) {
    if (lua_type(L, 1) != LUA_TFUNCTION) {
        luaL_error(L, "Usage: seterrorhandler(errfunc)");
        return 0;
    }

    if (s_errorHandlerFunc != -1) {
        luaL_unref(L, LUA_REGISTRYINDEX, s_errorHandlerFunc);
    }

    s_errorHandlerFunc = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}

// VERIFY: e7da762e auto
// OFFSET: 0x00817420
static int32_t Script_geterrorhandler(lua_State* L) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, s_errorHandlerFunc);

    return 1;
}

// VERIFY: 20d7d8ce manual
// OFFSET: 0x00817450
static int32_t Script_date(lua_State* L) {
    const char* format = luaL_optstring(L, 1, "%c");
    time_t t = lua_isnoneornil(L, 2) ? time(nullptr) : static_cast<time_t>(luaL_checknumber(L, 2));
    struct tm* stm;

    if (*format == '!') {
        stm = gmtime(&t);
        format++;
    } else {
        stm = localtime(&t);
    }

    if (!stm) {
        lua_pushnil(L);
        return 1;
    }

    if (!strcmp(format, "*t")) {
        lua_createtable(L, 0, 9);
        lua_pushinteger(L, stm->tm_sec);
        lua_setfield(L, -2, "sec");
        lua_pushinteger(L, stm->tm_min);
        lua_setfield(L, -2, "min");
        lua_pushinteger(L, stm->tm_hour);
        lua_setfield(L, -2, "hour");
        lua_pushinteger(L, stm->tm_mday);
        lua_setfield(L, -2, "day");
        lua_pushinteger(L, stm->tm_mon + 1);
        lua_setfield(L, -2, "month");
        lua_pushinteger(L, stm->tm_year + 1900);
        lua_setfield(L, -2, "year");
        lua_pushinteger(L, stm->tm_wday + 1);
        lua_setfield(L, -2, "wday");
        lua_pushinteger(L, stm->tm_yday + 1);
        lua_setfield(L, -2, "yday");

        if (stm->tm_isdst >= 0) {
            lua_pushboolean(L, stm->tm_isdst);
            lua_setfield(L, -2, "isdst");
        }
    } else {
        char buffer[256];

        if (strftime(buffer, sizeof(buffer), format, stm)) {
            lua_pushstring(L, buffer);
        } else {
            luaL_error(L, "'date' format too long");
        }
    }

    return 1;
}

// VERIFY: 385ff36c manual
// OFFSET: 0x00817650
static int32_t Script_time(lua_State* L) {
    time_t t;

    if (lua_isnoneornil(L, 1)) {
        t = time(nullptr);
    } else {
        struct tm ts;
        luaL_checktype(L, 1, LUA_TTABLE);
        lua_settop(L, 1);

        lua_getfield(L, -1, "sec");
        ts.tm_sec = lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 0;
        lua_pop(L, 1);

        lua_getfield(L, -1, "min");
        ts.tm_min = lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 0;
        lua_pop(L, 1);

        lua_getfield(L, -1, "hour");
        ts.tm_hour = lua_isnumber(L, -1) ? lua_tointeger(L, -1) : 12;
        lua_pop(L, 1);

        lua_getfield(L, -1, "day");

        if (!lua_isnumber(L, -1)) {
            luaL_error(L, "field '%s' missing in date table", "day");
        }

        ts.tm_mday = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "month");

        if (!lua_isnumber(L, -1)) {
            luaL_error(L, "field '%s' missing in date table", "month");
        }

        ts.tm_mon = lua_tointeger(L, -1) - 1;
        lua_pop(L, 1);

        lua_getfield(L, -1, "year");

        if (!lua_isnumber(L, -1)) {
            luaL_error(L, "field '%s' missing in date table", "year");
        }

        ts.tm_year = lua_tointeger(L, -1) - 1900;
        lua_pop(L, 1);

        lua_getfield(L, -1, "isdst");
        ts.tm_isdst = lua_isnil(L, -1) ? -1 : lua_toboolean(L, -1);
        lua_pop(L, 1);

        t = mktime(&ts);
    }

    if (t == static_cast<time_t>(-1)) {
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, static_cast<lua_Number>(t));
    }

    return 1;
}

// VERIFY: 35211e53 auto
// OFFSET: 0x008178A0
static int32_t Script_difftime(lua_State* L) {
    lua_pushnumber(L, difftime(static_cast<time_t>(luaL_checknumber(L, 1)), static_cast<time_t>(luaL_optnumber(L, 2, 0))));

    return 1;
}

// VERIFY: 4d31c728 manual
// OFFSET: 0x008178F0
static int32_t Script_debugstack(lua_State* L) {
    lua_State* L1 = L;
    uint32_t level = 1;
    uint32_t levels1 = 12;
    uint32_t levels2 = 10;
    int32_t firstpart = 1;
    lua_Debug ar;

    if (lua_type(L, 1) == LUA_TTHREAD) {
        L1 = lua_tothread(L, 1);
        level = 0;
        lua_remove(L, 1);
    }

    if (lua_isnumber(L, 1)) {
        level = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));
        lua_remove(L, 1);
    }

    if (lua_isnumber(L, 1)) {
        levels1 = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));
        lua_remove(L, 1);
    }

    if (lua_isnumber(L, 1)) {
        levels2 = static_cast<uint32_t>(static_cast<int64_t>(lua_tonumber(L, 1)));
        lua_remove(L, 1);
    }

    if (lua_gettop(L) == 0) {
        lua_pushliteral(L, "");
    } else if (!lua_isstring(L, 1)) {
        return 1;
    }

    uint32_t last = level + levels1;

    while (lua_getstack(L1, level++, &ar)) {
        if (level > last && firstpart) {
            if (!lua_getstack(L1, level + levels2, &ar)) {
                level--;
            } else {
                lua_pushliteral(L, "...\n");

                while (lua_getstack(L1, level + levels2, &ar)) {
                    level++;
                }
            }

            firstpart = 0;
            continue;
        }

        lua_getinfo(L1, "Snl", &ar);
        lua_pushfstring(L, "%s:", ar.short_src);

        if (ar.currentline > 0) {
            lua_pushfstring(L, "%d:", ar.currentline);
        }

        switch (*ar.namewhat) {
            case 'g':
            case 'l':
            case 'f':
            case 'm':
                lua_pushfstring(L, " in function `%s'", ar.name);
                break;

            default:
                if (*ar.what == 'm') {
                    lua_pushfstring(L, " in main chunk");
                } else if (*ar.what == 'C' || *ar.what == 't') {
                    lua_pushliteral(L, " ?");
                } else {
                    lua_pushfstring(L, " in function <%s:%d>", ar.short_src, ar.linedefined);
                }

                break;
        }

        lua_pushliteral(L, "\n");
        lua_concat(L, lua_gettop(L));
    }

    lua_concat(L, lua_gettop(L));

    return 1;
}

// VERIFY: 348d552b manual
// OFFSET: 0x00817BF0
static int32_t Script_scrub(lua_State* L) {
    int32_t top = lua_gettop(L);
    lua_checkstack(L, 1);

    for (int32_t i = 1; i <= top; i++) {
        int32_t type = lua_type(L, i);

        if (type != LUA_TBOOLEAN && (type < LUA_TNUMBER || type > LUA_TSTRING)) {
            lua_pushnil(L);
            lua_replace(L, i);
        }
    }

    return top;
}

// VERIFY: d7c57f5a auto
// OFFSET: 0x00817C70
static int32_t Script_strlenutf8(lua_State* L) {
    if (!lua_isstring(L, 1)) {
        luaL_error(L, "Usage: strlenutf8(string)");
        return 0;
    }

    lua_pushnumber(L, SStrLenUTF8(lua_tostring(L, 1)));

    return 1;
}

// VERIFY: 28a509c4 auto
// OFFSET: 0x00819A60
static int32_t Script_debuglocals(lua_State* L) {
    if (!s_inErrorHandler) {
        return 0;
    }

    int32_t level = lua_tonumber(L, 1);

    if (level <= 0) {
        level = 1;
    }

    char buffer[4096] = "\0";

    lua_Debug ar;

    if (lua_getstack(L, level, &ar)) {
        const char* name;

        for (int32_t i = 1; (name = lua_getlocal(L, &ar, i)) != nullptr; i++) {
            FrameScript_PackVariable(L, name, 0, buffer, sizeof(buffer));
            lua_pop(L, 1);
        }

        lua_getinfo(L, "f", &ar);

        for (int32_t i = 1; (name = lua_getupvalue(L, -1, i)) != nullptr; i++) {
            FrameScript_PackVariable(L, name, 0, buffer, sizeof(buffer));
            lua_pop(L, 1);
        }

        lua_pop(L, 1);
    }

    lua_pushstring(L, buffer);

    return 1;
}

// VERIFY: 85de31cd auto
// OFFSET: 0x008E5250
static int32_t Script_debugload(lua_State* L) {
    return 0;
}

// VERIFY: 2907b51f auto
// OFFSET: 0x008E5250
static int32_t Script_debuginfo(lua_State* L) {
    return 0;
}

// VERIFY: c72f64d9 auto
// OFFSET: 0x008E5250
static int32_t Script_debugprint(lua_State* L) {
    return 0;
}

// VERIFY: 41518b2f auto
// OFFSET: 0x008E5250
static int32_t Script_debugdump(lua_State* L) {
    return 0;
}

// VERIFY: 7104531d auto
// OFFSET: 0x008E5250
static int32_t Script_debugbreak(lua_State* L) {
    return 0;
}

// VERIFY: 11b77409 auto
// OFFSET: 0x008E5250
static int32_t Script_debughook(lua_State* L) {
    return 0;
}

// VERIFY: 580c4aae auto
// OFFSET: 0x008E5250
static int32_t Script_debugtimestamp(lua_State* L) {
    return 0;
}

// STATIC: 0x00A443A8
const luaL_Reg s_extraFuncs[] = {
    { "setglobal", Script_setglobal },
    { "getglobal", Script_getglobal },
    { "strtrim", Script_strtrim },
    { "strsplit", Script_strsplit },
    { "strjoin", Script_strjoin },
    { "strreplace", Script_strreplace },
    { "strconcat", Script_strconcat },
    { "strlenutf8", Script_strlenutf8 },
    { "issecure", Script_issecure },
    { "issecurevariable", Script_issecurevariable },
    { "forceinsecure", Script_forceinsecure },
    { "securecall", Script_securecall },
    { "hooksecurefunc", Script_hooksecurefunc },
    { "debugload", Script_debugload },
    { "debuginfo", Script_debuginfo },
    { "debugprint", Script_debugprint },
    { "debugdump", Script_debugdump },
    { "debugbreak", Script_debugbreak },
    { "debughook", Script_debughook },
    { "debugtimestamp", Script_debugtimestamp },
    { "debugprofilestart", Script_debugprofilestart },
    { "debugprofilestop", Script_debugprofilestop },
    { "seterrorhandler", Script_seterrorhandler },
    { "geterrorhandler", Script_geterrorhandler },
    { "date", Script_date },
    { "time", Script_time },
    { "difftime", Script_difftime },
    { "debugstack", Script_debugstack },
    { "debuglocals", Script_debuglocals },
    { "scrub", Script_scrub },
    { nullptr, nullptr },
};
