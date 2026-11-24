#ifndef WOW_SOURCE_DB_REC_ITEMRANDOMSUFFIXREC_H
#define WOW_SOURCE_DB_REC_ITEMRANDOMSUFFIXREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_ITEM_RANDOM_SUFFIX_ENCHANTMENTS = 5;

class ItemRandomSuffixRec {
    public:
    static const int32_t s_numColumns = 29;
    static const int32_t s_rowSize = 116;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ const char* m_internalName;
    /* 000C */ int32_t m_enchantment [NUM_ITEM_RANDOM_SUFFIX_ENCHANTMENTS];
    /* 0020 */ int32_t m_allocationPct [NUM_ITEM_RANDOM_SUFFIX_ENCHANTMENTS];

    ItemRandomSuffixRec();
    ~ItemRandomSuffixRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
