#ifndef WOW_SOURCE_UI_UIMACROS_H
#define WOW_SOURCE_UI_UIMACROS_H

#include <cstddef>
#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Ui/MacroSubstring.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/AccountData.h"

struct lua_State;

struct MacroCommand {

    enum TYPE : int32_t {
        NONE = 0,
        SHOW = 1,
        CAST_OR_USE = 2,
        CAST_OR_USE_RANDOM = 3,
        CAST_SEQUENCE = 4,
        EQUIP = 5,
        EQUIP_TO_SLOT = 6,
    };

    /* 0000 */ int32_t m_type;
    /* 0004 */ MacroSubstring m_text;
};

class MACRONODE : public TSHashObject<MACRONODE, HASHKEY_NONE> {
    public:
    /* 0018 */ uint32_t m_id;
    /* 001C */ uint32_t m_type;
    /* 0020 */ char m_name[64];
    /* 0060 */ char m_icon[256];
    /* 0160 */ char m_body[1024];
    /* 0560 */ uint32_t m_actionBarCount;
    /* 0564 */ int32_t m_spellID;
    /* 0568 */ int32_t m_itemID;
    /* 056C */ int32_t m_spellFlags;
    /* 0570 */ uint32_t m_showTooltip;
    /* 0574 */ uint32_t m_dynamic;
    /* 0578 */ WOWGUID m_target;
    /* 0580 */ TSGrowableArray<MacroCommand> m_commands;
    /* 0590 */ TSLink<MACRONODE> m_dynamicLink;

    MACRONODE();
    ~MACRONODE();
    int32_t FindSpellID();
    int32_t Parse();
    void IncrementActionBarCount();
    void DecrementActionBarCount();
};

int32_t EvaluateMacroOptions(const char* start, const char* end, MacroSubstring* result, MacroSubstring* target);
void MacroQueryCastSequence(MacroSubstring* text);
constexpr uint32_t MACRO_SET_ACCOUNT = 0;
constexpr uint32_t MACRO_SET_CHARACTER = 1;
constexpr uint32_t NUM_MACRO_SETS = 2;
constexpr uint32_t MAX_ACCOUNT_MACROS = 36;
constexpr uint32_t MAX_CHARACTER_MACROS = 18;
constexpr uint32_t NUM_SORTED_MACRO_SLOTS = NUM_MACRO_SETS * MAX_ACCOUNT_MACROS;

class CGUIMacros {
    public:
    static void SaveMacros();
    static void Shutdown();
    static uint32_t m_state;
    static const char* m_macroRunning;
    static MACRONODE* m_macroButton;
    static uint32_t m_numMacros[NUM_MACRO_SETS];
    static int32_t m_saveMacros;
    static uint32_t m_sortedMacros[NUM_SORTED_MACRO_SLOTS];

    static MACRONODE* FindMacro(const char* name);
    static void RemoveDynamicMacro(MACRONODE* macro);
    static bool IsActionAllowed(uint32_t action);
    static void UpdateMacroSpells();
    static MACRONODE* GetMacroByID(uint32_t id);
    static int32_t GetMacroIndexByID(uint32_t id);
    static MACRONODE* GetMacroByIndex(uint32_t index);
    static void Initialize();
    static void LoadIcons();
    static int32_t SetMacroBody(MACRONODE* macro, const char* body);
    static void SortMacros();
    static void CheckActionBar();
    static void LoadMacros();
    static void LoadMacrosCallback(HOSFILE localFile, HOSFILE cacheFile, void* param);
    static void SaveMacrosCallback(HOSFILE localFile, HOSFILE cacheFile, void* param);
    static void RunMacro(MACRONODE* macro, const char* button);
    static void UpdateDynamicMacros();
    static void RunMacro(int32_t id, const char* button);
    static void GetFullIconPath(uint32_t id, char* buffer, uint32_t size);
    static int32_t GetSortedIndexByName(const char* name);
    static void EditMacro(uint32_t id, const char* name, const char* icon, const char* body);
    static void DeleteMacro(uint32_t id);
    static uint32_t CreateMacro(const char* name, const char* icon, const char* body, int32_t perCharacter);
    static void CountActionBarSlots(MACRONODE* macro);
};

void UIMacrosRegisterScriptFunctions();
void UIMacrosUnegisterScriptFunctions();

const char* GetMacroIcon(uint32_t index);
const char* GetMacroItemIcon(uint32_t index);
extern const char* s_macroIconPath;
extern TSGrowableArray<char*> s_macroIcons;
extern TSGrowableArray<char*> s_macroItemIcons;
uint32_t GetNumMacroIcons();

#endif
