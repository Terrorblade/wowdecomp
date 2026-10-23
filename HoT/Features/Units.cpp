#include "HoT/Features/Units.h"
#include <cstdio>
#include <unordered_map>
#include <vector>
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/TimeManager.h"
#include "HoT/Core/CVars.h"
#include "HoT/Core/Modules.h"
#include "HoT/Lua/Script.h"
#include "HoT/Net/Handlers.h"
#include "HoT/Net/Opcodes.h"
#include "HoT/Net/Packet.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Object/ObjectClient/AuraData.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/NamePlateFrame.h"
#include "WoW/Source/Ui/ScriptEvents.h"
#include "WoW/Source/UIUtil/Tooltip.h"

namespace {

struct GuidHash {
    size_t operator()(const WOWGUID& guid) const {
        return std::hash<uint64_t>()(guid.m_guid);
    }
};

template <typename T>
using GuidMap = std::unordered_map<WOWGUID, T, GuidHash>;

const int32_t SPELL_AURA_SCHOOL_ABSORB = 69;
const int32_t SPELL_AURA_MANA_SHIELD = 97;
const int32_t SPELL_AURA_SCHOOL_HEAL_ABSORB = 301;
const uint32_t NUM_AURA_SLOTS = 64;

void SignalUnitEvent(const char* name, WOWGUID guid) {
    int32_t count = 0;
    char** tokens = Script_GetTokenIDsFromGUID(guid, &count);

    for (int32_t i = 0; tokens && i < count; i++) {
        HoT::Lua::SignalEvent(name, "%s", tokens[i]);
    }
}

WOWGUID GuidFromToken(const char* token) {
    WOWGUID guid = WOWGUID();

    if (token && *token) {
        Script_GetGUIDFromToken(token, guid, false);
    }

    return guid;
}

}

namespace HoT::HealPrediction {

void OnAuraValuesReceived(WOWGUID guid, uint32_t spellId);

}

namespace HoT::AuraValues {

static const uint32_t REQUEST_INTERVAL_MS = 1000;
static const uint32_t STALE_AFTER_MS = 3000;
static const uint32_t MAX_BACKOFF_SHIFT = 5;
static const size_t PRUNE_UNIT_THRESHOLD = 256;
static const uint32_t PRUNE_AGE_MS = 60000;

struct Entry {
    Values m_values = {};
    uint32_t m_lastRequestMs = 0;
    uint32_t m_receivedMs = 0;
    uint8_t m_unansweredRequests = 0;
    bool m_haveData = false;
};

struct TooltipCall {
    WOWGUID m_unit;
    int32_t m_slot;
    uint32_t m_spellId;
    CGTooltip* m_tooltip;
    bool m_valid;
};

static GuidMap<std::unordered_map<uint8_t, Entry>> s_entries;
static TooltipCall s_pending;
static TooltipCall s_active;
static TooltipCall s_displayed;

static void ClearAll() {
    s_entries.clear();
    s_pending = TooltipCall();
    s_active = TooltipCall();
    s_displayed = TooltipCall();
}

static void SendRequest(WOWGUID guid, uint8_t slot) {
    Net::Packet packet(Net::CMSG_AURA_VALUES_REQUEST);
    packet.PutUInt64(guid.m_guid);
    packet.PutUInt8(slot);
    packet.Send();
}

static void PruneIfLarge() {
    if (s_entries.size() < PRUNE_UNIT_THRESHOLD) {
        return;
    }

    uint32_t now = OsGetAsyncTimeMs();

    for (auto unit = s_entries.begin(); unit != s_entries.end();) {
        for (auto slot = unit->second.begin(); slot != unit->second.end();) {
            slot = now - slot->second.m_receivedMs > PRUNE_AGE_MS ? unit->second.erase(slot) : std::next(slot);
        }

        unit = unit->second.empty() ? s_entries.erase(unit) : std::next(unit);
    }
}

void RecordTooltipCall(const CGUnit_C* unit, int32_t slot) {
    s_pending = TooltipCall();

    if (unit && slot >= 0 && slot < static_cast<int32_t>(NUM_AURA_SLOTS)) {
        s_pending.m_unit = unit->m_obj->m_guid;
        s_pending.m_slot = slot;
        s_pending.m_valid = true;
    }
}

TooltipScope::TooltipScope(CGTooltip* tooltip, int32_t spellId) {
    s_active = s_pending;
    s_pending = TooltipCall();
    s_active.m_tooltip = tooltip;
    s_active.m_spellId = static_cast<uint32_t>(spellId);
}

TooltipScope::~TooltipScope() {
    s_displayed = s_active;
    s_active = TooltipCall();
}

static const Values* ResolveActive(uint32_t spellId) {
    if (!s_active.m_valid || s_active.m_spellId != spellId) {
        return nullptr;
    }

    uint32_t now = OsGetAsyncTimeMs();
    Entry& entry = s_entries[s_active.m_unit][static_cast<uint8_t>(s_active.m_slot)];
    bool stale = !entry.m_haveData || now - entry.m_receivedMs >= STALE_AFTER_MS;

    if (stale && now - entry.m_lastRequestMs >= REQUEST_INTERVAL_MS) {
        entry.m_lastRequestMs = now;
        SendRequest(s_active.m_unit, static_cast<uint8_t>(s_active.m_slot));
    }

    if (!entry.m_haveData || entry.m_values.m_spellId != spellId) {
        return nullptr;
    }

    return &entry.m_values;
}

bool GetActiveAmount(uint32_t spellId, uint32_t effectIndex, int32_t& out) {
    if (effectIndex < 1 || effectIndex > NUM_AURA_EFFECTS) {
        return false;
    }

    const Values* values = ResolveActive(spellId);
    uint32_t index = effectIndex - 1;

    if (!values || !(values->m_effectMask & (1u << index))) {
        return false;
    }

    out = values->m_amounts[index];
    return true;
}

bool GetActiveAbsorb(uint32_t spellId, const SpellRec* spell, int32_t& out) {
    const Values* values = ResolveActive(spellId);

    if (!values) {
        return false;
    }

    for (uint32_t i = 0; spell && i < NUM_AURA_EFFECTS; i++) {
        int32_t aura = spell->m_effectAura[i];

        if ((aura == SPELL_AURA_SCHOOL_ABSORB || aura == SPELL_AURA_MANA_SHIELD) && (values->m_effectMask & (1u << i))) {
            out = values->m_amounts[i];
            return true;
        }
    }

    for (uint32_t i = 0; i < NUM_AURA_EFFECTS; i++) {
        if (values->m_effectMask & (1u << i)) {
            out = values->m_amounts[i];
            return true;
        }
    }

    return false;
}

bool GetActiveStacks(uint32_t spellId, int32_t& out) {
    const Values* values = ResolveActive(spellId);

    if (!values) {
        return false;
    }

    out = values->m_stacks;
    return true;
}

bool GetSlotValues(WOWGUID guid, uint8_t slot, uint32_t spellId, Values& out) {
    if (!guid || slot >= NUM_AURA_SLOTS) {
        return false;
    }

    Entry& entry = s_entries[guid][slot];

    if (entry.m_haveData && entry.m_values.m_spellId == spellId) {
        out = entry.m_values;
        return true;
    }

    uint32_t shift = entry.m_unansweredRequests < MAX_BACKOFF_SHIFT ? entry.m_unansweredRequests : MAX_BACKOFF_SHIFT;
    uint32_t now = OsGetAsyncTimeMs();

    if (now - entry.m_lastRequestMs >= (REQUEST_INTERVAL_MS << shift)) {
        entry.m_lastRequestMs = now;

        if (entry.m_unansweredRequests < 0xFF) {
            entry.m_unansweredRequests++;
        }

        SendRequest(guid, slot);
    }

    return false;
}

static int32_t Packet_SMSG_AURA_VALUES(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    WOWGUID guid = { packet.GetUInt64() };
    uint8_t slot = packet.GetUInt8();

    Values values = {};
    values.m_spellId = packet.GetUInt32();
    values.m_effectMask = packet.GetUInt8();
    values.m_stacks = packet.GetUInt8();

    if (!guid || slot >= NUM_AURA_SLOTS) {
        return 1;
    }

    for (uint32_t i = 0; i < NUM_AURA_EFFECTS; i++) {
        if (values.m_effectMask & (1u << i)) {
            values.m_amounts[i] = packet.GetInt32();
        }
    }

    PruneIfLarge();

    Entry& entry = s_entries[guid][slot];
    entry.m_values = values;
    entry.m_receivedMs = OsGetAsyncTimeMs();
    entry.m_unansweredRequests = 0;
    entry.m_haveData = true;

    HealPrediction::OnAuraValuesReceived(guid, values.m_spellId);

    const TooltipCall& shown = s_displayed;

    if (shown.m_valid && shown.m_unit == guid && shown.m_slot == slot && shown.m_spellId == values.m_spellId && shown.m_tooltip && shown.m_tooltip->m_isVisible) {
        Lua::SignalEvent("HOT_AURA_VALUES_UPDATED", nullptr);
    }

    return 1;
}

class AuraValuesModule : public Module {
    public:
    const char* Name() const override {
        return "AuraValues";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_AURA_VALUES, Packet_SMSG_AURA_VALUES);
    }

    void OnEnterWorld() override {
        ClearAll();
    }

    void OnLoadingScreenStarted() override {
        ClearAll();
    }
};

Module* GetModule() {
    static AuraValuesModule module;
    return &module;
}

}

namespace HoT::HealPrediction {

static const uint32_t INCOMING_HEAL_GRACE_MS = 1500;

struct IncomingHeal {
    WOWGUID m_caster;
    uint32_t m_spellId;
    uint32_t m_amount;
    uint32_t m_expiresMs;
};

struct AbsorbEffects {
    uint8_t m_absorbMask;
    uint8_t m_healAbsorbMask;
};

static GuidMap<std::vector<IncomingHeal>> s_incomingHeals;
static std::unordered_map<uint32_t, AbsorbEffects> s_spellEffects;

static const AbsorbEffects& EffectsFor(uint32_t spellId) {
    auto it = s_spellEffects.find(spellId);

    if (it != s_spellEffects.end()) {
        return it->second;
    }

    AbsorbEffects effects = {};
    SpellRec spell;

    if (g_spellDB.GetRecord(spellId, &spell)) {
        for (uint32_t i = 0; i < AuraValues::NUM_AURA_EFFECTS; i++) {
            int32_t aura = spell.m_effectAura[i];

            if (aura == SPELL_AURA_SCHOOL_ABSORB || aura == SPELL_AURA_MANA_SHIELD) {
                effects.m_absorbMask |= static_cast<uint8_t>(1u << i);
            } else if (aura == SPELL_AURA_SCHOOL_HEAL_ABSORB) {
                effects.m_healAbsorbMask |= static_cast<uint8_t>(1u << i);
            }
        }
    }

    return s_spellEffects.emplace(spellId, effects).first->second;
}

static uint32_t SumAbsorbs(WOWGUID guid, bool healAbsorb) {
    CGUnit_C* unit = guid ? static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__)) : nullptr;

    if (!unit) {
        return 0;
    }

    uint32_t count = unit->GetAuraCount();
    count = count < NUM_AURA_SLOTS ? count : NUM_AURA_SLOTS;
    uint32_t total = 0;

    for (uint32_t slot = 0; slot < count; slot++) {
        AuraData* aura = unit->GetAura(slot);

        if (!aura || !aura->m_spellId) {
            continue;
        }

        const AbsorbEffects& effects = EffectsFor(aura->m_spellId);
        uint8_t mask = healAbsorb ? effects.m_healAbsorbMask : effects.m_absorbMask;
        AuraValues::Values values;

        if (!mask || !AuraValues::GetSlotValues(guid, static_cast<uint8_t>(slot), aura->m_spellId, values)) {
            continue;
        }

        for (uint32_t i = 0; i < AuraValues::NUM_AURA_EFFECTS; i++) {
            uint8_t bit = static_cast<uint8_t>(1u << i);

            if ((mask & bit) && (values.m_effectMask & bit) && values.m_amounts[i] > 0) {
                total += static_cast<uint32_t>(values.m_amounts[i]);
            }
        }
    }

    return total;
}

uint32_t GetTotalAbsorbs(WOWGUID guid) {
    return SumAbsorbs(guid, false);
}

uint32_t GetTotalHealAbsorbs(WOWGUID guid) {
    return SumAbsorbs(guid, true);
}

uint32_t GetIncomingHeals(WOWGUID guid, WOWGUID healer) {
    auto it = s_incomingHeals.find(guid);

    if (it == s_incomingHeals.end()) {
        return 0;
    }

    uint32_t now = OsGetAsyncTimeMs();
    uint32_t total = 0;
    std::vector<IncomingHeal>& heals = it->second;

    for (auto heal = heals.begin(); heal != heals.end();) {
        if (static_cast<int32_t>(now - heal->m_expiresMs) >= 0) {
            heal = heals.erase(heal);
            continue;
        }

        if (!healer || heal->m_caster == healer) {
            total += heal->m_amount;
        }

        ++heal;
    }

    if (heals.empty()) {
        s_incomingHeals.erase(it);
    }

    return total;
}

void OnAuraValuesReceived(WOWGUID guid, uint32_t spellId) {
    const AbsorbEffects& effects = EffectsFor(spellId);

    if (effects.m_absorbMask) {
        SignalUnitEvent("UNIT_ABSORB_AMOUNT_CHANGED", guid);
    }

    if (effects.m_healAbsorbMask) {
        SignalUnitEvent("UNIT_HEAL_ABSORB_AMOUNT_CHANGED", guid);
    }
}

static int32_t Packet_SMSG_UNIT_HEAL_PREDICTION(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    WOWGUID caster = { packet.GetUInt64() };
    WOWGUID target = { packet.GetUInt64() };
    uint32_t spellId = packet.GetUInt32();
    int32_t amount = packet.GetInt32();
    uint32_t remainingCastMs = packet.GetUInt32();

    if (!caster || !target) {
        return 1;
    }

    std::vector<IncomingHeal>& heals = s_incomingHeals[target];

    for (auto heal = heals.begin(); heal != heals.end();) {
        heal = heal->m_caster == caster ? heals.erase(heal) : std::next(heal);
    }

    if (amount > 0) {
        heals.push_back({ caster, spellId, static_cast<uint32_t>(amount), OsGetAsyncTimeMs() + remainingCastMs + INCOMING_HEAL_GRACE_MS });
    }

    if (heals.empty()) {
        s_incomingHeals.erase(target);
    }

    SignalUnitEvent("UNIT_HEAL_PREDICTION", target);
    return 1;
}

static bool ResolveUnitArg(lua_State* L, int32_t index, WOWGUID& guid) {
    guid = lua_gettop(L) >= index ? GuidFromToken(lua_tolstring(L, index, nullptr)) : WOWGUID();
    return static_cast<bool>(guid);
}

static int32_t Script_UnitGetTotalAbsorbs(lua_State* L) {
    WOWGUID guid = WOWGUID();

    if (ResolveUnitArg(L, 1, guid)) {
        lua_pushnumber(L, GetTotalAbsorbs(guid));
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int32_t Script_UnitGetTotalHealAbsorbs(lua_State* L) {
    WOWGUID guid = WOWGUID();

    if (ResolveUnitArg(L, 1, guid)) {
        lua_pushnumber(L, GetTotalHealAbsorbs(guid));
    } else {
        lua_pushnil(L);
    }

    return 1;
}

static int32_t Script_UnitGetIncomingHeals(lua_State* L) {
    WOWGUID guid = WOWGUID();
    WOWGUID healer = WOWGUID();

    if (!ResolveUnitArg(L, 1, guid)) {
        lua_pushnil(L);
    } else if (lua_gettop(L) >= 2 && !ResolveUnitArg(L, 2, healer)) {
        lua_pushnumber(L, 0);
    } else {
        lua_pushnumber(L, GetIncomingHeals(guid, healer));
    }

    return 1;
}

class HealPredictionModule : public Module {
    public:
    const char* Name() const override {
        return "HealPrediction";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_UNIT_HEAL_PREDICTION, Packet_SMSG_UNIT_HEAL_PREDICTION);

        Lua::RegisterFunction("UnitGetTotalAbsorbs", Script_UnitGetTotalAbsorbs);
        Lua::RegisterFunction("UnitGetTotalHealAbsorbs", Script_UnitGetTotalHealAbsorbs);
        Lua::RegisterFunction("UnitGetIncomingHeals", Script_UnitGetIncomingHeals);

        // Read by the interface code that draws the bars.
        CVars::Register("healPredictionAbsorb", "Draw absorb shields on unit frame health bars.", "1");
        CVars::Register("healPredictionAbsorbFullHealth", "Keep drawing the absorb shield when the unit is at full health, capped at the width of the health bar.", "0");
        CVars::Register("healPredictionAbsorbText", "Show the total absorb amount as text on the health bar.", "0");
        CVars::Register("healPredictionHealText", "Show the predicted incoming heal amount as text on the health bar.", "0");
        CVars::Register("healPredictionAbsorbColor", "Tint for the absorb shield marker, as rrggbb hex.", "ffffff");
        CVars::Register("healPredictionHealAbsorbColor", "Tint for the heal absorb marker, as rrggbb hex.", "ffffff");
        CVars::Register("healPredictionMyHealColor", "Colour of your own incoming heal marker, as rrggbb hex.", "00a89b");
        CVars::Register("healPredictionOtherHealColor", "Colour of other players' incoming heal markers, as rrggbb hex.", "155947");
        CVars::Register("healPredictionAbsorbTextColor", "Colour of the absorb amount text, as rrggbb hex.", "66b3ff");
        CVars::Register("healPredictionHealTextColor", "Colour of the incoming heal amount text, as rrggbb hex.", "ffd100");
    }

    void OnEnterWorld() override {
        s_incomingHeals.clear();
    }

    void OnLoadingScreenStarted() override {
        s_incomingHeals.clear();
    }
};

Module* GetModule() {
    static HealPredictionModule module;
    return &module;
}

}

namespace HoT::UnitLevel {

static const uint32_t RETRY_INTERVAL_MS = 2000;
static const uint8_t MAX_ATTEMPTS = 4;

struct Request {
    uint32_t m_lastMs;
    uint8_t m_attempts;
};

static GuidMap<uint32_t> s_playerItemLevels;
static GuidMap<uint8_t> s_playerSubClasses;
static GuidMap<uint32_t> s_creatureDungeonLevels;
static GuidMap<Request> s_requests;

static bool IsPlayerGuid(WOWGUID guid) {
    return guid && (guid.m_guid >> 48) == 0;
}

static bool WantsLevelCache(WOWGUID guid) {
    uint16_t high = static_cast<uint16_t>(guid.m_guid >> 48);
    return guid && (high == 0x0000 || high == 0xF130 || high == 0xF140 || high == 0xF150);
}

template <typename T>
static T Find(const GuidMap<T>& map, WOWGUID guid) {
    auto it = map.find(guid);
    return it != map.end() ? it->second : T();
}

bool HasPlayerItemLevel(WOWGUID guid) {
    return s_playerItemLevels.count(guid) != 0;
}

uint32_t GetPlayerItemLevel(WOWGUID guid) {
    return Find(s_playerItemLevels, guid);
}

uint8_t GetPlayerSubClass(WOWGUID guid) {
    return Find(s_playerSubClasses, guid);
}

uint32_t GetUnitLevel(WOWGUID guid) {
    auto it = s_playerItemLevels.find(guid);
    return it != s_playerItemLevels.end() ? it->second : Find(s_creatureDungeonLevels, guid);
}

static bool HasUnitLevel(WOWGUID guid) {
    return s_playerItemLevels.count(guid) || s_creatureDungeonLevels.count(guid);
}

static void StampRequest(WOWGUID guid) {
    Request& request = s_requests[guid];
    request.m_lastMs = OsGetAsyncTimeMs();

    if (request.m_attempts < MAX_ATTEMPTS) {
        request.m_attempts++;
    }
}

static bool ShouldRequest(WOWGUID guid) {
    auto it = s_requests.find(guid);

    if (it == s_requests.end()) {
        return true;
    }

    return it->second.m_attempts < MAX_ATTEMPTS && OsGetAsyncTimeMs() - it->second.m_lastMs >= RETRY_INTERVAL_MS;
}

static void ForgetRequests(bool players) {
    for (auto it = s_requests.begin(); it != s_requests.end();) {
        it = IsPlayerGuid(it->first) == players ? s_requests.erase(it) : std::next(it);
    }
}

static void Send(WOWGUID guid) {
    StampRequest(guid);
    Net::Packet packet(Net::CMSG_UNIT_LEVEL_CACHE_REQUEST);
    packet.PutUInt64(guid.m_guid);
    packet.Send();
}

void SendRequest(WOWGUID guid) {
    if (WantsLevelCache(guid)) {
        s_requests.erase(guid);
        Send(guid);
    }
}

static void EnsureRequested(WOWGUID guid) {
    if (WantsLevelCache(guid) && !HasUnitLevel(guid) && ShouldRequest(guid)) {
        Send(guid);
    }
}

uint32_t GetOrRequest(WOWGUID guid) {
    uint32_t level = GetUnitLevel(guid);

    if (!level) {
        EnsureRequested(guid);
    }

    return level;
}

int32_t GetDisplayLevel(const CGUnit_C* unit, int32_t fallback) {
    uint32_t level = unit ? GetOrRequest(unit->m_obj->m_guid) : 0;
    return level ? static_cast<int32_t>(level) : fallback;
}

const ChrClassesRec* GetSubClassRecord(const CGUnit_C* unit) {
    if (!unit) {
        return nullptr;
    }

    WOWGUID guid = unit->m_obj->m_guid;
    uint8_t subClass = GetPlayerSubClass(guid);

    if (!subClass) {
        EnsureRequested(guid);
        return nullptr;
    }

    return g_chrClassesDB.GetRecord(subClass);
}

bool PushScaledLevel(lua_State* L) {
    WOWGUID guid = GuidFromToken(lua_tolstring(L, 1, nullptr));

    if (!guid || !GetOrRequest(guid)) {
        return false;
    }

    CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__));

    if (!unit || unit->IsBossMob()) {
        return false;
    }

    lua_pushnumber(L, GetUnitLevel(guid));
    return true;
}

static void RefreshUnitDisplays(WOWGUID guid) {
    CGUnit_C* unit = static_cast<CGUnit_C*>(ClntObjMgrObjectPtr(guid, TYPE_UNIT, __FILE__, __LINE__));

    if (unit && unit->m_namePlateFrame) {
        unit->m_namePlateFrame->UpdateLevelDisplay(unit);
    }

    SignalUnitEvent("UNIT_LEVEL", guid);

    static const char* const s_viewTokens[] = { "mouseover", "target", "focus", "targettarget", "focustarget" };
    bool onScreen = false;

    for (const char* token : s_viewTokens) {
        if (GuidFromToken(token) != guid) {
            continue;
        }

        onScreen = true;

        if (strcmp(token, "mouseover")) {
            Lua::SignalEvent("UNIT_LEVEL", "%s", token);
        }
    }

    if (onScreen) {
        char script[192];
        snprintf(script, sizeof(script), "if GameTooltip:IsShown() then local _,u=GameTooltip:GetUnit() if u and UnitGUID(u)==\"0x%016llX\" then GameTooltip:SetUnit(u) end end", static_cast<unsigned long long>(guid.m_guid));
        FrameScript_Execute(script, "UnitLevelCache", nullptr);
    }
}

static int32_t Packet_SMSG_UNIT_LEVEL_CACHE_RESPONSE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    Net::Packet packet(msg);
    WOWGUID guid = { packet.GetUInt64() };
    uint32_t value = packet.GetUInt32();

    if (!guid || !value) {
        return 1;
    }

    if (IsPlayerGuid(guid)) {
        s_playerItemLevels[guid] = value;
        s_playerSubClasses[guid] = packet.GetUInt8();

        if (guid == ClntObjMgrGetActivePlayer()) {
            Lua::SignalEvent("HOT_PLAYER_ITEM_LEVEL", "%u", value);
        }
    } else {
        s_creatureDungeonLevels[guid] = value;
    }

    s_requests.erase(guid);
    RefreshUnitDisplays(guid);
    return 1;
}

// Returns nil while the value is being requested.
template <bool PlayerOnly, bool SubClass>
static int32_t Script_GetCachedLevel(lua_State* L) {
    const char* token = lua_tolstring(L, 1, nullptr);
    WOWGUID guid = GuidFromToken(token ? token : (SubClass ? "player" : nullptr));
    bool known = PlayerOnly ? HasPlayerItemLevel(guid) : HasUnitLevel(guid);

    if (!guid) {
        lua_pushnil(L);
    } else if (!known) {
        EnsureRequested(guid);
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, SubClass ? GetPlayerSubClass(guid) : PlayerOnly ? GetPlayerItemLevel(guid) : GetUnitLevel(guid));
    }

    return 1;
}

static int32_t Script_GetOurItemLevel(lua_State* L) {
    WOWGUID guid = ClntObjMgrGetActivePlayer();

    if (!guid) {
        lua_pushnil(L);
    } else if (!HasPlayerItemLevel(guid)) {
        EnsureRequested(guid);
        lua_pushnil(L);
    } else {
        lua_pushnumber(L, GetPlayerItemLevel(guid));
    }

    return 1;
}

class UnitLevelModule : public Module {
    public:
    const char* Name() const override {
        return "UnitLevel";
    }

    void OnInitializeGlobal() override {
        Net::RegisterWorldHandler(Net::SMSG_UNIT_LEVEL_CACHE_RESPONSE, Packet_SMSG_UNIT_LEVEL_CACHE_RESPONSE);

        Lua::RegisterFunction("GetUnitItemLevel", Script_GetCachedLevel<true, false>);
        Lua::RegisterFunction("GetUnitDungeonLevel", Script_GetCachedLevel<false, false>);
        Lua::RegisterFunction("GetUnitSubclass", Script_GetCachedLevel<true, true>);
        Lua::RegisterFunction("GetOurItemLevel", Script_GetOurItemLevel);
    }

    void OnEnterWorld() override {
        s_playerItemLevels.clear();
        s_playerSubClasses.clear();
        s_creatureDungeonLevels.clear();
        s_requests.clear();
    }

    void OnLoadingScreenStarted() override {
        s_creatureDungeonLevels.clear();
        ForgetRequests(false);
    }
};

Module* GetModule() {
    static UnitLevelModule module;
    return &module;
}

}
