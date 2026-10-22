#include "Bag_C.h"
#include "WoW/Source/DB/DBClient/DBCacheInstances.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ItemStats.h"
#include "WoW/Source/Object/ObjectClient/Item_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/CGBankInfo.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/Types.h"
#include "WoW/Source/Ui/AuctionHouse.h"
#include "WoW/Source/Ui/MailInfo.h"
#include "WoW/Source/Ui/TradeFrame.h"
#include "WoW/Source/UIUtil/Tooltip.h"

// STATIC: 0x00A37B10
static const BAGSLOTRANGE s_bagSlotRanges[8] = { { 23, 38 }, { 19, 22 }, { 86, 117 }, { 39, 66 }, { 67, 73 }, { 118, 149 }, { 0, 18 }, { 74, 85 } };

// VERIFY: 3944bddf auto
// OFFSET: 0x00753B00
static int32_t FindBagFamilyCallback(const CGItem_C* item, void* param) {
    return *static_cast<uint32_t*>(param) & item->GetBagFamily();
}

// VERIFY: d68f4a94 auto
// OFFSET: 0x00753FA0
static int32_t UnlockItemTypeCallback(const CGItem_C* item, void* param) {
    ItemTypeCountParam* count = static_cast<ItemTypeCountParam*>(param);

    if ((static_cast<int32_t>(item->m_obj->m_entryID) != count->m_itemId && count->m_itemId != -1) || CGTradeInfo::IsItemBeingTraded(item->m_obj->m_guid)) {
        return 0;
    }

    WOWGUID guid = item->m_obj->m_guid;

    if (!CGMailInfo::IsSendItem(guid) && guid != CGAuctionHouse::m_sellItem && !CGAuctionHouse::IsItemForSell(guid)) {
        const_cast<CGItem_C*>(item)->Unlock();
    }

    return 0;
}

// VERIFY: 94284adc manual
// OFFSET: 0x00589C00
int32_t CGBag_C::FindItemSlot(WOWGUID guid) const {
    for (uint32_t slot = 0; slot < static_cast<uint32_t>(this->m_numSlots); slot++) {
        if (this->m_slots[slot] == guid) {
            return slot;
        }
    }

    return -1;
}

// VERIFY: 578d255a manual
// OFFSET: 0x00754040
int32_t CGBag_C::FindItemSlot(const CGItem_C* item) const {
    if (!item) {
        return -1;
    }

    for (uint32_t slot = 0; slot < static_cast<uint32_t>(this->m_numSlots); slot++) {
        WOWGUID guid = slot < static_cast<uint32_t>(this->m_numSlots) ? this->m_slots[slot] : WOWGUID();

        if (ClntObjMgrObjectPtr(guid, TYPEMASK_ITEM, __FILE__, __LINE__) == item) {
            return slot;
        }
    }

    return -1;
}

// VERIFY: 121306f8 manual
// OFFSET: 0x00754390
const CGItem_C* CGBag_C::GetItemPointer(int32_t slot) const {
    if ((this->m_hasBank && ((slot >= 39 && slot <= 66) || static_cast<uint32_t>(slot - 67) <= 6) && !CGBankInfo::m_banker) || slot >= this->m_numSlots) {
        return nullptr;
    }

    WOWGUID guid = static_cast<uint32_t>(slot) < static_cast<uint32_t>(this->m_numSlots) ? this->m_slots[slot] : WOWGUID();

    return static_cast<CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPEMASK_ITEM, __FILE__, __LINE__));
}

// VERIFY: 6a3b7079 manual
// OFFSET: 0x00754400
CGItem_C* CGBag_C::GetItemPointer(int32_t slot) {
    if ((this->m_hasBank && ((slot >= 39 && slot <= 66) || static_cast<uint32_t>(slot - 67) <= 6) && !CGBankInfo::m_banker) || slot >= this->m_numSlots) {
        return nullptr;
    }

    WOWGUID guid = static_cast<uint32_t>(slot) < static_cast<uint32_t>(this->m_numSlots) ? this->m_slots[slot] : WOWGUID();

    return static_cast<CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPEMASK_ITEM, __FILE__, __LINE__));
}

// VERIFY: 8829b660 manual
// OFFSET: 0x00754470
int32_t CGBag_C::GetNumFreeInvSlots() {
    int32_t freeSlots = 0;

    for (uint32_t slot = 0; slot < static_cast<uint32_t>(this->m_numSlots); slot++) {
        if (this->m_hasBank) {
            if (slot <= 18 || (slot >= 39 && slot <= 66) || (slot >= 67 && slot <= 73) || (slot >= 86 && slot <= 117) || (slot >= 74 && slot <= 85) || slot - 118 <= 31) {
                continue;
            }

            if (slot - 19 <= 3) {
                WOWGUID guid = slot < static_cast<uint32_t>(this->m_numSlots) ? this->m_slots[slot] : WOWGUID();
                CGItem_C* item = static_cast<CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPEMASK_ITEM, __FILE__, __LINE__));

                if (item && item->GetClassID() == 1) {
                    freeSlots += item->GetBag()->GetNumFreeInvSlots();
                }

                continue;
            }
        }

        if (slot >= static_cast<uint32_t>(this->m_numSlots) || !this->m_slots[slot]) {
            freeSlots++;
        }
    }

    return freeSlots;
}

// VERIFY: 8d4f2769 auto
// OFFSET: 0x00754560
int32_t GetItemTypeCountCallback(const CGItem_C* item, void* param) {
    ItemTypeCountParam* count = static_cast<ItemTypeCountParam*>(param);

    if (static_cast<int32_t>(item->m_obj->m_entryID) == count->m_itemId || count->m_itemId == -1) {
        count->m_count += item->m_item->m_itemFieldStackCount;
    }

    return 0;
}

// VERIFY: af9218e7 manual
// OFFSET: 0x00754940
uint32_t CGBag_C::GetAvgEquippedDurability() const {
    int32_t durability = 0;
    int32_t maxDurability = 0;

    for (uint32_t slot = 0; slot <= 18; slot++) {
        if ((this->m_hasBank && ((slot >= 39 && slot <= 66) || slot - 67 <= 6) && !CGBankInfo::m_banker) || static_cast<int32_t>(slot) >= this->m_numSlots) {
            continue;
        }

        WOWGUID guid = slot < static_cast<uint32_t>(this->m_numSlots) ? this->m_slots[slot] : WOWGUID();
        CGItem_C* item = static_cast<CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPEMASK_ITEM, __FILE__, __LINE__));

        if (!item) {
            continue;
        }

        durability += (item->m_item->m_itemFieldFlags >> 3) & 1 ? 0 : item->m_item->m_itemFieldDurability;
        maxDurability += (item->m_item->m_itemFieldFlags >> 3) & 1 ? 0 : item->m_item->m_itemFieldMaxdurability;
    }

    if (!maxDurability) {
        return 0;
    }

    return static_cast<uint32_t>(static_cast<int64_t>(static_cast<float>(durability) / maxDurability * 100.0f));
}

// VERIFY: c1b9cf4f auto
// OFFSET: 0x005125B0
int32_t CGBag_C::GetNumItems() const {
    int32_t count = 0;

    for (int32_t i = 0; i < this->m_numSlots; i++) {
        if (this->m_slots[i]) {
            count++;
        }
    }

    return count;
}

// VERIFY: 724d9331 auto
// OFFSET: 0x00753A50
static int32_t __stdcall SlotMatchesSearchFlags(uint32_t slot, uint32_t flags) {
    return (slot <= 18 && (flags & 0x1)) || (slot - 23 <= 15 && (flags & 0x4)) || (slot - 19 <= 3 && (flags & 0x2)) || (((slot >= 39 && slot <= 66) || slot - 67 <= 6) && (flags & 0x8)) || (slot - 86 <= 31 && (flags & 0x40)) || (slot - 118 <= 31 && (flags & 0x200));
}

// VERIFY: ddfa3202 auto
// OFFSET: 0x00753B70
static int32_t FindItemFullNameCallback(const CGItem_C* item, void* param) {
    char fullName[1024];
    item->GetFullItemName(fullName, sizeof(fullName));
    return SStrCmpUTF8I(static_cast<const char*>(param), fullName, STORM_MAX_STR) == 0;
}

// VERIFY: e5b71395 auto
// OFFSET: 0x00753BB0
int32_t GetDisplayErrorByInventoryError(int32_t error) {
    switch (error) {
        case 0:
        case 81:
        case 83:
            return ERR_MAX_VALUE;

        case 1:
            return ERR_CANT_EQUIP_LEVEL_I;

        case 2:
            return ERR_CANT_EQUIP_SKILL;

        case 3:
            return ERR_WRONG_SLOT;

        case 5:
            return ERR_BAG_IN_BAG;

        case 6:
            return ERR_TRADE_EQUIPPED_BAG;

        case 7:
            return ERR_AMMO_ONLY;

        case 8:
            return ERR_PROFICIENCY_NEEDED;

        case 9:
        case 12:
        case 18:
            return ERR_NO_SLOT_AVAILABLE;

        case 10:
        case 11:
            return ERR_CANT_EQUIP_EVER;

        case 13:
            return ERR_2HANDED_EQUIPPED;

        case 14:
            return ERR_2HSKILLNOTFOUND;

        case 15:
        case 16:
            return ERR_WRONG_BAG_TYPE;

        case 17:
            return ERR_ITEM_MAX_COUNT;

        case 19:
        case 55:
            return ERR_CANT_STACK;

        case 20:
            return ERR_NOT_EQUIPPABLE;

        case 21:
            return ERR_CANT_SWAP;

        case 22:
            return ERR_SLOT_EMPTY;

        case 23:
        case 54:
            return ERR_ITEM_NOT_FOUND;

        case 24:
            return ERR_DROP_BOUND_ITEM;

        case 25:
            return ERR_OUT_OF_RANGE;

        case 26:
            return ERR_TOO_FEW_TO_SPLIT;

        case 27:
            return ERR_SPLIT_FAILED;

        case 28:
            return ERR_SPELL_FAILED_REAGENTS_GENERIC;

        case 29:
            return ERR_NOT_ENOUGH_MONEY;

        case 30:
            return ERR_NOT_A_BAG;

        case 31:
            return ERR_DESTROY_NONEMPTY_BAG;

        case 32:
            return ERR_NOT_OWNER;

        case 33:
            return ERR_ONLY_ONE_QUIVER;

        case 34:
            return ERR_NO_BANK_SLOT;

        case 35:
            return ERR_NO_BANK_HERE;

        case 36:
            return ERR_ITEM_LOCKED;

        case 37:
            return ERR_GENERIC_STUNNED;

        case 38:
            return ERR_PLAYER_DEAD;

        case 39:
            return ERR_CLIENT_LOCKED_OUT;

        case 40:
        case 73:
            return ERR_INTERNAL_BAG_ERROR;

        case 41:
            return ERR_ONLY_ONE_BOLT;

        case 42:
            return ERR_ONLY_ONE_AMMO;

        case 43:
            return ERR_CANT_WRAP_STACKABLE;

        case 44:
            return ERR_CANT_WRAP_EQUIPPED;

        case 45:
            return ERR_CANT_WRAP_WRAPPED;

        case 46:
            return ERR_CANT_WRAP_BOUND;

        case 47:
            return ERR_CANT_WRAP_UNIQUE;

        case 48:
            return ERR_CANT_WRAP_BAGS;

        case 49:
            return ERR_LOOT_GONE;

        case 50:
            return ERR_INV_FULL;

        case 51:
            return ERR_BANK_FULL;

        case 52:
        case 57:
            return ERR_VENDOR_SOLD_OUT;

        case 58:
            return ERR_OBJECT_IS_BUSY;

        case 59:
            return ERR_CANT_BE_DISENCHANTED;

        case 60:
            return ERR_NOT_IN_COMBAT;

        case 61:
            return ERR_NOT_WHILE_DISARMED;

        case 63:
            return ERR_CANT_EQUIP_RANK;

        case 64:
            return ERR_CANT_EQUIP_REPUTATION;

        case 65:
            return ERR_TOO_MANY_SPECIAL_BAGS;

        case 66:
            return ERR_LOOT_CANT_LOOT_THAT_NOW;

        case 67:
            return ERR_ITEM_UNIQUE_EQUIPPABLE;

        case 68:
            return ERR_VENDOR_MISSING_TURNINS;

        case 69:
            return ERR_NOT_ENOUGH_HONOR_POINTS;

        case 70:
            return ERR_NOT_ENOUGH_ARENA_POINTS;

        case 71:
            return ERR_ITEM_MAX_COUNT_SOCKETED;

        case 72:
            return ERR_MAIL_BOUND_ITEM;

        case 75:
            return ERR_ITEM_MAX_COUNT_EQUIPPED_SOCKETED;

        case 76:
            return ERR_ITEM_UNIQUE_EQUIPPABLE_SOCKETED;

        case 77:
            return ERR_TOO_MUCH_GOLD;

        case 78:
            return ERR_NOT_DURING_ARENA_MATCH;

        case 79:
            return ERR_TRADE_BOUND_ITEM;

        case 80:
            return ERR_CANT_EQUIP_RATING;

        case 82:
            return ERR_NOT_SAME_ACCOUNT;

        case 84:
            return ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS;

        case 85:
            return ERR_ITEM_MAX_LIMIT_CATEGORY_SOCKETED_EXCEEDED_IS;

        case 86:
            return ERR_SCALING_STAT_ITEM_LEVEL_EXCEEDED;

        case 87:
            return ERR_PURCHASE_LEVEL_TOO_LOW;

        case 88:
            return ERR_CANT_EQUIP_NEED_TALENT;

        case 89:
            return ERR_ITEM_MAX_LIMIT_CATEGORY_EQUIPPED_EXCEEDED_IS;

        case 90:
            return ERR_SHAPESHIFT_FORM_CANNOT_EQUIP;

        case 91:
            return ERR_ITEM_INVENTORY_FULL_SATCHEL;

        default:
            return ERR_BAG_FULL;
    }
}

// VERIFY: 9c2bc99e auto
// OFFSET: 0x00753F60
int32_t GetItemTypeChargesCallback(const CGItem_C* item, void* param) {
    ItemTypeCountParam* count = static_cast<ItemTypeCountParam*>(param);

    if ((static_cast<int32_t>(item->m_obj->m_entryID) == count->m_itemId || count->m_itemId == -1) && const_cast<CGItem_C*>(item)->GetEnchantmentSpell(0)) {
        int32_t charges = const_cast<CGItem_C*>(item)->GetCurrentSpellCharges();

        if (charges <= 0) {
            charges = -charges;
        }

        count->m_count += charges;
    }

    return 0;
}

// VERIFY: b4bf3df4 auto
// OFFSET: 0x00754020
int32_t FindItemIDCallback(const CGItem_C* item, void* param) {
    return static_cast<int32_t>(item->m_obj->m_entryID) == *static_cast<int32_t*>(param);
}

// VERIFY: 45f963f6 manual
// OFFSET: 0x007541F0
bool CGBag_C::FindTotemCategory(int32_t type, int32_t& mask, bool recurse, uint32_t flags) {
    if (!(flags & 0x7)) {
        flags |= 0x7;
    }

    uint32_t slot = this->m_hasBank ? s_bagSlotRanges[0].m_first : 0;

    while (slot < static_cast<uint32_t>(this->m_numSlots)) {
        if (!this->m_hasBank || SlotMatchesSearchFlags(slot, flags)) {
            WOWGUID guid = slot < static_cast<uint32_t>(this->m_numSlots) ? this->m_slots[slot] : WOWGUID();
            CGItem_C* item = static_cast<CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPEMASK_ITEM, __FILE__, __LINE__));

            if (item && (!this->m_hasBank || slot < 74 || slot > 85)) {
                WOWGUID none = { 0 };
                ItemStats_C* stats = g_itemDBCache.GetRecord(item->m_obj->m_entryID, none, nullptr, nullptr, false);

                if (stats && stats->m_totemCategory) {
                    TotemCategoryRec* category = g_totemCategoryDB.GetRecord(stats->m_totemCategory);

                    if (category && category->m_totemCategoryType == type) {
                        mask &= ~category->m_totemCategoryMask;

                        if (!mask) {
                            return true;
                        }
                    }
                }

                if (recurse && (item->m_obj->m_type & TYPEMASK_CONTAINER) && item->GetBag() && item->GetBag()->FindTotemCategory(type, mask, false, flags)) {
                    return true;
                }
            }
        }

        if (slot < static_cast<uint32_t>(this->m_numSlots)) {
            uint32_t i = 0;

            if (this->m_hasBank) {
                for (; i < sizeof(s_bagSlotRanges) / sizeof(s_bagSlotRanges[0]); i++) {
                    if (s_bagSlotRanges[i].m_last == slot) {
                        break;
                    }
                }
            }

            if (this->m_hasBank && i < sizeof(s_bagSlotRanges) / sizeof(s_bagSlotRanges[0])) {
                slot = i >= sizeof(s_bagSlotRanges) / sizeof(s_bagSlotRanges[0]) - 1 ? this->m_numSlots : s_bagSlotRanges[i + 1].m_first;
            } else {
                slot++;
            }
        } else {
            slot = this->m_numSlots;
        }
    }

    return false;
}

// VERIFY: 0476a53f manual
// OFFSET: 0x007540B0
bool CGBag_C::ContainsItem(WOWGUID guid, bool recurse, uint32_t flags) const {
    int32_t hasBank = this->m_hasBank;
    uint32_t slot = hasBank ? s_bagSlotRanges[0].m_first : 0;

    while (slot < static_cast<uint32_t>(this->m_numSlots)) {
        if (!hasBank || SlotMatchesSearchFlags(slot, flags)) {
            WOWGUID slotGuid = slot < static_cast<uint32_t>(this->m_numSlots) ? this->m_slots[slot] : WOWGUID();
            CGItem_C* item = static_cast<CGItem_C*>(ClntObjMgrObjectPtr(slotGuid, TYPEMASK_ITEM, __FILE__, __LINE__));

            if (item && (!hasBank || slot < 74 || slot > 85)) {
                WOWGUID itemGuid = slot < static_cast<uint32_t>(this->m_numSlots) ? this->m_slots[slot] : WOWGUID();

                if (itemGuid == guid) {
                    return true;
                }

                if (recurse && (item->m_obj->m_type & TYPEMASK_CONTAINER) && item->GetBag() && item->GetBag()->ContainsItem(guid, false, flags)) {
                    return true;
                }
            }
        }

        if (slot < static_cast<uint32_t>(this->m_numSlots)) {
            uint32_t i = 0;

            if (this->m_hasBank) {
                for (; i < sizeof(s_bagSlotRanges) / sizeof(s_bagSlotRanges[0]); i++) {
                    if (s_bagSlotRanges[i].m_last == slot) {
                        break;
                    }
                }
            }

            if (this->m_hasBank && i < sizeof(s_bagSlotRanges) / sizeof(s_bagSlotRanges[0])) {
                slot = i >= sizeof(s_bagSlotRanges) / sizeof(s_bagSlotRanges[0]) - 1 ? this->m_numSlots : s_bagSlotRanges[i + 1].m_first;
            } else {
                slot++;
            }
        } else {
            slot = this->m_numSlots;
        }
    }

    return false;
}

// VERIFY: 6ed45ee8 auto
// OFFSET: 0x007548F0
bool CGBag_C::FindTotemCategory(int32_t category, uint32_t flags) {
    TotemCategoryRec* rec = g_totemCategoryDB.GetRecord(category);

    if (!rec) {
        return false;
    }

    int32_t mask = rec->m_totemCategoryMask;
    return this->FindTotemCategory(rec->m_totemCategoryType, mask, true, flags);
}

// VERIFY: 12f34396 auto
// OFFSET: 0x00754A20
CGItem_C* CGBag_C::FindItemOfType(int32_t itemId, uint32_t flags) {
    WOWGUID container;
    uint32_t slot;
    return this->FindItem(FindItemIDCallback, &itemId, container, slot, flags);
}

// VERIFY: ef1d9110 auto
// OFFSET: 0x00754A50
CGItem_C* CGBag_C::FindItemOfType(int32_t itemId, WOWGUID& container, uint32_t& slot, uint32_t flags) {
    return this->FindItem(FindItemIDCallback, &itemId, container, slot, flags);
}

// VERIFY: 98f5a653 auto
// OFFSET: 0x00754D00
int32_t CGBag_C::GetItemTypeCount(int32_t itemId, uint32_t flags) {
    if (itemId == -2) {
        return this->GetNumFreeInvSlots();
    }

    ItemTypeCountParam param;
    param.m_itemId = itemId;
    param.m_count = 0;
    WOWGUID container;
    uint32_t slot;
    this->FindItem(GetItemTypeCountCallback, &param, container, slot, flags);
    return param.m_count;
}

// VERIFY: a7154eb9 auto
// OFFSET: 0x00754D50
int32_t CGBag_C::GetItemTypeCharges(int32_t itemId, uint32_t flags) {
    ItemTypeCountParam param;
    param.m_itemId = itemId;
    param.m_count = 0;
    WOWGUID container;
    uint32_t slot;
    this->FindItem(GetItemTypeChargesCallback, &param, container, slot, flags);
    return param.m_count;
}

// VERIFY: 0a0051b7 auto
// OFFSET: 0x00753AC0
int32_t FindItemClassCallback(const CGItem_C* item, void* param) {
    const uint32_t* classParam = static_cast<const uint32_t*>(param);
    return item->GetClassID() == classParam[0] && (!classParam[1] || ((1 << item->GetSubtypeID()) & classParam[1]));
}

// VERIFY: 4b4db806 auto
// OFFSET: 0x00754A80
CGItem_C* CGBag_C::FindItemOfClass(int32_t classId, uint32_t subclassMask, uint32_t flags) {
    uint32_t param[2];
    param[0] = classId;
    param[1] = subclassMask;
    WOWGUID container;
    uint32_t slot;
    return this->FindItem(FindItemClassCallback, param, container, slot, flags);
}

// VERIFY: 3ef5ccd0 manual
// OFFSET: 0x00512590
uint64_t CGBag_C::GetItemGuid(uint32_t slot) const {
    if (slot < static_cast<uint32_t>(this->m_numSlots)) {
        return this->m_slots[slot].m_guid;
    }

    return 0;
}

// VERIFY: eb9a8ff7 manual
// OFFSET: 0x007545D0
static int32_t FindItemLinkCallback(const CGItem_C* item, void* param) {
    const ItemLinkInfo* link = static_cast<const ItemLinkInfo*>(param);
    CGItem_C* object = const_cast<CGItem_C*>(item);

    if (static_cast<int32_t>(link->m_itemId) != object->m_obj->m_entryID) {
        return 0;
    }

    if (object->m_item->m_itemFieldFlags & 0x8) {
        if (link->m_enchants[0]) {
            return 0;
        }

        for (uint32_t i = 0; i < 3; i++) {
            if (link->m_enchants[2 + i]) {
                return 0;
            }
        }

        if (link->m_randomPropertiesId) {
            return 0;
        }

        return !link->m_propertySeed;
    }

    if (link->m_enchants[0] && link->m_enchants[0] != object->GetEnchantmentId(0)) {
        return 0;
    }

    for (uint32_t i = 0; i < 3; i++) {
        if (link->m_enchants[2 + i] && link->m_enchants[2 + i] != object->GetEnchantmentId(2 + i)) {
            return 0;
        }
    }

    if (link->m_randomPropertiesId && link->m_randomPropertiesId != object->m_item->m_itemFieldRandomPropertiesId) {
        return 0;
    }

    if (link->m_propertySeed && link->m_propertySeed != object->m_item->m_itemFieldPropertySeed) {
        return 0;
    }

    return 1;
}

// VERIFY: b7f49e2f auto
// OFFSET: 0x00753B20
static int32_t FindItemNameCallback(const CGItem_C* item, void* param) {
    const char* name = static_cast<const char*>(param);
    int32_t length = SStrLenUTF8(name);
    char fullName[1024];
    item->GetFullItemName(fullName, sizeof(fullName));
    return SStrCmpUTF8I(name, fullName, length) == 0;
}

// VERIFY: 19b67d6e manual
// OFFSET: 0x00754AF0
CGItem_C* CGBag_C::FindItemByName(const char* name, uint32_t flags) {
    if (!name || !*name) {
        return nullptr;
    }

    WOWGUID container;
    uint32_t slot;

    if (!SStrStr(name, "item:")) {
        return this->FindItem(&FindItemNameCallback, const_cast<char*>(name), container, slot, flags);
    }

    ItemLinkInfo link;
    link.Clear();
    link.GetItemIdFromLink(name);
    return this->FindItem(&FindItemLinkCallback, &link, container, slot, flags);
}

// VERIFY: 57bdbcdf manual
// OFFSET: 0x00754B90
CGItem_C* CGBag_C::GetItemIdByName(const char* name, WOWGUID& container, uint32_t& slot, uint32_t flags) {
    if (!name || !*name) {
        return nullptr;
    }

    if (!SStrStr(name, "item:")) {
        return this->FindItem(&FindItemNameCallback, const_cast<char*>(name), container, slot, flags);
    }

    ItemLinkInfo link;
    link.Clear();
    link.GetItemIdFromLink(name);
    return this->FindItem(&FindItemLinkCallback, &link, container, slot, flags);
}

// VERIFY: f40a505c manual
// OFFSET: 0x00754C30
CGItem_C* CGBag_C::FindItemByFullName(const char* name, uint32_t flags) {
    if (!name || !*name) {
        return nullptr;
    }

    if (!SStrStr(name, "item:")) {
        return this->FindItem(&FindItemFullNameCallback, const_cast<char*>(name), flags);
    }

    ItemLinkInfo link;
    link.Clear();
    link.GetItemIdFromLink(name);
    return this->FindItem(&FindItemLinkCallback, &link, flags);
}

// VERIFY: cfc4d837 auto
// OFFSET: 0x00754CD0
CGItem_C* CGBag_C::FindItem(int32_t (*callback)(const CGItem_C* item, void* param), void* param, uint32_t flags) {
    WOWGUID container;
    return this->FindItem(callback, param, container, flags, flags);
}

// VERIFY: 35fbcc70 manual
// OFFSET: 0x007546F0
CGItem_C* CGBag_C::FindItem(int32_t (*callback)(const CGItem_C* item, void* param), void* param, WOWGUID& container, uint32_t& slot, uint32_t flags) {
    if (this->m_hasBank && !(flags & 0x247)) {
        flags |= 0x247;
    }

    container = WOWGUID();
    slot = 0;
    uint32_t numSlots = this->m_numSlots;
    CGItem_C* best = nullptr;
    uint32_t index = this->m_hasBank ? 23 : 0;

    if (index >= numSlots) {
        return best;
    }

    while (true) {
        if (!this->m_hasBank || SlotMatchesSearchFlags(index, flags)) {
            WOWGUID guid = index < numSlots ? this->m_slots[index] : WOWGUID();
            CGItem_C* item = static_cast<CGItem_C*>(ClntObjMgrObjectPtr(guid, TYPEMASK_ITEM, __FILE__, __LINE__));

            if (item && !(item->m_modelFlags & 0x10000) && ((flags & 0x80) || (((item->m_item->m_itemFieldFlags & 0x8) || !item->m_item->m_itemFieldMaxdurability || item->m_item->m_itemFieldDurability) && !(item->m_item->m_itemFieldFlags & 0x10))) && (!(flags & 0x20) || (item->GetBag() && !(flags & 0x10)) || (item->GetEnchantmentSpell(0) && item->GetCurrentSpellCharges()))) {
                if (callback(item, param)) {
                    container = this->m_guid;
                    slot = index;
                } else if (item->GetBag() && !(flags & 0x10)) {
                    item = item->GetBag()->FindItem(callback, param, container, slot, flags);
                } else {
                    item = nullptr;
                }

                if (item) {
                    if (!(flags & 0x100)) {
                        return item;
                    }

                    if (!best || item->m_item->m_itemFieldStackCount < best->m_item->m_itemFieldStackCount) {
                        best = item;
                    }
                }
            }
        }

        numSlots = this->m_numSlots;

        if (index >= numSlots) {
            index = numSlots;
        } else if (this->m_hasBank) {
            uint32_t range = 0;

            while (s_bagSlotRanges[range].m_last != index) {
                if (++range >= 8) {
                    break;
                }
            }

            if (range >= 8) {
                index++;
            } else if (range >= 7) {
                index = numSlots;
            } else {
                index = s_bagSlotRanges[range + 1].m_first;
            }
        } else {
            index++;
        }

        if (index >= numSlots) {
            return best;
        }
    }
}

// VERIFY: 82130edd auto
// OFFSET: 0x00754590
int32_t GetItemLimitCategoryCallback(const CGItem_C* item, void* param) {
    int32_t* data = static_cast<int32_t*>(param);

    if (item && item->GetStats() && static_cast<int32_t>(item->GetStats()->m_itemLimitCategory) == data[0]) {
        data[1] += item->m_item->m_itemFieldStackCount;
    }

    return 0;
}

// VERIFY: 2670f6b3 auto
// OFFSET: 0x00754DD0
bool CGBag_C::ItemCountValid(int32_t itemId, const ItemStats_C* stats, int32_t count, bool showError) {
    if (stats->m_maxCount) {
        int32_t current;

        if (itemId == -2) {
            current = this->GetNumFreeInvSlots();
        } else {
            int32_t data[2] = { itemId, 0 };
            WOWGUID container;
            uint32_t slot;
            this->FindItem(GetItemTypeCountCallback, data, container, slot, 79);
            current = data[1];
        }

        if (count + current > static_cast<int32_t>(stats->m_maxCount)) {
            if (showError) {
                CGGameUI::DisplayError(ERR_ITEM_MAX_COUNT);
            }

            return false;
        }
    }

    if (stats->m_itemLimitCategory) {
        ItemLimitCategoryRec* category = g_itemLimitCategoryDB.GetRecord(stats->m_itemLimitCategory);
        int32_t data[2] = { static_cast<int32_t>(stats->m_itemLimitCategory), 0 };
        WOWGUID container;
        uint32_t slot;
        this->FindItem(GetItemLimitCategoryCallback, data, container, slot, 79);

        if (count + data[1] > category->m_quantity) {
            if (showError) {
                CGGameUI::DisplayError(ERR_ITEM_MAX_LIMIT_CATEGORY_COUNT_EXCEEDED_IS, category->m_quantity, category->m_name);
            }

            return false;
        }
    }

    return true;
}

// VERIFY: b3aecff9 auto
// OFFSET: 0x00754D90
void CGBag_C::UnlockItemsOfType(int32_t itemId, uint32_t flags) {
    ItemTypeCountParam param;
    param.m_itemId = itemId;
    param.m_count = 0;
    WOWGUID container;
    uint32_t slot;
    this->FindItem(UnlockItemTypeCallback, &param, container, slot, flags);
}

// VERIFY: 095e35cd auto
// OFFSET: 0x00754AC0
CGItem_C* CGBag_C::FindItemOfBagFamily(uint32_t bagFamily, uint32_t flags) {
    WOWGUID container;
    uint32_t slot;
    return this->FindItem(FindBagFamilyCallback, &bagFamily, container, slot, flags);
}
