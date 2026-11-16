#include "CSimpleModelFFX.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/Model2/M2Light.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// STATIC: 0x00AC467C
int32_t CSimpleModelFFX::s_metatable = LUA_NOREF;

// VERIFY: d1322319 auto
// OFFSET: 0x004E6BE0
static int32_t Script_ResetLights(lua_State* L) {
    CSimpleModelFFX* model = static_cast<CSimpleModelFFX*>(FrameScript_GetObjectThis(L, CSimpleModel::GetObjectType()));

    if (!model || model->GetScriptMetaTable() != CSimpleModelFFX::s_metatable) {
        luaL_error(L, "Wrong object type for member function");
        return 0;
    }

    for (uint32_t i = 0; i < 2; i++) {
        model->m_lightArray3[i].m_lightCount = 0;
        model->m_lightArray3[i].m_hasCustomLight = 0;
        model->m_lightArray2[i].m_lightCount = 0;
        model->m_lightArray2[i].m_hasCustomLight = 0;
        model->m_lightArray1[i].m_lightCount = 0;
        model->m_lightArray1[i].m_hasCustomLight = 0;
    }

    return 0;
}

// VERIFY: 1e97ca9e manual
// OFFSET: 0x004E6C60
static int32_t Script_AddLight(lua_State* L) {
    const char* name = "model";
    CSimpleModelFFX* model = static_cast<CSimpleModelFFX*>(FrameScript_GetObjectThis(L, CSimpleModel::GetObjectType()));

    if (!model || model->GetScriptMetaTable() != CSimpleModelFFX::s_metatable) {
        luaL_error(L, "Wrong object type for member function");
        return 0;
    }

    if (model) {
        uint32_t index = 0;

        if (lua_isnumber(L, 2)) {
            index = lua_tointeger(L, 2) != 0;
        }

        CM2Light light;

        if (CSimpleModel::SetLightHelper(L, 3, &light)) {
            CM2LightArray* lights = &model->m_lightArray1[index];

            if (lights->m_lightCount < 4) {
                lights->m_lights[lights->m_lightCount] = light;
                lights->m_lightCount++;
                model->m_lightArray1[index].m_hasCustomLight = 1;
            }

            return 0;
        }

        name = model->GetName();

        if (!name) {
            name = "<unnamed>";
        }
    }

    luaL_error(L, "Usage: %s:AddLight(index, enabled[, omni, dirX, dirY, dirZ, ambIntensity[, ambR, ambG, ambB], dirIntensity[, dirR, dirG, dirB]])", name);
    return 0;
}

// VERIFY: 0b9f472d manual
// OFFSET: 0x004E6D60
static int32_t Script_AddCharacterLight(lua_State* L) {
    const char* name = "model";
    CSimpleModelFFX* model = static_cast<CSimpleModelFFX*>(FrameScript_GetObjectThis(L, CSimpleModel::GetObjectType()));

    if (!model || model->GetScriptMetaTable() != CSimpleModelFFX::s_metatable) {
        luaL_error(L, "Wrong object type for member function");
        return 0;
    }

    if (model) {
        uint32_t index = 0;

        if (lua_isnumber(L, 2)) {
            index = lua_tointeger(L, 2) != 0;
        }

        CM2Light light;

        if (CSimpleModel::SetLightHelper(L, 3, &light)) {
            CM2LightArray* lights = &model->m_lightArray2[index];

            if (lights->m_lightCount < 4) {
                lights->m_lights[lights->m_lightCount] = light;
                lights->m_lightCount++;
                model->m_lightArray2[index].m_hasCustomLight = 1;
            }

            return 0;
        }

        name = model->GetName();

        if (!name) {
            name = "<unnamed>";
        }
    }

    luaL_error(L, "Usage: %s:AddCharacterLight(index, enabled[, omni, dirX, dirY, dirZ, ambIntensity[, ambR, ambG, ambB], dirIntensity[, dirR, dirG, dirB]])", name);
    return 0;
}

// VERIFY: eeab67da manual
// OFFSET: 0x004E6E60
static int32_t Script_AddPetLight(lua_State* L) {
    const char* name = "model";
    CSimpleModelFFX* model = static_cast<CSimpleModelFFX*>(FrameScript_GetObjectThis(L, CSimpleModel::GetObjectType()));

    if (!model || model->GetScriptMetaTable() != CSimpleModelFFX::s_metatable) {
        luaL_error(L, "Wrong object type for member function");
        return 0;
    }

    if (model) {
        uint32_t index = 0;

        if (lua_isnumber(L, 2)) {
            index = lua_tointeger(L, 2) != 0;
        }

        CM2Light light;

        if (CSimpleModel::SetLightHelper(L, 3, &light)) {
            CM2LightArray* lights = &model->m_lightArray3[index];

            if (lights->m_lightCount < 4) {
                lights->m_lights[lights->m_lightCount] = light;
                lights->m_lightCount++;
                model->m_lightArray3[index].m_hasCustomLight = 1;
            }

            return 0;
        }

        name = model->GetName();

        if (!name) {
            name = "<unnamed>";
        }
    }

    luaL_error(L, "Usage: %s:AddCharacterLight(index, enabled[, omni, dirX, dirY, dirZ, ambIntensity[, ambR, ambG, ambB], dirIntensity[, dirR, dirG, dirB]])", name);
    return 0;
}

// VERIFY: 3674f866 auto
// OFFSET: 0x004E6F60
void CSimpleModelFFX::CreateScriptMetaTable() {
    CSimpleModelFFX::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CSimpleModelFFX::RegisterScriptMethods);
}

// STATIC: 0x00AC465C
static FrameScript_Method s_ScriptMethods[] = {
    { "ResetLights", &Script_ResetLights },
    { "AddLight", &Script_AddLight },
    { "AddCharacterLight", &Script_AddCharacterLight },
    { "AddPetLight", &Script_AddPetLight },
};

// VERIFY: f7905f30 auto
// OFFSET: 0x004E6BC0
void CSimpleModelFFX::RegisterScriptMethods(lua_State* L) {
    CSimpleModel::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
