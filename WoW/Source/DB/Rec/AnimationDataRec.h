#ifndef WOW_SOURCE_DB_REC_ANIMATIONDATAREC_H
#define WOW_SOURCE_DB_REC_ANIMATIONDATAREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;

class AnimationDataRec {
    public:

    enum WeaponFlags : int32_t {
        ANIM_WEAPON_FLAG_UNKNOWN_2 = 0x4,
        ANIM_WEAPON_FLAG_SHEATHE = 0x10,
        ANIM_WEAPON_FLAG_UNSHEATHE = 0x20,
    };

    enum BodyFlags : int32_t {
        ANIM_BODY_FLAG_SUPPRESS_MOVEMENT = 0x80,
    };

    enum FallbackFlags : int32_t {
        ANIM_FLAG_REVERSE_FALLBACK = 0x10,
        ANIM_FLAG_FREEZE_FALLBACK = 0x20,
    };

    static const int32_t s_numColumns = 8;
    static const int32_t s_rowSize = 32;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ const char* m_name;
    /* 0008 */ int32_t m_weaponflags;
    /* 000C */ int32_t m_bodyflags;
    /* 0010 */ int32_t m_flags;
    /* 0014 */ int32_t m_fallback;
    /* 0018 */ int32_t m_behaviorID;
    /* 001C */ int32_t m_behaviorTier;

    AnimationDataRec();
    ~AnimationDataRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
