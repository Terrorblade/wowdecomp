#ifndef WOW_SOURCE_OBJECT_PLAYER_H
#define WOW_SOURCE_OBJECT_PLAYER_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/ObjectClient/VisibleItem_C.h"
#include "WoW/Source/Object/WGUID.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Magic/MagicClient/SpellEnums.h"
#include "WoW/Source/Object/Types.h"
#include "WoW/Source/Object/QuestCache.h"
#include "WoW/Source/Object/Object.h"
#include "WoW/Source/Object/Unit.h"
constexpr uint32_t MAX_HONOR_CURRENCY = 75000;
constexpr uint32_t MAX_ARENA_CURRENCY = 10000;
constexpr int32_t MAX_DAILY_QUESTS = 25;
constexpr int32_t NUM_PLAYER_SKILLS = 128;

enum COMBAT_RATING : int32_t {
    CR_WEAPON_SKILL = 0,
    CR_DEFENSE_SKILL = 1,
    CR_DODGE = 2,
    CR_PARRY = 3,
    CR_BLOCK = 4,
    CR_HIT_MELEE = 5,
    CR_HIT_RANGED = 6,
    CR_HIT_SPELL = 7,
    CR_CRIT_MELEE = 8,
    CR_CRIT_RANGED = 9,
    CR_CRIT_SPELL = 10,
    CR_HIT_TAKEN_MELEE = 11,
    CR_HIT_TAKEN_RANGED = 12,
    CR_HIT_TAKEN_SPELL = 13,
    CR_CRIT_TAKEN_MELEE = 14,
    CR_CRIT_TAKEN_RANGED = 15,
    CR_CRIT_TAKEN_SPELL = 16,
    CR_HASTE_MELEE = 17,
    CR_HASTE_RANGED = 18,
    CR_HASTE_SPELL = 19,
    CR_WEAPON_SKILL_MAINHAND = 20,
    CR_WEAPON_SKILL_OFFHAND = 21,
    CR_WEAPON_SKILL_RANGED = 22,
    CR_EXPERTISE = 23,
    CR_ARMOR_PENETRATION = 24,
    NUM_COMBAT_RATINGS = 25,
};
constexpr uint32_t MAX_ARENA_TEAMS = 3;

enum ARENA_TEAM_INFO : uint32_t {
    ARENA_TEAM_INFO_ID = 0,
    ARENA_TEAM_INFO_TYPE = 1,
    ARENA_TEAM_INFO_MEMBER = 2,
    ARENA_TEAM_INFO_GAMES_WEEK = 3,
    ARENA_TEAM_INFO_GAMES_SEASON = 4,
    ARENA_TEAM_INFO_WINS_SEASON = 5,
    ARENA_TEAM_INFO_PERSONAL_RATING = 6,
    NUM_ARENA_TEAM_INFO_FIELDS = 7,
};

class CGPlayerLocalData;

enum QUEST_LOG_ENTRY_STATE : uint32_t {
    QUEST_LOG_ENTRY_COMPLETE = 0x1,
    QUEST_LOG_ENTRY_FAILED = 0x2,
};
constexpr uint32_t QUESTLOG_FLAGS_NUM_COUNTS = 4;

struct PlayerQuestLogEntry {
    /* 0000 */ uint32_t m_questId;
    /* 0004 */ uint32_t m_state;
    /* 0008 */ uint16_t m_counts [QUESTLOG_FLAGS_NUM_COUNTS];
    /* 0010 */ uint32_t m_time;
};

struct PlayerSkillInfo {
    /* 0000 */ uint16_t m_skillId;
    /* 0002 */ uint16_t m_step;
    /* 0004 */ uint16_t m_rank;
    /* 0006 */ uint16_t m_maxRank;
    /* 0008 */ uint16_t m_tempBonus;
    /* 000A */ uint16_t m_permBonus;
};

class CGPlayer {
    public:
    PlayerQuestLogEntry* GetQuestLogEntry(uint32_t slot);
    uint32_t GetBuybackPrice(uint32_t slot);
    bool HasPvpMedal(uint32_t medal);
    bool HasAuraVision(uint8_t vision);
    static const uint32_t s_mirrorFields[173];
    static const uint32_t s_localMirrorFields[870];

    static uint32_t DescriptorToMirrorOffset(uint32_t offset, uint32_t size, int32_t local, uint32_t base);
    static uint32_t GetSavedIndexFromDataIndex(uint32_t index, uint32_t count, int32_t local);
    static int32_t TotalFields();
    static int32_t TotalRemoteFields();

    /* 0000 */ CGPlayerLocalData* m_player;
    /* 0004 */ void* m_playerMirror;

    float GetDrunknessTurnValue();
    int32_t HasZoneExplored(int32_t zone);
    bool IsDrunk();
};

class CGPlayerData {
    public:
    /* 0000 */ WOWGUID m_playerDuelArbiter;
    /* 0008 */ uint32_t m_playerFlags;
    /* 000C */ uint32_t m_playerGuildid;
    /* 0010 */ uint32_t m_playerGuildrank;
    /* 0014 */ uint8_t m_playerBytes [4];
    /* 0018 */ uint8_t m_playerBytes2 [4];
    /* 001C */ uint8_t m_playerBytes3 [4];
    /* 0020 */ uint32_t m_playerDuelTeam;
    /* 0024 */ uint32_t m_playerGuildTimestamp;
    /* 0028 */ PlayerQuestLogEntry m_playerQuestLog [25];
    /* 021C */ VisibleItem_C m_playerVisibleItems [19];
    /* 02B4 */ uint32_t m_playerChosenTitle;
    /* 02B8 */ int32_t m_playerFakeInebriation;
    /* 02BC */ uint32_t m_playerFieldPad0;
};

class CGPlayerLocalData : public CGPlayerData {
    public:
    enum LocalFlags : uint8_t {
        LOCAL_FLAG_NONE = 0,
        LOCAL_FLAG_ACTIVE_PET = 0x1,
        LOCAL_FLAG_TRACK_STEALTHED = 0x2,
        LOCAL_FLAG_UNKNOWN_2 = 0x4,
        LOCAL_FLAG_REPOP_DELAY = 0x8,
        LOCAL_FLAG_REPOP_IMMEDIATE = 0x10,
    };

    enum FIELD_ID : uint32_t {
        PLAYER_DUEL_ARBITER = 0,
        PLAYER_FLAGS = 2,
        PLAYER_GUILDID = 3,
        PLAYER_GUILDRANK = 4,
        PLAYER_BYTES = 5,
        PLAYER_BYTES_2 = 6,
        PLAYER_BYTES_3 = 7,
        PLAYER_DUEL_TEAM = 8,
        PLAYER_GUILD_TIMESTAMP = 9,
        PLAYER_QUEST_LOG_1_1 = 10,
        PLAYER_QUEST_LOG_1_2 = 11,
        PLAYER_QUEST_LOG_1_3 = 12,
        PLAYER_QUEST_LOG_1_4 = 14,
        PLAYER_QUEST_LOG_2_1 = 15,
        PLAYER_QUEST_LOG_2_2 = 16,
        PLAYER_QUEST_LOG_2_3 = 17,
        PLAYER_QUEST_LOG_2_5 = 19,
        PLAYER_QUEST_LOG_3_1 = 20,
        PLAYER_QUEST_LOG_3_2 = 21,
        PLAYER_QUEST_LOG_3_3 = 22,
        PLAYER_QUEST_LOG_3_5 = 24,
        PLAYER_QUEST_LOG_4_1 = 25,
        PLAYER_QUEST_LOG_4_2 = 26,
        PLAYER_QUEST_LOG_4_3 = 27,
        PLAYER_QUEST_LOG_4_5 = 29,
        PLAYER_QUEST_LOG_5_1 = 30,
        PLAYER_QUEST_LOG_5_2 = 31,
        PLAYER_QUEST_LOG_5_3 = 32,
        PLAYER_QUEST_LOG_5_5 = 34,
        PLAYER_QUEST_LOG_6_1 = 35,
        PLAYER_QUEST_LOG_6_2 = 36,
        PLAYER_QUEST_LOG_6_3 = 37,
        PLAYER_QUEST_LOG_6_5 = 39,
        PLAYER_QUEST_LOG_7_1 = 40,
        PLAYER_QUEST_LOG_7_2 = 41,
        PLAYER_QUEST_LOG_7_3 = 42,
        PLAYER_QUEST_LOG_7_5 = 44,
        PLAYER_QUEST_LOG_8_1 = 45,
        PLAYER_QUEST_LOG_8_2 = 46,
        PLAYER_QUEST_LOG_8_3 = 47,
        PLAYER_QUEST_LOG_8_5 = 49,
        PLAYER_QUEST_LOG_9_1 = 50,
        PLAYER_QUEST_LOG_9_2 = 51,
        PLAYER_QUEST_LOG_9_3 = 52,
        PLAYER_QUEST_LOG_9_5 = 54,
        PLAYER_QUEST_LOG_10_1 = 55,
        PLAYER_QUEST_LOG_10_2 = 56,
        PLAYER_QUEST_LOG_10_3 = 57,
        PLAYER_QUEST_LOG_10_5 = 59,
        PLAYER_QUEST_LOG_11_1 = 60,
        PLAYER_QUEST_LOG_11_2 = 61,
        PLAYER_QUEST_LOG_11_3 = 62,
        PLAYER_QUEST_LOG_11_5 = 64,
        PLAYER_QUEST_LOG_12_1 = 65,
        PLAYER_QUEST_LOG_12_2 = 66,
        PLAYER_QUEST_LOG_12_3 = 67,
        PLAYER_QUEST_LOG_12_5 = 69,
        PLAYER_QUEST_LOG_13_1 = 70,
        PLAYER_QUEST_LOG_13_2 = 71,
        PLAYER_QUEST_LOG_13_3 = 72,
        PLAYER_QUEST_LOG_13_5 = 74,
        PLAYER_QUEST_LOG_14_1 = 75,
        PLAYER_QUEST_LOG_14_2 = 76,
        PLAYER_QUEST_LOG_14_3 = 77,
        PLAYER_QUEST_LOG_14_5 = 79,
        PLAYER_QUEST_LOG_15_1 = 80,
        PLAYER_QUEST_LOG_15_2 = 81,
        PLAYER_QUEST_LOG_15_3 = 82,
        PLAYER_QUEST_LOG_15_5 = 84,
        PLAYER_QUEST_LOG_16_1 = 85,
        PLAYER_QUEST_LOG_16_2 = 86,
        PLAYER_QUEST_LOG_16_3 = 87,
        PLAYER_QUEST_LOG_16_5 = 89,
        PLAYER_QUEST_LOG_17_1 = 90,
        PLAYER_QUEST_LOG_17_2 = 91,
        PLAYER_QUEST_LOG_17_3 = 92,
        PLAYER_QUEST_LOG_17_5 = 94,
        PLAYER_QUEST_LOG_18_1 = 95,
        PLAYER_QUEST_LOG_18_2 = 96,
        PLAYER_QUEST_LOG_18_3 = 97,
        PLAYER_QUEST_LOG_18_5 = 99,
        PLAYER_QUEST_LOG_19_1 = 100,
        PLAYER_QUEST_LOG_19_2 = 101,
        PLAYER_QUEST_LOG_19_3 = 102,
        PLAYER_QUEST_LOG_19_5 = 104,
        PLAYER_QUEST_LOG_20_1 = 105,
        PLAYER_QUEST_LOG_20_2 = 106,
        PLAYER_QUEST_LOG_20_3 = 107,
        PLAYER_QUEST_LOG_20_5 = 109,
        PLAYER_QUEST_LOG_21_1 = 110,
        PLAYER_QUEST_LOG_21_2 = 111,
        PLAYER_QUEST_LOG_21_3 = 112,
        PLAYER_QUEST_LOG_21_5 = 114,
        PLAYER_QUEST_LOG_22_1 = 115,
        PLAYER_QUEST_LOG_22_2 = 116,
        PLAYER_QUEST_LOG_22_3 = 117,
        PLAYER_QUEST_LOG_22_5 = 119,
        PLAYER_QUEST_LOG_23_1 = 120,
        PLAYER_QUEST_LOG_23_2 = 121,
        PLAYER_QUEST_LOG_23_3 = 122,
        PLAYER_QUEST_LOG_23_5 = 124,
        PLAYER_QUEST_LOG_24_1 = 125,
        PLAYER_QUEST_LOG_24_2 = 126,
        PLAYER_QUEST_LOG_24_3 = 127,
        PLAYER_QUEST_LOG_24_5 = 129,
        PLAYER_QUEST_LOG_25_1 = 130,
        PLAYER_QUEST_LOG_25_2 = 131,
        PLAYER_QUEST_LOG_25_3 = 132,
        PLAYER_QUEST_LOG_25_5 = 134,
        PLAYER_VISIBLE_ITEM_1_ENTRYID = 135,
        PLAYER_VISIBLE_ITEM_1_ENCHANTMENT = 136,
        PLAYER_VISIBLE_ITEM_2_ENTRYID = 137,
        PLAYER_VISIBLE_ITEM_2_ENCHANTMENT = 138,
        PLAYER_VISIBLE_ITEM_3_ENTRYID = 139,
        PLAYER_VISIBLE_ITEM_3_ENCHANTMENT = 140,
        PLAYER_VISIBLE_ITEM_4_ENTRYID = 141,
        PLAYER_VISIBLE_ITEM_4_ENCHANTMENT = 142,
        PLAYER_VISIBLE_ITEM_5_ENTRYID = 143,
        PLAYER_VISIBLE_ITEM_5_ENCHANTMENT = 144,
        PLAYER_VISIBLE_ITEM_6_ENTRYID = 145,
        PLAYER_VISIBLE_ITEM_6_ENCHANTMENT = 146,
        PLAYER_VISIBLE_ITEM_7_ENTRYID = 147,
        PLAYER_VISIBLE_ITEM_7_ENCHANTMENT = 148,
        PLAYER_VISIBLE_ITEM_8_ENTRYID = 149,
        PLAYER_VISIBLE_ITEM_8_ENCHANTMENT = 150,
        PLAYER_VISIBLE_ITEM_9_ENTRYID = 151,
        PLAYER_VISIBLE_ITEM_9_ENCHANTMENT = 152,
        PLAYER_VISIBLE_ITEM_10_ENTRYID = 153,
        PLAYER_VISIBLE_ITEM_10_ENCHANTMENT = 154,
        PLAYER_VISIBLE_ITEM_11_ENTRYID = 155,
        PLAYER_VISIBLE_ITEM_11_ENCHANTMENT = 156,
        PLAYER_VISIBLE_ITEM_12_ENTRYID = 157,
        PLAYER_VISIBLE_ITEM_12_ENCHANTMENT = 158,
        PLAYER_VISIBLE_ITEM_13_ENTRYID = 159,
        PLAYER_VISIBLE_ITEM_13_ENCHANTMENT = 160,
        PLAYER_VISIBLE_ITEM_14_ENTRYID = 161,
        PLAYER_VISIBLE_ITEM_14_ENCHANTMENT = 162,
        PLAYER_VISIBLE_ITEM_15_ENTRYID = 163,
        PLAYER_VISIBLE_ITEM_15_ENCHANTMENT = 164,
        PLAYER_VISIBLE_ITEM_16_ENTRYID = 165,
        PLAYER_VISIBLE_ITEM_16_ENCHANTMENT = 166,
        PLAYER_VISIBLE_ITEM_17_ENTRYID = 167,
        PLAYER_VISIBLE_ITEM_17_ENCHANTMENT = 168,
        PLAYER_VISIBLE_ITEM_18_ENTRYID = 169,
        PLAYER_VISIBLE_ITEM_18_ENCHANTMENT = 170,
        PLAYER_VISIBLE_ITEM_19_ENTRYID = 171,
        PLAYER_VISIBLE_ITEM_19_ENCHANTMENT = 172,
        PLAYER_CHOSEN_TITLE = 173,
        PLAYER_FAKE_INEBRIATION = 174,
        PLAYER_FIELD_PAD_0 = 175,
        PLAYER_FIELD_INV_SLOT_HEAD = 176,
        PLAYER_FIELD_PACK_SLOT_1 = 222,
        PLAYER_FIELD_BANK_SLOT_1 = 254,
        PLAYER_FIELD_BANKBAG_SLOT_1 = 310,
        PLAYER_FIELD_VENDORBUYBACK_SLOT_1 = 324,
        PLAYER_FIELD_KEYRING_SLOT_1 = 348,
        PLAYER_FIELD_CURRENCYTOKEN_SLOT_1 = 412,
        PLAYER_FARSIGHT = 476,
        PLAYER__FIELD_KNOWN_TITLES = 478,
        PLAYER__FIELD_KNOWN_TITLES1 = 480,
        PLAYER__FIELD_KNOWN_TITLES2 = 482,
        PLAYER_FIELD_KNOWN_CURRENCIES = 484,
        PLAYER_XP = 486,
        PLAYER_NEXT_LEVEL_XP = 487,
        PLAYER_SKILL_INFO_1_1 = 488,
        PLAYER_CHARACTER_POINTS1 = 872,
        PLAYER_CHARACTER_POINTS2 = 873,
        PLAYER_TRACK_CREATURES = 874,
        PLAYER_TRACK_RESOURCES = 875,
        PLAYER_BLOCK_PERCENTAGE = 876,
        PLAYER_DODGE_PERCENTAGE = 877,
        PLAYER_PARRY_PERCENTAGE = 878,
        PLAYER_EXPERTISE = 879,
        PLAYER_OFFHAND_EXPERTISE = 880,
        PLAYER_CRIT_PERCENTAGE = 881,
        PLAYER_RANGED_CRIT_PERCENTAGE = 882,
        PLAYER_OFFHAND_CRIT_PERCENTAGE = 883,
        PLAYER_SPELL_CRIT_PERCENTAGE1 = 884,
        PLAYER_SHIELD_BLOCK = 891,
        PLAYER_SHIELD_BLOCK_CRIT_PERCENTAGE = 892,
        PLAYER_EXPLORED_ZONES_1 = 893,
        PLAYER_REST_STATE_EXPERIENCE = 1021,
        PLAYER_FIELD_COINAGE = 1022,
        PLAYER_FIELD_MOD_DAMAGE_DONE_POS = 1023,
        PLAYER_FIELD_MOD_DAMAGE_DONE_NEG = 1030,
        PLAYER_FIELD_MOD_DAMAGE_DONE_PCT = 1037,
        PLAYER_FIELD_MOD_HEALING_DONE_POS = 1044,
        PLAYER_FIELD_MOD_HEALING_PCT = 1045,
        PLAYER_FIELD_MOD_HEALING_DONE_PCT = 1046,
        PLAYER_FIELD_MOD_TARGET_RESISTANCE = 1047,
        PLAYER_FIELD_MOD_TARGET_PHYSICAL_RESISTANCE = 1048,
        PLAYER_FIELD_BYTES = 1049,
        PLAYER_AMMO_ID = 1050,
        PLAYER_SELF_RES_SPELL = 1051,
        PLAYER_FIELD_PVP_MEDALS = 1052,
        PLAYER_FIELD_BUYBACK_PRICE_1 = 1053,
        PLAYER_FIELD_BUYBACK_TIMESTAMP_1 = 1065,
        PLAYER_FIELD_KILLS = 1077,
        PLAYER_FIELD_TODAY_CONTRIBUTION = 1078,
        PLAYER_FIELD_YESTERDAY_CONTRIBUTION = 1079,
        PLAYER_FIELD_LIFETIME_HONORBALE_KILLS = 1080,
        PLAYER_FIELD_BYTES2 = 1081,
        PLAYER_FIELD_WATCHED_FACTION_INDEX = 1082,
        PLAYER_FIELD_COMBAT_RATING_1 = 1083,
        PLAYER_FIELD_ARENA_TEAM_INFO_1_1 = 1108,
        PLAYER_FIELD_HONOR_CURRENCY = 1129,
        PLAYER_FIELD_ARENA_CURRENCY = 1130,
        PLAYER_FIELD_MAX_LEVEL = 1131,
        PLAYER_FIELD_DAILY_QUESTS_1 = 1132,
        PLAYER_RUNE_REGEN_1 = 1157,
        PLAYER_NO_REAGENT_COST_1 = 1161,
        PLAYER_FIELD_GLYPH_SLOTS_1 = 1164,
        PLAYER_FIELD_GLYPHS_1 = 1170,
        PLAYER_GLYPHS_ENABLED = 1176,
        PLAYER_PET_SPELL_POWER = 1177,
        PLAYER_END = 1178,
    };

    enum FIELD_SIZE : uint32_t {
        PLAYER_DUEL_ARBITER_SIZE = 2,
        PLAYER_QUEST_LOG_1_3_SIZE = 2,
        PLAYER_QUEST_LOG_2_3_SIZE = 2,
        PLAYER_QUEST_LOG_3_3_SIZE = 2,
        PLAYER_QUEST_LOG_4_3_SIZE = 2,
        PLAYER_QUEST_LOG_5_3_SIZE = 2,
        PLAYER_QUEST_LOG_6_3_SIZE = 2,
        PLAYER_QUEST_LOG_7_3_SIZE = 2,
        PLAYER_QUEST_LOG_8_3_SIZE = 2,
        PLAYER_QUEST_LOG_9_3_SIZE = 2,
        PLAYER_QUEST_LOG_10_3_SIZE = 2,
        PLAYER_QUEST_LOG_11_3_SIZE = 2,
        PLAYER_QUEST_LOG_12_3_SIZE = 2,
        PLAYER_QUEST_LOG_13_3_SIZE = 2,
        PLAYER_QUEST_LOG_14_3_SIZE = 2,
        PLAYER_QUEST_LOG_15_3_SIZE = 2,
        PLAYER_QUEST_LOG_16_3_SIZE = 2,
        PLAYER_QUEST_LOG_17_3_SIZE = 2,
        PLAYER_QUEST_LOG_18_3_SIZE = 2,
        PLAYER_QUEST_LOG_19_3_SIZE = 2,
        PLAYER_QUEST_LOG_20_3_SIZE = 2,
        PLAYER_QUEST_LOG_21_3_SIZE = 2,
        PLAYER_QUEST_LOG_22_3_SIZE = 2,
        PLAYER_QUEST_LOG_23_3_SIZE = 2,
        PLAYER_QUEST_LOG_24_3_SIZE = 2,
        PLAYER_QUEST_LOG_25_3_SIZE = 2,
        PLAYER_FIELD_INV_SLOT_HEAD_SIZE = 46,
        PLAYER_FIELD_PACK_SLOT_1_SIZE = 32,
        PLAYER_FIELD_BANK_SLOT_1_SIZE = 56,
        PLAYER_FIELD_BANKBAG_SLOT_1_SIZE = 14,
        PLAYER_FIELD_VENDORBUYBACK_SLOT_1_SIZE = 24,
        PLAYER_FIELD_KEYRING_SLOT_1_SIZE = 64,
        PLAYER_FIELD_CURRENCYTOKEN_SLOT_1_SIZE = 64,
        PLAYER_FARSIGHT_SIZE = 2,
        PLAYER__FIELD_KNOWN_TITLES_SIZE = 2,
        PLAYER__FIELD_KNOWN_TITLES1_SIZE = 2,
        PLAYER__FIELD_KNOWN_TITLES2_SIZE = 2,
        PLAYER_FIELD_KNOWN_CURRENCIES_SIZE = 2,
        PLAYER_SKILL_INFO_1_1_SIZE = 384,
        PLAYER_SPELL_CRIT_PERCENTAGE1_SIZE = 7,
        PLAYER_EXPLORED_ZONES_1_SIZE = 128,
        PLAYER_FIELD_MOD_DAMAGE_DONE_POS_SIZE = 7,
        PLAYER_FIELD_MOD_DAMAGE_DONE_NEG_SIZE = 7,
        PLAYER_FIELD_MOD_DAMAGE_DONE_PCT_SIZE = 7,
        PLAYER_FIELD_BUYBACK_PRICE_1_SIZE = 12,
        PLAYER_FIELD_BUYBACK_TIMESTAMP_1_SIZE = 12,
        PLAYER_FIELD_COMBAT_RATING_1_SIZE = 25,
        PLAYER_FIELD_ARENA_TEAM_INFO_1_1_SIZE = 21,
        PLAYER_FIELD_DAILY_QUESTS_1_SIZE = 25,
        PLAYER_RUNE_REGEN_1_SIZE = 4,
        PLAYER_NO_REAGENT_COST_1_SIZE = 3,
        PLAYER_FIELD_GLYPH_SLOTS_1_SIZE = 6,
        PLAYER_FIELD_GLYPHS_1_SIZE = 6,
    };

    /* 02C0 */ WOWGUID m_playerFieldInvSlotHead [23];
    /* 0378 */ WOWGUID m_playerFieldPackSlot1 [16];
    /* 03F8 */ WOWGUID m_playerFieldBankSlot1 [28];
    /* 04D8 */ WOWGUID m_playerFieldBankbagSlot1 [7];
    /* 0510 */ WOWGUID m_playerFieldVendorbuybackSlot1 [NUM_BUYBACK_SLOTS];
    /* 0570 */ WOWGUID m_playerFieldKeyringSlot1 [32];
    /* 0670 */ WOWGUID m_playerFieldCurrencytokenSlot1 [32];
    /* 0770 */ WOWGUID m_playerFarsight;
    /* 0778 */ uint64_t m_playerFieldKnownTitles [3];
    /* 0790 */ WOWGUID m_playerFieldKnownCurrencies;
    /* 0798 */ uint32_t m_playerXp;
    /* 079C */ uint32_t m_playerNextLevelXp;
    /* 07A0 */ PlayerSkillInfo m_playerSkillInfo [NUM_PLAYER_SKILLS];
    /* 0DA0 */ int32_t m_playerCharacterPoints [2];
    /* 0DA8 */ uint32_t m_playerTrackCreatures;
    /* 0DAC */ uint32_t m_playerTrackResources;
    /* 0DB0 */ float m_playerBlockPercentage;
    /* 0DB4 */ float m_playerDodgePercentage;
    /* 0DB8 */ float m_playerParryPercentage;
    /* 0DBC */ uint32_t m_playerExpertise;
    /* 0DC0 */ uint32_t m_playerOffhandExpertise;
    /* 0DC4 */ float m_playerCritPercentage;
    /* 0DC8 */ float m_playerRangedCritPercentage;
    /* 0DCC */ float m_playerOffhandCritPercentage;
    /* 0DD0 */ float m_playerSpellCritPercentage1 [NUM_SPELL_SCHOOLS];
    /* 0DEC */ uint32_t m_playerShieldBlock;
    /* 0DF0 */ float m_playerShieldBlockCritPercentage;
    /* 0DF4 */ uint8_t m_playerExploredZones1 [512];
    /* 0FF4 */ uint32_t m_playerRestStateExperience;
    /* 0FF8 */ uint32_t m_playerFieldCoinage;
    /* 0FFC */ uint32_t m_playerFieldModDamageDonePos [NUM_SPELL_SCHOOLS];
    /* 1018 */ uint32_t m_playerFieldModDamageDoneNeg [NUM_SPELL_SCHOOLS];
    /* 1034 */ float m_playerFieldModDamageDonePct [7];
    /* 1050 */ uint32_t m_playerFieldModHealingDonePos;
    /* 1054 */ float m_playerFieldModHealingPct;
    /* 1058 */ float m_playerFieldModHealingDonePct;
    /* 105C */ uint32_t m_playerFieldModTargetResistance;
    /* 1060 */ uint32_t m_playerFieldModTargetPhysicalResistance;
    /* 1064 */ uint8_t m_playerFieldBytes [4];
    /* 1068 */ uint32_t m_playerAmmoId;
    /* 106C */ uint32_t m_playerSelfResSpell;
    /* 1070 */ uint32_t m_playerFieldPvpMedals;
    /* 1074 */ uint32_t m_playerFieldBuybackPrice1 [NUM_BUYBACK_SLOTS];
    /* 10A4 */ uint32_t m_playerFieldBuybackTimestamp1 [NUM_BUYBACK_SLOTS];
    /* 10D4 */ uint16_t m_playerFieldKills [2];
    /* 10D8 */ uint32_t m_playerFieldTodayContribution;
    /* 10DC */ uint32_t m_playerFieldYesterdayContribution;
    /* 10E0 */ uint32_t m_playerFieldLifetimeHonorbaleKills;
    /* 10E4 */ uint32_t m_playerFieldBytes2;
    /* 10E8 */ uint32_t m_playerFieldWatchedFactionIndex;
    /* 10EC */ uint32_t m_playerFieldCombatRating1 [NUM_COMBAT_RATINGS];
    /* 1150 */ uint32_t m_playerFieldArenaTeamInfo11 [MAX_ARENA_TEAMS * NUM_ARENA_TEAM_INFO_FIELDS];
    /* 11A4 */ uint32_t m_playerFieldHonorCurrency;
    /* 11A8 */ uint32_t m_playerFieldArenaCurrency;
    /* 11AC */ uint32_t m_playerFieldMaxLevel;
    /* 11B0 */ uint32_t m_playerFieldDailyQuests1 [MAX_DAILY_QUESTS];
    static constexpr int32_t NUM_RUNE_TYPES = 4;
    /* 1214 */ float m_playerRuneRegen1 [NUM_RUNE_TYPES];
    /* 1224 */ uint32_t m_playerNoReagentCost1 [NUM_SPELL_CLASSES_MASK];
    /* 1230 */ uint32_t m_playerFieldGlyphSlots1 [6];
    /* 1248 */ uint32_t m_playerFieldGlyphs1 [6];
    /* 1260 */ uint32_t m_playerGlyphsEnabled;
    /* 1264 */ uint32_t m_playerPetSpellPower;
};

#endif
