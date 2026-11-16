#ifndef ENGINE_SOURCE_FRAMESCRIPT_FRAMESCRIPT_OBJECT_H
#define ENGINE_SOURCE_FRAMESCRIPT_FRAMESCRIPT_OBJECT_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/lua-5.1/src/lua.h"

struct FrameScript_Method {
    /* 0000 */ const char* m_name;
    /* 0004 */ lua_CFunction m_method;
};

class FrameScript_Object {
    public:
    struct ScriptData {
        /* 0000 */ const char* m_wrapper;
    };

    class ScriptIx {
        public:
        /* 0000 */ int32_t m_luaRef = 0;
        /* 0004 */ const char* m_tainted = nullptr;

        ~ScriptIx();
    };

    static int32_t s_objectTypes;

    /* 0004 */ int32_t m_luaRegistered;
    /* 0008 */ int32_t m_luaObjectRef;
    /* 000C */ FrameScript_Object::ScriptIx m_onEvent;
    bool m_hasScriptEvents;

    static int32_t CreateScriptMetaTable(lua_State* L, void (*registerFunc)(lua_State*));
    static void FillScriptMethodTable(lua_State* L, FrameScript_Method* methods, int32_t count);

    virtual ~FrameScript_Object();
    virtual const char* GetName() = 0;
    virtual int32_t GetScriptMetaTable() = 0;
    virtual FrameScript_Object::ScriptIx* GetScriptByName(const char* name, FrameScript_Object::ScriptData& data);
    virtual bool IsA(int32_t type) = 0;

    FrameScript_Object();
    const char* GetDisplayName();
    int32_t GetLuaObjectRef();
    void GetScriptTime(lua_State* L, const FrameScript_Object::ScriptIx& script, double* time, uint32_t* calls);
    int32_t IsScriptEventRegistered(const char* name);
    void RegisterAllScriptEvents();
    int32_t RegisterScriptEvent(const char* name);
    void RegisterScriptObject(const char* name);
    void RunScript(const FrameScript_Object::ScriptIx& script, int32_t argCount, const char* taint);
    void UnregisterAllScriptEvents();
    void UnregisterScriptEvent(const char* name);
    void UnregisterScriptObject(const char* name);
};

#endif
