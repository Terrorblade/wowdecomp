#ifndef WOW_SOURCE_DB_REC_ACHIEVEMENT_CATEGORYREC_H
#define WOW_SOURCE_DB_REC_ACHIEVEMENT_CATEGORYREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum ACHIEVEMENT_CATEGORY_ID : int32_t {
    ACHIEVEMENT_CATEGORY_ALL_STATISTICS = -2,
    ACHIEVEMENT_CATEGORY_ALL_ACHIEVEMENTS = -1,
    ACHIEVEMENT_CATEGORY_STATISTICS = 1,
    ACHIEVEMENT_CATEGORY_FEATS_OF_STRENGTH = 81,
};

class Achievement_CategoryRec {
    public:
    static const int32_t s_numColumns = 20;
    static const int32_t s_rowSize = 80;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_parent;
    /* 0008 */ const char* m_name;
    /* 000C */ int32_t m_uiOrder;

    Achievement_CategoryRec();
    ~Achievement_CategoryRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
