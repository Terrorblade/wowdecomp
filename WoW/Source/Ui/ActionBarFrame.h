#ifndef WOW_SOURCE_UI_ACTIONBARFRAME_H
#define WOW_SOURCE_UI_ACTIONBARFRAME_H

#include <cstdint>
#include "WoW/Source/Object/WGUID.h"

struct lua_State;
class SpellRec;

class CGItem_C;
class PetAction;

PetAction* GetPetAction(uint32_t index);

class CGActionBar {
    public:
    static constexpr int32_t TEMP_ACTION_FIRST = 120;
    static constexpr int32_t NUM_TEMP_ACTION_SLOTS = 12;
    static constexpr int32_t MULTICAST_ACTION_FIRST = 132;
    static constexpr int32_t NUM_MULTICAST_ACTION_SLOTS = 12;
    static constexpr int32_t NUM_ACTION_SLOTS = 144;
    static constexpr int32_t NUM_ACTIONBAR_BUTTONS = 12;
    static constexpr int32_t NUM_ACTIONBAR_PAGES = 6;
    static constexpr int32_t MULTICAST_PAGE_OFFSET = MULTICAST_ACTION_FIRST / NUM_ACTIONBAR_BUTTONS + 1 - NUM_ACTIONBAR_PAGES;

    enum ACTION_TAG : uint32_t {
        ACTION_TAG_SPELL                    = 0x00000000,
        ACTION_TAG_PET_ACTION               = 0x10000000,
        ACTION_TAG_EQUIPMENT_SET            = 0x20000000,
        ACTION_TAG_MACRO                    = 0x40000000,
        ACTION_TAG_ITEM                     = 0x80000000,
    };

    enum ACTION_MASK : uint32_t {
        ACTION_MASK_TAG                      = 0xF0000000,
        ACTION_MASK_PET_ACTION_INDEX         = 0xEFFFFFFF,
        ACTION_MASK_EQUIPMENT_SET_ID         = 0xDFFFFFFF,
        ACTION_MASK_MACRO_ID                 = 0xBFFFFFFF,
        ACTION_MASK_ITEM_ID                  = 0x7FFFFFFF,
    };

    enum : uint32_t {
        TEMP_PAGE_NONE = 0,
        TEMP_PAGE_PET = 1,
        TEMP_PAGE_SHAPESHIFT = 2,
        TEMP_PAGE_OVERRIDE_SPELLS = 0x4,
    };

    static void LeaveWorld();
    static void SignalUpdateCooldown();
    static uint32_t m_tempPageActiveFlags;
    static uint32_t m_currentPage;
    static int32_t m_bonusBarOffset;
    static void UpdateOverrideSpellBar();
    static uint32_t GetSlotSpellAction(uint32_t slot);
    static void UpdateTemporaryActionBar(int32_t (*getSpell)(uint32_t index, const void* data), const void* data);
    static void EnterWorld();
    static int32_t m_slotNoMana[NUM_ACTION_SLOTS];
    static int32_t m_slotUsable[NUM_ACTION_SLOTS];
    static int32_t m_slotItemCount[NUM_ACTION_SLOTS];
    static uint32_t m_slotActions[NUM_ACTION_SLOTS];

    static int32_t IsMacro(uint32_t slot);
    static void OnAuraChanged(int32_t spellId);
    static uint32_t GetMacroID(uint32_t slot);
    static int32_t IsItem(uint32_t slot);
    static int32_t IsPetAction(uint32_t slot);
    static int32_t IsEquipmentSet(uint32_t slot);
    static int32_t GetEquipmentSetID(uint32_t slot);
    static int32_t HasMacro(uint32_t slot);
    static void PickupAction(uint32_t slot);
    static int32_t GetItem(uint32_t slot);
    static int32_t GetMacroItem(uint32_t slot);
    static int32_t GetMacroSpell(uint32_t slot, int32_t& petSpell);
    static int32_t GetItemSpell(uint32_t slot);
    static int32_t GetPetActionSpell(uint32_t slot);
    static CGItem_C* FindUseEnchantItemPtr(int32_t itemId);
    static int32_t IsEquippedAction(uint32_t slot);
    static int32_t IsConsumableAction(uint32_t slot);
    static int32_t GetSpell(uint32_t slot, int32_t& petSpell);
    static int32_t IsAttackAction(uint32_t slot);
    static int32_t IsAutoRepeatAction(uint32_t slot);
    static int32_t HasValidTarget(uint32_t slot, WOWGUID target, int32_t* inRange);
    static int32_t IsRangedAttackAction(uint32_t slot);
    static int32_t UpdateUsableAction(uint32_t slot, int32_t& noMana);
    static void SetAction(int32_t slot, uint32_t action, int32_t sendToServer, int32_t silent);
    static void UpdateBonusBar();
    static void SetTemporaryActionBarFlags(uint32_t flags, int32_t enable);
    static void UpdateShapeShiftBar();
    static void InitializeGame();
    static void ShutdownGame();
    static void UpdateAttackAction();
    static void UpdateItem(int32_t itemId);
    static void UpdateRangedAttackAction();
    static void HideGrid();
    static void UpdateUsable();
    static void UpdateUsablePower();
    static void UpdateCritter(int32_t critter);
    static void RemoveAction(uint32_t slot);
    static int32_t ActionHasRange(uint32_t slot);
    static void UpdateEquipmentSet(uint32_t id);
    static void RemoveEquipmentSet(uint32_t id);
    static void ReplaceSpell(int32_t oldSpell, int32_t newSpell);
    static int32_t HasAction(uint32_t slot);
    static void PutActionInSlot(int32_t slot);
    static void UpdateMultiCastBar();
    static void RemoveMacro(int32_t spellId);
    static void RemoveMacroActions(uint32_t macroId);
    static uint32_t GetMultiCastSlotMaskForTotemCategory(int32_t category);
    static bool IsEquipmentSetValid(int32_t setId);
    static int32_t IsSpell(uint32_t slot);
    static void ShowGrid();
    static void UpdateSelection();
    static const char* GetAttackTexture();
    static const char* GetRangedTexture();
    static int32_t CalculateItemCount(int32_t itemId);
    static void GetCooldown(int32_t slot, uint32_t& start, uint32_t& duration, uint32_t& enable);
    static const char* GetTexture(int32_t slot);
    static int32_t IsCurrentAction(int32_t slot);
    static void SlotChanged(int32_t slot, int32_t sendToServer, int32_t silent);
    static void UseAction(int32_t slot, const WOWGUID& target, const char* button);
    static void UpdateMacro(uint32_t macroId);
};

void ItemStatsCallback(int32_t id, const WOWGUID& guid, void* param, bool success);

int32_t IsStackableAucItem(uint32_t slot);
int32_t IsMultiCastSpellForSlot(int32_t slot, const SpellRec* spell, int32_t* category);
void ActionBarRegisterScriptFunctions();
void ActionBarUnregisterScriptFunctions();

#endif
