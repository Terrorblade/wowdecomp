#ifndef WOW_SOURCE_DB_REC_ITEMSUBCLASSREC_H
#define WOW_SOURCE_DB_REC_ITEMSUBCLASSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum ITEM_SUBCLASS_FLAGS : int32_t {
    ITEM_SUBCLASS_FLAG_UNARMED = 0x4,
    ITEM_SUBCLASS_FLAG_SKIP_AMMO_CHECK = 0x10,
    ITEM_SUBCLASS_FLAG_AUCTION_INVENTORY_FILTER = 0x200,
};

enum ITEM_SUBCLASS_DISPLAY_FLAGS : int32_t {
    ITEM_SUBCLASS_DISPLAY_FLAG_HIDE_IN_TOOLTIP = 0x1,
    ITEM_SUBCLASS_DISPLAY_FLAG_HIDE_IN_AUCTION = 0x2,
    ITEM_SUBCLASS_DISPLAY_FLAG_SHOW_CONTENTS_COUNT = 0x4,
};

class ItemSubClassRec {
    public:
    static const int32_t s_numColumns = 44;
    static const int32_t s_rowSize = 176;
    static const bool s_generatedID = true;

    /* 0000 */ int32_t m_classID;
    /* 0004 */ int32_t m_subClassID;
    /* 0008 */ int32_t m_prerequisiteProficiency;
    /* 000C */ int32_t m_postrequisiteProficiency;
    /* 0010 */ int32_t m_flags;
    /* 0014 */ int32_t m_displayFlags;
    /* 0018 */ int32_t m_weaponParrySeq;
    /* 001C */ int32_t m_weaponReadySeq;
    /* 0020 */ int32_t m_weaponAttackSeq;
    /* 0024 */ int32_t m_weaponSwingSize;
    /* 0028 */ const char* m_displayName;
    /* 002C */ const char* m_verboseName;
    /* 0030 */ int32_t m_generatedID;

    ItemSubClassRec();
    ~ItemSubClassRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
