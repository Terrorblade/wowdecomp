#ifndef WOW_SOURCE_UI_TUTORIAL_H
#define WOW_SOURCE_UI_TUTORIAL_H

#include <cstdint>
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Net/Types.h"

class CDataStore;
struct lua_State;

enum TUTORIAL : int32_t {
    TUTORIAL_QUESTGIVERS = 0,
    TUTORIAL_MOVEMENT = 1,
    TUTORIAL_CAMERA = 2,
    TUTORIAL_TARGETING = 3,
    TUTORIAL_TARGETING_ENEMY = 4,
    TUTORIAL_SPELLS_AND_ABILITIES = 5,
    TUTORIAL_LOOTING = 6,
    TUTORIAL_ITEMS = 7,
    TUTORIAL_USABLE_ITEMS = 8,
    TUTORIAL_BAGS = 9,
    TUTORIAL_FOOD = 10,
    TUTORIAL_DRINK = 11,
    TUTORIAL_TALENTS = 12,
    TUTORIAL_SKILLS = 13,
    TUTORIAL_ABILITIES = 14,
    TUTORIAL_REPUTATION = 15,
    TUTORIAL_TELLS = 16,
    TUTORIAL_GROUPING = 17,
    TUTORIAL_PLAYERS = 18,
    TUTORIAL_VENDORS = 19,
    TUTORIAL_QUEST_LOG = 20,
    TUTORIAL_FRIENDS = 21,
    TUTORIAL_CHATTING = 22,
    TUTORIAL_EQUIPPABLE_ITEMS = 23,
    TUTORIAL_DEATH = 24,
    TUTORIAL_RESTED = 25,
    TUTORIAL_FATIGUE = 26,
    TUTORIAL_SWIMMING = 27,
    TUTORIAL_BREATH = 28,
    TUTORIAL_RESTING = 29,
    TUTORIAL_HEARTHSTONES = 30,
    TUTORIAL_PVP = 31,
    TUTORIAL_JUMPING = 32,
    TUTORIAL_QUEST_COMPLETION = 33,
    TUTORIAL_TRAVEL = 34,
    TUTORIAL_DAMAGED_ITEMS = 35,
    TUTORIAL_BROKEN_ITEMS = 36,
    TUTORIAL_PROFESSIONS = 37,
    TUTORIAL_GROUPS = 38,
    TUTORIAL_SPELLBOOK = 39,
    TUTORIAL_ELITE_QUESTS = 40,
    TUTORIAL_WELCOME = 41,
    TUTORIAL_UNAVAILABLE_QUEST_GIVERS = 42,
    TUTORIAL_RANGED_WEAPONS = 43,
    TUTORIAL_AMMUNITION = 44,
    TUTORIAL_RAID_GROUPS = 45,
    TUTORIAL_TOTEM_BAR = 46,
    TUTORIAL_BATTLEGROUND_QUEUE = 47,
    TUTORIAL_BATTLEGROUND_PORT = 48,
    TUTORIAL_KEYRING = 49,
    TUTORIAL_DUNGEON_FINDER = 50,
    TUTORIAL_COMPANIONS = 51,
    TUTORIAL_MOUNTS = 52,
    TUTORIAL_THREAT_WARNINGS = 53,
    TUTORIAL_LEVEL_UP = 54,
    TUTORIAL_PLAYER_STATUS = 55,
    TUTORIAL_ENEMY_STATUS = 56,
    TUTORIAL_FULL_BAGS = 57,
    TUTORIAL_FREE_BAG_SPACE = 58,
    TUTORIAL_SPECIAL_ABILITIES = 59,
    NUM_TUTORIALS = 60,
};

class CGTutorial {
    public:
    static void SaveFlaggedTutorials();
    static void InitializeGame();
    static void ShutdownGame();
    static void LoadFlaggedTutorials();
    static TSBitField<TSFixedArray<uint32_t> > m_tutorialShown;
    static TSBitField<TSFixedArray<uint32_t> > m_tutorialFlags;

    static void FlagTutorial(uint32_t tutorial);
    static void TriggerTutorial(uint32_t tutorial);
    static int32_t GetNextCompletedTutorial(int32_t tutorial);
    static int32_t GetPrevCompletedTutorial(int32_t tutorial);
    static void ClearTutorials();
    static void ResetTutorials();
    static int32_t OnTutorialFlags(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
};

void TutorialRegisterScriptFunctions();
void TutorialUnregisterScriptFunctions();

#endif
