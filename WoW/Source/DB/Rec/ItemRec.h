#ifndef WOW_SOURCE_DB_REC_ITEMREC_H
#define WOW_SOURCE_DB_REC_ITEMREC_H

#include <cstddef>
#include <cstdint>
#include "WoW/Common/ItemTypes.h"

class SFileImpl;

enum ITEM_ID : int32_t {
    ITEM_ID_HEARTHSTONE = 6948,
};

class ItemRec {
    public:
    static const int32_t s_numColumns = 8;
    static const int32_t s_rowSize = 32;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_classID;
    /* 0008 */ int32_t m_subclassID;
    /* 000C */ int32_t m_soundOverrideSubclassID;
    /* 0010 */ int32_t m_material;
    /* 0014 */ int32_t m_displayInfoID;
    /* 0018 */ int32_t m_inventoryType;
    /* 001C */ int32_t m_sheatheType;

    ItemRec();
    ~ItemRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
