#include "HoT/Editor/Spell/SpellPreview.h"
#include <cmath>
#include <cstdlib>
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/Os/TimeManager.h"
#include "HoT/Editor/Dbc/Store.h"
#include "HoT/Game/Session.h"
#include "HoT/Lua/Script.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Magic/MagicClient/Spell_C.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Object/ObjectClient/Player_C.h"
#include "WoW/Source/Object/ObjectClient/Unit_C.h"
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/ObjectMgrClient/ObjectMgrClient.h"
#include "WoW/Source/Ui/GameUI.h"

namespace HoT::Editor::SpellPreview {

static constexpr int8_t kRequiredSecurityLevel = 3;
static constexpr uint32_t TARGET_FLAG_UNIT = 0x2;
static constexpr uint32_t TARGET_FLAG_DEST_LOCATION = 0x40;
static constexpr int32_t kInfiniteChannelMs = 10000;
static constexpr int32_t kMaxChannelMs = 60000;
static constexpr float kGroundDistance = 10.0f;

enum class Phase {
    Idle,
    Casting,
    Channeling,
};

struct State {
    Phase phase = Phase::Idle;
    int32_t spellId = 0;
    uint64_t caster = 0;
    uint64_t target = 0;
    uint32_t phaseEnd = 0;
    bool patched = false;
};

struct Targets {
    uint32_t flags = 0;
    uint64_t unit = 0;
    C3Vector dest = { 0.0f, 0.0f, 0.0f };
};

static State s_state;

// The fake cast must never chain into a real one or start auto attack, and moving must not cancel it.
static void StripSideEffects(SpellRec& rec) {
    rec.m_modalNextSpell = 0;
    rec.m_attributesExB &= ~0x100000;
    rec.m_interruptFlags &= ~(0x1 | 0x8);
}

static bool UsesDestination(const SpellRec& spell) {
    if (spell.m_targets & TARGET_FLAG_DEST_LOCATION) {
        return true;
    }

    for (int32_t i = 0; i < 3; i++) {
        for (int32_t target : { spell.m_implicitTargetA[i], spell.m_implicitTargetB[i] }) {
            switch (target) {
                case 8:
                case 16:
                case 28:
                case 29:
                case 31:
                case 34:
                case 53:
                case 63:
                case 87:
                    return true;
            }
        }
    }

    return false;
}

static CGPlayer_C* Caster() {
    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();
    return player && s_state.caster && player->m_obj->m_guid.m_guid == s_state.caster ? player : nullptr;
}

static CGObject_C* ObjectByGuid(uint64_t guid) {
    return guid ? ClntObjMgrObjectPtr(WOWGUID{ guid }, TYPE_OBJECT, __FILE__, __LINE__) : nullptr;
}

static Targets PickTargets(const SpellRec& spell, CGPlayer_C* player) {
    Targets targets;
    CGObject_C* target = ObjectByGuid(s_state.target);

    if (target && s_state.target != s_state.caster) {
        targets.flags |= TARGET_FLAG_UNIT;
        targets.unit = s_state.target;
    }

    if (UsesDestination(spell)) {
        targets.flags |= TARGET_FLAG_DEST_LOCATION;

        if (targets.unit) {
            targets.dest = target->GetPosition();
        } else {
            C3Vector position = player->GetPosition();
            float facing = player->GetFacing();
            targets.dest = { position.x + cosf(facing) * kGroundDistance, position.y + sinf(facing) * kGroundDistance, position.z };
        }
    }

    return targets;
}

static void PutGuid(CDataStore& msg, uint64_t guid) {
    SmartGUID packed{};
    packed.m_guid = guid;
    msg << packed;
}

static void PutHeader(CDataStore& msg, uint32_t castFlags) {
    PutGuid(msg, 0);
    PutGuid(msg, s_state.caster);
    msg.Put(static_cast<uint8_t>(0));
    msg.Put(static_cast<uint32_t>(s_state.spellId));
    msg.Put(castFlags);
}

static void PutTargets(CDataStore& msg, const Targets& targets) {
    msg.Put(targets.flags);

    if (targets.flags & TARGET_FLAG_UNIT) {
        PutGuid(msg, targets.unit);
    }

    if (targets.flags & TARGET_FLAG_DEST_LOCATION) {
        PutGuid(msg, 0);
        msg.Put(targets.dest.x);
        msg.Put(targets.dest.y);
        msg.Put(targets.dest.z);
    }
}

static void Dispatch(NETMESSAGE msgId, CDataStore& msg) {
    msg.Finalize();
    Packet_Group_30(nullptr, msgId, OsGetAsyncTimeMs(), &msg);
}

static PendingSpellCast* PendingCast() {
    return GetPendingSpellCast(0, s_state.spellId, WOWGUID{ s_state.caster });
}

static void SendStart(const SpellRec& spell, CGPlayer_C* player, int32_t castTime) {
    CDataStore msg;
    PutHeader(msg, 0);
    msg.Put(static_cast<uint32_t>(castTime));
    PutTargets(msg, PickTargets(spell, player));
    Dispatch(SMSG_SPELL_START, msg);
}

static void SendGo(const SpellRec& spell, CGPlayer_C* player) {
    Targets targets = PickTargets(spell, player);
    uint64_t hit = targets.unit ? targets.unit : s_state.caster;

    CDataStore msg;
    PutHeader(msg, 0);
    msg.Put(OsGetAsyncTimeMs());
    msg.Put(static_cast<uint8_t>(1));
    msg.Put(hit);
    msg.Put(static_cast<uint8_t>(0));
    PutTargets(msg, targets);

    if (targets.flags & TARGET_FLAG_DEST_LOCATION) {
        msg.Put(static_cast<uint8_t>(0));
    }

    Dispatch(SMSG_SPELL_GO, msg);
}

static void Unpatch() {
    if (s_state.patched) {
        Dbc::UnpatchSpell(s_state.spellId);
        s_state.patched = false;
    }
}

static void Finish() {
    Unpatch();
    s_state = State();
}

static int32_t ChannelDuration(const SpellRec& spell) {
    if (!(spell.m_attributesEx & (0x4 | 0x40))) {
        return 0;
    }

    SpellDurationRec* duration = g_spellDurationDB.GetRecord(spell.m_durationIndex);

    if (!duration || !duration->m_duration) {
        return 0;
    }

    if (duration->m_duration < 0) {
        return kInfiniteChannelMs;
    }

    return duration->m_duration < kMaxChannelMs ? duration->m_duration : kMaxChannelMs;
}

// The server would set the channel spell field, the preview plays the channel kit like CGUnit_C::UpdateChannelEffects.
static bool StartChannel(const SpellRec& spell, CGPlayer_C* player) {
    int32_t duration = ChannelDuration(spell);

    if (duration <= 0) {
        return false;
    }

    SpellVisualRec visualRec;
    const SpellVisualRec* visual = player->GetAppropriateSpellVisual(&spell, visualRec, 0, 0);
    SpellVisualKitRec* kit = visual ? g_spellVisualKitDB.GetRecord(visual->m_channelKit) : nullptr;

    if (kit) {
        SpellVisualKitRec* castKit = g_spellVisualKitDB.GetRecord(visual->m_castKit);
        PlaySpellVisuaKitData data(&spell, kit, 2);
        data.m_playAnim = !(castKit && player->GetCurrentTorsoAnimId() == static_cast<uint32_t>(castKit->m_animID));
        player->PlaySpellVisualKit(data);
    }

    player->SetUIChannelSpell(s_state.spellId, duration);
    s_state.phase = Phase::Channeling;
    s_state.phaseEnd = OsGetAsyncTimeMs() + static_cast<uint32_t>(duration);
    return true;
}

static void Go() {
    CGPlayer_C* player = Caster();
    SpellRec spell;

    if (!player || !g_spellDB.GetRecord(s_state.spellId, &spell)) {
        Finish();
        return;
    }

    SendGo(spell, player);
    Unpatch();

    if (!StartChannel(spell, player)) {
        s_state = State();
    }
}

bool Cast(int32_t spellId, uint64_t target) {
    Stop();

    CGPlayer_C* player = ClntObjMgrGetActivePlayerObj();

    if (!player || Spell_C_IsActivePlayerCasting() || player->m_channelSpellId || player->m_unit->m_unitChannelSpell) {
        return false;
    }

    SpellRec spell;

    if (!g_spellDB.GetRecord(spellId, &spell)) {
        return false;
    }

    uint64_t caster = player->m_obj->m_guid.m_guid;

    if (!target) {
        target = CGGameUI::m_lockedTarget.m_guid;
    }

    if (!ObjectByGuid(target)) {
        target = caster;
    }

    s_state.spellId = spellId;
    s_state.caster = caster;
    s_state.target = target;
    s_state.patched = Dbc::PatchSpell(spellId, StripSideEffects);

    g_spellDB.GetRecord(spellId, &spell);
    int32_t castTime = player->GetSpellCastingTime(&spell);

    if (castTime <= 0) {
        Go();
        return true;
    }

    SendStart(spell, player, castTime);

    if (!PendingCast()) {
        Finish();
        return false;
    }

    s_state.phase = Phase::Casting;
    s_state.phaseEnd = OsGetAsyncTimeMs() + static_cast<uint32_t>(castTime);
    return true;
}

void Stop() {
    CGPlayer_C* player = Caster();

    if (s_state.phase == Phase::Casting) {
        PendingSpellCast* cast = PendingCast();
        SpellRec spell;

        if (cast && player && g_spellDB.GetRecord(s_state.spellId, &spell)) {
            StripSideEffects(spell);
            HandleSpellFailedOrGo(player, cast, &spell, 27);
        }

        if (cast) {
            ReleasePendingSpellCast(cast);
        }
    } else if (s_state.phase == Phase::Channeling && player) {
        player->RemoveChannelEffects(s_state.spellId);

        if (player->m_channelSpellId == static_cast<uint32_t>(s_state.spellId)) {
            player->ClearUIChannel();
        }
    }

    Finish();
}

bool IsPlaying() {
    return s_state.phase != Phase::Idle;
}

void OnIdle() {
    if (s_state.phase == Phase::Idle) {
        return;
    }

    if (!Caster()) {
        Finish();
        return;
    }

    bool due = static_cast<int32_t>(OsGetAsyncTimeMs() - s_state.phaseEnd) >= 0;

    if (s_state.phase == Phase::Casting) {
        // Escape or a cancel from the client released the fake cast.
        if (!PendingCast()) {
            Finish();
        } else if (due) {
            Go();
        }
    } else if (due) {
        Stop();
    }
}

void OnGameDestroy() {
    Finish();
}

static uint64_t GuidArgument(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TSTRING) {
        return std::strtoull(lua_tostring(L, index), nullptr, 0);
    }

    if (lua_type(L, index) == LUA_TNUMBER) {
        return static_cast<uint64_t>(lua_tonumber(L, index));
    }

    return 0;
}

// SpellPreview_Cast(spellId [, targetGuid]) -> true when the preview started.
static int32_t Script_Cast(lua_State* L) {
    int32_t spellId = static_cast<int32_t>(lua_tonumber(L, 1));
    lua_pushboolean(L, spellId > 0 && Cast(spellId, GuidArgument(L, 2)));
    return 1;
}

static int32_t Script_Stop(lua_State* L) {
    Stop();
    return 0;
}

// SpellPreview_IsPlaying() -> playing, spellId.
static int32_t Script_IsPlaying(lua_State* L) {
    lua_pushboolean(L, IsPlaying());
    lua_pushnumber(L, s_state.spellId);
    return 2;
}

template <lua_CFunction Function>
static int32_t Gated(lua_State* L) {
    if (!Session::HasClearance(kRequiredSecurityLevel)) {
        return 0;
    }

    return Function(L);
}

void Apply() {
    Lua::RegisterFunction("SpellPreview_Cast", Gated<Script_Cast>);
    Lua::RegisterFunction("SpellPreview_Stop", Gated<Script_Stop>);
    Lua::RegisterFunction("SpellPreview_IsPlaying", Gated<Script_IsPlaying>);
}

}
