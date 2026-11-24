#ifndef WOW_SOURCE_DB_REC_GEMPROPERTIESREC_H
#define WOW_SOURCE_DB_REC_GEMPROPERTIESREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum SOCKET_COLOR : int32_t {
    SOCKET_COLOR_META = 0x1,
    SOCKET_COLOR_RED = 0x2,
    SOCKET_COLOR_YELLOW = 0x4,
    SOCKET_COLOR_BLUE = 0x8,
};
constexpr int32_t NUM_SOCKET_COLORS = 4;

class GemPropertiesRec {
    public:
    static const int32_t s_numColumns = 5;
    static const int32_t s_rowSize = 20;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_enchantID;
    /* 0008 */ int32_t m_maxcountInv;
    /* 000C */ int32_t m_maxcountItem;
    /* 0010 */ int32_t m_type;

    GemPropertiesRec();
    ~GemPropertiesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
