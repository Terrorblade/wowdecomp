#include <cstddef>
#include <cstdint>
#include <new>
#include "Engine/Source/Frame/CSimpleAnimGroup.h"
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/Base/Status.h"
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CScriptRegion.h"
#include "Engine/Source/Frame/CSimpleAlphaAnim.h"
#include "Engine/Source/Frame/CSimpleAnim.h"
#include "Engine/Source/Frame/CSimpleFrame.h"
#include "Engine/Source/Frame/CSimplePathAnim.h"
#include "Engine/Source/Frame/CSimpleRotationAnim.h"
#include "Engine/Source/Frame/CSimpleScaleAnim.h"
#include "Engine/Source/Frame/CSimpleTranslationAnim.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameXML/FrameXML.h"
#include "Engine/Source/FrameXML/LoadXML.h"
#include "Engine/Source/FrameXML/XMLTree.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"

// STATIC: 0x00AC1AB4
int32_t CSimpleAnimGroup::s_metatable = LUA_NOREF;

// STATIC: 0x00B499DC
int32_t CSimpleAnimGroup::s_objectType;

// STATIC: 0x00AC13A8
const char* CSimpleAnimGroup::s_objectTypeName = "AnimationGroup";

// VERIFY: eed0442b auto
// OFFSET: 0x004A4F20
int32_t CSimpleAnimGroup::GetObjectType() {
    if (!CSimpleAnimGroup::s_objectType) {
        CSimpleAnimGroup::s_objectType = ++FrameScript_Object::s_objectTypes;
    }

    return CSimpleAnimGroup::s_objectType;
}

// VERIFY: 3587a1ad auto
// OFFSET: 0x00497770
void CSimpleAnimGroup::PreLoadXML(XMLNode* node, CStatus* status) {
    this->m_flags |= SIMPLEANIMGROUP_FLAG_IS_LOADING;

    const char* name = node->GetAttributeByName("name");

    if (name && *name) {
        this->SetName(name);
    }
}

// VERIFY: 01e00f9f auto
// OFFSET: 0x004977B0
void CSimpleAnimGroup::LoadXML_Anim(XMLNode* node, CStatus* status, CSimpleAnim* anim) {
    if (!anim) {
        return;
    }

    anim->m_flags |= 1;

    const char* name = node->GetAttributeByName("name");

    if (name && *name) {
        anim->SetName(name);
    }

    anim->LoadXML(node, status);
}

// VERIFY: 3cf751ed auto
// OFFSET: 0x00497800
FrameScript_Object::ScriptIx* CSimpleAnimGroup::GetScriptByName(const char* name, FrameScript_Object::ScriptData& data) {
    FrameScript_Object::ScriptIx* script = this->FrameScript_Object::GetScriptByName(name, data);

    if (script) {
        return script;
    }

    if (!SStrCmpI(name, "OnLoad", STORM_MAX_STR)) {
        return &this->m_onLoad;
    }

    if (!SStrCmpI(name, "OnPlay", STORM_MAX_STR)) {
        return &this->m_onPlay;
    }

    if (!SStrCmpI(name, "OnPause", STORM_MAX_STR)) {
        return &this->m_onPause;
    }

    if (!SStrCmpI(name, "OnStop", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,requested) %s end";
        return &this->m_onStop;
    }

    if (!SStrCmpI(name, "OnFinished", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,requested) %s end";
        return &this->m_onFinished;
    }

    if (!SStrCmpI(name, "OnUpdate", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,elapsed) %s end";
        return &this->m_onUpdate;
    }

    if (!SStrCmpI(name, "OnLoop", STORM_MAX_STR)) {
        data.m_wrapper = "return function(self,loopState) %s end";
        return &this->m_onLoop;
    }

    return nullptr;
}

// VERIFY: 9f97796a manual
// OFFSET: 0x00497920
bool CSimpleAnimGroup::IsDone() {
    if (this->m_duration <= 0.0001f) {
        return true;
    }

    if (this->m_loopType == ANIM_LOOPTYPE_REPEAT || this->m_loopType == ANIM_LOOPTYPE_BOUNCE) {
        return false;
    }

    if (this->m_loopState == ANIM_LOOPSTATE_FORWARD || this->m_loopState == ANIM_LOOPSTATE_REVERSE) {
        return false;
    }

    return this->m_progress >= 1.0f;
}

// VERIFY: 6762d495 auto
// OFFSET: 0x00497B00
void CSimpleAnimGroup::RunOnUpdateScript(float elapsedSec) {
    if (this->m_onUpdate.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushnumber(L, elapsedSec);
        this->RunScript(this->m_onUpdate, 1, nullptr);
    }
}

// VERIFY: 8423b91b manual
// OFFSET: 0x00498370
void CSimpleAnimGroup::LoadXML_Scripts(XMLNode* node, CStatus* status) {
    lua_State* L = FrameScript_GetContext();

    for (XMLNode* child = node->m_child; child; child = child->m_next) {
        const char* body = child->m_body;
        const char* scriptName = child->m_name.m_str;

        FrameScript_Object::ScriptData data;
        data.m_wrapper = "return function(self) %s end";

        FrameScript_Object::ScriptIx* script = this->GetScriptByName(scriptName, data);

        if (!script) {
            const char* name = this->GetName();
            status->Add(STATUS_WARNING, "%s %s: Unknown script element %s", this->GetObjectTypeName(), name ? name : "<unnamed>", scriptName);
            continue;
        }

        if (script->m_luaRef) {
            luaL_unref(L, LUA_REGISTRYINDEX, script->m_luaRef);
        }

        if (!child->m_userData && body && *body) {
            char functionName[1024];
            SStrPrintf(functionName, sizeof(functionName), "*:%s", scriptName);

            void* mem = SMemAlloc(sizeof(FrameScriptFunctionRef), __FILE__, __LINE__, 0);
            child->m_userData = mem ? new (mem) FrameScriptFunctionRef(FrameScript_CompileFunction(functionName, data.m_wrapper, body, status)) : nullptr;
        }

        const char* function = child->GetAttributeByName("function");

        if (function && *function) {
            lua_pushstring(L, function);
            lua_rawget(L, LUA_GLOBALSINDEX);
            int32_t ref = luaL_ref(L, LUA_REGISTRYINDEX);

            if (ref == -1) {
                const char* name = this->GetName();
                status->Add(STATUS_WARNING, "%s %s: Unknown function %s in element %s", this->GetObjectTypeName(), name ? name : "<unnamed>", function, scriptName);
                script->m_luaRef = 0;
            } else {
                script->m_luaRef = ref;
            }
        } else if (child->m_userData) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, static_cast<FrameScriptFunctionRef*>(child->m_userData)->m_function);
            script->m_luaRef = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            script->m_luaRef = 0;
        }

        script->m_tainted = nullptr;
    }
}

// VERIFY: 606632db auto
// OFFSET: 0x00498580
void CSimpleAnimGroup::Finish() {
    if (!this->IsDone() && this->m_loopState) {
        this->m_flags |= FLAG_FINISH_REQUESTED;
    }
}

// VERIFY: 2facc1a9 manual
// OFFSET: 0x004985A0
void CSimpleAnimGroup::SetInitialOffset(const C2Vector& offset) {
    if (offset.y * offset.y + offset.x * offset.x <= 0.00000023841858f) {
        this->m_initialOffset.x = 0.0f;
        this->m_initialOffset.y = 0.0f;
    } else {
        this->m_initialOffset = offset;
    }
}

// VERIFY: 0aa25956 manual
// OFFSET: 0x00498A40
int32_t CSimpleAnimGroup::GetMaxAnimOrder() {
    if (!this->m_animLists.Count()) {
        return -1;
    }

    return this->m_animLists[this->m_animLists.Count() - 1]->Head()->m_anim->m_order;
}

// VERIFY: 8a3f8fbc manual
// OFFSET: 0x00498A70
TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* CSimpleAnimGroup::FindAnimList(int32_t order) {
    if (order == -1) {
        return nullptr;
    }

    int32_t low = 0;
    int32_t high = this->m_animLists.Count() - 1;

    while (low <= high) {
        uint32_t middle = static_cast<uint32_t>(high + low) >> 1;
        TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[middle];

        if (list->Head()) {
            int32_t listOrder = list->Head()->m_anim->m_order;

            if (listOrder < order) {
                low = middle + 1;
            } else if (listOrder > order) {
                high = middle - 1;
            } else {
                return list;
            }
        }
    }

    return nullptr;
}

// VERIFY: 5b9eb9c0 manual
// OFFSET: 0x00499720
bool CSimpleAnimGroup::HasAnimOfType(int32_t type) {
    for (CSimpleAnim* anim = this->m_animations.Head(); anim; anim = this->m_animations.Next(anim)) {
        if (anim->IsA(type)) {
            return true;
        }
    }

    return false;
}

// VERIFY: 52299f12 manual
// OFFSET: 0x00499A20
TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* CSimpleAnimGroup::RegisterAnimList(int32_t order) {
    if (order == -1) {
        return nullptr;
    }

    uint32_t count = this->m_animLists.Count();

    if (!count) {
        TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >** slot = this->m_animLists.New();

        if (!slot) {
            return nullptr;
        }

        void* mem = SMemAlloc(sizeof(TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >), __FILE__, __LINE__, 0);
        *slot = mem ? new (mem) TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >() : nullptr;

        return *slot;
    }

    uint32_t index = count;

    for (uint32_t i = 0; i < count; i++) {
        TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[i];
        SIMPLEANIMNODE* head = list->Head();

        if (!head) {
            return list;
        }

        int32_t listOrder = head->m_anim->m_order;

        if (order == listOrder) {
            return list;
        }

        if (order < listOrder) {
            index = i;
            break;
        }
    }

    if (!this->m_animLists.New()) {
        return nullptr;
    }

    for (uint32_t i = this->m_animLists.Count() - 1; i > index; i--) {
        this->m_animLists[i] = this->m_animLists[i - 1];
    }

    void* mem = SMemAlloc(sizeof(TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >), __FILE__, __LINE__, 0);
    this->m_animLists[index] = mem ? new (mem) TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >() : nullptr;

    return this->m_animLists[index];
}

// VERIFY: 66e7b230 manual
// OFFSET: 0x00499EB0
CSimpleAnimGroup::CSimpleAnimGroup(CScriptRegion* owner) {
    this->m_region = nullptr;
    this->m_flags = FLAG_NONE;
    this->m_loopType = ANIM_LOOPTYPE_NONE;
    this->m_loopState = ANIM_LOOPSTATE_NONE;
    this->m_currentOrder = -1;
    this->m_elapsed = 0.0f;
    this->m_duration = 0.0f;
    this->m_progress = 0.0f;

    if (owner) {
        this->m_region = owner;
        owner->RegisterAnimGroup(this);
    }

    this->m_animations.Clear();
    this->m_animLists.Clear();
}

// VERIFY: 256df9c1 auto
// OFFSET: 0x00499FB0
CScriptObject* CSimpleAnimGroup::GetScriptObjectParent() {
    return this->m_region;
}

// VERIFY: 43ebb9ad auto
// OFFSET: 0x00499FC0
bool CSimpleAnimGroup::IsA(int32_t type) {
    return type == CSimpleAnimGroup::s_objectType || type == CScriptObject::s_objectType;
}

// VERIFY: 66a44196 auto
// OFFSET: 0x00499FF0
bool CSimpleAnimGroup::IsA(const char* typeName) {
    return !SStrCmpI(typeName, CSimpleAnimGroup::s_objectTypeName, STORM_MAX_STR) || !SStrCmpI(typeName, CScriptObject::s_objectTypeName, STORM_MAX_STR);
}

// VERIFY: dc9c3f61 auto
// OFFSET: 0x0049A040
const char* CSimpleAnimGroup::GetObjectTypeName() {
    return CSimpleAnimGroup::s_objectTypeName;
}

// VERIFY: c5ac6209 auto
// OFFSET: 0x0049A050
int32_t CSimpleAnimGroup::GetScriptMetaTable() {
    return CSimpleAnimGroup::s_metatable;
}

// VERIFY: aa705a15 manual
// OFFSET: 0x0049A060
void CSimpleAnimGroup::LoadXML(XMLNode* node, CStatus* status) {
    this->m_flags |= SIMPLEANIMGROUP_FLAG_IS_LOADING;

    const char* inherits = node->GetAttributeByName("inherits");

    if (inherits && *inherits) {
        const char* taint;
        bool locked;
        XMLNode* inheritNode = FrameXML_AcquireHashNode(inherits, taint, locked);

        if (inheritNode) {
            if (locked) {
                status->Add(STATUS_WARNING, "Recursively inherited node: %s", inherits);
            } else {
                this->LoadXML(inheritNode, status);
                FrameXML_ReleaseHashNode(inherits);
            }
        } else {
            status->Add(STATUS_WARNING, "Couldn't find inherited node: %s", inherits);
        }
    }

    const char* parentKey = node->GetAttributeByName("parentKey");

    if (parentKey && *parentKey) {
        lua_State* L = FrameScript_GetContext();
        CScriptObject* parent = this->GetScriptObjectParent();

        if (parent) {
            if (!parent->m_luaRegistered) {
                parent->RegisterScriptObject(nullptr);
            }

            lua_rawgeti(L, LUA_REGISTRYINDEX, parent->m_luaObjectRef);
            lua_pushstring(L, parentKey);

            if (!this->m_luaRegistered) {
                this->RegisterScriptObject(nullptr);
            }

            lua_rawgeti(L, LUA_REGISTRYINDEX, this->m_luaObjectRef);
            lua_settable(L, -3);
            lua_settop(L, -2);
        }
    }

    const char* looping = node->GetAttributeByName("looping");

    if (looping && *looping) {
        SimpleAnimLoopType loopType;

        if (StringToLoopType(looping, loopType)) {
            this->m_loopType = loopType;
        } else {
            const char* name = this->GetName();
            status->Add(STATUS_WARNING, "%s %s: Invalid looping value: %s", this->GetObjectTypeName(), name ? name : "<unnamed>", looping);
        }
    }

    float offsetX = 0.0f;
    float offsetY = 0.0f;

    const char* initialOffsetX = node->GetAttributeByName("initialOffsetX");

    if (initialOffsetX && *initialOffsetX) {
        float value = SStrToFloat(initialOffsetX);
        offsetX = NDCToDDCWidth(value / (CoordinateGetAspectCompensation() * 1024.0f));
    }

    const char* initialOffsetY = node->GetAttributeByName("initialOffsetY");

    if (initialOffsetY && *initialOffsetY) {
        float value = SStrToFloat(initialOffsetY);
        offsetY = NDCToDDCWidth(value / (CoordinateGetAspectCompensation() * 1024.0f));
    }

    C2Vector offset;
    offset.x = offsetX;
    offset.y = offsetY;
    this->SetInitialOffset(offset);

    for (XMLNode* child = node->m_child; child; child = child->m_next) {
        if (!SStrCmpI(child->m_name.m_str, "Animation", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleAnim), __FILE__, __LINE__, 0);

            if (!mem) {
                continue;
            }

            CSimpleAnim* anim = new (mem) CSimpleAnim(this);
            this->LoadXML_Anim(child, status, anim);
        } else if (!SStrCmpI(child->m_name.m_str, "Translation", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleTranslationAnim), __FILE__, __LINE__, 0);

            if (!mem) {
                continue;
            }

            CSimpleAnim* anim = new (mem) CSimpleTranslationAnim(this);
            this->LoadXML_Anim(child, status, anim);
        } else if (!SStrCmpI(child->m_name.m_str, "Rotation", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleRotationAnim), __FILE__, __LINE__, 0);

            if (!mem) {
                continue;
            }

            CSimpleAnim* anim = new (mem) CSimpleRotationAnim(this);
            this->LoadXML_Anim(child, status, anim);
        } else if (!SStrCmpI(child->m_name.m_str, "Scale", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleScaleAnim), __FILE__, __LINE__, 0);
            this->LoadXML_Anim(child, status, mem ? new (mem) CSimpleScaleAnim(this) : nullptr);
        } else if (!SStrCmpI(child->m_name.m_str, "Path", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimplePathAnim), __FILE__, __LINE__, 0);
            this->LoadXML_Anim(child, status, mem ? new (mem) CSimplePathAnim(this) : nullptr);
        } else if (!SStrCmpI(child->m_name.m_str, "Alpha", STORM_MAX_STR)) {
            void* mem = SMemAlloc(sizeof(CSimpleAlphaAnim), __FILE__, __LINE__, 0);
            this->LoadXML_Anim(child, status, mem ? new (mem) CSimpleAlphaAnim(this) : nullptr);
        } else if (!SStrCmpI(child->m_name.m_str, "Scripts", STORM_MAX_STR)) {
            this->LoadXML_Scripts(child, status);
        }
    }
}

// VERIFY: 215d7926 manual
// OFFSET: 0x0049A580
bool CSimpleAnimGroup::AddAnimListNode(TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list, CSimpleAnim* anim) {
    if (!list) {
        return false;
    }

    SIMPLEANIMNODE* node = list->NewNode(TSLIST_LINK_BEFORE, 0, 0);

    if (!node) {
        return false;
    }

    node->m_anim = anim;
    return true;
}

// VERIFY: 7d4527a6 manual
// OFFSET: [0x0049A700, 0x0049B0C0]
CSimpleAnimGroup::~CSimpleAnimGroup() {
    for (uint32_t i = 0; i < this->m_animLists.Count(); i++) {
        this->m_animLists[i]->Clear();
        delete this->m_animLists[i];
    }

    this->m_animLists.Clear();
    this->m_animations.Clear();
}

// VERIFY: 107e34e8 manual
// OFFSET: 0x0049A8F0
bool CSimpleAnimGroup::Play() {
    if (!this->m_animations.Head()) {
        return false;
    }

    bool notify = true;

    if (this->m_flags & FLAG_FINISHED_CALLBACKS) {
        this->m_elapsed = 0.0f;
        this->m_flags = FLAG_NONE;
        this->m_progress = 0.0f;
        this->m_loopState = ANIM_LOOPSTATE_NONE;
        this->m_currentOrder = -1;
        notify = false;
    }

    if (this->m_loopState && !(this->m_flags & FLAG_PAUSED)) {
        return true;
    }

    this->m_flags &= ~FLAG_PAUSED;

    int32_t count = this->m_animLists.Count();

    if (count > 0) {
        if (this->m_currentOrder < 0 || this->m_currentOrder >= count) {
            if (this->m_currentOrder < 0) {
                this->m_currentOrder = 0;
                this->m_loopState = ANIM_LOOPSTATE_FORWARD;
            } else {
                this->m_currentOrder = count - 1;
                this->m_loopState = ANIM_LOOPSTATE_REVERSE;
            }

            for (CSimpleAnim* anim = this->m_animations.Head(); anim; anim = this->m_animations.Next(anim)) {
                anim->m_loopState = this->m_loopState > 0 && this->m_loopState <= 2 ? this->m_loopState : 0;
                anim->m_elapsed = 0.0f;
                anim->m_playState = CSimpleAnim::STATE_STOPPED;
                anim->m_progressWithDelay = 0.0f;
                anim->m_progress = 0.0f;
            }
        }

        TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[this->m_currentOrder];

        if (this->m_loopState == ANIM_LOOPSTATE_REVERSE) {
            for (SIMPLEANIMNODE* node = list->Tail(); node; node = list->Prev(node)) {
                node->m_anim->Play();
            }
        } else {
            for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
                node->m_anim->Play();
            }
        }

        if (notify) {
            this->m_region->NotifyAnimBegin(this);
        }
    }

    if (this->m_onPlay.m_luaRef) {
        this->RunScript(this->m_onPlay, 0, nullptr);
    }

    return true;
}

// VERIFY: c6353ca3 manual
// OFFSET: 0x0049AAB0
void CSimpleAnimGroup::Pause() {
    if (!this->m_loopState || (this->m_flags & FLAG_PAUSED)) {
        return;
    }

    this->m_flags = (this->m_flags & ~FLAG_PAUSE_TRANSITION_MASK) | FLAG_PAUSED;

    TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[this->m_currentOrder];

    for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
        node->m_anim->Pause();
    }

    if (this->m_onPause.m_luaRef) {
        this->RunScript(this->m_onPause, 0, nullptr);
    }
}

// VERIFY: 2adddbf0 manual
// OFFSET: 0x0049AB60
bool CSimpleAnimGroup::NotifyAnimStopped(CSimpleAnim* anim) {
    if (!this->m_loopState || (this->m_flags & FLAG_STOPPING)) {
        return false;
    }

    for (CSimpleAnim* other = this->m_animations.Head(); other; other = this->m_animations.Next(other)) {
        if (other != anim && other->m_playState) {
            return false;
        }
    }

    this->m_flags = (this->m_flags & ~FLAG_STOP_TRANSITION_MASK) | FLAG_STOPPING;

    TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[this->m_currentOrder];

    for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
        if (node->m_anim != anim) {
            node->m_anim->Stop(false);
        }
    }

    if (this->m_onStop.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushboolean(L, false);
        this->RunScript(this->m_onStop, 1, nullptr);
    }

    this->m_region->NotifyAnimEnd(this);

    this->m_elapsed = 0.0f;
    this->m_progress = 0.0f;
    this->m_flags = FLAG_NONE;
    this->m_loopState = ANIM_LOOPSTATE_NONE;
    this->m_currentOrder = -1;

    return true;
}

// VERIFY: 3590efcc manual
// OFFSET: 0x0049AC80
void CSimpleAnimGroup::ComputeDuration() {
    this->m_duration = 0.0f;

    for (uint32_t i = 0; i < this->m_animLists.Count(); i++) {
        float duration = 0.0f;
        TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[i];

        for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
            CSimpleAnim* anim = node->m_anim;
            float total = anim->m_duration + anim->m_endDelay + anim->m_startDelay;

            if (total > duration) {
                duration = total;
            }
        }

        this->m_duration += duration;
    }
}

// VERIFY: ff60489b auto
// OFFSET: 0x0049ACF0
void CSimpleAnimGroup::NotifyAnimAdded(CSimpleAnim* anim) {
    if (!anim) {
        return;
    }

    if (this->AddAnimListNode(this->RegisterAnimList(anim->m_order), anim)) {
        this->ComputeDuration();
    }
}

// VERIFY: 0fe08e97 auto
// OFFSET: 0x0049AD30
void CSimpleAnimGroup::NotifyOrderChanged(CSimpleAnim* anim, int32_t oldOrder) {
    if (!anim) {
        return;
    }

    if (this->RemoveAnimListNode(this->FindAnimList(oldOrder), anim)) {
        if (this->AddAnimListNode(this->RegisterAnimList(anim->m_order), anim)) {
            this->ComputeDuration();
        }
    }
}

// VERIFY: 249d80ec manual
// OFFSET: 0x0049B0F0
void CSimpleAnimGroup::Stop(bool requested) {
    if (!this->m_loopState || (this->m_flags & FLAG_STOPPING)) {
        return;
    }

    this->m_flags = (this->m_flags & ~FLAG_STOP_TRANSITION_MASK) | FLAG_STOPPING;

    TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[this->m_currentOrder];

    for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
        node->m_anim->Stop(requested);
    }

    if (this->m_onStop.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushboolean(L, requested);
        this->RunScript(this->m_onStop, 1, nullptr);
    }

    this->m_region->NotifyAnimEnd(this);

    this->m_elapsed = 0.0f;
    this->m_progress = 0.0f;
    this->m_flags = FLAG_NONE;
    this->m_loopState = ANIM_LOOPSTATE_NONE;
    this->m_currentOrder = -1;
}

// VERIFY: ea4a0d4f manual
// OFFSET: 0x0049B1C0
void CSimpleAnimGroup::SortAnimations() {
    this->Stop(false);

    if (!this->m_animations.Head()) {
        for (uint32_t i = 0; i < this->m_animLists.Count(); i++) {
            this->m_animLists[i]->Clear();
            delete this->m_animLists[i];
        }

        this->m_animLists.Clear();
        return;
    }

    for (uint32_t i = 0; i < this->m_animLists.Count(); i++) {
        this->m_animLists[i]->Clear();
    }

    int32_t nextOrder = 0;

    for (CSimpleAnim* anim = this->m_animations.Head(); anim; anim = this->m_animations.Next(anim)) {
        int32_t order = anim->m_order;

        if (order == -1) {
            order = nextOrder++;
            anim->m_order = order < 0 ? 0 : order;
        } else {
            nextOrder = order + 1;
        }

        if (nextOrder > 99) {
            nextOrder = 0;
        }

        this->AddAnimListNode(this->RegisterAnimList(order), anim);
    }

    int32_t count = this->m_animLists.Count();
    int32_t last = count - 1;

    while (last >= 0 && !this->m_animLists[last]->Head()) {
        last--;
    }

    if (last < 0) {
        return;
    }

    uint32_t newCount = last + 1;

    if (static_cast<int32_t>(newCount) < count) {
        for (int32_t i = newCount; i < count; i++) {
            delete this->m_animLists[i];
        }

        this->m_animLists.SetCount(newCount);

        if (newCount) {
            this->m_animLists.ReallocData(newCount);
        } else {
            this->m_animLists.Clear();
        }
    }
}

// VERIFY: bc9ea864 manual
// OFFSET: 0x0049B470
void CSimpleAnimGroup::AdvanceLoopState(float elapsedSec, float remaining) {
    int32_t count = this->m_animLists.Count();
    bool reverse = this->m_loopState == ANIM_LOOPSTATE_REVERSE;
    int32_t step;
    int32_t first;
    int32_t end;

    if (reverse) {
        step = -1;
        first = count - 1;
        end = -1;
    } else {
        step = 1;
        first = 0;
        end = count;
    }

    this->m_currentOrder += step;

    if (this->m_currentOrder != end) {
        TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[this->m_currentOrder];

        for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
            node->m_anim->Play();
        }

        return;
    }

    bool finishing = (this->m_flags & FLAG_FINISH_REQUESTED) != 0;

    if (!this->m_loopType || finishing) {
        this->m_flags |= 0x10;

        if (this->m_onUpdate.m_luaRef) {
            lua_State* L = FrameScript_GetContext();
            lua_pushnumber(L, elapsedSec);
            this->RunScript(this->m_onUpdate, 1, nullptr);
        }

        if (this->m_onFinished.m_luaRef) {
            lua_State* L = FrameScript_GetContext();
            lua_pushboolean(L, finishing);
            this->RunScript(this->m_onFinished, 1, nullptr);
        }

        if (this->m_flags & FLAG_FINISHED_CALLBACKS) {
            this->m_region->NotifyAnimEnd(this);
            this->m_elapsed = 0.0f;
            this->m_flags = FLAG_NONE;
            this->m_progress = 0.0f;
            this->m_loopState = ANIM_LOOPSTATE_NONE;
            this->m_currentOrder = -1;
        }

        return;
    }

    if (!reverse && remaining > 0.0001f) {
        this->m_flags |= FLAG_LOOP_INVERSE_PENDING;
    }

    if (this->m_loopType == ANIM_LOOPTYPE_BOUNCE) {
        this->m_loopState = !reverse + 1;
        this->m_currentOrder = end - step;
    } else {
        this->m_currentOrder = first;
    }

    this->m_elapsed = 0.0f;
    this->m_progress = 0.0f;

    for (int32_t i = 0; i < count; i++) {
        bool current = i == this->m_currentOrder;
        TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[i];

        for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
            CSimpleAnim* anim = node->m_anim;
            anim->m_loopState = this->m_loopState > 0 && this->m_loopState <= 2 ? this->m_loopState : 0;
            anim->m_elapsed = 0.0f;
            anim->m_progressWithDelay = 0.0f;
            anim->m_playState = CSimpleAnim::STATE_STOPPED;
            anim->m_progress = 0.0f;

            if (current) {
                anim->Play();
            }
        }
    }

    if (this->m_onLoop.m_luaRef) {
        lua_State* L = FrameScript_GetContext();
        lua_pushstring(L, LoopStateToString(static_cast<SimpleAnimLoopState>(this->m_loopState)));
        this->RunScript(this->m_onLoop, 1, nullptr);
    }

    if (this->m_flags & FLAG_LOOP_INVERSE_PENDING) {
        TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[this->m_currentOrder];

        for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
            node->m_anim->ApplyInverse(1.0f);
        }

        this->m_flags &= ~FLAG_LOOP_INVERSE_PENDING;
    }
}

// VERIFY: e5ae4d1d manual
// OFFSET: 0x0049B7B0
bool CSimpleAnimGroup::RemoveAnimListNode(TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list, CSimpleAnim* anim) {
    if (!list) {
        return false;
    }

    for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
        if (node->m_anim == anim) {
            list->DeleteNode(node);

            if (!list->Head()) {
                this->SortAnimations();
            }

            return true;
        }
    }

    return false;
}

// VERIFY: b559c4d7 manual
// OFFSET: 0x0049C2A0
void CSimpleAnimGroup::PostLoadXML(XMLNode* node, CStatus* status) {
    this->m_flags ^= SIMPLEANIMGROUP_FLAG_IS_LOADING;

    for (CSimpleAnim* anim = this->m_animations.Head(); anim; anim = this->m_animations.Next(anim)) {
        anim->PostLoadXML(node, status);
    }

    this->SortAnimations();
    this->ComputeDuration();

    if (this->m_onLoad.m_luaRef) {
        this->RunScript(this->m_onLoad, 0, nullptr);
    }
}

// VERIFY: 9f86e9ee auto
// OFFSET: 0x0049C310
void CSimpleAnimGroup::NotifyAnimRemoved(CSimpleAnim* anim) {
    if (!anim) {
        return;
    }

    // SetParent can change the order first, so search every list for the node.
    for (uint32_t i = 0; i < this->m_animLists.Count(); i++) {
        if (this->RemoveAnimListNode(this->m_animLists[i], anim)) {
            this->ComputeDuration();
            return;
        }
    }
}

// VERIFY: 58c2cc26 manual
// OFFSET: 0x0049C350
void CSimpleAnimGroup::DoGroupUpdateWork(float elapsedSec) {
    bool reverse = this->m_loopState == ANIM_LOOPSTATE_REVERSE;

    if (!reverse) {
        for (int32_t i = 0; i < this->m_currentOrder; i++) {
            TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[i];

            for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
                node->m_anim->Apply(1.0f);
            }
        }

        if (this->m_currentOrder >= 0) {
            this->m_region->AddAnimTranslation(this->m_initialOffset);
        }
    }

    if (this->m_flags & FLAG_PAUSED) {
        TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[this->m_currentOrder];

        for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
            node->m_anim->Apply(node->m_anim->m_smoothProgress);
        }

        this->m_region->AddAnimTranslation(this->m_initialOffset);
    } else {
        float remaining = elapsedSec < 0.0f ? 0.0f : (elapsedSec >= 60.0f ? 60.0f : elapsedSec);

        while (true) {
            this->m_elapsed += remaining;
            this->m_progress = this->m_elapsed < this->m_duration ? this->m_elapsed / this->m_duration : 1.0f;

            float maxConsumed = 0.0f;
            bool finished = true;
            TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[this->m_currentOrder];

            if (this->m_loopState == ANIM_LOOPSTATE_REVERSE) {
                for (SIMPLEANIMNODE* node = list->Tail(); node; node = list->Prev(node)) {
                    float consumed;
                    bool done = node->m_anim->OnUpdate(remaining, consumed);

                    if (!this->m_loopState) {
                        break;
                    }

                    if (consumed > maxConsumed) {
                        maxConsumed = consumed;
                    }

                    finished = finished && done;
                }
            } else {
                for (SIMPLEANIMNODE* node = list->Head(); node; node = list->Next(node)) {
                    float consumed;
                    bool done = node->m_anim->OnUpdate(remaining, consumed);

                    if (!this->m_loopState) {
                        break;
                    }

                    if (consumed > maxConsumed) {
                        maxConsumed = consumed;
                    }

                    finished = finished && done;
                }
            }

            if (!this->m_loopState) {
                break;
            }

            remaining -= maxConsumed;

            if (finished) {
                this->AdvanceLoopState(elapsedSec, remaining);
            }

            if (remaining <= 0.0001f || this->IsDone() || !this->m_loopState || (this->m_flags & FLAG_PAUSED)) {
                break;
            }
        }
    }

    if (reverse && this->m_loopState) {
        for (int32_t i = 0; i < this->m_currentOrder; i++) {
            TSList<SIMPLEANIMNODE, TSGetLink<SIMPLEANIMNODE> >* list = this->m_animLists[i];

            for (SIMPLEANIMNODE* node = list->Tail(); node; node = list->Prev(node)) {
                node->m_anim->Apply(1.0f);
            }
        }

        if (this->m_currentOrder >= 0) {
            this->m_region->AddAnimTranslation(this->m_initialOffset);
        }
    }
}

// VERIFY: c8a40b9b auto
// OFFSET: 0x0049C760
bool CSimpleAnimGroup::OnUpdate(float elapsedSec) {
    if (this->IsDone() || !this->m_loopState) {
        return true;
    }

    this->DoGroupUpdateWork(elapsedSec);

    if (this->IsDone()) {
        return true;
    }

    this->RunOnUpdateScript(elapsedSec);
    return false;
}
