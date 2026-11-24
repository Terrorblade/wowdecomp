#ifndef WOW_SOURCE_DB_REC_SOUNDENTRIESREC_H
#define WOW_SOURCE_DB_REC_SOUNDENTRIESREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_SOUND_ENTRY_FILES = 10;

enum SOUND_ENTRY_FLAGS : uint32_t {
    SOUND_ENTRY_FLAG_NONE          = 0x00000000,
    SOUND_ENTRY_FLAG_SUPPRESSIBLE  = 0x00000020,
    SOUND_ENTRY_FLAG_LOOPING       = 0x00000200,
    SOUND_ENTRY_FLAG_RANDOM_PITCH  = 0x00000400,
    SOUND_ENTRY_FLAG_RANDOM_VOLUME = 0x00000800,
};

enum SOUND_ENTRY_ID : int32_t {
    SOUND_ENTRY_ID_DEATHBIND = 1141,
    SOUND_ENTRY_ID_CLAP = 6576,
    SOUND_ENTRY_ID_PVP_ENTER_QUEUE = 8462,
    SOUND_ENTRY_ID_DRUID_MORPH_IMPACT_BASE = 10063,
    SOUND_ENTRY_ID_AGGRO_ENTER_WARNING_STATE = 15262,
    SOUND_ENTRY_ID_AGGRO_PULLED_AGGRO = 15263,
    SOUND_ENTRY_ID_LFG_DUNGEON_READY = 17318,
    SOUND_ENTRY_ID_LFG_DENIED = 17341,
};

class SoundEntriesRec {
    public:
    static const int32_t s_numColumns = 30;
    static const int32_t s_rowSize = 120;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_soundType;
    /* 0008 */ const char* m_name;
    /* 000C */ const char* m_file [NUM_SOUND_ENTRY_FILES];
    /* 0034 */ int32_t m_freq [NUM_SOUND_ENTRY_FILES];
    /* 005C */ const char* m_directoryBase;
    /* 0060 */ float m_volumeFloat;
    /* 0064 */ int32_t m_flags;
    /* 0068 */ float m_minDistance;
    /* 006C */ float m_distanceCutoff;
    /* 0070 */ int32_t m_eaxdef;
    /* 0074 */ int32_t m_soundEntriesAdvancedID;

    SoundEntriesRec();
    ~SoundEntriesRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
