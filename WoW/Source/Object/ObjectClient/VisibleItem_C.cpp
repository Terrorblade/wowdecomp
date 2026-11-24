#include "VisibleItem_C.h"
#include <cstdlib>
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/DB/Rec/ItemRec.h"
#include "WoW/Source/DB/Rec/ItemDisplayInfoRec.h"
#include "WoW/Source/DB/Rec/SpellItemEnchantmentRec.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Common/ComponentCore/Types.h"

// STATIC
// VERIFY: 9c793a34 auto
// OFFSET: 0x005EEB70
static void VisibleItemCacheCallback(int32_t id, const WOWGUID& guid, void* param, bool success) {
}

// VERIFY: 04ee8da3 auto
// OFFSET: 0x00758D30
uint32_t VisibleItem_C::GetClassID() {
    WOWGUID guid = WOWGUID();
    ItemStats_C* stats = g_itemDBCache.GetRecord(abs(this->m_itemId), guid, &VisibleItemCacheCallback, nullptr, true);

    return stats ? stats->m_class : 0;
}

// VERIFY: da6c80bd auto
// OFFSET: 0x00758D80
uint32_t VisibleItem_C::GetSubtypeID() {
    WOWGUID guid = WOWGUID();
    ItemStats_C* stats = g_itemDBCache.GetRecord(abs(this->m_itemId), guid, &VisibleItemCacheCallback, nullptr, true);

    return stats ? stats->m_subClass : 0;
}

// VERIFY: 0eef7107 auto
// OFFSET: 0x00758DD0
uint32_t VisibleItem_C::GetInventoryType() {
    WOWGUID guid = WOWGUID();
    ItemStats_C* stats = g_itemDBCache.GetRecord(abs(this->m_itemId), guid, &VisibleItemCacheCallback, nullptr, true);

    if (stats) {
        return stats->m_inventoryType;
    }

    ItemRec* rec = g_itemDB.GetRecord(abs(this->m_itemId));
    return rec ? rec->m_inventoryType : INVTYPE_NON_EQUIP;
}

// VERIFY: 9f906c98 auto
// OFFSET: 0x00758E50
uint32_t VisibleItem_C::GetDisplayID() {
    WOWGUID guid = WOWGUID();
    ItemStats_C* stats = g_itemDBCache.GetRecord(abs(this->m_itemId), guid, &VisibleItemCacheCallback, nullptr, true);

    if (stats) {
        return stats->m_displayId;
    }

    ItemRec* rec = g_itemDB.GetRecord(abs(this->m_itemId));
    return rec ? rec->m_displayInfoID : 0;
}

// VERIFY: dfd59b1b auto
// OFFSET: 0x00758ED0
uint32_t VisibleItem_C::GetMaterial() {
    WOWGUID guid = WOWGUID();
    ItemStats_C* stats = g_itemDBCache.GetRecord(abs(this->m_itemId), guid, &VisibleItemCacheCallback, nullptr, true);

    if (stats) {
        return stats->m_material;
    }

    ItemRec* rec = g_itemDB.GetRecord(abs(this->m_itemId));
    return rec ? rec->m_material : 0;
}

// VERIFY: 38f006d3 auto
// OFFSET: 0x00758F50
uint32_t VisibleItem_C::GetSheatheType() {
    WOWGUID guid = WOWGUID();
    ItemStats_C* stats = g_itemDBCache.GetRecord(abs(this->m_itemId), guid, &VisibleItemCacheCallback, nullptr, true);

    if (stats) {
        return stats->m_sheatheType;
    }

    ItemRec* rec = g_itemDB.GetRecord(abs(this->m_itemId));
    return rec ? rec->m_sheatheType : SHEATHE_NONE;
}

// VERIFY: 610fe897 auto
// OFFSET: 0x00758FD0
uint32_t VisibleItem_C::GetSoundOverrideSubclassID() {
    WOWGUID guid = WOWGUID();
    ItemStats_C* stats = g_itemDBCache.GetRecord(abs(this->m_itemId), guid, &VisibleItemCacheCallback, nullptr, true);

    if (stats) {
        return stats->m_soundOverrideSubclass;
    }

    ItemRec* rec = g_itemDB.GetRecord(abs(this->m_itemId));
    return rec ? rec->m_soundOverrideSubclassID : 0;
}

// VERIFY: e63c4fd5 manual
// OFFSET: 0x00759050
int32_t VisibleItem_C::GetItemEnchantmentVisualID() {
    ItemDisplayInfoRec displayRec;

    if (!g_itemDisplayInfoDB.GetRecord(this->GetDisplayID(), &displayRec)) {
        return 0;
    }

    if (g_itemVisualsDB.GetRecord(displayRec.m_itemVisual)) {
        return -1;
    }

    SpellItemEnchantmentRec* enchantment = g_spellItemEnchantmentDB.GetRecord(this->m_enchantment[0]);
    int32_t visual;

    if (enchantment && (visual = enchantment->m_itemVisual) != 0) {
        return visual;
    }

    enchantment = g_spellItemEnchantmentDB.GetRecord(this->m_enchantment[1]);

    if (enchantment && (visual = enchantment->m_itemVisual) != 0) {
        return visual;
    }

    return 0;
}
