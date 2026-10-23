#include <unordered_map>
#include <vector>
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Tempest/C3Vector.h"
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/Bag_C.h"
#include "WoW/Source/Object/ObjectClient/Container_C.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/ContainerFrame.h"
#include "WoW/Source/Ui/GameUI.h"

namespace HoT::Crafting {

static constexpr uint32_t MAX_CATALYSTS = 2;
static constexpr uint32_t MAX_SCRAP_ITEMS = 9;
static constexpr int32_t BACKPACK_FIRST_INDEX = 23;
static constexpr int32_t BACKPACK_LAST_INDEX = 38;
static constexpr int32_t MAX_EQUIPPED_BAGS = 4;
static constexpr uint32_t RANGE_CHECK_INTERVAL_MS = 500;
static constexpr uint32_t PENDING_TIMEOUT_MS = 15000;
static constexpr uint8_t SCRAP_ITEM_SKIPPED = 3;
static constexpr float RANGE_TOLERANCE = 1.5f;
static constexpr uint8_t OPEN_FLAG_CAN_CRAFT = 0x01;
static constexpr uint8_t RESULT_TIMED_OUT = 255;

enum Mode : uint8_t {
    MODE_CRAFTING = 0,
    MODE_SCRAPPING = 1,
};

enum ErrorCode : uint8_t {
    CRAFT_ERR_SLOTS_FULL = 1,
    CRAFT_ERR_ALREADY_ADDED = 2,
    CRAFT_ERR_NOT_A_CATALYST = 3,
    CRAFT_ERR_CANNOT_SCRAP = 4,
    CRAFT_ERR_ITEM_LOCKED = 5,
    CRAFT_ERR_BUSY = 6,
    CRAFT_ERR_CATALYST_CONFLICT = 7,
};

enum CatalystType : uint8_t {
    CATALYST_MIN_QUALITY = 1,
    CATALYST_STAT_GROUP = 2,
    CATALYST_LEVEL_BOOST = 3,
    CATALYST_TALENT_NOTABLE = 4,
};

struct PatternInfo {
    uint32_t m_entry;
    uint8_t m_category;
};

struct MaterialInfo {
    uint32_t m_entry;
    uint8_t m_category;
};

struct RecipeInfo {
    uint32_t m_pattern;
    uint32_t m_material;
    uint32_t m_result;
    uint32_t m_scrapCost;
};

struct CatalystInfo {
    uint32_t m_entry;
    uint8_t m_type;
    int32_t m_value1;
    int32_t m_value2;
};

struct ItemRef {
    uint64_t m_guid = 0;
    uint32_t m_entry = 0;
    uint32_t m_count = 0;
    int32_t m_bag = -1;
    int32_t m_slot = -1;
};

static uint32_t s_dataVersion;
static uint32_t s_scrapCurrency;
static float s_interactRange;
static std::vector<PatternInfo> s_patterns;
static std::vector<MaterialInfo> s_materials;
static std::vector<RecipeInfo> s_recipes;
static std::vector<CatalystInfo> s_catalystList;
static std::unordered_map<uint64_t, RecipeInfo> s_recipeLookup;
static std::unordered_map<uint32_t, CatalystInfo> s_catalystLookup;

static bool s_open;
static uint64_t s_station;
static uint8_t s_flags;
static Mode s_mode = MODE_CRAFTING;
static uint32_t s_pattern;
static uint32_t s_material;
static uint64_t s_catalysts[MAX_CATALYSTS];
static uint64_t s_scrapItems[MAX_SCRAP_ITEMS];
static bool s_craftPending;
static uint32_t s_craftPendingUntil;
static bool s_scrapPending;
static uint32_t s_scrapPendingUntil;
static uint32_t s_nextRangeCheck;

static uint64_t MakeRecipeKey(uint32_t pattern, uint32_t material) {
    return (static_cast<uint64_t>(pattern) << 32) | material;
}

static int32_t OptionalSlotArg(lua_State* L, int32_t index, int32_t maxSlots) {
    if (lua_gettop(L) < index || !lua_isnumber(L, index)) {
        return -1;
    }

    int32_t slot = static_cast<int32_t>(lua_tonumber(L, index)) - 1;

    if (slot < 0 || slot >= maxSlots) {
        return -1;
    }

    return slot;
}

static CGItem_C* GetItem(uint64_t guid) {
    if (!guid) {
        return nullptr;
    }

    return static_cast<CGItem_C*>(ClntObjMgrObjectPtr(WOWGUID{ guid }, TYPE_ITEM, __FILE__, __LINE__));
}

static void LockItem(uint64_t guid) {
    if (guid) {
        CGGameUI::LockItem(WOWGUID{ guid });
    }
}

static void UnlockItem(uint64_t guid) {
    if (guid) {
        CGGameUI::UnlockItem(WOWGUID{ guid });
    }
}

static void SignalError(ErrorCode code) {
    Lua::SignalEvent("HOT_CRAFTING_ERROR", "%d", static_cast<int32_t>(code));
}

static void SignalSlots(Mode mode) {
    Lua::SignalEvent("HOT_CRAFTING_SLOTS_UPDATE", "%d", static_cast<int32_t>(mode));
}

static bool FindItemBagSlot(uint64_t guid, int32_t& outBag, int32_t& outSlot) {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player) {
        return false;
    }

    for (int32_t index = BACKPACK_FIRST_INDEX; index <= BACKPACK_LAST_INDEX; index++) {
        CGItem_C* item = player->m_bag.GetItemPointer(index);

        if (item && item->m_obj->m_guid.m_guid == guid) {
            outBag = 0;
            outSlot = index - BACKPACK_FIRST_INDEX + 1;
            return true;
        }
    }

    for (int32_t bag = 0; bag < MAX_EQUIPPED_BAGS; bag++) {
        CGContainer_C* container = static_cast<CGContainer_C*>(ClntObjMgrObjectPtr(GetBagAtIndex(bag), TYPE_CONTAINER, __FILE__, __LINE__));

        if (!container) {
            continue;
        }

        CGBag_C* contents = container->GetBag();

        if (!contents) {
            continue;
        }

        for (int32_t index = 0; index < contents->m_numSlots; index++) {
            CGItem_C* item = contents->GetItemPointer(index);

            if (item && item->m_obj->m_guid.m_guid == guid) {
                outBag = bag + 1;
                outSlot = index + 1;
                return true;
            }
        }
    }

    return false;
}

static bool ResolveItem(uint64_t guid, ItemRef& out) {
    out = ItemRef();
    CGItem_C* item = GetItem(guid);

    if (!item) {
        return false;
    }

    out.m_guid = guid;
    out.m_entry = item->m_obj->m_entryID;
    out.m_count = static_cast<uint32_t>(item->m_item->m_itemFieldStackCount);
    FindItemBagSlot(guid, out.m_bag, out.m_slot);
    return true;
}

static bool IsScrapCandidate(uint32_t entry) {
    int32_t itemClass;

    if (ItemRec* record = g_itemDB.GetRecord(static_cast<int32_t>(entry))) {
        itemClass = record->m_classID;
    } else if (ItemStats_C* stats = g_itemDBCache.GetRecord(static_cast<int32_t>(entry), WOWGUID(), nullptr, nullptr, false)) {
        itemClass = static_cast<int32_t>(stats->m_class);
    } else {
        return true;
    }

    switch (itemClass) {
        case 0:
        case 1:
        case 6:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 16:
            return false;

        default:
            return true;
    }
}

static const CatalystInfo* FindCatalyst(uint32_t entry) {
    auto it = s_catalystLookup.find(entry);
    return it != s_catalystLookup.end() ? &it->second : nullptr;
}

static bool IsCatalystEntry(uint32_t entry) {
    return s_catalystLookup.find(entry) != s_catalystLookup.end();
}

static bool StacksWithOtherCatalysts(uint32_t entry, int32_t ignoreSlot) {
    const CatalystInfo* info = FindCatalyst(entry);

    if (!info) {
        return false;
    }

    if (info->m_type == CATALYST_LEVEL_BOOST) {
        return true;
    }

    for (uint32_t i = 0; i < MAX_CATALYSTS; i++) {
        if (static_cast<int32_t>(i) == ignoreSlot || !s_catalysts[i]) {
            continue;
        }

        ItemRef other;

        if (!ResolveItem(s_catalysts[i], other)) {
            continue;
        }

        const CatalystInfo* otherInfo = FindCatalyst(other.m_entry);

        if (otherInfo && otherInfo->m_type == info->m_type) {
            return false;
        }
    }

    return true;
}

static bool HasCatalyst(uint64_t guid) {
    for (uint64_t slotGuid : s_catalysts) {
        if (slotGuid && slotGuid == guid) {
            return true;
        }
    }

    return false;
}

static bool HasScrapItem(uint64_t guid) {
    for (uint64_t slotGuid : s_scrapItems) {
        if (slotGuid && slotGuid == guid) {
            return true;
        }
    }

    return false;
}

static bool PlaceCatalyst(uint64_t guid, uint32_t entry, int32_t preferredSlot) {
    if (s_craftPending || s_scrapPending) {
        SignalError(CRAFT_ERR_BUSY);
        return false;
    }

    if (!IsCatalystEntry(entry)) {
        SignalError(CRAFT_ERR_NOT_A_CATALYST);
        return false;
    }

    if (HasCatalyst(guid)) {
        SignalError(CRAFT_ERR_ALREADY_ADDED);
        return false;
    }

    int32_t slot = -1;

    if (preferredSlot >= 0 && preferredSlot < static_cast<int32_t>(MAX_CATALYSTS)) {
        slot = preferredSlot;
    } else {
        for (uint32_t i = 0; i < MAX_CATALYSTS; i++) {
            if (!s_catalysts[i]) {
                slot = static_cast<int32_t>(i);
                break;
            }
        }
    }

    if (slot < 0) {
        SignalError(CRAFT_ERR_SLOTS_FULL);
        return false;
    }

    if (!StacksWithOtherCatalysts(entry, slot)) {
        SignalError(CRAFT_ERR_CATALYST_CONFLICT);
        return false;
    }

    if (s_catalysts[slot]) {
        UnlockItem(s_catalysts[slot]);
    }

    s_catalysts[slot] = guid;
    SignalSlots(MODE_CRAFTING);
    return true;
}

static bool PlaceScrapItem(uint64_t guid, uint32_t entry, int32_t preferredSlot) {
    if (s_craftPending || s_scrapPending) {
        SignalError(CRAFT_ERR_BUSY);
        return false;
    }

    if (!IsScrapCandidate(entry)) {
        SignalError(CRAFT_ERR_CANNOT_SCRAP);
        return false;
    }

    if (HasScrapItem(guid)) {
        SignalError(CRAFT_ERR_ALREADY_ADDED);
        return false;
    }

    int32_t slot = -1;

    if (preferredSlot >= 0 && preferredSlot < static_cast<int32_t>(MAX_SCRAP_ITEMS)) {
        slot = preferredSlot;
    } else {
        for (uint32_t i = 0; i < MAX_SCRAP_ITEMS; i++) {
            if (!s_scrapItems[i]) {
                slot = static_cast<int32_t>(i);
                break;
            }
        }
    }

    if (slot < 0) {
        SignalError(CRAFT_ERR_SLOTS_FULL);
        return false;
    }

    if (s_scrapItems[slot]) {
        UnlockItem(s_scrapItems[slot]);
    }

    s_scrapItems[slot] = guid;
    SignalSlots(MODE_SCRAPPING);
    return true;
}

static bool AddCursorItem(Mode mode, int32_t preferredSlot) {
    if (!s_open) {
        return false;
    }

    uint64_t guid = CGGameUI::GetCursorItem().m_guid;
    CGItem_C* item = GetItem(guid);

    if (!item) {
        return false;
    }

    uint32_t entry = item->m_obj->m_entryID;
    bool placed = mode == MODE_CRAFTING ? PlaceCatalyst(guid, entry, preferredSlot) : PlaceScrapItem(guid, entry, preferredSlot);

    if (!placed) {
        return false;
    }

    CGGameUI::ClearCursor(1, 1);
    LockItem(guid);
    return true;
}

static void UnlockAllSlots() {
    for (uint64_t guid : s_catalysts) {
        UnlockItem(guid);
    }

    for (uint64_t guid : s_scrapItems) {
        UnlockItem(guid);
    }
}

static void ClearSlots() {
    for (uint64_t& guid : s_catalysts) {
        guid = 0;
    }

    for (uint64_t& guid : s_scrapItems) {
        guid = 0;
    }
}

static void CloseInternal(bool notifyServer, bool signal) {
    if (!s_open) {
        return;
    }

    UnlockAllSlots();
    ClearSlots();
    s_open = false;
    s_station = 0;
    s_flags = 0;
    s_craftPending = false;
    s_scrapPending = false;

    if (notifyServer) {
        Net::Packet(Net::CMSG_CRAFTING_CLOSE).Send();
    }

    if (signal) {
        Lua::SignalEvent("HOT_CRAFTING_CLOSE", nullptr);
    }
}

static void Reset() {
    s_open = false;
    s_station = 0;
    s_flags = 0;
    s_mode = MODE_CRAFTING;
    s_pattern = 0;
    s_material = 0;
    s_craftPending = false;
    s_scrapPending = false;
    ClearSlots();
}

static bool SendCraft() {
    if (!s_open || s_craftPending || s_scrapPending) {
        return false;
    }

    if (!s_pattern || !s_material || !(s_flags & OPEN_FLAG_CAN_CRAFT)) {
        return false;
    }

    if (s_recipeLookup.find(MakeRecipeKey(s_pattern, s_material)) == s_recipeLookup.end()) {
        return false;
    }

    Net::Packet packet(Net::CMSG_CRAFTING_CRAFT);
    packet.PutUInt32(s_pattern);
    packet.PutUInt32(s_material);
    packet.PutUInt64(s_catalysts[0]);
    packet.PutUInt64(s_catalysts[1]);
    packet.Send();

    s_craftPending = true;
    s_craftPendingUntil = OsGetAsyncTimeMs() + PENDING_TIMEOUT_MS;
    return true;
}

static bool SendScrap() {
    if (!s_open || s_craftPending || s_scrapPending) {
        return false;
    }

    uint8_t count = 0;

    for (uint64_t guid : s_scrapItems) {
        if (guid) {
            count++;
        }
    }

    if (!count) {
        return false;
    }

    Net::Packet packet(Net::CMSG_CRAFTING_SCRAP);
    packet.PutUInt8(count);

    for (uint64_t guid : s_scrapItems) {
        if (guid) {
            packet.PutUInt64(guid);
        }
    }

    packet.Send();

    s_scrapPending = true;
    s_scrapPendingUntil = OsGetAsyncTimeMs() + PENDING_TIMEOUT_MS;
    return true;
}

static void CheckStationRange() {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    CGObject_C* station = s_station ? ClntObjMgrObjectPtr(WOWGUID{ s_station }, TYPE_GAMEOBJECT, __FILE__, __LINE__) : nullptr;

    if (!player || !station || C3Vector::Distance(player->GetPosition(), station->GetPosition()) > s_interactRange + RANGE_TOLERANCE) {
        CloseInternal(true, true);
        return;
    }

    bool catalystsChanged = false;

    for (uint64_t& guid : s_catalysts) {
        if (guid && !GetItem(guid)) {
            guid = 0;
            catalystsChanged = true;
        }
    }

    bool scrapChanged = false;

    for (uint64_t& guid : s_scrapItems) {
        if (guid && !GetItem(guid)) {
            guid = 0;
            scrapChanged = true;
        }
    }

    if (catalystsChanged) {
        SignalSlots(MODE_CRAFTING);
    }

    if (scrapChanged) {
        SignalSlots(MODE_SCRAPPING);
    }
}

static void CheckPendingTimeouts() {
    uint32_t now = OsGetAsyncTimeMs();

    if (s_craftPending && static_cast<int32_t>(now - s_craftPendingUntil) >= 0) {
        s_craftPending = false;
        Lua::SignalEvent("HOT_CRAFTING_RESULT", "%d%d", static_cast<int32_t>(RESULT_TIMED_OUT), 0);
    }

    if (s_scrapPending && static_cast<int32_t>(now - s_scrapPendingUntil) >= 0) {
        s_scrapPending = false;
        Lua::SignalEvent("HOT_CRAFTING_SCRAP_RESULT", "%d%d%d", static_cast<int32_t>(RESULT_TIMED_OUT), 0, 0);
    }
}

static void Tick() {
    if (!s_open) {
        return;
    }

    // The DLL ticked from world render, so nothing runs without a player in the world.
    if (!ClntObjMgrGetActivePlayerObj()) {
        return;
    }

    uint32_t now = OsGetAsyncTimeMs();

    if (static_cast<int32_t>(now - s_nextRangeCheck) >= 0) {
        s_nextRangeCheck = now + RANGE_CHECK_INTERVAL_MS;
        CheckStationRange();

        if (!s_open) {
            return;
        }
    }

    CheckPendingTimeouts();
}

static int32_t Packet_SMSG_CRAFTING_DATA(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);

    s_dataVersion = packet.GetUInt32();
    s_scrapCurrency = packet.GetUInt32();
    s_interactRange = packet.GetFloat();

    s_patterns.clear();
    s_materials.clear();
    s_recipes.clear();
    s_catalystList.clear();
    s_recipeLookup.clear();
    s_catalystLookup.clear();

    uint8_t patternCount = packet.GetUInt8();
    s_patterns.reserve(patternCount);

    for (uint32_t i = 0; i < patternCount && packet.CanRead(1); i++) {
        PatternInfo info;
        info.m_entry = packet.GetUInt32();
        info.m_category = packet.GetUInt8();
        s_patterns.push_back(info);
    }

    uint8_t materialCount = packet.GetUInt8();
    s_materials.reserve(materialCount);

    for (uint32_t i = 0; i < materialCount && packet.CanRead(1); i++) {
        MaterialInfo info;
        info.m_entry = packet.GetUInt32();
        info.m_category = packet.GetUInt8();
        s_materials.push_back(info);
    }

    uint16_t recipeCount = packet.GetUInt16();
    s_recipes.reserve(recipeCount);

    for (uint32_t i = 0; i < recipeCount && packet.CanRead(1); i++) {
        RecipeInfo info;
        info.m_pattern = packet.GetUInt32();
        info.m_material = packet.GetUInt32();
        info.m_result = packet.GetUInt32();
        info.m_scrapCost = packet.GetUInt32();
        s_recipes.push_back(info);
        s_recipeLookup[MakeRecipeKey(info.m_pattern, info.m_material)] = info;
    }

    uint8_t catalystCount = packet.GetUInt8();
    s_catalystList.reserve(catalystCount);

    for (uint32_t i = 0; i < catalystCount && packet.CanRead(1); i++) {
        CatalystInfo info;
        info.m_entry = packet.GetUInt32();
        info.m_type = packet.GetUInt8();
        info.m_value1 = packet.GetInt32();
        info.m_value2 = packet.GetInt32();
        s_catalystList.push_back(info);
        s_catalystLookup[info.m_entry] = info;
    }

    Lua::SignalEvent("HOT_CRAFTING_DATA_UPDATE", "%d", static_cast<int32_t>(s_dataVersion));
    return 1;
}

static int32_t Packet_SMSG_CRAFTING_OPEN(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint64_t station = packet.GetUInt64();
    packet.GetUInt32();
    uint8_t flags = packet.GetUInt8();

    if (s_open) {
        UnlockAllSlots();
    }

    ClearSlots();
    s_open = true;
    s_station = station;
    s_flags = flags;
    s_mode = MODE_CRAFTING;
    s_pattern = 0;
    s_material = 0;
    s_craftPending = false;
    s_scrapPending = false;
    s_nextRangeCheck = OsGetAsyncTimeMs() + RANGE_CHECK_INTERVAL_MS;

    Lua::SignalEvent("HOT_CRAFTING_OPEN", "%d", static_cast<int32_t>(flags));
    return 1;
}

static int32_t Packet_SMSG_CRAFTING_CLOSE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    CloseInternal(false, true);
    return 1;
}

static int32_t Packet_SMSG_CRAFTING_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t code = packet.GetUInt8();
    uint32_t resultEntry = packet.GetUInt32();

    s_craftPending = false;

    if (code == 0) {
        for (uint64_t& guid : s_catalysts) {
            UnlockItem(guid);
            guid = 0;
        }

        SignalSlots(MODE_CRAFTING);
    }

    Lua::SignalEvent("HOT_CRAFTING_RESULT", "%d%d", static_cast<int32_t>(code), static_cast<int32_t>(resultEntry));
    return 1;
}

static int32_t Packet_SMSG_CRAFTING_SCRAP_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t code = packet.GetUInt8();
    uint8_t count = packet.GetUInt8();

    s_scrapPending = false;

    int32_t scrapped = 0;
    int32_t rejected = 0;
    bool changed = false;

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        uint64_t guid = packet.GetUInt64();
        uint8_t result = packet.GetUInt8();

        if (result == SCRAP_ITEM_SKIPPED) {
            continue;
        }

        if (result == 0) {
            scrapped++;
        } else {
            rejected++;
        }

        for (uint64_t& slotGuid : s_scrapItems) {
            if (slotGuid == guid) {
                UnlockItem(slotGuid);
                slotGuid = 0;
                changed = true;
            }
        }
    }

    if (changed) {
        SignalSlots(MODE_SCRAPPING);
    }

    Lua::SignalEvent("HOT_CRAFTING_SCRAP_RESULT", "%d%d%d", static_cast<int32_t>(code), scrapped, rejected);
    return 1;
}

static int32_t PushItemRef(lua_State* L, uint64_t guid) {
    ItemRef ref;

    if (!ResolveItem(guid, ref)) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, ref.m_entry);
    lua_pushnumber(L, ref.m_count);

    if (ref.m_bag >= 0) {
        lua_pushnumber(L, ref.m_bag);
        lua_pushnumber(L, ref.m_slot);
    } else {
        lua_pushnil(L);
        lua_pushnil(L);
    }

    return 4;
}

static int32_t Script_CraftingIsOpen(lua_State* L) {
    lua_pushboolean(L, s_open ? 1 : 0);
    return 1;
}

static int32_t Script_CraftingClose(lua_State* L) {
    CloseInternal(true, true);
    return 0;
}

static int32_t Script_CraftingCanCraft(lua_State* L) {
    lua_pushboolean(L, (s_open && (s_flags & OPEN_FLAG_CAN_CRAFT)) ? 1 : 0);
    return 1;
}

static int32_t Script_CraftingGetMode(lua_State* L) {
    lua_pushnumber(L, s_mode);
    return 1;
}

static int32_t Script_CraftingSetMode(lua_State* L) {
    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        int32_t mode = static_cast<int32_t>(lua_tonumber(L, 1));

        if (mode == MODE_CRAFTING || mode == MODE_SCRAPPING) {
            s_mode = static_cast<Mode>(mode);
        }
    }

    return 0;
}

static int32_t Script_CraftingGetDataVersion(lua_State* L) {
    lua_pushnumber(L, s_dataVersion);
    return 1;
}

static int32_t Script_CraftingGetScrapCurrency(lua_State* L) {
    lua_pushnumber(L, s_scrapCurrency);
    return 1;
}

static int32_t Script_CraftingGetNumPatterns(lua_State* L) {
    lua_pushnumber(L, static_cast<double>(s_patterns.size()));
    return 1;
}

static int32_t Script_CraftingGetPatternInfo(lua_State* L) {
    int32_t index = OptionalSlotArg(L, 1, static_cast<int32_t>(s_patterns.size()));

    if (index < 0) {
        lua_pushnil(L);
        return 1;
    }

    const PatternInfo& info = s_patterns[index];
    lua_pushnumber(L, info.m_entry);
    lua_pushnumber(L, info.m_category);
    return 2;
}

static int32_t Script_CraftingGetNumMaterials(lua_State* L) {
    lua_pushnumber(L, static_cast<double>(s_materials.size()));
    return 1;
}

static int32_t Script_CraftingGetMaterialInfo(lua_State* L) {
    int32_t index = OptionalSlotArg(L, 1, static_cast<int32_t>(s_materials.size()));

    if (index < 0) {
        lua_pushnil(L);
        return 1;
    }

    const MaterialInfo& info = s_materials[index];
    lua_pushnumber(L, info.m_entry);
    lua_pushnumber(L, info.m_category);
    return 2;
}

static int32_t Script_CraftingGetNumRecipes(lua_State* L) {
    lua_pushnumber(L, static_cast<double>(s_recipes.size()));
    return 1;
}

static int32_t Script_CraftingGetRecipeInfo(lua_State* L) {
    int32_t index = OptionalSlotArg(L, 1, static_cast<int32_t>(s_recipes.size()));

    if (index < 0) {
        lua_pushnil(L);
        return 1;
    }

    const RecipeInfo& info = s_recipes[index];
    lua_pushnumber(L, info.m_pattern);
    lua_pushnumber(L, info.m_material);
    lua_pushnumber(L, info.m_result);
    lua_pushnumber(L, info.m_scrapCost);
    return 4;
}

static int32_t Script_CraftingGetRecipe(lua_State* L) {
    if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        lua_pushnil(L);
        return 1;
    }

    uint32_t pattern = static_cast<uint32_t>(lua_tonumber(L, 1));
    uint32_t material = static_cast<uint32_t>(lua_tonumber(L, 2));
    auto it = s_recipeLookup.find(MakeRecipeKey(pattern, material));

    if (it == s_recipeLookup.end()) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, it->second.m_result);
    lua_pushnumber(L, it->second.m_scrapCost);
    return 2;
}

static int32_t Script_CraftingGetCatalystInfo(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isnumber(L, 1)) {
        lua_pushnil(L);
        return 1;
    }

    const CatalystInfo* info = FindCatalyst(static_cast<uint32_t>(lua_tonumber(L, 1)));

    if (!info) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushnumber(L, info->m_type);
    lua_pushnumber(L, info->m_value1);
    lua_pushnumber(L, info->m_value2);
    return 3;
}

static int32_t Script_CraftingSetPattern(lua_State* L) {
    uint32_t entry = 0;

    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        entry = static_cast<uint32_t>(lua_tonumber(L, 1));
    }

    bool known = false;

    for (const PatternInfo& info : s_patterns) {
        if (info.m_entry == entry) {
            known = true;
        }
    }

    s_pattern = known ? entry : 0;
    return 0;
}

static int32_t Script_CraftingSetMaterial(lua_State* L) {
    uint32_t entry = 0;

    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        entry = static_cast<uint32_t>(lua_tonumber(L, 1));
    }

    bool known = false;

    for (const MaterialInfo& info : s_materials) {
        if (info.m_entry == entry) {
            known = true;
        }
    }

    s_material = known ? entry : 0;
    return 0;
}

static int32_t Script_CraftingGetSelection(lua_State* L) {
    if (s_pattern) {
        lua_pushnumber(L, s_pattern);
    } else {
        lua_pushnil(L);
    }

    if (s_material) {
        lua_pushnumber(L, s_material);
    } else {
        lua_pushnil(L);
    }

    return 2;
}

static int32_t Script_CraftingGetCatalyst(lua_State* L) {
    int32_t slot = OptionalSlotArg(L, 1, MAX_CATALYSTS);

    if (slot < 0) {
        lua_pushnil(L);
        return 1;
    }

    return PushItemRef(L, s_catalysts[slot]);
}

static int32_t Script_CraftingAddCursorCatalyst(lua_State* L) {
    int32_t slot = OptionalSlotArg(L, 1, MAX_CATALYSTS);
    lua_pushboolean(L, AddCursorItem(MODE_CRAFTING, slot) ? 1 : 0);
    return 1;
}

static int32_t Script_CraftingRemoveCatalyst(lua_State* L) {
    int32_t slot = OptionalSlotArg(L, 1, MAX_CATALYSTS);

    if (slot < 0) {
        return 0;
    }

    if (s_craftPending) {
        SignalError(CRAFT_ERR_BUSY);
        return 0;
    }

    if (s_catalysts[slot]) {
        UnlockItem(s_catalysts[slot]);
        s_catalysts[slot] = 0;
        SignalSlots(MODE_CRAFTING);
    }

    return 0;
}

static int32_t Script_CraftingGetScrapItem(lua_State* L) {
    int32_t slot = OptionalSlotArg(L, 1, MAX_SCRAP_ITEMS);

    if (slot < 0) {
        lua_pushnil(L);
        return 1;
    }

    return PushItemRef(L, s_scrapItems[slot]);
}

static int32_t Script_CraftingGetNumScrapItems(lua_State* L) {
    int32_t count = 0;

    for (uint64_t guid : s_scrapItems) {
        if (guid) {
            count++;
        }
    }

    lua_pushnumber(L, count);
    return 1;
}

static int32_t Script_CraftingAddCursorScrapItem(lua_State* L) {
    int32_t slot = OptionalSlotArg(L, 1, MAX_SCRAP_ITEMS);
    lua_pushboolean(L, AddCursorItem(MODE_SCRAPPING, slot) ? 1 : 0);
    return 1;
}

static int32_t Script_CraftingRemoveScrapItem(lua_State* L) {
    int32_t slot = OptionalSlotArg(L, 1, MAX_SCRAP_ITEMS);

    if (slot < 0) {
        return 0;
    }

    if (s_scrapPending) {
        SignalError(CRAFT_ERR_BUSY);
        return 0;
    }

    if (s_scrapItems[slot]) {
        UnlockItem(s_scrapItems[slot]);
        s_scrapItems[slot] = 0;
        SignalSlots(MODE_SCRAPPING);
    }

    return 0;
}

static int32_t Script_CraftingClearSlots(lua_State* L) {
    int32_t mode = -1;

    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        mode = static_cast<int32_t>(lua_tonumber(L, 1));
    }

    if ((mode == -1 || mode == MODE_CRAFTING) && !s_craftPending) {
        for (uint64_t& guid : s_catalysts) {
            UnlockItem(guid);
            guid = 0;
        }

        SignalSlots(MODE_CRAFTING);
    }

    if ((mode == -1 || mode == MODE_SCRAPPING) && !s_scrapPending) {
        for (uint64_t& guid : s_scrapItems) {
            UnlockItem(guid);
            guid = 0;
        }

        SignalSlots(MODE_SCRAPPING);
    }

    return 0;
}

static int32_t Script_CraftingCraft(lua_State* L) {
    lua_pushboolean(L, SendCraft() ? 1 : 0);
    return 1;
}

static int32_t Script_CraftingScrap(lua_State* L) {
    lua_pushboolean(L, SendScrap() ? 1 : 0);
    return 1;
}

static int32_t Script_CraftingIsBusy(lua_State* L) {
    lua_pushboolean(L, (s_craftPending || s_scrapPending) ? 1 : 0);
    return 1;
}

static int32_t Script_CraftingCancelPending(lua_State* L) {
    s_craftPending = false;
    s_scrapPending = false;
    return 0;
}

bool IsOpen() {
    return s_open;
}

bool HandleUseContainerItem(lua_State* L) {
    if (!s_open) {
        return false;
    }

    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        return false;
    }

    CGBag_C* bag;
    int32_t slot;
    int32_t bank;

    if (!CGContainerInfo::GetLuaBagAndSlot(L, &bag, &slot, &bank)) {
        return false;
    }

    CGItem_C* item = bag->GetItemPointer(slot);

    if (!item) {
        return false;
    }

    uint32_t entry = item->m_obj->m_entryID;
    bool wanted = s_mode == MODE_CRAFTING ? IsCatalystEntry(entry) : IsScrapCandidate(entry);

    if (!wanted) {
        return false;
    }

    if (item->m_itemFlags & 1) {
        SignalError(CRAFT_ERR_ITEM_LOCKED);
        return true;
    }

    uint64_t guid = item->m_obj->m_guid.m_guid;
    bool placed = s_mode == MODE_CRAFTING ? PlaceCatalyst(guid, entry, -1) : PlaceScrapItem(guid, entry, -1);

    if (placed) {
        LockItem(guid);
    }

    return true;
}

class CraftingModule : public Module {
    public:
    const char* Name() const override {
        return "Crafting";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_CRAFTING_DATA, Packet_SMSG_CRAFTING_DATA);
        Net::RegisterWorldHandler(Net::SMSG_CRAFTING_OPEN, Packet_SMSG_CRAFTING_OPEN);
        Net::RegisterWorldHandler(Net::SMSG_CRAFTING_CLOSE, Packet_SMSG_CRAFTING_CLOSE);
        Net::RegisterWorldHandler(Net::SMSG_CRAFTING_RESULT, Packet_SMSG_CRAFTING_RESULT);
        Net::RegisterWorldHandler(Net::SMSG_CRAFTING_SCRAP_RESULT, Packet_SMSG_CRAFTING_SCRAP_RESULT);

        Lua::RegisterFunction("CraftingIsOpen", Script_CraftingIsOpen);
        Lua::RegisterFunction("CraftingClose", Script_CraftingClose);
        Lua::RegisterFunction("CraftingCanCraft", Script_CraftingCanCraft);
        Lua::RegisterFunction("CraftingGetMode", Script_CraftingGetMode);
        Lua::RegisterFunction("CraftingSetMode", Script_CraftingSetMode);
        Lua::RegisterFunction("CraftingGetDataVersion", Script_CraftingGetDataVersion);
        Lua::RegisterFunction("CraftingGetScrapCurrency", Script_CraftingGetScrapCurrency);
        Lua::RegisterFunction("CraftingGetNumPatterns", Script_CraftingGetNumPatterns);
        Lua::RegisterFunction("CraftingGetPatternInfo", Script_CraftingGetPatternInfo);
        Lua::RegisterFunction("CraftingGetNumMaterials", Script_CraftingGetNumMaterials);
        Lua::RegisterFunction("CraftingGetMaterialInfo", Script_CraftingGetMaterialInfo);
        Lua::RegisterFunction("CraftingGetNumRecipes", Script_CraftingGetNumRecipes);
        Lua::RegisterFunction("CraftingGetRecipeInfo", Script_CraftingGetRecipeInfo);
        Lua::RegisterFunction("CraftingGetRecipe", Script_CraftingGetRecipe);
        Lua::RegisterFunction("CraftingGetCatalystInfo", Script_CraftingGetCatalystInfo);
        Lua::RegisterFunction("CraftingSetPattern", Script_CraftingSetPattern);
        Lua::RegisterFunction("CraftingSetMaterial", Script_CraftingSetMaterial);
        Lua::RegisterFunction("CraftingGetSelection", Script_CraftingGetSelection);
        Lua::RegisterFunction("CraftingGetCatalyst", Script_CraftingGetCatalyst);
        Lua::RegisterFunction("CraftingAddCursorCatalyst", Script_CraftingAddCursorCatalyst);
        Lua::RegisterFunction("CraftingRemoveCatalyst", Script_CraftingRemoveCatalyst);
        Lua::RegisterFunction("CraftingGetScrapItem", Script_CraftingGetScrapItem);
        Lua::RegisterFunction("CraftingGetNumScrapItems", Script_CraftingGetNumScrapItems);
        Lua::RegisterFunction("CraftingAddCursorScrapItem", Script_CraftingAddCursorScrapItem);
        Lua::RegisterFunction("CraftingRemoveScrapItem", Script_CraftingRemoveScrapItem);
        Lua::RegisterFunction("CraftingClearSlots", Script_CraftingClearSlots);
        Lua::RegisterFunction("CraftingCraft", Script_CraftingCraft);
        Lua::RegisterFunction("CraftingScrap", Script_CraftingScrap);
        Lua::RegisterFunction("CraftingIsBusy", Script_CraftingIsBusy);
        Lua::RegisterFunction("CraftingCancelPending", Script_CraftingCancelPending);
    }

    void OnEnterWorld() override {
        Reset();
    }

    void OnIdle() override {
        Tick();
    }
};

Module* GetModule() {
    static CraftingModule module;
    return &module;
}

}
