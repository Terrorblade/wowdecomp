#include "HoT/Features/Items.h"
#include <unordered_map>
#include "HoT/Core/Modules.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/Rec/ItemRec.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"

namespace HoT::Items {

static const int32_t NUM_VISIBLE_SLOTS = 19;

static std::unordered_map<int32_t, ItemRec> s_serverRecords;
static std::unordered_map<int32_t, ItemRec> s_cacheRecords;

enum ItemFlags2 : uint32_t {
    ITEM_FLAG2_PARADOXICAL = 0x10000000,
    ITEM_FLAG2_HEIRLOOM = 0x20000000,
    ITEM_FLAG2_LEGENDARY = 0x40000000,
    ITEM_FLAG2_CORRUPTED = 0x80000000,
};

struct TooltipLabel {
    uint32_t m_mask;
    const char* m_key;
};

// Most specific combination first.
static const TooltipLabel s_tooltipLabels[] = {
    { ITEM_FLAG2_PARADOXICAL | ITEM_FLAG2_CORRUPTED | ITEM_FLAG2_LEGENDARY, "ITEM_PARADOXICAL_TIME_WARPED_LEGENDARY" },
    { ITEM_FLAG2_PARADOXICAL | ITEM_FLAG2_CORRUPTED, "ITEM_PARADOXICAL_CORRUPTED" },
    { ITEM_FLAG2_PARADOXICAL | ITEM_FLAG2_LEGENDARY, "ITEM_PARADOXICAL_LEGENDARY" },
    { ITEM_FLAG2_PARADOXICAL | ITEM_FLAG2_HEIRLOOM, "ITEM_PARADOXICAL_HEIRLOOM" },
    { ITEM_FLAG2_CORRUPTED | ITEM_FLAG2_LEGENDARY, "ITEM_TIME_WARPED_LEGENDARY" },
    { ITEM_FLAG2_CORRUPTED, "ITEM_CORRUPTED" },
    { ITEM_FLAG2_PARADOXICAL, "ITEM_PARADOXICAL" },
    { ITEM_FLAG2_LEGENDARY, "ITEM_LEGENDARY" },
    { ITEM_FLAG2_HEIRLOOM, "ITEM_HEIRLOOM" },
};

const char* GetHeroicLabelKey(const ItemStats_C* stats, const char* defaultKey) {
    for (const TooltipLabel& label : s_tooltipLabels) {
        if (stats && (stats->m_flags[1] & label.m_mask) == label.m_mask) {
            return label.m_key;
        }
    }

    return defaultKey;
}

static void Fill(ItemRec& rec, int32_t id, int32_t classID, int32_t subclassID, int32_t soundOverride, int32_t material, int32_t displayInfo, int32_t inventoryType, int32_t sheatheType) {
    rec.m_id = id;
    rec.m_classID = classID;
    rec.m_subclassID = subclassID;
    rec.m_soundOverrideSubclassID = soundOverride;
    rec.m_material = material;
    rec.m_displayInfoID = displayInfo;
    rec.m_inventoryType = inventoryType;
    rec.m_sheatheType = sheatheType;
}

ItemRec* Resolve(int32_t id, ItemRec* dbcRecord) {
    if (id <= 0) {
        return dbcRecord;
    }

    auto server = s_serverRecords.find(id);

    if (server != s_serverRecords.end()) {
        return &server->second;
    }

    // A null callback only looks the cache up, it never sends a query.
    ItemStats_C* stats = g_itemDBCache.GetRecord(id, WOWGUID(), nullptr, nullptr, false);

    if (!stats) {
        return dbcRecord;
    }

    ItemRec& rec = s_cacheRecords[id];
    Fill(rec, id, stats->m_class, stats->m_subClass, stats->m_soundOverrideSubclass, stats->m_material, stats->m_displayId, stats->m_inventoryType, stats->m_sheatheType);
    return &rec;
}

static int32_t RefreshVisibleItems(WOWGUID guid, void* param) {
    int32_t itemId = *static_cast<int32_t*>(param);
    CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(guid, TYPE_PLAYER, __FILE__, __LINE__));

    if (!player || !player->m_characterComponent) {
        return 1;
    }

    for (int32_t slot = 0; slot < NUM_VISIBLE_SLOTS; slot++) {
        VisibleItem_C* item = &player->m_player->m_playerVisibleItems[slot];

        if (item->m_itemId != itemId) {
            continue;
        }

        if (slot < 15 || slot > 17) {
            player->m_characterComponent->RemoveItemBySlot(static_cast<INVENTORY_SLOTS>(slot));
        }

        player->EquipVisibleItem(item, slot);
    }

    return 1;
}

// Also registered on the realm connection so records arrive before character select draws.
static int32_t Packet_SMSG_ITEM_DB_CACHE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    int32_t id = static_cast<int32_t>(packet.GetUInt32());
    int32_t classID = packet.GetInt32();
    int32_t subclassID = packet.GetInt32();
    int32_t displayInfo = packet.GetInt32();
    int32_t inventoryType = packet.GetInt32();
    int32_t material = packet.GetInt32();
    int32_t soundOverride = packet.GetInt32();
    int32_t sheatheType = packet.GetInt32();

    if (id <= 0) {
        return 1;
    }

    Fill(s_serverRecords[id], id, classID, subclassID, soundOverride, material, displayInfo, inventoryType, sheatheType);

    if (ClntObjMgrGetActivePlayer()) {
        ClntObjMgrEnumVisibleObjects(RefreshVisibleItems, &id);
    }

    return 1;
}

class ItemsModule : public Module {
    public:
    const char* Name() const override {
        return "Items";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_ITEM_DB_CACHE, Packet_SMSG_ITEM_DB_CACHE);
        Net::RegisterRealmHandler(Net::SMSG_ITEM_DB_CACHE, Packet_SMSG_ITEM_DB_CACHE);
    }
};

Module* GetModule() {
    static ItemsModule module;
    return &module;
}

}
