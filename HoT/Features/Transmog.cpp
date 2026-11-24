#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "Engine/Source/Frame/CScriptObject.h"
#include "Engine/Source/Frame/CSimpleModel.h"
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/Ui/DressUpModelFrame.h"
#include "WoW/Common/ItemTypes.h"

namespace HoT::Transmog {

struct TransmogAppearance {
    uint32_t m_displayId = 0;
    uint8_t m_inventoryType = 0;
    uint8_t m_sheath = 0;
    uint8_t m_itemClass = 0;
    uint8_t m_itemSubClass = 0;
    uint8_t m_quality = 0;
    uint32_t m_addedTime = 0;
    std::string m_name;
};

struct TransmogSlot {
    uint32_t m_currentDisplayId = 0;
    uint8_t m_currentSheath = 0;
    uint32_t m_originalDisplayId = 0;
    uint8_t m_originalSheath = 0;
    uint8_t m_flags = 0;
};

struct ApplyEntry {
    uint8_t m_slot = 0;
    uint32_t m_displayId = 0;
    uint8_t m_inventoryType = 0;
};

static constexpr uint8_t OPEN_FLAG_CAN_APPLY = 0x01;
static constexpr uint8_t SLOT_FLAG_TRANSMOGGED = 0x01;
static constexpr int32_t EQUIPMENT_SLOT_MAINHAND = 15;
static constexpr int32_t EQUIPMENT_SLOT_OFFHAND = 16;
static constexpr int32_t AUTO_HAND = -1;
static constexpr int32_t NO_COMPONENT_SLOT = -1;
static constexpr size_t MAX_APPLY_ENTRIES = 255;

static std::vector<TransmogAppearance> s_appearances;
static std::unordered_map<uint64_t, size_t> s_index;
static std::unordered_map<uint8_t, TransmogSlot> s_slots;
static uint64_t s_npcGuid;
static bool s_canApply;
static bool s_isOpen;

static bool AppearanceLess(const TransmogAppearance& a, const TransmogAppearance& b) {
    if (a.m_inventoryType != b.m_inventoryType) {
        return a.m_inventoryType < b.m_inventoryType;
    }

    return a.m_displayId < b.m_displayId;
}

static uint64_t AppearanceKey(uint32_t displayId, uint8_t inventoryType) {
    return (static_cast<uint64_t>(inventoryType) << 32) | displayId;
}

static void RebuildIndex() {
    s_index.clear();

    for (size_t i = 0; i < s_appearances.size(); i++) {
        s_index[AppearanceKey(s_appearances[i].m_displayId, s_appearances[i].m_inventoryType)] = i;
    }
}

static void Upsert(const TransmogAppearance& appearance) {
    auto it = std::lower_bound(s_appearances.begin(), s_appearances.end(), appearance, AppearanceLess);

    if (it != s_appearances.end() && it->m_inventoryType == appearance.m_inventoryType && it->m_displayId == appearance.m_displayId) {
        *it = appearance;
    } else {
        s_appearances.insert(it, appearance);
    }

    RebuildIndex();
}

static bool Remove(uint32_t displayId, uint8_t inventoryType) {
    auto it = s_index.find(AppearanceKey(displayId, inventoryType));

    if (it == s_index.end() || it->second >= s_appearances.size()) {
        return false;
    }

    s_appearances.erase(s_appearances.begin() + it->second);
    RebuildIndex();
    return true;
}

static const TransmogAppearance* FindAppearance(uint32_t displayId, uint8_t inventoryType) {
    auto it = s_index.find(AppearanceKey(displayId, inventoryType));

    if (it == s_index.end() || it->second >= s_appearances.size()) {
        return nullptr;
    }

    return &s_appearances[it->second];
}

static void ClearOpenState() {
    s_isOpen = false;
    s_npcGuid = 0;
    s_canApply = false;
}

static void Reset() {
    s_appearances.clear();
    s_index.clear();
    s_slots.clear();
    ClearOpenState();
}

static void ReadAppearance(Net::Packet& packet, TransmogAppearance& out) {
    out.m_displayId = packet.GetUInt32();
    out.m_inventoryType = packet.GetUInt8();
    out.m_sheath = packet.GetUInt8();
    out.m_itemClass = packet.GetUInt8();
    out.m_itemSubClass = packet.GetUInt8();
    out.m_quality = packet.GetUInt8();
    out.m_addedTime = packet.GetUInt32();

    char name[256] = {};
    packet.GetString(name, sizeof(name));
    out.m_name = name;
}

static bool PushDisplayIcon(lua_State* L, uint32_t displayId) {
    if (!displayId) {
        return false;
    }

    ItemDisplayInfoRec record;

    if (!g_itemDisplayInfoDB.GetRecord(static_cast<int32_t>(displayId), &record)) {
        return false;
    }

    const char* name = record.m_inventoryIcon[0];

    if (!name || !*name) {
        return false;
    }

    lua_pushfstring(L, "Interface\\Icons\\%s", name);
    return true;
}

static int32_t PushAppearance(lua_State* L, const TransmogAppearance& appearance) {
    lua_pushnumber(L, appearance.m_displayId);
    lua_pushnumber(L, appearance.m_inventoryType);
    lua_pushnumber(L, appearance.m_sheath);
    lua_pushnumber(L, appearance.m_itemClass);
    lua_pushnumber(L, appearance.m_itemSubClass);
    lua_pushnumber(L, appearance.m_quality);
    lua_pushstring(L, appearance.m_name.c_str());
    lua_pushnumber(L, appearance.m_addedTime);

    if (!PushDisplayIcon(L, appearance.m_displayId)) {
        lua_pushnil(L);
    }

    return 9;
}

static int32_t ArmorComponentSlot(uint8_t inventoryType) {
    switch (inventoryType) {
        case INVTYPE_HEAD:
            return ITEMSLOT_HEAD;

        case INVTYPE_SHOULDERS:
            return ITEMSLOT_SHOULDER;

        case INVTYPE_BODY:
            return ITEMSLOT_SHIRT;

        case INVTYPE_CHEST:
        case INVTYPE_ROBE:
            return ITEMSLOT_CHEST;

        case INVTYPE_WAIST:
            return ITEMSLOT_BELT;

        case INVTYPE_LEGS:
            return ITEMSLOT_PANTS;

        case INVTYPE_FEET:
            return ITEMSLOT_BOOTS;

        case INVTYPE_WRISTS:
            return ITEMSLOT_BRACERS;

        case INVTYPE_HANDS:
            return ITEMSLOT_GLOVES;

        case INVTYPE_TABARD:
            return ITEMSLOT_TABARD;

        case INVTYPE_CLOAK:
            return ITEMSLOT_CAPE;

        default:
            return NO_COMPONENT_SLOT;
    }
}

static bool IsWeaponInventoryType(uint8_t inventoryType) {
    switch (inventoryType) {
        case INVTYPE_WEAPON:
        case INVTYPE_SHIELD:
        case INVTYPE_RANGED:
        case INVTYPE_2HWEAPON:
        case INVTYPE_WEAPONMAINHAND:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:
        case INVTYPE_THROWN:
        case INVTYPE_RANGEDRIGHT:
            return true;

        default:
            return false;
    }
}

static INVENTORY_SLOTS ResolveHand(uint8_t inventoryType, int32_t slot) {
    if (slot == EQUIPMENT_SLOT_MAINHAND) {
        return INVSLOT_MAINHAND;
    }

    if (slot == EQUIPMENT_SLOT_OFFHAND) {
        return INVSLOT_OFFHAND;
    }

    switch (inventoryType) {
        case INVTYPE_SHIELD:
        case INVTYPE_RANGED:
        case INVTYPE_WEAPONOFFHAND:
        case INVTYPE_HOLDABLE:
            return INVSLOT_OFFHAND;

        default:
            return INVSLOT_MAINHAND;
    }
}

static CGUnitVirtualItem* HandRecord(CGDressUpModelFrame* frame, INVENTORY_SLOTS hand) {
    return hand == INVSLOT_MAINHAND ? &frame->m_mainHand : &frame->m_offHand;
}

static int32_t* HandItemId(INVENTORY_SLOTS hand) {
    return hand == INVSLOT_MAINHAND ? &s_mainHandItemId : &s_offHandItemId;
}

// Same lookup as FrameScript_GetObjectThis without raising a Lua error, and only frames store light userdata at [0].
static CScriptObject* GetFrameObject(lua_State* L, int32_t index) {
    if (lua_type(L, index) != LUA_TTABLE) {
        return nullptr;
    }

    lua_rawgeti(L, index, 0);
    CScriptObject* object = lua_type(L, -1) == LUA_TLIGHTUSERDATA ? static_cast<CScriptObject*>(lua_touserdata(L, -1)) : nullptr;
    lua_settop(L, -2);
    return object;
}

static bool RemoveHandDisplay(CGDressUpModelFrame* frame, CM2Model* model, INVENTORY_SLOTS hand) {
    CGUnitVirtualItem* record = HandRecord(frame, hand);

    if (!record->m_classID) {
        return false;
    }

    CCharacterComponent::RemoveHandItem(model, hand, static_cast<SHEATHE_TYPE>(record->m_sheatheType), record->m_inventoryType == INVTYPE_SHIELD);
    *record = CGUnitVirtualItem();
    *HandItemId(hand) = 0;
    return true;
}

static bool TryOnArmorDisplay(CGDressUpModelFrame* frame, uint32_t displayId, uint8_t inventoryType, const char*& reason) {
    int32_t componentSlot = ArmorComponentSlot(inventoryType);

    if (componentSlot == NO_COMPONENT_SLOT) {
        reason = "invtype";
        return false;
    }

    CCharacterComponent* component = frame->m_component;

    if (!component) {
        reason = "nocomponent";
        return false;
    }

    component->RemoveItem(static_cast<ITEM_SLOT>(componentSlot));
    component->AddItem(static_cast<ITEM_SLOT>(componentSlot), static_cast<int32_t>(displayId), 0);
    return true;
}

static bool TryOnWeaponDisplay(CGDressUpModelFrame* frame, uint32_t displayId, uint8_t inventoryType, uint8_t sheath, int32_t slot, const char*& reason) {
    CM2Model* model = frame->m_model;

    if (!model) {
        reason = "nomodel";
        return false;
    }

    INVENTORY_SLOTS hand = ResolveHand(inventoryType, slot);
    INVENTORY_SLOTS otherHand = hand == INVSLOT_MAINHAND ? INVSLOT_OFFHAND : INVSLOT_MAINHAND;

    bool touched = RemoveHandDisplay(frame, model, hand);
    bool otherHoldsTwoHander = HandRecord(frame, otherHand)->m_inventoryType == INVTYPE_2HWEAPON;

    if (inventoryType == INVTYPE_2HWEAPON || (hand == INVSLOT_OFFHAND && otherHoldsTwoHander)) {
        if (RemoveHandDisplay(frame, model, otherHand)) {
            touched = true;
        }
    }

    ItemDisplayInfoRec display;

    if (!g_itemDisplayInfoDB.GetRecord(static_cast<int32_t>(displayId), &display)) {
        reason = touched ? "nodisplay-removed" : "nodisplay";
        return false;
    }

    CGUnitVirtualItem* record = HandRecord(frame, hand);
    record->m_classID = (inventoryType == INVTYPE_SHIELD || inventoryType == INVTYPE_HOLDABLE) ? ITEM_CLASS_ARMOR : ITEM_CLASS_WEAPON;
    record->m_subclassID = 0;
    record->m_inventoryType = inventoryType;
    record->m_sheatheType = sheath;
    *HandItemId(hand) = 0;

    CCharacterComponent::AddHandItem(model, &display, hand, static_cast<SHEATHE_TYPE>(sheath), false, inventoryType == INVTYPE_SHIELD, inventoryType == INVTYPE_RANGEDRIGHT, 0);
    return true;
}

static bool TryOnDisplay(CGDressUpModelFrame* frame, uint32_t displayId, uint8_t inventoryType, uint8_t sheath, int32_t slot, const char*& reason) {
    reason = nullptr;

    if (IsWeaponInventoryType(inventoryType)) {
        return TryOnWeaponDisplay(frame, displayId, inventoryType, sheath, slot, reason);
    }

    return TryOnArmorDisplay(frame, displayId, inventoryType, reason);
}

static bool SendApply(const std::vector<ApplyEntry>& entries) {
    if (!s_isOpen || entries.empty()) {
        return false;
    }

    Net::Packet packet(Net::CMSG_TRANSMOG_APPLY);
    packet.PutUInt8(static_cast<uint8_t>(entries.size()));

    for (const ApplyEntry& entry : entries) {
        packet.PutUInt8(entry.m_slot);
        packet.PutUInt32(entry.m_displayId);
        packet.PutUInt8(entry.m_inventoryType);
    }

    packet.Send();
    return true;
}

static int32_t Packet_SMSG_TRANSMOG_COLLECTION(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t count = packet.GetUInt32();
    s_appearances.clear();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        TransmogAppearance appearance;
        ReadAppearance(packet, appearance);
        s_appearances.push_back(std::move(appearance));
    }

    std::stable_sort(s_appearances.begin(), s_appearances.end(), AppearanceLess);
    RebuildIndex();

    Lua::SignalEvent("HOT_TRANSMOG_COLLECTION_UPDATE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_TRANSMOG_COLLECTION_ADD(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    TransmogAppearance appearance;
    ReadAppearance(packet, appearance);

    if (!appearance.m_displayId) {
        return 1;
    }

    Upsert(appearance);
    Lua::SignalEvent("HOT_TRANSMOG_COLLECTION_ITEM_UPDATE", "%u%u", appearance.m_displayId, static_cast<uint32_t>(appearance.m_inventoryType));
    return 1;
}

static int32_t Packet_SMSG_TRANSMOG_COLLECTION_REMOVE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t displayId = packet.GetUInt32();
    uint8_t inventoryType = packet.GetUInt8();

    if (Remove(displayId, inventoryType)) {
        Lua::SignalEvent("HOT_TRANSMOG_COLLECTION_UPDATE", nullptr);
    }

    return 1;
}

static int32_t Packet_SMSG_TRANSMOG_ACTIVE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t count = packet.GetUInt8();
    s_slots.clear();

    for (uint8_t i = 0; i < count && packet.CanRead(1); i++) {
        uint8_t slot = packet.GetUInt8();
        TransmogSlot info;
        info.m_currentDisplayId = packet.GetUInt32();
        info.m_currentSheath = packet.GetUInt8();
        info.m_originalDisplayId = packet.GetUInt32();
        info.m_originalSheath = packet.GetUInt8();
        info.m_flags = packet.GetUInt8();
        s_slots[slot] = info;
    }

    Lua::SignalEvent("HOT_TRANSMOG_ACTIVE_UPDATE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_TRANSMOG_OPEN(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint64_t npcGuid = packet.GetUInt64();
    uint8_t flags = packet.GetUInt8();

    s_npcGuid = npcGuid;
    s_canApply = (flags & OPEN_FLAG_CAN_APPLY) != 0;
    s_isOpen = true;

    Lua::SignalEvent("HOT_TRANSMOG_OPEN", "%u", s_canApply ? 1u : 0u);
    return 1;
}

static int32_t Packet_SMSG_TRANSMOG_CLOSE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    ClearOpenState();
    Lua::SignalEvent("HOT_TRANSMOG_CLOSE", nullptr);
    return 1;
}

static int32_t Packet_SMSG_TRANSMOG_RESULT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint8_t code = packet.GetUInt8();
    uint8_t slot = packet.GetUInt8();

    Lua::SignalEvent("HOT_TRANSMOG_RESULT", "%u%u", static_cast<uint32_t>(code), static_cast<uint32_t>(slot));
    return 1;
}

static int32_t Script_TransmogRequestCollection(lua_State* L) {
    Net::Packet packet(Net::CMSG_TRANSMOG_COLLECTION_REQUEST);

    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        packet.PutUInt8(static_cast<uint8_t>(lua_tonumber(L, 1)));
    }

    packet.Send();
    return 0;
}

static int32_t Script_TransmogGetNumAppearances(lua_State* L) {
    lua_pushnumber(L, static_cast<lua_Number>(s_appearances.size()));
    return 1;
}

static int32_t Script_TransmogGetAppearanceInfo(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isnumber(L, 1)) {
        lua_pushnil(L);
        return 1;
    }

    int32_t index = static_cast<int32_t>(lua_tonumber(L, 1)) - 1;

    if (index < 0 || index >= static_cast<int32_t>(s_appearances.size())) {
        lua_pushnil(L);
        return 1;
    }

    return PushAppearance(L, s_appearances[index]);
}

static int32_t Script_TransmogGetAppearance(lua_State* L) {
    if (lua_gettop(L) < 2 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2)) {
        lua_pushnil(L);
        return 1;
    }

    double displayId = lua_tonumber(L, 1);
    double inventoryType = lua_tonumber(L, 2);

    if (displayId < 0 || inventoryType < 0 || inventoryType > 0xFF) {
        lua_pushnil(L);
        return 1;
    }

    const TransmogAppearance* appearance = FindAppearance(static_cast<uint32_t>(displayId), static_cast<uint8_t>(inventoryType));

    if (!appearance) {
        lua_pushnil(L);
        return 1;
    }

    return PushAppearance(L, *appearance);
}

static int32_t Script_TransmogGetSlotInfo(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isnumber(L, 1)) {
        lua_pushnil(L);
        return 1;
    }

    double slot = lua_tonumber(L, 1);

    if (slot < 0 || slot > 0xFF) {
        lua_pushnil(L);
        return 1;
    }

    auto it = s_slots.find(static_cast<uint8_t>(slot));

    if (it == s_slots.end()) {
        lua_pushnil(L);
        return 1;
    }

    const TransmogSlot& info = it->second;
    lua_pushnumber(L, info.m_currentDisplayId);
    lua_pushnumber(L, info.m_currentSheath);
    lua_pushnumber(L, info.m_originalDisplayId);
    lua_pushnumber(L, info.m_originalSheath);
    lua_pushboolean(L, (info.m_flags & SLOT_FLAG_TRANSMOGGED) ? 1 : 0);
    return 5;
}

static int32_t Script_TransmogIsOpen(lua_State* L) {
    lua_pushboolean(L, s_isOpen ? 1 : 0);
    return 1;
}

static int32_t Script_TransmogGetOpenInfo(lua_State* L) {
    lua_pushboolean(L, s_canApply ? 1 : 0);
    return 1;
}

static int32_t Script_TransmogApply(lua_State* L) {
    std::vector<ApplyEntry> entries;
    int32_t top = lua_gettop(L);

    for (int32_t i = 1; i + 2 <= top; i += 3) {
        if (!lua_isnumber(L, i) || !lua_isnumber(L, i + 1) || !lua_isnumber(L, i + 2)) {
            break;
        }

        double slot = lua_tonumber(L, i);
        double displayId = lua_tonumber(L, i + 1);
        double inventoryType = lua_tonumber(L, i + 2);

        if (slot < 0 || slot > 0xFF || displayId < 0 || inventoryType < 0 || inventoryType > 0xFF) {
            continue;
        }

        ApplyEntry entry;
        entry.m_slot = static_cast<uint8_t>(slot);
        entry.m_displayId = static_cast<uint32_t>(displayId);
        entry.m_inventoryType = static_cast<uint8_t>(inventoryType);
        entries.push_back(entry);

        if (entries.size() >= MAX_APPLY_ENTRIES) {
            break;
        }
    }

    bool sent = SendApply(entries);
    lua_pushboolean(L, sent ? 1 : 0);
    return 1;
}

static int32_t Script_TransmogClose(lua_State* L) {
    if (s_isOpen) {
        Net::Packet(Net::CMSG_TRANSMOG_CLOSE).Send();
    }

    ClearOpenState();
    return 0;
}

static int32_t Script_TransmogGetDisplayIcon(lua_State* L) {
    if (lua_gettop(L) < 1 || !lua_isnumber(L, 1) || !PushDisplayIcon(L, static_cast<uint32_t>(lua_tonumber(L, 1)))) {
        lua_pushnil(L);
        return 1;
    }

    return 1;
}

static int32_t PushTryOnFailure(lua_State* L, const char* reason) {
    lua_pushboolean(L, 0);
    lua_pushstring(L, reason ? reason : "unknown");
    return 2;
}

static int32_t Script_DressUpModelTryOnDisplay(lua_State* L) {
    if (lua_gettop(L) < 4 || !lua_isnumber(L, 2) || !lua_isnumber(L, 3) || !lua_isnumber(L, 4)) {
        return PushTryOnFailure(L, "args");
    }

    CScriptObject* object = GetFrameObject(L, 1);

    if (!object) {
        return PushTryOnFailure(L, "noframe");
    }

    // Without a DressUpModel type yet the object cannot be one, so never poke it.
    int32_t typeId = CGDressUpModelFrame::s_objectType;

    if (!typeId || !object->IsA(typeId)) {
        return PushTryOnFailure(L, "type");
    }

    double displayId = lua_tonumber(L, 2);
    double inventoryType = lua_tonumber(L, 3);
    double sheath = lua_tonumber(L, 4);

    if (displayId < 0 || inventoryType < 0 || inventoryType > 0xFF || sheath < 0 || sheath > 0xFF) {
        return PushTryOnFailure(L, "range");
    }

    int32_t slot = AUTO_HAND;

    if (lua_gettop(L) >= 5 && lua_isnumber(L, 5)) {
        slot = static_cast<int32_t>(lua_tonumber(L, 5));
    }

    const char* reason = nullptr;
    CGDressUpModelFrame* frame = static_cast<CGDressUpModelFrame*>(object);

    if (!TryOnDisplay(frame, static_cast<uint32_t>(displayId), static_cast<uint8_t>(inventoryType), static_cast<uint8_t>(sheath), slot, reason)) {
        return PushTryOnFailure(L, reason);
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int32_t Script_DressUpModelTryOnSlot(lua_State* L) {
    if (lua_gettop(L) < 2 || !lua_isnumber(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    CScriptObject* object = GetFrameObject(L, 1);

    if (!object) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int32_t itemEntry = static_cast<int32_t>(lua_tonumber(L, 2));
    int32_t slot = -1;

    if (lua_gettop(L) >= 3 && lua_isnumber(L, 3)) {
        slot = static_cast<int32_t>(lua_tonumber(L, 3));
    }

    int32_t typeId = CGDressUpModelFrame::s_objectType;

    // No DressUpModel type yet, so fall back to calling the frame's own TryOn method.
    if (!typeId) {
        lua_getfield(L, 1, "TryOn");

        if (lua_type(L, -1) != LUA_TFUNCTION) {
            lua_settop(L, -2);
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_pushvalue(L, 1);
        lua_pushnumber(L, itemEntry);

        if (lua_pcall(L, 2, 0, 0) != 0) {
            lua_settop(L, -2);
        }

        lua_pushboolean(L, 1);
        return 1;
    }

    if (!object->IsA(typeId)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int32_t hand = AUTO_HAND;

    if (slot == EQUIPMENT_SLOT_MAINHAND) {
        hand = INVSLOT_MAINHAND;
    } else if (slot == EQUIPMENT_SLOT_OFFHAND) {
        hand = INVSLOT_OFFHAND;
    }

    static_cast<CGDressUpModelFrame*>(object)->TryOn(itemEntry, nullptr, hand);
    lua_pushboolean(L, 1);
    return 1;
}

class TransmogModule : public Module {
    public:
    const char* Name() const override {
        return "Transmog";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_TRANSMOG_COLLECTION, Packet_SMSG_TRANSMOG_COLLECTION);
        Net::RegisterWorldHandler(Net::SMSG_TRANSMOG_COLLECTION_ADD, Packet_SMSG_TRANSMOG_COLLECTION_ADD);
        Net::RegisterWorldHandler(Net::SMSG_TRANSMOG_ACTIVE, Packet_SMSG_TRANSMOG_ACTIVE);
        Net::RegisterWorldHandler(Net::SMSG_TRANSMOG_OPEN, Packet_SMSG_TRANSMOG_OPEN);
        Net::RegisterWorldHandler(Net::SMSG_TRANSMOG_CLOSE, Packet_SMSG_TRANSMOG_CLOSE);
        Net::RegisterWorldHandler(Net::SMSG_TRANSMOG_RESULT, Packet_SMSG_TRANSMOG_RESULT);
        Net::RegisterWorldHandler(Net::SMSG_TRANSMOG_COLLECTION_REMOVE, Packet_SMSG_TRANSMOG_COLLECTION_REMOVE);

        Lua::RegisterFunction("TransmogRequestCollection", Script_TransmogRequestCollection);
        Lua::RegisterFunction("TransmogGetNumAppearances", Script_TransmogGetNumAppearances);
        Lua::RegisterFunction("TransmogGetAppearanceInfo", Script_TransmogGetAppearanceInfo);
        Lua::RegisterFunction("TransmogGetAppearance", Script_TransmogGetAppearance);
        Lua::RegisterFunction("TransmogGetSlotInfo", Script_TransmogGetSlotInfo);
        Lua::RegisterFunction("TransmogIsOpen", Script_TransmogIsOpen);
        Lua::RegisterFunction("TransmogGetOpenInfo", Script_TransmogGetOpenInfo);
        Lua::RegisterFunction("TransmogApply", Script_TransmogApply);
        Lua::RegisterFunction("TransmogClose", Script_TransmogClose);
        Lua::RegisterFunction("TransmogGetDisplayIcon", Script_TransmogGetDisplayIcon);
        Lua::RegisterFunction("DressUpModelTryOnDisplay", Script_DressUpModelTryOnDisplay);
        Lua::RegisterFunction("DressUpModelTryOnSlot", Script_DressUpModelTryOnSlot);
    }

    void OnEnterWorld() override {
        Reset();
    }
};

Module* GetModule() {
    static TransmogModule module;
    return &module;
}

}
