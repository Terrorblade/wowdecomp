#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>
#include "HoT/Character/PlayerRenderLimit.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Chat.h"
#include "HoT/Core/Hotkeys.h"
#include "HoT/Core/Log.h"
#include "HoT/Core/Modules.h"
#include "HoT/Profiler/Profiler.h"
#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Os/TimeManager.h"
#include "WoW/Common/ComponentCore/CharacterComponent.h"
#include "WoW/Source/Glue/ComponentData.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/PartyFrame.h"
#include "WoW/Source/Ui/RaidInfo.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/FriendList.h"

namespace HoT::PlayerRenderLimit {

static const uint32_t EVALUATE_INTERVAL_MS = 250;
static const float KEEP_VISIBLE_BONUS_YARDS = 5.0f;
static const uint32_t RELEASE_DELAY_MS = 3000;
static const size_t RELEASES_PER_TICK = 8;
static const int32_t MIN_LIMIT = 20;
static const int32_t MAX_LIMIT = 100;
static const uint32_t MAX_RAID_MEMBERS = 40;
static const uint32_t MAX_FRIENDS = 100;
static const uint32_t BASE_SKIN_TEXTURE = 1;

static const uint32_t MODEL_DRAW_ROOT = 0x8;
static const uint32_t MODEL_DRAW_CHILD = 0x80;
static const uint32_t MODEL_EVENTS_ROOT = 0x10000;
static const uint32_t MODEL_EVENTS_CHILD = 0x20000;

// Lower sorts first and stays visible longest.
enum Relation : uint8_t {
    RELATION_TARGET,
    RELATION_PARTY,
    RELATION_RAID,
    RELATION_FRIEND,
    RELATION_GUILD,
    RELATION_STRANGER,
};

struct Candidate {
    uint64_t m_guid;
    Relation m_relation;
    float m_distance;
};

struct HiddenState {
    uint32_t m_hiddenSinceMs = 0;
    bool m_released = false;
};

struct Scan {
    CGPlayer_C* m_self = nullptr;
    uint64_t m_selfGuid = 0;
    uint64_t m_targetGuid = 0;
    uint32_t m_guildId = 0;
};

static int32_t s_limit = 0;
static bool s_unlimited = false;
static uint32_t s_nextEvaluateMs = 0;
static std::unordered_map<uint64_t, Relation> s_relations;
static std::unordered_map<uint64_t, HiddenState> s_hidden;
static std::unordered_map<uint64_t, HiddenState> s_nextHidden;
static std::vector<Candidate> s_candidates;
static Scan s_scan;

static void SetLimit(int32_t maxPlayers) {
    s_limit = maxPlayers <= 0 ? MAX_LIMIT : std::clamp(maxPlayers, MIN_LIMIT, MAX_LIMIT);
    s_nextEvaluateMs = 0;
}

static void OnLimitChanged(const char* value) {
    SetLimit(CVars::Int(value));
}

static void Reset() {
    s_hidden.clear();
    s_nextHidden.clear();
    s_relations.clear();
    s_candidates.clear();
    s_nextEvaluateMs = 0;
}

bool IsHidden(const CGObject_C* object) {
    if (s_hidden.empty() || !object || !(object->m_obj->m_type & TYPE_PLAYER)) {
        return false;
    }

    return s_hidden.find(object->m_obj->m_guid.m_guid) != s_hidden.end();
}

bool SuppressUpdate(CGObject_C* object) {
    if (!IsHidden(object)) {
        return false;
    }

    CM2Model* model = object->GetObjectModel();

    if (!model) {
        return true;
    }

    if (model->m_attachParent) {
        model->m_fFlags &= ~(MODEL_DRAW_CHILD | MODEL_EVENTS_CHILD);
        return true;
    }

    model->m_fFlags &= ~(MODEL_DRAW_ROOT | MODEL_EVENTS_ROOT);
    model->SetAnimating(0);

    return true;
}

void OnShouldRender(CGObject_C* object, int32_t& hidden) {
    if (!IsHidden(object)) {
        return;
    }

    hidden = 1;

    CM2Model* model = object->GetObjectModel();

    if (model && !model->m_attachParent) {
        model->SetAnimating(0);
    }
}

static void CollectRelations() {
    s_relations.clear();

    for (const WOWGUID& guid : CGPartyInfo::m_members) {
        if (guid) {
            s_relations.emplace(guid.m_guid, RELATION_PARTY);
        }
    }

    uint32_t raidCount = std::min(CGRaidInfo::m_numMembers, MAX_RAID_MEMBERS);

    for (uint32_t i = 0; i < raidCount; i++) {
        CGRaidMember* member = CGRaidInfo::m_members[i];

        if (member && member->m_guid) {
            s_relations.emplace(member->m_guid.m_guid, RELATION_RAID);
        }
    }

    if (FriendList* friends = FriendList::s_friendList) {
        for (uint32_t i = 0; i < MAX_FRIENDS; i++) {
            WOWGUID guid = friends->m_friends[i].m_guid;

            if (!guid) {
                break;
            }

            s_relations.emplace(guid.m_guid, RELATION_FRIEND);
        }
    }
}

static int32_t CollectCandidate(WOWGUID guid, void* param) {
    if (guid.m_guid == s_scan.m_selfGuid) {
        return 1;
    }

    CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(guid, TYPE_PLAYER, __FILE__, __LINE__));

    if (!player) {
        return 1;
    }

    Relation relation = RELATION_STRANGER;
    auto it = s_relations.find(guid.m_guid);

    if (guid.m_guid == s_scan.m_targetGuid) {
        relation = RELATION_TARGET;
    } else if (it != s_relations.end()) {
        relation = it->second;
    } else if (s_scan.m_guildId && player->m_player->m_playerGuildid == s_scan.m_guildId) {
        relation = RELATION_GUILD;
    }

    C3Vector delta = player->GetPosition() - s_scan.m_self->GetPosition();
    float distance = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

    // Players already shown get a head start so two at the edge do not swap every tick.
    if (s_hidden.find(guid.m_guid) == s_hidden.end()) {
        distance -= KEEP_VISIBLE_BONUS_YARDS;
    }

    s_candidates.push_back({ guid.m_guid, relation, distance });

    return 1;
}

static bool ReleaseUnit(CGUnit_C* unit) {
    CCharacterComponent* component = unit->m_characterComponent;

    if (!component) {
        return false;
    }

    CM2Model* model = component->m_data.m_model;

    if (model && (model->m_fFlags & 1)) {
        model->ReplaceTexture(BASE_SKIN_TEXTURE, nullptr);
    }

    unit->FreeComponent();

    return true;
}

// Hidden players lose their composited skin after a while so a crowd does not hold on to texture memory.
static void ReleaseStale(uint32_t now) {
    size_t released = 0;
    size_t freed = 0;

    for (auto& entry : s_hidden) {
        HiddenState& state = entry.second;

        if (state.m_released) {
            released++;
            continue;
        }

        if (freed >= RELEASES_PER_TICK || static_cast<int32_t>(now - state.m_hiddenSinceMs) < static_cast<int32_t>(RELEASE_DELAY_MS)) {
            continue;
        }

        CGPlayer_C* player = static_cast<CGPlayer_C*>(ClntObjMgrObjectPtr(WOWGUID{ entry.first }, TYPE_PLAYER, __FILE__, __LINE__));

        if (!player || !ReleaseUnit(player)) {
            continue;
        }

        state.m_released = true;
        released++;
        freed++;
    }

    if (freed) {
        LOG_DEBUG << "player render limit: released " << freed << " player sheets, " << released << " of " << s_hidden.size() << " hidden now released";
    }
}

static void Evaluate(uint32_t now) {
    if (!ClntObjMgrGetCurrent()) {
        return;
    }

    CGPlayer_C* self = ClntObjMgrGetActivePlayerObj();
    WOWGUID selfGuid = ClntObjMgrGetActivePlayer();

    if (!self || !selfGuid) {
        s_hidden.clear();
        return;
    }

    s_scan.m_self = self;
    s_scan.m_selfGuid = selfGuid.m_guid;
    s_scan.m_targetGuid = self->m_unit->m_unitFieldTarget.m_guid;
    s_scan.m_guildId = self->m_player->m_playerGuildid;

    CollectRelations();
    s_candidates.clear();
    ClntObjMgrEnumVisibleObjects(CollectCandidate, nullptr);

    s_nextHidden.clear();

    if (!s_unlimited && s_candidates.size() > static_cast<size_t>(s_limit)) {
        std::sort(s_candidates.begin(), s_candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.m_relation != b.m_relation) {
                return a.m_relation < b.m_relation;
            }

            return a.m_distance < b.m_distance;
        });

        for (size_t i = static_cast<size_t>(s_limit); i < s_candidates.size(); i++) {
            uint64_t guid = s_candidates[i].m_guid;
            HiddenState state;
            auto it = s_hidden.find(guid);

            if (it != s_hidden.end()) {
                state = it->second;
            } else {
                state.m_hiddenSinceMs = now;
            }

            s_nextHidden[guid] = state;
        }
    }

    if (s_nextHidden.size() != s_hidden.size()) {
        LOG_DEBUG << "player render limit: hiding " << s_nextHidden.size() << " of " << s_candidates.size() << " players, limit " << s_limit;
    }

    std::swap(s_hidden, s_nextHidden);
    ReleaseStale(now);
}

// F10 lifts the limit for this session, hidden players come back on their next update.
static void ToggleUnlimited() {
    s_unlimited = !s_unlimited;
    s_hidden.clear();
    s_nextHidden.clear();
    s_nextEvaluateMs = 0;

    if (s_unlimited) {
        Chat::Print("Player render limit off, every player is drawn.");
    } else {
        Chat::Print("Player render limit on, at most %d other players are drawn.", s_limit);
    }
}

static void Tick() {
    if (!s_limit) {
        return;
    }

    Profiler::Count(Profiler::Counter::PlayersHidden, static_cast<uint32_t>(s_hidden.size()));
    Profiler::Count(Profiler::Counter::PlayersVisible, static_cast<uint32_t>(s_candidates.size() - (std::min)(s_candidates.size(), s_hidden.size())));

    uint32_t now = OsGetAsyncTimeMs();

    if (s_nextEvaluateMs && static_cast<int32_t>(now - s_nextEvaluateMs) < 0) {
        return;
    }

    s_nextEvaluateMs = now + EVALUATE_INTERVAL_MS;
    Evaluate(now);
}

class PlayerRenderLimitModule : public Module {
    public:
    const char* Name() const override {
        return "PlayerRenderLimit";
    }

    void OnInitializeGlobal() override {
        CVars::Register("playerRenderLimit", "Most other players drawn at once. Party, raid, friends and guild members are kept first, then the nearest. Clamped to 20 through 100.", "100", OnLimitChanged);
        Hotkeys::Register("playerRenderLimitToggleKey", "Key that turns the player render limit off and back on for this session, NONE turns it off", "F10", ToggleUnlimited);
    }

    void OnEnterWorld() override {
        Reset();
    }

    void OnIdle() override {
        Tick();
    }
};

Module* GetModule() {
    static PlayerRenderLimitModule module;
    return &module;
}

}
