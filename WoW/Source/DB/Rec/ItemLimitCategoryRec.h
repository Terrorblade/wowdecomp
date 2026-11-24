#ifndef WOW_SOURCE_DB_REC_ITEMLIMITCATEGORYREC_H
#define WOW_SOURCE_DB_REC_ITEMLIMITCATEGORYREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum ITEM_LIMIT_CATEGORY_FLAGS : int32_t {
    ITEM_LIMIT_CATEGORY_FLAG_NONE     = 0x00000000,
    ITEM_LIMIT_CATEGORY_FLAG_MULTIPLE = 0x00000001,
};

class ItemLimitCategoryRec {
    public:
    static const int32_t s_numColumns = 20;
    static const int32_t s_rowSize = 80;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_quantity;
    /* 000C */ int32_t m_flags;

    ItemLimitCategoryRec();
    ~ItemLimitCategoryRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
