#include "HoT/Core/CVars.h"
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"

namespace HoT::LootWindow {

static const float MAX_LOOT_RANGE = 45.0f;

static bool s_staysOpenWhileMoving = true;
static WOWGUID s_pushedGuid;

bool StaysOpenWhileMoving() {
    return s_staysOpenWhileMoving;
}

static void OnStaysOpenChanged(const char* value) {
    s_staysOpenWhileMoving = CVars::Bool(value);
}

// The server opens loot on its own, so the client has to accept the response for that target.
static int32_t Packet_SMSG_PRIME_LOOT_TARGET(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    WOWGUID guid = { Net::Packet(msg).GetUInt64() };
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (player) {
        player->m_lootTarget = guid;
        player->m_autoLoot = static_cast<uint8_t>(CGGameUI::IsAutoLooting());
        s_pushedGuid = guid;
    }

    return 1;
}

static void CloseLootOutOfRange() {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    WOWGUID lootGuid = player ? player->m_lootTarget : WOWGUID();

    if (!lootGuid) {
        s_pushedGuid = WOWGUID();
        return;
    }

    if (lootGuid == s_pushedGuid) {
        return;
    }

    CGObject_C* target = ClntObjMgrObjectPtr(lootGuid, static_cast<OBJECT_TYPE>(TYPE_UNIT | TYPE_GAMEOBJECT | TYPE_CORPSE), __FILE__, __LINE__);

    if (!target) {
        return;
    }

    C3Vector delta = player->GetPosition() - target->GetPosition();

    if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z > MAX_LOOT_RANGE * MAX_LOOT_RANGE) {
        CGGameUI::CloseLoot(true, true, false);
    }
}

class LootWindowModule : public Module {
    public:
    const char* Name() const override {
        return "LootWindow";
    }

    void OnInitializeGlobal() override {
        CVars::Register("lootStayOpenWhileMoving", "Keep the loot window open while moving.", "1", OnStaysOpenChanged);
        Net::RegisterWorldHandler(Net::SMSG_PRIME_LOOT_TARGET, Packet_SMSG_PRIME_LOOT_TARGET);
    }

    void OnIdle() override {
        CloseLootOutOfRange();
    }
};

Module* GetModule() {
    static LootWindowModule module;
    return &module;
}

}
