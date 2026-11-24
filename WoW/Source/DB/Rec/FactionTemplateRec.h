#ifndef WOW_SOURCE_DB_REC_FACTIONTEMPLATEREC_H
#define WOW_SOURCE_DB_REC_FACTIONTEMPLATEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_FACTION_TEMPLATE_RELATIONS = 4;

enum FACTION_TEMPLATE_FLAGS : int32_t {
    FACTION_TEMPLATE_FLAG_NONE                       = 0x00000000,
    FACTION_TEMPLATE_FLAG_ATTACK_PVP_ACTIVE_PLAYERS = 0x00001000,
};

enum FACTION_GROUP_MASK : int32_t {
    FACTION_GROUP_MASK_PLAYER   = 0x1,
    FACTION_GROUP_MASK_ALLIANCE = 0x2,
    FACTION_GROUP_MASK_HORDE    = 0x4,
    FACTION_GROUP_MASK_MONSTER  = 0x8,
};

class FactionTemplateRec {
    public:
    static const int32_t s_numColumns = 14;
    static const int32_t s_rowSize = 56;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_faction;
    /* 0008 */ int32_t m_flags;
    /* 000C */ int32_t m_factionGroup;
    /* 0010 */ int32_t m_friendGroup;
    /* 0014 */ int32_t m_enemyGroup;
    /* 0018 */ int32_t m_enemies [NUM_FACTION_TEMPLATE_RELATIONS];
    /* 0028 */ int32_t m_friend [NUM_FACTION_TEMPLATE_RELATIONS];

    FactionTemplateRec();
    ~FactionTemplateRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
