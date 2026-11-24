#include "DressUpModelFrame.h"
#include <cstdint>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/FrameScript/FrameScript_Object.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/SpellRec.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/UIUtil/Tooltip.h"
#include "WoW/Source/Ui/GameUI.h"

// VERIFY: d9ce6dd3 auto
// OFFSET: 0x00597CE0
void CGDressUpModelFrame::DestroyScriptMetaTable() {
    luaL_unref(FrameScript_GetContext(), LUA_REGISTRYINDEX, CGDressUpModelFrame::s_metatable);
    CGDressUpModelFrame::s_metatable = LUA_NOREF;
}

// VERIFY: 067c0bb2 auto
// OFFSET: 0x00597F20
void CGDressUpModelFrame::CreateScriptMetaTable() {
    CGDressUpModelFrame::s_metatable = FrameScript_Object::CreateScriptMetaTable(FrameScript_GetContext(), &CGDressUpModelFrame::RegisterScriptMethods);
}

// VERIFY: 4167113a manual
// OFFSET: 0x00597E90
static int32_t Script_Undress(lua_State* L) {
    CGDressUpModelFrame* frame = static_cast<CGDressUpModelFrame*>(FrameScript_GetObjectThis(L, CGDressUpModelFrame::GetObjectType()));
    frame->Undress();
    return 0;
}

// VERIFY: de21a638 manual
// OFFSET: 0x00597ED0
static int32_t Script_Dress(lua_State* L) {
    CGDressUpModelFrame* frame = static_cast<CGDressUpModelFrame*>(FrameScript_GetObjectThis(L, CGDressUpModelFrame::GetObjectType()));
    frame->RefreshUnit(frame->m_unitGuid);
    return 0;
}

// VERIFY: 9c209306 auto
// OFFSET: 0x00598830
static int32_t Script_TryOn(lua_State* L) {
    CGDressUpModelFrame* frame = static_cast<CGDressUpModelFrame*>(FrameScript_GetObjectThis(L, CGDressUpModelFrame::GetObjectType()));

    if (lua_isnumber(L, 2)) {
        frame->TryOn(static_cast<int32_t>(lua_tonumber(L, 2)), nullptr, -1);
        return 0;
    }

    if (lua_isstring(L, 2)) {
        const char* link = lua_tolstring(L, 2, nullptr);

        if (SStrStr(link, "item:")) {
            ItemLinkInfo info;
            info.Clear();
            info.GetItemIdFromLink(link);
            frame->TryOn(info.m_itemId, info.m_enchants, -1);
            return 0;
        }

        if (SStrStr(link, "enchant:")) {
            const char* enchant = SStrStr(link, "enchant:");

            if (enchant) {
                int32_t spellId = SStrToInt(enchant + 8);

                if (spellId > 0) {
                    SpellRec spell;

                    if (g_spellDB.GetRecord(spellId, &spell) && (spell.m_attributes & SPELL_ATTR0_IS_TRADESKILL) && spell.m_effectItemType[0]) {
                        frame->TryOn(spell.m_effectItemType[0], nullptr, -1);
                        return 0;
                    }
                }
            }
        } else {
            CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
            CGItem_C* item = player ? player->m_bag.FindItemByFullName(lua_tolstring(L, 2, nullptr), 0) : nullptr;

            if (item) {
                frame->TryOnItem(item, -1);
                return 0;
            }
        }
    }

    CGGameUI::DisplayError(ERR_NOT_EQUIPPABLE);
    return 0;
}

// STATIC: 0x00ACF550
static FrameScript_Method s_ScriptMethods[] = {
    { "Undress", &Script_Undress },
    { "Dress", &Script_Dress },
    { "TryOn", &Script_TryOn },
};

// VERIFY: 9ed91f35 auto
// OFFSET: 0x00597CC0
void CGDressUpModelFrame::RegisterScriptMethods(lua_State* L) {
    CGCharacterModelBase::RegisterScriptMethods(L);
    FrameScript_Object::FillScriptMethodTable(L, s_ScriptMethods, sizeof(s_ScriptMethods) / sizeof(s_ScriptMethods[0]));
}
