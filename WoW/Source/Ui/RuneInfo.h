#ifndef WOW_SOURCE_UI_RUNEINFO_H
#define WOW_SOURCE_UI_RUNEINFO_H

#include <cstdint>
constexpr int32_t NUM_INITIAL_RUNES = 6;
constexpr int32_t RUNE_BASE_COOLDOWN_MS = 10000;
constexpr float RUNE_BASE_COOLDOWN_MS_FLOAT = 10000.0f;

class CDataStore;
class SpellRec;

class CGRuneInfo {
    public:

    enum RUNE_TYPE : int32_t {
        RUNE_TYPE_BLOOD = 0,
        RUNE_TYPE_UNHOLY = 1,
        RUNE_TYPE_FROST = 2,
        RUNE_TYPE_DEATH = 3,
        NUM_RUNE_TYPES = 4,
    };

    static void LeaveWorld();
    static int32_t m_runes[8];
    static void ConvertRune(uint8_t rune, uint8_t type);
    static void ResyncRunes(CDataStore* msg);
    static void AddRunePower(CDataStore* msg);
    static int32_t m_baseRunes[8];
    static uint32_t m_runeCooldowns[8];
    static uint32_t m_runeLastUsed[8];
    static uint32_t m_numRunes;
    static uint32_t m_predictedRunes;
    static void SetPredictedRunes(uint8_t available, uint8_t used, const uint8_t* cooldowns);
    static void InitializeGame();
    static void ShutdownGame();
    static bool HasRunesForSpell(int32_t spellId, int32_t* needed);
    static uint32_t GetRuneRechargeTime(int32_t index);
    static int32_t CountPredictedRunesOfType(int32_t type);
    static bool ComputeRuneSpellCooldown(const SpellRec* spell, uint32_t* cooldown);
    static bool IsRuneIndexValid(int32_t index);
    static int32_t GetRuneTypeByIndex(int32_t index, bool base);
    static uint32_t GetRuneLastUsedTime(int32_t index);
    static void SetRuneLastUsedTime(int32_t index, uint32_t time);
    static void ComputeRuneRechargeTime(int32_t index, float progress);
};

#endif
