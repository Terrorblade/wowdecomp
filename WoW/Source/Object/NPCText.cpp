#include "NPCText.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"

// VERIFY: e8ce9f4d manual
// OFFSET: 0x00668070
NPCText::~NPCText() {
    for (int32_t i = 0; i < NUM_NPC_TEXT_ENTRIES; i++) {
        if (this->m_text[i][0]) {
            SMemFree(this->m_text[i][0], __FILE__, __LINE__, 0);
        }

        if (this->m_text[i][1]) {
            SMemFree(this->m_text[i][1], __FILE__, __LINE__, 0);
        }
    }
}

// VERIFY: 4f03db70 manual
// OFFSET: 0x0098E1F0
void NPCText::Pack(CDataStore* msg) {
    for (uint32_t i = 0; i < NUM_NPC_TEXT_ENTRIES; i++) {
        msg->Put(this->m_probability[i]);

        if (this->m_text[i][0]) {
            msg->PutString(this->m_text[i][0]);
        } else {
            msg->PutString("");
        }

        if (this->m_text[i][1]) {
            msg->PutString(this->m_text[i][1]);
        } else {
            msg->PutString("");
        }

        msg->Put(this->m_language[i]);

        for (int32_t j = 0; j < NUM_NPC_TEXT_EMOTES; j++) {
            msg->Put(this->m_emoteDelay[i][j]);
            msg->Put(this->m_emote[i][j]);
        }
    }
}

// VERIFY: 85422c48 manual
// OFFSET: 0x0098E2C0
void NPCText::Unpack(CDataStore* msg) {
    char buffer[3000];

    for (int32_t i = 0; i < NUM_NPC_TEXT_ENTRIES; i++) {
        msg->Get(this->m_probability[i]);

        msg->GetString(buffer, sizeof(buffer));

        if (this->m_text[i][0]) {
            SMemFree(this->m_text[i][0], __FILE__, __LINE__, 0);
        }

        this->m_text[i][0] = buffer[0] ? SStrDupA(buffer, __FILE__, __LINE__) : nullptr;

        msg->GetString(buffer, sizeof(buffer));

        if (this->m_text[i][1]) {
            SMemFree(this->m_text[i][1], __FILE__, __LINE__, 0);
        }

        this->m_text[i][1] = buffer[0] ? SStrDupA(buffer, __FILE__, __LINE__) : nullptr;

        msg->Get(this->m_language[i]);

        for (int32_t j = 0; j < NUM_NPC_TEXT_EMOTES; j++) {
            msg->Get(this->m_emoteDelay[i][j]);
            msg->Get(this->m_emote[i][j]);
        }
    }
}
