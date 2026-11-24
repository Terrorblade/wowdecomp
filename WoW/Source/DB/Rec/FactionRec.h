#ifndef WOW_SOURCE_DB_REC_FACTIONREC_H
#define WOW_SOURCE_DB_REC_FACTIONREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_FACTION_REPUTATION_ENTRIES = 4;

class FactionRec {
    public:
    static const int32_t s_numColumns = 57;
    static const int32_t s_rowSize = 228;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_reputationIndex;
    /* 0008 */ int32_t m_reputationRaceMask [NUM_FACTION_REPUTATION_ENTRIES];
    /* 0018 */ int32_t m_reputationClassMask [NUM_FACTION_REPUTATION_ENTRIES];
    /* 0028 */ int32_t m_reputationBase [NUM_FACTION_REPUTATION_ENTRIES];
    /* 0038 */ int32_t m_reputationFlags [NUM_FACTION_REPUTATION_ENTRIES];
    /* 0048 */ int32_t m_parentFactionID;
    /* 004C */ float m_parentFactionMod [2];
    /* 0054 */ int32_t m_parentFactionCap [2];
    /* 005C */ const char* m_name;
    /* 0060 */ const char* m_description;

    FactionRec();
    ~FactionRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
