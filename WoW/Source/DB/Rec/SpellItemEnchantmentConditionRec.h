#ifndef WOW_SOURCE_DB_REC_SPELLITEMENCHANTMENTCONDITIONREC_H
#define WOW_SOURCE_DB_REC_SPELLITEMENCHANTMENTCONDITIONREC_H

#include <cstddef>
#include <cstdint>

class SFileImpl;
constexpr int32_t NUM_ENCHANTMENT_CONDITIONS = 5;

class SpellItemEnchantmentConditionRec {
    public:
    static const int32_t s_numColumns = 31;
    static const int32_t s_rowSize = 64;
    static const bool s_generatedID = false;

    /* 0000 */ int32_t m_id;
    /* 0004 */ int8_t m_ltOperandType [NUM_ENCHANTMENT_CONDITIONS];
    /* 000C */ int32_t m_ltOperand [NUM_ENCHANTMENT_CONDITIONS];
    /* 0020 */ int8_t m_operator [NUM_ENCHANTMENT_CONDITIONS];
    /* 0025 */ int8_t m_rtOperandType [NUM_ENCHANTMENT_CONDITIONS];
    /* 002C */ int32_t m_rtOperand [NUM_ENCHANTMENT_CONDITIONS];
    /* 0040 */ int8_t m_logic [NUM_ENCHANTMENT_CONDITIONS];

    SpellItemEnchantmentConditionRec();
    ~SpellItemEnchantmentConditionRec();
    static const char* GetFilename();
    bool Read(SFileImpl* file, const char* stringBuffer);

    enum COMPARISON_OPERATOR : int32_t {
        OPERATOR_EQUAL = 0,
        OPERATOR_NOT_EQUAL = 1,
        OPERATOR_LESS_THAN = 2,
        OPERATOR_GREATER_THAN = 3,
        OPERATOR_LESS_OR_EQUAL = 4,
        OPERATOR_GREATER_OR_EQUAL = 5,
    };

};

#endif
