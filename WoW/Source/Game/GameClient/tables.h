#ifndef WOW_SOURCE_GAME_GAMECLIENT_TABLES_H
#define WOW_SOURCE_GAME_GAMECLIENT_TABLES_H

#include <cstdint>
#include "WoW/Source/DB/WowClientDB.h"

enum GAME_TABLE : int32_t {
    GAME_TABLE_BARBER_SHOP_COST_BASE = 0,
    GAME_TABLE_COMBAT_RATINGS = 1,
    GAME_TABLE_CHANCE_TO_MELEE_CRIT = 2,
    GAME_TABLE_CHANCE_TO_MELEE_CRIT_BASE = 3,
    GAME_TABLE_CHANCE_TO_SPELL_CRIT = 4,
    GAME_TABLE_CHANCE_TO_SPELL_CRIT_BASE = 5,
    GAME_TABLE_NPC_MANA_COST_SCALER = 6,
    GAME_TABLE_OCT_REGEN_HP = 7,
    GAME_TABLE_OCT_REGEN_MP = 8,
    GAME_TABLE_REGEN_HP_PER_SPIRIT = 9,
    GAME_TABLE_REGEN_MP_PER_SPIRIT = 10,
    GAME_TABLE_OCT_CLASS_COMBAT_RATING_SCALAR = 11,
    NUM_GAME_TABLES = 12,
};

class GameTablesRec;

class ClientGameTableBase {
    public:
    /* 0004 */ const char* m_name;
    /* 0008 */ GameTablesRec* m_gameTable;

    virtual float EvaluateTable(uint32_t column, uint32_t row) = 0;

    ClientGameTableBase(const char* name);
    void Initialize();
};

template <class T>
class ClientGameTable : public ClientGameTableBase {
    public:
    /* 000C */ WowClientDB<T>* m_db;

    float EvaluateTable(uint32_t column, uint32_t row) override;

    ClientGameTable(const char* name, WowClientDB<T>* db);
};

// INLINED
template <class T>
ClientGameTable<T>::ClientGameTable(const char* name, WowClientDB<T>* db) : ClientGameTableBase(name) {
    this->m_db = db;
}

// VERIFY: 2e38a015 auto
// OFFSET: [0x007F69B0, 0x007F69E0]
template <class T>
float ClientGameTable<T>::EvaluateTable(uint32_t column, uint32_t row) {
    return this->m_db->GetRecord(column + row * this->m_gameTable->m_numRows)->m_data;
}

#endif
