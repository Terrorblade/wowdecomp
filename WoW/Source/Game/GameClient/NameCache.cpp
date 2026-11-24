#include "NameCache.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"

// INLINED
NameCache::NameCache() {
    this->m_name[0] = '\0';
    this->m_declinedNames = nullptr;
    this->m_realmName[0] = '\0';
    this->m_guid = WOWGUID();
}

// INLINED
NameCache::~NameCache() {
    delete this->m_declinedNames;
}

// VERIFY: 0b199304 auto
// OFFSET: 0x007F7400
void NameCache::Pack(CDataStore* msg) {
    msg->Put(this->m_guid.m_guid);
    msg->PutString(this->m_name);

    if (this->m_declinedNames) {
        msg->Put(static_cast<uint8_t>(1));

        for (uint32_t i = 0; i < NUM_DECLENSIONS; i++) {
            msg->PutString(this->m_declinedNames->m_names[i]);
        }
    } else {
        msg->Put(static_cast<uint8_t>(0));
    }

    msg->PutString(this->m_realmName);
    msg->Put(this->m_race);
    msg->Put(this->m_sex);
    msg->Put(this->m_class);
}

// VERIFY: c6bdf007 manual
// OFFSET: 0x006680C0
NameCache& NameCache::Copy(const NameCache& other) {
    this->m_guid = other.m_guid;
    SStrCopy(this->m_name, other.m_name, sizeof(this->m_name));

    if (other.m_declinedNames) {
        if (!this->m_declinedNames) {
            this->m_declinedNames = static_cast<NameCache::DeclinedNames*>(SMemAlloc(sizeof(NameCache::DeclinedNames), __FILE__, __LINE__, 0));
        }

        for (uint32_t i = 0; i < sizeof(this->m_declinedNames->m_names) / sizeof(this->m_declinedNames->m_names[0]); i++) {
            SStrCopy(this->m_declinedNames->m_names[i], other.m_declinedNames->m_names[i], sizeof(this->m_declinedNames->m_names[i]));
        }
    }

    SStrCopy(this->m_realmName, other.m_realmName, sizeof(this->m_realmName));
    this->m_race = other.m_race;
    this->m_sex = other.m_sex;
    this->m_class = other.m_class;
    return *this;
}

// VERIFY: d7e9cfe1 manual
// OFFSET: 0x007F74A0
void NameCache::Unpack(CDataStore* msg) {
    uint8_t hasDeclinedNames = 0;
    msg->Get(this->m_guid.m_guid);
    msg->GetString(this->m_name, sizeof(this->m_name));
    msg->Get(hasDeclinedNames);

    if (hasDeclinedNames) {
        if (!this->m_declinedNames) {
            this->m_declinedNames = static_cast<NameCache::DeclinedNames*>(SMemAlloc(sizeof(NameCache::DeclinedNames), __FILE__, __LINE__, 0));
        }

        for (uint32_t i = 0; i < NUM_DECLENSIONS; i++) {
            msg->GetString(this->m_declinedNames->m_names[i], sizeof(this->m_declinedNames->m_names[i]));
        }
    }

    msg->GetString(this->m_realmName, sizeof(this->m_realmName));
    msg->Get(this->m_race);
    msg->Get(this->m_sex);
    msg->Get(this->m_class);
}
