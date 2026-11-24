#ifndef WOW_SOURCE_DB_REC_CREATUREDISPLAYINFOEXTRAREC_H
#define WOW_SOURCE_DB_REC_CREATUREDISPLAYINFOEXTRAREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_CREATURE_DISPLAY_EXTRA_ITEMS = 11;

class CreatureDisplayInfoExtraRec {
    public:

    enum EXTRA_FLAGS : int32_t {
        HAS_BAKED_TEXTURE = 0x1,
    };

    static const int32_t s_numColumns = 21;
    static const int32_t s_rowSize = 84;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_displayRaceID;
    /* 0008 */ int32_t m_displaySexID;
    /* 000C */ int32_t m_skinID;
    /* 0010 */ int32_t m_faceID;
    /* 0014 */ int32_t m_hairStyleID;
    /* 0018 */ int32_t m_hairColorID;
    /* 001C */ int32_t m_facialHairID;
    /* 0020 */ int32_t m_npcitemDisplay [NUM_CREATURE_DISPLAY_EXTRA_ITEMS];
    /* 004C */ int32_t m_flags;
    /* 0050 */ const char* m_bakeName;

    CreatureDisplayInfoExtraRec();
    ~CreatureDisplayInfoExtraRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
