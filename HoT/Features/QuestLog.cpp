#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <vector>
#include "Engine/Source/Os/TimeManager.h"
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/Player.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/PartyFrame.h"
#include "WoW/Source/Ui/QuestLog.h"
#include "WoW/Source/Ui/ScriptEvents.h"

namespace HoT::QuestLog {

static constexpr uint32_t PARTY_REQUEST_INTERVAL_MS = 3000;
static constexpr uint32_t SLOT_FIELD_BASE = 0x28;

static PlayerQuestLogEntry s_slots[NUM_SLOTS];
static std::unordered_map<uint64_t, std::vector<uint32_t>> s_partyQuests;
static std::unordered_map<uint64_t, uint32_t> s_partyRequestMs;

PlayerQuestLogEntry* GetSlot(uint32_t slot) {
    if (slot >= NUM_SLOTS) {
        return nullptr;
    }

    return &s_slots[slot];
}

void ClearPartyQuests() {
    s_partyQuests.clear();
    s_partyRequestMs.clear();
}

void Reset() {
    memset(s_slots, 0, sizeof(s_slots));
    ClearPartyQuests();
}

static bool IsOnQuest(uint32_t questId) {
    if (!questId) {
        return false;
    }

    for (const PlayerQuestLogEntry& slot : s_slots) {
        if (slot.m_questId == questId) {
            return true;
        }
    }

    return false;
}

static bool ShouldRequestParty(uint64_t guid) {
    uint32_t now = OsGetAsyncTimeMs();
    auto it = s_partyRequestMs.find(guid);

    if (it != s_partyRequestMs.end() && now - it->second < PARTY_REQUEST_INTERVAL_MS) {
        return false;
    }

    s_partyRequestMs[guid] = now;
    return true;
}

bool IsUnitOnQuest(uint64_t guid, int32_t questId) {
    if (!guid || questId <= 0) {
        return false;
    }

    if (guid == ClntObjMgrGetActivePlayer().m_guid) {
        return IsOnQuest(static_cast<uint32_t>(questId));
    }

    if (!CGPartyInfo::IsMember(WOWGUID{ guid })) {
        return false;
    }

    auto it = s_partyQuests.find(guid);

    if (it == s_partyQuests.end()) {
        if (ShouldRequestParty(guid)) {
            Net::Packet(Net::CMSG_QUEST_LOG_PARTY_REQUEST).PutUInt64(guid).Send();
        }

        return false;
    }

    return std::find(it->second.begin(), it->second.end(), static_cast<uint32_t>(questId)) != it->second.end();
}

static uint32_t ReadSlot(Net::Packet& packet, PlayerQuestLogEntry& slot) {
    uint32_t index = packet.GetUInt8();
    slot.m_questId = packet.GetUInt32();
    slot.m_state = packet.GetUInt32();
    uint32_t counters[2];
    counters[0] = packet.GetUInt32();
    counters[1] = packet.GetUInt32();
    memcpy(slot.m_counts, counters, sizeof(slot.m_counts));
    slot.m_time = packet.GetUInt32();
    return index;
}

// Runs the descriptor change handler so quest accept, auto complete and POI cleanup behave as they did for descriptor slots.
static void OnSlotReceived(uint32_t index, const PlayerQuestLogEntry& slot) {
    if (index >= NUM_SLOTS) {
        return;
    }

    PlayerQuestLogEntry old = s_slots[index];
    s_slots[index] = slot;

    WOWGUID guid = ClntObjMgrGetActivePlayer();

    if (!guid || !ClntObjMgrObjectPtr(guid, TYPE_PLAYER, __FILE__, __LINE__)) {
        return;
    }

    OnUpdateQuest(guid, SLOT_FIELD_BASE + index * static_cast<uint32_t>(sizeof(PlayerQuestLogEntry)), 0, &old, nullptr);
}

static int32_t Packet_SMSG_QUEST_LOG_FULL(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t count = packet.GetUInt8();
    memset(s_slots, 0, sizeof(s_slots));

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        PlayerQuestLogEntry slot;
        uint32_t index = ReadSlot(packet, slot);

        if (index < NUM_SLOTS) {
            s_slots[index] = slot;
        }
    }

    CGQuestLog::Update(1);
    return 1;
}

static int32_t Packet_SMSG_QUEST_LOG_SLOTS(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint32_t count = packet.GetUInt8();

    for (uint32_t i = 0; i < count && packet.CanRead(1); i++) {
        PlayerQuestLogEntry slot;
        uint32_t index = ReadSlot(packet, slot);
        OnSlotReceived(index, slot);
    }

    return 1;
}

static int32_t Packet_SMSG_QUEST_LOG_PARTY(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    uint64_t guid = packet.GetUInt64();
    uint32_t count = packet.GetUInt8();

    std::vector<uint32_t> questIds;
    questIds.reserve(count);

    for (uint32_t i = 0; i < count && packet.CanRead(4); i++) {
        questIds.push_back(packet.GetUInt32());
    }

    s_partyQuests[guid] = std::move(questIds);
    s_partyRequestMs.erase(guid);
    Script_SendUnitSignal(WOWGUID{ guid }, SCRIPT_EVENT_UNIT_QUEST_LOG_CHANGED);
    return 1;
}

class QuestLogModule : public Module {
    public:
    const char* Name() const override {
        return "QuestLog";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_QUEST_LOG_FULL, Packet_SMSG_QUEST_LOG_FULL);
        Net::RegisterWorldHandler(Net::SMSG_QUEST_LOG_SLOTS, Packet_SMSG_QUEST_LOG_SLOTS);
        Net::RegisterWorldHandler(Net::SMSG_QUEST_LOG_PARTY, Packet_SMSG_QUEST_LOG_PARTY);
    }
};

Module* GetModule() {
    static QuestLogModule module;
    return &module;
}

}
