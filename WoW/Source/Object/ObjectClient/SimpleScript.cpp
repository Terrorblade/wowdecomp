#include "SimpleScript.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Base/DynamicString.h"

// STATIC: 0x00CA1610
void (*CSimpleScriptManager::s_callbacks[16])(uint32_t);

// STATIC: 0x00CA1654
uint32_t CSimpleScriptManager::s_callbackCount;

// STATIC: 0x00CA1650
CSimpleScriptManager* CSimpleScriptManager::s_instance;

// INLINED
inline CSimpleScriptManager::CSimpleScriptManager() : m_hashMask(-1) {
}

// VERIFY: 8a75f330 manual
// OFFSET: 0x00758C50
void CSimpleScriptManager::Create() {
    CSimpleScriptManager* manager = new (__FILE__, __LINE__) CSimpleScriptManager();

    if (manager) {
        manager->Init(8);
        manager->CopyGlobal("math");
        manager->Execute(
            "local math = math\n"
            "abs = math.abs\n"
            "acos = function (x) return math.deg(math.acos(x)) end\n"
            "asin = function (x) return math.deg(math.asin(x)) end\n"
            "atan = function (x) return math.deg(math.atan(x)) end\n"
            "atan2 = function (x,y) return math.deg(math.atan2(x,y)) end\n"
            "ceil = math.ceil\n"
            "cos = function (x) return math.cos(math.rad(x)) end\n"
            "deg = math.deg\n"
            "exp = math.exp\n"
            "floor = math.floor\n"
            "frexp = math.frexp\n"
            "ldexp = math.ldexp\n"
            "log = math.log\n"
            "log10 = math.log10\n"
            "max = math.max\n"
            "min = math.min\n"
            "mod = math.fmod\n"
            "PI = math.pi\n"
            "rad = math.rad\n"
            "random = math.random\n"
            "randomseed = math.randomseed\n"
            "sin = function (x) return math.sin(math.rad(x)) end\n"
            "sqrt = math.sqrt\n"
            "tan = function (x) return math.tan(math.rad(x)) end\n",
            "MathAliases");
    }

    s_instance = manager;

    for (uint32_t i = 0; i < s_callbackCount; i++) {
        s_callbacks[i](0);
    }
}

// VERIFY: 9583ca22 manual
// OFFSET: 0x00758CD0
void CSimpleScriptManager::Destroy() {
    for (uint32_t i = 0; i < s_callbackCount; i++) {
        s_callbacks[i](1);
    }

    CSimpleScriptManager* manager = s_instance;

    if (manager) {
        manager->Uninit();
        delete manager;
    }

    s_instance = nullptr;
}

// VERIFY: 658186d6 auto
// OFFSET: 0x007581F0
int32_t CSimpleScriptManager::RegisterCallback(void (*callback)(uint32_t)) {
    uint32_t count = s_callbackCount;

    if (count == 16) {
        return 0;
    }

    s_callbacks[count] = callback;
    s_callbackCount = count + 1;
    return 1;
}

// VERIFY: d4694fe9 auto
// OFFSET: 0x007581A0
void CSimpleScriptManager::UnregisterCallback(void (*callback)(uint32_t)) {
    for (uint32_t i = 0; i < s_callbackCount; i++) {
        if (s_callbacks[i] == callback) {
            uint32_t last = s_callbackCount - 1;

            for (uint32_t j = i; j < last; j++) {
                s_callbacks[j] = s_callbacks[j + 1];
            }

            s_callbackCount = last;
            return;
        }
    }
}

// VERIFY: ef14b3e2 auto
// OFFSET: 0x00758480
int32_t CSimpleScriptManager::CallFunction(uint32_t handle, const SimpleScriptIO& in, SimpleScriptIO& out) {
    SimpleScriptFunction* function = this->GetFunction(handle);

    if (!function || function->m_luaRef == -2) {
        return 0;
    }

    lua_State* L = FrameScript_GetContext();
    lua_rawgeti(L, LUA_REGISTRYINDEX, FrameScript_GetErrorHandlerReference());
    int32_t errorHandler = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, function->m_luaRef);

    for (int32_t i = 0; i < function->m_numArgs; i++) {
        lua_pushnumber(L, in.m_numbers.m_data[i]);
    }

    for (int32_t i = 0; i < function->m_numStringArgs; i++) {
        lua_pushstring(L, &in.m_strings[i * in.m_stringSize]);
    }

    if (lua_pcall(L, function->m_numStringArgs + function->m_numArgs, function->m_numStringReturns + function->m_numReturns, errorHandler)) {
        lua_settop(L, -3);
        luaL_unref(L, LUA_REGISTRYINDEX, function->m_luaRef);
        function->m_luaRef = -2;
        return 0;
    }

    out.m_numbers.m_count = function->m_numReturns;
    out.m_stringCount = function->m_numStringReturns;

    for (int32_t i = function->m_numStringReturns - 1; i >= 0; i--) {
        const char* string = lua_tolstring(L, -1, nullptr);

        if (string) {
            SStrCopy(&out.m_strings[i * out.m_stringSize], string, out.m_stringSize);
        } else {
            out.m_strings[i * out.m_stringSize] = '\0';
        }

        lua_settop(L, -2);
    }

    for (int32_t i = function->m_numReturns - 1; i >= 0; i--) {
        out.m_numbers.m_data[i] = lua_tonumber(L, -1);
        lua_settop(L, -2);
    }

    lua_settop(L, -2);
    return 1;
}

// VERIFY: 5225ecc9 auto
// OFFSET: 0x00758220
void CSimpleScriptManager::CopyGlobal(const char* name) {
    lua_State* L = FrameScript_GetContext();
    lua_rawgeti(L, LUA_REGISTRYINDEX, this->m_envRef);
    lua_pushstring(L, name);
    lua_pushstring(L, name);
    lua_rawget(L, LUA_GLOBALSINDEX);
    lua_settable(L, -3);
    lua_settop(L, -2);
}

// VERIFY: a48f28a7 auto
// OFFSET: 0x00758280
int32_t CSimpleScriptManager::Execute(const char* script, const char* name) {
    lua_State* L = FrameScript_GetContext();
    lua_rawgeti(L, LUA_REGISTRYINDEX, FrameScript_GetErrorHandlerReference());

    if (luaL_loadbuffer(L, script, SStrLen(script), name)) {
        if (lua_pcall(L, 1, 0, 0)) {
            lua_settop(L, -2);
        }

        return 0;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, this->m_envRef);
    lua_setfenv(L, -2);

    if (lua_pcall(L, 0, 0, 1)) {
        lua_settop(L, -3);
        return 0;
    }

    lua_settop(L, -2);
    return 1;
}

// VERIFY: 8bb0a416 auto
// OFFSET: 0x00758340
uint32_t CSimpleScriptManager::FindFunction(int32_t id) {
    uint32_t bucketIndex = id & this->m_hashMask;
    TSGrowableArray<SimpleScriptFunction>& bucket = this->m_buckets[bucketIndex];

    for (uint32_t i = 0; i < bucket.m_count; i++) {
        if (bucket.m_data[i].m_id == id) {
            return i | (bucketIndex << 16);
        }
    }

    return 0xFFFFFFFF;
}

// VERIFY: c0a0927f manual
// OFFSET: 0x00758B80
int32_t CSimpleScriptManager::Init(uint32_t bits) {
    this->Uninit();

    if (bits > 16) {
        return 0;
    }

    lua_State* L = FrameScript_GetContext();
    lua_createtable(L, 0, 0);
    this->m_envRef = luaL_ref(L, LUA_REGISTRYINDEX);

    uint32_t count = 1 << bits;
    this->m_hashMask = count - 1;
    this->m_buckets = new (__FILE__, __LINE__) TSGrowableArray<SimpleScriptFunction>[count];

    return 1;
}

// VERIFY: dfbb685e manual
// OFFSET: 0x007586E0
void CSimpleScriptManager::Uninit() {
    if (this->m_hashMask == -1) {
        return;
    }

    lua_State* L = FrameScript_GetContext();

    for (int32_t i = 0; i < this->m_hashMask + 1; i++) {
        TSGrowableArray<SimpleScriptFunction>& bucket = this->m_buckets[i];

        for (int32_t j = 0; j < (int32_t)bucket.m_count; j++) {
            luaL_unref(L, LUA_REGISTRYINDEX, bucket.m_data[j].m_luaRef);
        }
    }

    delete[] this->m_buckets;

    luaL_unref(L, LUA_REGISTRYINDEX, this->m_envRef);
    this->m_hashMask = -1;
}

// VERIFY: e64eaffe manual
// OFFSET: 0x00758390
SimpleScriptFunction* CSimpleScriptManager::GetFunction(uint32_t handle) {
    if (handle == 0xFFFFFFFF || this->m_hashMask == -1 || (handle >> 16) > static_cast<uint32_t>(this->m_hashMask)) {
        return nullptr;
    }

    TSGrowableArray<SimpleScriptFunction>* list = &this->m_buckets[handle >> 16];
    uint32_t index = handle & 0xFFFF;

    if (index >= list->m_count) {
        return nullptr;
    }

    return &list->m_data[index];
}

// VERIFY: 2600d97c auto
// OFFSET: 0x007587D0
uint32_t CSimpleScriptManager::CreateFunction(int32_t id, const SimpleScriptFunctionDesc& desc) {
    CDynamicString script;
    script.SetCapacity(512);
    script += "return function(";
    int32_t separator = 0;
    int32_t numArgs = 0;
    int32_t numStringArgs = 0;

    if (desc.m_args && *desc.m_args) {
        do {
            if (separator) {
                script += ", ";
            }

            script += desc.m_args[numArgs];
            separator = 1;
            numArgs++;
        } while (desc.m_args[numArgs]);
    }

    if (desc.m_stringArgs && *desc.m_stringArgs) {
        do {
            if (separator) {
                script += ", ";
            }

            script += desc.m_stringArgs[numStringArgs];
            separator = 1;
            numStringArgs++;
        } while (desc.m_stringArgs[numStringArgs]);
    }

    script += ")\n";
    int32_t numReturns = 0;
    int32_t numStringReturns = 0;

    if (desc.m_returns && *desc.m_returns) {
        do {
            script += "local ";
            script += desc.m_returns[numReturns];
            script += " = 0.0\n";
            numReturns++;
        } while (desc.m_returns[numReturns]);
    }

    if (desc.m_stringReturns && *desc.m_stringReturns) {
        do {
            script += "local ";
            script += desc.m_stringReturns[numStringReturns];
            script += " = \"\"\n";
            numStringReturns++;
        } while (desc.m_stringReturns[numStringReturns]);
    }

    script += desc.m_body;
    script += "\nreturn ";
    separator = 0;

    if (desc.m_returns) {
        for (int32_t i = 0; desc.m_returns[i]; i++) {
            if (separator) {
                script += ", ";
            }

            script += desc.m_returns[i];
            separator = 1;
        }
    }

    if (desc.m_stringReturns) {
        for (int32_t i = 0; desc.m_stringReturns[i]; i++) {
            if (separator) {
                script += ", ";
            }

            script += desc.m_stringReturns[i];
            separator = 1;
        }
    }

    script += "\nend\n";
    lua_State* L = FrameScript_GetContext();
    lua_rawgeti(L, LUA_REGISTRYINDEX, FrameScript_GetErrorHandlerReference());

    if (luaL_loadbuffer(L, script.m_buffer, script.m_length, desc.m_name)) {
        if (lua_pcall(L, 1, 0, 0)) {
            lua_settop(L, -2);
        }

        return 0xFFFFFFFF;
    }

    if (lua_pcall(L, 0, 1, 1)) {
        lua_settop(L, -3);
        return 0xFFFFFFFF;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, this->m_envRef);
    lua_setfenv(L, -2);
    uint32_t handle = this->FindFunction(id);
    SimpleScriptFunction local;
    SimpleScriptFunction* function;

    if (handle == 0xFFFFFFFF) {
        function = &local;
    } else {
        function = this->GetFunction(handle);
        luaL_unref(L, LUA_REGISTRYINDEX, function->m_luaRef);
    }

    function->m_id = id;
    function->m_luaRef = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_settop(L, -2);
    function->m_numArgs = numArgs;
    function->m_numStringArgs = numStringArgs;
    function->m_numReturns = numReturns;
    function->m_numStringReturns = numStringReturns;

    if (handle != 0xFFFFFFFF) {
        return handle;
    }

    uint32_t bucket = id & this->m_hashMask;
    TSGrowableArray<SimpleScriptFunction>* list = &this->m_buckets[bucket];
    list->Add(1, function);
    return (bucket << 16) | (list->m_count - 1);
}
