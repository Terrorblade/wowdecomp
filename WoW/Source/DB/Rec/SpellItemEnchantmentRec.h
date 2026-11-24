#ifndef WOW_SOURCE_DB_REC_SPELLITEMENCHANTMENTREC_H
#define WOW_SOURCE_DB_REC_SPELLITEMENCHANTMENTREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_SPELL_ITEM_ENCHANTMENT_EFFECTS = 3;

enum ITEM_ENCHANTMENT_EFFECT : int32_t {
    ENCHANTMENT_EFFECT_NONE                  = 0,
    ENCHANTMENT_EFFECT_COMBAT_SPELL          = 1,
    ENCHANTMENT_EFFECT_WEAPON_DAMAGE         = 2,
    ENCHANTMENT_EFFECT_EQUIP_SPELL           = 3,
    ENCHANTMENT_EFFECT_RESISTANCE            = 4,
    ENCHANTMENT_EFFECT_STAT                  = 5,
    ENCHANTMENT_EFFECT_WEAPON_DPS            = 6,
    ENCHANTMENT_EFFECT_USE_SPELL             = 7,
    ENCHANTMENT_EFFECT_PRISMATIC_SOCKET      = 8,
};

enum ITEM_ENCHANTMENT_FLAGS : int32_t {
    ENCHANTMENT_FLAG_NONE      = 0x00000000,
    ENCHANTMENT_FLAG_SOULBOUND = 0x00000001,
    ENCHANTMENT_FLAG_SUPPRESS_COMBAT_LOG = 0x00000002,
};

class SpellItemEnchantmentRec {
    public:
    static const int32_t s_numColumns = 38;
    static const int32_t s_rowSize = 152;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_charges;
    /* 0008 */ int32_t m_effect [NUM_SPELL_ITEM_ENCHANTMENT_EFFECTS];
    /* 0014 */ int32_t m_effectPointsMin [NUM_SPELL_ITEM_ENCHANTMENT_EFFECTS];
    /* 0020 */ int32_t m_effectPointsMax [NUM_SPELL_ITEM_ENCHANTMENT_EFFECTS];
    /* 002C */ int32_t m_effectArg [NUM_SPELL_ITEM_ENCHANTMENT_EFFECTS];
    /* 0038 */ const char* m_name;
    /* 003C */ int32_t m_itemVisual;
    /* 0040 */ int32_t m_flags;
    /* 0044 */ int32_t m_srcItemID;
    /* 0048 */ int32_t m_conditionID;
    /* 004C */ int32_t m_requiredSkillID;
    /* 0050 */ int32_t m_requiredSkillRank;
    /* 0054 */ int32_t m_minLevel;

    SpellItemEnchantmentRec();
    ~SpellItemEnchantmentRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
