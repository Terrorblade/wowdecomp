#ifndef WOW_SOURCE_DB_REC_DANCEMOVESREC_H
#define WOW_SOURCE_DB_REC_DANCEMOVESREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum DANCE_MOVE_TYPE : int32_t {
    DANCE_MOVE_ANIMATION        = 0,
    DANCE_MOVE_SPELL_VISUAL_KIT = 1,
    DANCE_MOVE_SOUND_KIT        = 2,
    DANCE_MOVE_HOLD             = 3,
    DANCE_MOVE_REPEAT           = 4,
};

class DanceMovesRec {
    public:
    static const int32_t s_numColumns = 24;
    static const int32_t s_rowSize = 96;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_type;
    /* 0008 */ int32_t m_param;
    /* 000C */ int32_t m_fallback;
    /* 0010 */ int32_t m_racemask;
    /* 0014 */ const char* m_internalName;
    /* 0018 */ const char* m_name;
    /* 001C */ int32_t m_lockID;

    DanceMovesRec();
    ~DanceMovesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
