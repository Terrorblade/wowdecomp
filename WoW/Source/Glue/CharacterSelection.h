#ifndef WOW_SOURCE_GLUE_CHARACTERSELECTION_H
#define WOW_SOURCE_GLUE_CHARACTERSELECTION_H

#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Glue/CharacterSelectionDisplay.h"

class CHARACTER_INFO;
class CM2Lighting;
class CM2Model;
class CSimpleModelFFX;

class CCharacterSelection {
    public:

    enum RACE_RESTRICTION_INDEX : uint32_t {
        RACE_RESTRICTION_HUMAN = 0,
        RACE_RESTRICTION_DWARF = 1,
        RACE_RESTRICTION_GNOME = 2,
        RACE_RESTRICTION_NIGHT_ELF = 3,
        RACE_RESTRICTION_DRAENEI = 4,
        RACE_RESTRICTION_ORC = 5,
        RACE_RESTRICTION_TROLL = 6,
        RACE_RESTRICTION_TAUREN = 7,
        RACE_RESTRICTION_UNDEAD = 8,
        RACE_RESTRICTION_BLOOD_ELF = 9,
        NUM_RACE_RESTRICTIONS = 10,
    };

    static CSimpleModelFFX* m_modelFrame;
    static TSGrowableArray<CharacterSelectionDisplay> s_characterList;
    static int32_t s_selectionIndex;
    static int32_t s_characterCount;
    static float s_charFacing;
    static uint32_t s_raceRestrictions[NUM_RACE_RESTRICTIONS];

    static void ClearCharacterList();
    static void EnumerateCharactersCallback(CHARACTER_INFO& info, void* param);
    static void GenericLightingCallback(CM2Model* model, CM2Lighting* lighting, void* arg);
    static CharacterSelectionDisplay* GetCharacterDisplay(uint32_t index);
    static void GuildCallback(int32_t guildID, const WOWGUID& guid, void* param, bool success);
    static int32_t Idle(const void* data, void* param);
    static void Initialize();
    static void ModelLoadedCallback(CM2Model* model, void* arg);
    static void OnHide();
    static void SetBackgroundModel(const char* modelName);
    static void ShowCharacter();
    static void Shutdown();
    static void UpdateCharacterList();
    static void UpdateCharacterName(WOWGUID guid, const char* name);
    static void UpdateCharacterInfo(WOWGUID guid, const char* name, uint8_t sexID, uint8_t skinColorID, uint8_t faceID, uint8_t hairStyleID, uint8_t hairColorID, uint8_t facialHairStyleID, uint8_t raceID);
    static void UpdateCharacterNameDeclension(WOWGUID guid);
};

extern const int32_t g_scriptSexIndex[3];

void CharSelectRegisterScriptFunctions();
void CharSelectUnregisterScriptFunctions();

#endif
