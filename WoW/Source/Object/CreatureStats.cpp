#include "CreatureStats.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Object/ObjectClient/UnitCombatLog_C.h"

// VERIFY: 5eb2b3d8 auto
// OFFSET: 0x0053B8E0
const char* CreatureStats::GetName(int32_t index) const {
    const char* name = this->m_name[index];

    if (!name || !*name) {
        return this->m_name[0];
    }

    return name;
}

// VERIFY: 8a1df267 auto
// OFFSET: 0x0098D3A0
void CreatureStats::Pack(CDataStore* msg) const {
    for (uint32_t i = 0; i < 4; i++) {
        msg->PutString(this->m_name[i] ? this->m_name[i] : "");
    }

    msg->PutString(this->m_subName);
    msg->PutString(this->m_iconName);
    msg->Put(this->m_typeFlags);
    msg->Put(this->m_type);
    msg->Put(this->m_family);
    msg->Put(this->m_rank);
    msg->Put(this->m_killCredit[0]);
    msg->Put(this->m_killCredit[1]);
    msg->Put(this->m_displayId[0]);
    msg->Put(this->m_displayId[1]);
    msg->Put(this->m_displayId[2]);
    msg->Put(this->m_displayId[3]);
    msg->Put(this->m_hpModifier);
    msg->Put(this->m_mpModifier);
    msg->Put(static_cast<uint8_t>(this->m_racialLeader != 0));

    for (uint32_t i = 0; i < 6; i++) {
        msg->Put(this->m_questItem[i]);
    }

    msg->Put(this->m_movementId);
}

// VERIFY: c7554279 manual
// OFFSET: 0x0098D4C0
void CreatureStats::Unpack(CDataStore* msg) {
    char buffer[1024];

    for (uint32_t i = 0; i < 4; i++) {
        msg->GetString(buffer, sizeof(buffer));

        if (this->m_name[i] && !SStrCmp(this->m_name[i], buffer, STORM_MAX_STR)) {
            continue;
        }

        if (this->m_name[i]) {
            UnitCombatLogInvalidateName(this->m_name[i]);
            SMemFree(this->m_name[i], __FILE__, __LINE__, 0);
        }

        if (buffer[0]) {
            this->m_name[i] = SStrDupA(buffer, __FILE__, __LINE__);
        } else {
            this->m_name[i] = nullptr;
        }
    }

    if (this->m_subName) {
        SMemFree(this->m_subName, __FILE__, __LINE__, 0);
    }

    msg->GetString(buffer, sizeof(buffer));
    this->m_subName = SStrDupA(buffer, __FILE__, __LINE__);

    if (this->m_iconName) {
        SMemFree(this->m_iconName, __FILE__, __LINE__, 0);
    }

    msg->GetString(buffer, sizeof(buffer));
    this->m_iconName = SStrDupA(buffer, __FILE__, __LINE__);

    msg->Get(this->m_typeFlags);
    msg->Get(this->m_type);
    msg->Get(this->m_family);
    msg->Get(this->m_rank);
    msg->Get(this->m_killCredit[0]);
    msg->Get(this->m_killCredit[1]);
    msg->Get(this->m_displayId[0]);
    msg->Get(this->m_displayId[1]);
    msg->Get(this->m_displayId[2]);
    msg->Get(this->m_displayId[3]);
    msg->Get(this->m_hpModifier);
    msg->Get(this->m_mpModifier);

    uint8_t racialLeader;
    msg->Get(racialLeader);
    this->m_racialLeader = racialLeader != 0;

    for (uint32_t i = 0; i < 6; i++) {
        msg->Get(this->m_questItem[i]);
    }

    msg->Get(this->m_movementId);
}
