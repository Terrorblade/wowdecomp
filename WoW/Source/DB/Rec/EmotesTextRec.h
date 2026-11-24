#ifndef WOW_SOURCE_DB_REC_EMOTESTEXTREC_H
#define WOW_SOURCE_DB_REC_EMOTESTEXTREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

enum EMOTES_TEXT_SLOT : uint32_t {
    EMOTES_TEXT_SLOT_TARGET_IS_PLAYER = 0x1,
    EMOTES_TEXT_SLOT_SENDER_IS_PLAYER = 0x2,
    EMOTES_TEXT_SLOT_NO_TARGET = 0x4,
    EMOTES_TEXT_SLOT_FEMALE = 0x8,
    NUM_EMOTES_TEXT_SLOTS = 16,
};

class EmotesTextRec {
    public:
    static const int32_t s_numColumns = 19;
    static const int32_t s_rowSize = 76;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_emoteID;
    /* 000C */ int32_t m_emoteText [NUM_EMOTES_TEXT_SLOTS];

    EmotesTextRec();
    ~EmotesTextRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
