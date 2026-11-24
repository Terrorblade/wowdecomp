#ifndef WOW_SOURCE_DB_REC_STARTUP_STRINGSREC_H
#define WOW_SOURCE_DB_REC_STARTUP_STRINGSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum STARTUP_STRING_ID : int32_t {
    STARTUP_STRING_NONE = 0,
    MSG_TITLE_WOW = 1,
    MSG_GX_INIT_FAILED_D3D = 2,
    MSG_GX_INIT_FAILED = 3,
    MSG_HW_UNSUPPORTED = 4,
    MSG_HW_CHANGED = 5,
    MSG_GLUEXML_UI_CORRUPT = 9,
    MSG_FRAMEXML_UI_CORRUPT = 10,
    MSG_GX_NO_DEVICE = 11,
    MSG_REMOTE_DESKTOP = 12,
};

class Startup_StringsRec {
    public:
    static const int32_t s_numColumns = 19;
    static const int32_t s_rowSize = 76;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ const char* m_message;

    Startup_StringsRec();
    ~Startup_StringsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
