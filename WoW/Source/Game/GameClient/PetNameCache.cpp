#include "PetNameCache.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"

// INLINED
PetNameCache::PetNameCache() {
    this->m_petNumber = 0;
    this->m_name[0] = '\0';
    this->m_declinedNames = nullptr;
    this->m_timestamp = 0;
}

// INLINED
PetNameCache::~PetNameCache() {
    delete this->m_declinedNames;
}

// VERIFY: f23810ad auto
// OFFSET: 0x00668590
PetNameCache& PetNameCache::operator=(const PetNameCache& source) {
    this->m_petNumber = source.m_petNumber;
    SStrCopy(this->m_name, source.m_name, sizeof(this->m_name));

    if (source.m_declinedNames) {
        if (!this->m_declinedNames) {
            this->m_declinedNames = static_cast<PetNameCache::DeclinedNames*>(SMemAlloc(sizeof(PetNameCache::DeclinedNames), __FILE__, __LINE__, 0));
        }

        for (uint32_t i = 0; i < NUM_DECLENSIONS; i++) {
            SStrCopy(this->m_declinedNames->m_names[i], source.m_declinedNames->m_names[i], sizeof(this->m_declinedNames->m_names[i]));
        }
    }

    this->m_timestamp = source.m_timestamp;

    return *this;
}

// VERIFY: b3d5f8cd auto
// OFFSET: 0x007F7550
void PetNameCache::Pack(CDataStore* msg) {
    msg->Put(this->m_petNumber);
    msg->PutString(this->m_name);

    if (this->m_declinedNames) {
        msg->Put(static_cast<uint8_t>(1));

        for (uint32_t i = 0; i < NUM_DECLENSIONS; i++) {
            msg->PutString(this->m_declinedNames->m_names[i]);
        }
    } else {
        msg->Put(static_cast<uint8_t>(0));
    }

    msg->Put(this->m_timestamp);
}

// VERIFY: cb4c25ee manual
// OFFSET: 0x007F75D0
void PetNameCache::Unpack(CDataStore* msg) {
    uint8_t hasDeclinedNames = 0;
    msg->Get(this->m_petNumber);
    msg->GetString(this->m_name, sizeof(this->m_name));
    msg->Get(hasDeclinedNames);

    if (hasDeclinedNames) {
        if (!this->m_declinedNames) {
            this->m_declinedNames = static_cast<PetNameCache::DeclinedNames*>(SMemAlloc(sizeof(PetNameCache::DeclinedNames), __FILE__, __LINE__, 0));
        }

        for (uint32_t i = 0; i < NUM_DECLENSIONS; i++) {
            msg->GetString(this->m_declinedNames->m_names[i], sizeof(this->m_declinedNames->m_names[i]));
        }
    }

    uint32_t timestamp = 0;
    msg->Get(timestamp);
    this->m_timestamp = timestamp;
}
