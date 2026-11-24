#ifndef WOW_SOURCE_DB_REC_EMOTESREC_H
#define WOW_SOURCE_DB_REC_EMOTESREC_H

#include <cstddef>
#include <cstdint>

enum EMOTE_FLAGS : int32_t {
    EMOTE_FLAG_REQUIRES_STANDING = 0x1,
    EMOTE_FLAG_TALK = 0x8,
    EMOTE_FLAG_QUESTION = 0x10,
    EMOTE_FLAG_EXCLAMATION = 0x20,
    EMOTE_FLAG_SHOUT = 0x40,
    EMOTE_FLAG_DISALLOW_SWIMMING = 0x80,
    EMOTE_FLAG_LAUGH = 0x100,
    EMOTE_FLAG_ALLOW_SLEEPING_OR_DEAD = 0x200,
    EMOTE_FLAG_DISALLOW_TARGETED = 0x400,
    EMOTE_FLAG_SUPPRESS_STATE_ON_INTERACTION = 0x2000,
    EMOTE_FLAG_STAND_UP_ON_ACTIVE_MOVEMENT = 0x4000,
    EMOTE_FLAG_DISALLOW_FLYING = 0x8000,
};

enum EMOTESPECPROCS : int32_t {
    EMOTESPECPROC_NONE = 0,
    EMOTESPECPROC_STANDSTATEHANDLER = 1,
    EMOTESPECPROC_EMOTESTATEHANDLER = 2,
    // EMOTESPECPROC_NUMSPECPROCS = 3,
    EMOTESPECPROC_NUMSPECPROCS = 3,
};

// Bitset indexed by EMOTESPECPROCS, separate from EmotesRec::m_emoteFlags.
enum EMOTESPECPROC_MASK : uint32_t {
    allEmoteFlags = (1u << EMOTESPECPROC_NONE)
        | (1u << EMOTESPECPROC_STANDSTATEHANDLER)
        | (1u << EMOTESPECPROC_EMOTESTATEHANDLER),
};

class SFileImpl;

class EmotesRec {
    public:
    static const int32_t s_numColumns = 7;
    static const int32_t s_rowSize = 28;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_emoteSlashCommand;
    /* 0008 */ int32_t m_animID;
    /* 000C */ int32_t m_emoteFlags;
    /* 0010 */ EMOTESPECPROCS m_emoteSpecProc;
    /* 0014 */ int32_t m_emoteSpecProcParam;
    /* 0018 */ int32_t m_eventSoundID;

    EmotesRec();
    ~EmotesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
