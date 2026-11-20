#include "PageTextCache.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Storm/H/storm.h"

// VERIFY: ead7370d manual
// OFFSET: 0x00668550
PageTextCache::~PageTextCache() {
    if (this->m_text) {
        SMemFree(this->m_text, __FILE__, __LINE__, 0);
    }
}

// VERIFY: f2d3b58d manual
// OFFSET: 0x0098E490
void PageTextCache::Pack(CDataStore* msg) {
    if (this->m_text) {
        msg->PutString(this->m_text);
    } else {
        msg->PutString("");
    }

    msg->Put(this->m_nextPage);
}

// VERIFY: 8724ce7c manual
// OFFSET: 0x0098E4D0
void PageTextCache_C::Unpack(CDataStore* msg) {
    char buffer[4000];

    msg->GetString(buffer, sizeof(buffer));

    if (!this->m_text || SStrCmp(this->m_text, buffer, STORM_MAX_STR)) {
        if (this->m_text) {
            SMemFree(this->m_text, __FILE__, __LINE__, 0);
        }

        this->m_text = SStrDupA(buffer, __FILE__, __LINE__);
    }
    msg->Get(this->m_nextPage);
}
