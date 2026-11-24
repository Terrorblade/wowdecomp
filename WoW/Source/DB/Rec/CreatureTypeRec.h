#ifndef WOW_SOURCE_DB_REC_CREATURETYPEREC_H
#define WOW_SOURCE_DB_REC_CREATURETYPEREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum CREATURE_TYPE_REC_FLAGS : int32_t {
    CREATURE_TYPE_REC_FLAG_NONE          = 0x00000000,
    CREATURE_TYPE_REC_FLAG_NO_TAB_TARGET = 0x00000001,
};

class CreatureTypeRec {
    public:
    static const int32_t s_numColumns = 19;
    static const int32_t s_rowSize = 76;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_flags;

    CreatureTypeRec();
    ~CreatureTypeRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
