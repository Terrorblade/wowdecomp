#ifndef WOW_SOURCE_DB_REC_ITEMPURCHASEGROUPREC_H
#define WOW_SOURCE_DB_REC_ITEMPURCHASEGROUPREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_ITEM_PURCHASE_GROUP_ITEMS = 8;

class ItemPurchaseGroupRec {
    public:
    static const int32_t s_numColumns = 26;
    static const int32_t s_rowSize = 104;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_itemID [NUM_ITEM_PURCHASE_GROUP_ITEMS];
    /* 0024 */ const char* m_name;

    ItemPurchaseGroupRec();
    ~ItemPurchaseGroupRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
