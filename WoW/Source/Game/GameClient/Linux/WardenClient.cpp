#if defined(WOWDECOMP_PLATFORM_LINUX)

#include "WoW/Source/Game/GameClient/WardenClient.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/Storm/H/storm.h"
#include "Engine/Source/Storm/Source/SLock.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

static uint32_t s_lastProcessTime;

static SCritSect s_wardenCritSect;

// The 64-bit Windows client already never loads Warden modules: SMSG_WARDEN_DATA is consumed and dropped, and
// WardenClient_Initialize reports that no module is running. Linux keeps exactly that behaviour (no PE module could
// run here anyway); the module cache structures below are kept for the code that stores them.
static int32_t ReceiveWardenData(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    if (msgId != SMSG_WARDEN_DATA) {
        return 0;
    }

    s_wardenCritSect.Enter();

    void* data;
    msg->GetDataInSitu(data, msg->m_size - msg->m_read);

    s_wardenCritSect.Leave();

    return 1;
}

void WardenClient_Process() {
    s_wardenCritSect.Enter();
    s_lastProcessTime = OsGetAsyncTimeMs();
    s_wardenCritSect.Leave();
}

void WardenClient_Destroy() {
    s_wardenCritSect.Enter();
    ClientServices::ClearMessageHandler(SMSG_WARDEN_DATA);
    s_wardenCritSect.Leave();
}

bool WardenClient_Initialize() {
    s_wardenCritSect.Enter();
    WardenClient_Destroy();
    s_lastProcessTime = OsGetAsyncTimeMs();
    ClientServices::SetMessageHandler(SMSG_WARDEN_DATA, ReceiveWardenData, nullptr);
    s_wardenCritSect.Leave();

    return false;
}

CDataStore& operator<<(CDataStore& msg, const CWardenKey& key) {
    for (uint32_t i = 0; i < sizeof(key.m_key); i++) {
        msg.Put(key.m_key[i]);
    }

    return msg;
}

CDataStore& operator>>(CDataStore& msg, CWardenKey& key) {
    for (uint32_t i = 0; i < sizeof(key.m_key); i++) {
        msg.Get(key.m_key[i]);
    }

    return msg;
}

WardenCachedModule::WardenCachedModule() {
    this->m_size = 0;
    this->m_data = nullptr;
}

WardenCachedModule::~WardenCachedModule() {
    if (this->m_data) {
        SMemFree(this->m_data, __FILE__, __LINE__, 0);
    }
}

void WardenCachedModule::Pack(CDataStore* msg) {
    msg->Put(this->m_data ? this->m_size : 0);

    for (uint32_t i = 0; i < this->m_size; i++) {
        msg->Put(this->m_data[i]);
    }
}

void WardenCachedModule::Unpack(CDataStore* msg) {
    msg->Get(this->m_size);

    if (!this->m_size) {
        this->m_data = nullptr;
        return;
    }

    this->m_data = static_cast<uint8_t*>(SMemAlloc(this->m_size, __FILE__, __LINE__, 0));

    for (uint32_t i = 0; i < this->m_size; i++) {
        msg->Get(this->m_data[i]);
    }
}

#endif
