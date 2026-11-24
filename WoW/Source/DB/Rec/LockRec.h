#ifndef WOW_SOURCE_DB_REC_LOCKREC_H
#define WOW_SOURCE_DB_REC_LOCKREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum LOCK_KEY_TYPE : int32_t {
    LOCK_KEY_NONE = 0,
    LOCK_KEY_ITEM = 1,
    LOCK_KEY_SKILL = 2,
    LOCK_KEY_SPELL = 3,
};

enum LOCK_ACTION : int32_t {
    LOCK_ACTION_NOT_FOUND = -1,
    LOCK_ACTION_OPEN = 0,
    LOCK_ACTION_UNLOCK = 1,
    LOCK_ACTION_CLOSE = 2,
    LOCK_ACTION_OPEN_IGNORE_LOCK = 3,
    LOCK_ACTION_UNKNOWN_4 = 4,
};
constexpr int32_t NUM_LOCK_KEYS = 8;

class LockRec {
    public:
    static const int32_t s_numColumns = 33;
    static const int32_t s_rowSize = 132;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_type [NUM_LOCK_KEYS];
    /* 0024 */ int32_t m_index [NUM_LOCK_KEYS];
    /* 0044 */ int32_t m_skill [NUM_LOCK_KEYS];
    /* 0064 */ int32_t m_action [NUM_LOCK_KEYS];

    LockRec();
    ~LockRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
