#include "ArenaTeamCache.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"

// VERIFY: ca1384a9 auto
// OFFSET: 0x0098D1B0
void ArenaTeamCache::Pack(CDataStore* msg) {
    msg->Put(static_cast<uint32_t>(this->m_id));
    msg->PutString(this->m_name);
    msg->Put(this->m_type);
    msg->Put(this->m_backgroundColor);
    msg->Put(this->m_emblemStyle);
    msg->Put(this->m_emblemColor);
    msg->Put(this->m_borderStyle);
    msg->Put(this->m_borderColor);
}

// VERIFY: 075319b6 auto
// OFFSET: 0x0098D220
void ArenaTeamCache::Unpack(CDataStore* msg) {
    msg->Get(reinterpret_cast<uint32_t&>(this->m_id));
    msg->GetString(this->m_name, sizeof(this->m_name));
    msg->Get(this->m_type);
    msg->Get(this->m_backgroundColor);
    msg->Get(this->m_emblemStyle);
    msg->Get(this->m_emblemColor);
    msg->Get(this->m_borderStyle);
    msg->Get(this->m_borderColor);
}
