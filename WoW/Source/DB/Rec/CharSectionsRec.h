#ifndef WOW_SOURCE_DB_REC_CHARSECTIONSREC_H
#define WOW_SOURCE_DB_REC_CHARSECTIONSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class CharSectionsRec {
    public:

    enum SECTION_FLAGS : int32_t {
        PLAYER = 0x1,
        BARBERSHOP = 0x2,
        DEATH_KNIGHT = 0x4,
        NPC_SKIN = 0x8,
        DEATH_KNIGHT_COMPATIBLE = 0x10,
        PLAYER_OR_BARBERSHOP = 0x3,
        DEATH_KNIGHT_OR_NPC_SKIN = 0xC,
        DEATH_KNIGHT_VARIANTS = 0x14,
    };

    enum TEXTURE_INDEX : int32_t {
        SKIN_BODY = 0,
        SKIN_EXTRA = 1,
        FACE_LOWER = 0,
        FACE_UPPER = 1,
        FACIAL_HAIR_LOWER = 0,
        FACIAL_HAIR_UPPER = 1,
        HAIR_SCALP = 0,
        HAIR_LOWER = 1,
        HAIR_UPPER = 2,
        UNDERWEAR_LOWER = 0,
        UNDERWEAR_UPPER = 1,
        COUNT = 3,
    };

    static const int32_t s_numColumns = 10;
    static const int32_t s_rowSize = 40;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_raceID;
    /* 0008 */ int32_t m_sexID;
    /* 000C */ int32_t m_baseSection;
    /* 0010 */ const char* m_textureName [COUNT];
    /* 001C */ int32_t m_flags;
    /* 0020 */ int32_t m_variationIndex;
    /* 0024 */ int32_t m_colorIndex;

    CharSectionsRec();
    ~CharSectionsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
