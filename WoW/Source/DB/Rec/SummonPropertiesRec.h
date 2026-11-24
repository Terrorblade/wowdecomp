#ifndef WOW_SOURCE_DB_REC_SUMMONPROPERTIESREC_H
#define WOW_SOURCE_DB_REC_SUMMONPROPERTIESREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum SUMMON_SLOT : int32_t {
    SUMMON_SLOT_ANY_AVAILABLE_TOTEM = -1,
    SUMMON_SLOT_NONE = 0,
    SUMMON_SLOT_TOTEM_1 = 1,
    SUMMON_SLOT_TOTEM_2 = 2,
    SUMMON_SLOT_TOTEM_3 = 3,
    SUMMON_SLOT_TOTEM_4 = 4,
    SUMMON_SLOT_CRITTER = 5,
    SUMMON_SLOT_QUEST_PLAYERS_ONLY = 6,
};

enum SUMMON_CONTROL : int32_t {
    SUMMON_CONTROL_NONE = 0,
    SUMMON_CONTROL_GUARDIAN = 1,
    SUMMON_CONTROL_PET = 2,
    SUMMON_CONTROL_POSSESSED = 3,
    SUMMON_CONTROL_POSSESSED_VEHICLE = 4,
    SUMMON_CONTROL_WILD_VEHICLE_WITH_RIDE_SPELL = 5,
};

enum SUMMON_TYPE : int32_t {
    SUMMON_TYPE_DEFAULT_TITLE = -1,
    SUMMON_TYPE_NONE = 0,
    SUMMON_TYPE_PET = 1,
    SUMMON_TYPE_GUARDIAN = 2,
    SUMMON_TYPE_MINION = 3,
    SUMMON_TYPE_TOTEM = 4,
    SUMMON_TYPE_MINIPET = 5,
    SUMMON_TYPE_GUARDIAN_2 = 6,
    SUMMON_TYPE_WILD_2 = 7,
    SUMMON_TYPE_WILD_3 = 8,
    SUMMON_TYPE_VEHICLE = 9,
    SUMMON_TYPE_VEHICLE_2 = 10,
    SUMMON_TYPE_LIGHTWELL = 11,
    SUMMON_TYPE_JEEVES = 12,
};

enum SUMMON_PROPERTIES_FLAGS : int32_t {
    SUMMON_PROPERTIES_FLAG_ONLY_VISIBLE_TO_SUMMONER = 0x10,
    SUMMON_PROPERTIES_FLAG_CANNOT_DISMISS_PET = 0x20,
};

class SummonPropertiesRec {
    public:
    static const int32_t s_numColumns = 6;
    static const int32_t s_rowSize = 24;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_control;
    /* 0008 */ int32_t m_faction;
    /* 000C */ int32_t m_title;
    /* 0010 */ int32_t m_slot;
    /* 0014 */ int32_t m_flags;

    SummonPropertiesRec();
    ~SummonPropertiesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
