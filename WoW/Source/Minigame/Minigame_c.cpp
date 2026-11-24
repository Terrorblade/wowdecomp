#include "Minigame_c.h"
#include <cstring>
#include <new>
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/lua-5.1/src/lua.h"
#include "Engine/Source/Storm/H/storm.h"
#include "WoW/Source/Net/Types.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"

// INLINED
CMinigame_C::CMinigame_C(MINIGAME_TYPE type, uint64_t guid) {
    this->m_guid = guid;
    this->m_type = type;
}

// INLINED
CMinigameTicTacToe_C::CMinigameTicTacToe_C(uint64_t guid) : CMinigame_C(MINIGAME_TICTACTOE, guid) {
    this->m_guid = guid;
    this->m_type = MINIGAME_TICTACTOE;
    this->m_state = MINIGAME_STATE_NONE;
    this->unk_0010 = 0;
    this->unk_0018 = 0;
    this->unk_0020 = 0;
    this->unk_0021 = 0;
    memset(this->m_board, 0, sizeof(this->m_board));
}

// VERIFY: 3cfba3e7 manual
// OFFSET: 0x00847C20
CMinigame_C* CMinigame_C::CreateMinigame(MINIGAME_TYPE type, uint64_t guid) {
    if (type != MINIGAME_TICTACTOE) {
        return nullptr;
    }

    void* memory = SMemAlloc(sizeof(CMinigameTicTacToe_C), __FILE__, __LINE__, 0);
    return memory ? new (memory) CMinigameTicTacToe_C(guid) : nullptr;
}

// VERIFY: 34118bc1 auto
// OFFSET: 0x00847CA0
void CMinigame_C::UpdateSetup(CDataStore* msg) {
    uint8_t state;
    msg->Get(state);
    this->m_state = state;
    msg->Get(this->unk_0010);
    msg->Get(this->unk_0018);
    msg->GetData(this->m_board, sizeof(this->m_board));

    if (this->m_state == MINIGAME_STATE_UNKNOWN_2) {
        msg->Get(this->unk_0021);
    }
}

// VERIFY: 287363f9 auto
// OFFSET: 0x00847D00
void CMinigameTicTacToe_C::UpdateState(CDataStore* msg) {
    for (uint32_t i = 0; i < 9; i++) {
        msg->Get(this->m_board[i]);
    }

    msg->Get(this->unk_0020);
}

// VERIFY: 289f88b4 auto
// OFFSET: 0x00847D40
bool CMinigameTicTacToe_C::ValidateMoveClient(uint8_t moveType, int32_t param) {
    return CMinigameTicTacToe::ValidateMove(this, moveType, param);
}

// VERIFY: 05a92019 auto
// OFFSET: 0x00847D70
int32_t CMinigameTicTacToe_C::PushStateToLua(lua_State* L) {
    lua_checkstack(L, 9);

    for (uint32_t i = 0; i < 9; i++) {
        lua_pushnumber(L, this->m_board[i]);
    }

    return 9;
}

// VERIFY: 0b7228bf manual
// OFFSET: 0x00847DC0
void CMinigame_C::MakeMove(uint8_t moveType, int32_t param) {
    if (!this->ValidateMoveClient(moveType, param)) {
        return;
    }

    CDataStore msg;
    msg.Put(static_cast<uint32_t>(CMSG_MINIGAME_MOVE));
    msg.Put(this->m_guid);
    msg.Put(moveType);
    msg.Put(static_cast<uint32_t>(param));
    msg.Finalize();
    ClientServices::Send2(&msg);
}

// VERIFY: b53c7de0 auto
// OFFSET: 0x00847E70
bool CMinigameTicTacToe::ValidateMove(MiniGameData* data, uint8_t moveType, int32_t param) {
    return static_cast<uint32_t>(param) <= 9 && data->m_board[param] == 0xFF;
}
