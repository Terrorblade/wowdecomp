#ifndef WOW_SOURCE_DB_REC_ACHIEVEMENTREC_H
#define WOW_SOURCE_DB_REC_ACHIEVEMENTREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum ACHIEVEMENT_FLAGS : int32_t {
    ACHIEVEMENT_FLAG_NONE                         = 0x00000000,
    ACHIEVEMENT_FLAG_STATISTIC                    = 0x00000001,
    ACHIEVEMENT_FLAG_HIDDEN                       = 0x00000002,
    ACHIEVEMENT_FLAG_NO_COMPLETION_VISUAL         = 0x00000004,
    ACHIEVEMENT_FLAG_SUM                          = 0x00000008,
    ACHIEVEMENT_FLAG_MAXIMUM                      = 0x00000010,
    ACHIEVEMENT_FLAG_COMPLETED_CRITERIA_COUNT     = 0x00000020,
    ACHIEVEMENT_FLAG_AVERAGE_PER_DAY              = 0x00000040,
    ACHIEVEMENT_FLAG_PROGRESS_BAR                 = 0x00000080,
    ACHIEVEMENT_FLAG_REALM_FIRST_REACH            = 0x00000100,
    ACHIEVEMENT_FLAG_REALM_FIRST_KILL             = 0x00000200,
    ACHIEVEMENT_FLAG_HIDE_NAME_ON_TIE             = 0x00000400,
    ACHIEVEMENT_FLAG_HIDE_UNTIL_COMPLETED         = 0x00000800,
};

class AchievementRec {
    public:
    static const int32_t s_numColumns = 62;
    static const int32_t s_rowSize = 248;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_faction;
    /* 0008 */ int32_t m_instanceID;
    /* 000C */ int32_t m_supercedes;
    /* 0010 */ const char* m_title;
    /* 0014 */ const char* m_description;
    /* 0018 */ int32_t m_category;
    /* 001C */ int32_t m_points;
    /* 0020 */ int32_t m_uiOrder;
    /* 0024 */ int32_t m_flags;
    /* 0028 */ int32_t m_iconID;
    /* 002C */ const char* m_reward;
    /* 0030 */ int32_t m_minimumCriteria;
    /* 0034 */ int32_t m_sharesCriteria;

    AchievementRec();
    ~AchievementRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
