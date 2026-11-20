#include "ItemName.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Object/ItemNameCache.h"

// INLINED
ItemNameCache_C::~ItemNameCache_C() {
    if (this->m_name) {
        SMemFree(this->m_name, __FILE__, __LINE__, 0);
    }
}

// VERIFY: ea8b2772 manual
// OFFSET: 0x0098D8C0
void ItemNameCache_C::Unpack(CDataStore* msg) {
    char name[400];
    msg->GetString(name, sizeof(name));

    if (!this->m_name || SStrCmp(this->m_name, name, STORM_MAX_STR)) {
        if (this->m_name) {
            SMemFree(this->m_name, __FILE__, __LINE__, 0);
        }

        this->m_name = SStrDupA(name, __FILE__, __LINE__);
    }
    msg->Get(this->m_inventoryType);
}

// VERIFY: 27b86eaa auto
// OFFSET: 0x0098D890
void ItemNameCache_C::Pack(CDataStore* msg) {
    msg->PutString(this->m_name);
    msg->Put(this->m_inventoryType);
}
