#ifndef WOW_SOURCE_UI_MERCHANTFRAME_H
#define WOW_SOURCE_UI_MERCHANTFRAME_H

#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Common/ComponentCore/Types.h"

class ItemStats_C;
class CGItem_C;

struct lua_State;
constexpr int32_t MAX_MERCHANT_ITEMS = 150;

struct MerchantItem {
    /* 0000 */ int32_t m_slot;
    /* 0004 */ int32_t m_itemID;
    /* 0008 */ int32_t m_displayID;
    /* 000C */ int32_t m_maxCount;
    /* 0010 */ int32_t m_price;
    /* 0014 */ int32_t m_durability;
    /* 0018 */ int32_t m_buyCount;
    /* 001C */ int32_t m_extendedCostID;
};

struct BuybackEntry {
    /* 0000 */ uint32_t m_timestamp;
    /* 0004 */ uint32_t m_slot;
};

class CGMerchantInfo {
    public:
    static uint32_t m_buybackSlots[NUM_BUYBACK_SLOTS];
    static MerchantItem* GetItem(int32_t index);
    static ItemStats_C* GetItemStats(int32_t itemId);
    static uint32_t m_pendingItemStats;
    static void SetMerchant(WOWGUID merchant, const MerchantItem* items, int32_t count);
    static void RefreshList();
    static void UpdateItemQuantity(WOWGUID merchant, int32_t slot, int32_t quantity);
    static int32_t CanRepair();
    static bool IsBuybackItem(WOWGUID item);
    static void CloseMerchant();
    static void EnterWorld();
    static void LeaveWorld();
    static void SortBuybackItems();
    static MerchantItem* FindItemById(int32_t itemId);
    static int32_t GetRepairCost(CGItem_C* item);
    static MerchantItem m_items[MAX_MERCHANT_ITEMS];
    static WOWGUID m_merchant;
    static int32_t m_numMerchantItems;
    static uint32_t m_numBuybackItems;
};

void MerchantRegisterScriptFunctions();
void MerchantUnregisterScriptFunctions();

#endif
