#ifndef WOW_SOURCE_DB_REC_CFG_CATEGORIESREC_H
#define WOW_SOURCE_DB_REC_CFG_CATEGORIESREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum CFG_CATEGORY_FLAGS : int32_t {
    CFG_CATEGORY_FLAG_NONE       = 0x00000000,
    CFG_CATEGORY_FLAG_TOURNAMENT = 0x00000001,
};

class Cfg_CategoriesRec {
    public:
    static const int32_t s_numColumns = 21;
    static const int32_t s_rowSize = 84;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_localeMask;
    /* 0008 */ int32_t m_createCharsetMask;
    /* 000C */ int32_t m_flags;
    /* 0010 */ const char* m_name;

    Cfg_CategoriesRec();
    ~Cfg_CategoriesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
