#ifndef WOW_SOURCE_DB_REC_ACHIEVEMENT_CRITERIAREC_H
#define WOW_SOURCE_DB_REC_ACHIEVEMENT_CRITERIAREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum ACHIEVEMENT_CRITERIA_FLAGS : int32_t {
    ACHIEVEMENT_CRITERIA_FLAG_NONE         = 0x00000000,
    ACHIEVEMENT_CRITERIA_FLAG_PROGRESS_BAR = 0x00000001,
    ACHIEVEMENT_CRITERIA_FLAG_HIDDEN       = 0x00000002,
    ACHIEVEMENT_CRITERIA_FLAG_IS_DATE      = 0x00000010,
    ACHIEVEMENT_CRITERIA_FLAG_IS_MONEY     = 0x00000020,
};

class Achievement_CriteriaRec {
    public:
    static const int32_t s_numColumns = 31;
    static const int32_t s_rowSize = 124;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_achievementID;
    /* 0008 */ int32_t m_type;
    /* 000C */ int32_t m_assetID;
    /* 0010 */ int32_t m_quantity;
    /* 0014 */ int32_t m_startEvent;
    /* 0018 */ int32_t m_startAsset;
    /* 001C */ int32_t m_failEvent;
    /* 0020 */ int32_t m_failAsset;
    /* 0024 */ const char* m_description;
    /* 0028 */ int32_t m_flags;
    /* 002C */ int32_t m_timerStartEvent;
    /* 0030 */ int32_t m_timerAssetID;
    /* 0034 */ int32_t m_timerTime;
    /* 0038 */ int32_t m_uiOrder;

    Achievement_CriteriaRec();
    ~Achievement_CriteriaRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
