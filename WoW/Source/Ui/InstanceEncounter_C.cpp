#include "InstanceEncounter_C.h"
#include <cstdlib>
#include <cstring>
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/TimeManager.h"
#include "WoW/Source/Ui/GameUI.h"
#include "WoW/Source/Ui/Types.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

// STATIC: 0x00C242F8
TSFixedArray<ENCOUNTER_UNIT> CGInstanceEncounter_C::m_units;

// STATIC: 0x00AD14BC
TSList<ENCOUNTER_TIMER> CGInstanceEncounter_C::m_timers;

// STATIC: 0x00AD14C8
TSList<ENCOUNTER_OBJECTIVE> CGInstanceEncounter_C::m_objectives;

// VERIFY: 4679293d manual
// OFFSET: 0x005ED590
bool ENCOUNTER_UNIT::operator<(const ENCOUNTER_UNIT& other) const {
    if (!this->m_guid.m_guid) {
        return false;
    }

    if (!other.m_guid.m_guid) {
        return true;
    }

    if (this->m_priority == other.m_priority) {
        return this->m_index - other.m_index < 0;
    }

    return this->m_priority - other.m_priority < 0;
}

// VERIFY: fe77e6c5 manual
// OFFSET: 0x005ED5F0
int32_t ENCOUNTER_UNIT::Compare(const void* a, const void* b) {
    const ENCOUNTER_UNIT* unitA = static_cast<const ENCOUNTER_UNIT*>(a);
    const ENCOUNTER_UNIT* unitB = static_cast<const ENCOUNTER_UNIT*>(b);

    if (unitA->m_guid == unitB->m_guid) {
        return 0;
    }

    return *unitA < *unitB ? -1 : 1;
}

// VERIFY: 7ae67001 manual
// OFFSET: 0x005ED6C0
static ENCOUNTER_UNIT* FindUnit(const WOWGUID& guid) {
    for (uint32_t i = 0; i < CGInstanceEncounter_C::m_units.Count(); i++) {
        if (CGInstanceEncounter_C::m_units.m_data[i].m_guid == guid) {
            return &CGInstanceEncounter_C::m_units.m_data[i];
        }
    }

    return nullptr;
}

// VERIFY: f07369c6 manual
// OFFSET: 0x005ED710
WOWGUID CGInstanceEncounter_C::GetBoss(uint32_t index) {
    if (static_cast<int32_t>(index) < 0 || static_cast<int32_t>(index) >= static_cast<int32_t>(CGInstanceEncounter_C::m_units.Count())) {
        return WOWGUID{ 0 };
    }

    return CGInstanceEncounter_C::m_units.m_data[index].m_guid;
}

// VERIFY: 1908566c auto
// OFFSET: 0x005ED740
uint32_t CGInstanceEncounter_C::GetNumBossSlots() {
    return CGInstanceEncounter_C::m_units.Count();
}

// VERIFY: 3301311d auto
// OFFSET: 0x005ED750
static void SortUnits() {
    if (!CGInstanceEncounter_C::m_units.Count()) {
        return;
    }

    qsort(CGInstanceEncounter_C::m_units.m_data, CGInstanceEncounter_C::m_units.Count(), sizeof(ENCOUNTER_UNIT), ENCOUNTER_UNIT::Compare);

    for (uint32_t i = 0; i < 16; i++) {
        if (CGInstanceEncounter_C::m_units.m_data[i].m_guid.m_guid) {
            CGInstanceEncounter_C::m_units.m_data[i].m_index = static_cast<uint8_t>(i);
        }
    }
}

// VERIFY: bb045bc2 manual
// OFFSET: 0x005ED7B0
void CGInstanceEncounter_C::EngageUnit(const WOWGUID& guid, uint8_t priority) {
    if (FindUnit(guid)) {
        return;
    }

    if (CGInstanceEncounter_C::m_units.Count() < 16) {
        CGInstanceEncounter_C::m_units.SetCount(16);
        memset(CGInstanceEncounter_C::m_units.m_data, 0, sizeof(ENCOUNTER_UNIT) * 16);
    }

    for (uint32_t i = 0; i < CGInstanceEncounter_C::m_units.Count(); i++) {
        if (!CGInstanceEncounter_C::m_units.m_data[i].m_guid.m_guid) {
            CGInstanceEncounter_C::m_units.m_data[i].m_guid = guid;
            CGInstanceEncounter_C::m_units.m_data[i].m_priority = priority;
            CGInstanceEncounter_C::m_units.m_data[i].m_index = static_cast<uint8_t>(i);
            break;
        }
    }

    SortUnits();
    FrameScript_SignalEvent(SCRIPT_EVENT_INSTANCE_ENCOUNTER_ENGAGE_UNIT, nullptr);
}

// VERIFY: 5afd30df manual
// OFFSET: 0x005ED870
void CGInstanceEncounter_C::DisengageUnit(const WOWGUID& guid) {
    if (!FindUnit(guid)) {
        return;
    }

    for (uint32_t i = 0; i < CGInstanceEncounter_C::m_units.Count(); i++) {
        if (CGInstanceEncounter_C::m_units.m_data[i].m_guid == guid) {
            CGInstanceEncounter_C::m_units.m_data[i].m_guid = WOWGUID{ 0 };
            CGInstanceEncounter_C::m_units.m_data[i].m_priority = 0;
            CGInstanceEncounter_C::m_units.m_data[i].m_index = 0;
            break;
        }
    }

    SortUnits();
    FrameScript_SignalEvent(SCRIPT_EVENT_INSTANCE_ENCOUNTER_ENGAGE_UNIT, nullptr);

    if (!CGInstanceEncounter_C::m_units.m_data[0].m_guid.m_guid) {
        CGInstanceEncounter_C::m_units.Clear();
    }
}

// VERIFY: aa66d396 manual
// OFFSET: 0x005ED930
static void OnUnitMessage(int32_t type, WOWGUID guid, uint8_t priority) {
    if (type == CGInstanceEncounter_C::UPDATE_ENGAGE_UNIT) {
        CGInstanceEncounter_C::EngageUnit(guid, priority);
    } else if (type == CGInstanceEncounter_C::UPDATE_DISENGAGE_UNIT) {
        CGInstanceEncounter_C::DisengageUnit(guid);
    } else if (type == CGInstanceEncounter_C::UPDATE_UNIT_PRIORITY) {
        ENCOUNTER_UNIT* unit = FindUnit(guid);

        if (unit) {
            unit->m_priority = priority;
            SortUnits();
            FrameScript_SignalEvent(SCRIPT_EVENT_INSTANCE_ENCOUNTER_ENGAGE_UNIT, nullptr);
        }
    }
}

// VERIFY: 02f49995 manual
// OFFSET: 0x005EDB10
void CGInstanceEncounter_C::LeaveWorld() {
    if (CGGameUI::m_currentlyReloadingUI) {
        return;
    }

    ClientServices::ClearMessageHandler(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT);
    CGInstanceEncounter_C::m_units.Clear();
    CGInstanceEncounter_C::m_timers.Clear();
}

// VERIFY: 8b8e61cc manual
// OFFSET: 0x005EDB70
void CGInstanceEncounter_C::ClearExpiredTimers() {
    if (!CGInstanceEncounter_C::m_timers.Head()) {
        return;
    }

    uint32_t now = OsGetAsyncTimeMs();
    ENCOUNTER_TIMER* timer = CGInstanceEncounter_C::m_timers.Head();

    while (timer) {
        if (static_cast<int32_t>(now - timer->m_expireTime) < 0) {
            timer = CGInstanceEncounter_C::m_timers.Next(timer);
        } else {
            timer = CGInstanceEncounter_C::m_timers.DeleteNode(timer);
        }
    }
}

// VERIFY: ae200c3e manual
// OFFSET: 0x005EDBC0
void CGInstanceEncounter_C::AddTimer(uint32_t duration) {
    CGInstanceEncounter_C::ClearExpiredTimers();
    ENCOUNTER_TIMER* timer = CGInstanceEncounter_C::m_timers.NewNode(TSLIST_LINK_BEFORE, 0, 0);
    timer->m_expireTime = duration + OsGetAsyncTimeMs();
}

// VERIFY: c595bb8c manual
// OFFSET: 0x005EDC50
void CGInstanceEncounter_C::EnableObjective(int32_t id) {
    for (ENCOUNTER_OBJECTIVE* objective = CGInstanceEncounter_C::m_objectives.Head(); objective; objective = CGInstanceEncounter_C::m_objectives.Next(objective)) {
        if (objective->m_id == id) {
            return;
        }
    }

    ENCOUNTER_OBJECTIVE* objective = CGInstanceEncounter_C::m_objectives.NewNode(TSLIST_LINK_BEFORE, 0, 0);
    objective->m_id = static_cast<uint8_t>(id);
    objective->m_progress = 0;
}

// VERIFY: 33b377b6 manual
// OFFSET: 0x005EDD10
static void UpdateObjective(int32_t id, int32_t delta) {
    for (ENCOUNTER_OBJECTIVE* objective = CGInstanceEncounter_C::m_objectives.Head(); objective; objective = CGInstanceEncounter_C::m_objectives.Next(objective)) {
        if (objective->m_id == id) {
            objective->m_progress += delta;
            return;
        }
    }
}

// VERIFY: c5e756d6 manual
// OFFSET: 0x005EDD50
void CGInstanceEncounter_C::DisableObjective(int32_t id) {
    for (ENCOUNTER_OBJECTIVE* objective = CGInstanceEncounter_C::m_objectives.Head(); objective; objective = CGInstanceEncounter_C::m_objectives.Next(objective)) {
        if (objective->m_id == id) {
            CGInstanceEncounter_C::m_objectives.DeleteNode(objective);
            return;
        }
    }
}

// VERIFY: a2968217 manual
// OFFSET: 0x005EDD90
static void OnObjectiveMessage(int32_t type, uint8_t value) {
    if (type == CGInstanceEncounter_C::UPDATE_ADD_TIMER) {
        CGInstanceEncounter_C::AddTimer(value);
    } else if (type == CGInstanceEncounter_C::UPDATE_ENABLE_OBJECTIVE) {
        CGInstanceEncounter_C::EnableObjective(value);
    } else if (type == CGInstanceEncounter_C::UPDATE_DISABLE_OBJECTIVE) {
        CGInstanceEncounter_C::DisableObjective(value);
    }
}

// VERIFY: f40341b3 manual
// OFFSET: 0x005EDDD0
int32_t Packet_SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg) {
    uint32_t type = 0xFFFFFFFF;
    SmartGUID guid;
    uint8_t value = 0;
    uint8_t delta = 0;
    msg->Get(type);

    switch (type) {
        case CGInstanceEncounter_C::UPDATE_ENGAGE_UNIT:
        case CGInstanceEncounter_C::UPDATE_DISENGAGE_UNIT:
        case CGInstanceEncounter_C::UPDATE_UNIT_PRIORITY:
            *msg >> guid;
            msg->Get(value);

            if (msg->m_read > msg->m_size) {
                break;
            }

            OnUnitMessage(type, guid, value);
            break;

        case CGInstanceEncounter_C::UPDATE_ADD_TIMER:
        case CGInstanceEncounter_C::UPDATE_ENABLE_OBJECTIVE:
        case CGInstanceEncounter_C::UPDATE_DISABLE_OBJECTIVE:
            msg->Get(value);

            if (msg->m_read > msg->m_size) {
                break;
            }

            OnObjectiveMessage(type, value);
            break;

        case CGInstanceEncounter_C::UPDATE_OBJECTIVE_PROGRESS:
            msg->Get(value);
            msg->Get(delta);

            if (msg->m_read > msg->m_size) {
                break;
            }

            UpdateObjective(value, delta);
            break;

        case CGInstanceEncounter_C::UPDATE_REFRESH_UNITS:
            SortUnits();
            FrameScript_SignalEvent(SCRIPT_EVENT_INSTANCE_ENCOUNTER_ENGAGE_UNIT, nullptr);
            break;
    }

    return 1;
}

// VERIFY: e1fb6f4f auto
// OFFSET: 0x005EDED0
void CGInstanceEncounter_C::EnterWorld() {
    if (!CGGameUI::m_currentlyReloadingUI) {
        ClientServices::SetMessageHandler(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, Packet_SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, nullptr);
    }
}
