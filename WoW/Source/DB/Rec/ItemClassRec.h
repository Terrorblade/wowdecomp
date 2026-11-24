#ifndef WOW_SOURCE_DB_REC_ITEMCLASSREC_H
#define WOW_SOURCE_DB_REC_ITEMCLASSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum ITEM_CLASS_FLAGS : int32_t {
    ITEM_CLASS_FLAG_NONE   = 0x00000000,
    ITEM_CLASS_FLAG_WEAPON = 0x00000001,
};

class ItemClassRec {
    public:
    static const int32_t s_numColumns = 20;
    static const int32_t s_rowSize = 80;
    static const bool s_generatedID = true;

    /* 0000 */ int32_t m_classID;
    /* 0004 */ int32_t m_subclassMapID;
    /* 0008 */ int32_t m_flags;
    /* 000C */ const char* m_className;
    /* 0010 */ int32_t m_generatedID;

    ItemClassRec();
    ~ItemClassRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
