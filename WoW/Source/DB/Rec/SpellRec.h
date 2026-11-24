#ifndef WOW_SOURCE_DB_REC_SPELLREC_H
#define WOW_SOURCE_DB_REC_SPELLREC_H

#include "WoW/Source/Magic/MagicClient/SpellEnums.h"

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_SPELL_VISUALS = 2;
constexpr int32_t NUM_SPELL_REQUIRED_TOTEM_CATEGORIES = 2;

class SpellRec {
    public:

    enum ProcTypeFlags : int32_t {
        PROC_HEARTBEAT = 0x00000001,
    };

    enum InterruptFlags : int32_t {
        INTERRUPT_ON_MOVEMENT = 0x00000001,
        INTERRUPT_DMG_PUSHBACK_PLAYER = 0x00000002,
        INTERRUPT_STUN = 0x00000004,
        INTERRUPT_ON_COMBAT = 0x00000008,
        INTERRUPT_DMG_CANCELS_PLAYER = 0x00000010,
        INTERRUPT_MELEE_COMBAT = 0x00000020,
    };

    enum AuraInterruptFlags : int32_t {
        AURA_HOSTILE_ACTION_RECEIVED_CANCELS = 0x00000001,
        AURA_DAMAGE_CANCELS = 0x00000002,
        AURA_ACTION_CANCELS = 0x00000004,
        AURA_MOVING_CANCELS = 0x00000008,
        AURA_TURNING_CANCELS = 0x00000010,
        AURA_ANIM_CANCELS = 0x00000020,
        AURA_REQUIRE_MOUNTED = 0x00000040,
        AURA_REQUIRE_NOT_SWIMMING = 0x00000080,
        AURA_REQUIRE_SWIMMING = 0x00000100,
        AURA_REQUIRE_UNSHEATHED = 0x00000200,
        AURA_INTERACTING_CANCELS = 0x00000400,
        AURA_LOOTING_CANCELS = 0x00000800,
        AURA_ATTACKING_CANCELS = 0x00001000,
        AURA_ITEM_USE_CANCELS = 0x00002000,
        AURA_DAMAGE_CHANNEL_DURATION = 0x00004000,
        AURA_SHAPESHIFTING_CANCELS = 0x00008000,
        AURA_ACTION_CANCELS_LATE = 0x00010000,
        AURA_MOUNT_CANCELS = 0x00020000,
        AURA_STANDING_CANCELS = 0x00040000,
        AURA_LEAVE_WORLD_CANCELS = 0x00080000,
        AURA_STEALTH_OR_INVIS_CANCELS = 0x00100000,
        AURA_INVULNERABILITY_BUFF_CANCELS = 0x00200000,
        AURA_ENTER_WORLD_CANCELS = 0x00400000,
        AURA_PVP_ACTIVE_CANCELS = 0x00800000,
        AURA_NON_PERIODIC_DAMAGE_CANCELS = 0x01000000,
        AURA_LANDING_CANCELS = 0x02000000,
        AURA_RELEASE_CANCELS = 0x04000000,
        AURA_ENTERING_COMBAT_CANCELS = 0x10000000,
        AURA_LOGIN_CANCELS = 0x20000000,
        AURA_SUMMON_CANCELS = 0x40000000,
        AURA_LEAVING_COMBAT_CANCELS = INT32_MIN,
        AURA_MOVEMENT_GATE_MASK = 0x00000018,
    };

    enum ChannelInterruptFlags : int32_t {
        CHANNEL_HOSTILE_ACTION_RECEIVED_CANCELS = 0x00000001,
        CHANNEL_DAMAGE_CANCELS = 0x00000002,
        CHANNEL_ACTION_CANCELS = 0x00000004,
        CHANNEL_INTERRUPT_ON_MOVEMENT = 0x00000008,
        CHANNEL_INTERRUPT_ON_TURN = 0x00000010,
        CHANNEL_ANIM_CANCELS = 0x00000020,
        CHANNEL_REQUIRE_MOUNTED = 0x00000040,
        CHANNEL_REQUIRE_NOT_SWIMMING = 0x00000080,
        CHANNEL_REQUIRE_SWIMMING = 0x00000100,
        CHANNEL_REQUIRE_UNSHEATHED = 0x00000200,
        CHANNEL_INTERACTING_CANCELS = 0x00000400,
        CHANNEL_LOOTING_CANCELS = 0x00000800,
        CHANNEL_INTERRUPT_ON_COMBAT = 0x00001000,
        CHANNEL_ITEM_USE_CANCELS = 0x00002000,
        CHANNEL_DELAY = 0x00004000,
        CHANNEL_SHAPESHIFTING_CANCELS = 0x00008000,
        CHANNEL_ACTION_CANCELS_LATE = 0x00010000,
        CHANNEL_MOUNT_CANCELS = 0x00020000,
        CHANNEL_STANDING_CANCELS = 0x00040000,
        CHANNEL_LEAVE_WORLD_CANCELS = 0x00080000,
        CHANNEL_STEALTH_OR_INVIS_CANCELS = 0x00100000,
        CHANNEL_INVULNERABILITY_BUFF_CANCELS = 0x00200000,
        CHANNEL_ENTER_WORLD_CANCELS = 0x00400000,
        CHANNEL_NON_PERIODIC_DAMAGE_CANCELS = 0x01000000,
        CHANNEL_ENTERING_COMBAT_CANCELS = 0x10000000,
        CHANNEL_LOGIN_CANCELS = 0x20000000,
        CHANNEL_LEAVING_COMBAT_CANCELS = INT32_MIN,
        CHANNEL_MOVEMENT_OR_TURN_MASK = 0x00000018,
    };

    static const int32_t s_numColumns = 234;
    static const int32_t s_rowSize = 936;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int32_t m_category;
    /* 0008 */ int32_t m_dispelType;
    /* 000C */ int32_t m_mechanic;
    /* 0010 */ int32_t m_attributes;
    /* 0014 */ int32_t m_attributesEx;
    /* 0018 */ int32_t m_attributesExB;
    /* 001C */ int32_t m_attributesExC;
    /* 0020 */ int32_t m_attributesExD;
    /* 0024 */ int32_t m_attributesExE;
    /* 0028 */ int32_t m_attributesExF;
    /* 002C */ int32_t m_attributesExG;
    /* 0030 */ int32_t m_shapeshiftMask [NUM_SPELL_SHAPESHIFT_MASK_WORDS];
    /* 0038 */ int32_t m_shapeshiftExclude [NUM_SPELL_SHAPESHIFT_MASK_WORDS];
    /* 0040 */ int32_t m_targets;
    /* 0044 */ int32_t m_targetCreatureType;
    /* 0048 */ int32_t m_requiresSpellFocus;
    /* 004C */ int32_t m_facingCasterFlags;
    /* 0050 */ int32_t m_casterAuraState;
    /* 0054 */ int32_t m_targetAuraState;
    /* 0058 */ int32_t m_excludeCasterAuraState;
    /* 005C */ int32_t m_excludeTargetAuraState;
    /* 0060 */ int32_t m_casterAuraSpell;
    /* 0064 */ int32_t m_targetAuraSpell;
    /* 0068 */ int32_t m_excludeCasterAuraSpell;
    /* 006C */ int32_t m_excludeTargetAuraSpell;
    /* 0070 */ int32_t m_castingTimeIndex;
    /* 0074 */ int32_t m_recoveryTime;
    /* 0078 */ int32_t m_categoryRecoveryTime;
    /* 007C */ int32_t m_interruptFlags;
    /* 0080 */ int32_t m_auraInterruptFlags;
    /* 0084 */ int32_t m_channelInterruptFlags;
    /* 0088 */ int32_t m_procTypeMask;
    /* 008C */ int32_t m_procChance;
    /* 0090 */ int32_t m_procCharges;
    /* 0094 */ int32_t m_maxLevel;
    /* 0098 */ int32_t m_baseLevel;
    /* 009C */ int32_t m_spellLevel;
    /* 00A0 */ int32_t m_durationIndex;
    /* 00A4 */ int32_t m_powerType;
    /* 00A8 */ int32_t m_manaCost;
    /* 00AC */ int32_t m_manaCostPerLevel;
    /* 00B0 */ int32_t m_manaPerSecond;
    /* 00B4 */ int32_t m_manaPerSecondPerLevel;
    /* 00B8 */ int32_t m_rangeIndex;
    /* 00BC */ float m_speed;
    /* 00C0 */ int32_t m_modalNextSpell;
    /* 00C4 */ int32_t m_cumulativeAura;
    /* 00C8 */ int32_t m_totem [2];
    /* 00D0 */ int32_t m_reagent [NUM_SPELL_REAGENTS];
    /* 00F0 */ int32_t m_reagentCount [NUM_SPELL_REAGENTS];
    /* 0110 */ int32_t m_equippedItemClass;
    /* 0114 */ int32_t m_equippedItemSubclass;
    /* 0118 */ int32_t m_equippedItemInvTypes;
    /* 011C */ int32_t m_effect [NUM_SPELL_EFFECTS];
    /* 0128 */ int32_t m_effectDieSides [NUM_SPELL_EFFECTS];
    /* 0134 */ float m_effectRealPointsPerLevel [NUM_SPELL_EFFECTS];
    /* 0140 */ int32_t m_effectBasePoints [NUM_SPELL_EFFECTS];
    /* 014C */ int32_t m_effectMechanic [NUM_SPELL_EFFECTS];
    /* 0158 */ int32_t m_implicitTargetA [NUM_SPELL_EFFECTS];
    /* 0164 */ int32_t m_implicitTargetB [NUM_SPELL_EFFECTS];
    /* 0170 */ int32_t m_effectRadiusIndex [NUM_SPELL_EFFECTS];
    /* 017C */ int32_t m_effectAura [NUM_SPELL_EFFECTS];
    /* 0188 */ int32_t m_effectAuraPeriod [NUM_SPELL_EFFECTS];
    /* 0194 */ float m_effectAmplitude [NUM_SPELL_EFFECTS];
    /* 01A0 */ int32_t m_effectChainTargets [NUM_SPELL_EFFECTS];
    /* 01AC */ int32_t m_effectItemType [NUM_SPELL_EFFECTS];
    /* 01B8 */ int32_t m_effectMiscValue [NUM_SPELL_EFFECTS];
    /* 01C4 */ int32_t m_effectMiscValueB [NUM_SPELL_EFFECTS];
    /* 01D0 */ int32_t m_effectTriggerSpell [NUM_SPELL_EFFECTS];
    /* 01DC */ float m_effectPointsPerCombo [NUM_SPELL_EFFECTS];
    /* 01E8 */ int32_t m_effectSpellClassMaskA [NUM_SPELL_EFFECTS];
    /* 01F4 */ int32_t m_effectSpellClassMaskB [NUM_SPELL_EFFECTS];
    /* 0200 */ int32_t m_effectSpellClassMaskC [NUM_SPELL_EFFECTS];
    /* 020C */ int32_t m_spellVisualID [NUM_SPELL_VISUALS];
    /* 0214 */ int32_t m_spellIconID;
    /* 0218 */ int32_t m_activeIconID;
    /* 021C */ int32_t m_spellPriority;
    /* 0220 */ const char* m_name;
    /* 0224 */ const char* m_nameSubtext;
    /* 0228 */ const char* m_description;
    /* 022C */ const char* m_auraDescription;
    /* 0230 */ int32_t m_manaCostPct;
    /* 0234 */ int32_t m_startRecoveryCategory;
    /* 0238 */ int32_t m_startRecoveryTime;
    /* 023C */ int32_t m_maxTargetLevel;
    /* 0240 */ int32_t m_spellClassSet;
    /* 0244 */ int32_t m_spellClassMask [NUM_SPELL_CLASSES_MASK];
    /* 0250 */ int32_t m_maxTargets;
    /* 0254 */ int32_t m_defenseType;
    /* 0258 */ int32_t m_preventionType;
    /* 025C */ int32_t m_stanceBarOrder;
    /* 0260 */ float m_effectChainAmplitude [NUM_SPELL_EFFECTS];
    /* 026C */ int32_t m_minFactionID;
    /* 0270 */ int32_t m_minReputation;
    /* 0274 */ int32_t m_requiredAuraVision;
    /* 0278 */ int32_t m_requiredTotemCategoryID [NUM_SPELL_REQUIRED_TOTEM_CATEGORIES];
    /* 0280 */ int32_t m_requiredAreasID;
    /* 0284 */ int32_t m_schoolMask;
    /* 0288 */ int32_t m_runeCostID;
    /* 028C */ int32_t m_spellMissileID;
    /* 0290 */ int32_t m_powerDisplayID;
    /* 0294 */ float m_effectBonusCoefficient [NUM_SPELL_EFFECTS];
    /* 02A0 */ int32_t m_descriptionVariablesID;
    /* 02A4 */ int32_t m_difficulty;

    SpellRec();
    ~SpellRec();
    static const char* GetFilename();
    static bool HasAura(const SpellRec* spell, int32_t aura);
    static int32_t IsHelpfulOrHarmful(const SpellRec* spell);
    static bool WillSummonCritter(const SpellRec* spell);
    static const int32_t* GetClassMask(const SpellRec* spell, int32_t effect);
    static bool IsAffectedBy(const SpellRec* spell, const SpellRec* modifier, int32_t effect);
    static bool UsableInShapeshift(const SpellRec* spell, int32_t form);
    static bool NotUsableInShapeshiftForm(const SpellRec* spell, int32_t form);
    static int32_t GetCastTime(const SpellRec* spell, int32_t isPet, int32_t isUnit, int32_t allowNegative);
    static bool IsMeleeRange(const SpellRec* spell);
    static bool IsMinimumRange(const SpellRec* spell);
    static void GetMinRange(const SpellRec* spell, float* range);
    static bool HasModifier(const SpellRec* spell, int32_t modifier);
    bool Read(SFileImpl* file, const char* stringBuffer);
};

#endif
