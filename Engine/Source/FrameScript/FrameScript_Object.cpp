#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// STATIC: 0x00D3F778
int32_t FrameScript_Object::s_objectTypes;

// VERIFY: 53798b75 auto
// OFFSET: 0x00482AA0
const char* FrameScript_Object::GetDisplayName() {
    const char* name = this->GetName();

    return name ? name : "<unnamed>";
}

// VERIFY: da17dfd1 auto
// OFFSET: 0x00488380
int32_t FrameScript_Object::GetLuaObjectRef() {
    if (!this->m_luaRegistered) {
        this->RegisterScriptObject(nullptr);
    }

    return this->m_luaObjectRef;
}

// VERIFY: 1a7b2f73 auto
// OFFSET: 0x00816790
int32_t FrameScript_Object::CreateScriptMetaTable(lua_State* L, void (*registerFunc)(lua_State*)) {
    lua_newtable(L);
    lua_pushstring(L, "__index");
    lua_newtable(L);
    registerFunc(L);
    lua_settable(L, -3);

    return luaL_ref(L, LUA_REGISTRYINDEX);
}

// VERIFY: 9883b20b manual
// OFFSET: 0x008167E0
void FrameScript_Object::FillScriptMethodTable(lua_State* L, FrameScript_Method* methods, int32_t count) {
    for (int32_t i = 0; i < count; i++) {
        lua_pushstring(L, methods[i].m_name);
        lua_pushcfunction(L, methods[i].m_method);
        lua_settable(L, -3);
    }
}

// VERIFY: 12a35642 auto
// OFFSET: 0x00816830
FrameScript_Object::ScriptIx* FrameScript_Object::GetScriptByName(const char* name, FrameScript_Object::ScriptData& data) {
    if (!SStrCmpI(name, "OnEvent", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,event,...) %s end";
        return &this->m_onEvent;
    }

    return nullptr;
}

// VERIFY: 91b0dc03 auto
// OFFSET: 0x00816870
void FrameScript_Object::GetScriptTime(lua_State* L, const FrameScript_Object::ScriptIx& script, double* time, uint32_t* calls) {
    if (!script.m_luaRef) {
        return;
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, script.m_luaRef);

    double functionTime;
    double totalTime;
    int functionCalls;
    lua_gcgetfunctiontime(L, &functionTime, &totalTime, &functionCalls);
    lua_pop(L, 1);

    *time += totalTime;
    *calls += functionCalls;
}

// VERIFY: d82c975a auto
// OFFSET: 0x00819810
FrameScript_Object::ScriptIx::~ScriptIx() {
    if (this->m_luaRef) {
        luaL_unref(s_context, LUA_REGISTRYINDEX, this->m_luaRef);
    }
}

// VERIFY: 37adfba3 auto
// OFFSET: 0x00819830
FrameScript_Object::FrameScript_Object() {
    this->m_luaRegistered = 0;
    this->m_luaObjectRef = LUA_NOREF;
    this->m_hasScriptEvents = false;
}

// VERIFY: b0bf6152 auto
// OFFSET: [0x00819850, 0x00819E50]
FrameScript_Object::~FrameScript_Object() {
    lua_State* L = s_context;

    if (L && this->m_hasScriptEvents) {
        for (FrameScript_EventObject* event = s_scriptEventsHash.m_fulllist.Head(); event; event = s_scriptEventsHash.m_fulllist.Next(event)) {
            for (EVENTLISTENERNODE* node = event->m_registerListeners.Head(); !event->m_registerListeners.IsEnd(node); node = event->m_registerListeners.RawNext(node)) {
                if (node->m_listener == this) {
                    event->m_registerListeners.DeleteNode(node);
                    break;
                }
            }

            FrameScript_UnregisterScriptEvent(this, event);
        }
    }

    // Clear t[0] so Lua tables that outlive this object cannot reach freed memory.
    if (L && this->m_luaRegistered > 0 && this->m_luaObjectRef != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, this->m_luaObjectRef);

        if (lua_type(L, -1) == LUA_TTABLE) {
            lua_rawgeti(L, -1, 0);
            bool owned = lua_touserdata(L, -1) == this;
            lua_pop(L, 1);

            if (owned) {
                lua_pushnumber(L, 0.0);
                lua_pushnil(L);
                lua_rawset(L, -3);
                lua_pop(L, 1);
                luaL_unref(L, LUA_REGISTRYINDEX, this->m_luaObjectRef);
            } else {
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }

        this->m_luaRegistered = 0;
        this->m_luaObjectRef = LUA_NOREF;
    }
}

// VERIFY: 12e14749 auto
// OFFSET: 0x00819880
void FrameScript_Object::RegisterScriptObject(const char* name) {
    lua_State* L = s_context;

    if (!this->m_luaRegistered) {
        const char* taint = lua_tainted;

        if (++lua_taintexpected && !lua_taintedclosure) {
            lua_tainted = nullptr;
        }

        lua_newtable(L);
        lua_pushnumber(L, 0.0);
        lua_pushlightuserdata(L, this);
        lua_rawset(L, -3);
        lua_rawgeti(L, LUA_REGISTRYINDEX, this->GetScriptMetaTable());
        lua_setmetatable(L, -2);
        this->m_luaObjectRef = luaL_ref(L, LUA_REGISTRYINDEX);

        if (lua_taintexpected && !lua_taintedclosure) {
            lua_tainted = taint;
        }

        if (--lua_taintexpected <= 0) {
            lua_taintexpected = 0;
        }
    }

    this->m_luaRegistered++;

    if (name) {
        bool exists = false;

        lua_pushstring(L, name);
        lua_rawget(L, LUA_GLOBALSINDEX);

        if (lua_type(L, -1) != LUA_TNIL) {
            exists = true;
        }

        lua_pop(L, 1);

        if (!exists) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, this->m_luaObjectRef);
            lua_setglobal(L, name);
        }
    }
}

// VERIFY: 067f37ae auto
// OFFSET: 0x008199C0
void FrameScript_Object::UnregisterScriptObject(const char* name) {
    lua_State* L = s_context;

    if (this->m_luaRegistered > 0 && --this->m_luaRegistered == 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, this->m_luaObjectRef);
        lua_pushnumber(L, 0.0);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        luaL_unref(L, LUA_REGISTRYINDEX, this->m_luaObjectRef);
        this->m_luaObjectRef = LUA_NOREF;
    }

    if (name) {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }
}

// VERIFY: c69819bb auto
// OFFSET: 0x0081A2C0
void FrameScript_Object::RunScript(const FrameScript_Object::ScriptIx& script, int32_t argCount, const char* taint) {
    if (!taint) {
        taint = script.m_tainted;
    }

    FrameScript_Execute(script.m_luaRef, this, argCount, taint, nullptr);
}

// VERIFY: 965dec45 auto
// OFFSET: 0x0081B330
int32_t FrameScript_Object::IsScriptEventRegistered(const char* name) {
    FrameScript_EventObject* event = s_scriptEventsHash.Ptr(name);

    if (event) {
        for (EVENTLISTENERNODE* node = event->m_listeners.Head(); !event->m_listeners.IsEnd(node); node = event->m_listeners.RawNext(node)) {
            if (node->m_listener == this) {
                return 1;
            }
        }
    }

    return 0;
}

// VERIFY: 3ab070bd manual
// OFFSET: 0x0081B380
int32_t FrameScript_Object::RegisterScriptEvent(const char* name) {
    FrameScript_EventObject* event = s_scriptEventsHash.Ptr(name);

    if (!event) {
        return 0;
    }

    if (event->m_pendingSignalCount) {
        for (EVENTLISTENERNODE* node = event->m_unregisterListeners.Head(); !event->m_unregisterListeners.IsEnd(node); node = event->m_unregisterListeners.RawNext(node)) {
            if (node->m_listener == this) {
                event->m_unregisterListeners.DeleteNode(node);
                break;
            }
        }
    }

    for (EVENTLISTENERNODE* node = event->m_listeners.Head(); !event->m_listeners.IsEnd(node); node = event->m_listeners.RawNext(node)) {
        if (node->m_listener == this) {
            return 1;
        }
    }

    this->m_hasScriptEvents = true;
    FrameScript_RegisterScriptEvent(this, event);

    return 1;
}

// VERIFY: f450bc3d manual
// OFFSET: 0x0081B410
void FrameScript_Object::UnregisterScriptEvent(const char* name) {
    FrameScript_EventObject* event = s_scriptEventsHash.Ptr(name);

    if (!event) {
        return;
    }

    if (event->m_pendingSignalCount) {
        for (EVENTLISTENERNODE* node = event->m_registerListeners.Head(); !event->m_registerListeners.IsEnd(node); node = event->m_registerListeners.RawNext(node)) {
            if (node->m_listener == this) {
                event->m_registerListeners.DeleteNode(node);
                break;
            }
        }
    }

    FrameScript_UnregisterScriptEvent(this, event);
}

// VERIFY: e2ddf32b manual
// OFFSET: 0x0081B470
void FrameScript_Object::RegisterAllScriptEvents() {
    for (FrameScript_EventObject* event = s_scriptEventsHash.m_fulllist.Head(); event; event = s_scriptEventsHash.m_fulllist.Next(event)) {
        this->RegisterScriptEvent(event->m_key.m_str);
    }
}

// VERIFY: 5895f3a8 manual
// OFFSET: 0x0081B4C0
void FrameScript_Object::UnregisterAllScriptEvents() {
    for (FrameScript_EventObject* event = s_scriptEventsHash.m_fulllist.Head(); event; event = s_scriptEventsHash.m_fulllist.Next(event)) {
        this->UnregisterScriptEvent(event->m_key.m_str);
    }
}
