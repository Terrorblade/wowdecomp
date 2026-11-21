#include "CGMinigameInfo.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "WoW/Source/Minigame/Minigame_c.h"
#include "WoW/Source/Ui/Types.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

// STATIC: 0x00C20FEC
CMinigame_C* CGMinigameInfo::m_minigame;

// VERIFY: 7481e654 auto
// OFFSET: 0x005C5720
void CGMinigameInfo::InitializeGame() {
    ClientServices::SetMessageHandler(SMSG_MINIGAME_SETUP, Packet_SMSG_MINIGAME_SETUP, nullptr);
    ClientServices::SetMessageHandler(SMSG_MINIGAME_STATE, Packet_SMSG_MINIGAME_STATE, nullptr);
}

// VERIFY: 8dc40695 auto
// OFFSET: 0x005C54B0
void CGMinigameInfo::ShutdownGame() {
    ClientServices::ClearMessageHandler(SMSG_MINIGAME_SETUP);
    ClientServices::ClearMessageHandler(SMSG_MINIGAME_STATE);
}

// VERIFY: f115ee6d auto
// OFFSET: 0x005C54D0
int32_t Packet_SMSG_MINIGAME_STATE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    if (CGMinigameInfo::m_minigame) {
        CGMinigameInfo::m_minigame->UpdateState(msg);
    }

    FrameScript_SignalEvent(SCRIPT_EVENT_MINIGAME_UPDATE, nullptr);
    return 1;
}

// VERIFY: b1ed2177 auto
// OFFSET: 0x005C5660
void CGMinigameInfo::LeaveWorld() {
    if (CGMinigameInfo::m_minigame) {
        delete CGMinigameInfo::m_minigame;
        CGMinigameInfo::m_minigame = nullptr;
    }
}

// VERIFY: 67e71a08 auto
// OFFSET: 0x005C5690
int32_t Packet_SMSG_MINIGAME_SETUP(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    uint64_t guid = 0;
    uint8_t type = 0;
    msg->Get(guid);
    msg->Get(type);

    if (msg->m_read > msg->m_size) {
        return 1;
    }

    if (CGMinigameInfo::m_minigame) {
        delete CGMinigameInfo::m_minigame;
        CGMinigameInfo::m_minigame = nullptr;
    }

    CGMinigameInfo::m_minigame = CMinigame_C::CreateMinigame(static_cast<MINIGAME_TYPE>(type), guid);

    if (!CGMinigameInfo::m_minigame) {
        return 1;
    }

    CGMinigameInfo::m_minigame->UpdateSetup(msg);
    FrameScript_SignalEvent(SCRIPT_EVENT_START_MINIGAME, nullptr);
    FrameScript_SignalEvent(SCRIPT_EVENT_MINIGAME_UPDATE, nullptr);
    return 1;
}
