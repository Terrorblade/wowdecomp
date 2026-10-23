#include "HoT/Lua/Builtins.h"
#include <cstring>
#include "Engine/Source/Base/Coordinate.h"
#include "Engine/Source/lua-5.1/src/lauxlib.h"
#include "HoT/Core/Modules.h"
#include "HoT/Core/Shell.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/Bag_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ActionBarFrame.h"
#include "WoW/Source/Ui/ContainerFrame.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/QuestTextParser.h"
#include "WoW/Source/Ui/WorldFrame.h"

namespace HoT::Builtins {

static const uint32_t ITEM_SPELLTRIGGER_ON_SOCKET = 3;
static const int32_t SPELL_AURA_TEMP_LEARN_SPELL = 317;
static const int32_t NUM_ACTION_SLOTS = sizeof(CGActionBar::m_slotActions) / sizeof(CGActionBar::m_slotActions[0]);
static const int32_t NUM_LFG_AFFIXES = 4;

static int32_t Script_FlashGameWindow(lua_State* L) {
    Shell::FlashGameWindow();
    return 0;
}

static int32_t Script_OpenUrl(lua_State* L) {
    static const char* const s_prefixes[] = { "https://houroftwilight.net", "http://houroftwilight.net" };

    const char* url = lua_tolstring(L, 1, nullptr);

    if (!url || !*url || strpbrk(url, "<>\"'`|&;${}()[]\\")) {
        return 0;
    }

    for (const char* prefix : s_prefixes) {
        size_t length = strlen(prefix);

        if (!strncmp(url, prefix, length) && strchr("/?#", url[length])) {
            Shell::OpenUrl(url);
            break;
        }
    }

    return 0;
}

static int32_t Script_GetShapeshiftFormID(lua_State* L) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    lua_pushnumber(L, player ? player->GetShapeshiftForm() : 0);
    return 1;
}

static int32_t Script_GetSpellDescription(lua_State* L) {
    SpellRec spell;

    if (lua_isnumber(L, 1) && g_spellDB.GetRecord(static_cast<int32_t>(lua_tonumber(L, 1)), &spell)) {
        char description[1024];
        SpellParserParseText(&spell, description, sizeof(description), 0, 0, 0, 0, 1, 0);
        lua_pushstring(L, description);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int32_t Script_GetSpellNameById(lua_State* L) {
    SpellRec spell;

    if (lua_isnumber(L, 1) && g_spellDB.GetRecord(static_cast<int32_t>(lua_tonumber(L, 1)), &spell)) {
        lua_pushstring(L, spell.m_name);
        lua_pushstring(L, spell.m_nameSubtext);
    } else {
        lua_pushnil(L);
        lua_pushnil(L);
    }

    return 2;
}

static int32_t Script_GetCurrentMapId(lua_State* L) {
    lua_pushnumber(L, CGGameUI::m_iCurrentMapID);
    return 1;
}

static int32_t Script_GetMapNameById(lua_State* L) {
    MapRec* map = lua_isnumber(L, 1) ? g_mapDB.GetRecord(static_cast<int32_t>(lua_tonumber(L, 1))) : nullptr;

    if (map && map->m_mapName) {
        lua_pushstring(L, map->m_mapName);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static uint32_t GemColorFromSubClass(uint32_t subClass) {
    switch (subClass) {
        case 8:
            return 126;
        case 9:
            return 2;
        case 10:
            return 8;
        case 11:
            return 4;
        case 12:
            return 64;
        default:
            return 0;
    }
}

// Returns false while the item is still being queried, the caller retries.
static bool ResolveGemDisplay(int32_t itemId, uint32_t& spellId, uint32_t& gemColor, const char*& icon) {
    spellId = 0;
    gemColor = 0;
    icon = nullptr;

    ItemStats_C* stats = g_itemDBCache.GetRecord(itemId, WOWGUID(), nullptr, nullptr, false);

    if (!stats) {
        return false;
    }

    uint32_t triggerSpell = 0;

    for (int32_t i = 0; i < 5 && !triggerSpell; i++) {
        if (stats->m_spellTrigger[i] == ITEM_SPELLTRIGGER_ON_SOCKET && static_cast<int32_t>(stats->m_spellId[i]) > 0) {
            triggerSpell = stats->m_spellId[i];
        }
    }

    if (!triggerSpell) {
        return false;
    }

    gemColor = GemColorFromSubClass(stats->m_subClass);
    spellId = triggerSpell;

    SpellRec spell;

    if (g_spellDB.GetRecord(triggerSpell, &spell)) {
        for (int32_t i = 0; i < 3; i++) {
            if (spell.m_effectAura[i] == SPELL_AURA_TEMP_LEARN_SPELL && spell.m_effectTriggerSpell[i]) {
                spellId = spell.m_effectTriggerSpell[i];
                break;
            }
        }
    }

    if (g_spellDB.GetRecord(spellId, &spell)) {
        SpellIconRec* iconRec = g_spellIconDB.GetRecord(spell.m_spellIconID);
        icon = iconRec ? iconRec->m_textureFilename : nullptr;
    }

    return true;
}

static int32_t Script_GetGemSpellInfo(lua_State* L) {
    uint32_t spellId = 0;
    uint32_t gemColor = 0;
    const char* icon = nullptr;

    if (!lua_isnumber(L, 1)) {
        lua_pushnil(L);
        lua_pushnil(L);
        lua_pushnil(L);
        return 3;
    }

    lua_pushboolean(L, ResolveGemDisplay(static_cast<int32_t>(lua_tonumber(L, 1)), spellId, gemColor, icon));

    if (spellId) {
        lua_pushnumber(L, spellId);
    } else {
        lua_pushnil(L);
    }

    if (icon) {
        lua_pushstring(L, icon);
    } else {
        lua_pushnil(L);
    }

    return 3;
}

static int32_t Script_GetGemType(lua_State* L) {
    uint32_t spellId = 0;
    uint32_t gemColor = 0;
    const char* icon = nullptr;

    if (!lua_isnumber(L, 1) || !ResolveGemDisplay(static_cast<int32_t>(lua_tonumber(L, 1)), spellId, gemColor, icon)) {
        lua_pushnil(L);
        return 1;
    }

    switch (gemColor) {
        case 1:
        case 126:
            lua_pushstring(L, "Prismatic");
            break;
        case 2:
            lua_pushstring(L, "Red");
            break;
        case 4:
            lua_pushstring(L, "Yellow");
            break;
        case 8:
            lua_pushstring(L, "Blue");
            break;
        case 16:
            lua_pushstring(L, "Orange");
            break;
        case 32:
            lua_pushstring(L, "Purple");
            break;
        case 64:
            lua_pushstring(L, "Green");
            break;
        default:
            lua_pushstring(L, "Gem");
            break;
    }

    return 1;
}

static int32_t Script_ConvertCoordsToScreenSpace(lua_State* L) {
    C3Vector world;
    world.x = static_cast<float>(lua_tonumber(L, 1));
    world.y = static_cast<float>(lua_tonumber(L, 2));
    world.z = static_cast<float>(lua_tonumber(L, 3));

    C3Vector screen = {};
    uint32_t result = 0;

    if (CGWorldFrame::s_currentWorldFrame) {
        CGWorldFrame::s_currentWorldFrame->GetScreenCoordinates(world, &screen, &result);
    }

    // The DLL scaled both axes by the width factor, addons are tuned to that.
    float scale = CoordinateGetAspectCompensation() * 1024.0f;
    lua_pushnumber(L, DDCToNDCWidth(screen.x) * scale);
    lua_pushnumber(L, DDCToNDCWidth(screen.y) * scale);
    lua_pushnumber(L, screen.z);
    return 3;
}

static int32_t Script_FindSpellActionBarSlots(lua_State* L) {
    uint32_t spellId = static_cast<uint32_t>(lua_tonumber(L, 1));
    int32_t count = 0;

    for (int32_t i = 0; i < NUM_ACTION_SLOTS; i++) {
        if (CGActionBar::m_slotActions[i] == spellId) {
            lua_pushnumber(L, i);
            count++;
        }
    }

    if (!count) {
        lua_pushnil(L);
        return 1;
    }

    return count;
}

static int32_t Script_ReplaceActionBarSpell(lua_State* L) {
    uint32_t oldSpellId = static_cast<uint32_t>(lua_tonumber(L, 1));
    uint32_t newSpellId = static_cast<uint32_t>(lua_tonumber(L, 2));

    for (int32_t i = 0; i < NUM_ACTION_SLOTS; i++) {
        if (CGActionBar::m_slotActions[i] == oldSpellId) {
            CGActionBar::m_slotActions[i] = newSpellId;
            CGActionBar::SlotChanged(i, 1, 0);
        }
    }

    return 0;
}

static int32_t Script_SetSpellInActionBarSlot(lua_State* L) {
    uint32_t spellId = static_cast<uint32_t>(lua_tonumber(L, 1));
    int32_t slot = static_cast<int32_t>(lua_tonumber(L, 2));

    if (slot >= 0 && slot < NUM_ACTION_SLOTS) {
        CGActionBar::m_slotActions[slot] = spellId;
        CGActionBar::SlotChanged(slot, 1, 0);
    }

    return 0;
}

static int32_t Script_CustomLfgQueue(lua_State* L) {
    uint32_t role = static_cast<uint32_t>(lua_tonumber(L, 1));
    const char* selections = lua_tolstring(L, 2, nullptr);
    bool premadeWait = lua_tonumber(L, 3) == 1;

    Net::Packet packet(Net::CMSG_CUSTOM_LFG_QUEUE);
    packet.PutUInt32(role);
    packet.PutUInt8(premadeWait ? 1 : 0);

    for (int32_t i = 0; i < NUM_LFG_AFFIXES; i++) {
        packet.PutUInt32(static_cast<uint32_t>(lua_tonumber(L, 4 + i)));
    }

    packet.PutString(selections ? selections : "");
    packet.Send();
    return 0;
}

// Takes nothing for the cursor item, (bag, slot), or an item id or name searched in the backpack.
static CGItem_C* GetItemFromLuaArg(lua_State* L) {
    if (lua_gettop(L) == 0 || lua_type(L, 1) == LUA_TNIL) {
        WOWGUID guid = CGGameUI::GetCursorItem();
        return guid ? static_cast<CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPE_ITEM, __FILE__, __LINE__)) : nullptr;
    }

    if (lua_gettop(L) >= 2 && lua_isnumber(L, 1) && lua_isnumber(L, 2)) {
        CGBag_C* bag = nullptr;
        int32_t slot = 0;
        int32_t bank = 0;

        if (!CGContainerInfo::GetLuaBagAndSlot(L, &bag, &slot, &bank) || !bag) {
            return nullptr;
        }

        return bag->GetItemPointer(slot);
    }

    int32_t itemId = 0;

    if (lua_isnumber(L, 1)) {
        itemId = static_cast<int32_t>(lua_tonumber(L, 1));
    } else {
        const char* name = lua_tolstring(L, 1, nullptr);
        itemId = name && *name ? GetItemIDByName(name) : 0;
    }

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!itemId || !player) {
        return nullptr;
    }

    return player->m_bag.FindItemOfType(itemId, 0xA1);
}

static int32_t Script_LockBagItem(lua_State* L) {
    if (CGItem_C* item = GetItemFromLuaArg(L)) {
        item->Lock();
    }

    return 0;
}

static int32_t Script_UnlockBagItem(lua_State* L) {
    if (CGItem_C* item = GetItemFromLuaArg(L)) {
        item->Unlock();
    }

    return 0;
}

static int32_t Script_SendSanityCheck(lua_State* L) {
    Session::SendSanityCheck(true);
    return 0;
}

class BuiltinsModule : public Module {
    public:
    const char* Name() const override {
        return "Builtins";
    }

    void OnInitializeGlobal() override {
        Lua::RegisterFunction("FlashGameWindow", Script_FlashGameWindow, Lua::State::Any);
        Lua::RegisterFunction("OpenUrl", Script_OpenUrl, Lua::State::Any);
        Lua::RegisterFunction("GetShapeshiftFormID", Script_GetShapeshiftFormID);
        Lua::RegisterFunction("GetSpellDescription", Script_GetSpellDescription);
        Lua::RegisterFunction("GetSpellNameById", Script_GetSpellNameById);
        Lua::RegisterFunction("GetMapNameById", Script_GetMapNameById);
        Lua::RegisterFunction("GetCurrentMapId", Script_GetCurrentMapId);
        Lua::RegisterFunction("GetGemSpellInfo", Script_GetGemSpellInfo);
        Lua::RegisterFunction("GetGemType", Script_GetGemType);
        Lua::RegisterFunction("ConvertCoordsToScreenSpace", Script_ConvertCoordsToScreenSpace);
        Lua::RegisterFunction("FindSpellActionBarSlots", Script_FindSpellActionBarSlots);
        Lua::RegisterFunction("ReplaceActionBarSpell", Script_ReplaceActionBarSpell);
        Lua::RegisterFunction("SetSpellInActionBarSlot", Script_SetSpellInActionBarSlot);
        Lua::RegisterFunction("CustomLfgQueue", Script_CustomLfgQueue);
        Lua::RegisterFunction("LockBagItem", Script_LockBagItem);
        Lua::RegisterFunction("UnlockBagItem", Script_UnlockBagItem);
        Lua::RegisterFunction("SendSanityCheck", Script_SendSanityCheck);
    }
};

Module* GetModule() {
    static BuiltinsModule module;
    return &module;
}

}
