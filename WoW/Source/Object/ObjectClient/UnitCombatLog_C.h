#ifndef WOW_SOURCE_OBJECT_OBJECTCLIENT_UNITCOMBATLOG_C_H
#define WOW_SOURCE_OBJECT_OBJECTCLIENT_UNITCOMBATLOG_C_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Object/WGUID.h"
#include "WoW/Source/Net/Types.h"
#include "Engine/Source/Base/RCString.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Object/ObjectClient/Types.h"

class ATTACKROUNDINFO;
class CGUnit_C;
class CDataStore;
class SpellRec;
struct lua_State;
struct PendingCombatLogData;

enum COMBAT_LOG_INFO_FLAGS : uint32_t
{
    COMBAT_LOG_INFO_FLAG_NONE = 0x00000000u,
    COMBAT_LOG_INFO_FLAG_EXTRA_ATTACK_COUNT         = 0x00000001u,
    COMBAT_LOG_INFO_FLAG_FAIL_REASON                = 0x00000002u,
    COMBAT_LOG_INFO_FLAG_MISS_TYPE                  = 0x00000004u,
    COMBAT_LOG_INFO_FLAG_ENVIRONMENTAL_TYPE         = 0x00000008u,
    COMBAT_LOG_INFO_FLAG_DAMAGE                     = 0x00000010u,
    COMBAT_LOG_INFO_FLAG_HEAL                       = 0x00000020u,
    COMBAT_LOG_INFO_FLAG_ENERGIZE                   = 0x00000040u,
    COMBAT_LOG_INFO_FLAG_POWER_DRAIN                = 0x00000080u,
    COMBAT_LOG_INFO_FLAG_EXTRA_SPELL                = 0x00000100u,
    COMBAT_LOG_INFO_FLAG_ITEM                       = 0x00000200u,
    COMBAT_LOG_INFO_FLAG_AURA_TYPE                  = 0x00000400u,
    COMBAT_LOG_INFO_FLAG_AURA_STACK_COUNT           = 0x00000800u,
    COMBAT_LOG_INFO_FLAG_ENCHANT_NAME               = 0x00001000u,
    COMBAT_LOG_INFO_FLAG_ABSORBED                   = 0x00002000u,
    COMBAT_LOG_INFO_FLAG_RESISTED                   = 0x00004000u,
    COMBAT_LOG_INFO_FLAG_BLOCKED                    = 0x00008000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_16                  = 0x00010000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_17                  = 0x00020000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_18                  = 0x00040000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_19                  = 0x00080000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_20                  = 0x00100000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_21                  = 0x00200000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_22                  = 0x00400000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_23                  = 0x00800000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_24                  = 0x01000000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_25                  = 0x02000000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_26                  = 0x04000000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_27                  = 0x08000000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_28                  = 0x10000000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_29                  = 0x20000000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_30                  = 0x40000000u,
    COMBAT_LOG_INFO_FLAG_UNUSED_31                  = 0x80000000u,
};

enum COMBAT_LOG_EVENT : int32_t
{
    COMBAT_LOG_EVENT_NONE                        = -1,
    COMBAT_LOG_EVENT_ENVIRONMENTAL_DAMAGE        = 0,
    COMBAT_LOG_EVENT_SWING_DAMAGE                = 1,
    COMBAT_LOG_EVENT_SWING_MISSED                = 2,
    COMBAT_LOG_EVENT_RANGE_DAMAGE                = 3,
    COMBAT_LOG_EVENT_RANGE_MISSED                = 4,
    COMBAT_LOG_EVENT_SPELL_CAST_START            = 5,
    COMBAT_LOG_EVENT_SPELL_CAST_SUCCESS          = 6,
    COMBAT_LOG_EVENT_SPELL_CAST_FAILED           = 7,
    COMBAT_LOG_EVENT_SPELL_MISSED                = 8,
    COMBAT_LOG_EVENT_SPELL_DAMAGE                = 9,
    COMBAT_LOG_EVENT_SPELL_HEAL                  = 10,
    COMBAT_LOG_EVENT_SPELL_ENERGIZE              = 11,
    COMBAT_LOG_EVENT_SPELL_DRAIN                 = 12,
    COMBAT_LOG_EVENT_SPELL_LEECH                 = 13,
    COMBAT_LOG_EVENT_SPELL_INSTAKILL             = 14,
    COMBAT_LOG_EVENT_SPELL_SUMMON                = 15,
    COMBAT_LOG_EVENT_SPELL_CREATE                = 16,
    COMBAT_LOG_EVENT_SPELL_INTERRUPT             = 17,
    COMBAT_LOG_EVENT_SPELL_EXTRA_ATTACKS         = 18,
    COMBAT_LOG_EVENT_SPELL_DURABILITY_DAMAGE     = 19,
    COMBAT_LOG_EVENT_SPELL_DURABILITY_DAMAGE_ALL = 20,
    COMBAT_LOG_EVENT_SPELL_AURA_APPLIED          = 21,
    COMBAT_LOG_EVENT_SPELL_AURA_APPLIED_DOSE     = 22,
    COMBAT_LOG_EVENT_SPELL_AURA_REMOVED_DOSE     = 23,
    COMBAT_LOG_EVENT_SPELL_AURA_REMOVED          = 24,
    COMBAT_LOG_EVENT_SPELL_AURA_REFRESH          = 25,
    COMBAT_LOG_EVENT_SPELL_DISPEL                = 26,
    COMBAT_LOG_EVENT_SPELL_STOLEN                = 27,
    COMBAT_LOG_EVENT_SPELL_AURA_BROKEN           = 28,
    COMBAT_LOG_EVENT_SPELL_AURA_BROKEN_SPELL     = 29,
    COMBAT_LOG_EVENT_DAMAGE_AURA_BROKEN          = 30,
    COMBAT_LOG_EVENT_ENCHANT_APPLIED             = 31,
    COMBAT_LOG_EVENT_ENCHANT_REMOVED             = 32,
    COMBAT_LOG_EVENT_SPELL_PERIODIC_MISSED       = 33,
    COMBAT_LOG_EVENT_SPELL_PERIODIC_DAMAGE       = 34,
    COMBAT_LOG_EVENT_SPELL_PERIODIC_HEAL         = 35,
    COMBAT_LOG_EVENT_SPELL_PERIODIC_ENERGIZE     = 36,
    COMBAT_LOG_EVENT_SPELL_PERIODIC_DRAIN        = 37,
    COMBAT_LOG_EVENT_SPELL_PERIODIC_LEECH        = 38,
    COMBAT_LOG_EVENT_SPELL_DISPEL_FAILED         = 39,
    COMBAT_LOG_EVENT_DAMAGE_SHIELD               = 40,
    COMBAT_LOG_EVENT_DAMAGE_SHIELD_MISSED        = 41,
    COMBAT_LOG_EVENT_DAMAGE_SPLIT                = 42,
    COMBAT_LOG_EVENT_PARTY_KILL                  = 43,
    COMBAT_LOG_EVENT_UNIT_DIED                   = 44,
    COMBAT_LOG_EVENT_UNIT_DESTROYED              = 45,
    COMBAT_LOG_EVENT_SPELL_RESURRECT             = 46,
    COMBAT_LOG_EVENT_SPELL_BUILDING_DAMAGE       = 47,
    COMBAT_LOG_EVENT_SPELL_BUILDING_HEAL         = 48,
    COMBAT_LOG_EVENT_UNIT_DISSIPATES             = 49,
    NUM_COMBAT_LOG_EVENTS                        = 50,
};

enum COMBAT_TEXT_TYPE : int32_t
{
    COMBAT_TEXT_INTERRUPT                 = 0,
    COMBAT_TEXT_DAMAGE_CRIT               = 1,
    COMBAT_TEXT_DAMAGE                    = 2,
    COMBAT_TEXT_MISS                      = 3,
    COMBAT_TEXT_DODGE                     = 4,
    COMBAT_TEXT_PARRY                     = 5,
    COMBAT_TEXT_EVADE                     = 6,
    COMBAT_TEXT_IMMUNE                    = 7,
    COMBAT_TEXT_DEFLECT                   = 8,
    COMBAT_TEXT_REFLECT                   = 9,
    COMBAT_TEXT_RESIST                    = 10,
    COMBAT_TEXT_BLOCK                     = 11,
    COMBAT_TEXT_ABSORB                    = 12,
    COMBAT_TEXT_SPELL_DAMAGE_CRIT         = 13,
    COMBAT_TEXT_SPELL_DAMAGE              = 14,
    COMBAT_TEXT_SPELL_MISS                = 15,
    COMBAT_TEXT_SPELL_DODGE               = 16,
    COMBAT_TEXT_SPELL_PARRY               = 17,
    COMBAT_TEXT_SPELL_EVADE               = 18,
    COMBAT_TEXT_SPELL_IMMUNE              = 19,
    COMBAT_TEXT_SPELL_DEFLECT             = 20,
    COMBAT_TEXT_SPELL_REFLECT             = 21,
    COMBAT_TEXT_SPELL_RESIST              = 22,
    COMBAT_TEXT_SPELL_BLOCK               = 23,
    COMBAT_TEXT_SPELL_ABSORB              = 24,
    COMBAT_TEXT_ENCHANTMENT_REMOVED       = 25,
    COMBAT_TEXT_ENCHANTMENT_ADDED         = 26,
    COMBAT_TEXT_PERIODIC_HEAL             = 27,
    COMBAT_TEXT_ENERGIZE                  = 28,
    COMBAT_TEXT_PERIODIC_ENERGIZE         = 29,
    COMBAT_TEXT_SPELL_CAST                = 30,
    COMBAT_TEXT_SPELL_AURA_END            = 31,
    COMBAT_TEXT_SPELL_AURA_END_HARMFUL    = 32,
    COMBAT_TEXT_SPELL_AURA_START          = 33,
    COMBAT_TEXT_SPELL_AURA_START_HARMFUL  = 34,
    COMBAT_TEXT_SPELL_ACTIVE              = 35,
    COMBAT_TEXT_FACTION                   = 36,
    COMBAT_TEXT_HEAL_CRIT                 = 37,
    COMBAT_TEXT_HEAL                      = 38,
    COMBAT_TEXT_DAMAGE_SHIELD             = 39,
    COMBAT_TEXT_SPELL_DISPELLED           = 40,
    COMBAT_TEXT_EXTRA_ATTACKS             = 41,
    COMBAT_TEXT_SPLIT_DAMAGE              = 42,
    COMBAT_TEXT_HONOR_GAINED              = 43,
    COMBAT_TEXT_PERIODIC_HEAL_ABSORB      = 44,
    COMBAT_TEXT_HEAL_CRIT_ABSORB          = 45,
    COMBAT_TEXT_HEAL_ABSORB               = 46,
    COMBAT_TEXT_ARENA_POINTS_GAINED       = 47,
    NUM_COMBAT_TEXT_TYPES                 = 48,
};

enum COMBATLOG_OBJECT : uint32_t
{
    COMBATLOG_OBJECT_AFFILIATION_MINE               = 0x00000001u,
    COMBATLOG_OBJECT_AFFILIATION_PARTY              = 0x00000002u,
    COMBATLOG_OBJECT_AFFILIATION_RAID               = 0x00000004u,
    COMBATLOG_OBJECT_AFFILIATION_OUTSIDER           = 0x00000008u,
    COMBATLOG_OBJECT_AFFILIATION_MASK               = 0x0000000Fu,
    COMBATLOG_OBJECT_REACTION_FRIENDLY              = 0x00000010u,
    COMBATLOG_OBJECT_REACTION_NEUTRAL               = 0x00000020u,
    COMBATLOG_OBJECT_REACTION_HOSTILE               = 0x00000040u,
    COMBATLOG_OBJECT_REACTION_MASK                  = 0x000000F0u,
    COMBATLOG_OBJECT_CONTROL_PLAYER                 = 0x00000100u,
    COMBATLOG_OBJECT_CONTROL_NPC                    = 0x00000200u,
    COMBATLOG_OBJECT_CONTROL_MASK                   = 0x00000300u,
    COMBATLOG_OBJECT_TYPE_PLAYER                    = 0x00000400u,
    COMBATLOG_OBJECT_TYPE_NPC                       = 0x00000800u,
    COMBATLOG_OBJECT_TYPE_PET                       = 0x00001000u,
    COMBATLOG_OBJECT_TYPE_GUARDIAN                  = 0x00002000u,
    COMBATLOG_OBJECT_TYPE_OBJECT                    = 0x00004000u,
    COMBATLOG_OBJECT_TYPE_MASK                      = 0x0000FC00u,
    COMBATLOG_OBJECT_TARGET                         = 0x00010000u,
    COMBATLOG_OBJECT_FOCUS                          = 0x00020000u,
    COMBATLOG_OBJECT_MAINTANK                       = 0x00040000u,
    COMBATLOG_OBJECT_MAINASSIST                     = 0x00080000u,
    COMBATLOG_OBJECT_RAIDTARGET1                    = 0x00100000u,
    COMBATLOG_OBJECT_RAIDTARGET_MASK                = 0x0FF00000u,
    COMBATLOG_OBJECT_NONE                           = 0x80000000u,
    COMBATLOG_OBJECT_SPECIAL_MASK                   = 0xFFFF0000u,
};

struct CombatLogEntry : public TSLinkedNode<CombatLogEntry> {

    enum DAMAGE_FLAGS : int32_t {
        DAMAGE_CRITICAL = 0x1,
        DAMAGE_GLANCING = 0x2,
        DAMAGE_CRUSHING = 0x4,
        DAMAGE_RESULT_MASK = 0x7,
    };

    enum PENDING_NAME_FLAGS : uint32_t {
        PENDING_NAME_SOURCE = 0x1,
        PENDING_NAME_DEST = 0x2,
        PENDING_NAME_ITEM = 0x4,
    };

    /* 0008 */ uint32_t m_timestamp;
    /* 000C */ int32_t m_event;
    /* 0010 */ uint32_t m_pendingMask;
    /* 0014 */ uint32_t unused_0014;
    /* 0018 */ WOWGUID m_sourceGuid;
    /* 0020 */ const char* m_sourceName;
    /* 0024 */ const char* m_sourceRealm;
    /* 0028 */ uint32_t m_sourceFlags;
    /* 002C */ uint32_t unused_002C;
    /* 0030 */ WOWGUID m_destGuid;
    /* 0038 */ const char* m_destName;
    /* 003C */ const char* m_destRealm;
    /* 0040 */ uint32_t m_destFlags;
    /* 0044 */ int32_t m_spellId;
    /* 0048 */ RCString m_spellName;
    /* 0054 */ uint32_t m_infoFlags;
    /* 0058 */ union {
        char* m_text;
        int32_t m_values[7];
        const char* m_names[7];
    } m_info;
    /* 0074 */ uint8_t m_infoBits;

    static void PendingCombatLogResolved(int32_t id, const WOWGUID& guid, void* param, bool success);

    CombatLogEntry();
    ~CombatLogEntry();
    void Clear();
    void CancelPendingCallbacks();
    const char* ResolveName(PendingCombatLogData* data, const char** realm, bool pending);
    const char* GetObjectName(const WOWGUID& guid, uint32_t mask, const char** realm);
    int32_t PushEvent(lua_State* L);
    bool MatchesFilter();
    void PackEvent(char* buffer, uint32_t size);
    void SetFailReason(const char* reason);
    void SetMissInfo(int32_t missType, int32_t amount);
    void SetExecuteItem(int32_t itemId);
};

struct InvalidatedName : public TSLinkedNode<InvalidatedName> {
    /* 0008 */ char* m_name;

    InvalidatedName();
    ~InvalidatedName();
};

struct PendingCombatLogData : public TSLinkedNode<PendingCombatLogData> {

    enum NAME_SOURCE : int32_t {
        NAME_SOURCE_NONE = 0,
        NAME_SOURCE_PLAYER = 1,
        NAME_SOURCE_PET = 2,
        NAME_SOURCE_CREATURE = 3,
        NAME_SOURCE_GAMEOBJECT = 4,
        NAME_SOURCE_ITEM = 5,
    };

    /* 0008 */ CombatLogEntry* m_entry;
    /* 000C */ uint32_t unused_000C;
    /* 0010 */ WOWGUID m_guid;
    /* 0018 */ int32_t m_id;
    /* 001C */ int32_t m_type;
    /* 0020 */ uint32_t m_mask;
    /* 0024 */ uint32_t unused_0024;
};

struct CombatLogFilter : public TSLinkedNode<CombatLogFilter> {
    /* 0008 */ TSBitField<TSFixedArray<uint32_t>> m_events;
    /* 0018 */ WOWGUID m_sourceGuid;
    /* 0020 */ WOWGUID m_destGuid;
    /* 0028 */ uint32_t m_sourceFlags;
    /* 002C */ uint32_t m_destFlags;
    /* 0030 */ int32_t m_spellId;
    /* 0034 */ RCString m_spellName;

    CombatLogFilter();
    bool SetFilterData(const char* events, const WOWGUID& sourceGuid, uint32_t sourceFlags, const WOWGUID& destGuid, uint32_t destFlags, int32_t spellId, const char* spellName);
    bool MatchesFilter(int32_t event, const WOWGUID& sourceGuid, uint32_t sourceFlags, const WOWGUID& destGuid, uint32_t destFlags, int32_t spellId, const char* spellName);
};

class CombatLog {
    public:
    static int32_t HandleSpellRemoveLog(NETMESSAGE msgId, CDataStore* msg, int32_t timeOffset);
    static int32_t HandleSpellLogMiss(CDataStore* msg, int32_t timeOffset);
    static int32_t HandleSpellLogExecute(CDataStore* msg, int32_t timeOffset);
    static int32_t HandleSpellNonMeleeDamageLog(CDataStore* msg, int32_t timeOffset);
    static int32_t HandleSpellHealLog(CDataStore* msg, int32_t timeOffset);
    static int32_t HandleSpellLogEnergizeLog(CDataStore* msg, int32_t timeOffset);
    static int32_t HandlePeriodicAuraLog(CDataStore* msg, int32_t timeOffset);
    static void AddSpellCastEntry(const WOWGUID& source, const WOWGUID& dest, int32_t spellId);
    static void AddSpellResurrectEntry(const WOWGUID& source, const WOWGUID& dest, int32_t spellId);
    static void AddSpellDamageEntry(const WOWGUID& source, const WOWGUID& dest, int32_t spellId, int32_t damage, uint32_t timeOffset);
    static void AddSpellExecuteEntry(const WOWGUID& source, const WOWGUID& dest, int32_t itemId, int32_t spellId);
    static void AddSpellEnergizeEntry(const WOWGUID& source, const WOWGUID& dest, int32_t spellId);
    static void AddAuraReplacedEntry(const WOWGUID& source, const WOWGUID& dest, int32_t spellId, int32_t replacedSpellId, uint8_t harmful, bool stolen, uint32_t timeOffset);
    static void AddSpellRemoveEntry(const WOWGUID& source, const WOWGUID& dest, int32_t dispelSpellId, int32_t spellId, uint8_t harmful);
    static void GetSpellLogLinkInfo(const WOWGUID& source, const WOWGUID& dest, int32_t spellId);
    static void QueueMissedSpellText(const WOWGUID& source, const WOWGUID& dest, int32_t event, const SpellRec* spell, int32_t missType, int32_t amount, uint32_t timeOffset);
};

void UnitCombatLogInstakill(const WOWGUID& casterGuid, const WOWGUID& targetGuid, int32_t spellId);
void UnitCombatLogSpellMissed(const WOWGUID& casterGuid, const WOWGUID& targetGuid, int32_t spellId, int32_t missType, bool a5, bool a6, int32_t a7);
void UnitCombatLogDamageShield(const WOWGUID& victimGuid, const WOWGUID& casterGuid, int32_t spellId, int32_t damage, uint32_t overkill, uint32_t schoolMask);
PendingCombatLogData* GetPendingCombatLogData(CombatLogEntry* entry, const WOWGUID& guid, uint32_t mask);
CombatLogEntry* GetCombatLogEntry(const WOWGUID& source, const WOWGUID& dest, int32_t event, const SpellRec* spell);
void RecycleCombatLogEntry(CombatLogEntry* entry);
uint32_t GetCombatLogFlags(const WOWGUID& guid);
void SendCombatLogEvents();
void QueueCombatLogEntry(CombatLogEntry* entry, uint32_t timeOffset);
void UnitCombatLogBuildingDamage(const WOWGUID& caster, const WOWGUID& owner, const WOWGUID& building, int32_t damage, int32_t spellId);
void UnitCombatLogBuildingHeal(const WOWGUID& caster, const WOWGUID& owner, const WOWGUID& building, int32_t amount, int32_t spellId);
extern const uint32_t s_environmentalDamageSchools[NUM_ENVIRONMENTAL_DAMAGE_TYPES];
bool ShouldShowFeedback(const ATTACKROUNDINFO& info);
void UnitCombatLogInvalidateName(const char* name);
char* GUIDToHexString(WOWGUID guid, char* buffer);
WOWGUID StringToGuid(const char* string);
void UnitCombatLog(const WOWGUID& guid, uint32_t type, int32_t damage, int32_t absorbed, int32_t resisted);
void UnitCombatLog(const ATTACKROUNDINFO& info);
void UnitCombatLogUnitDead(CGUnit_C* unit);
void UnitCombatLogCastGo(const WOWGUID& caster, const WOWGUID& target, int32_t spellId);
void UnitCombatLogSpellFail(const WOWGUID& caster, int32_t spellId, const char* reason);
class AuraData;
void UnitCombatLogAuraDose(CGUnit_C* unit, const AuraData* aura, int32_t spellId, int32_t oldStack, int32_t newStack, uint8_t a6);
void UnitCombatLogAuraRefreshed(CGUnit_C* unit, const AuraData* aura, int32_t spellId, uint8_t a4);
int32_t Packet_SMSG_PERIODICAURALOG(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_ENCHANTMENTLOG(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_PARTYKILLLOG(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_PROCRESIST(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_DISPEL_FAILED(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_DESTRUCTIBLE_BUILDING_DAMAGE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
void __stdcall CheckToSanitizeUnitLowGuid(WOWGUID* guid);
int32_t IsCombatTextActiveUnit(WOWGUID guid);
void SendToCombatText(int32_t type, const char* text, int32_t amount);
void SendToCombatText(int32_t type, const char* text, int32_t amount, int32_t overflow);
void SendToCombatText(int32_t type, int32_t amount, int32_t overflow);
void SendToCombatText(int32_t type, int32_t amount);
void SendToCombatText(int32_t type, const char* text);
void SendToCombatText(int32_t type);
void UnitCombatLogAuraAddedOrRemoved(CGUnit_C* unit, AuraData* aura, const SpellRec* spell, uint8_t harmful, int32_t added);
void UnitCombatLogProcGo(const WOWGUID& casterGuid, const SpellRec* spell);
void UnitCombatLogPowerDrain(const WOWGUID& casterGuid, const WOWGUID& targetGuid, int32_t spellId, uint32_t powerType, uint32_t amount, float multiplier, bool a7, uint32_t timeOffset);
void UnitCombatLogInterrupt(const WOWGUID& casterGuid, const WOWGUID& targetGuid, int32_t spellId, int32_t interruptedSpellId);
void UnitCombatLogHeal(const WOWGUID& casterGuid, const WOWGUID& targetGuid, int32_t spellId, int32_t amount, int32_t overheal, int32_t absorbed, bool crit, bool periodic, uint32_t timeOffset);
void UnitCombatLogEnergize(const WOWGUID& casterGuid, const WOWGUID& targetGuid, int32_t spellId, int32_t powerType, int32_t amount, bool periodic, uint32_t timeOffset);
void SendHealEvent(int32_t event, const WOWGUID& source, const WOWGUID& dest, const SpellRec* spell, int32_t amount, int32_t overheal, int32_t absorbed, bool crit, uint32_t timeOffset);
void SendEnergizeEvent(int32_t event, const WOWGUID& source, const WOWGUID& dest, const SpellRec* spell, int32_t powerType, int32_t amount, uint32_t timeOffset);
void AddPowerDrainEntry(int32_t event, const WOWGUID& source, const WOWGUID& dest, const SpellRec* spell, int32_t amount, int32_t powerType, int32_t extra, uint32_t timeOffset);

enum SPELL_HIT_TYPE : uint32_t {
    SPELL_HIT_TYPE_UNKNOWN_0 = 0x1,
    SPELL_HIT_TYPE_CRIT = 0x2,
    SPELL_HIT_TYPE_UNKNOWN_2 = 0x4,
    SPELL_HIT_TYPE_SPLIT = 0x8,
    SPELL_HIT_TYPE_UNKNOWN_4 = 0x10,
    SPELL_HIT_TYPE_UNKNOWN_5 = 0x20,
};

void SendDamageEvent(int32_t event, const WOWGUID& source, const WOWGUID& dest, const SpellRec* spell, int32_t damage, int32_t overkill, uint32_t school, int32_t resisted, int32_t blocked, int32_t absorbed, bool crit, bool a11, bool a12, uint32_t timeOffset);
void UnitCombatLogSpellDamage(const WOWGUID& source, const WOWGUID& dest, int32_t spellId, int32_t damage, int32_t overkill, uint32_t school, int32_t absorbed, int32_t resisted, int32_t blocked, uint32_t hitInfo, bool periodic, uint32_t timeOffset);
void UnitCombatLogSplitDamage(const WOWGUID& source, const WOWGUID& dest, int32_t spellId, int32_t damage, int32_t overkill, uint32_t school, int32_t absorbed, int32_t resisted, int32_t blocked, uint32_t hitInfo, uint32_t timeOffset);
void UnitCombatLogSpellMechanic(const WOWGUID& guid, const AuraData* aura, const SpellRec* spell);
bool SpellIsHiddenInCombatLog(const SpellRec* spell);
bool SpellHasOpenLockEffect(const SpellRec* spell);
class SpellMechanicRec;
void DisplayMechanicEffect(const WOWGUID* unitGuid, const WOWGUID* sourceGuid, const SpellRec* spell, const SpellMechanicRec* mechanic);

void UnitCombatLogRegisterScriptFunctions();
void UnitCombatLogUnregisterScriptFunctions();

// INLINED
inline CombatLogFilter::CombatLogFilter() {
    this->m_events.m_count = 50;
    this->m_events.m_bits.SetCount(((50 - 1) >> 5) + 1);
}

extern TSList<CombatLogEntry, TSGetLink<CombatLogEntry> > s_entries;
extern WOWGUID s_combatTextActiveUnit;
extern CombatLogEntry* s_currentEntry;
extern CombatLogEntry* s_pendingEntries;
extern const char* g_spellMissStrings[MISS_NUMMISSTYPES];

#endif
