#include "HoT/Ui/Atlas.h"
#include <cctype>
#include <string>
#include <unordered_map>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "HoT/Core/Modules.h"
#include "HoT/Lua/Script.h"

namespace HoT::Atlas {

struct AtlasEntry {
    int32_t m_width;
    int32_t m_height;
    double m_left;
    double m_right;
    double m_top;
    double m_bottom;
    bool m_tileHorizontally;
    bool m_tileVertically;
    std::string m_file;
};

static std::unordered_map<std::string, AtlasEntry> s_atlases;

static const std::string& NormalizeName(const char* name) {
    static std::string s_key;
    s_key.assign(name ? name : "");

    for (char& c : s_key) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    return s_key;
}

// Paths are typed by hand, so forward slashes and trailing whitespace are accepted.
static std::string NormalizePath(const char* path) {
    std::string out(path ? path : "");

    for (char& c : out) {
        if (c == '/') {
            c = '\\';
        }
    }

    while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) {
        out.pop_back();
    }

    return out;
}

static double FieldNumber(lua_State* L, int32_t table, int32_t n) {
    lua_rawgeti(L, table, n);
    double value = lua_type(L, -1) == LUA_TNUMBER ? lua_tonumber(L, -1) : 0.0;
    lua_pop(L, 1);
    return value;
}

static bool FieldBool(lua_State* L, int32_t table, int32_t n) {
    lua_rawgeti(L, table, n);
    bool value = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return value;
}

// Leaves the method and its self argument on the stack for the caller to push arguments.
static bool BeginMethod(lua_State* L, int32_t object, const char* method) {
    lua_getfield(L, object, method);

    if (lua_type(L, -1) != LUA_TFUNCTION) {
        lua_pop(L, 1);
        return false;
    }

    lua_pushvalue(L, object);
    return true;
}

static void EndMethod(lua_State* L, int32_t argCount) {
    if (lua_pcall(L, argCount + 1, 0, 0) != 0) {
        lua_pop(L, 1);
    }
}

// C_Texture.LoadAtlasData({ ["path\\to\\sheet.blp"] = { ["atlas-name"] = { w, h, l, r, t, b, tileH, tileV } } }) returns the number added.
static int32_t Script_LoadAtlasData(lua_State* L) {
    if (lua_type(L, 1) != LUA_TTABLE) {
        lua_pushnumber(L, 0);
        return 1;
    }

    int32_t count = 0;

    lua_pushnil(L);

    while (lua_next(L, 1)) {
        int32_t group = lua_gettop(L);

        if (lua_type(L, group - 1) == LUA_TSTRING && lua_type(L, group) == LUA_TTABLE) {
            std::string file = NormalizePath(lua_tolstring(L, group - 1, nullptr));

            lua_pushnil(L);

            while (lua_next(L, group)) {
                int32_t data = lua_gettop(L);

                if (lua_type(L, data - 1) == LUA_TSTRING && lua_type(L, data) == LUA_TTABLE) {
                    AtlasEntry entry;
                    entry.m_width = static_cast<int32_t>(FieldNumber(L, data, 1));
                    entry.m_height = static_cast<int32_t>(FieldNumber(L, data, 2));
                    entry.m_left = FieldNumber(L, data, 3);
                    entry.m_right = FieldNumber(L, data, 4);
                    entry.m_top = FieldNumber(L, data, 5);
                    entry.m_bottom = FieldNumber(L, data, 6);
                    entry.m_tileHorizontally = FieldBool(L, data, 7);
                    entry.m_tileVertically = FieldBool(L, data, 8);
                    entry.m_file = file;

                    s_atlases[NormalizeName(lua_tolstring(L, data - 1, nullptr))] = std::move(entry);
                    count++;
                }

                lua_pop(L, 1);
            }
        }

        lua_pop(L, 1);
    }

    lua_pushnumber(L, count);
    return 1;
}

static int32_t Script_GetAtlasInfo(lua_State* L) {
    if (lua_type(L, 1) != LUA_TSTRING) {
        lua_pushnil(L);
        return 1;
    }

    auto it = s_atlases.find(NormalizeName(lua_tolstring(L, 1, nullptr)));

    if (it == s_atlases.end()) {
        lua_pushnil(L);
        return 1;
    }

    const AtlasEntry& entry = it->second;

    lua_createtable(L, 0, 13);

    lua_pushnumber(L, entry.m_width);
    lua_setfield(L, -2, "width");
    lua_pushnumber(L, entry.m_height);
    lua_setfield(L, -2, "height");

    lua_pushnumber(L, entry.m_left);
    lua_setfield(L, -2, "left");
    lua_pushnumber(L, entry.m_right);
    lua_setfield(L, -2, "right");
    lua_pushnumber(L, entry.m_top);
    lua_setfield(L, -2, "top");
    lua_pushnumber(L, entry.m_bottom);
    lua_setfield(L, -2, "bottom");

    // Retail spellings, so ported UI code works unedited.
    lua_pushnumber(L, entry.m_left);
    lua_setfield(L, -2, "leftTexCoord");
    lua_pushnumber(L, entry.m_right);
    lua_setfield(L, -2, "rightTexCoord");
    lua_pushnumber(L, entry.m_top);
    lua_setfield(L, -2, "topTexCoord");
    lua_pushnumber(L, entry.m_bottom);
    lua_setfield(L, -2, "bottomTexCoord");

    lua_pushboolean(L, entry.m_tileHorizontally);
    lua_setfield(L, -2, "tilesHorizontally");
    lua_pushboolean(L, entry.m_tileVertically);
    lua_setfield(L, -2, "tilesVertically");

    lua_pushstring(L, entry.m_file.c_str());
    lua_setfield(L, -2, "file");

    return 1;
}

// SetAtlas(texture, atlasName, useAtlasSize), also reachable as C_Texture.SetAtlas.
static int32_t Script_SetAtlas(lua_State* L) {
    if (lua_type(L, 1) <= 0 || lua_type(L, 2) != LUA_TSTRING) {
        lua_pushboolean(L, 0);
        return 1;
    }

    auto it = s_atlases.find(NormalizeName(lua_tolstring(L, 2, nullptr)));

    if (it == s_atlases.end()) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const AtlasEntry& entry = it->second;
    bool useAtlasSize = lua_toboolean(L, 3) != 0;

    if (BeginMethod(L, 1, "SetTexture")) {
        lua_pushstring(L, entry.m_file.c_str());
        EndMethod(L, 1);
    }

    if (BeginMethod(L, 1, "SetTexCoord")) {
        lua_pushnumber(L, entry.m_left);
        lua_pushnumber(L, entry.m_right);
        lua_pushnumber(L, entry.m_top);
        lua_pushnumber(L, entry.m_bottom);
        EndMethod(L, 4);
    }

    if (BeginMethod(L, 1, "SetHorizTile")) {
        lua_pushboolean(L, entry.m_tileHorizontally);
        EndMethod(L, 1);
    }

    if (BeginMethod(L, 1, "SetVertTile")) {
        lua_pushboolean(L, entry.m_tileVertically);
        EndMethod(L, 1);
    }

    if (useAtlasSize) {
        if (BeginMethod(L, 1, "SetWidth")) {
            lua_pushnumber(L, entry.m_width);
            EndMethod(L, 1);
        }

        if (BeginMethod(L, 1, "SetHeight")) {
            lua_pushnumber(L, entry.m_height);
            EndMethod(L, 1);
        }
    }

    lua_pushboolean(L, 1);
    return 1;
}

static void Install(lua_State* L) {
    if (!L) {
        return;
    }

    lua_getfield(L, LUA_GLOBALSINDEX, "C_Texture");

    if (lua_type(L, -1) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_createtable(L, 0, 4);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_GLOBALSINDEX, "C_Texture");
    }

    lua_pushcclosure(L, Script_LoadAtlasData, 0);
    lua_setfield(L, -2, "LoadAtlasData");

    lua_pushcclosure(L, Script_GetAtlasInfo, 0);
    lua_setfield(L, -2, "GetAtlasInfo");

    lua_pushcclosure(L, Script_SetAtlas, 0);
    lua_setfield(L, -2, "SetAtlas");

    lua_pop(L, 1);

    lua_pushcclosure(L, Script_SetAtlas, 0);
    lua_setfield(L, LUA_GLOBALSINDEX, "SetAtlas");
}

class AtlasModule : public Module {
    public:
    const char* Name() const override {
        return "Atlas";
    }

    // Runs before FrameXML.toc so UI files can call SetAtlas from their OnLoad.
    void OnScriptsLoaded(Lua::State state) override {
        Install(FrameScript_GetContext());
    }
};

Module* GetModule() {
    static AtlasModule module;
    return &module;
}

}
