#include "CGPetition.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"

// VERIFY: f4e45cbe manual
// OFFSET: 0x00599FD0
CGPetition::CGPetition() {
    this->m_id = 0;
    this->m_ownerGuid = WOWGUID();
    this->m_title[0] = '\0';
    this->m_bodyText[0] = '\0';
    this->m_minSignatures = 0;
    this->m_maxSignatures = 0;
    this->m_deadline = 0;
    this->m_issueDate = 0;
    this->m_allowedGuildId = 0;
    this->m_allowedClasses = 0;
    this->m_allowedRaces = 0;
    this->m_allowedGenders = 0;
    this->m_allowedMaxLevel = 0;
    this->m_allowedMinLevel = 0;

    for (uint32_t i = 0; i < 10; i++) {
        this->m_choiceText[i][0] = '\0';
    }

    this->m_numChoices = 0;
    this->m_muid = 0;
    this->m_staticType = STATIC_TYPE_GUILD;
}

// VERIFY: ac373b5d auto
// OFFSET: 0x00668600
CGPetition& CGPetition::operator=(const CGPetition& source) {
    this->m_id = source.m_id;
    this->m_ownerGuid = source.m_ownerGuid;
    SStrCopy(this->m_title, source.m_title, sizeof(this->m_title));
    SStrCopy(this->m_bodyText, source.m_bodyText, sizeof(this->m_bodyText));
    this->m_minSignatures = source.m_minSignatures;
    this->m_maxSignatures = source.m_maxSignatures;
    this->m_deadline = source.m_deadline;
    this->m_issueDate = source.m_issueDate;
    this->m_allowedGuildId = source.m_allowedGuildId;
    this->m_allowedClasses = source.m_allowedClasses;
    this->m_allowedRaces = source.m_allowedRaces;
    this->m_allowedGenders = source.m_allowedGenders;
    this->m_allowedMaxLevel = source.m_allowedMaxLevel;
    this->m_allowedMinLevel = source.m_allowedMinLevel;
    this->m_muid = source.m_muid;
    this->m_staticType = source.m_staticType;

    for (uint32_t i = 0; i < 10; i++) {
        SStrCopy(this->m_choiceText[i], source.m_choiceText[i], sizeof(this->m_choiceText[i]));
    }

    this->m_numChoices = source.m_numChoices;

    return *this;
}

// VERIFY: a4660661 auto
// OFFSET: 0x0098CF70
void CGPetition::Pack(CDataStore* msg) {
    msg->Put(static_cast<uint32_t>(this->m_id));
    msg->Put(this->m_ownerGuid.m_guid);
    msg->PutString(this->m_title);
    msg->PutString(this->m_bodyText);
    msg->Put(this->m_minSignatures);
    msg->Put(this->m_maxSignatures);
    msg->Put(this->m_deadline);
    msg->Put(this->m_issueDate);
    msg->Put(this->m_allowedGuildId);
    msg->Put(this->m_allowedClasses);
    msg->Put(this->m_allowedRaces);
    msg->Put(this->m_allowedGenders);
    msg->Put(this->m_allowedMinLevel);
    msg->Put(this->m_allowedMaxLevel);
    msg->Put(this->m_numChoices);

    for (uint32_t i = 0; i < 10; i++) {
        msg->PutString(this->m_choiceText[i]);
    }

    msg->Put(this->m_muid);
    msg->Put(this->m_staticType);
}

// VERIFY: 114de37f auto
// OFFSET: 0x0098D090
void CGPetition::Unpack(CDataStore* msg) {
    msg->Get(reinterpret_cast<uint32_t&>(this->m_id));
    msg->Get(this->m_ownerGuid.m_guid);
    msg->GetString(this->m_title, sizeof(this->m_title));
    msg->GetString(this->m_bodyText, sizeof(this->m_bodyText));
    msg->Get(this->m_minSignatures);
    msg->Get(this->m_maxSignatures);
    msg->Get(this->m_deadline);
    msg->Get(this->m_issueDate);
    msg->Get(this->m_allowedGuildId);
    msg->Get(this->m_allowedClasses);
    msg->Get(this->m_allowedRaces);
    msg->Get(this->m_allowedGenders);
    msg->Get(this->m_allowedMinLevel);
    msg->Get(this->m_allowedMaxLevel);
    msg->Get(this->m_numChoices);

    for (uint32_t i = 0; i < 10; i++) {
        msg->GetString(this->m_choiceText[i], sizeof(this->m_choiceText[i]));
    }

    msg->Get(this->m_muid);
    msg->Get(this->m_staticType);
}
