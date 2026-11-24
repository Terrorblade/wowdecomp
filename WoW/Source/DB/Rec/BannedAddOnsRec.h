#ifndef WOW_SOURCE_DB_REC_BANNEDADDONSREC_H
#define WOW_SOURCE_DB_REC_BANNEDADDONSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum BANNED_ADDON_FLAGS : int32_t {
    BANNED_ADDON_FLAG_NONE = 0x0,
    BANNED_ADDON_FLAG_ACTIVE = 0x1,
};

class BannedAddOnsRec {
    public:
    static const int32_t s_numColumns = 11;
    static const int32_t s_rowSize = 44;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_nameMd5 [4];
    /* 0014 */ int32_t m_versionMd5 [4];
    /* 0024 */ int32_t m_lastModified;
    /* 0028 */ int32_t m_flags;

    BannedAddOnsRec();
    ~BannedAddOnsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
