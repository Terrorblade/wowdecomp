#ifndef WOW_SOURCE_DB_REC_SPELLCHAINEFFECTSREC_H
#define WOW_SOURCE_DB_REC_SPELLCHAINEFFECTSREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class SpellChainEffectsRec {
    public:

    enum CHAIN_EFFECT_FLAGS : int32_t {
        USE_FIXED_JOINT_COUNT = 0x1,
        USE_HIERARCHICAL_POINTS = 0x2,
        JOINT_JUMP = 0x4,
        HELICAL_WAVE = 0x8,
        FLICKER = 0x10,
        PULSE = 0x20,
        LOOP_PULSE = 0x40,
        UNKNOWN_80 = 0x80,
        SCALE_TEXTURE_BY_LENGTH = 0x100,
        OFFSET_TEXTURE_BY_LENGTH = 0x200,
        USE_DEFINED_WAVE_PHASE = 0x400,
    };

    static const int32_t s_numColumns = 48;
    static const int32_t s_rowSize = 177;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ float m_avgSegLen;
    /* 0008 */ float m_width;
    /* 000C */ float m_noiseScale;
    /* 0010 */ float m_texCoordScale;
    /* 0014 */ int32_t m_segDuration;
    /* 0018 */ int32_t m_segDelay;
    /* 001C */ const char* m_texture;
    /* 0020 */ int32_t m_flags;
    /* 0024 */ int32_t m_jointCount;
    /* 0028 */ float m_jointOffsetRadius;
    /* 002C */ int32_t m_jointsPerMinorJoint;
    /* 0030 */ int32_t m_minorJointsPerMajorJoint;
    /* 0034 */ float m_minorJointScale;
    /* 0038 */ float m_majorJointScale;
    /* 003C */ float m_jointMoveSpeed;
    /* 0040 */ float m_jointSmoothness;
    /* 0044 */ float m_minDurationBetweenJointJumps;
    /* 0048 */ float m_maxDurationBetweenJointJumps;
    /* 004C */ float m_waveHeight;
    /* 0050 */ float m_waveFreq;
    /* 0054 */ float m_waveSpeed;
    /* 0058 */ float m_minWaveAngle;
    /* 005C */ float m_maxWaveAngle;
    /* 0060 */ float m_minWaveSpin;
    /* 0064 */ float m_maxWaveSpin;
    /* 0068 */ float m_arcHeight;
    /* 006C */ float m_minArcAngle;
    /* 0070 */ float m_maxArcAngle;
    /* 0074 */ float m_minArcSpin;
    /* 0078 */ float m_maxArcSpin;
    /* 007C */ float m_delayBetweenEffects;
    /* 0080 */ float m_minFlickerOnDuration;
    /* 0084 */ float m_maxFlickerOnDuration;
    /* 0088 */ float m_minFlickerOffDuration;
    /* 008C */ float m_maxFlickerOffDuration;
    /* 0090 */ float m_pulseSpeed;
    /* 0094 */ float m_pulseOnLength;
    /* 0098 */ float m_pulseFadeLength;
    /* 009C */ int8_t m_alpha;
    /* 009D */ int8_t m_red;
    /* 009E */ int8_t m_green;
    /* 009F */ int8_t m_blue;
    /* 00A0 */ int8_t m_blendMode;
    /* 00A4 */ const char* m_combo;
    /* 00A8 */ int32_t m_renderLayer;
    /* 00AC */ float m_textureLength;
    /* 00B0 */ float m_wavePhase;

    SpellChainEffectsRec();
    ~SpellChainEffectsRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
