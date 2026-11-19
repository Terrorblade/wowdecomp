#include <unordered_map>
#include "Engine/Source/Os/TimeManager.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "HoT/Core/Modules.h"
#include "HoT/Features/Features.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"

namespace HoT::CooldownRate {

enum Field : uint8_t {
    FIELD_SPELL,
    FIELD_CATEGORY,
    FIELD_GCD,
};

struct Tracked {
    int32_t m_spellId;
    int32_t m_itemId;
    Field m_field;
};

static constexpr float MIN_RATE = 0.02f;
static constexpr float MAX_RATE = 10.0f;
static constexpr uint32_t MAX_AFFECTED_MS = 60 * 60 * 1000;
static constexpr int32_t MATCH_TOLERANCE_MS = 2;

static float s_rate = 1.0f;
static bool s_includeGcd;
static double s_carryMs;
static uint32_t s_lastTickMs;
static std::unordered_map<const void*, Tracked> s_frames;

static TSList<SPELLHISTORY, TSGetLink<SPELLHISTORY>>& PlayerHistory() {
    return Spell_C_GetSpellHistory(0).m_history;
}

static bool Near(uint32_t a, uint32_t b) {
    int32_t diff = static_cast<int32_t>(a - b);
    return diff >= -MATCH_TOLERANCE_MS && diff <= MATCH_TOLERANCE_MS;
}

static bool Live(const SPELLHISTORY* entry, uint32_t now, uint32_t start, uint32_t length) {
    if (!length || entry->m_onHold) {
        return false;
    }

    int32_t remaining = static_cast<int32_t>(start + length - now);
    return remaining > 0 && static_cast<uint32_t>(remaining) <= MAX_AFFECTED_MS;
}

static void SetRate(float rate, bool includeGcd) {
    if (!(rate > 0.0f) || rate < MIN_RATE) {
        rate = MIN_RATE;
    } else if (rate > MAX_RATE) {
        rate = MAX_RATE;
    }

    s_rate = rate;
    s_includeGcd = includeGcd;
    s_carryMs = 0.0;
    s_lastTickMs = OsGetAsyncTimeMs();
}

static void Tick() {
    uint32_t now = OsGetAsyncTimeMs();
    int32_t elapsed = static_cast<int32_t>(now - s_lastTickMs);
    s_lastTickMs = now;

    if (s_rate == 1.0f || elapsed <= 0 || elapsed > 5000) {
        return;
    }

    double want = static_cast<double>(elapsed) * (1.0 - s_rate) + s_carryMs;
    int32_t shift = static_cast<int32_t>(want);
    s_carryMs = want - shift;
    if (!shift) {
        return;
    }

    auto& history = PlayerHistory();

    for (SPELLHISTORY* entry = history.Head(); entry; entry = history.Next(entry)) {
        uint32_t gcdEnd = entry->m_time + entry->m_startRecoveryTime;
        bool gcdLive = Live(entry, now, entry->m_time, entry->m_startRecoveryTime);

        if (Live(entry, now, entry->m_time, entry->m_recoveryTime)) {
            entry->m_time += shift;
        } else if (gcdLive && s_includeGcd) {
            entry->m_time += shift;
        }

        if (Live(entry, now, entry->m_categoryTime, entry->m_categoryRecoveryTime)) {
            entry->m_categoryTime += shift;
        }

        if (gcdLive) {
            uint32_t wanted = s_includeGcd ? gcdEnd + shift : gcdEnd;
            int32_t length = static_cast<int32_t>(wanted - entry->m_time);
            entry->m_startRecoveryTime = length > 0 ? static_cast<uint32_t>(length) : 0;
        }
    }
}

static bool Match(uint32_t start, uint32_t duration, Tracked& out) {
    uint32_t now = OsGetAsyncTimeMs();
    auto& history = PlayerHistory();

    for (SPELLHISTORY* entry = history.Head(); entry; entry = history.Next(entry)) {
        if (Live(entry, now, entry->m_time, entry->m_recoveryTime) && Near(start, entry->m_time) && Near(duration, entry->m_recoveryTime)) {
            out = { entry->m_spellId, entry->m_itemId, FIELD_SPELL };
            return true;
        }

        if (Live(entry, now, entry->m_categoryTime, entry->m_categoryRecoveryTime) && Near(start, entry->m_categoryTime) && Near(duration, entry->m_categoryRecoveryTime)) {
            out = { entry->m_spellId, entry->m_itemId, FIELD_CATEGORY };
            return true;
        }

        if (s_includeGcd && Live(entry, now, entry->m_time, entry->m_startRecoveryTime) && Near(start, entry->m_time) && Near(duration, entry->m_startRecoveryTime)) {
            out = { entry->m_spellId, entry->m_itemId, FIELD_GCD };
            return true;
        }
    }

    return false;
}

void Forget() {
    s_rate = 1.0f;
    s_includeGcd = false;
    s_carryMs = 0.0;
    s_frames.clear();
}

void OnCooldownSet(const void* frame, uint32_t start, uint32_t duration) {
    Tracked tracked;

    if (Match(start, duration, tracked)) {
        s_frames[frame] = tracked;
    } else {
        s_frames.erase(frame);
    }
}

void OnCooldownUpdate(const void* frame, uint32_t& startTime) {
    auto tracked = s_frames.find(frame);
    if (tracked == s_frames.end()) {
        return;
    }

    uint32_t now = OsGetAsyncTimeMs();
    auto& history = PlayerHistory();

    for (SPELLHISTORY* entry = history.Head(); entry; entry = history.Next(entry)) {
        if (entry->m_spellId != tracked->second.m_spellId || entry->m_itemId != tracked->second.m_itemId) {
            continue;
        }

        uint32_t start = entry->m_time;
        uint32_t length = entry->m_recoveryTime;

        if (tracked->second.m_field == FIELD_CATEGORY) {
            start = entry->m_categoryTime;
            length = entry->m_categoryRecoveryTime;
        } else if (tracked->second.m_field == FIELD_GCD) {
            length = entry->m_startRecoveryTime;
        }

        if (!Live(entry, now, start, length)) {
            continue;
        }

        startTime = start;
        return;
    }

    s_frames.erase(tracked);
}

void OnCooldownDestroyed(const void* frame) {
    s_frames.erase(frame);
}

static int32_t Packet_SMSG_COOLDOWN_RATE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);

    if (!packet.CanRead(sizeof(float) + sizeof(uint8_t))) {
        return 1;
    }

    float rate = packet.GetFloat();
    uint8_t flags = packet.GetUInt8();

    SetRate(rate, (flags & 0x1) != 0);
    return 1;
}

static int32_t Script_GetCooldownRate(lua_State* L) {
    lua_pushnumber(L, s_rate);
    lua_pushboolean(L, s_includeGcd ? 1 : 0);
    return 2;
}

class CooldownRateModule : public Module {
    public:
    const char* Name() const override {
        return "CooldownRate";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_COOLDOWN_RATE, Packet_SMSG_COOLDOWN_RATE);
        Lua::RegisterFunction("GetCooldownRate", Script_GetCooldownRate);
    }

    void OnDestroyGame() override {
        Forget();
    }

    void OnIdle() override {
        Tick();
    }
};

Module* GetModule() {
    static CooldownRateModule module;
    return &module;
}

}
