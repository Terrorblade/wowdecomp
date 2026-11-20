#include "WoW/Source/DB/WowClientDB.h"

// INLINED
WowClientDB_Base::WowClientDB_Base() {
    this->m_loaded = 0;
    this->m_numRecords = 0;
    this->m_maxID = -1;
    this->m_minID = 0xFFFFFFF;
    this->m_strings = nullptr;
}

// VERIFY: b5021150 manual
// OFFSET: 0x008C6780
WowClientDB_Base::~WowClientDB_Base() {
}
