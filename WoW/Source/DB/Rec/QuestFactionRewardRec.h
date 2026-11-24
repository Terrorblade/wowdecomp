#ifndef WOW_SOURCE_DB_REC_QUESTFACTIONREWARDREC_H
#define WOW_SOURCE_DB_REC_QUESTFACTIONREWARDREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_QUEST_FACTION_REWARD_DIFFICULTIES = 10;

class QuestFactionRewardRec {
    public:
    static const int32_t s_numColumns = 11;
    static const int32_t s_rowSize = 44;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_difficulty [NUM_QUEST_FACTION_REWARD_DIFFICULTIES];

    QuestFactionRewardRec();
    ~QuestFactionRewardRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
