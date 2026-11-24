#ifndef WOW_SOURCE_UI_DANCESTUDIO_H
#define WOW_SOURCE_UI_DANCESTUDIO_H

#include <cstddef>
#include <cstdint>
#include "WoW/Source/Net/Types.h"
#include "Engine/Source/Storm/H/stpl.h"
#include "WoW/Source/Game/GameClient/RandomSeed.h"
#include "WoW/Source/Object/WGUID.h"

class CDataStore;
class DanceCache;
struct lua_State;

enum DANCE_NOTIFY_FLAGS : uint32_t {
    DANCE_NOTIFY_ADD = 0x1,
    DANCE_NOTIFY_REPLACE = 0x2,
    DANCE_NOTIFY_DELETE = 0x4,
    DANCE_NOTIFY_ERROR = 0x8,
};

enum DANCE_ERROR_CODE : uint32_t {
    DANCE_ERROR_CREATE_DUPLICATE = 0,
    DANCE_ERROR_SAVE_FAILED = 1,
    DANCE_ERROR_DELETE_FAILED = 2,
};

struct KNOWNDANCES : public TSHashObject<KNOWNDANCES, HASHKEY_STR> {
    /* 0018 */ uint32_t m_id;
    /* 001C */ uint32_t unk_001C;
};

struct DANCE_REQUEST {
    /* 0000 */ WOWGUID m_guid;
    /* 0008 */ int32_t m_danceId;
    /* 000C */ uint32_t m_start;
    /* 0010 */ _RandomSeed m_seed;
};

int32_t Packet_SMSG_PLAY_DANCE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_STOP_DANCE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_NOTIFY_DANCE(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);
int32_t Packet_SMSG_LEARNED_DANCE_MOVES(void* param, NETMESSAGE msgId, uint32_t time, CDataStore* msg);

class CGDanceStudio {
    public:
    static void LeaveWorld();
    static uint64_t m_learnedDanceMoves;

    static void InitializeGame();
    static void ShutdownGame();
    static void LoadDance(int32_t danceId);
    static void PlayDance(DanceCache* record, DANCE_REQUEST* request);
    static void DisplayDance(DanceCache* record);
    static void RegisterScriptFunctions();
    static void UnregisterScriptFunctions();
    static bool HasDance(const char* name);
};

void PlayDance(const char* name);

#endif
