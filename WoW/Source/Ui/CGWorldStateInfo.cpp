#include "CGWorldStateInfo.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "Engine/Source/Os/TimeManager.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/Ui/Types.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <ctime>
#include "WoW/Source/Ui/MinimapFrame.h"
#include "WoW/Source/DB/DBClient/DBClient.h"
#include "WoW/Source/Ui/GameUI.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Ui/QuestTextParser.h"

// STATIC: 0x00BE8F40
uint32_t CGWorldStateInfo::m_nextUITimeUpdate;

// STATIC: 0x00BE8F44
uint32_t CGWorldStateInfo::m_UITimeOffset;

// STATIC: 0x00BE8F48
uint32_t CGWorldStateInfo::m_nextUpdateTime;

// STATIC: 0x00BE8F4C
int32_t CGWorldStateInfo::m_subAreaID;

// STATIC: 0x00BE8F50
int32_t CGWorldStateInfo::m_areaID;

// STATIC: 0x00BE8F54
int32_t CGWorldStateInfo::m_mapID;

// STATIC: 0x00BE8F58
TSHashTable<WORLDSTATE, HASHKEY_NONE> CGWorldStateInfo::m_worldStates;

// STATIC: 0x00BE8F80
TSGrowableArray<WorldStateUIEntry> CGWorldStateInfo::m_worldStateUI;

// VERIFY: 50fb45a8 auto
// OFFSET: 0x00548570
void CGWorldStateInfo::InitializeGame() {
    CGWorldStateInfo::m_mapID = -1;
    CGWorldStateInfo::m_areaID = 0;
    CGWorldStateInfo::m_nextUpdateTime = OsGetAsyncTimeMs();
    CGWorldStateInfo::m_UITimeOffset = 0;
    CGWorldStateInfo::m_nextUITimeUpdate = 0;
}

// VERIFY: a62d0161 auto
// OFFSET: 0x00548700
int32_t CGWorldStateInfo::GetUITime() {
    return CGWorldStateInfo::m_UITimeOffset + static_cast<int32_t>(time(nullptr));
}

// VERIFY: dd8a89da auto
// OFFSET: 0x00548D10
int32_t GetWorldState(int32_t id) {
    WORLDSTATE* state = CGWorldStateInfo::m_worldStates.Ptr(id, HASHKEY_NONE());

    if (state) {
        return state->m_value;
    }

    return 0;
}

// VERIFY: 8d223bd3 manual
// OFFSET: 0x00548760
static void SendWorldStateUITimerUpdate() {
    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_UI_TIME_REQUEST));
    msg.Finalize();
    ClientServices::Send2(&msg);
}

// VERIFY: eeea7d70 auto
// OFFSET: 0x005488F0
void CheckEventWorldStateUITimerUpdate() {
    uint32_t now = OsGetAsyncTimeMs();

    if (static_cast<int32_t>(now - CGWorldStateInfo::m_nextUITimeUpdate) >= 0) {
        CGWorldStateInfo::m_nextUITimeUpdate = now + 20000;
        SendWorldStateUITimerUpdate();
    }

    if (static_cast<int32_t>(now - CGWorldStateInfo::m_nextUpdateTime) < 0) {
        return;
    }

    CGWorldStateInfo::m_nextUpdateTime = now + 1000;

    for (uint32_t i = 0; i < CGWorldStateInfo::m_worldStateUI.m_count; i++) {
        if (CGWorldStateInfo::m_worldStateUI.m_data[i].m_state == WORLD_STATE_UI_TYPE_TIMER) {
            FrameScript_SignalEvent(SCRIPT_EVENT_WORLD_STATE_UI_TIMER_UPDATE, nullptr);
            return;
        }
    }
}

// VERIFY: 7e04e5b5 auto
// OFFSET: 0x00548970
bool CGWorldStateInfo::InitWorldStates(uint32_t mapId, uint32_t zoneId, uint32_t areaId) {
    if (mapId == static_cast<uint32_t>(CGWorldStateInfo::m_mapID) && zoneId == static_cast<uint32_t>(CGWorldStateInfo::m_areaID) && areaId == static_cast<uint32_t>(CGWorldStateInfo::m_subAreaID)) {
        return false;
    }

    CGWorldStateInfo::m_mapID = mapId;
    CGWorldStateInfo::m_areaID = zoneId;
    CGWorldStateInfo::m_subAreaID = areaId;
    CGWorldStateInfo::ResetWorldStateUI();
    return true;
}

// VERIFY: 3f42a7fa manual
// OFFSET: 0x00549440
bool CGWorldStateInfo::SetWorldState(uint32_t state, uint32_t value) {
    WORLDSTATE* worldState = CGWorldStateInfo::m_worldStates.Ptr(state, HASHKEY_NONE());

    if (!worldState) {
        worldState = CGWorldStateInfo::m_worldStates.New(state, HASHKEY_NONE(), 0, 0);
    }

    if (worldState->m_value == static_cast<int32_t>(value)) {
        return false;
    }

    worldState->m_value = value;

    if (state == WORLD_STATE_ARENA_SEASON) {
        FrameScript_SignalEvent(SCRIPT_EVENT_ARENA_SEASON_WORLD_STATE, nullptr);
    }

    RefreshWorldStatePOIs(state);
    return true;
}

// VERIFY: 597548a3 auto
// OFFSET: 0x005486E0
void CGWorldStateInfo::OnUITime(uint32_t time) {
    CGWorldStateInfo::m_UITimeOffset = time - static_cast<uint32_t>(::time(nullptr));
}

// VERIFY: f7de346e manual
// OFFSET: 0x005487D0
void CGWorldStateInfo::ResetWorldStateUI() {
    CGWorldStateInfo::m_worldStateUI.m_count = 0;

    for (int32_t i = 0; i < g_worldStateUIDB.m_numRecords; i++) {
        WorldStateUIRec* rec = g_worldStateUIDB.GetRecordByIndex(i);

        if (rec->m_mapID != -1 && rec->m_mapID != CGWorldStateInfo::m_mapID) {
            continue;
        }

        if (rec->m_areaID && rec->m_areaID != CGWorldStateInfo::m_areaID && rec->m_areaID != CGWorldStateInfo::m_subAreaID) {
            continue;
        }

        if (rec->m_phaseShift && !(rec->m_phaseShift & CGGameUI::m_phaseMask)) {
            continue;
        }

        if (rec->m_type && rec->m_type != WORLD_STATE_UI_TYPE_TIMER && rec->m_type != WORLD_STATE_UI_TYPE_UNKNOWN_1) {
            continue;
        }

        WorldStateUIEntry* entry = CGWorldStateInfo::m_worldStateUI.New();
        entry->m_id = rec->m_id;
        entry->m_state = rec->m_type;
    }
}

// VERIFY: 52346cc7 manual
// OFFSET: 0x00548B20
void CGWorldStateInfo::ShutdownGame() {
    CGWorldStateInfo::m_worldStates.InternalClear(0);
    CGWorldStateInfo::m_worldStateUI.Clear();
}

// VERIFY: 78ef8608 auto
// OFFSET: 0x005770D0
bool WorldStateParseText(const char* text, char* buffer, uint32_t size) {
    if (!text) {
        return false;
    }

    int32_t error = 0;
    *buffer = '\0';
    const char* start = text;

    for (const char* percent = SStrChr(text, '%'); percent && *percent; percent = SStrChr(start, '%')) {
        uint32_t length = static_cast<uint32_t>(percent - start);

        if (length) {
            uint32_t used = static_cast<uint32_t>(SStrLen(buffer));
            SStrPack(buffer, start, size);

            if (used + length < size) {
                buffer[used + length] = '\0';
            }
        }

        const char* cursor = percent + 1;
        const char* number = cursor;

        while (*cursor >= '0' && *cursor <= '9') {
            cursor++;
        }

        switch (*cursor) {
            case 'K':
            case 'k':
                ParseCountDownTimerText(buffer, size, GetWorldState(SStrToInt(number)));
                cursor++;
                break;

            case 'W':
            case 'w': {
                char value[16];
                SStrPrintf(value, sizeof(value), "%d", GetWorldState(SStrToInt(number)));
                SStrPack(buffer, value, size);
                cursor++;
                break;
            }

            default:
                error = 1;
                SStrPack(buffer, "%", size);
                break;
        }

        start = cursor;
    }

    SStrPack(buffer, start, size);
    return error == 0;
}
