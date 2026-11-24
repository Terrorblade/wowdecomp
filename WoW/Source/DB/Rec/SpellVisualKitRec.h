#ifndef WOW_SOURCE_DB_REC_SPELLVISUALKITREC_H
#define WOW_SOURCE_DB_REC_SPELLVISUALKITREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_SPELL_VISUAL_CHAR_PROCS = 4;
constexpr int32_t NUM_SPELL_VISUAL_SPECIAL_EFFECTS = 3;

enum SPELL_VISUAL_KIT_ID : int32_t {
    SPELL_VISUAL_KIT_ID_GHOST = 989,
};

enum SPELL_VISUAL_CHAR_PROC : int32_t {
    SPELL_VISUAL_CHAR_PROC_NONE = -1,
    SPELL_VISUAL_CHAR_PROC_CHAIN_LIGHTNING = 0,
    SPELL_VISUAL_CHAR_PROC_COLOR_TINT = 1,
    SPELL_VISUAL_CHAR_PROC_UNKNOWN_2 = 2,
    SPELL_VISUAL_CHAR_PROC_UNKNOWN_3 = 3,
    SPELL_VISUAL_CHAR_PROC_UNKNOWN_4 = 4,
    SPELL_VISUAL_CHAR_PROC_UNKNOWN_5 = 5,
    SPELL_VISUAL_CHAR_PROC_ECLIPSE = 6,
    SPELL_VISUAL_CHAR_PROC_UNKNOWN_7 = 7,
    SPELL_VISUAL_CHAR_PROC_WEAPON_TRAIL = 8,
    SPELL_VISUAL_CHAR_PROC_BLIZZARD = 9,
    SPELL_VISUAL_CHAR_PROC_UNKNOWN_10 = 10,
    SPELL_VISUAL_CHAR_PROC_FREEZE_ANIMATION = 11,
    SPELL_VISUAL_CHAR_PROC_CHAIN_LIGHTNING_12 = 12,
    SPELL_VISUAL_CHAR_PROC_FLASH_COLOR = 13,
    SPELL_VISUAL_CHAR_PROC_ALPHA = 14,
    SPELL_VISUAL_CHAR_PROC_PROCEDURAL_FADE = 15,
    SPELL_VISUAL_CHAR_PROC_MOUNT_TRANSITION = 16,
    SPELL_VISUAL_CHAR_PROC_ITEM_VISUAL = 17,
};

class SpellVisualKitRec {
    public:

    enum KIT_FLAGS : int32_t {
        NONE = 0x0,
        WAIT_FOR_BEAM_ACTIVATION = 0x1,
        FORCE_SPECIFIC_ANIMATION = 0x4,
        BYPASS_ANIMATION_TIERS = 0x8,
        PLAY_SPECIFIC_WEAPON_SPECIAL = 0x10,
        LOOP_STAND_FOR_AURA = 0x20,
        REPEAT_CHANNEL_ANIMATION = 0x40,
        REQUIRES_ACTIVE_SPELL_EFFECT = 0x80,
        DO_NOT_SHEATHE_WEAPON = 0x100,
        IGNORE_PARENT_EFFECTIVE_ALPHA = 0x200,
        SKIP_MOUNT_ALPHA = 0x400,
        RAW_ANIMATION_ID_MASK = 0xC,
    };

    static const int32_t s_numColumns = 38;
    static const int32_t s_rowSize = 152;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_startAnimID;
    /* 0008 */ int32_t m_animID;
    /* 000C */ int32_t m_headEffect;
    /* 0010 */ int32_t m_chestEffect;
    /* 0014 */ int32_t m_baseEffect;
    /* 0018 */ int32_t m_leftHandEffect;
    /* 001C */ int32_t m_rightHandEffect;
    /* 0020 */ int32_t m_breathEffect;
    /* 0024 */ int32_t m_leftWeaponEffect;
    /* 0028 */ int32_t m_rightWeaponEffect;
    /* 002C */ int32_t m_specialEffect [NUM_SPELL_VISUAL_SPECIAL_EFFECTS];
    /* 0038 */ int32_t m_worldEffect;
    /* 003C */ int32_t m_soundID;
    /* 0040 */ int32_t m_shakeID;
    /* 0044 */ int32_t m_charProc [NUM_SPELL_VISUAL_CHAR_PROCS];
    /* 0054 */ float m_charParamZero [NUM_SPELL_VISUAL_CHAR_PROCS];
    /* 0064 */ float m_charParamOne [NUM_SPELL_VISUAL_CHAR_PROCS];
    /* 0074 */ float m_charParamTwo [NUM_SPELL_VISUAL_CHAR_PROCS];
    /* 0084 */ float m_charParamThree [NUM_SPELL_VISUAL_CHAR_PROCS];
    /* 0094 */ int32_t m_flags;

    SpellVisualKitRec();
    ~SpellVisualKitRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
