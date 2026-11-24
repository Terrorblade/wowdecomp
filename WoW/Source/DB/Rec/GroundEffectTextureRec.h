#ifndef WOW_SOURCE_DB_REC_GROUNDEFFECTTEXTUREREC_H
#define WOW_SOURCE_DB_REC_GROUNDEFFECTTEXTUREREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_GROUND_EFFECT_DOODADS = 4;

class GroundEffectTextureRec {
    public:
    static const int32_t s_numColumns = 11;
    static const int32_t s_rowSize = 44;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_doodadID [NUM_GROUND_EFFECT_DOODADS];
    /* 0014 */ int32_t m_doodadWeight [NUM_GROUND_EFFECT_DOODADS];
    /* 0024 */ int32_t m_density;
    /* 0028 */ int32_t m_sound;

    GroundEffectTextureRec();
    ~GroundEffectTextureRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
