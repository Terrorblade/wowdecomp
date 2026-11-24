#ifndef WOW_SOURCE_DB_REC_ITEMRANDOMPROPERTIESREC_H
#define WOW_SOURCE_DB_REC_ITEMRANDOMPROPERTIESREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_ITEM_RANDOM_PROPERTY_ENCHANTMENTS = 5;

class ItemRandomPropertiesRec {
    public:
    static const int32_t s_numColumns = 24;
    static const int32_t s_rowSize = 96;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_enchantment [NUM_ITEM_RANDOM_PROPERTY_ENCHANTMENTS];
    /* 001C */ const char* m_nameLang;

    ItemRandomPropertiesRec();
    ~ItemRandomPropertiesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
