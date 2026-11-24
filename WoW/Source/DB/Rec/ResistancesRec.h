#ifndef WOW_SOURCE_DB_REC_RESISTANCESREC_H
#define WOW_SOURCE_DB_REC_RESISTANCESREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum RESISTANCE_FLAGS : int32_t {
    RESISTANCE_FLAG_NONE     = 0x00000000,
    RESISTANCE_FLAG_PHYSICAL = 0x00000001,
};

class ResistancesRec {
    public:
    static const int32_t s_numColumns = 20;
    static const int32_t s_rowSize = 80;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_flags;
    /* 0008 */ int32_t m_fizzleSoundID;
    /* 000C */ const char* m_name;

    ResistancesRec();
    ~ResistancesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
